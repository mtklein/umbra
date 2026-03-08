#pragma once
#include <stdint.h>

// X(name) — order matters: op_type() relies on 32-bit < 16-bit < half.
#define OP_LIST(X) \
    /* 32-bit ops */ \
    X(lane) \
    X(imm_32) X(uni_32) X(load_32) X(gather_32) X(store_32) X(scatter_32) \
    X(add_f32) X(sub_f32) X(mul_f32) X(div_f32) \
    X(min_f32) X(max_f32) X(sqrt_f32) X(fma_f32) \
    X(add_i32) X(sub_i32) X(mul_i32) \
    X(shl_i32) X(shr_u32) X(shr_s32) X(shl_i32_imm) X(shr_u32_imm) X(shr_s32_imm) \
    X(and_32) X(or_32) X(xor_32) X(sel_32) \
    X(f32_from_i32) X(i32_from_f32) X(f32_from_half) X(i32_from_half) X(i32_from_i16) \
    X(eq_f32) X(ne_f32) X(lt_f32) X(le_f32) X(gt_f32) X(ge_f32) \
    X(eq_i32) X(ne_i32) \
    X(lt_s32) X(le_s32) X(gt_s32) X(ge_s32) \
    X(lt_u32) X(le_u32) X(gt_u32) X(ge_u32) \
    /* 16-bit ops */ \
    X(imm_16) X(uni_16) X(load_16) X(gather_16) X(store_16) X(scatter_16) \
    X(load_8x4)  /* produces 4 outputs (u8->u16 widened); continuations reference base */ \
    X(store_8x4) /* consumes 4 u16 inputs (x=ch0, y=ch1, z=ch2, w=ch3), narrowed to u8 */ \
    X(add_i16) X(sub_i16) X(mul_i16) \
    X(shl_i16) X(shr_u16) X(shr_s16) X(shl_i16_imm) X(shr_u16_imm) X(shr_s16_imm) \
    X(and_16) X(or_16) X(xor_16) X(sel_16) \
    X(i16_from_i32) \
    X(eq_i16) X(ne_i16) \
    X(lt_s16) X(le_s16) X(gt_s16) X(ge_s16) \
    X(lt_u16) X(le_u16) X(gt_u16) X(ge_u16) \
    /* Half ops (fp16 in memory, unspecified width in registers) */ \
    X(imm_half) X(uni_half) X(load_half) X(gather_half) X(store_half) X(scatter_half) \
    X(add_half) X(sub_half) X(mul_half) X(div_half) \
    X(min_half) X(max_half) X(sqrt_half) X(fma_half) \
    X(and_half) X(or_half) X(xor_half) X(sel_half) \
    X(half_from_f32) X(half_from_i32) X(half_from_i16) X(i16_from_half) \
    X(eq_half) X(ne_half) \
    X(lt_half) X(le_half) X(gt_half) X(ge_half)

enum op {
    #define OP_ENUM(name) op_##name,
    OP_LIST(OP_ENUM)
    #undef OP_ENUM
};

struct bb_inst {
    enum op op;
    int     x,y,z,w;
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
        || op == op_scatter_16 || op == op_scatter_32 || op == op_scatter_half
        || op == op_store_8x4;
}

static inline _Bool has_ptr(enum op op) {
    return (op >= op_uni_32 && op <= op_scatter_32)
        || op == op_load_8x4 || op == op_store_8x4
        || (op >= op_uni_16 && op <= op_scatter_16)
        || (op >= op_uni_half && op <= op_scatter_half);
}

static inline _Bool is_varying(enum op op) {
    return op == op_lane
        || op == op_load_16 || op == op_load_32 || op == op_load_half
        || op == op_store_16 || op == op_store_32 || op == op_store_half
        || op == op_load_8x4 || op == op_store_8x4;
}

enum op_type { OP_32, OP_16, OP_HALF };

static inline enum op_type op_type(enum op op) {
    if (op >= op_imm_half)    return OP_HALF;
    if (op >= op_imm_16)      return OP_16;
    return OP_32;
}
