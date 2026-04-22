#include "assume.h"
#include "flat_ir.h"
#include "count.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wfloat-equal"
#ifdef __clang__
#pragma clang diagnostic ignored "-Wgnu-label-as-value"
#else
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#define cast(T, v) __builtin_convertvector(v, T)

#define K 16
static int const iota[K] = {
    0, 1, 2, 3,  4, 5, 6, 7,  8, 9,10,11, 12,13,14,15,
};

typedef int32_t  I32 __attribute__((vector_size(K * 4)));
typedef uint32_t U32 __attribute__((vector_size(K * 4)));
typedef float    F32 __attribute__((vector_size(K * 4)));
typedef double   F64 __attribute__((vector_size(K * 8)));
typedef uint64_t U64 __attribute__((vector_size(K * 8)));
typedef uint16_t U16 __attribute__((vector_size(K * 2)));
typedef int16_t  S16 __attribute__((vector_size(K * 2)));

#ifdef __clang__
#define vec_sqrt(v) __builtin_elementwise_sqrt(v)
#define vec_abs(v) __builtin_elementwise_abs(v)
#define vec_round(v) __builtin_elementwise_roundeven(v)
#define vec_floor(v) __builtin_elementwise_floor(v)
#define vec_ceil(v) __builtin_elementwise_ceil(v)
#define vec_min(a, b) __builtin_elementwise_min(a, b)
#define vec_max(a, b) __builtin_elementwise_max(a, b)
#else
static F32 vec_sqrt(F32 v) {
    F32 r;
    for (int i = 0; i < K; i++) { r[i] = sqrtf(v[i]); }
    return r;
}
static F32 vec_abs(F32 v) {
    F32 r;
    for (int i = 0; i < K; i++) { r[i] = fabsf(v[i]); }
    return r;
}
static F32 vec_round(F32 v) {
    F32 r;
    for (int i = 0; i < K; i++) { r[i] = rintf(v[i]); }
    return r;
}
static F32 vec_floor(F32 v) {
    F32 r;
    for (int i = 0; i < K; i++) { r[i] = floorf(v[i]); }
    return r;
}
static F32 vec_ceil(F32 v) {
    F32 r;
    for (int i = 0; i < K; i++) { r[i] = ceilf(v[i]); }
    return r;
}
static F32 vec_min(F32 a, F32 b) {
    F32 r;
    for (int i = 0; i < K; i++) { r[i] = a[i] < b[i] ? a[i] : b[i]; }
    return r;
}
static F32 vec_max(F32 a, F32 b) {
    F32 r;
    for (int i = 0; i < K; i++) { r[i] = a[i] > b[i] ? a[i] : b[i]; }
    return r;
}
#endif

#if defined(__ARM_FEATURE_FMA) || defined(__FMA__)
static F32 vec_fma(F32 x, F32 y, F32 z) {
#ifdef __clang__
    return z + x * y;
#else
    for (int i = 0; i < K; i++) { z[i] = __builtin_fmaf(x[i], y[i], z[i]); }
    return z;
#endif
}
#endif

#if !defined(__wasm__)
typedef __fp16 F16 __attribute__((vector_size(K * 2)));
static F32 f16_to_f32(U16 h) {
    F16 tmp;
    __builtin_memcpy(&tmp, &h, sizeof h);
    return cast(F32, tmp);
}
static U16 f32_to_f16(F32 f) {
    F16 tmp = cast(F16, f);
    U16 result;
    __builtin_memcpy(&result, &tmp, sizeof tmp);
    return result;
}
#else
static F32 f16_to_f32(U16 h) {
    F32 r = {0};
    for (int i = 0; i < K; i++) {
        __fp16 tmp;
        __builtin_memcpy(&tmp, (char*)&h + 2 * i, 2);
        r[i] = (float)tmp;
    }
    return r;
}
static U16 f32_to_f16(F32 f) {
    U16 r = {0};
    for (int i = 0; i < K; i++) {
        __fp16 tmp = (__fp16)f[i];
        __builtin_memcpy((char*)&r + 2 * i, &tmp, 2);
    }
    return r;
}
#endif

typedef union {
    I32 i32;
    U32 u32;
    F32 f32;
} ival;

// Tag values: all enum op values (0..op_le_s32_imm), plus interpreter-only ops.
//
// Register variants: op_<ret>_<name>_<params> where r=register, m=memory.
// The existing op_<name> is the all-memory variant (mm->m for binary, m->m for unary).
// Example: op_r_mul_f32_rm = "result to register, x from register, y from memory"

// Generated from X-macro flags.
#define COMM_FLAG(name, flags) [op_##name] = (flags),
static uint8_t const binary_flags[] = { BINARY_OPS(COMM_FLAG) };
#undef COMM_FLAG
static _Bool is_commutative(enum op op) { return !!(binary_flags[op] & OP_COMMUTATIVE); }

enum {
    SW_DONE = op_le_s32_imm + 1,

#define BINARY_ENUM(name, ...) op_r_##name##_mm, op_r_##name##_rm, op_m_##name##_rm,
    BINARY_OPS(BINARY_ENUM)
#undef BINARY_ENUM

#define UNARY_ENUM(name, ...) op_r_##name##_r, op_m_##name##_r,
    UNARY_OPS(UNARY_ENUM)
#undef UNARY_ENUM

#define IMM_ENUM(name, ...) op_r_##name##_r, op_m_##name##_r,
    IMM_OPS(IMM_ENUM)
#undef IMM_ENUM

    op_r_imm_32, op_r_x, op_r_y,
    op_r_fma_f32_mmm, op_r_fma_f32_mmr, op_m_fma_f32_mmr,
    op_r_fms_f32_mmm, op_r_fms_f32_mmr, op_m_fms_f32_mmr,
    op_r_sel_32_mm, op_r_sel_32_rm, op_m_sel_32_rm,

    SW_NUM_OPS,
};

struct sw_inst {
    int tag;
    int x, y, z, w;
    int ptr;
};

struct interp_program {
    struct umbra_program base;
    struct sw_inst *inst;
    int             preamble, ptrs, bindings, vars, v_slots, :32;
    struct buffer_binding *binding;
};

static struct interp_program* interp_program(struct umbra_flat_ir const *ir) {
    int *id = calloc((size_t)ir->insts, sizeof *id);

    struct interp_program *p = malloc(sizeof *p);
    int slots = 1;
    for (int i = 0; i < ir->insts; i++) {
        enum op const op = ir->inst[i].op;
        if (op == op_load_16x4 || op == op_load_16x4_planar || op == op_load_8x4) {
            slots += 4;
        } else {
            slots += 1;
        }
    }
    p->inst    = malloc((size_t)slots * sizeof *p->inst);
    p->v_slots = slots;

    int max_ptr = -1;
    for (int i = 0; i < ir->insts; i++) {
        if (op_has_ptr(ir->inst[i].op) && max_ptr < ir->inst[i].ptr.bits) {
            max_ptr = ir->inst[i].ptr.bits;
        }
    }
    p->ptrs     = max_ptr + 1;
    p->bindings = ir->bindings;
    if (p->bindings) {
        size_t const sz = (size_t)p->bindings * sizeof *p->binding;
        p->binding = malloc(sz);
        __builtin_memcpy(p->binding, ir->binding, sz);
    } else {
        p->binding = NULL;
    }

    struct umbra_flat_ir *resolved = NULL;
#if !__has_feature(address_sanitizer)
    ir = resolved = flat_ir_resolve(ir, JOIN_KEEP_X);
#endif

