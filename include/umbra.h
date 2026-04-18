#pragma once

#include <stddef.h>
#include <stdio.h>

struct umbra_builder* umbra_builder(void);
void   umbra_builder_free(struct umbra_builder*);
void   umbra_builder_dump(struct umbra_builder const*, FILE*);

struct umbra_flat_ir* umbra_flat_ir(struct umbra_builder*);
void   umbra_flat_ir_free(struct umbra_flat_ir*);
void   umbra_flat_ir_dump(struct umbra_flat_ir const*, FILE*);

struct umbra_backend_stats {
    double gpu_sec;
    double encode_sec;
    double submit_sec;
    size_t upload_bytes, pad;
    int    uniform_ring_rotations;
    int    dispatches;
    int    submits, :32;
};

struct umbra_backend {
    struct umbra_program*      (*compile)(struct umbra_backend*, struct umbra_flat_ir const*);
    void                       (*flush  )(struct umbra_backend*);
    void                       (*free   )(struct umbra_backend*);
    struct umbra_backend_stats (*stats  )(struct umbra_backend const*);
};
struct umbra_backend* umbra_backend_interp(void);
struct umbra_backend* umbra_backend_jit   (void);
struct umbra_backend* umbra_backend_metal (void);
struct umbra_backend* umbra_backend_vulkan(void);
struct umbra_backend* umbra_backend_wgpu  (void);
void   umbra_backend_free(struct umbra_backend*);

struct umbra_buf {
    void *ptr;
    int   count;
    int   stride;
};

struct umbra_program {
    void (*queue)(struct umbra_program*, int l, int t, int r, int b, struct umbra_buf[]);
    void (*dump )(struct umbra_program const*, FILE*);
    void (*free )(struct umbra_program*);
    struct umbra_backend *backend;
    _Bool                 queue_is_threadsafe, pad[7];
};
void umbra_program_free(struct umbra_program*);

typedef struct { int id:30; unsigned chan:2; } umbra_val16;
typedef struct { int id:30; unsigned chan:2; } umbra_val32;

// Pointer handles.  `ix` indexes into the caller's buf[] at dispatch time.
typedef struct { int ix; } umbra_ptr16;
typedef struct { int ix; } umbra_ptr32;
typedef struct { int ix; } umbra_ptr64;

// Register a span of uniform data, returning a ptr handle for use with
// umbra_uniform_32() and/or umbra_gather_32().  The uniforms are not retained
// and must outlive this umbra_builder and any derived umbra_flat_ir or
// umbra_programs.
//
// `slot` should be 4-byte aligned, and `slots` counts those 4-byte slots.
umbra_ptr32 umbra_uniforms(struct umbra_builder*, void const *slot, int slots);
umbra_val32 umbra_uniform_32(struct umbra_builder*, umbra_ptr32, int slot);

// Bind a caller-owned `struct umbra_buf` to a ptr handle.  At each dispatch
// the backend reads the current contents of *buf, so callers may mutate
// .ptr / .count / .stride between dispatches without rebuilding the program.
// Unlike umbra_uniforms() (which captures a fixed ptr/slots pair), the buf
// itself is the source of truth.  The buf must outlive the builder and any
// derived umbra_flat_ir or umbra_programs.
umbra_ptr16 umbra_bind_buf16(struct umbra_builder*, struct umbra_buf const*);
umbra_ptr32 umbra_bind_buf32(struct umbra_builder*, struct umbra_buf const*);
umbra_ptr64 umbra_bind_buf64(struct umbra_builder*, struct umbra_buf const*);

umbra_val32 umbra_x(struct umbra_builder*);
umbra_val32 umbra_y(struct umbra_builder*);

umbra_val32 umbra_imm_i32(struct umbra_builder*, int);
umbra_val32 umbra_imm_f32(struct umbra_builder*, float);

umbra_val16 umbra_gather_16(struct umbra_builder*, umbra_ptr16, umbra_val32 ix);
umbra_val32 umbra_gather_32(struct umbra_builder*, umbra_ptr32, umbra_val32 ix);

umbra_val16 umbra_load_16(struct umbra_builder*, umbra_ptr16);
umbra_val32 umbra_load_32(struct umbra_builder*, umbra_ptr32);

void umbra_store_16(struct umbra_builder*, umbra_ptr16, umbra_val16);
void umbra_store_32(struct umbra_builder*, umbra_ptr32, umbra_val32);

void umbra_load_8x4 (struct umbra_builder*, umbra_ptr32,
                     umbra_val32*, umbra_val32*, umbra_val32*, umbra_val32*);
void umbra_store_8x4(struct umbra_builder*, umbra_ptr32,
                     umbra_val32 , umbra_val32 , umbra_val32 , umbra_val32 );

void umbra_load_16x4 (struct umbra_builder*, umbra_ptr64,
                      umbra_val16*, umbra_val16*, umbra_val16*, umbra_val16*);
