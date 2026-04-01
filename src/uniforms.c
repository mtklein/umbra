#include "../include/umbra_uniforms.h"
#include <assert.h>
#include <stdlib.h>

umbra_uniform umbra_reserve_f32(struct umbra_uniforms *u, int n) {
    u->size = (u->size + 3) & ~(size_t)3;
    umbra_uniform h = {.off = u->size};
    u->size += (size_t)n * 4;
    return h;
}
umbra_uniform umbra_reserve_ptr(struct umbra_uniforms *u) {
    u->size = (u->size + 7) & ~(size_t)7;
    umbra_uniform h = {.off = u->size};
    u->size += 24;
    return h;
}

static void ensure_allocated(struct umbra_uniforms *u) {
    if (!u->data) {
        u->data = calloc(1, u->size);
    }
}

void umbra_set_f32(struct umbra_uniforms *u, umbra_uniform h, float const *v, int n) {
    assert(h.off + (size_t)n * 4 <= u->size);
    ensure_allocated(u);
    __builtin_memcpy(u->data + h.off, v, (size_t)n * 4);
}
void umbra_set_ptr(struct umbra_uniforms *u, umbra_uniform h,
                   void *ptr, size_t sz, _Bool read_only, size_t row_bytes) {
    assert(h.off + 24 <= u->size);
    ensure_allocated(u);
    ptrdiff_t ssz = read_only ? -(ptrdiff_t)sz : (ptrdiff_t)sz;
    __builtin_memset(u->data + h.off, 0, 24);
    __builtin_memcpy(u->data + h.off,      &ptr,       sizeof ptr);
    __builtin_memcpy(u->data + h.off + 8,  &ssz,       sizeof ssz);
    __builtin_memcpy(u->data + h.off + 16, &row_bytes, sizeof row_bytes);
}
