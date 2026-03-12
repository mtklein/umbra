#pragma once

struct umbra_basic_block* umbra_basic_block(void);
void umbra_basic_block_free(struct umbra_basic_block*);
void umbra_basic_block_optimize(struct umbra_basic_block*);

typedef struct {int id;} umbra_v16;
typedef struct {int id;} umbra_v32;
typedef struct {int id;} umbra_half;
typedef struct {int ix;} umbra_ptr;

umbra_v32 umbra_lane(struct umbra_basic_block*);

umbra_v32  umbra_imm_32  (struct umbra_basic_block*, unsigned int   bits);
umbra_v16  umbra_imm_16  (struct umbra_basic_block*, unsigned short bits);
umbra_half umbra_imm_half(struct umbra_basic_block*, unsigned short bits);

umbra_v32  umbra_load_32  (struct umbra_basic_block*, umbra_ptr src, umbra_v32 ix);
umbra_v16  umbra_load_16  (struct umbra_basic_block*, umbra_ptr src, umbra_v32 ix);
umbra_half umbra_load_half(struct umbra_basic_block*, umbra_ptr src, umbra_v32 ix);

void umbra_store_32  (struct umbra_basic_block*, umbra_ptr dst, umbra_v32 ix, umbra_v32);
void umbra_store_16  (struct umbra_basic_block*, umbra_ptr dst, umbra_v32 ix, umbra_v16);
void umbra_store_half(struct umbra_basic_block*, umbra_ptr dst, umbra_v32 ix, umbra_half);

umbra_v32 umbra_add_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_sub_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_mul_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_div_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_min_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_max_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);

umbra_half umbra_add_half(struct umbra_basic_block*, umbra_half, umbra_half);
umbra_half umbra_sub_half(struct umbra_basic_block*, umbra_half, umbra_half);
umbra_half umbra_mul_half(struct umbra_basic_block*, umbra_half, umbra_half);
umbra_half umbra_div_half(struct umbra_basic_block*, umbra_half, umbra_half);
umbra_half umbra_min_half(struct umbra_basic_block*, umbra_half, umbra_half);
umbra_half umbra_max_half(struct umbra_basic_block*, umbra_half, umbra_half);

umbra_v32  umbra_sqrt_f32 (struct umbra_basic_block*, umbra_v32);
umbra_half umbra_sqrt_half(struct umbra_basic_block*, umbra_half);

umbra_v32 umbra_add_i32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_sub_i32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_mul_i32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_shl_i32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_shr_u32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_shr_s32(struct umbra_basic_block*, umbra_v32, umbra_v32);

umbra_v16 umbra_add_i16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_sub_i16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_mul_i16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_shl_i16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_shr_u16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_shr_s16(struct umbra_basic_block*, umbra_v16, umbra_v16);

umbra_v32 umbra_and_32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_or_32 (struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_xor_32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_sel_32(struct umbra_basic_block*, umbra_v32, umbra_v32, umbra_v32);

umbra_v16 umbra_and_16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_or_16 (struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_xor_16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_sel_16(struct umbra_basic_block*, umbra_v16, umbra_v16, umbra_v16);

umbra_half umbra_and_half(struct umbra_basic_block*, umbra_half, umbra_half);
umbra_half umbra_or_half (struct umbra_basic_block*, umbra_half, umbra_half);
umbra_half umbra_xor_half(struct umbra_basic_block*, umbra_half, umbra_half);
umbra_half umbra_sel_half(struct umbra_basic_block*, umbra_half, umbra_half, umbra_half);

umbra_v32  umbra_f32_from_i32 (struct umbra_basic_block*, umbra_v32);
umbra_v32  umbra_i32_from_f32 (struct umbra_basic_block*, umbra_v32);
umbra_half umbra_half_from_f32(struct umbra_basic_block*, umbra_v32);
umbra_half umbra_half_from_i32(struct umbra_basic_block*, umbra_v32);
umbra_half umbra_half_from_i16(struct umbra_basic_block*, umbra_v16);
umbra_v32  umbra_f32_from_half(struct umbra_basic_block*, umbra_half);
umbra_v32  umbra_i32_from_half(struct umbra_basic_block*, umbra_half);
umbra_v16  umbra_i16_from_half(struct umbra_basic_block*, umbra_half);
umbra_v16  umbra_i16_from_i32 (struct umbra_basic_block*, umbra_v32);
umbra_v32  umbra_i32_from_i16 (struct umbra_basic_block*, umbra_v16);

void umbra_load_8x4 (struct umbra_basic_block*, umbra_ptr src, umbra_v32 ix, umbra_v16 out[4]);
void umbra_store_8x4(struct umbra_basic_block*, umbra_ptr dst, umbra_v32 ix, umbra_v16 in[4]);

umbra_v32 umbra_eq_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_ne_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_lt_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_le_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_gt_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_ge_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);

umbra_half umbra_eq_half(struct umbra_basic_block*, umbra_half, umbra_half);
umbra_half umbra_ne_half(struct umbra_basic_block*, umbra_half, umbra_half);
umbra_half umbra_lt_half(struct umbra_basic_block*, umbra_half, umbra_half);
umbra_half umbra_le_half(struct umbra_basic_block*, umbra_half, umbra_half);
umbra_half umbra_gt_half(struct umbra_basic_block*, umbra_half, umbra_half);
umbra_half umbra_ge_half(struct umbra_basic_block*, umbra_half, umbra_half);

umbra_v32 umbra_eq_i32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_ne_i32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_lt_s32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_le_s32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_gt_s32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_ge_s32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_lt_u32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_le_u32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_gt_u32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_ge_u32(struct umbra_basic_block*, umbra_v32, umbra_v32);

umbra_v16 umbra_eq_i16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_ne_i16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_lt_s16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_le_s16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_gt_s16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_ge_s16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_lt_u16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_le_u16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_gt_u16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_ge_u16(struct umbra_basic_block*, umbra_v16, umbra_v16);

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
void umbra_metal_run (struct umbra_metal*, int n, umbra_buf[]);
void umbra_metal_free(struct umbra_metal*);

#include <stdio.h>
void umbra_basic_block_dump(struct umbra_basic_block const*, FILE*);
void umbra_codegen_dump    (struct umbra_codegen const*, FILE*);
void umbra_jit_dump        (struct umbra_jit const*, FILE*);
void umbra_jit_dump_bin    (struct umbra_jit const*, FILE*);
void umbra_jit_mca         (struct umbra_jit const*, FILE*);
void umbra_metal_dump      (struct umbra_metal const*, FILE*);
