#include "bb.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#if !defined(__APPLE__) || !defined(__aarch64__) || defined(__wasm__)

struct umbra_backend *umbra_backend_vulkan(void) { return 0; }

#else

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include <vulkan/vulkan.h>
#pragma clang diagnostic pop

// ---------------------------------------------------------------------------
//  Backend state.
// ---------------------------------------------------------------------------

struct vk_backend {
    struct umbra_backend base;
    VkInstance     instance;
    VkPhysicalDevice phys;
    VkDevice       device;
    VkQueue        queue;
    VkCommandPool  cmd_pool;
    uint32_t       queue_family;
    uint32_t       mem_type_host;   // host-visible, coherent memory type index
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

// ---------------------------------------------------------------------------
//  Program stub (to be filled in with SPIR-V codegen).
// ---------------------------------------------------------------------------

struct vk_program {
    struct umbra_program base;
    struct vk_backend *be;
};

static void vk_program_queue(struct umbra_program *p, int l, int t, int r, int b,
                              umbra_buf buf[]) {
    (void)p; (void)l; (void)t; (void)r; (void)b; (void)buf;
    // TODO: dispatch compute shader
}
static void vk_program_dump(struct umbra_program const *p, FILE *f) {
    (void)p; (void)f;
}
static void vk_program_free(struct umbra_program *p) {
    free(p);
}

static struct umbra_program *vk_compile(struct umbra_backend *be,
                                         struct umbra_basic_block const *bb) {
    (void)bb;
    struct vk_program *p = calloc(1, sizeof *p);
    p->be = (struct vk_backend *)be;
    p->base.queue = vk_program_queue;
    p->base.dump  = vk_program_dump;
    p->base.free  = vk_program_free;
    p->base.backend = be;
    return &p->base;
}

// ---------------------------------------------------------------------------
//  Backend lifecycle.
// ---------------------------------------------------------------------------

static void vk_flush(struct umbra_backend *be) {
    struct vk_backend *v = (struct vk_backend *)be;
    vkQueueWaitIdle(v->queue);
}

static void vk_free(struct umbra_backend *be) {
    struct vk_backend *v = (struct vk_backend *)be;
    vkDestroyCommandPool(v->device, v->cmd_pool, 0);
    vkDestroyDevice(v->device, 0);
    vkDestroyInstance(v->instance, 0);
    free(v);
}

struct umbra_backend *umbra_backend_vulkan(void) {
    VkInstance instance;
    {
        VkApplicationInfo app = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .apiVersion = VK_MAKE_VERSION(1, 0, 0),
        };
        VkInstanceCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &app,
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
        VkDeviceCreateInfo dci = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &qci,
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

    struct vk_backend *v = calloc(1, sizeof *v);
    v->instance     = instance;
    v->phys         = phys;
    v->device       = device;
    v->queue        = queue;
    v->cmd_pool     = cmd_pool;
    v->queue_family = (uint32_t)qf;
    v->mem_type_host = find_host_memory(phys);
    v->base.compile  = vk_compile;
    v->base.flush    = vk_flush;
    v->base.free     = vk_free;
    return &v->base;
}

#endif