    int n = 0;
    int loop_begin_sw_n = -1;
#define emit(...) p->inst[n] = (struct sw_inst){ __VA_ARGS__ }
#define RESOLVE_PTR(inst) ((inst)->ptr.bits)
    for (int pass = 0; pass < 2; pass++) {
        int const lo = pass ? ir->preamble : 0, hi = pass ? ir->insts : ir->preamble;
        if (pass) { p->preamble = n; }
        for (int i = lo; i < hi; i++) {
            struct ir_inst const *inst = &ir->inst[i];
            int const X = id[inst->x.id] + (int)inst->x.chan - n;
            int const Y = id[inst->y.id] + (int)inst->y.chan - n;
            int const Z = id[inst->z.id] + (int)inst->z.chan - n;
            int const W = id[inst->w.id] + (int)inst->w.chan - n;
            switch (inst->op) {
            case op_x:      emit(.tag = op_x);      break;
            case op_y:      emit(.tag = op_y);      break;
            case op_imm_32: emit(.tag = op_imm_32, .x = inst->imm); break;
            case op_join:   emit(.tag = op_join, .x = X, .y = Y); break;

            case op_uniform_32:
                emit(.tag = op_uniform_32, .ptr = RESOLVE_PTR(inst), .x = inst->imm);
                break;

            case op_load_16: emit(.tag = op_load_16, .ptr = RESOLVE_PTR(inst)); break;
            case op_load_32: emit(.tag = op_load_32, .ptr = RESOLVE_PTR(inst)); break;
            case op_load_8:  emit(.tag = op_load_8,  .ptr = RESOLVE_PTR(inst)); break;
            case op_gather_16:         emit(.tag = op_gather_16,
                                            .ptr = RESOLVE_PTR(inst), .x = X); break;
            case op_gather_uniform_32: emit(.tag = op_gather_uniform_32,
                                            .ptr = RESOLVE_PTR(inst), .x = X); break;
            case op_gather_32:         emit(.tag = op_gather_32,
                                            .ptr = RESOLVE_PTR(inst), .x = X); break;
            case op_gather_8:          emit(.tag = op_gather_8,
                                            .ptr = RESOLVE_PTR(inst), .x = X); break;

            case op_store_16: emit(.tag = op_store_16, .ptr = RESOLVE_PTR(inst), .x = Y); break;
            case op_store_32: emit(.tag = op_store_32, .ptr = RESOLVE_PTR(inst), .x = Y); break;
            case op_store_8:  emit(.tag = op_store_8,  .ptr = RESOLVE_PTR(inst), .x = Y); break;

            case op_loop_begin:
                emit(.tag = op_loop_begin, .x = X, .y = 0);
                loop_begin_sw_n = n;
                break;
            case op_loop_end:
                p->inst[loop_begin_sw_n].y = n - loop_begin_sw_n;
                emit(.tag = op_loop_end,
                     .x = loop_begin_sw_n - n,
                     .y = p->inst[loop_begin_sw_n].x + (loop_begin_sw_n - n),
                     .z = inst->imm);
                break;
            case op_if_begin:
                emit(.tag = op_if_begin, .x = X, .y = 0);
                break;
            case op_if_end:
                emit(.tag = op_if_end);
                break;
            case op_load_var:  emit(.tag = op_load_var,  .x = inst->imm); break;
            case op_store_var: emit(.tag = op_store_var, .x = Y, .y = inst->imm); break;

            case op_load_16x4:
                emit(.tag = op_load_16x4, .ptr = RESOLVE_PTR(inst));
                id[i] = n;
                n++;
                p->inst[n++] = (struct sw_inst){.tag = op_load_16x4};
                p->inst[n++] = (struct sw_inst){.tag = op_load_16x4};
                p->inst[n++] = (struct sw_inst){.tag = op_load_16x4};
                continue;
            case op_load_16x4_planar:
                emit(.tag = op_load_16x4_planar, .ptr = RESOLVE_PTR(inst));
                id[i] = n;
                n++;
                p->inst[n++] = (struct sw_inst){.tag = op_load_16x4_planar};
                p->inst[n++] = (struct sw_inst){.tag = op_load_16x4_planar};
                p->inst[n++] = (struct sw_inst){.tag = op_load_16x4_planar};
                continue;
            case op_store_16x4:
                emit(.tag = op_store_16x4, .ptr = RESOLVE_PTR(inst),
                     .x = X, .y = Y, .z = Z, .w = W);
                break;
            case op_store_16x4_planar:
                emit(.tag = op_store_16x4_planar, .ptr = RESOLVE_PTR(inst),
                     .x = X, .y = Y, .z = Z, .w = W);
                break;
            case op_load_8x4:
                emit(.tag = op_load_8x4, .ptr = RESOLVE_PTR(inst));
                id[i] = n;
                n++;
                p->inst[n++] = (struct sw_inst){.tag = op_load_8x4};
                p->inst[n++] = (struct sw_inst){.tag = op_load_8x4};
                p->inst[n++] = (struct sw_inst){.tag = op_load_8x4};
                continue;
            case op_store_8x4:
                emit(.tag = op_store_8x4, .ptr = RESOLVE_PTR(inst),
                     .x = X, .y = Y, .z = Z, .w = W);
                break;

            case op_shl_i32_imm:
            case op_shr_u32_imm:
            case op_shr_s32_imm:
            case op_and_32_imm:
            case op_or_32_imm:
            case op_xor_32_imm:
            case op_add_f32_imm:
            case op_sub_f32_imm:
            case op_mul_f32_imm:
            case op_div_f32_imm:
            case op_min_f32_imm:
            case op_max_f32_imm:
            case op_add_i32_imm:
            case op_sub_i32_imm:
            case op_mul_i32_imm:
            case op_eq_f32_imm:
            case op_lt_f32_imm:
            case op_le_f32_imm:
            case op_eq_i32_imm:
            case op_lt_s32_imm:
            case op_le_s32_imm:
                emit(.tag = inst->op, .x = X, .y = inst->imm);
                break;

            case op_add_f32:
            case op_sub_f32:
            case op_mul_f32:
            case op_div_f32:
            case op_min_f32:
            case op_max_f32:
            case op_sqrt_f32:
            case op_abs_f32:
            case op_square_f32:
            case op_round_f32:
            case op_floor_f32:
            case op_ceil_f32:
            case op_round_i32:
            case op_floor_i32:
            case op_ceil_i32:
            case op_fma_f32:
            case op_fms_f32:
            case op_square_add_f32:
            case op_square_sub_f32:
            case op_add_i32:
            case op_sub_i32:
            case op_mul_i32:
            case op_shl_i32:
            case op_shr_u32:
            case op_shr_s32:
            case op_and_32:
            case op_or_32:
            case op_xor_32:
            case op_sel_32:
            case op_f32_from_i32:
            case op_i32_from_f32:
            case op_f32_from_f16:
            case op_f16_from_f32:
            case op_i32_from_s16:
            case op_i32_from_u16:
            case op_i16_from_i32:
            case op_eq_f32:
            case op_lt_f32:
            case op_le_f32:
            case op_eq_i32:
            case op_lt_s32:
            case op_le_s32:
            case op_lt_u32:
            case op_le_u32:
                emit(.tag = inst->op, .x = X, .y = Y, .z = Z);
                break;
            }
            id[i] = n++;
        }
    }
#undef emit
#undef RESOLVE_PTR
    p->inst[n] = (struct sw_inst){.tag = SW_DONE};

