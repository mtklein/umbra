#pragma once
#include <stdint.h>
#include "../include/umbra.h"

// Internally, umbra_val is a union giving named access to the packed fields.
// Low 2 bits = channel, upper 30 bits = instruction index.
// The public header sees only { int bits; }.
typedef union {
    int      bits;
    struct { unsigned chan : 2, id : 30; };
} val_;

// Ops not covered by BINARY_OPS, UNARY_OPS, or IMM_OPS.
#define OTHER_OPS(X)                                                                       \
    X(x) X(y) X(imm_32) X(uniform_32)                                                     \
    X(load_32) X(load_16x4) X(load_16x4_planar) X(gather_uniform_32) X(gather_32)          \
    X(sample_32) X(store_32) X(store_16x4) X(store_16x4_planar) X(deref_ptr)              \
    X(fma_f32) X(fms_f32) X(sel_32)                                                       \
    X(load_16) X(store_16) X(gather_16)                                                    \
    X(i32_from_s16) X(i32_from_u16) X(i16_from_i32) X(f32_from_f16) X(f16_from_f32)

// Ops that get register variants.
#define BINARY_OPS(X)                                                                      \
    X(add_f32, f32, f32) X(sub_f32, f32, f32) X(mul_f32, f32, f32) X(div_f32, f32, f32)   \
    X(min_f32, f32, f32) X(max_f32, f32, f32)                                             \
    X(add_i32, i32, i32) X(sub_i32, i32, i32) X(mul_i32, i32, i32)                        \
    X(shl_i32, i32, i32) X(shr_u32, u32, u32) X(shr_s32, i32, i32)                        \
    X(and_32,  i32, i32) X(or_32,   i32, i32) X(xor_32,  i32, i32)                        \
    X(eq_f32,  i32, f32) X(lt_f32,  i32, f32) X(le_f32,  i32, f32)                        \
    X(eq_i32,  i32, i32) X(lt_s32,  i32, i32) X(le_s32,  i32, i32)                        \
    X(lt_u32,  i32, u32) X(le_u32,  i32, u32)

#define UNARY_OPS(X)                                                                       \
    X(sqrt_f32,    f32, f32) X(abs_f32,     f32, f32)                                      \
    X(round_f32,   f32, f32) X(floor_f32,   f32, f32) X(ceil_f32,    f32, f32)             \
    X(round_i32,   i32, f32) X(floor_i32,   i32, f32) X(ceil_i32,    i32, f32)             \
    X(f32_from_i32,f32, i32) X(i32_from_f32,i32, f32)

// _imm ops: x is the only variable input, y is the immediate.
#define IMM_OPS(X)                                                                         \
    X(shl_i32_imm, i32, i32) X(shr_u32_imm, u32, u32) X(shr_s32_imm, i32, i32)           \
    X(and_32_imm,  u32, u32) X(or_32_imm,   u32, u32) X(xor_32_imm,  u32, u32)           \
    X(add_f32_imm, f32, f32) X(sub_f32_imm, f32, f32) X(mul_f32_imm, f32, f32)            \
    X(div_f32_imm, f32, f32) X(min_f32_imm, f32, f32) X(max_f32_imm, f32, f32)            \
    X(add_i32_imm, i32, i32) X(sub_i32_imm, i32, i32) X(mul_i32_imm, i32, i32)            \
    X(eq_f32_imm,  i32, f32) X(lt_f32_imm,  i32, f32) X(le_f32_imm,  i32, f32)           \
    X(eq_i32_imm,  i32, i32) X(lt_s32_imm,  i32, i32) X(le_s32_imm,  i32, i32)

enum op {
#define OP_ENUM_1(name)         op_##name,
#define OP_ENUM_3(name, rt, pt) op_##name,
    OTHER_OPS(OP_ENUM_1)
    BINARY_OPS(OP_ENUM_3)
    UNARY_OPS(OP_ENUM_3)
    IMM_OPS(OP_ENUM_3)
#undef OP_ENUM_1
#undef OP_ENUM_3
};

struct bb_inst {
    enum op op;
    _Bool       uniform, pad_[3];
    val_        x, y, z, w;
    int         ptr, imm;
};

struct hash_slot {
    uint32_t hash;
    int      ix;
};

struct umbra_builder {
    struct bb_inst    *inst;
    struct hash_slot  *ht;
    int                insts, ht_mask;
};

struct umbra_basic_block {
    struct bb_inst *inst;
    int             insts, preamble;
};

_Bool is_store(enum op);
_Bool has_ptr(enum op);
_Bool is_varying(enum op);

_Bool is_fused_imm(enum op);

int umbra_const_eval(enum op, int, int, int);

