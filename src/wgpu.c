#include "assume.h"
#include "spirv.h"
#include "uniform_ring.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if !defined(__APPLE__) || !defined(__aarch64__) || defined(__wasm__)

struct umbra_backend *umbra_backend_wgpu(void) { return 0; }

#else

#include <wgpu.h>

// ---------------------------------------------------------------------------
//  Buffer cache / copyback / free pool.
// ---------------------------------------------------------------------------

struct wgpu_buf_cache_entry {
    WGPUBuffer  buf;
    void       *host;
    void       *shadow;   // last-uploaded snapshot; skip re-upload if unchanged
    uint64_t    size;
    _Bool       writable;
    _Bool       copy_tracked; int :16, :32;
};

struct wgpu_free_buf {
    WGPUBuffer buf;
    uint64_t   size;
};

struct wgpu_copyback {
    void     *host;
    WGPUBuffer buf;
    size_t     bytes;
};

// ---------------------------------------------------------------------------
//  Backend and program structs.
// ---------------------------------------------------------------------------

enum { WGPU_N_FRAMES = 2, WGPU_RING_HIGH_WATER = 64 * 1024, TS_RESOLVE_ALIGN = 256 };

struct wgpu_backend {
    struct umbra_backend      base;
    WGPUInstance              instance;
    WGPUAdapter               adapter;
    WGPUDevice                device;
    WGPUQueue                 queue;

    WGPUCommandEncoder        batch_enc;
    WGPUComputePassEncoder    batch_pass;
    _Bool                     batch_has_dispatch; int :24, :32;

    WGPUSubmissionIndex       frame_submitted[WGPU_N_FRAMES];
    _Bool                     frame_has_work [WGPU_N_FRAMES]; int :16, :32;

    struct wgpu_buf_cache_entry *batch_cache;
    int                          batch_cache_n, batch_cache_cap;
    struct wgpu_free_buf        *free_bufs;
    int                          free_bufs_n, free_bufs_cap;
    struct wgpu_copyback        *batch_copies;
    int                          batch_n_copies, batch_copies_cap;

    // Bind group cache: reuse across dispatches when data bindings don't change.
    WGPUBindGroup             cached_bg;
    WGPUBuffer                cached_bg_push_buf;  // ring chunk buf used for push binding
    WGPUBuffer                cached_bg_bufs[32];
    uint64_t                  cached_bg_off [32];
    uint64_t                  cached_bg_sz  [32];
    int                       cached_bg_n, :32;

    struct uniform_ring_pool  uni_pool;
    double                    gpu_time_accum;
    double                    ts_period;
    WGPUQuerySet              ts_query;
    WGPUBuffer                ts_resolve;
    WGPUBuffer                ts_staging;
};

struct wgpu_program {
    struct umbra_program     base;
    struct wgpu_backend     *be;
    WGPUShaderModule         shader;
    WGPUBindGroupLayout      bg_layout;
    WGPUPipelineLayout       pipe_layout;
    WGPUComputePipeline      pipeline;

    int max_ptr;
    int total_bufs;
    int n_deref;
    int push_words;

    struct deref_info *deref;
    uint8_t          *buf_rw;

    uint32_t *spirv;
    int       spirv_words, :32;
};

// ---------------------------------------------------------------------------
//  Uniform ring callbacks.
// ---------------------------------------------------------------------------

struct wgpu_ring_chunk {
    WGPUBuffer buf;
    void      *mapped;
};

static struct uniform_ring_chunk wgpu_ring_new_chunk(size_t min_bytes, void *ctx) {
    struct wgpu_backend *be = ctx;
    size_t cap = min_bytes > WGPU_RING_HIGH_WATER ? min_bytes : WGPU_RING_HIGH_WATER;
    struct wgpu_ring_chunk *chunk = calloc(1, sizeof *chunk);
    chunk->buf = wgpuDeviceCreateBuffer(be->device, &(WGPUBufferDescriptor){
        .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst,
        .size  = cap,
    });
    chunk->mapped = malloc(cap);
    void *mapped = chunk->mapped;
    return (struct uniform_ring_chunk){
        .handle = chunk,
        .mapped = mapped,
        .cap    = cap,
        .used   = 0,
    };
}

static void wgpu_ring_free_chunk(void *handle, void *ctx) {
    (void)ctx;
    struct wgpu_ring_chunk *chunk = handle;
    wgpuBufferRelease(chunk->buf);
    free(chunk->mapped);
    free(chunk);
}

// ---------------------------------------------------------------------------
//  Batch helpers.
// ---------------------------------------------------------------------------

