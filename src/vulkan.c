#include "assume.h"
#include "dispatch_overlap.h"
#include "count.h"
#include "spirv.h"
#include "uniform_ring.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if !defined(__APPLE__) || !defined(__aarch64__) || defined(__wasm__)

struct umbra_backend* umbra_backend_vulkan(void) { return 0; }

#else

#include <vulkan/vulkan.h>

struct vk_buf_handle {
    VkBuffer       buf;
    VkDeviceMemory mem;
    void          *mapped;
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
    uint32_t              mem_type_host_import;
    _Bool                 batch_has_dispatch; int :24;    // whether batch_cmd holds any work
    PFN_vkGetMemoryHostPointerPropertiesEXT get_host_props;
    VkDeviceSize          host_import_align;
    VkCommandBuffer       batch_cmd;                       // currently-encoding cmdbuf, or NULL
    VkCommandBuffer       frame_committed[VK_N_FRAMES];    // last committed cmdbuf per frame
    VkFence               frame_fences[VK_N_FRAMES];     // signals when frame_committed[i] is done
    VkQueryPool           ts_pool;                         // 2 timestamps per frame
    double                timestamp_period;                // ns per tick
    double                gpu_time_accum;
    struct gpu_buf_cache  cache;
    struct uniform_ring_pool uni_pool;
    int                   total_dispatches;
    int                   total_submits;
    struct dispatch_overlap overlap;
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

struct vk_program {
    struct umbra_program base;
    struct vk_backend *be;
    VkShaderModule          shader;
    VkDescriptorSetLayout   ds_layout;
    VkPipelineLayout        pipe_layout;
    VkPipeline              pipeline;

    int max_ptr;
    int total_bufs;
    int push_words, bindings;
    struct buffer_binding *binding;

    struct buffer_metadata *buf;

    uint32_t *spirv;
    int       spirv_words, :32;
};

static VkBuffer create_buffer(VkDevice device, VkDeviceSize size) {
    VkBufferCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
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

enum { VK_RING_HIGH_WATER = 64 * 1024 };

struct vk_ring_chunk {
    VkBuffer       buf;
    VkDeviceMemory mem;
};

static struct uniform_ring_chunk vk_ring_new_chunk(size_t min_bytes, void *ctx) {
    struct vk_backend *be = ctx;
    size_t cap_bytes = min_bytes > VK_RING_HIGH_WATER ? min_bytes : VK_RING_HIGH_WATER;
    struct vk_ring_chunk *chunk = calloc(1, sizeof *chunk);
    chunk->buf = create_buffer(be->device, (VkDeviceSize)cap_bytes);
    chunk->mem = alloc_and_bind(be->device, chunk->buf, be->mem_type_host);
    void *mapped = 0;
    VkResult rc = vkMapMemory(be->device, chunk->mem, 0, (VkDeviceSize)cap_bytes, 0, &mapped);
    assume(rc == VK_SUCCESS);
    return (struct uniform_ring_chunk){
        .handle=chunk,
        .mapped=mapped,
        .cap=cap_bytes,
        .used=0,
    };
}

static void vk_ring_free_chunk(void *handle, void *ctx) {
    struct vk_backend    *be    = ctx;
    struct vk_ring_chunk *chunk = handle;
    vkFreeMemory   (be->device, chunk->mem, 0);
    vkDestroyBuffer(be->device, chunk->buf, 0);
    free(chunk);
}

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
    dispatch_overlap_reset(&be->overlap);
}

static gpu_buf vk_cache_alloc(size_t size, void *ctx) {
    struct vk_backend *v = ctx;
    VkDeviceSize sz = size ? (VkDeviceSize)size : 4;
    struct vk_buf_handle *h = malloc(sizeof *h);
    h->buf = create_buffer(v->device, sz);
    h->mem = alloc_and_bind(v->device, h->buf, v->mem_type_host);
    vkMapMemory(v->device, h->mem, 0, sz, 0, &h->mapped);
    return (gpu_buf){.ptr = h, .size = (size_t)sz};
}

static void vk_cache_upload(gpu_buf buf, void const *host, size_t bytes, void *ctx) {
    (void)ctx;
    struct vk_buf_handle *h = buf.ptr;
    memcpy(h->mapped, host, bytes);
}

static void vk_cache_download(gpu_buf buf, void *host, size_t bytes, void *ctx) {
    (void)ctx;
    struct vk_buf_handle *h = buf.ptr;
    memcpy(host, h->mapped, bytes);
}

static gpu_buf vk_cache_import(void *host, size_t bytes, void *ctx) {
    struct vk_backend *v = ctx;
    VkDeviceSize align = v->host_import_align;
    if (align && host && ((uintptr_t)host % align) == 0) {
        VkDeviceSize sz = (VkDeviceSize)bytes;
        if (sz < 4) { sz = 4; }
        VkDeviceSize import_sz = (sz + align - 1) & ~(align - 1);
        VkBuffer buf_vk = create_buffer(v->device, import_sz);
        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(v->device, buf_vk, &req);
        VkImportMemoryHostPointerInfoEXT imp = {
            .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT,
            .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT,
            .pHostPointer = host,
        };
        VkMemoryAllocateInfo ai = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = &imp,
            .allocationSize = import_sz > req.size ? import_sz : req.size,
            .memoryTypeIndex = v->mem_type_host_import,
        };
        VkDeviceMemory mem = VK_NULL_HANDLE;
        if (vkAllocateMemory(v->device, &ai, 0, &mem) == VK_SUCCESS &&
            vkBindBufferMemory(v->device, buf_vk, mem, 0) == VK_SUCCESS) {
            struct vk_buf_handle *h = malloc(sizeof *h);
            h->buf    = buf_vk;
            h->mem    = mem;
            h->mapped = host;
            return (gpu_buf){.ptr = h, .size = bytes};
        }
        // Import failed — fall back.
        if (mem) { vkFreeMemory(v->device, mem, 0); }
        vkDestroyBuffer(v->device, buf_vk, 0);
    }
    return (gpu_buf){0};
}

