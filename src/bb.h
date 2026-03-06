#pragma once
#include <stdint.h>

enum op {
    // 32-bit ops
    op_lane,
    op_imm_32, op_uni_32, op_load_32, op_gather_32, op_store_32, op_scatter_32,
    op_add_f32, op_sub_f32, op_mul_f32, op_div_f32,
    op_min_f32, op_max_f32, op_sqrt_f32, op_fma_f32,
    op_add_i32, op_sub_i32, op_mul_i32,
    op_shl_i32, op_shr_u32, op_shr_s32, op_shl_i32_imm, op_shr_u32_imm, op_shr_s32_imm,
    op_and_32, op_or_32, op_xor_32, op_sel_32,
    op_f32_from_i32, op_i32_from_f32, op_f32_from_half, op_i32_from_half, op_i32_from_i16,
    op_eq_f32, op_ne_f32, op_lt_f32, op_le_f32, op_gt_f32, op_ge_f32,
    op_eq_i32, op_ne_i32,
    op_lt_s32, op_le_s32, op_gt_s32, op_ge_s32,
    op_lt_u32, op_le_u32, op_gt_u32, op_ge_u32,
    op_bytes,

    // 8-bit ops
    op_load_8x4_0, op_load_8x4_1, op_load_8x4_2, op_load_8x4_3,

    // 16-bit ops
    op_imm_16, op_uni_16, op_load_16, op_gather_16, op_store_16, op_scatter_16,
    op_add_i16, op_sub_i16, op_mul_i16,
    op_shl_i16, op_shr_u16, op_shr_s16, op_shl_i16_imm, op_shr_u16_imm, op_shr_s16_imm,
    op_and_16, op_or_16, op_xor_16, op_sel_16,
    op_i16_from_i32, op_i16_from_u8,
    op_eq_i16, op_ne_i16,
    op_lt_s16, op_le_s16, op_gt_s16, op_ge_s16,
    op_lt_u16, op_le_u16, op_gt_u16, op_ge_u16,

    // Half ops (fp16 in memory, unspecified width in registers)
    op_imm_half, op_uni_half, op_load_half, op_gather_half, op_store_half, op_scatter_half,
    op_add_half, op_sub_half, op_mul_half, op_div_half,
    op_min_half, op_max_half, op_sqrt_half, op_fma_half,
    op_and_half, op_or_half, op_xor_half, op_sel_half,
    op_half_from_f32, op_half_from_i32, op_half_from_i16, op_i16_from_half,
    op_eq_half, op_ne_half,
    op_lt_half, op_le_half, op_gt_half, op_ge_half,
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
    int               insts, ht_mask, preamble, :32;
};

static inline _Bool is_store(enum op op) {
    return op == op_store_16 || op == op_store_32 || op == op_store_half
        || op == op_scatter_16 || op == op_scatter_32 || op == op_scatter_half;
}

static inline _Bool has_ptr(enum op op) {
    return (op >= op_uni_32 && op <= op_scatter_32)
        || (op >= op_load_8x4_0 && op <= op_load_8x4_3)
        || (op >= op_uni_16 && op <= op_scatter_16)
        || (op >= op_uni_half && op <= op_scatter_half);
}

static inline _Bool is_varying(enum op op) {
    return op == op_lane
        || op == op_load_16 || op == op_load_32 || op == op_load_half
        || op == op_store_16 || op == op_store_32 || op == op_store_half
        || (op >= op_load_8x4_0 && op <= op_load_8x4_3);
}

enum op_type { OP_32, OP_8, OP_16, OP_HALF };

static inline enum op_type op_type(enum op op) {
    if (op >= op_imm_half)    return OP_HALF;
    if (op >= op_imm_16)      return OP_16;
    if (op >= op_load_8x4_0)  return OP_8;
    return OP_32;
}
