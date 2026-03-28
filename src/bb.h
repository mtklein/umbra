#pragma once
#include <stdint.h>
#include "../include/umbra.h"

// umbra_val packing: low 2 bits = channel, upper 30 bits = instruction index.
static inline int       val_id  (umbra_val v) { return v.bits >> 2; }
static inline int       val_chan(umbra_val v) { return v.bits & 3; }
static inline umbra_val val_make(int id, int chan) { return (umbra_val){.bits = (id << 2) | (chan & 3)}; }

#define OP_LIST(X)                                                                        \
    X(x) X(y)                                                                             \
    X(imm_32)                                                                             \
    X(uniform_32)                                                                         \
    X(load_32) X(load_32x2) X(load_8x4) X(gather_uniform_32) X(gather_32)              \
    X(store_32) X(store_32x2) X(store_8x4) X(deref_ptr) X(add_f32) X(sub_f32)                           \
        X(mul_f32) X(div_f32) X(min_f32) X(max_f32) X(sqrt_f32) X(abs_f32) X(neg_f32) X(\
            round_f32) X(floor_f32) X(ceil_f32) X(round_i32) X(floor_i32) X(ceil_i32)    \
            X(fma_f32) X(fms_f32) X(add_i32) X(sub_i32) X(mul_i32) X(shl_i32) X(shr_u32)\
                X(shr_s32) X(and_32) X(or_32) X(xor_32) X(sel_32) X(f32_from_i32)        \
                    X(i32_from_f32) X(eq_f32) X(lt_f32) X(le_f32) X(eq_i32) X(lt_s32) X( \
                        le_s32) X(lt_u32) X(le_u32) X(uniform_16) X(load_16)              \
                        X(store_16) X(gather_uniform_16) X(gather_16) X(i32_from_s16) X(i32_from_u16)          \
                            X(i16_from_i32) X(f32_from_f16) X(f16_from_f32)              \
                                X(shl_i32_imm) X(shr_u32_imm) X(shr_s32_imm) X(and_32_imm)      \
                                    X(pack) X(add_f32_imm) X(sub_f32_imm) X(mul_f32_imm)  \
                                        X(div_f32_imm) X(min_f32_imm) X(max_f32_imm)      \
                                            X(add_i32_imm) X(sub_i32_imm) X(mul_i32_imm)  \
                                                X(or_32_imm) X(xor_32_imm) X(eq_f32_imm)  \
                                                    X(lt_f32_imm) X(le_f32_imm)            \
                                                        X(eq_i32_imm) X(lt_s32_imm)       \
                                                            X(le_s32_imm)

enum op {
#define OP_ENUM(name) op_##name,
    OP_LIST(OP_ENUM)
#undef OP_ENUM
};

struct bb_inst {
    enum op op;
    _Bool   uniform, pad_[3];
    int     x, y, z, w;
    int     cx, cy, cz, cw;
    int     ptr, imm;
};

struct hash_slot {
    uint32_t hash;
    int      ix;
};

struct umbra_builder {
    struct bb_inst   *inst;
    struct hash_slot *ht;
    int               insts, ht_mask, uni_len, : 32;
};

struct umbra_basic_block {
    struct bb_inst *inst;
    int             insts, preamble, uni_len, : 32;
};

_Bool is_store(enum op);
_Bool has_ptr(enum op);
_Bool is_varying(enum op);

_Bool is_fused_imm(enum op);

int umbra_const_eval(enum op, int, int, int);

