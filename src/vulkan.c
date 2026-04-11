#include "assume.h"
#include "spirv.h"
#include "uniform_ring.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if !defined(__APPLE__) || !defined(__aarch64__) || defined(__wasm__)

struct umbra_backend *umbra_backend_vulkan(void) { return 0; }

#else

#include <vulkan/vulkan.h>

// ---------------------------------------------------------------------------
//  Backend state.
// ---------------------------------------------------------------------------

struct buf_cache_entry {
    VkBuffer       buf;
    VkDeviceMemory mem;
    void          *mapped;
    void          *host;
    VkDeviceSize   size;
    _Bool          nocopy, writable; int :16, :32;
};

struct free_vk_buf {
    VkBuffer       buf;
    VkDeviceMemory mem;
    void          *mapped;
    VkDeviceSize   size;
};

enum { VK_N_FRAMES = 2 };

struct vk_backend {
    struct umbra_backend  base;
    VkInstance            instance;
    VkPhysicalDevice      phys;
    VkDevice              device;
    VkQueue               queue;
    VkCommandPool         cmd_pool;
    uint32_t              queue_family;
    uint32_t              mem_type_host;
    uint32_t              mem_type_host_import; int :32;
    PFN_vkGetMemoryHostPointerPropertiesEXT get_host_props;
    VkDeviceSize          host_import_align;
    VkCommandBuffer       batch_cmd;                       // currently-encoding cmdbuf, or NULL
    VkCommandBuffer       frame_committed[VK_N_FRAMES];    // last committed cmdbuf per frame
    VkFence               frame_fences   [VK_N_FRAMES];    // signals when frame_committed[i] is done
    VkQueryPool           ts_pool;                         // 2 timestamps per frame
    double                timestamp_period;                // ns per tick
    double                gpu_time_accum;
    struct copyback      *batch_copies;
    int                   batch_n_copies, batch_copies_cap;
    struct buf_cache_entry *batch_cache;
    int                     batch_cache_n, batch_cache_cap;
    struct free_vk_buf     *free_bufs;
    int                     free_bufs_n, free_bufs_cap;
    struct uniform_ring_pool uni_pool;
    size_t                total_upload_bytes;
    int                   total_dispatches;
    _Bool                 batch_has_dispatch; int :24;
};

