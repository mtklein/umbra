#include "program.h"
#include "bb.h"
#include <math.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>

#define cast(T, v) __builtin_convertvector(v, T)

#define K 8
typedef int32_t  I32 __attribute__((vector_size(K * 4)));
typedef uint32_t U32 __attribute__((vector_size(K * 4)));
typedef float    F32 __attribute__((vector_size(K * 4)));
typedef uint16_t U16 __attribute__((vector_size(K * 2)));

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
} val;

struct sw_inst {
    int tag;   // enum op value, or SW_DONE / SW_LOAD_64_FUSED
    int x, y, z;
};
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wswitch-enum"
enum { SW_DONE = -1, SW_LOAD_64_FUSED = -2 };

struct umbra_switch_interp {
    struct sw_inst *inst;
    val            *v;
    umbra_buf      *buf;
    int             preamble, nptr, n_deref, pad_;
};

struct umbra_switch_interp* umbra_switch_interp(struct umbra_basic_block const *bb) {
    int *id = calloc((size_t)bb->insts, sizeof *id);

    struct umbra_switch_interp *p = malloc(sizeof *p);
    int const num_insts = bb->insts + 1;
    p->inst = malloc((size_t)num_insts * sizeof *p->inst);
    p->v = malloc((size_t)num_insts * sizeof *p->v);

    int max_ptr = -1;
    int n_deref = 0;
    for (int i = 0; i < bb->insts; i++) {
        if (has_ptr(bb->inst[i].op) && bb->inst[i].ptr >= 0 && max_ptr < bb->inst[i].ptr) {
            max_ptr = bb->inst[i].ptr;
        }
        if (bb->inst[i].op == op_deref_ptr) {
            n_deref++;
        }
    }
    p->nptr = max_ptr + 1;
    p->n_deref = n_deref;
    int const total_ptrs = p->nptr + n_deref;
    p->buf = calloc((size_t)total_ptrs, sizeof *p->buf);

    int *deref_slot = calloc((size_t)bb->insts, sizeof *deref_slot);
    {
        int di = 0;
        for (int i = 0; i < bb->insts; i++) {
            if (bb->inst[i].op == op_deref_ptr) {
                deref_slot[i] = p->nptr + di++;
            }
        }
    }

    int n = 0;
#define emit(...) p->inst[n] = (struct sw_inst){ __VA_ARGS__ }
#define RESOLVE_PTR(inst) ((inst)->ptr < 0 ? deref_slot[~(inst)->ptr] : (inst)->ptr)
    for (int pass = 0; pass < 2; pass++) {
        int const lo = pass ? bb->preamble : 0, hi = pass ? bb->insts : bb->preamble;
        if (pass) { p->preamble = n; }
        for (int i = lo; i < hi; i++) {
            struct bb_inst const *inst = &bb->inst[i];
            int const X = id[inst->x] - n, Y = id[inst->y] - n, Z = id[inst->z] - n;
            switch (inst->op) {
            case op_x:      emit(.tag = op_x);      break;
            case op_y:      emit(.tag = op_y);      break;
            case op_imm_32: emit(.tag = op_imm_32, .x = inst->imm); break;

            case op_deref_ptr:
                emit(.tag = op_deref_ptr, .x = inst->ptr, .y = inst->imm, .z = deref_slot[i]);
                break;

            case op_uniform_16: emit(.tag = op_uniform_16, .x = RESOLVE_PTR(inst), .y = inst->imm); break;
            case op_uniform_32: emit(.tag = op_uniform_32, .x = RESOLVE_PTR(inst), .y = inst->imm); break;

            case op_load_16: emit(.tag = op_load_16, .x = RESOLVE_PTR(inst)); break;
            case op_load_32: emit(.tag = op_load_32, .x = RESOLVE_PTR(inst)); break;
            case op_load_64_lo:
                if (i + 1 < hi
                    && bb->inst[i + 1].op == op_load_64_hi
                    && bb->inst[i + 1].ptr == inst->ptr) {
                    emit(.tag = SW_LOAD_64_FUSED, .x = RESOLVE_PTR(inst));
                    id[i] = n - 1;
                    id[i + 1] = n;
                    i++;
                    n++;
                    continue;
                }
                emit(.tag = op_load_64_lo, .x = RESOLVE_PTR(inst));
                break;
            case op_load_64_hi: emit(.tag = op_load_64_hi, .x = RESOLVE_PTR(inst)); break;

            case op_gather_uniform_16: emit(.tag = op_gather_uniform_16, .x = RESOLVE_PTR(inst), .y = X); break;
            case op_gather_16:         emit(.tag = op_gather_16,         .x = RESOLVE_PTR(inst), .y = X); break;
            case op_gather_uniform_32: emit(.tag = op_gather_uniform_32, .x = RESOLVE_PTR(inst), .y = X); break;
            case op_gather_32:         emit(.tag = op_gather_32,         .x = RESOLVE_PTR(inst), .y = X); break;

            case op_store_16: emit(.tag = op_store_16, .x = RESOLVE_PTR(inst), .y = Y); break;
            case op_store_32: emit(.tag = op_store_32, .x = RESOLVE_PTR(inst), .y = Y); break;
            case op_store_64: emit(.tag = op_store_64, .x = RESOLVE_PTR(inst), .y = X, .z = Y); break;

            case op_pack: emit(.tag = op_pack, .x = X, .y = Y, .z = inst->imm); break;

            default:
                if (inst->op >= op_shl_i32_imm && inst->op <= op_le_s32_imm) {
                    emit(.tag = inst->op, .x = X, .y = inst->imm);
                } else {
                    emit(.tag = inst->op, .x = X, .y = Y, .z = Z);
                }
                break;
            }
            id[i] = n++;
        }
    }
#undef emit
#undef RESOLVE_PTR
    p->inst[n] = (struct sw_inst){.tag = SW_DONE};

