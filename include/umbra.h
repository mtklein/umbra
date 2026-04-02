#pragma once

#include <stddef.h>
#include <stdio.h>

struct umbra_builder* umbra_builder(void);
void   umbra_builder_free(struct umbra_builder*);
void   umbra_builder_dump(struct umbra_builder const*, FILE*);

struct umbra_basic_block* umbra_basic_block(struct umbra_builder*);
void   umbra_basic_block_free(struct umbra_basic_block*);
void   umbra_basic_block_dump(struct umbra_basic_block const*, FILE*);

struct umbra_backend {
    struct umbra_program* (*compile)(struct umbra_backend*, struct umbra_basic_block const*);
    void                  (*flush  )(struct umbra_backend*);
    void                  (*free   )(struct umbra_backend*);
    int                     threadsafe, :32;  // TODO: _Bool const threadsafe, pad_[7];
};
struct umbra_backend* umbra_backend_interp(void);
struct umbra_backend* umbra_backend_jit   (void);
struct umbra_backend* umbra_backend_metal (void);
struct umbra_backend* umbra_backend_vulkan(void);

// TODO: remove this typedef, just write `struct umbra_buf`.
typedef struct {
    void   *ptr;
    size_t  sz;
    size_t  row_bytes;
    _Bool   read_only,
            pad_[7];
} umbra_buf;

struct umbra_program {
    void (*queue)(struct umbra_program*, int l, int t, int r, int b, umbra_buf[]);
    void (*dump )(struct umbra_program const*, FILE*);
    void (*free )(struct umbra_program*);
    struct umbra_backend *backend;
};

// TODO: remove this typedef, just write `enum umbra_fmt`.
typedef enum {
    umbra_fmt_8888,
    umbra_fmt_565,
    umbra_fmt_1010102,
    umbra_fmt_fp16,
    umbra_fmt_fp16_planar,
} umbra_fmt;
size_t umbra_fmt_size(umbra_fmt);

typedef struct { int bits; } umbra_val;
typedef struct { umbra_val r, g, b, a; } umbra_color;
// TODO: make this opaque struct { int bits; }, internally { int ix :31, deref :1; } or similar.
typedef struct { int ix, :24; _Bool deref; } umbra_ptr;

umbra_ptr umbra_deref_ptr(struct umbra_builder*, umbra_ptr buf, size_t off);

umbra_val umbra_x(struct umbra_builder*);
umbra_val umbra_y(struct umbra_builder*);

umbra_val umbra_imm_i32(struct umbra_builder*, int);
umbra_val umbra_imm_f32(struct umbra_builder*, float);

umbra_val umbra_uniform_32(struct umbra_builder*, umbra_ptr, size_t off);

umbra_val umbra_gather_16(struct umbra_builder*, umbra_ptr, umbra_val ix);
umbra_val umbra_gather_32(struct umbra_builder*, umbra_ptr, umbra_val ix);
umbra_val umbra_sample_32(struct umbra_builder*, umbra_ptr, umbra_val ix);

umbra_val umbra_load_16(struct umbra_builder*, umbra_ptr);
umbra_val umbra_load_32(struct umbra_builder*, umbra_ptr);

void umbra_store_16(struct umbra_builder*, umbra_ptr, umbra_val);
void umbra_store_32(struct umbra_builder*, umbra_ptr, umbra_val);

umbra_color umbra_load_color (struct umbra_builder*, umbra_ptr, umbra_fmt);
void        umbra_store_color(struct umbra_builder*, umbra_ptr, umbra_color, umbra_fmt);

umbra_val umbra_i32_from_s16(struct umbra_builder*, umbra_val);
umbra_val umbra_i32_from_u16(struct umbra_builder*, umbra_val);
umbra_val umbra_i16_from_i32(struct umbra_builder*, umbra_val);

umbra_val umbra_f32_from_f16(struct umbra_builder*, umbra_val);
umbra_val umbra_f16_from_f32(struct umbra_builder*, umbra_val);

umbra_val umbra_f32_from_i32(struct umbra_builder*, umbra_val);
umbra_val umbra_i32_from_f32(struct umbra_builder*, umbra_val);

umbra_val umbra_add_f32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_sub_f32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_mul_f32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_div_f32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_min_f32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_max_f32(struct umbra_builder*, umbra_val, umbra_val);

umbra_val umbra_sqrt_f32(struct umbra_builder*, umbra_val);
umbra_val umbra_abs_f32 (struct umbra_builder*, umbra_val);
umbra_val umbra_neg_f32 (struct umbra_builder*, umbra_val);

umbra_val umbra_round_f32(struct umbra_builder*, umbra_val);
umbra_val umbra_floor_f32(struct umbra_builder*, umbra_val);
umbra_val umbra_ceil_f32 (struct umbra_builder*, umbra_val);

umbra_val umbra_round_i32(struct umbra_builder*, umbra_val);
umbra_val umbra_floor_i32(struct umbra_builder*, umbra_val);
umbra_val umbra_ceil_i32 (struct umbra_builder*, umbra_val);

umbra_val umbra_add_i32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_sub_i32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_mul_i32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_shl_i32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_shr_u32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_shr_s32(struct umbra_builder*, umbra_val, umbra_val);

umbra_val umbra_and_32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_or_32 (struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_xor_32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_sel_32(struct umbra_builder*, umbra_val, umbra_val, umbra_val);

umbra_val umbra_eq_f32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_lt_f32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_le_f32(struct umbra_builder*, umbra_val, umbra_val);

umbra_val umbra_eq_i32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_lt_s32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_le_s32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_lt_u32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_le_u32(struct umbra_builder*, umbra_val, umbra_val);