static int find_compute_queue(VkPhysicalDevice phys) {
    uint32_t n = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &n, 0);
    VkQueueFamilyProperties *props = calloc(n, sizeof *props);
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &n, props);
    int idx = -1;
    for (uint32_t i = 0; i < n; i++) {
        if (props[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { idx = (int)i; break; }
    }
    free(props);
    return idx;
}

static uint32_t find_host_memory(VkPhysicalDevice phys) {
    VkPhysicalDeviceMemoryProperties mem;
    vkGetPhysicalDeviceMemoryProperties(phys, &mem);
    for (uint32_t i = 0; i < mem.memoryTypeCount; i++) {
        VkMemoryPropertyFlags f = mem.memoryTypes[i].propertyFlags;
        if ((f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
            (f & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            return i;
        }
    }
    return 0;
}

// deref_info is in spirv.h, shared with the wgpu backend.

// ---------------------------------------------------------------------------
//  Copyback tracking.
// ---------------------------------------------------------------------------

struct copyback {
    void *host;
    void *mapped;
    size_t bytes;
};

// ---------------------------------------------------------------------------
//  Program.
// ---------------------------------------------------------------------------

struct vk_program {
    struct umbra_program base;
    struct vk_backend *be;
    VkShaderModule          shader;
    VkDescriptorSetLayout   ds_layout;
    VkPipelineLayout        pipe_layout;
    VkPipeline              pipeline;

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
//  Vulkan resource creation helpers.
// ---------------------------------------------------------------------------

static VkBuffer create_buffer(VkDevice device, VkDeviceSize size) {
    VkBufferCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkBuffer buf;
    VkResult rc = vkCreateBuffer(device, &ci, 0, &buf);
    assume(rc == VK_SUCCESS);
    return buf;
}

static VkDeviceMemory alloc_and_bind(VkDevice device, VkBuffer buf, uint32_t mem_type) {
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, buf, &req);
    VkMemoryAllocateInfo ai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex = mem_type,
    };
    VkDeviceMemory mem;
    VkResult rc = vkAllocateMemory(device, &ai, 0, &mem);
    assume(rc == VK_SUCCESS);
    rc = vkBindBufferMemory(device, buf, mem, 0);
    assume(rc == VK_SUCCESS);
    return mem;
}


// ---------------------------------------------------------------------------
//  Uniform ring chunk lifecycle.
// ---------------------------------------------------------------------------

enum { VK_RING_HIGH_WATER = 64 * 1024 };

struct vk_ring_chunk {
    VkBuffer       buf;
    VkDeviceMemory mem;
};

static struct uniform_ring_chunk vk_ring_new_chunk(size_t min_bytes, void *ctx) {
    struct vk_backend *be = ctx;
    size_t cap = min_bytes > VK_RING_HIGH_WATER ? min_bytes : VK_RING_HIGH_WATER;
    struct vk_ring_chunk *chunk = calloc(1, sizeof *chunk);
    chunk->buf = create_buffer(be->device, (VkDeviceSize)cap);
    chunk->mem = alloc_and_bind(be->device, chunk->buf, be->mem_type_host);
    void *mapped = 0;
    VkResult rc = vkMapMemory(be->device, chunk->mem, 0, (VkDeviceSize)cap, 0, &mapped);
    assume(rc == VK_SUCCESS);
    return (struct uniform_ring_chunk){
        .handle=chunk,
        .mapped=mapped,
        .cap   =cap,
        .used  =0,
    };
}

static void vk_ring_free_chunk(void *handle, void *ctx) {
    struct vk_backend    *be    = ctx;
    struct vk_ring_chunk *chunk = handle;
    vkFreeMemory   (be->device, chunk->mem, 0);
    vkDestroyBuffer(be->device, chunk->buf, 0);
    free(chunk);
}

// ---------------------------------------------------------------------------
//  Batch helpers.
// ---------------------------------------------------------------------------

static void begin_batch(struct vk_backend *be) {
    if (be->batch_cmd) { return; }
    VkCommandBufferAllocateInfo ai = {
        .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool=be->cmd_pool,
        .level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount=1,
    };
    vkAllocateCommandBuffers(be->device, &ai, &be->batch_cmd);
    VkCommandBufferBeginInfo bi = {
        .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(be->batch_cmd, &bi);
    if (be->ts_pool) {
        uint32_t const base = (uint32_t)be->uni_pool.cur * 2;
        vkCmdResetQueryPool(be->batch_cmd, be->ts_pool, base, 2);
        vkCmdWriteTimestamp(be->batch_cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            be->ts_pool, base);
    }
    be->batch_has_dispatch = 0;
}

static void batch_track_copy(struct vk_backend *be, void *host, void *mapped, size_t bytes) {
    if (be->batch_n_copies >= be->batch_copies_cap) {
        be->batch_copies_cap = be->batch_copies_cap ? 2 * be->batch_copies_cap : 64;
        be->batch_copies = realloc(be->batch_copies, (size_t)be->batch_copies_cap * sizeof *be->batch_copies);
    }
    be->batch_copies[be->batch_n_copies++] = (struct copyback){host, mapped, bytes};
}


// ---------------------------------------------------------------------------
//  Program implementation.
// ---------------------------------------------------------------------------

static void vk_flush(struct umbra_backend *be);
static void vk_submit_cmdbuf(struct vk_backend *be);

// Returns an index into be->batch_cache.  rw is the BUF_READ|BUF_WRITTEN
// mask: copyback is only tracked when BUF_WRITTEN is set.
static int cache_buf(struct vk_backend *be, void *host, size_t bytes,
                     VkDeviceSize sz, uint8_t rw) {
    for (int i = 0; i < be->batch_cache_n; i++) {
        struct buf_cache_entry *ce = &be->batch_cache[i];
        if (ce->host == host && ce->size >= sz) {
            return i;
        }
    }

    if (be->batch_cache_n >= be->batch_cache_cap) {
        be->batch_cache_cap = be->batch_cache_cap ? 2 * be->batch_cache_cap : 16;
        be->batch_cache = realloc(be->batch_cache,
            (size_t)be->batch_cache_cap * sizeof *be->batch_cache);
    }
    int idx = be->batch_cache_n++;
    struct buf_cache_entry *ce = &be->batch_cache[idx];
    *ce = (struct buf_cache_entry){0};

    // Try zero-copy host import for page-aligned pointers.
    VkDeviceSize align = be->host_import_align;
    if (align && host && ((uintptr_t)host % align) == 0) {
        VkDeviceSize import_sz = (sz + align - 1) & ~(align - 1);
        VkBuffer buf = create_buffer(be->device, import_sz);
        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(be->device, buf, &req);
        VkImportMemoryHostPointerInfoEXT imp = {
            .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT,
            .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT,
            .pHostPointer = host,
        };
        VkMemoryAllocateInfo ai = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = &imp,
            .allocationSize = import_sz > req.size ? import_sz : req.size,
            .memoryTypeIndex = be->mem_type_host_import,
        };
        VkDeviceMemory mem = VK_NULL_HANDLE;
        if (vkAllocateMemory(be->device, &ai, 0, &mem) == VK_SUCCESS &&
            vkBindBufferMemory(be->device, buf, mem, 0) == VK_SUCCESS) {
            ce->buf      = buf;
            ce->mem      = mem;
            ce->size     = import_sz;
            ce->mapped   = host;
            ce->host     = host;
            ce->nocopy   = 1;
            ce->writable = rw & BUF_WRITTEN;
            return idx;
        }
        // Import failed — fall back.
        if (mem) { vkFreeMemory(be->device, mem, 0); }
        vkDestroyBuffer(be->device, buf, 0);
    }

    // Try to reuse a buffer from the free pool.
    for (int i = 0; i < be->free_bufs_n; i++) {
        if (be->free_bufs[i].size >= sz) {
            ce->buf    = be->free_bufs[i].buf;
            ce->mem    = be->free_bufs[i].mem;
            ce->mapped = be->free_bufs[i].mapped;
            ce->size   = be->free_bufs[i].size;
            be->free_bufs[i] = be->free_bufs[--be->free_bufs_n];
            goto fill;
        }
    }
    ce->buf      = create_buffer(be->device, sz);
    ce->mem      = alloc_and_bind(be->device, ce->buf, be->mem_type_host);
    ce->size     = sz;
    vkMapMemory(be->device, ce->mem, 0, sz, 0, &ce->mapped);
fill:
    ce->host     = host;
    ce->writable = rw & BUF_WRITTEN;
    if (bytes) { memcpy(ce->mapped, host, bytes); be->total_upload_bytes += bytes; }
    if ((rw & BUF_WRITTEN) && host && bytes) {
        batch_track_copy(be, host, ce->mapped, bytes);
    }
    return idx;
}

static void vk_program_queue(struct umbra_program *p, int l, int t, int r, int b,
                              struct umbra_buf buf[]) {
    struct vk_program *vp = (struct vk_program *)p;
    struct vk_backend *be = vp->be;

    int w = r - l, h = b - t;
    if (w <= 0 || h <= 0) { return; }

    begin_batch(be);

    int n = vp->total_bufs;

    // For each binding we need a (VkBuffer, offset, range) triple. Read-only
    // flat top-level buffers go through the per-batch uniform ring; everything
    // else (writable, row-structured, deref'd) goes through cache_buf so the
    // writable->readonly handoff path is preserved.
    VkBuffer     bind_buffer[32];
    VkDeviceSize bind_offset[32];
    VkDeviceSize bind_range [32];
    assume(n <= 32);
    for (int i = 0; i < n; i++) {
        bind_buffer[i] = VK_NULL_HANDLE;
        bind_offset[i] = 0;
        bind_range [i] = 0;
    }

    for (int i = 0; i <= vp->max_ptr; i++) {
        if (buf[i].ptr && buf[i].sz) {
            uint8_t const rw = vp->buf_rw[i];
            if (!(rw & BUF_WRITTEN) && !buf[i].row_bytes) {
                struct uniform_ring_loc loc =
                    uniform_ring_pool_alloc(&be->uni_pool, buf[i].ptr, buf[i].sz);
                struct vk_ring_chunk *chunk = loc.handle;
                bind_buffer[i] = chunk->buf;
                bind_offset[i] = (VkDeviceSize)loc.offset;
                bind_range [i] = (VkDeviceSize)buf[i].sz;
            } else {
                VkDeviceSize sz = (VkDeviceSize)buf[i].sz;
                if (sz < 4) { sz = 4; }
                int idx = cache_buf(be, buf[i].ptr, buf[i].sz, sz, rw);
                bind_buffer[i] = be->batch_cache[idx].buf;
                bind_range [i] = VK_WHOLE_SIZE;
            }
        }
    }

    uint32_t push_data[67] = {0};
    push_data[0] = (uint32_t)w;
    push_data[1] = (uint32_t)l;
    push_data[2] = (uint32_t)t;

    for (int i = 0; i <= vp->max_ptr; i++) {
        push_data[3 + i] = (uint32_t)buf[i].sz;
        push_data[3 + vp->total_bufs + i] = (uint32_t)buf[i].row_bytes;
    }

    for (int d = 0; d < vp->n_deref; d++) {
        char *base = (char *)buf[vp->deref[d].src_buf].ptr;
        void  *derived;
        size_t dsz, drb;
        memcpy(&derived, base + vp->deref[d].off,      sizeof derived);
        memcpy(&dsz,     base + vp->deref[d].off + 8,  sizeof dsz);
        memcpy(&drb,     base + vp->deref[d].off + 16, sizeof drb);
        int bi = vp->deref[d].buf_idx;

        VkDeviceSize sz = (VkDeviceSize)dsz;
        if (sz < 4) { sz = 4; }
        int idx = cache_buf(be, derived, dsz, sz, vp->buf_rw[bi]);
        bind_buffer[bi] = be->batch_cache[idx].buf;
        bind_range [bi] = VK_WHOLE_SIZE;

        push_data[3 + bi] = (uint32_t)dsz;
        push_data[3 + vp->total_bufs + bi] = (uint32_t)drb;
    }

    for (int i = 0; i < n; i++) {
        if (bind_buffer[i] == VK_NULL_HANDLE) {
            int idx = cache_buf(be, 0, 0, 4, 0);
            bind_buffer[i] = be->batch_cache[idx].buf;
            bind_range [i] = VK_WHOLE_SIZE;
        }
    }

    // Push descriptors: each dispatch records its own buffer bindings directly
    // in the command buffer, so no external descriptor set to protect.
    VkDescriptorBufferInfo buf_infos[32];
    VkWriteDescriptorSet   writes[32];
    for (int i = 0; i < n; i++) {
        buf_infos[i] = (VkDescriptorBufferInfo){
            .buffer=bind_buffer[i],
            .offset=bind_offset[i],
            .range =bind_range [i],
        };
        writes[i] = (VkWriteDescriptorSet){
            .sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding=(uint32_t)i,
            .descriptorCount=1,
            .descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo=&buf_infos[i],
        };
    }
    int n_desc = n;

    vkCmdBindPipeline(be->batch_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vp->pipeline);
    if (n_desc) {
        vkCmdPushDescriptorSet(be->batch_cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                               vp->pipe_layout, 0, (uint32_t)n_desc, writes);
    }
    vkCmdPushConstants(be->batch_cmd, vp->pipe_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, (uint32_t)(vp->push_words * (int)sizeof(uint32_t)), push_data);

    if (be->batch_has_dispatch) {
        VkMemoryBarrier mb = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        };
        vkCmdPipelineBarrier(be->batch_cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 1, &mb, 0, NULL, 0, NULL);
    }
    be->batch_has_dispatch = 1;
    uint32_t gx = ((uint32_t)w + SPIRV_WG_SIZE - 1) / SPIRV_WG_SIZE;
    vkCmdDispatch(be->batch_cmd, gx, (uint32_t)h, 1);
    be->total_dispatches++;

    if (uniform_ring_pool_should_rotate(&be->uni_pool)) {
        vk_submit_cmdbuf(be);
    }
}

static void vk_program_dump(struct umbra_program const *p, FILE *f) {
    struct vk_program const *vp = (struct vk_program const *)p;

    // Write SPIR-V binary to a temp file, disassemble with spirv-dis if available.
    char tmp[] = "/tmp/umbra_spirv_XXXXXX";
    int fd = mkstemp(tmp);
    if (fd >= 0) {
        size_t n = (size_t)vp->spirv_words * sizeof(uint32_t);
        if (write(fd, vp->spirv, n) == (ssize_t)n) {
            close(fd);
            char cmd[256];
            snprintf(cmd, sizeof cmd, "spirv-dis --no-color '%s' 2>/dev/null", tmp);
            FILE *dis = popen(cmd, "r");
            if (dis) {
                char line[256];
                while (fgets(line, (int)sizeof line, dis)) { fputs(line, f); }
                if (pclose(dis) == 0) { unlink(tmp); return; }
            }
        } else {
            close(fd);
        }
        unlink(tmp);
    }
    // Fallback: summary only.
    fprintf(f, "vulkan SPIR-V (%d words, %d bufs, %d push words)\n",
            vp->spirv_words, vp->total_bufs, vp->push_words);
}

static void vk_program_free(struct umbra_program *p) {
    struct vk_program *vp = (struct vk_program *)p;
    struct vk_backend *be = vp->be;

    vkDestroyPipeline(be->device, vp->pipeline, 0);
    vkDestroyPipelineLayout(be->device, vp->pipe_layout, 0);
    vkDestroyDescriptorSetLayout(be->device, vp->ds_layout, 0);
    vkDestroyShaderModule(be->device, vp->shader, 0);
    free(vp->deref);
    free(vp->buf_rw);
    free(vp->spirv);
    free(vp);
}

static struct umbra_program *vk_compile(struct umbra_backend *be,
                                         struct umbra_basic_block const *bb) {
    struct vk_backend *vbe = (struct vk_backend *)be;

    int spirv_words = 0, max_ptr = -1, total_bufs = 0, n_deref = 0, push_words = 0;
    struct deref_info *deref = 0;
    uint8_t *buf_rw = 0;
    uint32_t *spirv = build_spirv(bb, SPIRV_FLOAT_CONTROLS | SPIRV_ALWAYS_16BIT,
                                   &spirv_words, &max_ptr, &total_bufs,
                                   &n_deref, &deref, &push_words, &buf_rw);
    if (!spirv) { return 0; }

    int n_desc = total_bufs;

    // Create shader module.
    VkShaderModule shader;
    {
        VkShaderModuleCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = (size_t)spirv_words * sizeof(uint32_t),
            .pCode = spirv,
        };
        VkResult rc = vkCreateShaderModule(vbe->device, &ci, 0, &shader);
        assume(rc == VK_SUCCESS);
    }

    // Descriptor set layout: one storage buffer per non-push buffer slot.
    VkDescriptorSetLayoutBinding *bindings = calloc((size_t)(n_desc ? n_desc : 1), sizeof *bindings);
    for (int i = 0; i < n_desc; i++) {
        bindings[i] = (VkDescriptorSetLayoutBinding){
            .binding = (uint32_t)i,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        };
    }
    VkDescriptorSetLayout ds_layout;
    {
        VkDescriptorSetLayoutCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
            .bindingCount = (uint32_t)n_desc,
            .pBindings = bindings,
        };
        vkCreateDescriptorSetLayout(vbe->device, &ci, 0, &ds_layout);
    }
    free(bindings);

    // Push constant range.
    VkPushConstantRange pcr = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = (uint32_t)(push_words * (int)sizeof(uint32_t)),
    };

    // Pipeline layout.
    VkPipelineLayout pipe_layout;
    {
        VkPipelineLayoutCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &ds_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pcr,
        };
        vkCreatePipelineLayout(vbe->device, &ci, 0, &pipe_layout);
    }

    // Compute pipeline.
    VkPipeline pipeline;
    {
        VkComputePipelineCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = shader,
                .pName = "main",
            },
            .layout = pipe_layout,
        };
        VkResult rc = vkCreateComputePipelines(vbe->device, VK_NULL_HANDLE, 1, &ci, 0, &pipeline);
        assume(rc == VK_SUCCESS);
    }

    struct vk_program *p = calloc(1, sizeof *p);
    p->be          = vbe;
    p->shader      = shader;
    p->ds_layout   = ds_layout;
    p->pipe_layout = pipe_layout;
    p->pipeline    = pipeline;
    p->max_ptr     = max_ptr;
    p->total_bufs  = total_bufs;
    p->n_deref     = n_deref;
    p->push_words    = push_words;
    p->deref       = deref;
    p->buf_rw      = buf_rw;
    p->spirv       = spirv;
    p->spirv_words = spirv_words;

    p->base.queue   = vk_program_queue;
    p->base.dump    = vk_program_dump;
    p->base.free    = vk_program_free;
    p->base.backend = be;
    return &p->base;
}