static void vk_cache_release(gpu_buf buf, void *ctx) {
    struct vk_backend *v = ctx;
    struct vk_buf_handle *h = buf.ptr;
    vkDestroyBuffer(v->device, h->buf, 0);
    vkFreeMemory(v->device, h->mem, 0);
    free(h);
}

static void vk_flush(struct umbra_backend *be);
static void vk_submit_cmdbuf(struct vk_backend *be);

static void vk_program_queue(struct umbra_program *p, int l, int t, int r, int b,
                             struct umbra_late_binding const *late, int lates) {
    struct vk_program *vp = (struct vk_program *)p;
    struct vk_backend *be = vp->be;

    int w = r - l, h = b - t;
    if (w <= 0 || h <= 0) { return; }

    assume(vp->max_ptr + 1 <= 32);
    struct umbra_buf buf[32];
    resolve_bindings(buf, vp->binding, vp->bindings, late, lates);

    begin_batch(be);

    int n = vp->total_bufs;

    // For each binding we need a (VkBuffer, offset, range) triple. Read-only
    // flat top-level buffers go through the per-batch uniform ring; everything
    // else (writable, row-structured, deref'd) goes through the shared cache
    // so the writable->readonly handoff path is preserved.
    VkBuffer     bind_buffer[32];
    VkDeviceSize bind_offset[32];
    VkDeviceSize bind_range [32];
    assume(n <= 32);
    for (int i = 0; i < n; i++) {
        bind_buffer[i] = VK_NULL_HANDLE;
        bind_offset[i] = 0;
        bind_range [i] = 0;
    }

    _Bool pinned[32] = {0};
    for (int k = 0; k < vp->bindings; k++) {
        if (binding_is_uniform(vp->binding[k].kind)) { pinned[vp->binding[k].ix] = 1; }
    }

    for (int i = 0; i <= vp->max_ptr; i++) {
        if (buf[i].ptr && buf[i].count) {
            size_t const bytes = (size_t)buf[i].count << vp->buf[i].shift;
            uint8_t const rw = (uint8_t)(vp->buf[i].rw
                             | (vp->buf[i].host_readonly ? BUF_HOST_READONLY : 0));
            if (!(rw & BUF_WRITTEN) && pinned[i]) {
                struct uniform_ring_loc loc =
                    uniform_ring_pool_alloc(&be->uni_pool, buf[i].ptr, bytes);
                struct vk_ring_chunk *chunk = loc.handle;
                size_t const range = vp->buf[i].is_uniform ? ((bytes + 15) & ~(size_t)15)
                                                           : bytes;
                bind_buffer[i] = chunk->buf;
                bind_offset[i] = (VkDeviceSize)loc.offset;
                bind_range [i] = (VkDeviceSize)range;
            } else {
                int idx = gpu_buf_cache_get(&be->cache, buf[i].ptr, bytes, rw);
                struct vk_buf_handle *bh = be->cache.entry[idx].buf.ptr;
                bind_buffer[i] = bh->buf;
                bind_range [i] = VK_WHOLE_SIZE;
            }
        }
    }

    uint32_t push_data[67] = {0};
    push_data[0] = (uint32_t)w;
    push_data[1] = (uint32_t)l;
    push_data[2] = (uint32_t)t;

    for (int i = 0; i <= vp->max_ptr; i++) {
        push_data[3 + i]                    = (uint32_t)buf[i].count;
        push_data[3 + vp->total_bufs + i] = (uint32_t)buf[i].stride;
    }

    for (int i = 0; i < n; i++) {
        if (bind_buffer[i] == VK_NULL_HANDLE) {
            int idx = gpu_buf_cache_get(&be->cache, 0, 0, 0);
            struct vk_buf_handle *bh = be->cache.entry[idx].buf.ptr;
            bind_buffer[i] = bh->buf;
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
            .descriptorType=vp->buf[i].is_uniform ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                                                  : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo=&buf_infos[i],
        };
    }
    int descs = n;

    vkCmdBindPipeline(be->batch_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vp->pipeline);
    if (descs) {
        vkCmdPushDescriptorSet(be->batch_cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                               vp->pipe_layout, 0, (uint32_t)descs, writes);
    }
    vkCmdPushConstants(be->batch_cmd, vp->pipe_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, (uint32_t)(vp->push_words * (int)sizeof(uint32_t)), push_data);

    _Bool needs_barrier = 0;
    if (be->batch_has_dispatch) {
        for (int i = 0; i < n && !needs_barrier; i++) {
            if (vp->buf[i].rw) {
                needs_barrier = dispatch_overlap_check(&be->overlap, bind_buffer[i],
                                                       l, t, r, b);
            }
        }
    }
    if (needs_barrier) {
        VkMemoryBarrier mb = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        };
        vkCmdPipelineBarrier(be->batch_cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 1, &mb, 0, NULL, 0, NULL);
        dispatch_overlap_reset(&be->overlap);
    }
    be->batch_has_dispatch = 1;
    uint32_t gx = ((uint32_t)w + SPIRV_WG_SIZE - 1) / SPIRV_WG_SIZE;
    vkCmdDispatch(be->batch_cmd, gx, (uint32_t)h, 1);
    be->total_dispatches++;

    for (int i = 0; i < n; i++) {
        if (vp->buf[i].rw & BUF_WRITTEN) {
            dispatch_overlap_record(&be->overlap, bind_buffer[i], l, t, r, b);
        }
    }

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
        size_t bytes = (size_t)vp->spirv_words * sizeof(uint32_t);
        if (write(fd, vp->spirv, bytes) == (ssize_t)bytes) {
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
    free(vp->buf);
    free(vp->binding);
    free(vp->spirv);
    free(vp);
}

