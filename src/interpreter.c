#include "program.h"
#include "bb.h"
#include <math.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>

#define cast(T, v) __builtin_convertvector(v, T)

#define K 16
static const int iota[K] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
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

__attribute__((noinline))
static void interp_load_color(val v[4], umbra_buf const *b,
                               char const *src, int i, int rem) {
    switch (b->fmt) {
    case umbra_fmt_8888: {
        U32 px;
        if (rem >= K) {
            __builtin_memcpy(&px, src + i * 4, sizeof px);
        } else {
            px = (U32){0};
            for (int ll = 0; ll < rem; ll++) {
                uint32_t tmp;
                __builtin_memcpy(&tmp, src + (i + ll) * 4, 4);
                px[ll] = tmp;
            }
        }
        U32 const mask = (U32){0} + 0xFFu;
        F32 const inv  = (F32){0} + (1.f/255);
        v[0].f32 = cast(F32, (I32)(px & mask))         * inv;
        v[1].f32 = cast(F32, (I32)((px >>  8) & mask)) * inv;
        v[2].f32 = cast(F32, (I32)((px >> 16) & mask)) * inv;
        v[3].f32 = cast(F32, (I32)(px >> 24))          * inv;
    } break;
    case umbra_fmt_565: {
        U16 px;
        if (rem >= K) {
            __builtin_memcpy(&px, src + i * 2, sizeof px);
        } else {
            px = (U16){0};
            for (int ll = 0; ll < rem; ll++) {
                uint16_t tmp;
                __builtin_memcpy(&tmp, src + (i + ll) * 2, 2);
                px[ll] = tmp;
            }
        }
        U16 const r5 = px >> 11;
        U16 const g6 = (px >> 5) & (U16)((U16){0} + 0x3F);
        U16 const b5 = px & (U16)((U16){0} + 0x1F);
        v[0].f32 = cast(F32, cast(I32, cast(U32, r5))) * ((F32){0} + (1.f/31));
        v[1].f32 = cast(F32, cast(I32, cast(U32, g6))) * ((F32){0} + (1.f/63));
        v[2].f32 = cast(F32, cast(I32, cast(U32, b5))) * ((F32){0} + (1.f/31));
        v[3].f32 = (F32){0} + 1.f;
    } break;
    case umbra_fmt_1010102: {
        U32 px;
        if (rem >= K) {
            __builtin_memcpy(&px, src + i * 4, sizeof px);
        } else {
            px = (U32){0};
            for (int ll = 0; ll < rem; ll++) {
                uint32_t tmp;
                __builtin_memcpy(&tmp, src + (i + ll) * 4, 4);
                px[ll] = tmp;
            }
        }
        U32 const mask10 = (U32){0} + 0x3FFu;
        v[0].f32 = cast(F32, cast(I32, px & mask10))         * ((F32){0} + (1.f/1023));
        v[1].f32 = cast(F32, cast(I32, (px >> 10) & mask10)) * ((F32){0} + (1.f/1023));
        v[2].f32 = cast(F32, cast(I32, (px >> 20) & mask10)) * ((F32){0} + (1.f/1023));
        v[3].f32 = cast(F32, cast(I32, px >> 30))            * ((F32){0} + (1.f/3));
    } break;
    case umbra_fmt_fp16: {
        U16 hr, hg, hb, ha;
        if (rem >= K) {
            U64 px;
            __builtin_memcpy(&px, src + i * 8, sizeof px);
            U64 const mask16 = (U64){0} + 0xFFFFULL;
            hr = cast(U16,  px        & mask16);
            hg = cast(U16, (px >> 16) & mask16);
            hb = cast(U16, (px >> 32) & mask16);
            ha = cast(U16,  px >> 48);
        } else {
            hr = (U16){0}; hg = (U16){0}; hb = (U16){0}; ha = (U16){0};
            for (int ll = 0; ll < rem; ll++) {
                uint16_t h[4];
                __builtin_memcpy(h, src + (i + ll) * 8, 8);
                hr[ll] = h[0]; hg[ll] = h[1]; hb[ll] = h[2]; ha[ll] = h[3];
            }
        }
        v[0].f32 = f16_to_f32(hr);
        v[1].f32 = f16_to_f32(hg);
        v[2].f32 = f16_to_f32(hb);
        v[3].f32 = f16_to_f32(ha);
    } break;
    case umbra_fmt_srgb: {
        U32 px;
        if (rem >= K) {
            __builtin_memcpy(&px, src + i * 4, sizeof px);
        } else {
            px = (U32){0};
            for (int ll = 0; ll < rem; ll++) {
                uint32_t tmp;
                __builtin_memcpy(&tmp, src + (i + ll) * 4, 4);
                px[ll] = tmp;
            }
        }
        U32 const mask = (U32){0} + 0xFFu;
        F32 const inv  = (F32){0} + (1.f/255);
        v[0].f32 = cast(F32, (I32)(px & mask))         * inv;
        v[1].f32 = cast(F32, (I32)((px >>  8) & mask)) * inv;
        v[2].f32 = cast(F32, (I32)((px >> 16) & mask)) * inv;
        v[3].f32 = cast(F32, (I32)(px >> 24))          * inv;
        // sRGB→linear: polynomial approximation (no powf).
        for (int ch = 0; ch < 3; ch++) {
            F32 s = v[ch].f32;
            F32 const lo = s * ((F32){0} + (1.0f/12.92f));
            F32 const hi = s*s * (s * ((F32){0} + 0.3f) + ((F32){0} + 0.6975f))
                         + ((F32){0} + 0.0025f);
            I32 const sel = (I32)(s < ((F32){0} + 0.055f));
            union { F32 f; I32 i; } lo_u = {.f=lo}, hi_u = {.f=hi}, r;
            r.i = (sel & lo_u.i) | (~sel & hi_u.i);
            v[ch].f32 = r.f;
        }
    } break;
    case umbra_fmt_none: break;
    case umbra_fmt_fp16_planar: {
        size_t const ps = b->sz / 4;
        char const *p0 = src;
        U16 hr = {0}, hg = {0}, hb = {0}, ha = {0};
        if (rem >= K) {
            __builtin_memcpy(&hr, p0           + i * 2, sizeof hr);
            __builtin_memcpy(&hg, p0 + ps      + i * 2, sizeof hg);
            __builtin_memcpy(&hb, p0 + ps * 2  + i * 2, sizeof hb);
            __builtin_memcpy(&ha, p0 + ps * 3  + i * 2, sizeof ha);
        } else {
            for (int ll = 0; ll < rem; ll++) {
                uint16_t tmp;
                __builtin_memcpy(&tmp, p0           + (i + ll) * 2, 2); hr[ll] = tmp;
                __builtin_memcpy(&tmp, p0 + ps      + (i + ll) * 2, 2); hg[ll] = tmp;
                __builtin_memcpy(&tmp, p0 + ps * 2  + (i + ll) * 2, 2); hb[ll] = tmp;
                __builtin_memcpy(&tmp, p0 + ps * 3  + (i + ll) * 2, 2); ha[ll] = tmp;
            }
        }
        v[0].f32 = f16_to_f32(hr);
        v[1].f32 = f16_to_f32(hg);
        v[2].f32 = f16_to_f32(hb);
        v[3].f32 = f16_to_f32(ha);
    } break;
    }
}