// ---------------------------------------------------------------------------
//  Backend lifecycle.
// ---------------------------------------------------------------------------

// uniform_ring_pool wait_frame callback: wait for the frame's in-flight
// cmdbuf to retire, free it, and reset its fence. The pool resets the
// ring after this returns.
static void vk_wait_frame(int frame, void *ctx) {
    struct vk_backend *v = ctx;
    if (v->frame_committed[frame]) {
        vkWaitForFences(v->device, 1, &v->frame_fences[frame], VK_TRUE, UINT64_MAX);
        if (v->ts_pool) {
            uint64_t ts[2];
            VkResult r = vkGetQueryPoolResults(v->device, v->ts_pool,
                                               (uint32_t)frame * 2, 2,
                                               sizeof ts, ts, sizeof ts[0],
                                               VK_QUERY_RESULT_64_BIT);
            if (r == VK_SUCCESS) {
                v->gpu_time_accum += (double)(ts[1] - ts[0]) * v->timestamp_period * 1e-9;
            }
        }
        vkResetFences(v->device, 1, &v->frame_fences[frame]);
        vkFreeCommandBuffers(v->device, v->cmd_pool, 1, &v->frame_committed[frame]);
        v->frame_committed[frame] = VK_NULL_HANDLE;
    }
}

// Backpressure release: submit the current frame's cmdbuf without waiting,
// stash it in frame_committed[cur], then rotate the pool. Pool calls
// vk_wait_frame on the new cur (only blocks if its prior cmdbuf is still
// running) and resets that ring. Writebacks and cache_buf entries stay
// live across rotation.
static void vk_submit_cmdbuf(struct vk_backend *v) {
    if (!v->batch_cmd) { return; }

    if (v->ts_pool) {
        vkCmdWriteTimestamp(v->batch_cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                            v->ts_pool, (uint32_t)v->uni_pool.cur * 2 + 1);
    }
    vkEndCommandBuffer(v->batch_cmd);

    VkSubmitInfo si = {
        .sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount=1,
        .pCommandBuffers=&v->batch_cmd,
    };
    vkQueueSubmit(v->queue, 1, &si, v->frame_fences[v->uni_pool.cur]);

    v->frame_committed[v->uni_pool.cur] = v->batch_cmd;
    v->batch_cmd                        = VK_NULL_HANDLE;
    v->batch_has_dispatch               = 0;

    uniform_ring_pool_rotate(&v->uni_pool);
}

