#pragma once
#include <stdint.h>

#define OP_LIST(X)                                       \
    X(lane)                                              \
    X(imm_32) X(uni_32) X(load_32)                      \
    X(gather_32) X(store_32) X(scatter_32)               \
    X(deref_ptr)                                         \
    X(add_f32) X(sub_f32) X(mul_f32) X(div_f32)         \
    X(min_f32) X(max_f32) X(sqrt_f32)                   \
    X(fma_f32) X(fms_f32)                               \
    X(add_i32) X(sub_i32) X(mul_i32)                    \
    X(shl_i32) X(shr_u32) X(shr_s32)                    \
    X(and_32) X(or_32) X(xor_32) X(sel_32)              \
    X(f32_from_i32) X(i32_from_f32)                     \
    X(eq_f32) X(lt_f32) X(le_f32)                       \
    X(eq_i32)                                            \
    X(lt_s32) X(le_s32)                                  \
    X(lt_u32) X(le_u32)                                  \
    X(uni_16) X(load_16) X(store_16)                     \
    X(gather_16) X(scatter_16)                           \
    X(widen_s16) X(widen_u16) X(narrow_16)                \
    X(widen_f16) X(narrow_f32)

enum op {
    #define OP_ENUM(name) op_##name,
    OP_LIST(OP_ENUM)
    #undef OP_ENUM
};

struct bb_inst {
    enum op op;
    int     x,y,z,w;
    int     ptr, imm;
};

struct hash_slot { uint32_t hash; int ix; };

struct umbra_basic_block {
    struct bb_inst   *inst;
    struct hash_slot *ht;
    int               insts, ht_mask, preamble, uni_len;
};

static inline _Bool is_store(enum op op) {
    return op == op_store_16
        || op == op_store_32
        || op == op_scatter_16
        || op == op_scatter_32;
}

static inline _Bool has_ptr(enum op op) {
    return op == op_deref_ptr
        || (op >= op_uni_32 && op <= op_scatter_32)
        || (op >= op_uni_16 && op <= op_scatter_16);
}

static inline _Bool is_varying(enum op op) {
    return op == op_lane
        || op == op_load_16
        || op == op_load_32
        || op == op_store_16
        || op == op_store_32;
}

int umbra_const_eval(enum op, int, int, int);