__attribute__((noinline))
static void interp_store_color(val const v[], umbra_buf const *b,
                                char *dst, int i, int rem) {
    F32 cr = v[0].f32, cg = v[1].f32, cb = v[2].f32, ca = v[3].f32;
    switch (b->fmt) {
    case umbra_fmt_8888: {
        F32 const zero = {0}, one = (F32){0} + 1.f, scale = (F32){0} + 255.f;
        F32 const rc = vec_round(vec_min(vec_max(cr, zero), one) * scale);
        F32 const gc = vec_round(vec_min(vec_max(cg, zero), one) * scale);
        F32 const bc = vec_round(vec_min(vec_max(cb, zero), one) * scale);
        F32 const ac = vec_round(vec_min(vec_max(ca, zero), one) * scale);
        U32 const px = cast(U32, cast(I32, rc))
                     | cast(U32, cast(I32, gc)) <<  8
                     | cast(U32, cast(I32, bc)) << 16
                     | cast(U32, cast(I32, ac)) << 24;
        if (rem >= K) {
            __builtin_memcpy(dst + i * 4, &px, sizeof px);
        } else {
            for (int ll = 0; ll < rem; ll++) {
                uint32_t tmp = px[ll];
                __builtin_memcpy(dst + (i + ll) * 4, &tmp, 4);
            }
        }
    } break;
    case umbra_fmt_565: {
        F32 const zero = {0}, one = (F32){0} + 1.f;
        F32 const rc = vec_round(vec_min(vec_max(cr, zero), one) * ((F32){0} + 31.f));
        F32 const gc = vec_round(vec_min(vec_max(cg, zero), one) * ((F32){0} + 63.f));
        F32 const bc = vec_round(vec_min(vec_max(cb, zero), one) * ((F32){0} + 31.f));
        U32 const px32 = cast(U32, cast(I32, rc)) << 11
                       | cast(U32, cast(I32, gc)) <<  5
                       | cast(U32, cast(I32, bc));
        U16 const px = cast(U16, cast(S16, cast(I32, px32)));
        if (rem >= K) {
            __builtin_memcpy(dst + i * 2, &px, sizeof px);
        } else {
            for (int ll = 0; ll < rem; ll++) {
                uint16_t tmp = px[ll];
                __builtin_memcpy(dst + (i + ll) * 2, &tmp, 2);
            }
        }
    } break;
    case umbra_fmt_1010102: {
        F32 const zero = {0}, one = (F32){0} + 1.f;
        F32 const rc = vec_round(vec_min(vec_max(cr, zero), one) * ((F32){0} + 1023.f));
        F32 const gc = vec_round(vec_min(vec_max(cg, zero), one) * ((F32){0} + 1023.f));
        F32 const bc = vec_round(vec_min(vec_max(cb, zero), one) * ((F32){0} + 1023.f));
        F32 const ac = vec_round(vec_min(vec_max(ca, zero), one) * ((F32){0} + 3.f));
        U32 const px = cast(U32, cast(I32, rc))
                     | cast(U32, cast(I32, gc)) << 10
                     | cast(U32, cast(I32, bc)) << 20
                     | cast(U32, cast(I32, ac)) << 30;
        if (rem >= K) {
            __builtin_memcpy(dst + i * 4, &px, sizeof px);
        } else {
            for (int ll = 0; ll < rem; ll++) {
                uint32_t tmp = px[ll];
                __builtin_memcpy(dst + (i + ll) * 4, &tmp, 4);
            }
        }
    } break;
    case umbra_fmt_fp16: {
        U16 hr = f32_to_f16(cr);
        U16 hg = f32_to_f16(cg);
        U16 hb = f32_to_f16(cb);
        U16 ha = f32_to_f16(ca);
        if (rem >= K) {
            U64 const px = cast(U64, hr)
                         | cast(U64, hg) << 16
                         | cast(U64, hb) << 32
                         | cast(U64, ha) << 48;
            __builtin_memcpy(dst + i * 8, &px, sizeof px);
        } else {
            for (int ll = 0; ll < rem; ll++) {
                uint16_t h[4] = {hr[ll], hg[ll], hb[ll], ha[ll]};
                __builtin_memcpy(dst + (i + ll) * 8, h, 8);
            }
        }
    } break;
    case umbra_fmt_srgb: {
        // linear→sRGB: rsqrt/rcp rational approximation (no powf).
        {
            F32 const vc  = (F32){0} + 1.12999999523f;
            F32 const vd  = (F32){0} + 0.14137776196f;
            F32 const vk1 = (F32){0} + 0.01383202704f;
            F32 const vk2 = (F32){0} + -0.00245423456f;
            F32 *chs[3] = {&cr, &cg, &cb};
            for (int ci = 0; ci < 3; ci++) {
                F32 l = vec_max(*chs[ci], (F32){0});
                F32 const lo = l * ((F32){0} + 12.92f);
                F32 const t  = ((F32){0} + 1.f) / vec_sqrt(vec_max(l, (F32){0} + 1e-30f));
                F32 const hi = (vc + t * (vk1 + t * vk2)) / (vd + t);
                I32 const mask = (I32)(l < ((F32){0} + 0.00465985f));
                union { F32 f; I32 i; } lo_u = {.f=lo}, hi_u = {.f=hi}, r;
                r.i = (mask & lo_u.i) | (~mask & hi_u.i);
                *chs[ci] = r.f;
            }
        }
        F32 const zero = {0}, one = (F32){0} + 1.f, scale = (F32){0} + 255.f;
        F32 const rc = vec_round(vec_min(vec_max(cr, zero), one) * scale);
        F32 const gc = vec_round(vec_min(vec_max(cg, zero), one) * scale);
        F32 const bc = vec_round(vec_min(vec_max(cb, zero), one) * scale);
        F32 const ac = vec_round(vec_min(vec_max(ca, zero), one) * scale);
        U32 const px = cast(U32, cast(I32, rc))
                     | cast(U32, cast(I32, gc)) <<  8
                     | cast(U32, cast(I32, bc)) << 16
                     | cast(U32, cast(I32, ac)) << 24;
        if (rem >= K) {
            __builtin_memcpy(dst + i * 4, &px, sizeof px);
        } else {
            for (int ll = 0; ll < rem; ll++) {
                uint32_t tmp = px[ll];
                __builtin_memcpy(dst + (i + ll) * 4, &tmp, 4);
            }
        }
    } break;
    case umbra_fmt_none: break;
    case umbra_fmt_fp16_planar: {
        size_t const ps = b->sz / 4;
        U16 hr = f32_to_f16(cr);
        U16 hg = f32_to_f16(cg);
        U16 hb = f32_to_f16(cb);
        U16 ha = f32_to_f16(ca);
        if (rem >= K) {
            __builtin_memcpy(dst           + i * 2, &hr, sizeof hr);
            __builtin_memcpy(dst + ps      + i * 2, &hg, sizeof hg);
            __builtin_memcpy(dst + ps * 2  + i * 2, &hb, sizeof hb);
            __builtin_memcpy(dst + ps * 3  + i * 2, &ha, sizeof ha);
        } else {
            for (int ll = 0; ll < rem; ll++) {
                uint16_t tmp;
                tmp = hr[ll]; __builtin_memcpy(dst           + (i + ll) * 2, &tmp, 2);
                tmp = hg[ll]; __builtin_memcpy(dst + ps      + (i + ll) * 2, &tmp, 2);
                tmp = hb[ll]; __builtin_memcpy(dst + ps * 2  + (i + ll) * 2, &tmp, 2);
                tmp = ha[ll]; __builtin_memcpy(dst + ps * 3  + (i + ll) * 2, &tmp, 2);
            }
        }
    } break;
    }
}