static void wgpu_submit_cmdbuf(struct wgpu_backend *be);

static void map_cb(WGPUMapAsyncStatus status, WGPUStringView msg,
                   void *u1, void *u2) {
    (void)status; (void)msg; (void)u1; (void)u2;
}

static void device_request_cb(WGPURequestDeviceStatus status, WGPUDevice dev,
                              WGPUStringView msg, void *u1, void *u2) {
    (void)msg; (void)u2;
    if (status == WGPURequestDeviceStatus_Success) {
        *(WGPUDevice *)u1 = dev;
    }
}

static _Bool wgpu_had_error;

static void error_cb(WGPUDevice const *dev, WGPUErrorType type, WGPUStringView msg,
                     void *u1, void *u2) {
    (void)dev; (void)u1; (void)u2;
    if (type != WGPUErrorType_NoError) {
        wgpu_had_error = 1;
        dprintf(2, "wgpu error: %.*s\n", (int)msg.length, msg.data);
    }
}

static WGPUComputePassEncoder begin_pass(struct wgpu_backend *be, _Bool begin_ts) {
    if (be->ts_query) {
        WGPUPassTimestampWrites ts = {
            .querySet                = be->ts_query,
            .beginningOfPassWriteIndex = begin_ts ? (uint32_t)be->uni_pool.cur * 2
                                                  : WGPU_QUERY_SET_INDEX_UNDEFINED,
            .endOfPassWriteIndex     = (uint32_t)be->uni_pool.cur * 2 + 1,
        };
        return wgpuCommandEncoderBeginComputePass(be->batch_enc,
            &(WGPUComputePassDescriptor){.timestampWrites = &ts});
    }
    return wgpuCommandEncoderBeginComputePass(be->batch_enc, NULL);
}

static void begin_batch(struct wgpu_backend *be) {
    if (be->batch_enc) { return; }
    be->batch_enc  = wgpuDeviceCreateCommandEncoder(be->device, NULL);
    be->batch_pass = begin_pass(be, 1);
    be->batch_has_dispatch = 0;
}

static void batch_track_copy(struct wgpu_backend *be,
                             void *host, WGPUBuffer buf, size_t bytes) {
    if (be->batch_n_copies >= be->batch_copies_cap) {
        be->batch_copies_cap = be->batch_copies_cap ? 2 * be->batch_copies_cap : 64;
        be->batch_copies = realloc(be->batch_copies,
            (size_t)be->batch_copies_cap * sizeof *be->batch_copies);
    }
    be->batch_copies[be->batch_n_copies++] =
        (struct wgpu_copyback){host, buf, bytes};
}

// ---------------------------------------------------------------------------
//  Buffer cache.
// ---------------------------------------------------------------------------

// WebGPU requires buffer writes to be 4-byte aligned.
static void queue_write_aligned(struct wgpu_backend *be, WGPUBuffer buf,
                                void const *data, size_t bytes) {
    size_t aligned = (bytes + 3) & ~(size_t)3;
    if (aligned == bytes) {
        wgpuQueueWriteBuffer(be->queue, buf, 0, data, bytes);
    } else {
        // Pad to 4-byte alignment with zeros on the stack.
        char tmp[4] = {0};
        __builtin_memcpy(tmp, (char const *)data + (bytes & ~(size_t)3), bytes & 3);
        wgpuQueueWriteBuffer(be->queue, buf, 0, data, bytes & ~(size_t)3);
        wgpuQueueWriteBuffer(be->queue, buf, bytes & ~(size_t)3, tmp, 4);
    }
}

