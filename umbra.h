#pragma once

struct umbra_basic_block* umbra_basic_block(void);
void umbra_basic_block_free(struct umbra_basic_block*);

typedef struct {int id;} umbra_v16;
typedef struct {int id;} umbra_v32;
typedef struct {int ix;} umbra_ptr;

umbra_v32 umbra_lane(struct umbra_basic_block*);

umbra_v16 umbra_imm_16(struct umbra_basic_block*, unsigned short bits);
umbra_v32 umbra_imm_32(struct umbra_basic_block*, unsigned int   bits);

umbra_v16 umbra_load_16(struct umbra_basic_block*, umbra_ptr src, umbra_v32 ix);
umbra_v32 umbra_load_32(struct umbra_basic_block*, umbra_ptr src, umbra_v32 ix);

void umbra_store_16(struct umbra_basic_block*, umbra_ptr dst, umbra_v32 ix, umbra_v16);
void umbra_store_32(struct umbra_basic_block*, umbra_ptr dst, umbra_v32 ix, umbra_v32);

umbra_v16 umbra_add_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_sub_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_mul_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_div_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_min_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_max_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);

umbra_v32 umbra_add_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_sub_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_mul_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_div_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_min_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_max_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);

umbra_v16 umbra_sqrt_f16(struct umbra_basic_block*, umbra_v16);
umbra_v32 umbra_sqrt_f32(struct umbra_basic_block*, umbra_v32);

umbra_v16 umbra_add_i16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_sub_i16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_mul_i16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_shl_i16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_shr_u16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_shr_s16(struct umbra_basic_block*, umbra_v16, umbra_v16);

umbra_v32 umbra_add_i32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_sub_i32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_mul_i32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_shl_i32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_shr_u32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_shr_s32(struct umbra_basic_block*, umbra_v32, umbra_v32);

umbra_v16 umbra_and_16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_or_16 (struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_xor_16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_sel_16(struct umbra_basic_block*, umbra_v16, umbra_v16, umbra_v16);

umbra_v32 umbra_and_32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_or_32 (struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_xor_32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_sel_32(struct umbra_basic_block*, umbra_v32, umbra_v32, umbra_v32);

umbra_v16 umbra_f16_from_f32(struct umbra_basic_block*, umbra_v32);
umbra_v32 umbra_f32_from_f16(struct umbra_basic_block*, umbra_v16);
umbra_v32 umbra_f32_from_i32(struct umbra_basic_block*, umbra_v32);
umbra_v32 umbra_i32_from_f32(struct umbra_basic_block*, umbra_v32);

umbra_v16 umbra_eq_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_ne_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_lt_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_le_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_gt_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_ge_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);

umbra_v32 umbra_eq_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_ne_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_lt_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_le_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_gt_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_ge_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);

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

struct umbra_interpreter* umbra_interpreter(struct umbra_basic_block const*);
void umbra_interpreter_run (struct umbra_interpreter*, int n, void* ptr[]);
void umbra_interpreter_free(struct umbra_interpreter*);

struct umbra_codegen* umbra_codegen(struct umbra_basic_block const*);
void umbra_codegen_run (struct umbra_codegen*, int n, void* ptr[]);
void umbra_codegen_free(struct umbra_codegen*);