    // Register variant upgrade: find values that die immediately (last_use == i+1)
    // and upgrade their op tags to register variants.
    {
        // Compute last_use[i] = last instruction that reads value i.
        int *lu = calloc((size_t)n, sizeof *lu);
        for (int i = 0; i < n; i++) {
            struct sw_inst const *s = &p->inst[i];
            if (s->x < 0) { int src = i + s->x; if (src >= 0) { lu[src] = i; } }
            if (s->y < 0) { int src = i + s->y; if (src >= 0) { lu[src] = i; } }
            if (s->z < 0) { int src = i + s->z; if (src >= 0) { lu[src] = i; } }
            if (s->w < 0) { int src = i + s->w; if (src >= 0) { lu[src] = i; } }
        }

        // Walk forward, upgrading tags.
        _Bool prev_r = 0;  // did the previous instruction output to register?
        for (int i = 0; i < n; i++) {
            struct sw_inst *s = &p->inst[i];
            if (i == p->preamble) { prev_r = 0; }
            _Bool x_r = prev_r && s->x == -1;
            _Bool y_r = prev_r && s->y == -1;
            _Bool z_r = prev_r && s->z == -1; (void)z_r;
            if (!x_r && y_r && is_commutative((enum op)s->tag)) {
                int tmp = s->x; s->x = s->y; s->y = tmp;
                x_r = 1; y_r = 0;
            }

            _Bool out_r = 0;
            if (lu[i] == i + 1 && i + 1 != p->preamble) {
                int const next_tag = p->inst[i + 1].tag;
#define CHECK_BINARY(name, ...) || (next_tag == op_##name                            \
                    && p->inst[i + 1].x != p->inst[i + 1].y                            \
                    && (p->inst[i + 1].x == -1                                          \
                     || (p->inst[i + 1].y == -1 && is_commutative(op_##name))))
#define CHECK_UNARY(name, ...)  || next_tag == op_##name
#define CHECK_IMM(name, ...)    || next_tag == op_##name
                out_r = 0 BINARY_OPS(CHECK_BINARY) UNARY_OPS(CHECK_UNARY) IMM_OPS(CHECK_IMM)
                        || (next_tag == op_sel_32 && p->inst[i + 1].x == -1)
#if defined(__ARM_FEATURE_FMA) || defined(__FMA__)
                        || ((next_tag == op_fma_f32 || next_tag == op_fms_f32)
                            && p->inst[i + 1].z == -1)
#endif
                        ;
#undef CHECK_BINARY
#undef CHECK_UNARY
#undef CHECK_IMM
            }

            int const tag = s->tag;

            static struct { int out_r_in_r, out_m_in_r, out_r; } const upgrade[] = {
#define BV(name,...) [op_##name] = {op_r_##name##_rm, op_m_##name##_rm, op_r_##name##_mm},
                BINARY_OPS(BV)
#undef BV
                [op_sel_32] = {op_r_sel_32_rm, op_m_sel_32_rm, op_r_sel_32_mm},
#define UV(name,...) [op_##name] = {op_r_##name##_r, op_m_##name##_r, 0},
                UNARY_OPS(UV)
                IMM_OPS(UV)
#undef UV
            };

            if (tag < count(upgrade) && upgrade[tag].out_r_in_r) {
                if      ( out_r && x_r)                  { s->tag = upgrade[tag].out_r_in_r; }
                else if (!out_r && x_r)                  { s->tag = upgrade[tag].out_m_in_r; }
                else if ( out_r && upgrade[tag].out_r)    { s->tag = upgrade[tag].out_r; }
            }
#if defined(__ARM_FEATURE_FMA) || defined(__FMA__)
            else if (tag == op_fma_f32) {
                if      ( out_r &&  z_r) { s->tag = op_r_fma_f32_mmr; }
                else if (!out_r &&  z_r) { s->tag = op_m_fma_f32_mmr; }
                else if ( out_r && !z_r) { s->tag = op_r_fma_f32_mmm; }
            } else if (tag == op_fms_f32) {
                if      ( out_r &&  z_r) { s->tag = op_r_fms_f32_mmr; }
                else if (!out_r &&  z_r) { s->tag = op_m_fms_f32_mmr; }
                else if ( out_r && !z_r) { s->tag = op_r_fms_f32_mmm; }
            }
#endif
            else if (out_r && (tag == op_imm_32 || tag == op_x || tag == op_y)) {
                     if (tag == op_imm_32) { s->tag = op_r_imm_32; }
                else if (tag == op_x)      { s->tag = op_r_x; }
                else if (tag == op_y)      { s->tag = op_r_y; }
            }

            prev_r = (s->tag != tag) ? out_r : 0;
        }
        free(lu);
    }

    p->vars = ir->vars;

    free(id);
    umbra_flat_ir_free(resolved);

    return p;
}

static void interp_program_run(struct interp_program *p, int l, int t, int r, int b,
                               struct umbra_late_binding const *late, int lates) {
    assume(p->ptrs <= 64);
    struct umbra_buf buf[64];
    resolve_bindings(buf, p->binding, p->bindings, late, lates);

    ival *const scratch = malloc(((size_t)p->v_slots + (size_t)p->vars) * sizeof *scratch);
    ival *const v_base = scratch;
    ival *const var    = scratch + p->v_slots;

    int const      P   = p->preamble;
    I32                   if_mask_stack[8];
    int                   if_depth = 0;

    for (int row = t; row < b; row++) {
        for (int col = l; col < r; col += K) {
            int const              end = col + K;
            int const              n   = r;
            struct sw_inst const  *ip  = p->inst + (col == l ? 0 : P);
            ival                  *v   = v_base  + (col == l ? 0 : P);

            if_depth = 0;

            ival acc = {0};
#define F32_IMM union { int i; float f; } const u = {.i = ip->y}; F32 const imm = (F32){0} + u.f
            // Computed goto on native, switch on WASM.
#if defined(__GNUC__) && !defined(__wasm__)
    #define DISPATCH    goto *labels[ip->tag]
    #define CASE(label) L_##label:
    // The asm volatile prevents the compiler from tail-merging dispatch sites.
    // Each NEXT must be its own indirect branch for branch predictor accuracy.
    #define NEXT        ip++; v++; __asm__ volatile(""); DISPATCH
    #define DONE        goto next_tile
            static void *const labels[SW_NUM_OPS] = {
                [op_x] = &&L_op_x, [op_y] = &&L_op_y, [op_imm_32] = &&L_op_imm_32,
                [op_uniform_32] = &&L_op_uniform_32,
                [op_load_16] = &&L_op_load_16, [op_load_32] = &&L_op_load_32,
                [op_store_16] = &&L_op_store_16, [op_store_32] = &&L_op_store_32,
                [op_load_16x4] = &&L_op_load_16x4,
                [op_load_16x4_planar] = &&L_op_load_16x4_planar,
                [op_store_16x4] = &&L_op_store_16x4,
                [op_store_16x4_planar] = &&L_op_store_16x4_planar,
                [op_load_8x4] = &&L_op_load_8x4, [op_store_8x4] = &&L_op_store_8x4,
                [op_load_8]   = &&L_op_load_8,   [op_store_8]   = &&L_op_store_8,
                [op_gather_uniform_32] = &&L_op_gather_uniform_32,
                [op_gather_16] = &&L_op_gather_16, [op_gather_32] = &&L_op_gather_32,
                [op_gather_8]  = &&L_op_gather_8,
                [op_join] = &&L_op_join,
                [op_loop_begin] = &&L_op_loop_begin, [op_loop_end] = &&L_op_loop_end,
                [op_if_begin] = &&L_op_if_begin, [op_if_end] = &&L_op_if_end,
                [op_load_var] = &&L_op_load_var, [op_store_var] = &&L_op_store_var,
                [op_f32_from_f16] = &&L_op_f32_from_f16, [op_f16_from_f32] = &&L_op_f16_from_f32,
                [op_i32_from_s16] = &&L_op_i32_from_s16,
                [op_i32_from_u16] = &&L_op_i32_from_u16,
                [op_i16_from_i32] = &&L_op_i16_from_i32,
                [op_f32_from_i32] = &&L_op_f32_from_i32, [op_i32_from_f32] = &&L_op_i32_from_f32,
                [op_add_f32] = &&L_op_add_f32, [op_sub_f32] = &&L_op_sub_f32,
                [op_mul_f32] = &&L_op_mul_f32, [op_div_f32] = &&L_op_div_f32,
                [op_min_f32] = &&L_op_min_f32, [op_max_f32] = &&L_op_max_f32,
                [op_sqrt_f32] = &&L_op_sqrt_f32, [op_abs_f32] = &&L_op_abs_f32,
                [op_square_f32] = &&L_op_square_f32,

                [op_round_f32] = &&L_op_round_f32, [op_floor_f32] = &&L_op_floor_f32,
                [op_ceil_f32] = &&L_op_ceil_f32,
                [op_round_i32] = &&L_op_round_i32, [op_floor_i32] = &&L_op_floor_i32,
                [op_ceil_i32] = &&L_op_ceil_i32,
                [op_fma_f32] = &&L_op_fma_f32, [op_fms_f32] = &&L_op_fms_f32,
                [op_square_add_f32] = &&L_op_square_add_f32,
                [op_square_sub_f32] = &&L_op_square_sub_f32,
                [op_add_i32] = &&L_op_add_i32, [op_sub_i32] = &&L_op_sub_i32,
                [op_mul_i32] = &&L_op_mul_i32,
                [op_shl_i32] = &&L_op_shl_i32, [op_shr_u32] = &&L_op_shr_u32,
                [op_shr_s32] = &&L_op_shr_s32,
                [op_and_32] = &&L_op_and_32, [op_or_32] = &&L_op_or_32, [op_xor_32] = &&L_op_xor_32,
                [op_sel_32] = &&L_op_sel_32,
                [op_eq_f32] = &&L_op_eq_f32, [op_lt_f32] = &&L_op_lt_f32,
                [op_le_f32] = &&L_op_le_f32,
                [op_eq_i32] = &&L_op_eq_i32, [op_lt_s32] = &&L_op_lt_s32,
                [op_le_s32] = &&L_op_le_s32,
                [op_lt_u32] = &&L_op_lt_u32, [op_le_u32] = &&L_op_le_u32,
                [op_shl_i32_imm] = &&L_op_shl_i32_imm, [op_shr_u32_imm] = &&L_op_shr_u32_imm,
                [op_shr_s32_imm] = &&L_op_shr_s32_imm,
                [op_and_32_imm] = &&L_op_and_32_imm, [op_or_32_imm] = &&L_op_or_32_imm,
                [op_xor_32_imm] = &&L_op_xor_32_imm,
                [op_add_f32_imm] = &&L_op_add_f32_imm, [op_sub_f32_imm] = &&L_op_sub_f32_imm,
                [op_mul_f32_imm] = &&L_op_mul_f32_imm, [op_div_f32_imm] = &&L_op_div_f32_imm,
                [op_min_f32_imm] = &&L_op_min_f32_imm, [op_max_f32_imm] = &&L_op_max_f32_imm,
                [op_eq_f32_imm] = &&L_op_eq_f32_imm, [op_lt_f32_imm] = &&L_op_lt_f32_imm,
                [op_le_f32_imm] = &&L_op_le_f32_imm,
                [op_add_i32_imm] = &&L_op_add_i32_imm, [op_sub_i32_imm] = &&L_op_sub_i32_imm,
                [op_mul_i32_imm] = &&L_op_mul_i32_imm,
                [op_eq_i32_imm] = &&L_op_eq_i32_imm, [op_lt_s32_imm] = &&L_op_lt_s32_imm,
                [op_le_s32_imm] = &&L_op_le_s32_imm,
                [SW_DONE] = &&L_SW_DONE,

#define BINARY_LABELS(name, ...) \
                [op_r_##name##_mm] = &&L_op_r_##name##_mm, \
                [op_r_##name##_rm] = &&L_op_r_##name##_rm, \
                [op_m_##name##_rm] = &&L_op_m_##name##_rm,
                BINARY_OPS(BINARY_LABELS)
#undef BINARY_LABELS
#define UNARY_LABELS(name, ...) \
                [op_r_##name##_r] = &&L_op_r_##name##_r, \
                [op_m_##name##_r] = &&L_op_m_##name##_r,
                UNARY_OPS(UNARY_LABELS)
#undef UNARY_LABELS
#define IMM_LABELS(name, ...) \
                [op_r_##name##_r] = &&L_op_r_##name##_r, \
                [op_m_##name##_r] = &&L_op_m_##name##_r,
                IMM_OPS(IMM_LABELS)
#undef IMM_LABELS

                [op_r_imm_32] = &&L_op_r_imm_32,
                [op_r_x] = &&L_op_r_x,
                [op_r_y] = &&L_op_r_y,
                [op_r_sel_32_mm] = &&L_op_r_sel_32_mm,
                [op_r_sel_32_rm] = &&L_op_r_sel_32_rm,
                [op_m_sel_32_rm] = &&L_op_m_sel_32_rm,
                [op_r_fma_f32_mmm] = &&L_op_r_fma_f32_mmm,
                [op_r_fma_f32_mmr] = &&L_op_r_fma_f32_mmr,
                [op_m_fma_f32_mmr] = &&L_op_m_fma_f32_mmr,
                [op_r_fms_f32_mmm] = &&L_op_r_fms_f32_mmm,
                [op_r_fms_f32_mmr] = &&L_op_r_fms_f32_mmr,
                [op_m_fms_f32_mmr] = &&L_op_m_fms_f32_mmr,
            };
            DISPATCH;
#else
    #define CASE(label) case label:
    #define NEXT        break
    #define DONE        goto next_tile
            for (;;) { switch (ip->tag) {
#endif
                CASE(op_imm_32) v->i32 = (I32){0} + ip->x; NEXT;
                CASE(op_join) {
                    I32 ja, jb;
                    __builtin_memcpy(&ja, &v[ip->x].i32, sizeof ja);
                    __builtin_memcpy(&jb, &v[ip->y].i32, sizeof jb);
                    for (int jk = 0; jk < K; jk++) {
                        assume(ja[jk] == jb[jk]);
                    }

                    v->i32 = v[ip->x].i32;
                } NEXT;
                CASE(op_x) {
                    I32 seq;
                    __builtin_memcpy(&seq, iota, sizeof seq);
                    v->i32 = seq + (end - K);
                } NEXT;
                CASE(op_y) v->i32 = (I32){0} + row; NEXT;

                CASE(op_uniform_32) {
                    assume(buf[ip->ptr].stride == 0);
                    int32_t uni = ((int32_t const*)buf[ip->ptr].ptr)[ip->x];
                    v->i32 = (I32){0} + uni;
                } NEXT;

                CASE(op_load_16) {
                    uint16_t const *src =
                        (uint16_t const*)buf[ip->ptr].ptr + row * buf[ip->ptr].stride;
                    int const       i = end - K;
                    int const       rem = n - i;
                    if (rem >= K) {
                        U16 tmp;
                        __builtin_memcpy(&tmp, src + i, 2 * K);
                        v->u32 = (U32){0};
                        __builtin_memcpy(v, &tmp, sizeof tmp);
                    } else {
                        v->u32 = (U32){0};
                        for (int ll = 0; ll < rem; ll++) {
                            uint16_t s;
                            __builtin_memcpy(&s, src + i + ll, 2);
                            __builtin_memcpy((char*)v + 2 * ll, &s, 2);
                        }
                    }
                } NEXT;
                CASE(op_load_32) {
                    int32_t const *src =
                        (int32_t const*)buf[ip->ptr].ptr + row * buf[ip->ptr].stride;
                    int const      i = end - K;
                    int const      rem = n - i;
                    if (rem >= K) {
                        __builtin_memcpy(v, src + i, 4 * K);
                    } else {
                        int32_t tmp[K];
                        __builtin_memset(tmp, 0, sizeof tmp);
                        for (int ll = 0; ll < rem; ll++) {
                            __builtin_memcpy(&tmp[ll], src + i + ll, 4);
                        }
                        __builtin_memcpy(&v->i32, tmp, sizeof v->i32);
                    }
                } NEXT;
                CASE(op_store_16) {
                    uint16_t *dst = (uint16_t*)buf[ip->ptr].ptr + row * buf[ip->ptr].stride;
                    int const i = end - K;
                    int const rem = n - i;
                    if (rem >= K) {
                        __builtin_memcpy(dst + i, &v[ip->x], 2 * K);
                    } else {
                        for (int ll = 0; ll < rem; ll++) {
                            uint16_t s;
                            __builtin_memcpy(&s, (char*)&v[ip->x] + 2 * ll, 2);
                            __builtin_memcpy(dst + i + ll, &s, 2);
                        }
                    }
                } NEXT;
                CASE(op_store_32) {
                    int32_t *dst = (int32_t*)buf[ip->ptr].ptr + row * buf[ip->ptr].stride;
                    int const i = end - K;
                    int const rem = n - i;
                    if (rem >= K) {
                        __builtin_memcpy(dst + i, v + ip->x, 4 * K);
                    } else {
                        for (int ll = 0; ll < rem; ll++) {
                            int32_t tmp;
                            __builtin_memcpy(&tmp, (char*)&v[ip->x].i32 + 4 * ll, 4);
                            __builtin_memcpy(dst + i + ll, &tmp, 4);
                        }
                    }
                } NEXT;

                CASE(op_load_16x4) {
                    uint64_t const *src =
                        (uint64_t const*)buf[ip->ptr].ptr + row * buf[ip->ptr].stride;
                    int const i = end - K;
                    int const rem = n - i;
                    U16 hr, hg, hb, ha;
                    if (rem >= K) {
                        U64 px;
                        __builtin_memcpy(&px, src + i, sizeof px);
                        U64 const mask16 = (U64){0} + 0xFFFFULL;
                        hr = cast(U16,  px        & mask16);
                        hg = cast(U16, (px >> 16) & mask16);
                        hb = cast(U16, (px >> 32) & mask16);
                        ha = cast(U16,  px >> 48);
                    } else {
                        uint16_t tr[K], tg[K], tb[K], ta[K];
                        __builtin_memset(tr, 0, sizeof tr);
                        __builtin_memset(tg, 0, sizeof tg);
                        __builtin_memset(tb, 0, sizeof tb);
                        __builtin_memset(ta, 0, sizeof ta);
                        for (int ll = 0; ll < rem; ll++) {
                            uint16_t h[4];
                            __builtin_memcpy(h, src + i + ll, 8);
                            tr[ll] = h[0]; tg[ll] = h[1]; tb[ll] = h[2]; ta[ll] = h[3];
                        }
                        __builtin_memcpy(&hr, tr, sizeof hr);
                        __builtin_memcpy(&hg, tg, sizeof hg);
                        __builtin_memcpy(&hb, tb, sizeof hb);
                        __builtin_memcpy(&ha, ta, sizeof ha);
                    }
                    v[0].u32 = (U32){0}; __builtin_memcpy(&v[0], &hr, sizeof hr);
                    v[1].u32 = (U32){0}; __builtin_memcpy(&v[1], &hg, sizeof hg);
                    v[2].u32 = (U32){0}; __builtin_memcpy(&v[2], &hb, sizeof hb);
                    v[3].u32 = (U32){0}; __builtin_memcpy(&v[3], &ha, sizeof ha);
                    ip += 3; v += 3;
                } NEXT;
                CASE(op_load_16x4_planar) {
                    uint16_t const *src =
                        (uint16_t const*)buf[ip->ptr].ptr + row * buf[ip->ptr].stride;
                    int const ps = buf[ip->ptr].count / 4;
                    int const i = end - K;
                    int const rem = n - i;
                    U16 hr = {0}, hg = {0}, hb = {0}, ha = {0};
                    if (rem >= K) {
                        __builtin_memcpy(&hr, src           + i, sizeof hr);
                        __builtin_memcpy(&hg, src + ps      + i, sizeof hg);
                        __builtin_memcpy(&hb, src + ps * 2  + i, sizeof hb);
                        __builtin_memcpy(&ha, src + ps * 3  + i, sizeof ha);
                    } else {
                        uint16_t tr[K], tg[K], tb[K], ta[K];
                        __builtin_memset(tr, 0, sizeof tr);
                        __builtin_memset(tg, 0, sizeof tg);
                        __builtin_memset(tb, 0, sizeof tb);
                        __builtin_memset(ta, 0, sizeof ta);
                        for (int ll = 0; ll < rem; ll++) {
                            uint16_t tmp;
                            __builtin_memcpy(&tmp, src           + i + ll, 2); tr[ll] = tmp;
                            __builtin_memcpy(&tmp, src + ps      + i + ll, 2); tg[ll] = tmp;
                            __builtin_memcpy(&tmp, src + ps * 2  + i + ll, 2); tb[ll] = tmp;
                            __builtin_memcpy(&tmp, src + ps * 3  + i + ll, 2); ta[ll] = tmp;
                        }
                        __builtin_memcpy(&hr, tr, sizeof hr);
                        __builtin_memcpy(&hg, tg, sizeof hg);
                        __builtin_memcpy(&hb, tb, sizeof hb);
                        __builtin_memcpy(&ha, ta, sizeof ha);
                    }
                    v[0].u32 = (U32){0}; __builtin_memcpy(&v[0], &hr, sizeof hr);
                    v[1].u32 = (U32){0}; __builtin_memcpy(&v[1], &hg, sizeof hg);
                    v[2].u32 = (U32){0}; __builtin_memcpy(&v[2], &hb, sizeof hb);
                    v[3].u32 = (U32){0}; __builtin_memcpy(&v[3], &ha, sizeof ha);
                    ip += 3; v += 3;
                } NEXT;
                CASE(op_store_16x4) {
                    uint64_t *dst = (uint64_t*)buf[ip->ptr].ptr + row * buf[ip->ptr].stride;
                    int const i = end - K;
                    int const rem = n - i;
                    U16 hr, hg, hb, ha;
                    __builtin_memcpy(&hr, &v[ip->x], sizeof hr);
                    __builtin_memcpy(&hg, &v[ip->y], sizeof hg);
                    __builtin_memcpy(&hb, &v[ip->z], sizeof hb);
                    __builtin_memcpy(&ha, &v[ip->w], sizeof ha);
                    if (rem >= K) {
                        U64 const px = cast(U64, hr)
                                     | cast(U64, hg) << 16
                                     | cast(U64, hb) << 32
                                     | cast(U64, ha) << 48;
                        __builtin_memcpy(dst + i, &px, sizeof px);
                    } else {
                        uint16_t tr[K], tg[K], tb[K], ta[K];
                        __builtin_memcpy(tr, &hr, sizeof tr);
                        __builtin_memcpy(tg, &hg, sizeof tg);
                        __builtin_memcpy(tb, &hb, sizeof tb);
                        __builtin_memcpy(ta, &ha, sizeof ta);
                        for (int ll = 0; ll < rem; ll++) {
                            uint16_t h[4] = {tr[ll], tg[ll], tb[ll], ta[ll]};
                            __builtin_memcpy(dst + i + ll, h, 8);
                        }
                    }
                } NEXT;
                CASE(op_store_16x4_planar) {
                    uint16_t *dst = (uint16_t*)buf[ip->ptr].ptr + row * buf[ip->ptr].stride;
                    int const ps = buf[ip->ptr].count / 4;
                    int const i = end - K;
                    int const rem = n - i;
                    U16 hr, hg, hb, ha;
                    __builtin_memcpy(&hr, &v[ip->x], sizeof hr);
                    __builtin_memcpy(&hg, &v[ip->y], sizeof hg);
                    __builtin_memcpy(&hb, &v[ip->z], sizeof hb);
                    __builtin_memcpy(&ha, &v[ip->w], sizeof ha);
                    if (rem >= K) {
                        __builtin_memcpy(dst           + i, &hr, sizeof hr);
                        __builtin_memcpy(dst + ps      + i, &hg, sizeof hg);
                        __builtin_memcpy(dst + ps * 2  + i, &hb, sizeof hb);
                        __builtin_memcpy(dst + ps * 3  + i, &ha, sizeof ha);
                    } else {
                        uint16_t tr[K], tg[K], tb[K], ta[K];
                        __builtin_memcpy(tr, &hr, sizeof tr);
                        __builtin_memcpy(tg, &hg, sizeof tg);
                        __builtin_memcpy(tb, &hb, sizeof tb);
                        __builtin_memcpy(ta, &ha, sizeof ta);
                        for (int ll = 0; ll < rem; ll++) {
                            __builtin_memcpy(dst           + i + ll, &tr[ll], 2);
                            __builtin_memcpy(dst + ps      + i + ll, &tg[ll], 2);
                            __builtin_memcpy(dst + ps * 2  + i + ll, &tb[ll], 2);
                            __builtin_memcpy(dst + ps * 3  + i + ll, &ta[ll], 2);
                        }
                    }
                } NEXT;

                CASE(op_load_8x4) {
                    uint32_t const *src =
                        (uint32_t const*)buf[ip->ptr].ptr + row * buf[ip->ptr].stride;
                    int const i = end - K;
                    int const rem = n - i;
                    U32 const mask8 = (U32){0} + 0xFFu;
                    if (rem >= K) {
                        U32 px;
                        __builtin_memcpy(&px, src + i, sizeof px);
                        v[0].u32 =  px        & mask8;
                        v[1].u32 = (px >>  8) & mask8;
                        v[2].u32 = (px >> 16) & mask8;
                        v[3].u32 =  px >> 24;
                    } else {
                        uint32_t tr[K], tg[K], tb[K], ta[K];
                        __builtin_memset(tr, 0, sizeof tr);
                        __builtin_memset(tg, 0, sizeof tg);
                        __builtin_memset(tb, 0, sizeof tb);
                        __builtin_memset(ta, 0, sizeof ta);
                        for (int ll = 0; ll < rem; ll++) {
                            uint32_t px;
                            __builtin_memcpy(&px, src + i + ll, 4);
                            tr[ll] = px & 0xFF; tg[ll] = (px>>8) & 0xFF;
                            tb[ll] = (px>>16) & 0xFF; ta[ll] = px>>24;
                        }
                        __builtin_memcpy(&v[0].u32, tr, sizeof v[0].u32);
                        __builtin_memcpy(&v[1].u32, tg, sizeof v[1].u32);
                        __builtin_memcpy(&v[2].u32, tb, sizeof v[2].u32);
                        __builtin_memcpy(&v[3].u32, ta, sizeof v[3].u32);
                    }
                    ip += 3; v += 3;
                } NEXT;
                CASE(op_store_8x4) {
                    uint32_t *dst = (uint32_t*)buf[ip->ptr].ptr + row * buf[ip->ptr].stride;
                    int const i = end - K;
                    int const rem = n - i;
                    U32 const mask8 = (U32){0} + 0xFFu;
                    U32 const px = (v[ip->x].u32 & mask8)
                                 | ((v[ip->y].u32 & mask8) <<  8)
                                 | ((v[ip->z].u32 & mask8) << 16)
                                 | ((v[ip->w].u32 & mask8) << 24);
                    if (rem >= K) {
                        __builtin_memcpy(dst + i, &px, sizeof px);
                    } else {
                        uint32_t tmp[K];
                        __builtin_memcpy(tmp, &px, sizeof tmp);
                        for (int ll = 0; ll < rem; ll++) {
                            __builtin_memcpy(dst + i + ll, &tmp[ll], 4);
                        }
                    }
                } NEXT;
                CASE(op_load_8) {
                    uint8_t const *src =
                        (uint8_t const*)buf[ip->ptr].ptr + row * buf[ip->ptr].stride;
                    int const i = end - K;
                    int const rem = n - i;
                    uint32_t tmp[K];
                    __builtin_memset(tmp, 0, sizeof tmp);
                    int const lim = rem < K ? rem : K;
                    for (int ll = 0; ll < lim; ll++) { tmp[ll] = src[i + ll]; }
                    __builtin_memcpy(&v->u32, tmp, sizeof tmp);
                } NEXT;
                CASE(op_store_8) {
                    uint8_t *dst = (uint8_t*)buf[ip->ptr].ptr + row * buf[ip->ptr].stride;
                    int const i = end - K;
                    int const rem = n - i;
                    uint32_t tmp[K];
                    __builtin_memcpy(tmp, &v[ip->x].u32, sizeof tmp);
                    int const lim = rem < K ? rem : K;
                    for (int ll = 0; ll < lim; ll++) { dst[i + ll] = (uint8_t)tmp[ll]; }
                } NEXT;

                CASE(op_gather_uniform_32) {
                    int const ix = v[ip->x].i32[0];
                    int const count = buf[ip->ptr].count;
                    if (ix < 0 || ix >= count) { v->i32 = (I32){0}; break; }
                    int32_t gval;
                    int32_t const *ptr = (int32_t const*)buf[ip->ptr].ptr;
                    __builtin_memcpy(&gval, ptr + ix, 4);
                    v->i32 = (I32){0} + gval;
                } NEXT;
                CASE(op_gather_16) {
                    int const count = buf[ip->ptr].count;
                    int const rem = n - (end - K);
                    uint16_t const *ptr = (uint16_t const*)buf[ip->ptr].ptr;
                    int32_t ix[K];
                    __builtin_memcpy(ix, &v[ip->x].i32, sizeof ix);
                    uint16_t tmp[K];
                    __builtin_memset(tmp, 0, sizeof tmp);
                    int const lim = rem < K ? rem : K;
                    for (int ll = 0; ll < lim; ll++) {
                        if (ix[ll] >= 0 && ix[ll] < count) {
                            __builtin_memcpy(&tmp[ll], ptr + ix[ll], 2);
                        }
                    }
                    v->u32 = (U32){0};
                    __builtin_memcpy(v, tmp, sizeof tmp);
                } NEXT;
                CASE(op_gather_8) {
                    int const count = buf[ip->ptr].count;
                    int const rem = n - (end - K);
                    uint8_t const *ptr = (uint8_t const*)buf[ip->ptr].ptr;
                    int32_t ix[K];
                    __builtin_memcpy(ix, &v[ip->x].i32, sizeof ix);
                    uint32_t tmp[K];
                    __builtin_memset(tmp, 0, sizeof tmp);
                    int const lim = rem < K ? rem : K;
                    for (int ll = 0; ll < lim; ll++) {
                        if (ix[ll] >= 0 && ix[ll] < count) {
                            tmp[ll] = ptr[ix[ll]];
                        }
                    }
                    __builtin_memcpy(&v->u32, tmp, sizeof tmp);
                } NEXT;
                CASE(op_gather_32) {
                    int const count = buf[ip->ptr].count;
                    int const rem = n - (end - K);
                    int32_t const *ptr = (int32_t const*)buf[ip->ptr].ptr;
                    int32_t ix[K];
                    __builtin_memcpy(ix, &v[ip->x].i32, sizeof ix);
                    int32_t tmp[K];
                    __builtin_memset(tmp, 0, sizeof tmp);
                    int const lim = rem < K ? rem : K;
                    for (int ll = 0; ll < lim; ll++) {
                        if (ix[ll] >= 0 && ix[ll] < count) {
                            __builtin_memcpy(&tmp[ll], ptr + ix[ll], 4);
                        }
                    }
                    __builtin_memcpy(&v->i32, tmp, sizeof v->i32);
                } NEXT;
                CASE(op_loop_begin) {
                    int const trips = v[ip->x].i32[0];
                    if (trips <= 0) {
                        int const skip = ip->y;
                        ip += skip;
                        v  += skip;
                    }
                } NEXT;
                CASE(op_loop_end) {
                    int const back   = ip->x;
                    int const trips = v[ip->y].i32[0];
                    int const i_next = var[ip->z].i32[0];
                    if (i_next < trips) {
                        ip += back;
                        v  += back;
                    }
                } NEXT;
                CASE(op_if_begin) {
                    I32 cond;
                    __builtin_memcpy(&cond, &v[ip->x].i32, sizeof cond);
                    if (if_depth > 0) { cond &= if_mask_stack[if_depth - 1]; }
                    if_mask_stack[if_depth++] = cond;
                } NEXT;
                CASE(op_if_end) {
                    if_depth--;
                } NEXT;
                CASE(op_load_var)  v->i32 = var[ip->x].i32; NEXT;
                CASE(op_store_var) {
                    if (if_depth > 0) {
                        I32 const mask = if_mask_stack[if_depth - 1];
                        var[ip->y].i32 = (v[ip->x].i32 & mask)
                                       | (var[ip->y].i32 & ~mask);
                    } else {
                        var[ip->y] = v[ip->x];
                    }
                } NEXT;

                CASE(op_f32_from_f16) {
                    U16 h; __builtin_memcpy(&h, &v[ip->x], sizeof h);
                    v->f32 = f16_to_f32(h);
                } NEXT;
                CASE(op_f16_from_f32) {
                    U16 const h = f32_to_f16(v[ip->x].f32);
                    v->u32 = (U32){0}; __builtin_memcpy(v, &h, sizeof h);
                } NEXT;
                CASE(op_i32_from_s16) {
                    U16 tmp; __builtin_memcpy(&tmp, &v[ip->x], sizeof tmp);
                    S16 stmp; __builtin_memcpy(&stmp, &tmp, sizeof tmp);
                    v->i32 = cast(I32, stmp);
                } NEXT;
                CASE(op_i32_from_u16) {
                    U16 tmp; __builtin_memcpy(&tmp, &v[ip->x], sizeof tmp);
                    v->u32 = cast(U32, tmp);
                } NEXT;
                CASE(op_i16_from_i32) {
                    U16 tmp = cast(U16, v[ip->x].u32);
                    v->u32 = (U32){0};
                    __builtin_memcpy(v, &tmp, sizeof tmp);
                } NEXT;

                CASE(op_f32_from_i32) v->f32 = cast(F32, v[ip->x].i32); NEXT;
                CASE(op_i32_from_f32) v->i32 = cast(I32, v[ip->x].f32); NEXT;

                CASE(op_add_f32) v->f32 = v[ip->x].f32 + v[ip->y].f32; NEXT;
                CASE(op_sub_f32) v->f32 = v[ip->x].f32 - v[ip->y].f32; NEXT;
                CASE(op_mul_f32) v->f32 = v[ip->x].f32 * v[ip->y].f32; NEXT;
                CASE(op_div_f32) v->f32 = v[ip->x].f32 / v[ip->y].f32; NEXT;
                CASE(op_min_f32) v->f32 = vec_min(v[ip->x].f32, v[ip->y].f32); NEXT;
                CASE(op_max_f32) v->f32 = vec_max(v[ip->x].f32, v[ip->y].f32); NEXT;
                CASE(op_sqrt_f32)   v->f32 = vec_sqrt(v[ip->x].f32); NEXT;
                CASE(op_abs_f32)    v->f32 = vec_abs(v[ip->x].f32); NEXT;
                CASE(op_square_f32) v->f32 = v[ip->x].f32 * v[ip->x].f32; NEXT;
                CASE(op_round_f32) v->f32 = vec_round(v[ip->x].f32); NEXT;
                CASE(op_floor_f32) v->f32 = vec_floor(v[ip->x].f32); NEXT;
                CASE(op_ceil_f32)  v->f32 = vec_ceil(v[ip->x].f32);  NEXT;
                CASE(op_round_i32) v->i32 = cast(I32, vec_round(v[ip->x].f32)); NEXT;
                CASE(op_floor_i32) v->i32 = cast(I32, vec_floor(v[ip->x].f32)); NEXT;
                CASE(op_ceil_i32)  v->i32 = cast(I32, vec_ceil(v[ip->x].f32));  NEXT;

#if defined(__ARM_FEATURE_FMA) || defined(__FMA__)
                CASE(op_fma_f32) v->f32 = vec_fma(v[ip->x].f32, v[ip->y].f32, v[ip->z].f32); NEXT;
                CASE(op_fms_f32) v->f32 = vec_fma(-v[ip->x].f32, v[ip->y].f32, v[ip->z].f32); NEXT;
                CASE(op_square_add_f32) v->f32 = vec_fma(v[ip->x].f32, v[ip->x].f32,
                                                         v[ip->y].f32); NEXT;
                CASE(op_square_sub_f32) v->f32 = vec_fma(-v[ip->x].f32, v[ip->x].f32,
                                                         v[ip->y].f32); NEXT;
#else
                CASE(op_fma_f32) {
                    F64 const x = cast(F64, v[ip->x].f32),
                              y = cast(F64, v[ip->y].f32),
                              z = cast(F64, v[ip->z].f32);
                    v->f32 = cast(F32, x * y + z);
                } NEXT;
                CASE(op_fms_f32) {
                    F64 const x = cast(F64, v[ip->x].f32),
                              y = cast(F64, v[ip->y].f32),
                              z = cast(F64, v[ip->z].f32);
                    v->f32 = cast(F32, z - x * y);
                } NEXT;
                CASE(op_square_add_f32) {
                    F64 const x = cast(F64, v[ip->x].f32), y = cast(F64, v[ip->y].f32);
                    v->f32 = cast(F32, x * x + y);
                } NEXT;
                CASE(op_square_sub_f32) {
                    F64 const x = cast(F64, v[ip->x].f32), y = cast(F64, v[ip->y].f32);
                    v->f32 = cast(F32, y - x * x);
                } NEXT;
#endif

                CASE(op_add_i32) v->i32 = v[ip->x].i32 + v[ip->y].i32; NEXT;
                CASE(op_sub_i32) v->i32 = v[ip->x].i32 - v[ip->y].i32; NEXT;
                CASE(op_mul_i32) v->i32 = v[ip->x].i32 * v[ip->y].i32; NEXT;
                CASE(op_shl_i32) v->i32 = v[ip->x].i32 << v[ip->y].i32; NEXT;
                CASE(op_shr_u32) v->u32 = v[ip->x].u32 >> v[ip->y].u32; NEXT;
                CASE(op_shr_s32) v->i32 = v[ip->x].i32 >> v[ip->y].i32; NEXT;
                CASE(op_and_32)  v->i32 = v[ip->x].i32 & v[ip->y].i32;  NEXT;
                CASE(op_or_32)   v->i32 = v[ip->x].i32 | v[ip->y].i32;  NEXT;
                CASE(op_xor_32)  v->i32 = v[ip->x].i32 ^ v[ip->y].i32;  NEXT;
                CASE(op_sel_32)  v->i32 = (v[ip->x].i32 & v[ip->y].i32)
                                        | (~v[ip->x].i32 & v[ip->z].i32); NEXT;

                CASE(op_eq_f32) v->i32 = (I32)(v[ip->x].f32 == v[ip->y].f32); NEXT;
                CASE(op_lt_f32) v->i32 = (I32)(v[ip->x].f32 <  v[ip->y].f32); NEXT;
                CASE(op_le_f32) v->i32 = (I32)(v[ip->x].f32 <= v[ip->y].f32); NEXT;
                CASE(op_eq_i32) v->i32 = (I32)(v[ip->x].i32 == v[ip->y].i32); NEXT;
                CASE(op_lt_s32) v->i32 = (I32)(v[ip->x].i32 <  v[ip->y].i32); NEXT;
                CASE(op_le_s32) v->i32 = (I32)(v[ip->x].i32 <= v[ip->y].i32); NEXT;
                CASE(op_lt_u32) v->i32 = (I32)(v[ip->x].u32 <  v[ip->y].u32); NEXT;
                CASE(op_le_u32) v->i32 = (I32)(v[ip->x].u32 <= v[ip->y].u32); NEXT;

                CASE(op_shl_i32_imm) {
                    I32 const sh = (I32){0} + ip->y; v->i32 = v[ip->x].i32 << sh;
                } NEXT;
                CASE(op_shr_u32_imm) {
                    U32 const sh = (U32){0} + (uint32_t)ip->y; v->u32 = v[ip->x].u32 >> sh;
                } NEXT;
                CASE(op_shr_s32_imm) {
                    I32 const sh = (I32){0} + ip->y; v->i32 = v[ip->x].i32 >> sh;
                } NEXT;
                CASE(op_and_32_imm)  {
                    U32 const m = (U32){0} + (uint32_t)ip->y; v->u32 = v[ip->x].u32 & m;
                } NEXT;
                CASE(op_or_32_imm)   {
                    U32 const m = (U32){0} + (uint32_t)ip->y; v->u32 = v[ip->x].u32 | m;
                } NEXT;
                CASE(op_xor_32_imm)  {
                    U32 const m = (U32){0} + (uint32_t)ip->y; v->u32 = v[ip->x].u32 ^ m;
                } NEXT;

                CASE(op_add_f32_imm) { F32_IMM; v->f32 = v[ip->x].f32 + imm; } NEXT;
                CASE(op_sub_f32_imm) { F32_IMM; v->f32 = v[ip->x].f32 - imm; } NEXT;
                CASE(op_mul_f32_imm) { F32_IMM; v->f32 = v[ip->x].f32 * imm; } NEXT;
                CASE(op_div_f32_imm) { F32_IMM; v->f32 = v[ip->x].f32 / imm; } NEXT;
                CASE(op_min_f32_imm) { F32_IMM; v->f32 = vec_min(v[ip->x].f32, imm); } NEXT;
                CASE(op_max_f32_imm) { F32_IMM; v->f32 = vec_max(v[ip->x].f32, imm); } NEXT;
                CASE(op_eq_f32_imm)  { F32_IMM; v->i32 = (I32)(v[ip->x].f32 == imm); } NEXT;
                CASE(op_lt_f32_imm)  { F32_IMM; v->i32 = (I32)(v[ip->x].f32 <  imm); } NEXT;
                CASE(op_le_f32_imm)  { F32_IMM; v->i32 = (I32)(v[ip->x].f32 <= imm); } NEXT;

#define I32_IMM I32 const imm = (I32){0} + ip->y
                CASE(op_add_i32_imm) { I32_IMM; v->i32 = v[ip->x].i32 + imm; } NEXT;
                CASE(op_sub_i32_imm) { I32_IMM; v->i32 = v[ip->x].i32 - imm; } NEXT;
                CASE(op_mul_i32_imm) { I32_IMM; v->i32 = v[ip->x].i32 * imm; } NEXT;
                CASE(op_eq_i32_imm)  { I32_IMM; v->i32 = (I32)(v[ip->x].i32 == imm); } NEXT;
                CASE(op_lt_s32_imm)  { I32_IMM; v->i32 = (I32)(v[ip->x].i32 <  imm); } NEXT;
                CASE(op_le_s32_imm)  { I32_IMM; v->i32 = (I32)(v[ip->x].i32 <= imm); } NEXT;
#undef I32_IMM

                // Binary acc variants: r_mm (start), r_rm (continue), m_rm (end).
#define BIN3(name, dst, OP, x_t, y_t)                                                  \
                CASE(op_r_##name##_mm) acc.dst = v[ip->x].x_t OP v[ip->y].y_t; NEXT;   \
                CASE(op_r_##name##_rm) acc.dst = acc.x_t       OP v[ip->y].y_t; NEXT;   \
                CASE(op_m_##name##_rm) v->dst  = acc.x_t       OP v[ip->y].y_t; NEXT;
                BIN3(add_f32, f32, +,  f32, f32)
                BIN3(sub_f32, f32, -,  f32, f32)
                BIN3(mul_f32, f32, *,  f32, f32)
                BIN3(div_f32, f32, /,  f32, f32)
                BIN3(add_i32, i32, +,  i32, i32)
                BIN3(sub_i32, i32, -,  i32, i32)
                BIN3(mul_i32, i32, *,  i32, i32)
                BIN3(shl_i32, i32, <<, i32, i32)
                BIN3(shr_u32, u32, >>, u32, u32)
                BIN3(shr_s32, i32, >>, i32, i32)
                BIN3(and_32,  i32, &,  i32, i32)
                BIN3(or_32,   i32, |,  i32, i32)
                BIN3(xor_32,  i32, ^,  i32, i32)
#undef BIN3
#define CMP3(name, dst, OP, pt)                                                              \
                CASE(op_r_##name##_mm) acc.dst = (I32)(v[ip->x].pt OP v[ip->y].pt); NEXT;   \
                CASE(op_r_##name##_rm) acc.dst = (I32)(acc.pt       OP v[ip->y].pt); NEXT;   \
                CASE(op_m_##name##_rm) v->dst  = (I32)(acc.pt       OP v[ip->y].pt); NEXT;
                CMP3(eq_f32, i32, ==, f32)
                CMP3(lt_f32, i32, <,  f32)
                CMP3(le_f32, i32, <=, f32)
                CMP3(eq_i32, i32, ==, i32)
                CMP3(lt_s32, i32, <,  i32)
                CMP3(le_s32, i32, <=, i32)
                CMP3(lt_u32, i32, <,  u32)
                CMP3(le_u32, i32, <=, u32)
#undef CMP3
#define FN3(name, dst, FN, pt)                                                        \
                CASE(op_r_##name##_mm) acc.dst = FN(v[ip->x].pt, v[ip->y].pt); NEXT; \
                CASE(op_r_##name##_rm) acc.dst = FN(acc.pt,       v[ip->y].pt); NEXT; \
                CASE(op_m_##name##_rm) v->dst  = FN(acc.pt,       v[ip->y].pt); NEXT;
                FN3(min_f32, f32, vec_min, f32)
                FN3(max_f32, f32, vec_max, f32)
#undef FN3

                // Unary acc variants: r_r (continue), m_r (end).
#define UN2(name, dst, EXPR)                                   \
                CASE(op_r_##name##_r) acc.dst = EXPR; NEXT;    \
                CASE(op_m_##name##_r) v->dst  = EXPR; NEXT;
                UN2(sqrt_f32,     f32, vec_sqrt(acc.f32))
                UN2(abs_f32,      f32, vec_abs(acc.f32))
                UN2(square_f32,   f32, acc.f32 * acc.f32)
                UN2(round_f32,    f32, vec_round(acc.f32))
                UN2(floor_f32,    f32, vec_floor(acc.f32))
                UN2(ceil_f32,     f32, vec_ceil(acc.f32))
                UN2(round_i32,    i32, cast(I32, vec_round(acc.f32)))
                UN2(floor_i32,    i32, cast(I32, vec_floor(acc.f32)))
                UN2(ceil_i32,     i32, cast(I32, vec_ceil(acc.f32)))
                UN2(f32_from_i32, f32, cast(F32, acc.i32))
                UN2(i32_from_f32, i32, cast(I32, acc.f32))
#undef UN2
                CASE(op_r_f32_from_f16_r) {
                    U16 h; __builtin_memcpy(&h, &acc, sizeof h);
                    acc.f32 = f16_to_f32(h);
                } NEXT;
                CASE(op_m_f32_from_f16_r) {
                    U16 h; __builtin_memcpy(&h, &acc, sizeof h);
                    v->f32 = f16_to_f32(h);
                } NEXT;
                CASE(op_r_f16_from_f32_r) {
                    U16 const h = f32_to_f16(acc.f32);
                    acc.u32 = (U32){0}; __builtin_memcpy(&acc, &h, sizeof h);
                } NEXT;
                CASE(op_m_f16_from_f32_r) {
                    U16 const h = f32_to_f16(acc.f32);
                    v->u32 = (U32){0}; __builtin_memcpy(v, &h, sizeof h);
                } NEXT;
                CASE(op_r_i32_from_s16_r) {
                    U16 u; __builtin_memcpy(&u, &acc, sizeof u);
                    S16 s; __builtin_memcpy(&s, &u, sizeof u);
                    acc.i32 = cast(I32, s);
                } NEXT;
                CASE(op_m_i32_from_s16_r) {
                    U16 u; __builtin_memcpy(&u, &acc, sizeof u);
                    S16 s; __builtin_memcpy(&s, &u, sizeof u);
                    v->i32 = cast(I32, s);
                } NEXT;
                CASE(op_r_i32_from_u16_r) {
                    U16 u; __builtin_memcpy(&u, &acc, sizeof u);
                    acc.u32 = cast(U32, u);
                } NEXT;
                CASE(op_m_i32_from_u16_r) {
                    U16 u; __builtin_memcpy(&u, &acc, sizeof u);
                    v->u32 = cast(U32, u);
                } NEXT;
                CASE(op_r_i16_from_i32_r) {
                    U16 u = cast(U16, acc.u32);
                    acc.u32 = (U32){0}; __builtin_memcpy(&acc, &u, sizeof u);
                } NEXT;
                CASE(op_m_i16_from_i32_r) {
                    U16 u = cast(U16, acc.u32);
                    v->u32 = (U32){0}; __builtin_memcpy(v, &u, sizeof u);
                } NEXT;

                // Imm acc variants: r_r (continue), m_r (end).
#define IMM2_I(name, dst, EXPR)                                                             \
                CASE(op_r_##name##_r) { I32 const imm = (I32){0} + ip->y; acc.dst = EXPR; } NEXT; \
                CASE(op_m_##name##_r) { I32 const imm = (I32){0} + ip->y; v->dst  = EXPR; } NEXT;
#define IMM2_U(name, dst, EXPR)                                                              \
                CASE(op_r_##name##_r) {                                                      \
                    U32 const imm = (U32){0} + (uint32_t)ip->y; acc.dst = EXPR;              \
                } NEXT;                                                                      \
                CASE(op_m_##name##_r) {                                                      \
                    U32 const imm = (U32){0} + (uint32_t)ip->y; v->dst  = EXPR;              \
                } NEXT;
#define IMM2_F(name, dst, EXPR)                                             \
                CASE(op_r_##name##_r) { F32_IMM; acc.dst = EXPR; } NEXT;   \
                CASE(op_m_##name##_r) { F32_IMM; v->dst  = EXPR; } NEXT;
                IMM2_I(shl_i32_imm, i32, acc.i32 << imm)
                IMM2_U(shr_u32_imm, u32, acc.u32 >> imm)
                IMM2_I(shr_s32_imm, i32, acc.i32 >> imm)
                IMM2_U(and_32_imm,  u32, acc.u32 & imm)
                IMM2_U(or_32_imm,   u32, acc.u32 | imm)
                IMM2_U(xor_32_imm,  u32, acc.u32 ^ imm)
                IMM2_F(add_f32_imm, f32, acc.f32 + imm)
                IMM2_F(sub_f32_imm, f32, acc.f32 - imm)
                IMM2_F(mul_f32_imm, f32, acc.f32 * imm)
                IMM2_F(div_f32_imm, f32, acc.f32 / imm)
                IMM2_F(min_f32_imm, f32, vec_min(acc.f32, imm))
                IMM2_F(max_f32_imm, f32, vec_max(acc.f32, imm))
                IMM2_I(add_i32_imm, i32, acc.i32 + imm)
                IMM2_I(sub_i32_imm, i32, acc.i32 - imm)
                IMM2_I(mul_i32_imm, i32, acc.i32 * imm)
                IMM2_F(eq_f32_imm,  i32, (I32)(acc.f32 == imm))
                IMM2_F(lt_f32_imm,  i32, (I32)(acc.f32 <  imm))
                IMM2_F(le_f32_imm,  i32, (I32)(acc.f32 <= imm))
                IMM2_I(eq_i32_imm,  i32, (I32)(acc.i32 == imm))
                IMM2_I(lt_s32_imm,  i32, (I32)(acc.i32 <  imm))
                IMM2_I(le_s32_imm,  i32, (I32)(acc.i32 <= imm))
#undef IMM2_I
#undef IMM2_U
#undef IMM2_F

                // sel_32 register variants.
#define SEL(xv,yv,zv) (((xv).i32 & (yv).i32) | (~(xv).i32 & (zv).i32))
                CASE(op_r_sel_32_mm) acc.i32 = SEL(v[ip->x], v[ip->y], v[ip->z]); NEXT;
                CASE(op_r_sel_32_rm) acc.i32 = SEL(acc,       v[ip->y], v[ip->z]); NEXT;
                CASE(op_m_sel_32_rm) v->i32  = SEL(acc,       v[ip->y], v[ip->z]); NEXT;
#undef SEL

                // fma/fms register variants.
#if defined(__ARM_FEATURE_FMA) || defined(__FMA__)
#define FMA_OP(xv,yv,zv) vec_fma((xv), (yv), (zv))
#define FMS_OP(xv,yv,zv) vec_fma(-(xv), (yv), (zv))
#else
                // F64 intermediate for precision on non-FMA platforms.
                // (uses the existing F64 typedef from the non-variant fma cases)
#define FMA_OP(xv,yv,zv) cast(F32, cast(F64,(xv)) * cast(F64,(yv)) + cast(F64,(zv)))
#define FMS_OP(xv,yv,zv) cast(F32, cast(F64,(zv)) - cast(F64,(xv)) * cast(F64,(yv)))
#endif
                CASE(op_r_fma_f32_mmm) {
                    acc.f32 = FMA_OP(v[ip->x].f32, v[ip->y].f32, v[ip->z].f32);
                } NEXT;
                CASE(op_r_fma_f32_mmr) acc.f32 = FMA_OP(v[ip->x].f32, v[ip->y].f32, acc.f32); NEXT;
                CASE(op_m_fma_f32_mmr) v->f32  = FMA_OP(v[ip->x].f32, v[ip->y].f32, acc.f32); NEXT;
                CASE(op_r_fms_f32_mmm) {
                    acc.f32 = FMS_OP(v[ip->x].f32, v[ip->y].f32, v[ip->z].f32);
                } NEXT;
                CASE(op_r_fms_f32_mmr) acc.f32 = FMS_OP(v[ip->x].f32, v[ip->y].f32, acc.f32); NEXT;
                CASE(op_m_fms_f32_mmr) v->f32  = FMS_OP(v[ip->x].f32, v[ip->y].f32, acc.f32); NEXT;
#undef FMA_OP
#undef FMS_OP

                // Output-only register variants.
                CASE(op_r_imm_32) acc.i32 = (I32){0} + ip->x; NEXT;
                CASE(op_r_x) {
                    I32 seq;
                    __builtin_memcpy(&seq, iota, sizeof seq);
                    acc.i32 = seq + (end - K);
                } NEXT;
                CASE(op_r_y) acc.i32 = (I32){0} + row; NEXT;
                CASE(SW_DONE) DONE;
#if !defined(__GNUC__) || defined(__wasm__)
                }
                ip++;
                v++;
            }
#endif
            next_tile:;
        }
    }
    free(scratch);
}
#undef F32_IMM
#undef DISPATCH
#undef CASE
#undef NEXT
#undef DONE

static void interp_program_free(struct interp_program *p) {
    if (p) {
        free(p->inst);
        free(p->binding);
        free(p);
    }
}

static void run_interp(struct umbra_program *prog, int l, int t, int r, int b,
                       struct umbra_late_binding const *late, int lates) {
    interp_program_run((struct interp_program*)prog, l, t, r, b, late, lates);
}
static void free_interp(struct umbra_program *prog) {
    interp_program_free((struct interp_program*)prog);
}
static struct umbra_program* compile_interp(struct umbra_backend *be,
                                             struct umbra_flat_ir const *ir) {
    struct interp_program *p = interp_program(ir);
    p->base = (struct umbra_program){
        .queue      = run_interp,
        .dump       = 0,
        .free       = free_interp,
        .backend    = be,
        .queue_is_threadsafe = !flat_ir_has_early_writes(ir),
    };
    return &p->base;
}
static void flush_be_noop(struct umbra_backend *be) { (void)be; }
static void free_be_interp(struct umbra_backend *be) { free(be); }
static struct umbra_backend_stats stats_zero(struct umbra_backend const *be) {
    (void)be;
    return (struct umbra_backend_stats){0};
}
struct umbra_backend* umbra_backend_interp(void) {
    struct umbra_backend *be = malloc(sizeof *be);
    *be = (struct umbra_backend){
        .compile = compile_interp,
        .flush   = flush_be_noop,
        .free    = free_be_interp,
        .stats   = stats_zero,
        .program_queue_is_cheap  = 1,
        .program_switch_is_cheap = 1,
    };
    return be;
}
