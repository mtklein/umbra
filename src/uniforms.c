#include "../include/umbra_uniforms.h"
#include <stdlib.h>

struct umbra_uniforms {
    char  *data;
    size_t size, cap;
};

struct umbra_uniforms *umbra_uniforms(void) {
    struct umbra_uniforms *u = calloc(1, sizeof *u);
    return u;
}
void umbra_uniforms_free(struct umbra_uniforms *u) {
    if (u) { free(u->data); free(u); }
}

static void uni_grow(struct umbra_uniforms *u, size_t need) {
    if (need > u->cap) {
        size_t cap = u->cap ? u->cap : 64;
        while (cap < need) { cap *= 2; }
        u->data = realloc(u->data, cap);
        __builtin_memset(u->data + u->cap, 0, cap - u->cap);
        u->cap = cap;
    }
}

umbra_uniform umbra_reserve_f32(struct umbra_uniforms *u, int n) {
    u->size = (u->size + 3) & ~(size_t)3;
    umbra_uniform h = {.off = u->size};
    u->size += (size_t)n * 4;
    uni_grow(u, u->size);
    return h;
}
umbra_uniform_ptr umbra_reserve_ptr_slot(struct umbra_uniforms *u) {
    u->size = (u->size + 7) & ~(size_t)7;
    umbra_uniform_ptr h = {.off = u->size};
    u->size += 24;
    uni_grow(u, u->size);
    return h;
}

size_t umbra_uniforms_size(struct umbra_uniforms const *u) { return u->size; }

void umbra_uniforms_set_size(struct umbra_uniforms *u, size_t s) { u->size = s; }

void umbra_set_f32(struct umbra_uniforms *u, umbra_uniform h, float const *v, int n) {
    __builtin_memcpy(u->data + h.off, v, (size_t)n * 4);
}
void umbra_set_ptr(struct umbra_uniforms *u, umbra_uniform_ptr h,
                   void *ptr, size_t sz, _Bool read_only, size_t row_bytes) {
    ptrdiff_t ssz = read_only ? -(ptrdiff_t)sz : (ptrdiff_t)sz;
    __builtin_memset(u->data + h.off, 0, 24);
    __builtin_memcpy(u->data + h.off,      &ptr,       sizeof ptr);
    __builtin_memcpy(u->data + h.off + 8,  &ssz,       sizeof ssz);
    __builtin_memcpy(u->data + h.off + 16, &row_bytes, sizeof row_bytes);
}
umbra_buf umbra_uniforms_buf(struct umbra_uniforms const *u) {
    return (umbra_buf){.ptr = u->data, .sz = u->size, .read_only = 1};
}