    free(deref_slot);
    free(id);
    return p;
}

void umbra_switch_interp_run(struct umbra_switch_interp *p, int l, int t, int r, int b,
                             umbra_buf caller_buf[]) {
    int const nall = p->nptr + p->n_deref;
    for (int i = 0; i < p->nptr; i++) { p->buf[i] = caller_buf[i]; }
    for (int i = p->nptr; i < nall; i++) { p->buf[i] = (umbra_buf){0}; }

    int const      P   = p->preamble;
    umbra_buf     *buf = p->buf;

    for (int row = t; row < b; row++) {
        for (int col = l; col < r; col += K) {
            int const              end = col + K;
            int const              n   = r;
            struct sw_inst const  *ip  = p->inst + (col == l ? 0 : P);
            val                   *v   = p->v    + (col == l ? 0 : P);

            for (;;) {
                switch (ip->tag) {

                case op_imm_32: v->i32 = (I32){0} + ip->x; break;
                case op_x: { I32 const seq = {0,1,2,3,4,5,6,7}; v->i32 = seq + (end - K); } break;
                case op_y: v->i32 = (I32){0} + row; break;

                case op_uniform_16: {
                    assert(buf[ip->x].row_bytes == 0);
                    uint16_t uni;
                    __builtin_memcpy(&uni, (uint16_t const*)buf[ip->x].ptr + ip->y, sizeof uni);
                    v->u32 = (U32){0} + (uint32_t)uni;
                } break;
                case op_uniform_32: {
                    assert(buf[ip->x].row_bytes == 0);
                    int32_t uni;
                    __builtin_memcpy(&uni, (int32_t const*)buf[ip->x].ptr + ip->y, sizeof uni);
                    v->i32 = (I32){0} + uni;
                } break;

                case op_load_16: {
                    void const     *base = (char*)buf[ip->x].ptr + (size_t)row * buf[ip->x].row_bytes;
                    uint16_t const *src = (uint16_t const*)base;
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
                } break;
                case op_load_32: {
                    void const    *base = (char*)buf[ip->x].ptr + (size_t)row * buf[ip->x].row_bytes;
                    int32_t const *src = (int32_t const*)base;
                    int const      i = end - K;
                    int const      rem = n - i;
                    if (rem >= K) {
                        __builtin_memcpy(v, src + i, 4 * K);
                    } else {
                        v->i32 = (I32){0};
                        for (int ll = 0; ll < rem; ll++) {
                            int32_t tmp;
                            __builtin_memcpy(&tmp, src + i + ll, 4);
                            v->i32[ll] = tmp;
                        }
                    }
                } break;
                case op_load_64_lo: {
                    char const *src = (char const*)buf[ip->x].ptr + (size_t)row * buf[ip->x].row_bytes;
                    int const   i = end - K;
                    int const   rem = n - i;
                    v->i32 = (I32){0};
                    for (int ll = 0; ll < (rem < K ? rem : K); ll++) {
                        int32_t tmp;
                        __builtin_memcpy(&tmp, src + (i + ll) * 8, 4);
                        v->i32[ll] = tmp;
                    }
                } break;
                case op_load_64_hi: {
                    char const *src = (char const*)buf[ip->x].ptr + (size_t)row * buf[ip->x].row_bytes;
                    int const   i = end - K;
                    int const   rem = n - i;
                    v->i32 = (I32){0};
                    for (int ll = 0; ll < (rem < K ? rem : K); ll++) {
                        int32_t tmp;
                        __builtin_memcpy(&tmp, src + (i + ll) * 8 + 4, 4);
                        v->i32[ll] = tmp;
                    }
                } break;
                case SW_LOAD_64_FUSED: {
                    char const *src = (char const*)buf[ip->x].ptr + (size_t)row * buf[ip->x].row_bytes;
                    int const   i = end - K;
                    int const   rem = n - i;
                    v[0].i32 = (I32){0};
                    v[1].i32 = (I32){0};
                    for (int ll = 0; ll < (rem < K ? rem : K); ll++) {
                        int32_t lo, hi;
                        __builtin_memcpy(&lo, src + (i + ll) * 8,     4);
                        __builtin_memcpy(&hi, src + (i + ll) * 8 + 4, 4);
                        v[0].i32[ll] = lo;
                        v[1].i32[ll] = hi;
                    }
                    v++;
                } break;

                case op_store_16: {
                    void     *base = (char*)buf[ip->x].ptr + (size_t)row * buf[ip->x].row_bytes;
                    uint16_t *dst = (uint16_t*)base;
                    int const i = end - K;
                    int const rem = n - i;
                    if (rem >= K) {
                        __builtin_memcpy(dst + i, &v[ip->y], 2 * K);
                    } else {
                        for (int ll = 0; ll < rem; ll++) {
                            uint16_t s;
                            __builtin_memcpy(&s, (char*)&v[ip->y] + 2 * ll, 2);
                            __builtin_memcpy(dst + i + ll, &s, 2);
                        }
                    }
                } break;
                case op_store_32: {
                    void    *base = (char*)buf[ip->x].ptr + (size_t)row * buf[ip->x].row_bytes;
                    int32_t *dst = (int32_t*)base;
                    int const i = end - K;
                    int const rem = n - i;
                    if (rem >= K) {
                        __builtin_memcpy(dst + i, v + ip->y, 4 * K);
                    } else {
                        for (int ll = 0; ll < rem; ll++) {
                            int32_t tmp;
                            __builtin_memcpy(&tmp, (char*)&v[ip->y].i32 + 4 * ll, 4);
                            __builtin_memcpy(dst + i + ll, &tmp, 4);
                        }
                    }
                } break;
                case op_store_64: {
                    char *dst = (char*)buf[ip->x].ptr + (size_t)row * buf[ip->x].row_bytes;
                    int const i = end - K;
                    int const rem = n - i;
                    for (int ll = 0; ll < (rem < K ? rem : K); ll++) {
                        int32_t lo, hi;
                        __builtin_memcpy(&lo, (char*)&v[ip->y].i32 + 4 * ll, 4);
                        __builtin_memcpy(&hi, (char*)&v[ip->z].i32 + 4 * ll, 4);
                        __builtin_memcpy(dst + (i + ll) * 8,     &lo, 4);
                        __builtin_memcpy(dst + (i + ll) * 8 + 4, &hi, 4);
                    }
                } break;

                case op_gather_uniform_16: {
                    int const ix = v[ip->y].i32[0];
                    int const count = (int)(buf[ip->x].sz / 2);
                    if (ix < 0 || ix >= count) { v->u32 = (U32){0}; break; }
                    uint16_t s;
                    __builtin_memcpy(&s, (char const*)buf[ip->x].ptr + 2 * ix, 2);
                    U16 const packed = (U16){0} + s;
                    v->u32 = (U32){0};
                    __builtin_memcpy(v, &packed, sizeof packed);
                } break;
                case op_gather_uniform_32: {
                    int const ix = v[ip->y].i32[0];
                    int const count = (int)(buf[ip->x].sz / 4);
                    if (ix < 0 || ix >= count) { v->i32 = (I32){0}; break; }
                    int32_t gval;
                    __builtin_memcpy(&gval, (char const*)buf[ip->x].ptr + 4 * ix, 4);
                    v->i32 = (I32){0} + gval;
                } break;
                case op_gather_16: {
                    I32 const ix = v[ip->y].i32;
                    int const count = (int)(buf[ip->x].sz / 2);
                    int const rem = n - (end - K);
                    v->u32 = (U32){0};
                    for (int ll = 0; ll < (rem < K ? rem : K); ll++) {
                        if (ix[ll] >= 0 && ix[ll] < count) {
                            uint16_t s;
                            __builtin_memcpy(&s, (char const*)buf[ip->x].ptr + 2 * ix[ll], 2);
                            __builtin_memcpy((char*)v + 2 * ll, &s, 2);
                        }
                    }
                } break;
                case op_gather_32: {
                    I32 const ix = v[ip->y].i32;
                    int const count = (int)(buf[ip->x].sz / 4);
                    int const rem = n - (end - K);
                    v->i32 = (I32){0};
                    for (int ll = 0; ll < (rem < K ? rem : K); ll++) {
                        if (ix[ll] >= 0 && ix[ll] < count) {
                            int32_t tmp;
                            __builtin_memcpy(&tmp, (char const*)buf[ip->x].ptr + 4 * ix[ll], 4);
                            v->i32[ll] = tmp;
                        }
                    }
                } break;

                case op_deref_ptr: {
                    char *base = (char*)buf[ip->x].ptr + (size_t)row * buf[ip->x].row_bytes;
                    void *derived;
                    ptrdiff_t ssz;
                    size_t drb;
                    __builtin_memcpy(&derived, base + ip->y,      sizeof derived);
                    __builtin_memcpy(&ssz,     base + ip->y + 8,  sizeof ssz);
                    __builtin_memcpy(&drb,     base + ip->y + 16, sizeof drb);
                    buf[ip->z].ptr       = derived;
                    buf[ip->z].sz        = ssz < 0 ? (size_t)-ssz : (size_t)ssz;
                    buf[ip->z].row_bytes = drb;
                } break;

                case op_f32_from_f16: { U16 h; __builtin_memcpy(&h, &v[ip->x], sizeof h); v->f32 = f16_to_f32(h); } break;
                case op_f16_from_f32: { U16 const h = f32_to_f16(v[ip->x].f32); v->u32 = (U32){0}; __builtin_memcpy(v, &h, sizeof h); } break;
                case op_i32_from_s16: {
                    U16 tmp; __builtin_memcpy(&tmp, &v[ip->x], sizeof tmp);
                    typedef int16_t S16 __attribute__((vector_size(K * 2)));
                    S16 stmp; __builtin_memcpy(&stmp, &tmp, sizeof tmp);
                    v->i32 = cast(I32, stmp);
                } break;
                case op_i32_from_u16: { U16 tmp; __builtin_memcpy(&tmp, &v[ip->x], sizeof tmp); v->u32 = cast(U32, tmp); } break;
                case op_i16_from_i32: {
                    U16 tmp;
                    for (int ll = 0; ll < K; ll++) { tmp[ll] = (uint16_t)v[ip->x].u32[ll]; }
                    v->u32 = (U32){0};
                    __builtin_memcpy(v, &tmp, sizeof tmp);
                } break;

                case op_f32_from_i32: v->f32 = cast(F32, v[ip->x].i32); break;
                case op_i32_from_f32: v->i32 = cast(I32, v[ip->x].f32); break;

                case op_add_f32: v->f32 = v[ip->x].f32 + v[ip->y].f32; break;
                case op_sub_f32: v->f32 = v[ip->x].f32 - v[ip->y].f32; break;
                case op_mul_f32: v->f32 = v[ip->x].f32 * v[ip->y].f32; break;
                case op_div_f32: v->f32 = v[ip->x].f32 / v[ip->y].f32; break;
                case op_min_f32: v->f32 = vec_min(v[ip->x].f32, v[ip->y].f32); break;
                case op_max_f32: v->f32 = vec_max(v[ip->x].f32, v[ip->y].f32); break;
                case op_sqrt_f32: v->f32 = vec_sqrt(v[ip->x].f32); break;
                case op_abs_f32: v->f32 = vec_abs(v[ip->x].f32); break;
                case op_neg_f32: v->f32 = -v[ip->x].f32; break;
                case op_round_f32: v->f32 = vec_round(v[ip->x].f32); break;
                case op_floor_f32: v->f32 = vec_floor(v[ip->x].f32); break;
                case op_ceil_f32:  v->f32 = vec_ceil(v[ip->x].f32);  break;
                case op_round_i32: v->i32 = cast(I32, vec_round(v[ip->x].f32)); break;
                case op_floor_i32: v->i32 = cast(I32, vec_floor(v[ip->x].f32)); break;
                case op_ceil_i32:  v->i32 = cast(I32, vec_ceil(v[ip->x].f32));  break;

#if defined(__ARM_FEATURE_FMA) || defined(__FMA__)
                case op_fma_f32: v->f32 = v[ip->z].f32 + v[ip->x].f32 * v[ip->y].f32; break;
                case op_fms_f32: v->f32 = v[ip->z].f32 - v[ip->x].f32 * v[ip->y].f32; break;
#else
                case op_fma_f32: {
                    typedef double F64 __attribute__((vector_size(K * 8)));
                    F64 const x = cast(F64, v[ip->x].f32), y = cast(F64, v[ip->y].f32), z = cast(F64, v[ip->z].f32);
                    v->f32 = cast(F32, x * y + z);
                } break;
                case op_fms_f32: {
                    typedef double F64 __attribute__((vector_size(K * 8)));
                    F64 const x = cast(F64, v[ip->x].f32), y = cast(F64, v[ip->y].f32), z = cast(F64, v[ip->z].f32);
                    v->f32 = cast(F32, z - x * y);
                } break;
#endif

                case op_add_i32: v->i32 = v[ip->x].i32 + v[ip->y].i32; break;
                case op_sub_i32: v->i32 = v[ip->x].i32 - v[ip->y].i32; break;
                case op_mul_i32: v->i32 = v[ip->x].i32 * v[ip->y].i32; break;
                case op_shl_i32: v->i32 = v[ip->x].i32 << v[ip->y].i32; break;
                case op_shr_u32: v->u32 = v[ip->x].u32 >> v[ip->y].u32; break;
                case op_shr_s32: v->i32 = v[ip->x].i32 >> v[ip->y].i32; break;
                case op_and_32:  v->i32 = v[ip->x].i32 & v[ip->y].i32;  break;
                case op_or_32:   v->i32 = v[ip->x].i32 | v[ip->y].i32;  break;
                case op_xor_32:  v->i32 = v[ip->x].i32 ^ v[ip->y].i32;  break;
                case op_sel_32:  v->i32 = (v[ip->x].i32 & v[ip->y].i32) | (~v[ip->x].i32 & v[ip->z].i32); break;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
                case op_eq_f32: v->i32 = (I32)(v[ip->x].f32 == v[ip->y].f32); break;
#pragma clang diagnostic pop
                case op_lt_f32: v->i32 = (I32)(v[ip->x].f32 <  v[ip->y].f32); break;
                case op_le_f32: v->i32 = (I32)(v[ip->x].f32 <= v[ip->y].f32); break;
                case op_eq_i32: v->i32 = (I32)(v[ip->x].i32 == v[ip->y].i32); break;
                case op_lt_s32: v->i32 = (I32)(v[ip->x].i32 <  v[ip->y].i32); break;
                case op_le_s32: v->i32 = (I32)(v[ip->x].i32 <= v[ip->y].i32); break;
                case op_lt_u32: v->i32 = (I32)(v[ip->x].u32 <  v[ip->y].u32); break;
                case op_le_u32: v->i32 = (I32)(v[ip->x].u32 <= v[ip->y].u32); break;

                case op_shl_i32_imm: { I32 const sh = (I32){0} + ip->y; v->i32 = v[ip->x].i32 << sh; } break;
                case op_shr_u32_imm: { U32 const sh = (U32){0} + (uint32_t)ip->y; v->u32 = v[ip->x].u32 >> sh; } break;
                case op_shr_s32_imm: { I32 const sh = (I32){0} + ip->y; v->i32 = v[ip->x].i32 >> sh; } break;
                case op_and_32_imm:  { U32 const m = (U32){0} + (uint32_t)ip->y; v->u32 = v[ip->x].u32 & m; } break;
                case op_or_32_imm:   { U32 const m = (U32){0} + (uint32_t)ip->y; v->u32 = v[ip->x].u32 | m; } break;
                case op_xor_32_imm:  { U32 const m = (U32){0} + (uint32_t)ip->y; v->u32 = v[ip->x].u32 ^ m; } break;

#define F32_IMM union { int i; float f; } const u = {.i = ip->y}; F32 const imm = (F32){0} + u.f
                case op_add_f32_imm: { F32_IMM; v->f32 = v[ip->x].f32 + imm; } break;
                case op_sub_f32_imm: { F32_IMM; v->f32 = v[ip->x].f32 - imm; } break;
                case op_mul_f32_imm: { F32_IMM; v->f32 = v[ip->x].f32 * imm; } break;
                case op_div_f32_imm: { F32_IMM; v->f32 = v[ip->x].f32 / imm; } break;
                case op_min_f32_imm: { F32_IMM; v->f32 = vec_min(v[ip->x].f32, imm); } break;
                case op_max_f32_imm: { F32_IMM; v->f32 = vec_max(v[ip->x].f32, imm); } break;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
                case op_eq_f32_imm:  { F32_IMM; v->i32 = (I32)(v[ip->x].f32 == imm); } break;
#pragma clang diagnostic pop
                case op_lt_f32_imm:  { F32_IMM; v->f32 = (F32)(v[ip->x].f32 <  imm); } break;
                case op_le_f32_imm:  { F32_IMM; v->f32 = (F32)(v[ip->x].f32 <= imm); } break;
#undef F32_IMM

#define I32_IMM I32 const imm = (I32){0} + ip->y
                case op_add_i32_imm: { I32_IMM; v->i32 = v[ip->x].i32 + imm; } break;
                case op_sub_i32_imm: { I32_IMM; v->i32 = v[ip->x].i32 - imm; } break;
                case op_mul_i32_imm: { I32_IMM; v->i32 = v[ip->x].i32 * imm; } break;
                case op_eq_i32_imm:  { I32_IMM; v->i32 = (I32)(v[ip->x].i32 == imm); } break;
                case op_lt_s32_imm:  { I32_IMM; v->i32 = (I32)(v[ip->x].i32 <  imm); } break;
                case op_le_s32_imm:  { I32_IMM; v->i32 = (I32)(v[ip->x].i32 <= imm); } break;
#undef I32_IMM

                case op_pack: {
                    I32 const sh = (I32){0} + ip->z;
                    v->u32 = v[ip->x].u32 | (U32)(v[ip->y].i32 << sh);
                } break;

                case SW_DONE: goto next_tile;
                }
                ip++;
                v++;
            }
            next_tile:;
        }
    }
}

void umbra_switch_interp_free(struct umbra_switch_interp *p) {
    if (p) {
        free(p->buf);
        free(p->inst);
        free(p->v);
        free(p);
    }
}