static int cache_buf(struct wgpu_backend *be, void *host, size_t bytes,
                     uint8_t rw) {
    for (int i = 0; i < be->batch_cache_n; i++) {
        struct wgpu_buf_cache_entry *ce = &be->batch_cache[i];
        if (ce->host == host && ce->size >= bytes) {
            if (host && bytes) {
                // Skip re-upload if host data hasn't changed since last upload.
                if (!ce->shadow || memcmp(ce->shadow, host, bytes)) {
                    queue_write_aligned(be, ce->buf, host, bytes);
                    if (!ce->shadow) { ce->shadow = malloc(bytes); }
                    memcpy(ce->shadow, host, bytes);
                }
            }
            if ((rw & BUF_WRITTEN) && !ce->copy_tracked && host && bytes) {
                batch_track_copy(be, host, ce->buf, bytes);
                ce->copy_tracked = 1;
            }
            return i;
        }
    }

    if (be->batch_cache_n >= be->batch_cache_cap) {
        be->batch_cache_cap = be->batch_cache_cap ? 2 * be->batch_cache_cap : 16;
        be->batch_cache = realloc(be->batch_cache,
            (size_t)be->batch_cache_cap * sizeof *be->batch_cache);
    }
    int idx = be->batch_cache_n++;
    struct wgpu_buf_cache_entry *ce = &be->batch_cache[idx];
    *ce = (struct wgpu_buf_cache_entry){0};

    uint64_t alloc_size = bytes ? (bytes + 3) & ~(uint64_t)3 : 4;
    // Reuse from free pool.
    for (int i = 0; i < be->free_bufs_n; i++) {
        if (be->free_bufs[i].size >= alloc_size) {
            ce->buf  = be->free_bufs[i].buf;
            ce->size = be->free_bufs[i].size;
            be->free_bufs[i] = be->free_bufs[--be->free_bufs_n];
            goto fill;
        }
    }
    ce->buf = wgpuDeviceCreateBuffer(be->device, &(WGPUBufferDescriptor){
        .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst
               | WGPUBufferUsage_CopySrc,
        .size  = alloc_size,
    });
    ce->size = alloc_size;
fill:
    ce->host     = host;
    ce->writable = rw & BUF_WRITTEN;
    if (host && bytes) {
        queue_write_aligned(be, ce->buf, host, bytes);
        if (bytes <= 1024 * 1024) {
            ce->shadow = malloc(bytes);
            memcpy(ce->shadow, host, bytes);
        }
    }
    if ((rw & BUF_WRITTEN) && host && bytes) {
        batch_track_copy(be, host, ce->buf, bytes);
        ce->copy_tracked = 1;
    }
    return idx;
}

// ---------------------------------------------------------------------------
//  Submit and flush.
// ---------------------------------------------------------------------------

static void wgpu_wait_frame(int frame, void *ctx) {
    struct wgpu_backend *be = ctx;
    if (be->frame_has_work[frame]) {
        wgpuDevicePoll(be->device, 1, &be->frame_submitted[frame]);
        be->frame_has_work[frame] = 0;
        if (be->ts_query) {
            uint64_t const off = (uint64_t)frame * TS_RESOLVE_ALIGN;
            uint64_t const sz  = 2 * sizeof(uint64_t);
            wgpuBufferMapAsync(be->ts_staging, WGPUMapMode_Read, off, sz,
                (WGPUBufferMapCallbackInfo){.callback = map_cb});
            wgpuDevicePoll(be->device, 1, NULL);
            void const *mapped = wgpuBufferGetConstMappedRange(be->ts_staging, off, sz);
            if (mapped) {
                uint64_t ts[2];
                memcpy(ts, mapped, sizeof ts);
                be->gpu_time_accum += (double)(ts[1] - ts[0]) * be->ts_period;
            }
            wgpuBufferUnmap(be->ts_staging);
        }
    }
}

