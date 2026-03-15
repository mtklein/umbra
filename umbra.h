#pragma once

struct umbra_basic_block* umbra_basic_block(void);
void umbra_basic_block_free(struct umbra_basic_block*);
void umbra_basic_block_optimize(struct umbra_basic_block*);

typedef struct {int id;} umbra_i16;
typedef struct {int id;} umbra_i32;
typedef struct {int id;} umbra_f32;
typedef struct {int id;} umbra_half;
typedef struct {int id;} umbra_half_mask;
typedef struct {int ix;} umbra_ptr;

umbra_i32 umbra_lane(struct umbra_basic_block*);

int       umbra_reserve_i32 (struct umbra_basic_block*, int n);
int       umbra_reserve_f32 (struct umbra_basic_block*, int n);
int       umbra_reserve_half(struct umbra_basic_block*, int n);
int       umbra_reserve_ptr (struct umbra_basic_block*);
umbra_ptr umbra_deref_ptr   (struct umbra_basic_block*, umbra_ptr buf, int byte_off);
int       umbra_uni_len    (struct umbra_basic_block const*);
void      umbra_set_uni_len(struct umbra_basic_block*, int);

umbra_i32       umbra_imm_i32      (struct umbra_basic_block*, unsigned int   bits);
umbra_f32       umbra_imm_f32      (struct umbra_basic_block*, unsigned int   bits);
umbra_i16       umbra_imm_i16      (struct umbra_basic_block*, unsigned short bits);
umbra_half      umbra_imm_half     (struct umbra_basic_block*, unsigned short bits);
umbra_half_mask umbra_imm_half_mask(struct umbra_basic_block*, unsigned short bits);

umbra_i32  umbra_load_i32 (struct umbra_basic_block*, umbra_ptr src, umbra_i32 ix);
umbra_f32  umbra_load_f32 (struct umbra_basic_block*, umbra_ptr src, umbra_i32 ix);
umbra_i16  umbra_load_i16 (struct umbra_basic_block*, umbra_ptr src, umbra_i32 ix);
umbra_half umbra_load_half(struct umbra_basic_block*, umbra_ptr src, umbra_i32 ix);

void umbra_store_i32 (struct umbra_basic_block*, umbra_ptr dst, umbra_i32 ix, umbra_i32);
void umbra_store_f32 (struct umbra_basic_block*, umbra_ptr dst, umbra_i32 ix, umbra_f32);
void umbra_store_i16 (struct umbra_basic_block*, umbra_ptr dst, umbra_i32 ix, umbra_i16);
void umbra_store_half(struct umbra_basic_block*, umbra_ptr dst, umbra_i32 ix, umbra_half);

umbra_f32 umbra_add_f32(struct umbra_basic_block*, umbra_f32, umbra_f32);
umbra_f32 umbra_sub_f32(struct umbra_basic_block*, umbra_f32, umbra_f32);
umbra_f32 umbra_mul_f32(struct umbra_basic_block*, umbra_f32, umbra_f32);
umbra_f32 umbra_div_f32(struct umbra_basic_block*, umbra_f32, umbra_f32);
umbra_f32 umbra_min_f32(struct umbra_basic_block*, umbra_f32, umbra_f32);
umbra_f32 umbra_max_f32(struct umbra_basic_block*, umbra_f32, umbra_f32);

umbra_half umbra_add_half(struct umbra_basic_block*, umbra_half, umbra_half);
umbra_half umbra_sub_half(struct umbra_basic_block*, umbra_half, umbra_half);
umbra_half umbra_mul_half(struct umbra_basic_block*, umbra_half, umbra_half);
umbra_half umbra_div_half(struct umbra_basic_block*, umbra_half, umbra_half);
umbra_half umbra_min_half(struct umbra_basic_block*, umbra_half, umbra_half);
umbra_half umbra_max_half(struct umbra_basic_block*, umbra_half, umbra_half);