static void vk_flush(struct umbra_backend *be) {
    struct vk_backend *v = (struct vk_backend *)be;

    vk_submit_cmdbuf(v);
    uniform_ring_pool_drain_all(&v->uni_pool);

    for (int i = 0; i < v->batch_n_copies; i++) {
        memcpy(v->batch_copies[i].host, v->batch_copies[i].mapped, v->batch_copies[i].bytes);
    }

    {
        int recyclable = 0;
        for (int i = 0; i < v->batch_cache_n; i++) {
            if (!v->batch_cache[i].nocopy) { recyclable++; }
        }
        int need = v->free_bufs_n + recyclable;
        if (need > v->free_bufs_cap) {
            v->free_bufs_cap = need;
            v->free_bufs = realloc(v->free_bufs,
                (size_t)v->free_bufs_cap * sizeof *v->free_bufs);
        }
        for (int i = 0; i < v->batch_cache_n; i++) {
            struct buf_cache_entry *ce = &v->batch_cache[i];
            if (ce->nocopy) {
                vkFreeMemory  (v->device, ce->mem, 0);
                vkDestroyBuffer(v->device, ce->buf, 0);
            } else {
                v->free_bufs[v->free_bufs_n++] = (struct free_vk_buf){
                    .buf    = ce->buf,
                    .mem    = ce->mem,
                    .mapped = ce->mapped,
                    .size   = ce->size,
                };
            }
        }
    }

    v->batch_n_copies = 0;
    v->batch_cache_n  = 0;
}