static void wgpu_submit_cmdbuf(struct wgpu_backend *be) {
    if (!be->batch_enc) { return; }

    wgpuComputePassEncoderEnd(be->batch_pass);
    wgpuComputePassEncoderRelease(be->batch_pass);
    be->batch_pass = NULL;

    if (be->ts_query) {
        uint32_t const base = (uint32_t)be->uni_pool.cur * 2;
        uint64_t const off  = (uint64_t)be->uni_pool.cur * TS_RESOLVE_ALIGN;
        wgpuCommandEncoderResolveQuerySet(be->batch_enc, be->ts_query,
                                          base, 2, be->ts_resolve, off);
        wgpuCommandEncoderCopyBufferToBuffer(be->batch_enc,
            be->ts_resolve, off, be->ts_staging, off, 2 * sizeof(uint64_t));
    }

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(be->batch_enc, NULL);
    wgpuCommandEncoderRelease(be->batch_enc);
    be->batch_enc = NULL;

    // Bulk-upload all ring data accumulated since the last submit.
    // Ring alloc copies into shadow; one QueueWriteBuffer per chunk replaces
    // the N per-dispatch uploads that were here before.
    {
        struct uniform_ring *ring = &be->uni_pool.rings[be->uni_pool.cur];
        for (int i = 0; i < ring->n; i++) {
            if (ring->chunks[i].used) {
                struct wgpu_ring_chunk *wc = ring->chunks[i].handle;
                wgpuQueueWriteBuffer(be->queue, wc->buf, 0,
                                     ring->chunks[i].mapped, ring->chunks[i].used);
            }
        }
    }

    int cur = be->uni_pool.cur;
    be->frame_submitted[cur] =
        wgpuQueueSubmitForIndex(be->queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    be->frame_has_work[cur] = 1;
    be->batch_has_dispatch  = 0;

    uniform_ring_pool_rotate(&be->uni_pool);
}

static void wgpu_flush(struct umbra_backend *base) {
    struct wgpu_backend *be = (struct wgpu_backend *)base;

    wgpu_submit_cmdbuf(be);
    uniform_ring_pool_drain_all(&be->uni_pool);

    // Copyback: for each writable buffer, read data back from GPU.
    for (int i = 0; i < be->batch_n_copies; i++) {
        struct wgpu_copyback *c = &be->batch_copies[i];
        // Create a staging buffer, copy, map, read back.
        size_t aligned_sz = (c->bytes + 3) & ~(size_t)3;
        WGPUBuffer staging = wgpuDeviceCreateBuffer(be->device, &(WGPUBufferDescriptor){
            .usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst,
            .size  = aligned_sz,
        });
        WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(be->device, NULL);
        wgpuCommandEncoderCopyBufferToBuffer(enc, c->buf, 0, staging, 0, aligned_sz);
        WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, NULL);
        wgpuCommandEncoderRelease(enc);

        WGPUSubmissionIndex si = wgpuQueueSubmitForIndex(be->queue, 1, &cmd);
        wgpuCommandBufferRelease(cmd);
        wgpuDevicePoll(be->device, 1, &si);

        // Map the staging buffer synchronously.
        wgpuBufferMapAsync(staging, WGPUMapMode_Read, 0, aligned_sz,
            (WGPUBufferMapCallbackInfo){
                .callback = map_cb,
            });
        wgpuDevicePoll(be->device, 1, NULL);
        void const *mapped = wgpuBufferGetConstMappedRange(staging, 0, aligned_sz);
        if (mapped) {
            memcpy(c->host, mapped, c->bytes);
        }
        wgpuBufferUnmap(staging);
        wgpuBufferRelease(staging);
    }
    be->batch_n_copies = 0;

    // Cache entries persist across flushes so shadows can skip re-upload for
    // unchanged data (e.g. font atlases).  Reset per-batch copyback tracking,
    // and clear shadows for writable entries: the GPU has modified their data,
    // so the shadow no longer reflects what's on the GPU buffer.
    for (int i = 0; i < be->batch_cache_n; i++) {
        be->batch_cache[i].copy_tracked = 0;
        if (be->batch_cache[i].writable) {
            free(be->batch_cache[i].shadow);
            be->batch_cache[i].shadow = NULL;
        }
    }

    // Invalidate cached bind group — it references batch cache buffers.
    if (be->cached_bg) {
        wgpuBindGroupRelease(be->cached_bg);
        be->cached_bg = NULL;
    }
}

// ---------------------------------------------------------------------------
//  Program: compile.
// ---------------------------------------------------------------------------

