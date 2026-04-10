#include "op.h"
#include "assume.h"
#include <math.h>
#include <stdint.h>

#pragma clang diagnostic ignored "-Wfloat-equal"

// Op flag table generated from X-macro flags at compile time.
enum { OP_FUSED_IMM = 1 << 4 };
#define      FLAG(name, flags) [op_##name] = (flags),
#define FUSED_FLAG(name, flags) [op_##name] = (flags) | OP_FUSED_IMM,
static uint8_t const op_flags[] = {
    OTHER_OPS(FLAG) BINARY_OPS(FLAG) UNARY_OPS(FLAG) IMM_OPS(FUSED_FLAG)
};
#undef FUSED_FLAG
#undef FLAG

_Bool op_is_store    (enum op op) { return !!(op_flags[op] & OP_STORE); }
_Bool op_has_ptr     (enum op op) { return !!(op_flags[op] & OP_PTR); }
_Bool op_is_varying  (enum op op) { return !!(op_flags[op] & OP_VARYING); }
_Bool op_is_fused_imm(enum op op) { return !!(op_flags[op] & OP_FUSED_IMM); }

char const* op_name(enum op op) {
    static char const *names[] = {
#define OP_NAME(name, ...) [op_##name] = #name,
        OTHER_OPS(OP_NAME)
        BINARY_OPS(OP_NAME)
        UNARY_OPS(OP_NAME)
        IMM_OPS(OP_NAME)
#undef OP_NAME
    };
    return names[op];
}

int op_eval(enum op op, int xb, int yb, int zb, int wb) {
    typedef union { int i; float f; uint32_t u; } s32;
    s32 x = {xb},
        y = {yb},
        z = {zb},
        w = {wb},
        r = { 0};
    (void)w;

    switch ((int)op) {
    case op_add_f32: r.f = x.f + y.f; return r.i;
    case op_sub_f32: r.f = x.f - y.f; return r.i;
    case op_mul_f32: r.f = x.f * y.f; return r.i;
    case op_div_f32: r.f = x.f / y.f; return r.i;
    case op_min_f32: r.f = x.f < y.f ? x.f : y.f; return r.i;
    case op_max_f32: r.f = x.f > y.f ? x.f : y.f; return r.i;
    case op_sqrt_f32:  r.f = sqrtf(x.f); return r.i;
    case op_abs_f32:   r.f = fabsf(x.f); return r.i;
    case op_round_f32: r.f = rintf(x.f); return r.i;
    case op_floor_f32: r.f = floorf(x.f); return r.i;
    case op_ceil_f32:  r.f = ceilf(x.f); return r.i;
    case op_round_i32: { float t = rintf(x.f);  r.i = (int32_t)t; } return r.i;
    case op_floor_i32: { float t = floorf(x.f); r.i = (int32_t)t; } return r.i;
    case op_ceil_i32:  { float t = ceilf(x.f);  r.i = (int32_t)t; } return r.i;
    case op_add_i32: r.i = x.i + y.i; return r.i;
    case op_sub_i32: r.i = x.i - y.i; return r.i;
    case op_mul_i32: r.i = x.i * y.i; return r.i;
    case op_and_32:  r.i = x.i & y.i; return r.i;
    case op_or_32:   r.i = x.i | y.i; return r.i;
    case op_xor_32:  r.i = x.i ^ y.i; return r.i;
    case op_sel_32:  r.i = (x.i & y.i) | (~x.i & z.i); return r.i;
    case op_f32_from_i32: r.f = (float)x.i; return r.i;
    case op_i32_from_f32: r.i = (int32_t)x.f; return r.i;
    case op_eq_f32: r.i = x.f == y.f ? -1 : 0; return r.i;
    case op_lt_f32: r.i = x.f <  y.f ? -1 : 0; return r.i;
    case op_le_f32: r.i = x.f <= y.f ? -1 : 0; return r.i;
    case op_eq_i32: r.i = x.i == y.i ? -1 : 0; return r.i;
    case op_lt_s32: r.i = x.i <  y.i ? -1 : 0; return r.i;
    case op_le_s32: r.i = x.i <= y.i ? -1 : 0; return r.i;
    case op_lt_u32: r.i = x.u <  y.u ? -1 : 0; return r.i;
    case op_le_u32: r.i = x.u <= y.u ? -1 : 0; return r.i;
    }
    __builtin_unreachable();
}