static struct umbra_backend_stats vk_stats(struct umbra_backend const *be) {
    struct vk_backend const *v = (struct vk_backend const*)be;
    return (struct umbra_backend_stats){
        .gpu_sec        = v->gpu_time_accum,
        .uniform_ring_rotations = v->uni_pool.rotations,
        .dispatches     = v->total_dispatches,
        .upload_bytes   = v->total_upload_bytes,
    };
}

static void vk_free(struct umbra_backend *be) {
    vk_flush(be);
    struct vk_backend *v = (struct vk_backend *)be;
    uniform_ring_pool_free(&v->uni_pool);
    for (int i = 0; i < VK_N_FRAMES; i++) {
        vkDestroyFence(v->device, v->frame_fences[i], 0);
    }
    for (int i = 0; i < v->free_bufs_n; i++) {
        vkFreeMemory  (v->device, v->free_bufs[i].mem, 0);
        vkDestroyBuffer(v->device, v->free_bufs[i].buf, 0);
    }
    if (v->ts_pool) { vkDestroyQueryPool(v->device, v->ts_pool, 0); }
    vkDestroyCommandPool(v->device, v->cmd_pool, 0);
    vkDestroyDevice(v->device, 0);
    vkDestroyInstance(v->instance, 0);
    free(v->free_bufs);
    free(v->batch_copies);
    free(v->batch_cache);
    free(v);
}