static struct umbra_program *wgpu_compile(struct umbra_backend *base,
                                          struct umbra_basic_block const *bb) {
    struct wgpu_backend *be = (struct wgpu_backend *)base;

    wgpu_had_error = 0;
    int spirv_words = 0, max_ptr = -1, total_bufs = 0, n_deref = 0, push_words = 0;
    struct deref_info *deref = 0;
    uint8_t *buf_rw = 0;
    uint32_t *spirv = build_spirv(bb, SPIRV_PUSH_VIA_SSBO | SPIRV_NO_16BIT_TYPES,
                                   &spirv_words, &max_ptr, &total_bufs,
                                   &n_deref, &deref, &push_words, &buf_rw);
    if (!spirv) { return 0; }

    WGPUShaderSourceSPIRV spirv_src = {
        .chain    = {.sType = WGPUSType_ShaderSourceSPIRV},
        .codeSize = (uint32_t)spirv_words,
        .code     = spirv,
    };
    WGPUShaderModule shader = wgpuDeviceCreateShaderModule(be->device,
        &(WGPUShaderModuleDescriptor){
            .nextInChain = &spirv_src.chain,
        });
    if (!shader) { free(spirv); free(deref); return 0; }

    // Bind group layout: one storage buffer per data slot + one for push data.
    int n_desc = total_bufs + 1;
    WGPUBindGroupLayoutEntry *entries =
        calloc((size_t)n_desc, sizeof *entries);
    // Binding 0 (user uniforms) and the last binding (push data) use dynamic
    // offsets so we can reuse the bind group across dispatches that differ only
    // in ring-allocated uniform/push data.
    for (int i = 0; i < n_desc; i++) {
        entries[i] = (WGPUBindGroupLayoutEntry){
            .binding    = (uint32_t)i,
            .visibility = WGPUShaderStage_Compute,
            .buffer = {
                .type             = WGPUBufferBindingType_Storage,
                .hasDynamicOffset = i == 0 || i == n_desc - 1,
            },
        };
    }
    WGPUBindGroupLayout bg_layout = wgpuDeviceCreateBindGroupLayout(be->device,
        &(WGPUBindGroupLayoutDescriptor){
            .entryCount = (size_t)n_desc,
            .entries    = entries,
        });
    free(entries);

    WGPUPipelineLayout pipe_layout = wgpuDeviceCreatePipelineLayout(be->device,
        &(WGPUPipelineLayoutDescriptor){
            .bindGroupLayoutCount  = 1,
            .bindGroupLayouts      = &bg_layout,
        });

    WGPUComputePipeline pipeline = wgpuDeviceCreateComputePipeline(be->device,
            &(WGPUComputePipelineDescriptor){
                .layout  = pipe_layout,
                .compute = {
                    .module     = shader,
                    .entryPoint = {.data = "main", .length = 4},
                },
            });
    if (!pipeline || wgpu_had_error) {
        if (pipeline) { wgpuComputePipelineRelease(pipeline); }
        wgpuPipelineLayoutRelease(pipe_layout);
        wgpuBindGroupLayoutRelease(bg_layout);
        wgpuShaderModuleRelease(shader);
        free(spirv); free(deref);
        return 0;
    }

    struct wgpu_program *p = calloc(1, sizeof *p);
    p->be          = be;
    p->shader      = shader;
    p->bg_layout   = bg_layout;
    p->pipe_layout = pipe_layout;
    p->pipeline    = pipeline;
    p->max_ptr     = max_ptr;
    p->total_bufs  = total_bufs;
    p->n_deref     = n_deref;
    p->push_words  = push_words;
    p->deref       = deref;
    p->buf_rw      = buf_rw;
    p->spirv       = spirv;
    p->spirv_words = spirv_words;
    return &p->base;
}

// ---------------------------------------------------------------------------
//  Program: queue dispatch.
// ---------------------------------------------------------------------------