void umbra_store_16x4(struct umbra_builder*, umbra_ptr64,
                      umbra_val16 , umbra_val16 , umbra_val16 , umbra_val16 );

void umbra_load_16x4_planar (struct umbra_builder*, umbra_ptr16,
                             umbra_val16 *r, umbra_val16 *g, umbra_val16 *b, umbra_val16 *a);
void umbra_store_16x4_planar(struct umbra_builder*, umbra_ptr16,
                             umbra_val16 r, umbra_val16 g, umbra_val16 b, umbra_val16 a);

umbra_val32 umbra_i32_from_s16(struct umbra_builder*, umbra_val16);
umbra_val32 umbra_i32_from_u16(struct umbra_builder*, umbra_val16);
umbra_val16 umbra_i16_from_i32(struct umbra_builder*, umbra_val32);

umbra_val32 umbra_f32_from_f16(struct umbra_builder*, umbra_val16);
umbra_val16 umbra_f16_from_f32(struct umbra_builder*, umbra_val32);

umbra_val32 umbra_f32_from_i32(struct umbra_builder*, umbra_val32);
umbra_val32 umbra_i32_from_f32(struct umbra_builder*, umbra_val32);

umbra_val32 umbra_add_f32(struct umbra_builder*, umbra_val32, umbra_val32);
umbra_val32 umbra_sub_f32(struct umbra_builder*, umbra_val32, umbra_val32);
umbra_val32 umbra_mul_f32(struct umbra_builder*, umbra_val32, umbra_val32);
umbra_val32 umbra_div_f32(struct umbra_builder*, umbra_val32, umbra_val32);
umbra_val32 umbra_min_f32(struct umbra_builder*, umbra_val32, umbra_val32);
umbra_val32 umbra_max_f32(struct umbra_builder*, umbra_val32, umbra_val32);

umbra_val32 umbra_sqrt_f32(struct umbra_builder*, umbra_val32);
umbra_val32 umbra_abs_f32 (struct umbra_builder*, umbra_val32);

umbra_val32 umbra_round_f32(struct umbra_builder*, umbra_val32);
umbra_val32 umbra_floor_f32(struct umbra_builder*, umbra_val32);
umbra_val32 umbra_ceil_f32 (struct umbra_builder*, umbra_val32);

umbra_val32 umbra_round_i32(struct umbra_builder*, umbra_val32);
umbra_val32 umbra_floor_i32(struct umbra_builder*, umbra_val32);
umbra_val32 umbra_ceil_i32 (struct umbra_builder*, umbra_val32);

umbra_val32 umbra_add_i32(struct umbra_builder*, umbra_val32, umbra_val32);
umbra_val32 umbra_sub_i32(struct umbra_builder*, umbra_val32, umbra_val32);
umbra_val32 umbra_mul_i32(struct umbra_builder*, umbra_val32, umbra_val32);
umbra_val32 umbra_shl_i32(struct umbra_builder*, umbra_val32, umbra_val32);
umbra_val32 umbra_shr_u32(struct umbra_builder*, umbra_val32, umbra_val32);
umbra_val32 umbra_shr_s32(struct umbra_builder*, umbra_val32, umbra_val32);

umbra_val32 umbra_and_32(struct umbra_builder*, umbra_val32, umbra_val32);
umbra_val32 umbra_or_32 (struct umbra_builder*, umbra_val32, umbra_val32);
umbra_val32 umbra_xor_32(struct umbra_builder*, umbra_val32, umbra_val32);
umbra_val32 umbra_sel_32(struct umbra_builder*, umbra_val32, umbra_val32, umbra_val32);

umbra_val32 umbra_eq_f32(struct umbra_builder*, umbra_val32, umbra_val32);
umbra_val32 umbra_lt_f32(struct umbra_builder*, umbra_val32, umbra_val32);
umbra_val32 umbra_le_f32(struct umbra_builder*, umbra_val32, umbra_val32);

umbra_val32 umbra_eq_i32(struct umbra_builder*, umbra_val32, umbra_val32);
umbra_val32 umbra_lt_s32(struct umbra_builder*, umbra_val32, umbra_val32);
umbra_val32 umbra_le_s32(struct umbra_builder*, umbra_val32, umbra_val32);
umbra_val32 umbra_lt_u32(struct umbra_builder*, umbra_val32, umbra_val32);
umbra_val32 umbra_le_u32(struct umbra_builder*, umbra_val32, umbra_val32);

umbra_val32 umbra_loop    (struct umbra_builder*, umbra_val32 uniform_loop_count);
void        umbra_end_loop(struct umbra_builder*);

void        umbra_if    (struct umbra_builder*, umbra_val32 cond);
void        umbra_end_if(struct umbra_builder*);

typedef struct { int id; } umbra_var32;
umbra_var32 umbra_declare_var32(struct umbra_builder*);
umbra_val32 umbra_load_var32   (struct umbra_builder*, umbra_var32);
void        umbra_store_var32  (struct umbra_builder*, umbra_var32, umbra_val32);
