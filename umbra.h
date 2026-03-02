#pragma once

#include <stddef.h>

struct umbra_inst {
    enum {
        umbra_imm_16, umbra_load_16, umbra_store_16,
        umbra_imm_32, umbra_load_32, umbra_store_32,
        umbra_lane,

        umbra_add_f16, umbra_sub_f16, umbra_mul_f16, umbra_div_f16,
        umbra_add_f32, umbra_sub_f32, umbra_mul_f32, umbra_div_f32,

        umbra_add_i16, umbra_sub_i16, umbra_mul_i16,
        umbra_shl_i16, umbra_shr_u16, umbra_shr_s16,
        umbra_and_i16, umbra_or_i16,  umbra_xor_i16, umbra_sel_i16,

        umbra_add_i32, umbra_sub_i32, umbra_mul_i32,
        umbra_shl_i32, umbra_shr_u32, umbra_shr_s32,
        umbra_and_i32, umbra_or_i32,  umbra_xor_i32, umbra_sel_i32,

        umbra_eq_f16, umbra_ne_f16, umbra_lt_f16, umbra_le_f16, umbra_gt_f16, umbra_ge_f16,
        umbra_eq_f32, umbra_ne_f32, umbra_lt_f32, umbra_le_f32, umbra_gt_f32, umbra_ge_f32,

        umbra_eq_i16, umbra_ne_i16,
        umbra_lt_s16, umbra_le_s16, umbra_gt_s16, umbra_ge_s16,
        umbra_lt_u16, umbra_le_u16, umbra_gt_u16, umbra_ge_u16,

        umbra_eq_i32, umbra_ne_i32,
        umbra_lt_s32, umbra_le_s32, umbra_gt_s32, umbra_ge_s32,
        umbra_lt_u32, umbra_le_u32, umbra_gt_u32, umbra_ge_u32,
    } op;
    int    x,y,z;
    union { int immi; float immf; };
    int    ptr;
};

struct umbra_program* umbra_program     (struct umbra_inst const inst[], int insts);
void                  umbra_program_run (struct umbra_program const*, int n, void* ptr[]);
void                  umbra_program_free(struct umbra_program*);
