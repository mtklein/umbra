#pragma once

// Op metadata flags.
//   OTHER_OPS:  X(name, flags) where flags combine OP_STORE, OP_PTR, OP_VARYING.
//   BINARY_OPS: X(name, flags) where flags may include OP_COMMUTATIVE.
//   UNARY_OPS:  X(name, flags) — no flags defined yet, always 0.
//   IMM_OPS:    X(name, flags) — no flags defined yet, always 0.
enum {
    OP_STORE       = 1 << 0,
    OP_PTR         = 1 << 1,
    OP_VARYING     = 1 << 2,
    OP_COMMUTATIVE = 1 << 3,
};

// Ops not covered by BINARY_OPS, UNARY_OPS, or IMM_OPS.
#define OTHER_OPS(X)                                                                       \
    X(x,                                    OP_VARYING)                                    \
    X(y,                                    OP_VARYING)                                    \
    X(imm_32,                               0)                                             \
    X(uniform_32,                           OP_PTR)                                        \
    X(load_32,                              OP_PTR|OP_VARYING)                             \
    X(load_16x4,                            OP_PTR|OP_VARYING)                             \
    X(load_16x4_planar,                     OP_PTR|OP_VARYING)                             \
    X(load_8x4,                             OP_PTR|OP_VARYING)                             \
    X(gather_uniform_32,                    OP_PTR)                                        \
    X(gather_32,                            OP_PTR)                                        \
    X(sample_32,                            OP_PTR)                                        \
    X(store_32,                             OP_STORE|OP_PTR|OP_VARYING)                    \
    X(store_16x4,                           OP_STORE|OP_PTR|OP_VARYING)                    \
    X(store_16x4_planar,                    OP_STORE|OP_PTR|OP_VARYING)                    \
    X(store_8x4,                            OP_STORE|OP_PTR|OP_VARYING)                    \
    X(deref_ptr,                            OP_PTR)                                        \
    X(fma_f32,                              0)                                             \
    X(fms_f32,                              0)                                             \
    X(sel_32,                               0)                                             \
    X(load_16,                              OP_PTR|OP_VARYING)                             \
    X(store_16,                             OP_STORE|OP_PTR|OP_VARYING)                    \
    X(gather_16,                            OP_PTR)                                        \
    X(loop_begin,                           0)                                             \
    X(loop_end,                             0)                                             \
    X(load_var,                             OP_VARYING)                                    \
    X(store_var,                            OP_STORE|OP_VARYING)                           \
    X(cond_store_var,                       OP_STORE|OP_VARYING)                           \
    X(inc_var,                              OP_STORE|OP_VARYING)

// Ops that get register variants in the interpreter.
#define BINARY_OPS(X)                                                                      \
    X(add_f32, OP_COMMUTATIVE) X(sub_f32, 0) X(mul_f32, OP_COMMUTATIVE)                   \
    X(div_f32, 0) X(min_f32, OP_COMMUTATIVE) X(max_f32, OP_COMMUTATIVE)                   \
    X(add_i32, OP_COMMUTATIVE) X(sub_i32, 0) X(mul_i32, OP_COMMUTATIVE)                   \
    X(shl_i32, 0) X(shr_u32, 0) X(shr_s32, 0)                                            \
    X(and_32,  OP_COMMUTATIVE) X(or_32, OP_COMMUTATIVE) X(xor_32, OP_COMMUTATIVE)         \
    X(eq_f32,  OP_COMMUTATIVE) X(lt_f32, 0) X(le_f32, 0)                                  \
    X(eq_i32,  OP_COMMUTATIVE) X(lt_s32, 0) X(le_s32, 0)                                  \
    X(lt_u32,  0) X(le_u32, 0)

#define UNARY_OPS(X)                                                                       \
    X(sqrt_f32, 0) X(abs_f32, 0)                                                          \
    X(round_f32, 0) X(floor_f32, 0) X(ceil_f32, 0)                                        \
    X(round_i32, 0) X(floor_i32, 0) X(ceil_i32, 0)                                        \
    X(f32_from_i32, 0) X(i32_from_f32, 0)                                                 \
    X(f32_from_f16, 0) X(f16_from_f32, 0)                                                 \
    X(i32_from_s16, 0) X(i32_from_u16, 0) X(i16_from_i32, 0)

// _imm ops: x is the only variable input, y is the immediate.
#define IMM_OPS(X)                                                                         \
    X(shl_i32_imm, 0) X(shr_u32_imm, 0) X(shr_s32_imm, 0)                                \
    X(and_32_imm,  0) X(or_32_imm,   0) X(xor_32_imm,   0)                                \
    X(add_f32_imm, 0) X(sub_f32_imm, 0) X(mul_f32_imm, 0)                                 \
    X(div_f32_imm, 0) X(min_f32_imm, 0) X(max_f32_imm, 0)                                 \
    X(add_i32_imm, 0) X(sub_i32_imm, 0) X(mul_i32_imm, 0)                                 \
    X(eq_f32_imm,  0) X(lt_f32_imm,  0) X(le_f32_imm,  0)                                 \
    X(eq_i32_imm,  0) X(lt_s32_imm,  0) X(le_s32_imm,  0)

enum op {
#define OP_ENUM(name, ...) op_##name,
    OTHER_OPS(OP_ENUM)
    BINARY_OPS(OP_ENUM)
    UNARY_OPS(OP_ENUM)
    IMM_OPS(OP_ENUM)
#undef OP_ENUM
};

_Bool       op_is_store    (enum op);
_Bool       op_has_ptr     (enum op);
_Bool       op_is_varying  (enum op);
_Bool       op_is_fused_imm(enum op);
char const* op_name        (enum op);
int         op_eval        (enum op, int, int, int, int);