static struct umbra_program* vk_compile(struct umbra_backend *be,
                                         struct umbra_flat_ir const *ir) {
    struct vk_backend *v = (struct vk_backend *)be;

    struct spirv_result const sr = build_spirv(ir, 0);

    int descs = sr.total_bufs;

    // Create shader module.
    VkShaderModule shader;
    {
        VkShaderModuleCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = (size_t)sr.spirv_words * sizeof(uint32_t),
            .pCode = sr.spirv,
        };
        VkResult rc = vkCreateShaderModule(v->device, &ci, 0, &shader);
        assume(rc == VK_SUCCESS);
    }

    // Descriptor set layout: one storage or uniform buffer per non-push slot.
    VkDescriptorSetLayoutBinding *bindings =
        calloc((size_t)(descs ? descs : 1), sizeof *bindings);
    for (int i = 0; i < descs; i++) {
        bindings[i] = (VkDescriptorSetLayoutBinding){
            .binding = (uint32_t)i,
            .descriptorType = sr.buf[i].is_uniform ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                                                   : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        };
    }
    VkDescriptorSetLayout ds_layout;
    {
        VkDescriptorSetLayoutCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
            .bindingCount = (uint32_t)descs,
            .pBindings = bindings,
        };
        vkCreateDescriptorSetLayout(v->device, &ci, 0, &ds_layout);
    }
    free(bindings);

    VkPushConstantRange pcr = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = (uint32_t)(sr.push_words * (int)sizeof(uint32_t)),
    };

    VkPipelineLayout pipe_layout;
    {
        VkPipelineLayoutCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &ds_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pcr,
        };
        vkCreatePipelineLayout(v->device, &ci, 0, &pipe_layout);
    }

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
        VkResult rc = vkCreateComputePipelines(v->device, VK_NULL_HANDLE, 1, &ci, 0, &pipeline);
        assume(rc == VK_SUCCESS);
    }

    struct vk_program *p = calloc(1, sizeof *p);
    p->be          = v;
    p->shader      = shader;
    p->ds_layout   = ds_layout;
    p->pipe_layout = pipe_layout;
    p->pipeline    = pipeline;
    p->max_ptr     = sr.max_ptr;
    p->total_bufs  = sr.total_bufs;
    p->push_words  = sr.push_words;
    p->buf            = sr.buf;
    p->spirv       = sr.spirv;
    p->spirv_words = sr.spirv_words;

    p->bindings = ir->bindings;
    if (p->bindings) {
        size_t const sz = (size_t)p->bindings * sizeof *p->binding;
        p->binding = malloc(sz);
        __builtin_memcpy(p->binding, ir->binding, sz);
    }

    p->base.queue   = vk_program_queue;
    p->base.dump    = vk_program_dump;
    p->base.free    = vk_program_free;
    p->base.backend = be;
    return &p->base;
}

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
// running) and resets that ring. Cache entries stay live across rotation.
static void vk_submit_cmdbuf(struct vk_backend *v) {
    if (v->batch_cmd) {
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
        v->total_submits++;

        v->frame_committed[v->uni_pool.cur] = v->batch_cmd;
        v->batch_cmd                        = VK_NULL_HANDLE;
        v->batch_has_dispatch               = 0;
        dispatch_overlap_reset(&v->overlap);

        uniform_ring_pool_rotate(&v->uni_pool);
    }
}

