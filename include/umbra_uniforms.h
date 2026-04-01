#pragma once
#include <stddef.h>

struct umbra_uniforms {
    char  *data;
    size_t size;
};

typedef struct { size_t off; } umbra_uniform;

umbra_uniform umbra_reserve_f32(struct umbra_uniforms*, int n);
umbra_uniform umbra_reserve_ptr(struct umbra_uniforms*);

void umbra_set_f32(struct umbra_uniforms*, umbra_uniform,
                   float const*, int n);
void umbra_set_ptr(struct umbra_uniforms*, umbra_uniform,
                   void *ptr, size_t sz, _Bool read_only, size_t row_bytes);
