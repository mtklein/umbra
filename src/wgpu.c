#include "assume.h"
#include "gpu_buf_cache.h"
#include "count.h"
#include "spirv.h"
#include "uniform_ring.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if !defined(__APPLE__) || !defined(__aarch64__) || defined(__wasm__)

// TODO: add a second wasm build config (e.g. build/wasm_browser.ninja) that
// builds browser-hosted test, bench, and demo running the interp and webgpu
// backends.  The current wasm config targets wasi-sdk/wasmtime, which has no
// WebGPU and no DOM — only the interp backend runs, and only headlessly,
// which is why umbra_backend_wgpu() is stubbed out below on __wasm__.
//
// Sketch:
//   - Use Emscripten (emcc) instead of clang+wasi-sdk.  Emscripten ships a
//     navigator.gpu-backed webgpu.h whose C API closely tracks wgpu-native,
//     so this file should port with minimal glue (-sUSE_WEBGPU=1, and the
//     -sASYNCIFY path for the async adapter/device requests).  Metal/Vulkan
//     backends stay stubbed out.
//   - Each tool gets an emcc link step producing .js + .wasm + an HTML shell:
//       test   -> headless run, forward stdout to console; exit code via
//                 emscripten_force_exit or postMessage to a harness.
//       bench  -> needs emscripten_get_now() instead of clock_gettime for
//                 wall time; otherwise tools/bench.c should be portable.
//       demo   -> SDL3 path: Emscripten has SDL3 support, but the main loop
//                 must yield (emscripten_set_main_loop or -sASYNCIFY) since
//                 the browser won't tolerate a blocking while-loop.
//   - Tests currently shard via --shards/--shard; in the browser that
//     becomes query-string args parsed in the HTML shell.
//   - CI/run story: headless Chrome via puppeteer or `chrome --headless`
//     with --enable-unsafe-webgpu; wasmtime lane stays as-is for the
//     interp-only smoke test.
//
// This note lives here rather than in build/wasm.ninja because configure.py
// regenerates that file from scratch and strips free-form comments.

struct umbra_backend* umbra_backend_wgpu(void) { return 0; }

#else

#include <wgpu.h>

enum { WGPU_N_FRAMES = 2, WGPU_RING_HIGH_WATER = 64 * 1024, TS_RESOLVE_ALIGN = 256 };

struct wgpu_backend {
    struct umbra_backend      base;
    WGPUInstance              instance;
    WGPUAdapter               adapter;
    WGPUDevice                device;
    WGPUQueue                 queue;

    WGPUCommandEncoder        batch_enc;
    WGPUComputePassEncoder    batch_pass;

    uint32_t                  max_dyn_storage;
    uint32_t                  max_dyn_uniform;

    WGPUSubmissionIndex       frame_submitted[WGPU_N_FRAMES];
    _Bool                     frame_has_work [WGPU_N_FRAMES];
    _Bool                     had_error; int :8, :32;

    struct gpu_buf_cache          cache;

    struct uniform_ring_pool  uni_pool;
    struct uniform_ring_pool  uni_pool_uniform;
    int                       total_dispatches; int :32;
    size_t                    total_upload_bytes;

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
    int push_words, bindings;
    struct buffer_binding *binding;

    uint8_t          *buf_rw;
    uint8_t          *buf_shift;
    uint8_t          *buf_is_uniform;

    // When true, every read-only binding uses a dynamic offset so the
    // bind group survives across dispatches whose offsets differ.  When
    // false, only push data is dynamic -- the pipeline layout's dynamic
    // slot budget wasn't enough to cover every read-only binding.
    _Bool     dynamic_offset_bindings, pad[3];
    int       spirv_words;
    uint32_t *spirv;