static void wgpu_program_queue(struct umbra_program *prog, int l, int t,
                               int r, int b, struct umbra_buf buf[]) {
    struct wgpu_program *p  = (struct wgpu_program *)prog;
    struct wgpu_backend *be = p->be;

    int w = r - l, h = b - t;
    if (w <= 0 || h <= 0) { return; }

    begin_batch(be);

    int n = p->total_bufs;

    // Resolve bindings.
    WGPUBuffer bind_buf   [32];
    uint64_t   bind_offset[32];
    uint64_t   bind_size  [32];
    assume(n <= 32);
    for (int i = 0; i < n; i++) {
        bind_buf   [i] = NULL;
        bind_offset[i] = 0;
        bind_size  [i] = 0;
    }

    for (int i = 0; i <= p->max_ptr; i++) {
        if (buf[i].ptr && buf[i].sz) {
            uint8_t const rw = p->buf_rw[i];
            if (!(rw & BUF_WRITTEN) && !buf[i].row_bytes) {
                // Ring alloc copies data into shadow; bulk-uploaded at submit.
                struct uniform_ring_loc loc =
                    uniform_ring_pool_alloc(&be->uni_pool, buf[i].ptr, buf[i].sz);
                struct wgpu_ring_chunk *chunk = loc.handle;
                bind_buf   [i] = chunk->buf;
                bind_offset[i] = loc.offset;
                bind_size  [i] = (buf[i].sz + 3) & ~(size_t)3;
            } else {
                uint64_t sz = buf[i].sz;
                if (sz < 4) { sz = 4; }
                int idx = cache_buf(be, buf[i].ptr, buf[i].sz, rw);
                bind_buf [i] = be->batch_cache[idx].buf;
                bind_size[i] = be->batch_cache[idx].size;
            }
        }
    }

    // Push constant data.
    uint32_t push_data[67] = {0};
    push_data[0] = (uint32_t)w;
    push_data[1] = (uint32_t)l;
    push_data[2] = (uint32_t)t;
    for (int i = 0; i <= p->max_ptr; i++) {
        push_data[3 + i] = (uint32_t)buf[i].sz;
        push_data[3 + p->total_bufs + i] = (uint32_t)buf[i].row_bytes;
    }

    // Resolve derefs.
    for (int d = 0; d < p->n_deref; d++) {
        char *base = (char *)buf[p->deref[d].src_buf].ptr;
        void  *derived;
        size_t dsz, drb;
        memcpy(&derived, base + p->deref[d].off,      sizeof derived);
        memcpy(&dsz,     base + p->deref[d].off + 8,  sizeof dsz);
        memcpy(&drb,     base + p->deref[d].off + 16, sizeof drb);
        int bi = p->deref[d].buf_idx;

        int idx = cache_buf(be, derived, dsz, p->buf_rw[bi]);
        bind_buf [bi] = be->batch_cache[idx].buf;
        bind_size[bi] = be->batch_cache[idx].size;

        push_data[3 + bi] = (uint32_t)dsz;
        push_data[3 + p->total_bufs + bi] = (uint32_t)drb;
    }

    // Fill unbound slots with dummy buffers.
    for (int i = 0; i < n; i++) {
        if (!bind_buf[i]) {
            int idx = cache_buf(be, 0, 0, 1);
            bind_buf [i] = be->batch_cache[idx].buf;
            bind_size[i] = be->batch_cache[idx].size;
        }
    }

    // Push data via ring; bulk-uploaded at submit.
    size_t push_sz = (size_t)p->push_words * sizeof(uint32_t);
    struct uniform_ring_loc push_loc =
        uniform_ring_pool_alloc(&be->uni_pool, push_data, push_sz);
    struct wgpu_ring_chunk *push_chunk = push_loc.handle;

    // Reuse bind group if buffer bindings haven't changed.  Bindings 0 (user
    // uniforms) and n (push data) use dynamic offsets, so only buffer handles
    // matter for those — offsets are passed at SetBindGroup time.
    _Bool bg_hit = be->cached_bg
                && be->cached_bg_n == n
                && be->cached_bg_bufs[0]    == bind_buf[0]
                && be->cached_bg_sz  [0]    == bind_size[0]
                && be->cached_bg_push_buf   == push_chunk->buf
                && !memcmp(be->cached_bg_bufs + 1, bind_buf + 1,
                           (size_t)(n - 1) * sizeof bind_buf[0])
                && !memcmp(be->cached_bg_off  + 1, bind_offset + 1,
                           (size_t)(n - 1) * sizeof bind_offset[0])
                && !memcmp(be->cached_bg_sz   + 1, bind_size + 1,
                           (size_t)(n - 1) * sizeof bind_size[0]);
    if (!bg_hit) {
        if (be->cached_bg) { wgpuBindGroupRelease(be->cached_bg); }
        int n_bg = n + 1;
        WGPUBindGroupEntry bg_entries[33];
        for (int i = 0; i < n; i++) {
            bg_entries[i] = (WGPUBindGroupEntry){
                .binding = (uint32_t)i,
                .buffer  = bind_buf[i],
                .offset  = i == 0 ? 0 : bind_offset[i],
                .size    = bind_size[i],
            };
        }
        size_t push_sz_aligned = (push_sz + 3) & ~(size_t)3;
        bg_entries[n] = (WGPUBindGroupEntry){
            .binding = (uint32_t)n,
            .buffer  = push_chunk->buf,
            .offset  = 0,
            .size    = push_sz_aligned,
        };
        be->cached_bg = wgpuDeviceCreateBindGroup(be->device,
            &(WGPUBindGroupDescriptor){
                .layout     = p->bg_layout,
                .entryCount = (size_t)n_bg,
                .entries    = bg_entries,
            });
        be->cached_bg_n = n;
        be->cached_bg_push_buf = push_chunk->buf;
        memcpy(be->cached_bg_bufs, bind_buf,    (size_t)n * sizeof bind_buf[0]);
        memcpy(be->cached_bg_off,  bind_offset, (size_t)n * sizeof bind_offset[0]);
        memcpy(be->cached_bg_sz,   bind_size,   (size_t)n * sizeof bind_size[0]);
    }

    // Barrier: end/begin compute pass between dispatches.
    if (be->batch_has_dispatch) {
        wgpuComputePassEncoderEnd(be->batch_pass);
        wgpuComputePassEncoderRelease(be->batch_pass);
        be->batch_pass = begin_pass(be, 0);
    }

    uint32_t dyn_offsets[2] = {
        (uint32_t)bind_offset[0],   // uniform buffer offset
        (uint32_t)push_loc.offset,  // push data offset
    };
    wgpuComputePassEncoderSetPipeline(be->batch_pass, p->pipeline);
    wgpuComputePassEncoderSetBindGroup(be->batch_pass, 0, be->cached_bg,
                                       2, dyn_offsets);

    uint32_t gx = ((uint32_t)w + SPIRV_WG_SIZE - 1) / SPIRV_WG_SIZE;
    wgpuComputePassEncoderDispatchWorkgroups(be->batch_pass, gx, (uint32_t)h, 1);

    be->batch_has_dispatch = 1;

    if (uniform_ring_pool_should_rotate(&be->uni_pool)) {
        wgpu_submit_cmdbuf(be);
    }
}

