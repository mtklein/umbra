#pragma once
#include <stddef.h>

struct umbra_uniforms {
    void  *data;
    size_t size;
};

size_t umbra_reserve_f32(struct umbra_uniforms*, int n);
size_t umbra_reserve_ptr(struct umbra_uniforms*);

void umbra_set_f32(struct umbra_uniforms*, size_t off,
                   float const*, int n);
void umbra_set_ptr(struct umbra_uniforms*, size_t off,
                   void *ptr, size_t sz, _Bool read_only, size_t row_bytes);
