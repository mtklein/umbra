#pragma once

struct umbra_builder* umbra_builder(void);
void   umbra_builder_free(struct umbra_builder*);

typedef struct {int id;} umbra_val;
typedef struct {int ix;} umbra_ptr;

int       umbra_reserve    (struct umbra_builder*, int n);
int       umbra_reserve_ptr(struct umbra_builder*);
umbra_ptr umbra_deref_ptr  (struct umbra_builder*, umbra_ptr buf, int byte_off);

int   umbra_uni_len    (struct umbra_builder const*);
void  umbra_set_uni_len(struct umbra_builder*, int);

umbra_val umbra_iota(struct umbra_builder*);
umbra_val umbra_imm_i32(struct umbra_builder*, int bits);
static inline umbra_val umbra_imm_f32(struct umbra_builder *b, float v) {
    union { float f; int i; } u = {.f=v};
    return umbra_imm_i32(b, u.i);
}

umbra_val umbra_load_i32 (struct umbra_builder*, umbra_ptr, umbra_val ix);
umbra_val umbra_load_i16 (struct umbra_builder*, umbra_ptr, umbra_val ix);
void      umbra_store_i32(struct umbra_builder*, umbra_ptr, umbra_val ix, umbra_val);
void      umbra_store_i16(struct umbra_builder*, umbra_ptr, umbra_val ix, umbra_val);

umbra_val umbra_widen_s16 (struct umbra_builder*, umbra_val);
umbra_val umbra_widen_u16 (struct umbra_builder*, umbra_val);
umbra_val umbra_narrow_i16(struct umbra_builder*, umbra_val);

umbra_val umbra_widen_f16 (struct umbra_builder*, umbra_val);
umbra_val umbra_narrow_f32(struct umbra_builder*, umbra_val);

umbra_val umbra_add_f32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_sub_f32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_mul_f32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_div_f32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_min_f32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_max_f32(struct umbra_builder*, umbra_val, umbra_val);

umbra_val umbra_sqrt_f32(struct umbra_builder*, umbra_val);
umbra_val umbra_abs_f32 (struct umbra_builder*, umbra_val);
umbra_val umbra_sign_f32(struct umbra_builder*, umbra_val);

umbra_val umbra_add_i32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_sub_i32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_mul_i32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_shl_i32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_shr_u32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_shr_s32(struct umbra_builder*, umbra_val, umbra_val);

umbra_val umbra_and_i32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_or_i32 (struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_pack   (struct umbra_builder*, umbra_val base, umbra_val val, int shift);
umbra_val umbra_xor_i32(struct umbra_builder*, umbra_val, umbra_val);
umbra_val umbra_sel_i32(struct umbra_builder*, umbra_val, umbra_val, umbra_val);

umbra_val umbra_cvt_f32_i32(struct umbra_builder*, umbra_val);
umbra_val umbra_cvt_i32_f32(struct umbra_builder*, umbra_val);

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

struct umbra_basic_block* umbra_basic_block(struct umbra_builder*);
void   umbra_basic_block_free(struct umbra_basic_block*);

typedef struct { void *ptr; long sz; } umbra_buf;

struct umbra_backend* umbra_backend_interp(void);
struct umbra_backend* umbra_backend_jit   (void);
struct umbra_backend* umbra_backend_metal (void);
struct umbra_program* umbra_backend_compile(struct umbra_backend*,
                                            struct umbra_basic_block const*);
void umbra_backend_flush(struct umbra_backend*);
void umbra_backend_free (struct umbra_backend*);

struct umbra_backend* umbra_program_backend(struct umbra_program*);
void umbra_program_queue(struct umbra_program*, int n, umbra_buf[]);
void umbra_program_free (struct umbra_program*);

#include <stdio.h>
void umbra_program_dump    (struct umbra_program*, FILE*);
void umbra_dump_builder    (struct umbra_builder const*, FILE*);
void umbra_dump_basic_block(struct umbra_basic_block const*, FILE*);