// ---------------------------------------------------------------------------
//  Program: dump and free.
// ---------------------------------------------------------------------------

static void wgpu_program_dump(struct umbra_program const *prog, FILE *f) {
    struct wgpu_program const *p = (struct wgpu_program const *)prog;
    char tmp[] = "/tmp/umbra_spirv_XXXXXX";
    int fd = mkstemp(tmp);
    if (fd >= 0) {
        size_t sz = (size_t)p->spirv_words * sizeof(uint32_t);
        write(fd, p->spirv, sz);
        close(fd);
        char cmd[256];
        snprintf(cmd, sizeof cmd, "spirv-dis %s 2>/dev/null", tmp);
        FILE *dis = popen(cmd, "r");
        if (dis) {
            char line[512];
            while (fgets(line, (int)sizeof line, dis)) {
                fputs(line, f);
            }
            pclose(dis);
        }
        unlink(tmp);
    }
}

static void wgpu_program_free(struct umbra_program *prog) {
    struct wgpu_program *p = (struct wgpu_program *)prog;
    wgpuComputePipelineRelease(p->pipeline);
    wgpuPipelineLayoutRelease(p->pipe_layout);
    wgpuBindGroupLayoutRelease(p->bg_layout);
    wgpuShaderModuleRelease(p->shader);
    free(p->spirv);
    free(p->deref);
    free(p->buf_rw);
    free(p);
}

// ---------------------------------------------------------------------------
//  Backend: creation and destruction.
// ---------------------------------------------------------------------------

static int wgpu_ring_rotations(struct umbra_backend const *base) {
    return ((struct wgpu_backend const *)base)->uni_pool.rotations;
}
static double wgpu_gpu_time(struct umbra_backend const *base) {
    return ((struct wgpu_backend const *)base)->gpu_time_accum;
}

static void wgpu_free(struct umbra_backend *base) {
    wgpu_flush(base);
    struct wgpu_backend *be = (struct wgpu_backend *)base;
    uniform_ring_pool_free(&be->uni_pool);
    for (int i = 0; i < be->batch_cache_n; i++) {
        wgpuBufferRelease(be->batch_cache[i].buf);
        free(be->batch_cache[i].shadow);
    }
    for (int i = 0; i < be->free_bufs_n; i++) {
        wgpuBufferRelease(be->free_bufs[i].buf);
    }
    if (be->ts_query) {
        wgpuQuerySetRelease(be->ts_query);
        wgpuBufferRelease(be->ts_resolve);
        wgpuBufferRelease(be->ts_staging);
    }
    wgpuQueueRelease(be->queue);
    wgpuDeviceRelease(be->device);
    wgpuAdapterRelease(be->adapter);
    wgpuInstanceRelease(be->instance);
    free(be->free_bufs);
    free(be->batch_copies);
    free(be->batch_cache);
    free(be);
}

static void wgpu_queue_fn(struct umbra_program *p, int l, int t, int r, int b,
                          struct umbra_buf buf[]) {
    wgpu_program_queue(p, l, t, r, b, buf);
}
static void wgpu_dump_fn(struct umbra_program const *p, FILE *f) {
    wgpu_program_dump(p, f);
}
static void wgpu_free_fn(struct umbra_program *p) {
    wgpu_program_free(p);
}

static struct umbra_program *wgpu_compile_fn(struct umbra_backend *be,
                                              struct umbra_basic_block const *bb) {
    struct umbra_program *p = wgpu_compile(be, bb);
    if (p) {
        p->queue   = wgpu_queue_fn;
        p->dump    = wgpu_dump_fn;
        p->free    = wgpu_free_fn;
        p->backend = be;
    }
    return p;
}