// Tag values: all enum op values (0..op_le_s32_imm), plus interpreter-only ops.
//
// Register variants: op_<ret>_<name>_<params> where r=register, m=memory.
// The existing op_<name> is the all-memory variant (mm->m for binary, m->m for unary).
// Example: op_r_mul_f32_rm = "result to register, x from register, y from memory"

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
    X(sqrt_f32,    f32, f32) X(abs_f32,     f32, f32) X(neg_f32,     f32, f32)             \
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

enum {
    SW_DONE = op_le_s32_imm + 1,
    // Binary op variants: 7 new per op (existing op_X is the mm->m variant)
#define BINARY_VARIANTS(name, rt, pt) \
    op_r_##name##_mm, op_r_##name##_rm, op_m_##name##_rm, \
    op_r_##name##_mr, op_m_##name##_mr, \
    op_r_##name##_rr, op_m_##name##_rr,
    BINARY_OPS(BINARY_VARIANTS)
#undef BINARY_VARIANTS

    // Unary op variants: 3 new per op (existing op_X is the m->m variant)
#define UNARY_VARIANTS(name, rt, pt) \
    op_r_##name##_m, op_r_##name##_r, op_m_##name##_r,
    UNARY_OPS(UNARY_VARIANTS)
#undef UNARY_VARIANTS

    // Imm op variants: 3 new per op (existing op_X is the m->m variant, y is always imm)
#define IMM_VARIANTS(name, rt, pt) \
    op_r_##name##_m, op_r_##name##_r, op_m_##name##_r,
    IMM_OPS(IMM_VARIANTS)
#undef IMM_VARIANTS

    // Output-only variants: no register inputs, just output to register.
    op_r_imm_32, op_r_x, op_r_y, op_r_uniform_32,
    op_r_pack_mm, op_r_pack_rm, op_m_pack_rm,
    op_r_pack_mr, op_m_pack_mr, op_r_pack_rr, op_m_pack_rr,
    // fma/fms: z is most commonly the register operand (accumulator chains)
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
#pragma clang diagnostic ignored "-Wsign-conversion"
#ifdef __clang__
#pragma clang diagnostic ignored "-Wgnu-label-as-value"
#else
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

struct umbra_interpreter {
    struct sw_inst *inst;
    val            *v;
    umbra_buf      *buf;
    int             preamble, nptr, n_deref, pad_;
};

struct umbra_interpreter* umbra_interpreter(struct umbra_basic_block const *bb) {
    int *id = calloc((size_t)bb->insts, sizeof *id);

