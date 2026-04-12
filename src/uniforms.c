#include "../include/umbra.h"
#include "assume.h"
#include <stdlib.h>

size_t umbra_uniforms_reserve_f32(struct umbra_uniforms_layout *u, int n) {
    u->size = (u->size + 3) & ~(size_t)3;
    size_t const h = u->size;
    u->size += (size_t)n * 4;
    return h;
}
size_t umbra_uniforms_reserve_ptr(struct umbra_uniforms_layout *u) {
    u->size = (u->size + 7) & ~(size_t)7;
    size_t const h = u->size;
    u->size += 24;
    return h;
}

void* umbra_uniforms_alloc(struct umbra_uniforms_layout const *u) {
    return calloc(1, u->size);
}

void umbra_uniforms_fill_f32(void *data, size_t h, float const *v, int n) {
    char *p = (char*)data + h;
    __builtin_memcpy(p, v, (size_t)n * 4);
}
void umbra_uniforms_fill_ptr(void *data, size_t h, struct umbra_buf b) {
    char *p = (char*)data + h;
    __builtin_memset(p, 0, 24);
    __builtin_memcpy(p,      &b.ptr,       sizeof b.ptr);
    __builtin_memcpy(p + 8,  &b.sz,        sizeof b.sz);
    __builtin_memcpy(p + 16, &b.row_bytes, sizeof b.row_bytes);
}
