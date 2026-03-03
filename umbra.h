#pragma once

enum umbra_op {
    umbra_lane,

    umbra_imm_16, umbra_load_16, umbra_store_16,
    umbra_imm_32, umbra_load_32, umbra_store_32,

    umbra_add_f16, umbra_sub_f16, umbra_mul_f16, umbra_div_f16,
    umbra_add_f32, umbra_sub_f32, umbra_mul_f32, umbra_div_f32,

    umbra_min_f16, umbra_max_f16, umbra_sqrt_f16, umbra_fma_f16,
    umbra_min_f32, umbra_max_f32, umbra_sqrt_f32, umbra_fma_f32,

    umbra_add_i16, umbra_sub_i16, umbra_mul_i16,
    umbra_add_i32, umbra_sub_i32, umbra_mul_i32,

    umbra_shl_i16, umbra_shr_u16, umbra_shr_s16,
    umbra_shl_i32, umbra_shr_u32, umbra_shr_s32,

    umbra_and_16, umbra_or_16,  umbra_xor_16, umbra_sel_16,
    umbra_and_32, umbra_or_32,  umbra_xor_32, umbra_sel_32,

    umbra_f16_from_f32, umbra_f32_from_f16,
    umbra_f32_from_i32, umbra_i32_from_f32,

    umbra_eq_f16, umbra_ne_f16, umbra_lt_f16, umbra_le_f16, umbra_gt_f16, umbra_ge_f16,
    umbra_eq_f32, umbra_ne_f32, umbra_lt_f32, umbra_le_f32, umbra_gt_f32, umbra_ge_f32,

    umbra_eq_i16, umbra_ne_i16,
    umbra_eq_i32, umbra_ne_i32,
    umbra_lt_s16, umbra_le_s16, umbra_gt_s16, umbra_ge_s16,
    umbra_lt_s32, umbra_le_s32, umbra_gt_s32, umbra_ge_s32,
    umbra_lt_u16, umbra_le_u16, umbra_gt_u16, umbra_ge_u16,
    umbra_lt_u32, umbra_le_u32, umbra_gt_u32, umbra_ge_u32,
};

struct umbra_inst {
    enum umbra_op op;
    int           x,y,z,w;
    union   { int ptr; int immi; float immf; };
};
int umbra_optimize(struct umbra_inst[], int insts);

struct umbra_interpreter* umbra_interpreter     (struct umbra_inst const[], int insts);
void                      umbra_interpreter_free(struct umbra_interpreter*);
void umbra_interpreter_run(struct umbra_interpreter const*, int n, void* ptr[]);
