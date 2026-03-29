#pragma once

#include <stddef.h>

struct umbra_builder* umbra_builder(void);
void   umbra_builder_free(struct umbra_builder*);

struct umbra_basic_block* umbra_basic_block(struct umbra_builder*);
void   umbra_basic_block_free(struct umbra_basic_block*);

struct umbra_backend* umbra_backend_interp(void);
struct umbra_backend* umbra_backend_jit   (void);
struct umbra_backend* umbra_backend_metal (void);

void   umbra_backend_free      (struct umbra_backend*);
void   umbra_backend_flush     (struct umbra_backend*);
_Bool  umbra_backend_threadsafe(struct umbra_backend const*);

typedef enum {
    umbra_fmt_none = 0,
    umbra_fmt_8888,
    umbra_fmt_565,
    umbra_fmt_1010102,
    umbra_fmt_fp16,
    umbra_fmt_f16,
    umbra_fmt_f32,
    umbra_fmt_f16_planar,
    umbra_fmt_srgb,
} umbra_fmt;

int umbra_pixel_bytes(umbra_fmt);

typedef struct {
    void           *ptr;
    size_t          sz;
    size_t          row_bytes;
    umbra_fmt       fmt;
    _Bool           read_only;
    char            pad_[3];
} umbra_buf;


struct umbra_program* umbra_program(struct umbra_backend*,
                                    struct umbra_basic_block const*);
void   umbra_program_free (struct umbra_program*);
void   umbra_program_queue(struct umbra_program*,
                           int l, int t, int r, int b, umbra_buf[]);
struct umbra_backend* umbra_program_backend(struct umbra_program const*);

typedef struct { int bits; } umbra_val;
typedef struct { umbra_val r, g, b, a; } umbra_color;
typedef struct { int ix, :24; _Bool deref; } umbra_ptr;

// TODO: I don't like any of these first six methods.
int       umbra_reserve    (struct umbra_builder*, int n);
int       umbra_reserve_ptr(struct umbra_builder*);
umbra_ptr umbra_deref_ptr  (struct umbra_builder*, umbra_ptr buf, int byte_off);
int       umbra_uni_len    (struct umbra_builder const*);
void      umbra_set_uni_len(struct umbra_builder*, int);
int       umbra_max_ptr    (struct umbra_builder const*);

umbra_val umbra_x(struct umbra_builder*);
umbra_val umbra_y(struct umbra_builder*);

umbra_val umbra_imm_i32(struct umbra_builder*, int);
umbra_val umbra_imm_f32(struct umbra_builder*, float);

umbra_val umbra_uniform_32(struct umbra_builder*, umbra_ptr, int slot);

umbra_val umbra_gather_16(struct umbra_builder*, umbra_ptr, umbra_val ix);
umbra_val umbra_gather_32(struct umbra_builder*, umbra_ptr, umbra_val ix);

umbra_val umbra_load_16(struct umbra_builder*, umbra_ptr);
umbra_val umbra_load_32(struct umbra_builder*, umbra_ptr);

void      umbra_store_16(struct umbra_builder*, umbra_ptr, umbra_val);
void      umbra_store_32(struct umbra_builder*, umbra_ptr, umbra_val);

umbra_color umbra_load_color (struct umbra_builder*, umbra_ptr);
void        umbra_store_color(struct umbra_builder*, umbra_ptr, umbra_color);

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
umbra_val umbra_sign_f32(struct umbra_builder*, umbra_val);

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

umbra_val umbra_and_i32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_or_i32 (struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_xor_i32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_sel_i32(struct umbra_builder*, umbra_val, umbra_val, umbra_val);

// pack(x,y,k): insert y into x starting at bit k.
// Like x|(y<<k) but assumes x's bits at and above k are clear;
// when true, maps to ARM SLI (shift-left-insert) or x86 shift+or.
umbra_val umbra_pack(struct umbra_builder*, umbra_val x, umbra_val y, int k);

umbra_val umbra_eq_f32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_ne_f32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_lt_f32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_le_f32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_gt_f32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_ge_f32(struct umbra_builder*, umbra_val, umbra_val);

umbra_val umbra_eq_i32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_ne_i32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_lt_s32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_le_s32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_gt_s32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_ge_s32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_lt_u32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_le_u32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_gt_u32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_ge_u32(struct umbra_builder*, umbra_val, umbra_val);


#include <stdio.h>
void umbra_program_dump(struct umbra_program*, FILE*);
void umbra_dump_builder(struct umbra_builder const*, FILE*);
void umbra_dump_basic_block(struct umbra_basic_block const*, FILE*);
