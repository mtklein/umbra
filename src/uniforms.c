#include "../include/umbra.h"
#include <stdint.h>
#include <stdlib.h>

int umbra_uniforms_reserve_f32(struct umbra_uniforms_layout *u, int n) {
    int const slot = u->slots;
    u->slots += n;
    return slot;
}
int umbra_uniforms_reserve_ptr(struct umbra_uniforms_layout *u) {
    u->slots = (u->slots + 1) & ~1;
    int const slot = u->slots;
    u->slots += 4;
    return slot;
}

void* umbra_uniforms_alloc(struct umbra_uniforms_layout const *u) {
    return calloc((size_t)u->slots, 4);
}

void umbra_uniforms_fill_f32(void *data, int slot, float const *v, int n) {
    __builtin_memcpy((uint32_t*)data + slot, v, (size_t)n * 4);
}
void umbra_uniforms_fill_ptr(void *data, int slot, struct umbra_buf b) {
    uint32_t *p = (uint32_t*)data + slot;
    __builtin_memset(p, 0, 16);
    __builtin_memcpy(p,     &b.ptr,    sizeof b.ptr);
    __builtin_memcpy(p + 2, &b.count,  sizeof b.count);
    __builtin_memcpy(p + 3, &b.stride, sizeof b.stride);
}