struct umbra_backend *umbra_backend_vulkan(void) {
    VkInstance instance;
    {
        VkApplicationInfo app = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .apiVersion = VK_MAKE_VERSION(1, 0, 0),
        };
        char const *inst_exts[] = { "VK_KHR_get_physical_device_properties2" };
        VkInstanceCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &app,
            .enabledExtensionCount = 1,
            .ppEnabledExtensionNames = inst_exts,
        };
        if (vkCreateInstance(&ci, 0, &instance) != VK_SUCCESS) { return 0; }
    }

    VkPhysicalDevice phys;
    {
        uint32_t n = 1;
        if (vkEnumeratePhysicalDevices(instance, &n, &phys) < 0 || n == 0) {
            vkDestroyInstance(instance, 0);
            return 0;
        }
    }

    int qf = find_compute_queue(phys);
    if (qf < 0) { vkDestroyInstance(instance, 0); return 0; }

    VkDevice device;
    {
        float prio = 1.0f;
        VkDeviceQueueCreateInfo qci = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = (uint32_t)qf,
            .queueCount = 1,
            .pQueuePriorities = &prio,
        };
        char const *exts[] = {
            "VK_KHR_16bit_storage",
            "VK_KHR_shader_float16_int8",
            "VK_KHR_external_memory",
            "VK_EXT_external_memory_host",
            "VK_KHR_shader_float_controls",
            "VK_KHR_push_descriptor",
        };
        VkPhysicalDeviceFloat16Int8FeaturesKHR f16_int8 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT16_INT8_FEATURES_KHR,
            .shaderFloat16 = VK_TRUE,
        };
        VkPhysicalDevice16BitStorageFeatures f16 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES,
            .pNext = &f16_int8,
            .storageBuffer16BitAccess = VK_TRUE,
        };
        VkDeviceCreateInfo dci = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &f16,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &qci,
            .enabledExtensionCount = sizeof exts / sizeof *exts,
            .ppEnabledExtensionNames = exts,
        };
        if (vkCreateDevice(phys, &dci, 0, &device) != VK_SUCCESS) {
            vkDestroyInstance(instance, 0);
            return 0;
        }
    }

    VkQueue queue;
    vkGetDeviceQueue(device, (uint32_t)qf, 0, &queue);

    VkCommandPool cmd_pool;
    {
        VkCommandPoolCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = (uint32_t)qf,
        };
        if (vkCreateCommandPool(device, &ci, 0, &cmd_pool) != VK_SUCCESS) {
            vkDestroyDevice(device, 0);
            vkDestroyInstance(instance, 0);
            return 0;
        }
    }

    // Query host import alignment.
    VkDeviceSize host_import_align = 0;
    {
        PFN_vkVoidFunction fn =
            vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR");
        PFN_vkGetPhysicalDeviceProperties2KHR getProps2;
        memcpy(&getProps2, &fn, sizeof fn);
        if (getProps2) {
            VkPhysicalDeviceExternalMemoryHostPropertiesEXT host_props = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT,
            };
            VkPhysicalDeviceProperties2 props2 = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                .pNext = &host_props,
            };
            getProps2(phys, &props2);
            host_import_align = host_props.minImportedHostPointerAlignment;
        }
    }

    // Find memory type for host pointer import.
    uint32_t mem_type_host_import = 0;
    PFN_vkGetMemoryHostPointerPropertiesEXT get_host_props = 0;
    if (host_import_align) {
        PFN_vkVoidFunction fn2 =
            vkGetDeviceProcAddr(device, "vkGetMemoryHostPointerPropertiesEXT");
        memcpy(&get_host_props, &fn2, sizeof fn2);
        if (get_host_props) {
            // Probe with a page-aligned allocation to discover compatible memory types.
            void *probe = 0;
            posix_memalign(&probe, (size_t)host_import_align, (size_t)host_import_align);
            VkMemoryHostPointerPropertiesEXT hp = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT,
            };
            if (get_host_props(device,
                               VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT,
                               probe, &hp) == VK_SUCCESS && hp.memoryTypeBits) {
                // Pick the first compatible type.
                for (uint32_t i = 0; i < 32; i++) {
                    if (hp.memoryTypeBits & (1u << i)) { mem_type_host_import = i; break; }
                }
            } else {
                host_import_align = 0;  // disable import
            }
            free(probe);
        } else {
            host_import_align = 0;  // disable import
        }
    }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(phys, &props);
    size_t ssbo_align = (size_t)props.limits.minStorageBufferOffsetAlignment;
    if (ssbo_align < 16) { ssbo_align = 16; }

    VkQueryPool ts_pool = VK_NULL_HANDLE;
    if (props.limits.timestampComputeAndGraphics) {
        VkQueryPoolCreateInfo qci = {
            .sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .queryType  = VK_QUERY_TYPE_TIMESTAMP,
            .queryCount = VK_N_FRAMES * 2,
        };
        vkCreateQueryPool(device, &qci, 0, &ts_pool);
    }

    struct vk_backend *v = calloc(1, sizeof *v);
    v->instance            = instance;
    v->phys                = phys;
    v->device              = device;
    v->queue               = queue;
    v->cmd_pool            = cmd_pool;
    v->queue_family        = (uint32_t)qf;
    v->mem_type_host       = find_host_memory(phys);
    v->mem_type_host_import = mem_type_host_import;
    v->host_import_align   = host_import_align;
    v->get_host_props      = get_host_props;
    v->ts_pool             = ts_pool;
    v->timestamp_period    = (double)props.limits.timestampPeriod;
    v->uni_pool = (struct uniform_ring_pool){
        .n         =VK_N_FRAMES,
        .high_water=VK_RING_HIGH_WATER,
        .ctx       =v,
        .wait_frame=vk_wait_frame,
    };
    for (int i = 0; i < VK_N_FRAMES; i++) {
        v->uni_pool.rings[i] = (struct uniform_ring){
            .align     =ssbo_align,
            .ctx       =v,
            .new_chunk =vk_ring_new_chunk,
            .free_chunk=vk_ring_free_chunk,
        };
        VkFenceCreateInfo fi = { .sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        vkCreateFence(device, &fi, 0, &v->frame_fences[i]);
    }
    v->base.compile        = vk_compile;
    v->base.flush          = vk_flush;
    v->base.free           = vk_free;
    v->base.stats          = vk_stats;
    return &v->base;
}

#endif