    struct umbra_interpreter *p = malloc(sizeof *p);
    // Count slots, leaving room for multi-result ops (load_32x2 uses 2, load_8x4 uses 4).
    int num_slots = 1; // +1 for SW_DONE sentinel
    for (int i = 0; i < bb->insts; i++) {
        enum op const op = bb->inst[i].op;
        if (op == op_load_color) { num_slots += 4; }
        else                    { num_slots += 1; }
    }
    p->inst = malloc((size_t)num_slots * sizeof *p->inst);
    p->v = malloc((size_t)num_slots * sizeof *p->v);

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
            int const X = id[inst->x.id] + (int)inst->x.chan - n;
            int const Y = id[inst->y.id] + (int)inst->y.chan - n;
            int const Z = id[inst->z.id] + (int)inst->z.chan - n;
            int const W = id[inst->w.id] + (int)inst->w.chan - n;
            switch (inst->op) {
            case op_x:      emit(.tag = op_x);      break;
            case op_y:      emit(.tag = op_y);      break;
            case op_imm_32: emit(.tag = op_imm_32, .x = inst->imm); break;

            case op_deref_ptr:
                emit(.tag = op_deref_ptr, .ptr = inst->ptr, .x = inst->imm, .y = deref_slot[i]);
                break;

            case op_uniform_32: emit(.tag = op_uniform_32, .ptr = RESOLVE_PTR(inst), .x = inst->imm); break;

            case op_load_16: emit(.tag = op_load_16, .ptr = RESOLVE_PTR(inst)); break;
            case op_load_32: emit(.tag = op_load_32, .ptr = RESOLVE_PTR(inst)); break;
            case op_gather_16:         emit(.tag = op_gather_16,         .ptr = RESOLVE_PTR(inst), .x = X); break;
            case op_gather_uniform_32: emit(.tag = op_gather_uniform_32, .ptr = RESOLVE_PTR(inst), .x = X); break;
            case op_gather_32:         emit(.tag = op_gather_32,         .ptr = RESOLVE_PTR(inst), .x = X); break;

            case op_store_16: emit(.tag = op_store_16, .ptr = RESOLVE_PTR(inst), .x = Y); break;
            case op_store_32: emit(.tag = op_store_32, .ptr = RESOLVE_PTR(inst), .x = Y); break;

            case op_load_color:
                emit(.tag = op_load_color, .ptr = RESOLVE_PTR(inst));
                id[i] = n;
                n++;
                p->inst[n++] = (struct sw_inst){.tag = op_load_color};
                p->inst[n++] = (struct sw_inst){.tag = op_load_color};
                p->inst[n++] = (struct sw_inst){.tag = op_load_color};
                continue;
            case op_store_color:
                emit(.tag = op_store_color, .ptr = RESOLVE_PTR(inst),
                     .x = X, .y = Y, .z = Z, .w = W);
                break;

            case op_pack: emit(.tag = op_pack, .x = X, .y = Y, .z = inst->imm); break;

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
            case op_neg_f32:
            case op_round_f32:
            case op_floor_f32:
            case op_ceil_f32:
            case op_round_i32:
            case op_floor_i32:
            case op_ceil_i32:
            case op_fma_f32:
            case op_fms_f32:
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
            // Output to register only if the sole consumer is an upgradable ALU op
            // and doesn't cross the preamble/body boundary.
            _Bool out_r = 0;
            if (lu[i] == i + 1 && i + 1 != p->preamble) {
                int const next_tag = p->inst[i + 1].tag;
#define CHECK_BINARY(name, rt, pt) || next_tag == op_##name
#define CHECK_UNARY(name, rt, pt)  || next_tag == op_##name
#define CHECK_IMM(name, rt, pt)    || next_tag == op_##name
                out_r = 0 BINARY_OPS(CHECK_BINARY) UNARY_OPS(CHECK_UNARY) IMM_OPS(CHECK_IMM)
                        || next_tag == op_pack
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

            // Only upgrade ALU ops — loads/stores/gathers/deref stay as-is.
            int const tag = s->tag;

            // Try binary ops.
#define TRY_BINARY(name, rt, pt) \
            if (tag == op_##name) { \
                if      ( out_r &&  x_r &&  y_r) { s->tag = op_r_##name##_rr; } \
                else if ( out_r &&  x_r && !y_r) { s->tag = op_r_##name##_rm; } \
                else if (!out_r &&  x_r && !y_r) { s->tag = op_m_##name##_rm; } \
                else if ( out_r && !x_r &&  y_r) { s->tag = op_r_##name##_mr; } \
                else if (!out_r && !x_r &&  y_r) { s->tag = op_m_##name##_mr; } \
                else if ( out_r &&  x_r &&  y_r) { s->tag = op_r_##name##_rr; } \
                else if (!out_r &&  x_r &&  y_r) { s->tag = op_m_##name##_rr; } \
                else if ( out_r && !x_r && !y_r) { s->tag = op_r_##name##_mm; } \
                /* else: !out_r && !x_r && !y_r → keep original tag (mm→m) */ \
            } else
            BINARY_OPS(TRY_BINARY)
#undef TRY_BINARY

            // Try unary ops (only x input, ignore y_r).
#define TRY_UNARY(name, rt, pt) \
            if (tag == op_##name) { \
                if      ( out_r &&  x_r) { s->tag = op_r_##name##_r; } \
                else if (!out_r &&  x_r) { s->tag = op_m_##name##_r; } \
                else if ( out_r && !x_r) { s->tag = op_r_##name##_m; } \
            } else
            UNARY_OPS(TRY_UNARY)
#undef TRY_UNARY

            // Try _imm ops (only x is variable input).
#define TRY_IMM(name, rt, pt) \
            if (tag == op_##name) { \
                if      ( out_r &&  x_r) { s->tag = op_r_##name##_r; } \
                else if (!out_r &&  x_r) { s->tag = op_m_##name##_r; } \
                else if ( out_r && !x_r) { s->tag = op_r_##name##_m; } \
            } else
            IMM_OPS(TRY_IMM)
#undef TRY_IMM
            // fma/fms: z is most commonly from register (accumulator chains).
            // Only on FMA platforms — the F64 fallback path has precision
            // differences between register and memory accumulation.
#if defined(__ARM_FEATURE_FMA) || defined(__FMA__)
            if (tag == op_fma_f32) {
                if      ( out_r &&  z_r) { s->tag = op_r_fma_f32_mmr; }
                else if (!out_r &&  z_r) { s->tag = op_m_fma_f32_mmr; }
                else if ( out_r && !z_r) { s->tag = op_r_fma_f32_mmm; }
            } else
            if (tag == op_fms_f32) {
                if      ( out_r &&  z_r) { s->tag = op_r_fms_f32_mmr; }
                else if (!out_r &&  z_r) { s->tag = op_m_fms_f32_mmr; }
                else if ( out_r && !z_r) { s->tag = op_r_fms_f32_mmm; }
            } else
#endif

            // sel_32: x (mask) from register.
            if (tag == op_sel_32) {
                if      ( out_r &&  x_r) { s->tag = op_r_sel_32_rm; }
                else if (!out_r &&  x_r) { s->tag = op_m_sel_32_rm; }
                else if ( out_r && !x_r) { s->tag = op_r_sel_32_mm; }
            } else

            // pack: binary pattern (x,y inputs, z is immediate shift).
            if (tag == op_pack) {
                if      ( out_r &&  x_r &&  y_r) { s->tag = op_r_pack_rr; }
                else if ( out_r &&  x_r && !y_r) { s->tag = op_r_pack_rm; }
                else if (!out_r &&  x_r && !y_r) { s->tag = op_m_pack_rm; }
                else if ( out_r && !x_r &&  y_r) { s->tag = op_r_pack_mr; }
                else if (!out_r && !x_r &&  y_r) { s->tag = op_m_pack_mr; }
                else if (!out_r &&  x_r &&  y_r) { s->tag = op_m_pack_rr; }
                else if ( out_r && !x_r && !y_r) { s->tag = op_r_pack_mm; }
            } else

            // Output-only ops: no register inputs, just output to register.
            if (out_r && (tag == op_imm_32 || tag == op_x || tag == op_y
                       || tag == op_uniform_32)) {
                     if (tag == op_imm_32)      { s->tag = op_r_imm_32; }
                else if (tag == op_x)           { s->tag = op_r_x; }
                else if (tag == op_y)           { s->tag = op_r_y; }
                else if (tag == op_uniform_32)  { s->tag = op_r_uniform_32; }
            } else
            { /* not an upgradable op */ }

            prev_r = (s->tag != tag) ? out_r : 0;
        }
        free(lu);
    }

    free(deref_slot);
    free(id);
    return p;
}

void umbra_interpreter_run(struct umbra_interpreter *p, int l, int t, int r, int b,
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

            val acc = {0};
#define F32_IMM union { int i; float f; } const u = {.i = ip->y}; F32 const imm = (F32){0} + u.f
            // Computed goto on native, switch on WASM.
#if defined(__GNUC__) && !defined(__wasm__)
    #define DISPATCH    goto *labels[ip->tag]
    #define CASE(label) L_##label:
    // The asm volatile prevents the compiler from tail-merging dispatch sites.
    // Each NEXT must be its own indirect branch for branch predictor accuracy.
    #define NEXT        do { ip++; v++; __asm__ volatile(""); DISPATCH; } while (0)
    #define DONE        goto next_tile
            static void *const labels[SW_NUM_OPS] = {
                [op_x] = &&L_op_x, [op_y] = &&L_op_y, [op_imm_32] = &&L_op_imm_32,
                [op_uniform_32] = &&L_op_uniform_32,
                [op_load_16] = &&L_op_load_16, [op_load_32] = &&L_op_load_32,
                [op_store_16] = &&L_op_store_16, [op_store_32] = &&L_op_store_32,
                [op_load_color] = &&L_op_load_color, [op_store_color] = &&L_op_store_color,
                [op_gather_uniform_32] = &&L_op_gather_uniform_32,
                [op_gather_16] = &&L_op_gather_16, [op_gather_32] = &&L_op_gather_32,
                [op_deref_ptr] = &&L_op_deref_ptr,
                [op_f32_from_f16] = &&L_op_f32_from_f16, [op_f16_from_f32] = &&L_op_f16_from_f32,
                [op_i32_from_s16] = &&L_op_i32_from_s16, [op_i32_from_u16] = &&L_op_i32_from_u16,
                [op_i16_from_i32] = &&L_op_i16_from_i32,
                [op_f32_from_i32] = &&L_op_f32_from_i32, [op_i32_from_f32] = &&L_op_i32_from_f32,
                [op_add_f32] = &&L_op_add_f32, [op_sub_f32] = &&L_op_sub_f32,
                [op_mul_f32] = &&L_op_mul_f32, [op_div_f32] = &&L_op_div_f32,
                [op_min_f32] = &&L_op_min_f32, [op_max_f32] = &&L_op_max_f32,
                [op_sqrt_f32] = &&L_op_sqrt_f32, [op_abs_f32] = &&L_op_abs_f32,
                [op_neg_f32] = &&L_op_neg_f32,
                [op_round_f32] = &&L_op_round_f32, [op_floor_f32] = &&L_op_floor_f32,
                [op_ceil_f32] = &&L_op_ceil_f32,
                [op_round_i32] = &&L_op_round_i32, [op_floor_i32] = &&L_op_floor_i32,
                [op_ceil_i32] = &&L_op_ceil_i32,
                [op_fma_f32] = &&L_op_fma_f32, [op_fms_f32] = &&L_op_fms_f32,
                [op_add_i32] = &&L_op_add_i32, [op_sub_i32] = &&L_op_sub_i32,
                [op_mul_i32] = &&L_op_mul_i32,
                [op_shl_i32] = &&L_op_shl_i32, [op_shr_u32] = &&L_op_shr_u32,
                [op_shr_s32] = &&L_op_shr_s32,
                [op_and_32] = &&L_op_and_32, [op_or_32] = &&L_op_or_32, [op_xor_32] = &&L_op_xor_32,
                [op_sel_32] = &&L_op_sel_32,
                [op_eq_f32] = &&L_op_eq_f32, [op_lt_f32] = &&L_op_lt_f32, [op_le_f32] = &&L_op_le_f32,
                [op_eq_i32] = &&L_op_eq_i32, [op_lt_s32] = &&L_op_lt_s32, [op_le_s32] = &&L_op_le_s32,
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
                [op_pack] = &&L_op_pack,
                [SW_DONE] = &&L_SW_DONE,

                // Register variant labels.
#define BINARY_LABELS(name, rt, pt) \
                [op_r_##name##_mm] = &&L_op_r_##name##_mm, \
                [op_r_##name##_rm] = &&L_op_r_##name##_rm, \
                [op_m_##name##_rm] = &&L_op_m_##name##_rm, \
                [op_r_##name##_mr] = &&L_op_r_##name##_mr, \
                [op_m_##name##_mr] = &&L_op_m_##name##_mr, \
                [op_r_##name##_rr] = &&L_op_r_##name##_rr, \
                [op_m_##name##_rr] = &&L_op_m_##name##_rr,
                BINARY_OPS(BINARY_LABELS)
#undef BINARY_LABELS
#define UNARY_LABELS(name, rt, pt) \
                [op_r_##name##_m] = &&L_op_r_##name##_m, \
                [op_r_##name##_r] = &&L_op_r_##name##_r, \
                [op_m_##name##_r] = &&L_op_m_##name##_r,
                UNARY_OPS(UNARY_LABELS)
#undef UNARY_LABELS
#define IMM_LABELS(name, rt, pt) \
                [op_r_##name##_m] = &&L_op_r_##name##_m, \
                [op_r_##name##_r] = &&L_op_r_##name##_r, \
                [op_m_##name##_r] = &&L_op_m_##name##_r,
                IMM_OPS(IMM_LABELS)
#undef IMM_LABELS
                [op_r_imm_32] = &&L_op_r_imm_32,
                [op_r_x] = &&L_op_r_x,
                [op_r_y] = &&L_op_r_y,
                [op_r_uniform_32] = &&L_op_r_uniform_32,
                [op_r_pack_mm] = &&L_op_r_pack_mm, [op_r_pack_rm] = &&L_op_r_pack_rm,
                [op_m_pack_rm] = &&L_op_m_pack_rm, [op_r_pack_mr] = &&L_op_r_pack_mr,
                [op_m_pack_mr] = &&L_op_m_pack_mr, [op_r_pack_rr] = &&L_op_r_pack_rr,
                [op_m_pack_rr] = &&L_op_m_pack_rr,
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
                CASE(op_x) {
                    I32 seq;
                    __builtin_memcpy(&seq, iota, sizeof seq);
                    v->i32 = seq + (end - K);
                } NEXT;
                CASE(op_y) v->i32 = (I32){0} + row; NEXT;

                CASE(op_uniform_32) {
                    assert(buf[ip->ptr].row_bytes == 0);
                    int32_t uni;
                    __builtin_memcpy(&uni, (int32_t const*)buf[ip->ptr].ptr + ip->x, sizeof uni);
                    v->i32 = (I32){0} + uni;
                } NEXT;

                CASE(op_load_16) {
                    void const     *base = (char*)buf[ip->ptr].ptr + (size_t)row * buf[ip->ptr].row_bytes;
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
                } NEXT;
                CASE(op_load_32) {
                    void const    *base = (char*)buf[ip->ptr].ptr + (size_t)row * buf[ip->ptr].row_bytes;
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
                } NEXT;
                CASE(op_store_16) {
                    void     *base = (char*)buf[ip->ptr].ptr + (size_t)row * buf[ip->ptr].row_bytes;
                    uint16_t *dst = (uint16_t*)base;
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
                    void    *base = (char*)buf[ip->ptr].ptr + (size_t)row * buf[ip->ptr].row_bytes;
                    int32_t *dst = (int32_t*)base;
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

                CASE(op_load_color) {
                    char const *src = (char const*)buf[ip->ptr].ptr + (size_t)row * buf[ip->ptr].row_bytes;
                    interp_load_color(v, &buf[ip->ptr], src, end - K, n - (end - K));
                    ip += 3; v += 3;
                } NEXT;
                CASE(op_store_color) {
                    char *dst = (char*)buf[ip->ptr].ptr + (size_t)row * buf[ip->ptr].row_bytes;
                    val cv[4] = { v[ip->x], v[ip->y], v[ip->z], v[ip->w] };
                    interp_store_color(cv, &buf[ip->ptr], dst, end - K, n - (end - K));
                } NEXT;

                CASE(op_gather_uniform_32) {
                    int const ix = v[ip->x].i32[0];
                    int const count = (int)(buf[ip->ptr].sz / 4);
                    if (ix < 0 || ix >= count) { v->i32 = (I32){0}; break; }
                    int32_t gval;
                    __builtin_memcpy(&gval, (char const*)buf[ip->ptr].ptr + 4 * ix, 4);
                    v->i32 = (I32){0} + gval;
                } NEXT;
                CASE(op_gather_16) {
                    I32 const ix = v[ip->x].i32;
                    int const count = (int)(buf[ip->ptr].sz / 2);
                    int const rem = n - (end - K);
                    v->u32 = (U32){0};
                    for (int ll = 0; ll < (rem < K ? rem : K); ll++) {
                        if (ix[ll] >= 0 && ix[ll] < count) {
                            uint16_t s;
                            __builtin_memcpy(&s, (char const*)buf[ip->ptr].ptr + 2 * ix[ll], 2);
                            __builtin_memcpy((char*)v + 2 * ll, &s, 2);
                        }
                    }
                } NEXT;
                CASE(op_gather_32) {
                    I32 const ix = v[ip->x].i32;
                    int const count = (int)(buf[ip->ptr].sz / 4);
                    int const rem = n - (end - K);
                    v->i32 = (I32){0};
                    for (int ll = 0; ll < (rem < K ? rem : K); ll++) {
                        if (ix[ll] >= 0 && ix[ll] < count) {
                            int32_t tmp;
                            __builtin_memcpy(&tmp, (char const*)buf[ip->ptr].ptr + 4 * ix[ll], 4);
                            v->i32[ll] = tmp;
                        }
                    }
                } NEXT;

                CASE(op_deref_ptr) {
                    char *base = (char*)buf[ip->ptr].ptr + (size_t)row * buf[ip->ptr].row_bytes;
                    void *derived;
                    ptrdiff_t ssz;
                    size_t drb;
                    __builtin_memcpy(&derived, base + ip->x,      sizeof derived);
                    __builtin_memcpy(&ssz,     base + ip->x + 8,  sizeof ssz);
                    __builtin_memcpy(&drb,     base + ip->x + 16, sizeof drb);
                    buf[ip->y].ptr       = derived;
                    buf[ip->y].sz        = ssz < 0 ? (size_t)-ssz : (size_t)ssz;
                    buf[ip->y].row_bytes = drb;
                } NEXT;

                CASE(op_f32_from_f16) { U16 h; __builtin_memcpy(&h, &v[ip->x], sizeof h); v->f32 = f16_to_f32(h); } NEXT;
                CASE(op_f16_from_f32) { U16 const h = f32_to_f16(v[ip->x].f32); v->u32 = (U32){0}; __builtin_memcpy(v, &h, sizeof h); } NEXT;
                CASE(op_i32_from_s16) {
                    U16 tmp; __builtin_memcpy(&tmp, &v[ip->x], sizeof tmp);
                    S16 stmp; __builtin_memcpy(&stmp, &tmp, sizeof tmp);
                    v->i32 = cast(I32, stmp);
                } NEXT;
                CASE(op_i32_from_u16) { U16 tmp; __builtin_memcpy(&tmp, &v[ip->x], sizeof tmp); v->u32 = cast(U32, tmp); } NEXT;
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
                CASE(op_sqrt_f32) v->f32 = vec_sqrt(v[ip->x].f32); NEXT;
                CASE(op_abs_f32) v->f32 = vec_abs(v[ip->x].f32); NEXT;
                CASE(op_neg_f32) v->f32 = -v[ip->x].f32; NEXT;
                CASE(op_round_f32) v->f32 = vec_round(v[ip->x].f32); NEXT;
                CASE(op_floor_f32) v->f32 = vec_floor(v[ip->x].f32); NEXT;
                CASE(op_ceil_f32)  v->f32 = vec_ceil(v[ip->x].f32);  NEXT;
                CASE(op_round_i32) v->i32 = cast(I32, vec_round(v[ip->x].f32)); NEXT;
                CASE(op_floor_i32) v->i32 = cast(I32, vec_floor(v[ip->x].f32)); NEXT;
                CASE(op_ceil_i32)  v->i32 = cast(I32, vec_ceil(v[ip->x].f32));  NEXT;

#if defined(__ARM_FEATURE_FMA) || defined(__FMA__)
                CASE(op_fma_f32) v->f32 = v[ip->z].f32 + v[ip->x].f32 * v[ip->y].f32; NEXT;
                CASE(op_fms_f32) v->f32 = v[ip->z].f32 - v[ip->x].f32 * v[ip->y].f32; NEXT;
#else
                CASE(op_fma_f32) {
                    F64 const x = cast(F64, v[ip->x].f32), y = cast(F64, v[ip->y].f32), z = cast(F64, v[ip->z].f32);
                    v->f32 = cast(F32, x * y + z);
                } NEXT;
                CASE(op_fms_f32) {
                    F64 const x = cast(F64, v[ip->x].f32), y = cast(F64, v[ip->y].f32), z = cast(F64, v[ip->z].f32);
                    v->f32 = cast(F32, z - x * y);
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
                CASE(op_sel_32)  v->i32 = (v[ip->x].i32 & v[ip->y].i32) | (~v[ip->x].i32 & v[ip->z].i32); NEXT;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
                CASE(op_eq_f32) v->i32 = (I32)(v[ip->x].f32 == v[ip->y].f32); NEXT;
#pragma clang diagnostic pop
                CASE(op_lt_f32) v->i32 = (I32)(v[ip->x].f32 <  v[ip->y].f32); NEXT;
                CASE(op_le_f32) v->i32 = (I32)(v[ip->x].f32 <= v[ip->y].f32); NEXT;
                CASE(op_eq_i32) v->i32 = (I32)(v[ip->x].i32 == v[ip->y].i32); NEXT;
                CASE(op_lt_s32) v->i32 = (I32)(v[ip->x].i32 <  v[ip->y].i32); NEXT;
                CASE(op_le_s32) v->i32 = (I32)(v[ip->x].i32 <= v[ip->y].i32); NEXT;
                CASE(op_lt_u32) v->i32 = (I32)(v[ip->x].u32 <  v[ip->y].u32); NEXT;
                CASE(op_le_u32) v->i32 = (I32)(v[ip->x].u32 <= v[ip->y].u32); NEXT;

                CASE(op_shl_i32_imm) { I32 const sh = (I32){0} + ip->y; v->i32 = v[ip->x].i32 << sh; } NEXT;
                CASE(op_shr_u32_imm) { U32 const sh = (U32){0} + (uint32_t)ip->y; v->u32 = v[ip->x].u32 >> sh; } NEXT;
                CASE(op_shr_s32_imm) { I32 const sh = (I32){0} + ip->y; v->i32 = v[ip->x].i32 >> sh; } NEXT;
                CASE(op_and_32_imm)  { U32 const m = (U32){0} + (uint32_t)ip->y; v->u32 = v[ip->x].u32 & m; } NEXT;
                CASE(op_or_32_imm)   { U32 const m = (U32){0} + (uint32_t)ip->y; v->u32 = v[ip->x].u32 | m; } NEXT;
                CASE(op_xor_32_imm)  { U32 const m = (U32){0} + (uint32_t)ip->y; v->u32 = v[ip->x].u32 ^ m; } NEXT;

                CASE(op_add_f32_imm) { F32_IMM; v->f32 = v[ip->x].f32 + imm; } NEXT;
                CASE(op_sub_f32_imm) { F32_IMM; v->f32 = v[ip->x].f32 - imm; } NEXT;
                CASE(op_mul_f32_imm) { F32_IMM; v->f32 = v[ip->x].f32 * imm; } NEXT;
                CASE(op_div_f32_imm) { F32_IMM; v->f32 = v[ip->x].f32 / imm; } NEXT;
                CASE(op_min_f32_imm) { F32_IMM; v->f32 = vec_min(v[ip->x].f32, imm); } NEXT;
                CASE(op_max_f32_imm) { F32_IMM; v->f32 = vec_max(v[ip->x].f32, imm); } NEXT;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
                CASE(op_eq_f32_imm)  { F32_IMM; v->i32 = (I32)(v[ip->x].f32 == imm); } NEXT;
#pragma clang diagnostic pop
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

                CASE(op_pack) {
                    I32 const sh = (I32){0} + ip->z;
                    v->u32 = v[ip->x].u32 | (U32)(v[ip->y].i32 << sh);
                } NEXT;

                // Register variant dispatch.
                // Binary: 7 variants per op (the existing case is the mm→m fallback).
#define BINARY_DISPATCH(name, rt, pt) \
                CASE(op_r_##name##_mm) acc.rt = v[ip->x].pt OP_##name v[ip->y].pt; NEXT; \
                CASE(op_r_##name##_rm) acc.rt = acc.pt       OP_##name v[ip->y].pt; NEXT; \
                CASE(op_m_##name##_rm) v->rt  = acc.pt       OP_##name v[ip->y].pt; NEXT; \
                CASE(op_r_##name##_mr) acc.rt = v[ip->x].pt OP_##name acc.pt;       NEXT; \
                CASE(op_m_##name##_mr) v->rt  = v[ip->x].pt OP_##name acc.pt;       NEXT; \
                CASE(op_r_##name##_rr) acc.rt = acc.pt       OP_##name acc.pt;       NEXT; \
                CASE(op_m_##name##_rr) v->rt  = acc.pt       OP_##name acc.pt;       NEXT;

                // Define the binary operator for each op.
#define OP_add_f32 +
#define OP_sub_f32 -
#define OP_mul_f32 *
#define OP_div_f32 /
#define OP_add_i32 +
#define OP_sub_i32 -
#define OP_mul_i32 *
#define OP_shl_i32 <<
#define OP_shr_u32 >>
#define OP_shr_s32 >>
#define OP_and_32  &
#define OP_or_32   |
#define OP_xor_32  ^
                // Arithmetic binary ops.
                BINARY_DISPATCH(add_f32, f32, f32)
                BINARY_DISPATCH(sub_f32, f32, f32)
                BINARY_DISPATCH(mul_f32, f32, f32)
                BINARY_DISPATCH(div_f32, f32, f32)
                BINARY_DISPATCH(add_i32, i32, i32)
                BINARY_DISPATCH(sub_i32, i32, i32)
                BINARY_DISPATCH(mul_i32, i32, i32)
                BINARY_DISPATCH(shl_i32, i32, i32)
                BINARY_DISPATCH(shr_u32, u32, u32)
                BINARY_DISPATCH(shr_s32, i32, i32)
                BINARY_DISPATCH(and_32,  i32, i32)
                BINARY_DISPATCH(or_32,   i32, i32)
                BINARY_DISPATCH(xor_32,  i32, i32)
#undef BINARY_DISPATCH

                // min/max/cmp need custom expansion (not simple operators).
#define MINMAX_DISPATCH(name, fn, rt, pt) \
                CASE(op_r_##name##_mm) acc.rt = fn(v[ip->x].pt, v[ip->y].pt); NEXT; \
                CASE(op_r_##name##_rm) acc.rt = fn(acc.pt,       v[ip->y].pt); NEXT; \
                CASE(op_m_##name##_rm) v->rt  = fn(acc.pt,       v[ip->y].pt); NEXT; \
                CASE(op_r_##name##_mr) acc.rt = fn(v[ip->x].pt, acc.pt);       NEXT; \
                CASE(op_m_##name##_mr) v->rt  = fn(v[ip->x].pt, acc.pt);       NEXT; \
                CASE(op_r_##name##_rr) acc.rt = fn(acc.pt,       acc.pt);       NEXT; \
                CASE(op_m_##name##_rr) v->rt  = fn(acc.pt,       acc.pt);       NEXT;
                MINMAX_DISPATCH(min_f32, vec_min, f32, f32)
                MINMAX_DISPATCH(max_f32, vec_max, f32, f32)
#undef MINMAX_DISPATCH

                // Comparison variants: result type (i32) differs from param type.
#define CMP_DISPATCH(name, op, rt, pt) \
                CASE(op_r_##name##_mm) acc.rt = (I32)(v[ip->x].pt op v[ip->y].pt); NEXT; \
                CASE(op_r_##name##_rm) acc.rt = (I32)(acc.pt       op v[ip->y].pt); NEXT; \
                CASE(op_m_##name##_rm) v->rt  = (I32)(acc.pt       op v[ip->y].pt); NEXT; \
                CASE(op_r_##name##_mr) acc.rt = (I32)(v[ip->x].pt op acc.pt);       NEXT; \
                CASE(op_m_##name##_mr) v->rt  = (I32)(v[ip->x].pt op acc.pt);       NEXT; \
                CASE(op_r_##name##_rr) acc.rt = (I32)(acc.pt       op acc.pt);       NEXT; \
                CASE(op_m_##name##_rr) v->rt  = (I32)(acc.pt       op acc.pt);       NEXT;
                CMP_DISPATCH(lt_f32, <,  i32, f32)
                CMP_DISPATCH(le_f32, <=, i32, f32)
                CMP_DISPATCH(eq_i32, ==, i32, i32)
                CMP_DISPATCH(lt_s32, <,  i32, i32)
                CMP_DISPATCH(le_s32, <=, i32, i32)
                CMP_DISPATCH(lt_u32, <,  i32, u32)
                CMP_DISPATCH(le_u32, <=, i32, u32)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
                CMP_DISPATCH(eq_f32, ==, i32, f32)
#pragma clang diagnostic pop
#undef CMP_DISPATCH

                // Unary op variants: 3 per op.
#define UNARY_DISPATCH(name, fn, rt, pt) \
                CASE(op_r_##name##_m) acc.rt = fn(v[ip->x].pt); NEXT; \
                CASE(op_r_##name##_r) acc.rt = fn(acc.pt);       NEXT; \
                CASE(op_m_##name##_r) v->rt  = fn(acc.pt);       NEXT;
                UNARY_DISPATCH(sqrt_f32,     vec_sqrt,  f32, f32)
                UNARY_DISPATCH(abs_f32,      vec_abs,   f32, f32)
                UNARY_DISPATCH(round_f32,    vec_round, f32, f32)
                UNARY_DISPATCH(floor_f32,    vec_floor, f32, f32)
                UNARY_DISPATCH(ceil_f32,     vec_ceil,  f32, f32)
#undef UNARY_DISPATCH
                // neg, convert, round_i32 etc need custom expansion.
#define UNARY3(name, expr_m, expr_r) \
                CASE(op_r_##name##_m) acc expr_m; NEXT; \
                CASE(op_r_##name##_r) acc expr_r; NEXT; \
                CASE(op_m_##name##_r) v[0] expr_r; NEXT;
                UNARY3(neg_f32,     .f32 = -v[ip->x].f32, .f32 = -acc.f32)
                UNARY3(round_i32,   .i32 = cast(I32, vec_round(v[ip->x].f32)), .i32 = cast(I32, vec_round(acc.f32)))
                UNARY3(floor_i32,   .i32 = cast(I32, vec_floor(v[ip->x].f32)), .i32 = cast(I32, vec_floor(acc.f32)))
                UNARY3(ceil_i32,    .i32 = cast(I32, vec_ceil(v[ip->x].f32)),  .i32 = cast(I32, vec_ceil(acc.f32)))
                UNARY3(f32_from_i32,.f32 = cast(F32, v[ip->x].i32),           .f32 = cast(F32, acc.i32))
                UNARY3(i32_from_f32,.i32 = cast(I32, v[ip->x].f32),           .i32 = cast(I32, acc.f32))
#undef UNARY3

                // _imm op variants: 3 per op, y is always the immediate.
#define IMM_DISPATCH_F(name, op) \
                CASE(op_r_##name##_m) { F32_IMM; acc.f32 = v[ip->x].f32 op imm; } NEXT; \
                CASE(op_r_##name##_r) { F32_IMM; acc.f32 = acc.f32       op imm; } NEXT; \
                CASE(op_m_##name##_r) { F32_IMM; v->f32  = acc.f32       op imm; } NEXT;
#define IMM_DISPATCH_I(name, op, rt, pt) \
                CASE(op_r_##name##_m) { I32 const imm = (I32){0} + ip->y; acc.rt = v[ip->x].pt op imm; } NEXT; \
                CASE(op_r_##name##_r) { I32 const imm = (I32){0} + ip->y; acc.rt = acc.pt       op imm; } NEXT; \
                CASE(op_m_##name##_r) { I32 const imm = (I32){0} + ip->y; v->rt  = acc.pt       op imm; } NEXT;
#define IMM_DISPATCH_U(name, op) \
                CASE(op_r_##name##_m) { U32 const imm = (U32){0} + (uint32_t)ip->y; acc.u32 = v[ip->x].u32 op imm; } NEXT; \
                CASE(op_r_##name##_r) { U32 const imm = (U32){0} + (uint32_t)ip->y; acc.u32 = acc.u32       op imm; } NEXT; \
                CASE(op_m_##name##_r) { U32 const imm = (U32){0} + (uint32_t)ip->y; v->u32  = acc.u32       op imm; } NEXT;
                IMM_DISPATCH_I(shl_i32_imm, <<, i32, i32)
                IMM_DISPATCH_I(shr_s32_imm, >>, i32, i32)
                IMM_DISPATCH_U(shr_u32_imm, >>)
                IMM_DISPATCH_U(and_32_imm,  &)
                IMM_DISPATCH_U(or_32_imm,   |)
                IMM_DISPATCH_U(xor_32_imm,  ^)
                IMM_DISPATCH_F(add_f32_imm, +)
                IMM_DISPATCH_F(sub_f32_imm, -)
                IMM_DISPATCH_F(mul_f32_imm, *)
                IMM_DISPATCH_F(div_f32_imm, /)
#define IMM_DISPATCH_FMIN(name, fn) \
                CASE(op_r_##name##_m) { F32_IMM; acc.f32 = fn(v[ip->x].f32, imm); } NEXT; \
                CASE(op_r_##name##_r) { F32_IMM; acc.f32 = fn(acc.f32,       imm); } NEXT; \
                CASE(op_m_##name##_r) { F32_IMM; v->f32  = fn(acc.f32,       imm); } NEXT;
                IMM_DISPATCH_FMIN(min_f32_imm, vec_min)
                IMM_DISPATCH_FMIN(max_f32_imm, vec_max)
#undef IMM_DISPATCH_FMIN
#define IMM_CMP_F(name, op) \
                CASE(op_r_##name##_m) { F32_IMM; acc.i32 = (I32)(v[ip->x].f32 op imm); } NEXT; \
                CASE(op_r_##name##_r) { F32_IMM; acc.i32 = (I32)(acc.f32       op imm); } NEXT; \
                CASE(op_m_##name##_r) { F32_IMM; v->i32  = (I32)(acc.f32       op imm); } NEXT;
#define IMM_CMP_I(name, op) \
                CASE(op_r_##name##_m) { I32 const imm = (I32){0} + ip->y; acc.i32 = (I32)(v[ip->x].i32 op imm); } NEXT; \
                CASE(op_r_##name##_r) { I32 const imm = (I32){0} + ip->y; acc.i32 = (I32)(acc.i32       op imm); } NEXT; \
                CASE(op_m_##name##_r) { I32 const imm = (I32){0} + ip->y; v->i32  = (I32)(acc.i32       op imm); } NEXT;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
                IMM_CMP_F(eq_f32_imm, ==)
#pragma clang diagnostic pop
                IMM_CMP_F(lt_f32_imm, <)
                IMM_CMP_F(le_f32_imm, <=)
                IMM_CMP_I(eq_i32_imm, ==)
                IMM_CMP_I(lt_s32_imm, <)
                IMM_CMP_I(le_s32_imm, <=)
                IMM_DISPATCH_I(add_i32_imm, +, i32, i32)
                IMM_DISPATCH_I(sub_i32_imm, -, i32, i32)
                IMM_DISPATCH_I(mul_i32_imm, *, i32, i32)
#undef IMM_DISPATCH_F
#undef IMM_DISPATCH_I
#undef IMM_DISPATCH_U
#undef IMM_CMP_F
#undef IMM_CMP_I

                // sel_32 register variants.
#define SEL(xv,yv,zv) (((xv).i32 & (yv).i32) | (~(xv).i32 & (zv).i32))
                CASE(op_r_sel_32_mm) acc.i32 = SEL(v[ip->x], v[ip->y], v[ip->z]); NEXT;
                CASE(op_r_sel_32_rm) acc.i32 = SEL(acc,       v[ip->y], v[ip->z]); NEXT;
                CASE(op_m_sel_32_rm) v->i32  = SEL(acc,       v[ip->y], v[ip->z]); NEXT;
#undef SEL

                // fma/fms register variants.
#if defined(__ARM_FEATURE_FMA) || defined(__FMA__)
#define FMA_OP(xv,yv,zv) ((zv) + (xv) * (yv))
#define FMS_OP(xv,yv,zv) ((zv) - (xv) * (yv))
#else
                // F64 intermediate for precision on non-FMA platforms.
                // (uses the existing F64 typedef from the non-variant fma cases)
#define FMA_OP(xv,yv,zv) cast(F32, cast(F64,(xv)) * cast(F64,(yv)) + cast(F64,(zv)))
#define FMS_OP(xv,yv,zv) cast(F32, cast(F64,(zv)) - cast(F64,(xv)) * cast(F64,(yv)))
#endif
                CASE(op_r_fma_f32_mmm) acc.f32 = FMA_OP(v[ip->x].f32, v[ip->y].f32, v[ip->z].f32); NEXT;
                CASE(op_r_fma_f32_mmr) acc.f32 = FMA_OP(v[ip->x].f32, v[ip->y].f32, acc.f32);       NEXT;
                CASE(op_m_fma_f32_mmr) v->f32  = FMA_OP(v[ip->x].f32, v[ip->y].f32, acc.f32);       NEXT;
                CASE(op_r_fms_f32_mmm) acc.f32 = FMS_OP(v[ip->x].f32, v[ip->y].f32, v[ip->z].f32); NEXT;
                CASE(op_r_fms_f32_mmr) acc.f32 = FMS_OP(v[ip->x].f32, v[ip->y].f32, acc.f32);       NEXT;
                CASE(op_m_fms_f32_mmr) v->f32  = FMS_OP(v[ip->x].f32, v[ip->y].f32, acc.f32);       NEXT;
#undef FMA_OP
#undef FMS_OP

                // pack register variants.
#define PACK_SH I32 const sh = (I32){0} + ip->z
#define PACK_OP(xv,yv) ((xv).u32 | (U32)((yv).i32 << sh))
                CASE(op_r_pack_mm) { PACK_SH; acc.u32 = PACK_OP(v[ip->x], v[ip->y]); } NEXT;
                CASE(op_r_pack_rm) { PACK_SH; acc.u32 = PACK_OP(acc,       v[ip->y]); } NEXT;
                CASE(op_m_pack_rm) { PACK_SH; v->u32  = PACK_OP(acc,       v[ip->y]); } NEXT;
                CASE(op_r_pack_mr) { PACK_SH; acc.u32 = PACK_OP(v[ip->x], acc);       } NEXT;
                CASE(op_m_pack_mr) { PACK_SH; v->u32  = PACK_OP(v[ip->x], acc);       } NEXT;
                CASE(op_r_pack_rr) { PACK_SH; acc.u32 = PACK_OP(acc,       acc);       } NEXT;
                CASE(op_m_pack_rr) { PACK_SH; v->u32  = PACK_OP(acc,       acc);       } NEXT;
#undef PACK_OP
#undef PACK_SH

                // Output-only register variants.
                CASE(op_r_imm_32) acc.i32 = (I32){0} + ip->x; NEXT;
                CASE(op_r_x) {
                    I32 seq;
                    __builtin_memcpy(&seq, iota, sizeof seq);
                    acc.i32 = seq + (end - K);
                } NEXT;
                CASE(op_r_y) acc.i32 = (I32){0} + row; NEXT;
                CASE(op_r_uniform_32) {
                    assert(buf[ip->x].row_bytes == 0);
                    int32_t uni;
                    __builtin_memcpy(&uni, (int32_t const*)buf[ip->x].ptr + ip->y, sizeof uni);
                    acc.i32 = (I32){0} + uni;
                } NEXT;
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
}

void umbra_interpreter_free(struct umbra_interpreter *p) {
    if (p) {
        free(p->buf);
        free(p->inst);
        free(p->v);
        free(p);
    }
}
