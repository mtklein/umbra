#pragma once
#include <stddef.h>
#include <stdbool.h>

typedef struct { size_t off; } umbra_uniform;
typedef struct { size_t off; } umbra_uniform_ptr;

struct umbra_uniforms* umbra_uniforms     (void);
void                   umbra_uniforms_free(struct umbra_uniforms*);

umbra_uniform     umbra_reserve_f32     (struct umbra_uniforms*, int n);
umbra_uniform_ptr umbra_reserve_ptr_slot(struct umbra_uniforms*);
size_t            umbra_uniforms_size    (struct umbra_uniforms const*);
void              umbra_uniforms_set_size(struct umbra_uniforms*, size_t);

void      umbra_set_f32(struct umbra_uniforms*, umbra_uniform,     float const*, int n);
void      umbra_set_ptr(struct umbra_uniforms*, umbra_uniform_ptr,
                        void *ptr, size_t sz, bool read_only, size_t row_bytes);

void* umbra_uniforms_data(struct umbra_uniforms const*);