struct umbra_backend *umbra_backend_wgpu(void) {
    WGPUInstanceFeatureName inst_feats[] = {
        WGPUInstanceFeatureName_ShaderSourceSPIRV,
    };
    WGPUInstanceExtras instance_extras = {
        .chain    = {.sType = (WGPUSType)WGPUSType_InstanceExtras},
        .backends = WGPUInstanceBackend_Metal,
    };
    WGPUInstance instance = wgpuCreateInstance(&(WGPUInstanceDescriptor){
        .nextInChain         = &instance_extras.chain,
        .requiredFeatureCount = sizeof inst_feats / sizeof inst_feats[0],
        .requiredFeatures     = inst_feats,
    });
    if (!instance) { return 0; }

    // Enumerate adapters and pick the first one.
    size_t n_adapters = wgpuInstanceEnumerateAdapters(instance, NULL, NULL);
    if (!n_adapters) { wgpuInstanceRelease(instance); return 0; }
    WGPUAdapter *adapters = calloc(n_adapters, sizeof *adapters);
    wgpuInstanceEnumerateAdapters(instance, NULL, adapters);
    WGPUAdapter adapter = adapters[0];
    for (size_t i = 1; i < n_adapters; i++) {
        wgpuAdapterRelease(adapters[i]);
    }
    free(adapters);

    // Request the adapter's actual limits so we get the full storage buffer
    // count (default 8 is too few for push-via-SSBO on shaders with 8 bufs).
    WGPULimits adapter_limits = {0};
    wgpuAdapterGetLimits(adapter, &adapter_limits);
    adapter_limits.nextInChain = NULL;

    _Bool has_ts = wgpuAdapterHasFeature(adapter, WGPUFeatureName_TimestampQuery);
    WGPUFeatureName features[2];
    int n_features = 0;
    features[n_features++] = WGPUFeatureName_ShaderF16;
    if (has_ts) { features[n_features++] = WGPUFeatureName_TimestampQuery; }
    WGPUDevice dev = NULL;
    WGPUDeviceDescriptor dev_desc    = WGPU_DEVICE_DESCRIPTOR_INIT;
    dev_desc.requiredFeatureCount     = (size_t)n_features;
    dev_desc.requiredFeatures         = features;
    dev_desc.requiredLimits           = &adapter_limits;
    dev_desc.uncapturedErrorCallbackInfo.callback = (WGPUUncapturedErrorCallback)error_cb;
    wgpuAdapterRequestDevice(adapter, &dev_desc,
        (WGPURequestDeviceCallbackInfo){
            .callback  = device_request_cb,
            .userdata1 = &dev,
        });
    if (!dev) {
        wgpuAdapterRelease(adapter);
        wgpuInstanceRelease(instance);
        return 0;
    }

    WGPUQueue queue = wgpuDeviceGetQueue(dev);

    struct wgpu_backend *be = calloc(1, sizeof *be);
    be->instance = instance;
    be->adapter  = adapter;
    be->device   = dev;
    be->queue    = queue;
    if (has_ts) {
        be->ts_query = wgpuDeviceCreateQuerySet(dev, &(WGPUQuerySetDescriptor){
            .type  = WGPUQueryType_Timestamp,
            .count = WGPU_N_FRAMES * 2,
        });
        be->ts_resolve = wgpuDeviceCreateBuffer(dev, &(WGPUBufferDescriptor){
            .usage = WGPUBufferUsage_QueryResolve | WGPUBufferUsage_CopySrc,
            .size  = WGPU_N_FRAMES * TS_RESOLVE_ALIGN,
        });
        be->ts_staging = wgpuDeviceCreateBuffer(dev, &(WGPUBufferDescriptor){
            .usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst,
            .size  = WGPU_N_FRAMES * TS_RESOLVE_ALIGN,
        });
        be->ts_period = (double)wgpuQueueGetTimestampPeriod(queue) * 1e-9;
    }
    be->base = (struct umbra_backend){
        .compile        = wgpu_compile_fn,
        .flush          = wgpu_flush,
        .free           = (void (*)(struct umbra_backend *))wgpu_free,
        .ring_rotations = wgpu_ring_rotations,
        .gpu_time       = wgpu_gpu_time,
    };

    // Initialize uniform ring pool.
    be->uni_pool = (struct uniform_ring_pool){
        .n          = WGPU_N_FRAMES,
        .high_water = WGPU_RING_HIGH_WATER,
        .ctx        = be,
        .wait_frame = wgpu_wait_frame,
    };
    for (int i = 0; i < WGPU_N_FRAMES; i++) {
        be->uni_pool.rings[i] = (struct uniform_ring){
            .align      = 256,
            .ctx        = be,
            .new_chunk  = wgpu_ring_new_chunk,
            .free_chunk = wgpu_ring_free_chunk,
        };
    }

    return &be->base;
}

#endif
