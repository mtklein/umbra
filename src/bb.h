#pragma once
#include <stdint.h>

// Bottom bit encodes result width: even=32-bit, odd=16-bit.
enum op {
    op_lane      =  0,  //          32-bit
    op_imm_32    =  2,  op_imm_16    =  3,
    op_load_32   =  4,  op_load_16   =  5,
    op_store_32  =  6,  op_store_16  =  7,

    op_add_f32   =  8,  op_add_f16   =  9,
    op_sub_f32   = 10,  op_sub_f16   = 11,
    op_mul_f32   = 12,  op_mul_f16   = 13,
    op_div_f32   = 14,  op_div_f16   = 15,
    op_min_f32   = 16,  op_min_f16   = 17,
    op_max_f32   = 18,  op_max_f16   = 19,
    op_sqrt_f32  = 20,  op_sqrt_f16  = 21,
    op_fma_f32   = 22,  op_fma_f16   = 23,

    op_add_i32   = 24,  op_add_i16   = 25,
    op_sub_i32   = 26,  op_sub_i16   = 27,
    op_mul_i32   = 28,  op_mul_i16   = 29,
    op_shl_i32   = 30,  op_shl_i16   = 31,
    op_shr_u32   = 32,  op_shr_u16   = 33,
    op_shr_s32   = 34,  op_shr_s16   = 35,
    op_and_32    = 36,  op_and_16    = 37,
    op_or_32     = 38,  op_or_16     = 39,
    op_xor_32    = 40,  op_xor_16    = 41,
    op_sel_32    = 42,  op_sel_16    = 43,

    op_f32_from_f16 = 44,  op_f16_from_f32 = 45,
    op_f32_from_i32 = 46,
    op_i32_from_f32 = 48,

    op_eq_f32    = 50,  op_eq_f16    = 51,
    op_ne_f32    = 52,  op_ne_f16    = 53,
    op_lt_f32    = 54,  op_lt_f16    = 55,
    op_le_f32    = 56,  op_le_f16    = 57,
    op_gt_f32    = 58,  op_gt_f16    = 59,
    op_ge_f32    = 60,  op_ge_f16    = 61,

    op_eq_i32    = 62,  op_eq_i16    = 63,
    op_ne_i32    = 64,  op_ne_i16    = 65,
    op_lt_s32    = 66,  op_lt_s16    = 67,
    op_le_s32    = 68,  op_le_s16    = 69,
    op_gt_s32    = 70,  op_gt_s16    = 71,
    op_ge_s32    = 72,  op_ge_s16    = 73,
    op_lt_u32    = 74,  op_lt_u16    = 75,
    op_le_u32    = 76,  op_le_u16    = 77,
    op_gt_u32    = 78,  op_gt_u16    = 79,
    op_ge_u32    = 80,  op_ge_u16    = 81,
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

static inline _Bool is_16bit(enum op op) {
    return op & 1;
}