    // Bind group cache: reuse across dispatches when buffer handles are
    // stable.  Per-program so alternating programs don't clobber each other,
    // and per-ring (WGPU_N_FRAMES slots) so ring rotation doesn't either.
    struct {
        WGPUBindGroup bg;
        WGPUBuffer    push_buf;
        WGPUBuffer    bufs[32];
        uint64_t      off [32];  // only read when dynamic_offset_bindings=0
        uint64_t      sz  [32];
        int           n, :32;
    } cached[WGPU_N_FRAMES];
};

struct wgpu_ring_chunk {
    WGPUBuffer buf;
    void      *mapped;
};

static struct uniform_ring_chunk wgpu_ring_new_chunk_inner(size_t min_bytes, void *ctx,
                                                           WGPUBufferUsage extra_usage) {
    struct wgpu_backend *be = ctx;
    size_t cap = min_bytes > WGPU_RING_HIGH_WATER ? min_bytes : WGPU_RING_HIGH_WATER;
    struct wgpu_ring_chunk *chunk = calloc(1, sizeof *chunk);
    chunk->buf = wgpuDeviceCreateBuffer(be->device, &(WGPUBufferDescriptor){
        .usage = extra_usage | WGPUBufferUsage_CopyDst,
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

static struct uniform_ring_chunk wgpu_ring_new_chunk(size_t min_bytes, void *ctx) {
    return wgpu_ring_new_chunk_inner(min_bytes, ctx, WGPUBufferUsage_Storage);
}

static struct uniform_ring_chunk wgpu_ring_new_chunk_uniform(size_t min_bytes, void *ctx) {
    return wgpu_ring_new_chunk_inner(min_bytes, ctx, WGPUBufferUsage_Uniform);
}

static void wgpu_ring_free_chunk(void *handle, void *ctx) {
    (void)ctx;
    struct wgpu_ring_chunk *chunk = handle;
    wgpuBufferRelease(chunk->buf);
    free(chunk->mapped);
    free(chunk);
}

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

static void error_cb(WGPUDevice const *dev, WGPUErrorType type, WGPUStringView msg,
                     void *u1, void *u2) {
    (void)dev; (void)u2;
    if (type != WGPUErrorType_NoError) {
        ((struct wgpu_backend *)u1)->had_error = 1;
        dprintf(2, "wgpu error: %.*s\n", (int)msg.length, msg.data);
    }
}

static WGPUComputePassEncoder begin_pass(struct wgpu_backend *be, _Bool begin_ts) {
    WGPUPassTimestampWrites ts = {
        .querySet                = be->ts_query,
        .beginningOfPassWriteIndex = begin_ts ? (uint32_t)be->uni_pool.cur * 2
                                              : WGPU_QUERY_SET_INDEX_UNDEFINED,
        .endOfPassWriteIndex     = (uint32_t)be->uni_pool.cur * 2 + 1,
    };
    return wgpuCommandEncoderBeginComputePass(be->batch_enc,
        &(WGPUComputePassDescriptor){.timestampWrites = &ts});
}

static void begin_batch(struct wgpu_backend *be) {
    if (be->batch_enc) { return; }
    be->batch_enc  = wgpuDeviceCreateCommandEncoder(be->device, NULL);
    be->batch_pass = begin_pass(be, 1);
}

static gpu_buf wgpu_cache_alloc(size_t size, void *ctx) {
    struct wgpu_backend *be = ctx;
    uint64_t alloc_size = (size + 3) & ~(uint64_t)3;
    if (alloc_size < 4) { alloc_size = 4; }
    WGPUBuffer buf = wgpuDeviceCreateBuffer(be->device, &(WGPUBufferDescriptor){
        .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst
               | WGPUBufferUsage_CopySrc,
        .size  = alloc_size,
    });
    return (gpu_buf){.ptr = buf, .size = (size_t)alloc_size};
}

// WebGPU requires buffer writes to be 4-byte aligned.
static void wgpu_cache_upload(gpu_buf buf, void const *data, size_t bytes,
                              void *ctx) {
    struct wgpu_backend *be = ctx;
    WGPUBuffer wbuf = buf.ptr;
    size_t aligned = (bytes + 3) & ~(size_t)3;
    if (aligned == bytes) {
        wgpuQueueWriteBuffer(be->queue, wbuf, 0, data, bytes);
    } else {
        char tmp[4] = {0};
        __builtin_memcpy(tmp, (char const *)data + (bytes & ~(size_t)3), bytes & 3);
        wgpuQueueWriteBuffer(be->queue, wbuf, 0, data, bytes & ~(size_t)3);
        wgpuQueueWriteBuffer(be->queue, wbuf, bytes & ~(size_t)3, tmp, 4);
    }
}

static void wgpu_cache_download(gpu_buf buf, void *host, size_t bytes, void *ctx) {
    struct wgpu_backend *be = ctx;
    size_t aligned_sz = (bytes + 3) & ~(size_t)3;
    WGPUBuffer staging = wgpuDeviceCreateBuffer(be->device, &(WGPUBufferDescriptor){
        .usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst,
        .size  = aligned_sz,
    });
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(be->device, NULL);
    wgpuCommandEncoderCopyBufferToBuffer(enc, buf.ptr, 0, staging, 0, aligned_sz);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, NULL);
    wgpuCommandEncoderRelease(enc);

    WGPUSubmissionIndex si = wgpuQueueSubmitForIndex(be->queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    wgpuDevicePoll(be->device, 1, &si);

    wgpuBufferMapAsync(staging, WGPUMapMode_Read, 0, aligned_sz,
        (WGPUBufferMapCallbackInfo){.callback = map_cb});
    wgpuDevicePoll(be->device, 1, NULL);
    void const *mapped = wgpuBufferGetConstMappedRange(staging, 0, aligned_sz);
    if (mapped) { memcpy(host, mapped, bytes); }
    wgpuBufferUnmap(staging);
    wgpuBufferRelease(staging);
}

static void wgpu_cache_release(gpu_buf buf, void *ctx) {
    (void)ctx;
    wgpuBufferRelease(buf.ptr);
}

static void wgpu_wait_frame_noop(int frame, void *ctx) { (void)frame; (void)ctx; }

static void wgpu_wait_frame(int frame, void *ctx) {
    struct wgpu_backend *be = ctx;
    if (be->frame_has_work[frame]) {
        wgpuDevicePoll(be->device, 1, &be->frame_submitted[frame]);
        be->frame_has_work[frame] = 0;
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

static void wgpu_submit_cmdbuf(struct wgpu_backend *be) {
    if (be->batch_enc) {
        wgpuComputePassEncoderEnd(be->batch_pass);
        wgpuComputePassEncoderRelease(be->batch_pass);
        be->batch_pass = NULL;

        uint32_t const base = (uint32_t)be->uni_pool.cur * 2;
        uint64_t const off  = (uint64_t)be->uni_pool.cur * TS_RESOLVE_ALIGN;
        wgpuCommandEncoderResolveQuerySet(be->batch_enc, be->ts_query,
                                          base, 2, be->ts_resolve, off);
        wgpuCommandEncoderCopyBufferToBuffer(be->batch_enc,
            be->ts_resolve, off, be->ts_staging, off, 2 * sizeof(uint64_t));

        WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(be->batch_enc, NULL);
        wgpuCommandEncoderRelease(be->batch_enc);
        be->batch_enc = NULL;

        // Bulk-upload all ring data accumulated since the last submit.
        // Ring alloc copies into the chunk's shadow buffer; one QueueWriteBuffer
        // per chunk replaces the N per-dispatch uploads that were here before.
        {
            struct uniform_ring *rings[2] = {
                &be->uni_pool        .rings[be->uni_pool        .cur],
                &be->uni_pool_uniform.rings[be->uni_pool_uniform.cur],
            };
            for (int r = 0; r < 2; r++) {
                struct uniform_ring *ring = rings[r];
                for (int i = 0; i < ring->n; i++) {
                    if (ring->chunks[i].used) {
                        struct wgpu_ring_chunk *wc = ring->chunks[i].handle;
                        wgpuQueueWriteBuffer(be->queue, wc->buf, 0,
                                             ring->chunks[i].mapped, ring->chunks[i].used);
                        be->total_upload_bytes += ring->chunks[i].used;
                    }
                }
            }
        }

        int cur = be->uni_pool.cur;
        be->frame_submitted[cur] =
            wgpuQueueSubmitForIndex(be->queue, 1, &cmd);
        wgpuCommandBufferRelease(cmd);
        be->frame_has_work[cur] = 1;

        uniform_ring_pool_rotate(&be->uni_pool);
        uniform_ring_pool_rotate(&be->uni_pool_uniform);
    }
}

static void wgpu_flush(struct umbra_backend *base) {
    struct wgpu_backend *be = (struct wgpu_backend *)base;

    wgpu_submit_cmdbuf(be);
    uniform_ring_pool_drain_all(&be->uni_pool);
    uniform_ring_pool_drain_all(&be->uni_pool_uniform);
    gpu_buf_cache_copyback (&be->cache);
    gpu_buf_cache_end_batch(&be->cache);
}

static struct umbra_program* wgpu_compile(struct umbra_backend *base,
                                          struct umbra_flat_ir const *ir) {
    struct wgpu_backend *be = (struct wgpu_backend *)base;

    be->had_error = 0;
    // SPIRV_NO_CONTRACT forces naga to preserve our IR's mul/add split on
    // the wgpu path — see src/metal.c for the matching Metal pragma and
    // UMBRA_NO_BACKEND_FP_CONTRACT rationale.
    int spirv_flags = SPIRV_PUSH_VIA_SSBO;
#if defined(UMBRA_NO_BACKEND_FP_CONTRACT)
    spirv_flags |= SPIRV_NO_CONTRACT;
#endif
    struct spirv_result const sr = build_spirv(ir, spirv_flags);

    WGPUShaderSourceSPIRV spirv_src = {
        .chain    = {.sType = WGPUSType_ShaderSourceSPIRV},
        .codeSize = (uint32_t)sr.spirv_words,
        .code     = sr.spirv,
    };
    WGPUShaderModule shader = wgpuDeviceCreateShaderModule(be->device,
        &(WGPUShaderModuleDescriptor){
            .nextInChain = &spirv_src.chain,
        });
    if (!shader) { free(sr.spirv); return 0; }

    int descs = sr.total_bufs + 1;
    WGPUBindGroupLayoutEntry *entries =
        calloc((size_t)descs, sizeof *entries);

    // Count read-only bindings split by descriptor type.  If both classes
    // (storage RO + push, uniform RO) fit within their respective dynamic
    // budgets, mark every RO binding hasDynamicOffset=true so the bind group
    // cache survives across dispatches whose offsets change.  Otherwise fall
    // back to only-push-is-dynamic; bg cache thrashes but we stay within
    // limits.
    int n_ro_storage = 0, n_ro_uniform = 0;
    for (int i = 0; i < descs - 1; i++) {
        if (!(sr.buf_rw[i] & BUF_WRITTEN)) {
            if (sr.buf_is_uniform[i]) { n_ro_uniform++; }
            else                      { n_ro_storage++; }
        }
    }
    _Bool const dynamic_offset_bindings =
        (uint32_t)(n_ro_storage + 1) <= be->max_dyn_storage
     && (uint32_t) n_ro_uniform      <= be->max_dyn_uniform;

    for (int i = 0; i < descs - 1; i++) {
        _Bool const is_ring = dynamic_offset_bindings && !(sr.buf_rw[i] & BUF_WRITTEN);
        entries[i] = (WGPUBindGroupLayoutEntry){
            .binding    = (uint32_t)i,
            .visibility = WGPUShaderStage_Compute,
            .buffer = {
                .type             = sr.buf_is_uniform[i] ? WGPUBufferBindingType_Uniform
                                                         : WGPUBufferBindingType_Storage,
                .hasDynamicOffset = is_ring,
            },
        };
    }
    entries[descs - 1] = (WGPUBindGroupLayoutEntry){
        .binding    = (uint32_t)(descs - 1),
        .visibility = WGPUShaderStage_Compute,
        .buffer = {
            .type             = WGPUBufferBindingType_Storage,
            .hasDynamicOffset = 1,
        },
    };
    WGPUBindGroupLayout bg_layout = wgpuDeviceCreateBindGroupLayout(be->device,
        &(WGPUBindGroupLayoutDescriptor){
            .entryCount = (size_t)descs,
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
    if (!pipeline || be->had_error) {
        if (pipeline) { wgpuComputePipelineRelease(pipeline); }
        wgpuPipelineLayoutRelease(pipe_layout);
        wgpuBindGroupLayoutRelease(bg_layout);
        wgpuShaderModuleRelease(shader);
        free(sr.spirv);
        return 0;
    }

    struct wgpu_program *p = calloc(1, sizeof *p);
    p->be          = be;
    p->shader      = shader;
    p->bg_layout   = bg_layout;
    p->pipe_layout = pipe_layout;
    p->pipeline    = pipeline;
    p->max_ptr     = sr.max_ptr;
    p->total_bufs  = sr.total_bufs;
    p->push_words  = sr.push_words;
    p->buf_rw         = sr.buf_rw;
    p->buf_shift      = sr.buf_shift;
    p->buf_is_uniform = sr.buf_is_uniform;
    p->dynamic_offset_bindings = dynamic_offset_bindings;
    p->spirv       = sr.spirv;
    p->spirv_words = sr.spirv_words;

    p->bindings = ir->bindings;
    if (p->bindings) {
        size_t const sz = (size_t)p->bindings * sizeof *p->binding;
        p->binding = malloc(sz);
        __builtin_memcpy(p->binding, ir->binding, sz);
    }

    return &p->base;
}

static void wgpu_program_queue(struct umbra_program *prog,
                               int l, int t, int r, int b) {
    struct wgpu_program *p  = (struct wgpu_program *)prog;
    struct wgpu_backend *be = p->be;

    int w = r - l, h = b - t;

    assume(p->max_ptr + 1 <= 32);
    struct umbra_buf buf[32];
    for (int i = 0; i < p->bindings; i++) {
        buf[p->binding[i].ix] = p->binding[i].buf ? *p->binding[i].buf : p->binding[i].uniforms;
    }

    begin_batch(be);

    int n = p->total_bufs;

    WGPUBuffer bind_buf   [32];
    uint64_t   bind_offset[32];
    uint64_t   bind_size  [32];
    assume(n <= 32);
    for (int i = 0; i < n; i++) {
        bind_buf   [i] = NULL;
        bind_offset[i] = 0;
        bind_size  [i] = 0;
    }

    _Bool pinned[32] = {0};
    for (int k = 0; k < p->bindings; k++) {
        if (!p->binding[k].buf) { pinned[p->binding[k].ix] = 1; }
    }

    for (int i = 0; i <= p->max_ptr; i++) {
        if (buf[i].ptr && buf[i].count) {
            size_t const bytes = (size_t)buf[i].count << p->buf_shift[i];
            uint8_t const rw = p->buf_rw[i];
            if (!(rw & BUF_WRITTEN) && pinned[i]) {
                // Ring alloc copies data into chunk buffer; bulk-uploaded at submit.
                // Uniform-class bindings come from a dedicated pool whose chunks
                // carry only the Uniform usage; mixing usage classes within a
                // single dispatch on the same WGPU buffer is a validation error.
                struct uniform_ring_pool *pool = p->buf_is_uniform[i] ? &be->uni_pool_uniform
                                                                      : &be->uni_pool;
                struct uniform_ring_loc loc =
                    uniform_ring_pool_alloc(pool, buf[i].ptr, bytes);
                struct wgpu_ring_chunk *chunk = loc.handle;
                size_t const align = p->buf_is_uniform[i] ? (size_t)16 : (size_t)4;
                bind_buf   [i] = chunk->buf;
                bind_offset[i] = loc.offset;
                bind_size  [i] = (bytes + align - 1) & ~(align - 1);
            } else {
                int idx = gpu_buf_cache_get(&be->cache, buf[i].ptr, bytes, rw);
                bind_buf [i] = be->cache.entry[idx].buf.ptr;
                bind_size[i] = be->cache.entry[idx].buf.size;
            }
        }
    }

    uint32_t push_data[67] = {0};
    push_data[0] = (uint32_t)w;
    push_data[1] = (uint32_t)l;
    push_data[2] = (uint32_t)t;
    for (int i = 0; i <= p->max_ptr; i++) {
        push_data[3 + i]                    = (uint32_t)buf[i].count;
        push_data[3 + p->total_bufs + i] = (uint32_t)buf[i].stride;
    }

    // Fill unbound slots with dummy buffers.
    for (int i = 0; i < n; i++) {
        if (!bind_buf[i]) {
            int idx = gpu_buf_cache_get(&be->cache, 0, 0, BUF_READ);
            bind_buf [i] = be->cache.entry[idx].buf.ptr;
            bind_size[i] = be->cache.entry[idx].buf.size;
        }
    }

    // Push data via ring; bulk-uploaded at submit.
    size_t push_sz = (size_t)p->push_words * sizeof(uint32_t);
    struct uniform_ring_loc push_loc =
        uniform_ring_pool_alloc(&be->uni_pool, push_data, push_sz);
    struct wgpu_ring_chunk *push_chunk = push_loc.handle;

    // Reuse bind group if buffer handles haven't changed.  When
    // dynamic_offset_bindings is set, every ring-backed binding has
    // hasDynamicOffset=true, so its offset is passed in dyn_offsets[] at
    // SetBindGroup rather than baked into the bg -- the cache then survives
    // offset changes.  When dynamic_offset_bindings is false (budget too
    // tight), offsets are baked and the cache falls back to comparing them too.
    int const ring_ix = be->uni_pool.cur;
    _Bool bg_hit = p->cached[ring_ix].bg
                && p->cached[ring_ix].n == n
                && p->cached[ring_ix].push_buf == push_chunk->buf
                && !memcmp(p->cached[ring_ix].bufs, bind_buf,
                           (size_t)n * sizeof bind_buf[0])
                && !memcmp(p->cached[ring_ix].sz,   bind_size,
                           (size_t)n * sizeof bind_size[0])
                && (p->dynamic_offset_bindings
                    || !memcmp(p->cached[ring_ix].off, bind_offset,
                               (size_t)n * sizeof bind_offset[0]));
    if (!bg_hit) {
        if (p->cached[ring_ix].bg) { wgpuBindGroupRelease(p->cached[ring_ix].bg); }
        int n_bg = n + 1;
        WGPUBindGroupEntry bg_entries[33];
        for (int i = 0; i < n; i++) {
            _Bool const is_ring = p->dynamic_offset_bindings && !(p->buf_rw[i] & BUF_WRITTEN);
            bg_entries[i] = (WGPUBindGroupEntry){
                .binding = (uint32_t)i,
                .buffer  = bind_buf[i],
                .offset  = is_ring ? 0 : bind_offset[i],
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
        p->cached[ring_ix].bg = wgpuDeviceCreateBindGroup(be->device,
            &(WGPUBindGroupDescriptor){
                .layout     = p->bg_layout,
                .entryCount = (size_t)n_bg,
                .entries    = bg_entries,
            });
        p->cached[ring_ix].n = n;
        p->cached[ring_ix].push_buf = push_chunk->buf;
        memcpy(p->cached[ring_ix].bufs, bind_buf,    (size_t)n * sizeof bind_buf[0]);
        memcpy(p->cached[ring_ix].off,  bind_offset, (size_t)n * sizeof bind_offset[0]);
        memcpy(p->cached[ring_ix].sz,   bind_size,   (size_t)n * sizeof bind_size[0]);
    }

    // No explicit barriers between dispatches: the WebGPU spec guarantees that
    // each dispatch is its own synchronization scope, with serial memory
    // semantics.  The runtime inserts any necessary barriers automatically.
    // https://github.com/gpuweb/gpuweb/discussions/4434

    uint32_t dyn_offsets[33];
    int n_dyn = 0;
    if (p->dynamic_offset_bindings) {
        for (int i = 0; i < n; i++) {
            if (!(p->buf_rw[i] & BUF_WRITTEN)) {
                dyn_offsets[n_dyn++] = (uint32_t)bind_offset[i];
            }
        }
    }
    dyn_offsets[n_dyn++] = (uint32_t)push_loc.offset;

    wgpuComputePassEncoderSetPipeline(be->batch_pass, p->pipeline);
    wgpuComputePassEncoderSetBindGroup(be->batch_pass, 0, p->cached[ring_ix].bg,
                                       (size_t)n_dyn, dyn_offsets);

    uint32_t gx = ((uint32_t)w + SPIRV_WG_SIZE - 1) / SPIRV_WG_SIZE;
    wgpuComputePassEncoderDispatchWorkgroups(be->batch_pass, gx, (uint32_t)h, 1);
    be->total_dispatches++;

    if (uniform_ring_pool_should_rotate(&be->uni_pool)
     || uniform_ring_pool_should_rotate(&be->uni_pool_uniform)) {
        wgpu_submit_cmdbuf(be);
    }
}

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
    for (int i = 0; i < WGPU_N_FRAMES; i++) {
        if (p->cached[i].bg) { wgpuBindGroupRelease(p->cached[i].bg); }
    }
    wgpuComputePipelineRelease(p->pipeline);
    wgpuPipelineLayoutRelease(p->pipe_layout);
    wgpuBindGroupLayoutRelease(p->bg_layout);
    wgpuShaderModuleRelease(p->shader);
    free(p->spirv);
    free(p->buf_rw);
    free(p->buf_shift);
    free(p->buf_is_uniform);
    free(p->binding);
    free(p);
}

static struct umbra_backend_stats wgpu_stats(struct umbra_backend const *be) {
    struct wgpu_backend const *wbe = (struct wgpu_backend const *)be;
    return (struct umbra_backend_stats){
        .gpu_sec         = wbe->gpu_time_accum,
        .uniform_ring_rotations = wbe->uni_pool.rotations,
        .dispatches      = wbe->total_dispatches,
        .upload_bytes    = wbe->cache.upload_bytes + wbe->total_upload_bytes,
    };
}

static void wgpu_free(struct umbra_backend *base) {
    wgpu_flush(base);
    struct wgpu_backend *be = (struct wgpu_backend *)base;
    uniform_ring_pool_free(&be->uni_pool);
    uniform_ring_pool_free(&be->uni_pool_uniform);
    gpu_buf_cache_free(&be->cache);
    wgpuQuerySetRelease(be->ts_query);
    wgpuBufferRelease(be->ts_resolve);
    wgpuBufferRelease(be->ts_staging);
    wgpuQueueRelease(be->queue);
    wgpuDeviceRelease(be->device);
    wgpuAdapterRelease(be->adapter);
    wgpuInstanceRelease(be->instance);
    free(be);
}

static struct umbra_program* wgpu_compile_fn(struct umbra_backend *be,
                                              struct umbra_flat_ir const *ir) {
    struct umbra_program *p = wgpu_compile(be, ir);
    p->queue   = wgpu_program_queue;
    p->dump    = wgpu_program_dump;
    p->free    = wgpu_program_free;
    p->backend = be;
    return p;
}

struct umbra_backend* umbra_backend_wgpu(void) {
    WGPUInstanceFeatureName inst_feats[] = {
        WGPUInstanceFeatureName_ShaderSourceSPIRV,
    };
    WGPUInstanceExtras instance_extras = {
        .chain    = {.sType = (WGPUSType)WGPUSType_InstanceExtras},
        .backends = WGPUInstanceBackend_Metal,
    };
    WGPUInstance instance = wgpuCreateInstance(&(WGPUInstanceDescriptor){
        .nextInChain         = &instance_extras.chain,
        .requiredFeatureCount = (size_t)count(inst_feats),
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

    WGPUFeatureName features[] = {
        WGPUFeatureName_ShaderF16,
        WGPUFeatureName_TimestampQuery,
    };
    int n_features = count(features);

    struct wgpu_backend *be = calloc(1, sizeof *be);
    be->instance = instance;
    be->adapter  = adapter;

    WGPUDevice dev = NULL;
    WGPUDeviceDescriptor dev_desc    = WGPU_DEVICE_DESCRIPTOR_INIT;
    dev_desc.requiredFeatureCount     = (size_t)n_features;
    dev_desc.requiredFeatures         = features;
    dev_desc.requiredLimits           = &adapter_limits;
    dev_desc.uncapturedErrorCallbackInfo.callback  = (WGPUUncapturedErrorCallback)error_cb;
    dev_desc.uncapturedErrorCallbackInfo.userdata1 = be;
    wgpuAdapterRequestDevice(adapter, &dev_desc,
        (WGPURequestDeviceCallbackInfo){
            .callback  = device_request_cb,
            .userdata1 = &dev,
        });
    if (!dev) {
        wgpuAdapterRelease(adapter);
        wgpuInstanceRelease(instance);
        free(be);
        return 0;
    }

    be->device = dev;
    be->queue  = wgpuDeviceGetQueue(dev);
    be->max_dyn_storage = adapter_limits.maxDynamicStorageBuffersPerPipelineLayout;
    be->max_dyn_uniform = adapter_limits.maxDynamicUniformBuffersPerPipelineLayout;
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
    be->ts_period = (double)wgpuQueueGetTimestampPeriod(be->queue) * 1e-9;
    be->cache = (struct gpu_buf_cache){
        .ops = {wgpu_cache_alloc, wgpu_cache_upload, wgpu_cache_download,
                NULL, wgpu_cache_release},
        .ctx = be,
    };
    be->base = (struct umbra_backend){
        .compile        = wgpu_compile_fn,
        .flush          = wgpu_flush,
        .free           = (void (*)(struct umbra_backend *))wgpu_free,
        .stats          = wgpu_stats,
    };

    be->uni_pool = (struct uniform_ring_pool){
        .n          = WGPU_N_FRAMES,
        .high_water = WGPU_RING_HIGH_WATER,
        .ctx        = be,
        .wait_frame = wgpu_wait_frame,
    };
    be->uni_pool_uniform = (struct uniform_ring_pool){
        .n          = WGPU_N_FRAMES,
        .high_water = WGPU_RING_HIGH_WATER,
        .ctx        = be,
        .wait_frame = wgpu_wait_frame_noop,
    };
    for (int i = 0; i < WGPU_N_FRAMES; i++) {
        be->uni_pool.rings[i] = (struct uniform_ring){
            .align      = 256,
            .ctx        = be,
            .new_chunk  = wgpu_ring_new_chunk,
            .free_chunk = wgpu_ring_free_chunk,
        };
        be->uni_pool_uniform.rings[i] = (struct uniform_ring){
            .align      = 256,
            .ctx        = be,
            .new_chunk  = wgpu_ring_new_chunk_uniform,
            .free_chunk = wgpu_ring_free_chunk,
        };
    }

    return &be->base;
}

#endif