static void vk_flush(struct umbra_backend *be) {
    struct vk_backend *v = (struct vk_backend *)be;

    vk_submit_cmdbuf(v);
    uniform_ring_pool_drain_all(&v->uni_pool);
    gpu_buf_cache_copyback (&v->cache);
    gpu_buf_cache_end_batch(&v->cache);
}

static struct umbra_backend_stats vk_stats(struct umbra_backend const *be) {
    struct vk_backend const *v = (struct vk_backend const*)be;
    return (struct umbra_backend_stats){
        .gpu_sec                = v->gpu_time_accum,
        .uniform_ring_rotations = v->uni_pool.rotations,
        .dispatches             = v->total_dispatches,
        .submits                = v->total_submits,
        .upload_bytes           = v->cache.upload_bytes,
    };
}

static void vk_free(struct umbra_backend *be) {
    vk_flush(be);
    struct vk_backend *v = (struct vk_backend *)be;
    uniform_ring_pool_free(&v->uni_pool);
    for (int i = 0; i < VK_N_FRAMES; i++) {
        vkDestroyFence(v->device, v->frame_fences[i], 0);
    }
    gpu_buf_cache_free(&v->cache);
    if (v->ts_pool) { vkDestroyQueryPool(v->device, v->ts_pool, 0); }
    vkDestroyCommandPool(v->device, v->cmd_pool, 0);
    vkDestroyDevice(v->device, 0);
    vkDestroyInstance(v->instance, 0);
    free(v);
}

struct umbra_backend* umbra_backend_vulkan(void) {
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
            .enabledExtensionCount = (uint32_t)count(exts),
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
    size_t ubo_align  = (size_t)props.limits.minUniformBufferOffsetAlignment;
    if (ssbo_align < ubo_align) { ssbo_align = ubo_align; }
    if (ssbo_align < 16)        { ssbo_align = 16; }

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
    v->cache = (struct gpu_buf_cache){
        .ops = {vk_cache_alloc, vk_cache_upload, vk_cache_download,
                vk_cache_import, vk_cache_release},
        .ctx = v,
    };
    v->uni_pool = (struct uniform_ring_pool){
        .n=VK_N_FRAMES,
        .high_water=VK_RING_HIGH_WATER,
        .ctx=v,
        .wait_frame=vk_wait_frame,
    };
    for (int i = 0; i < VK_N_FRAMES; i++) {
        v->uni_pool.rings[i] = (struct uniform_ring){
            .align=ssbo_align,
            .ctx=v,
            .new_chunk=vk_ring_new_chunk,
            .free_chunk=vk_ring_free_chunk,
        };
        VkFenceCreateInfo fi = { .sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        vkCreateFence(device, &fi, 0, &v->frame_fences[i]);
    }
    v->base.compile = vk_compile;
    v->base.flush   = vk_flush;
    v->base.free    = vk_free;
    v->base.stats   = vk_stats;
    return &v->base;
}

#endif
