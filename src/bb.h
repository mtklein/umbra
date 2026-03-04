#pragma once
#include <stdint.h>

enum op {
    op_lane, op_imm_16, op_imm_32,
    op_load_16, op_load_32, op_store_16, op_store_32,

    op_add_f16, op_sub_f16, op_mul_f16, op_div_f16,
    op_min_f16, op_max_f16, op_sqrt_f16, op_fma_f16,
    op_add_f32, op_sub_f32, op_mul_f32, op_div_f32,
    op_min_f32, op_max_f32, op_sqrt_f32, op_fma_f32,

    op_add_i16, op_sub_i16, op_mul_i16,
    op_shl_i16, op_shr_u16, op_shr_s16,
    op_and_16, op_or_16, op_xor_16, op_sel_16,
    op_add_i32, op_sub_i32, op_mul_i32,
    op_shl_i32, op_shr_u32, op_shr_s32,
    op_and_32, op_or_32, op_xor_32, op_sel_32,

    op_f16_from_f32, op_f32_from_f16,
    op_f32_from_i32, op_i32_from_f32,

    op_eq_f16, op_ne_f16, op_lt_f16, op_le_f16, op_gt_f16, op_ge_f16,
    op_eq_f32, op_ne_f32, op_lt_f32, op_le_f32, op_gt_f32, op_ge_f32,
    op_eq_i16, op_ne_i16,
    op_lt_s16, op_le_s16, op_gt_s16, op_ge_s16,
    op_lt_u16, op_le_u16, op_gt_u16, op_ge_u16,
    op_eq_i32, op_ne_i32,
    op_lt_s32, op_le_s32, op_gt_s32, op_ge_s32,
    op_lt_u32, op_le_u32, op_gt_u32, op_ge_u32,
};

struct bb_inst {
    enum op op;
    int     x,y,z;
    union { int ptr; int imm; };
};

struct hash_slot { uint32_t hash; int ix; };

struct umbra_basic_block {
    struct bb_inst   *inst;
    struct hash_slot *ht;
    int               insts, ht_mask;
};

static inline _Bool is_store(enum op op) {
    return op == op_store_16 || op == op_store_32;
}

static inline _Bool is_16bit_result(enum op op) {
    switch (op) {
        case op_imm_16: case op_load_16: case op_store_16:
        case op_add_f16: case op_sub_f16: case op_mul_f16: case op_div_f16:
        case op_min_f16: case op_max_f16: case op_sqrt_f16: case op_fma_f16:
        case op_add_i16: case op_sub_i16: case op_mul_i16:
        case op_shl_i16: case op_shr_u16: case op_shr_s16:
        case op_and_16: case op_or_16: case op_xor_16: case op_sel_16:
        case op_eq_f16: case op_ne_f16: case op_lt_f16: case op_le_f16:
        case op_gt_f16: case op_ge_f16:
        case op_eq_i16: case op_ne_i16:
        case op_lt_s16: case op_le_s16: case op_gt_s16: case op_ge_s16:
        case op_lt_u16: case op_le_u16: case op_gt_u16: case op_ge_u16:
        case op_f16_from_f32:
            return 1;
        default:
            return 0;
    }
}