umbra_f32  umbra_sqrt_f32 (struct umbra_basic_block*, umbra_f32);
umbra_half umbra_sqrt_half(struct umbra_basic_block*, umbra_half);

umbra_i32 umbra_add_i32(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_sub_i32(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_mul_i32(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_shl_i32(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_shr_u32(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_shr_s32(struct umbra_basic_block*, umbra_i32, umbra_i32);

umbra_i16 umbra_add_i16(struct umbra_basic_block*, umbra_i16, umbra_i16);
umbra_i16 umbra_sub_i16(struct umbra_basic_block*, umbra_i16, umbra_i16);
umbra_i16 umbra_mul_i16(struct umbra_basic_block*, umbra_i16, umbra_i16);
umbra_i16 umbra_shl_i16(struct umbra_basic_block*, umbra_i16, umbra_i16);
umbra_i16 umbra_shr_u16(struct umbra_basic_block*, umbra_i16, umbra_i16);
umbra_i16 umbra_shr_s16(struct umbra_basic_block*, umbra_i16, umbra_i16);

umbra_i32 umbra_and_32(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_or_32 (struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_xor_32(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_sel_i32(struct umbra_basic_block*, umbra_i32, umbra_i32, umbra_i32);
umbra_f32 umbra_sel_f32(struct umbra_basic_block*, umbra_i32, umbra_f32, umbra_f32);

umbra_i16 umbra_and_16(struct umbra_basic_block*, umbra_i16, umbra_i16);
umbra_i16 umbra_or_16 (struct umbra_basic_block*, umbra_i16, umbra_i16);
umbra_i16 umbra_xor_16(struct umbra_basic_block*, umbra_i16, umbra_i16);
umbra_i16 umbra_sel_16(struct umbra_basic_block*, umbra_i16, umbra_i16, umbra_i16);

umbra_half_mask umbra_and_half(struct umbra_basic_block*, umbra_half_mask, umbra_half_mask);
umbra_half_mask umbra_or_half (struct umbra_basic_block*, umbra_half_mask, umbra_half_mask);
umbra_half_mask umbra_xor_half(struct umbra_basic_block*, umbra_half_mask, umbra_half_mask);
umbra_half      umbra_sel_half(struct umbra_basic_block*, umbra_half_mask, umbra_half, umbra_half);

umbra_f32  umbra_f32_from_i32 (struct umbra_basic_block*, umbra_i32);
umbra_i32  umbra_i32_from_f32 (struct umbra_basic_block*, umbra_f32);
umbra_half umbra_half_from_f32(struct umbra_basic_block*, umbra_f32);
umbra_half umbra_half_from_i32(struct umbra_basic_block*, umbra_i32);
umbra_half umbra_half_from_i16(struct umbra_basic_block*, umbra_i16);
umbra_f32  umbra_f32_from_half(struct umbra_basic_block*, umbra_half);
umbra_i32  umbra_i32_from_half(struct umbra_basic_block*, umbra_half);
umbra_i16  umbra_i16_from_half(struct umbra_basic_block*, umbra_half);
umbra_i16  umbra_i16_from_i32 (struct umbra_basic_block*, umbra_i32);
umbra_i32  umbra_i32_from_i16 (struct umbra_basic_block*, umbra_i16);
umbra_half_mask umbra_half_mask_from_i32(struct umbra_basic_block*, umbra_i32);

void umbra_load_8x4 (struct umbra_basic_block*, umbra_ptr src, umbra_i32 ix, umbra_i16 out[4]);
void umbra_store_8x4(struct umbra_basic_block*, umbra_ptr dst, umbra_i32 ix, umbra_i16 in[4]);

umbra_i32 umbra_eq_f32(struct umbra_basic_block*, umbra_f32, umbra_f32);
umbra_i32 umbra_ne_f32(struct umbra_basic_block*, umbra_f32, umbra_f32);
umbra_i32 umbra_lt_f32(struct umbra_basic_block*, umbra_f32, umbra_f32);
umbra_i32 umbra_le_f32(struct umbra_basic_block*, umbra_f32, umbra_f32);
umbra_i32 umbra_gt_f32(struct umbra_basic_block*, umbra_f32, umbra_f32);
umbra_i32 umbra_ge_f32(struct umbra_basic_block*, umbra_f32, umbra_f32);

umbra_half_mask umbra_eq_half(struct umbra_basic_block*, umbra_half, umbra_half);
umbra_half_mask umbra_ne_half(struct umbra_basic_block*, umbra_half, umbra_half);
umbra_half_mask umbra_lt_half(struct umbra_basic_block*, umbra_half, umbra_half);
umbra_half_mask umbra_le_half(struct umbra_basic_block*, umbra_half, umbra_half);
umbra_half_mask umbra_gt_half(struct umbra_basic_block*, umbra_half, umbra_half);
umbra_half_mask umbra_ge_half(struct umbra_basic_block*, umbra_half, umbra_half);

umbra_i32 umbra_eq_i32(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_ne_i32(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_lt_s32(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_le_s32(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_gt_s32(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_ge_s32(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_lt_u32(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_le_u32(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_gt_u32(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_ge_u32(struct umbra_basic_block*, umbra_i32, umbra_i32);

umbra_i16 umbra_eq_i16(struct umbra_basic_block*, umbra_i16, umbra_i16);
umbra_i16 umbra_ne_i16(struct umbra_basic_block*, umbra_i16, umbra_i16);
umbra_i16 umbra_lt_s16(struct umbra_basic_block*, umbra_i16, umbra_i16);
umbra_i16 umbra_le_s16(struct umbra_basic_block*, umbra_i16, umbra_i16);
umbra_i16 umbra_gt_s16(struct umbra_basic_block*, umbra_i16, umbra_i16);
umbra_i16 umbra_ge_s16(struct umbra_basic_block*, umbra_i16, umbra_i16);
umbra_i16 umbra_lt_u16(struct umbra_basic_block*, umbra_i16, umbra_i16);
umbra_i16 umbra_le_u16(struct umbra_basic_block*, umbra_i16, umbra_i16);
umbra_i16 umbra_gt_u16(struct umbra_basic_block*, umbra_i16, umbra_i16);
umbra_i16 umbra_ge_u16(struct umbra_basic_block*, umbra_i16, umbra_i16);

typedef struct { void *ptr; long sz; } umbra_buf;

struct umbra_interpreter* umbra_interpreter(struct umbra_basic_block const*);
void umbra_interpreter_run (struct umbra_interpreter*, int n, umbra_buf[]);
void umbra_interpreter_free(struct umbra_interpreter*);

struct umbra_codegen* umbra_codegen(struct umbra_basic_block const*);
void umbra_codegen_run (struct umbra_codegen*, int n, umbra_buf[]);
void umbra_codegen_free(struct umbra_codegen*);

struct umbra_jit* umbra_jit(struct umbra_basic_block const*);
void umbra_jit_run (struct umbra_jit*, int n, umbra_buf[]);
void umbra_jit_free(struct umbra_jit*);

struct umbra_metal* umbra_metal(struct umbra_basic_block const*);
void umbra_metal_run        (struct umbra_metal*, int n, umbra_buf[]);
void umbra_metal_begin_batch(struct umbra_metal*);
void umbra_metal_flush      (struct umbra_metal*);
void umbra_metal_free       (struct umbra_metal*);

#include <stdio.h>
void umbra_basic_block_dump(struct umbra_basic_block const*, FILE*);
void umbra_codegen_dump    (struct umbra_codegen const*, FILE*);
void umbra_jit_dump        (struct umbra_jit const*, FILE*);
void umbra_jit_dump_bin    (struct umbra_jit const*, FILE*);
void umbra_jit_mca         (struct umbra_jit const*, FILE*);
void umbra_metal_dump      (struct umbra_metal const*, FILE*);
