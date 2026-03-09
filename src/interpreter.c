#include "../umbra.h"
#include "bb.h"
#include <stdint.h>
#include <stdlib.h>

#define cast(T,v) __builtin_convertvector(v,T)

#define K 8
typedef  int16_t I16 __attribute__((vector_size(K*2)));
typedef  int32_t I32 __attribute__((vector_size(K*4)));
typedef uint16_t U16 __attribute__((vector_size(K*2)));
typedef uint32_t U32 __attribute__((vector_size(K*4)));
typedef    float F32 __attribute__((vector_size(K*4)));

#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC) || defined(__F16C__)
    typedef _Float16 F16 __attribute__((vector_size(K*2)));
#endif

typedef union {
    I16 i16;
    I32 i32;
    U16 u16;
    U32 u32;
#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
    F16 f16;
#endif
    F32 f32;
} val;

struct interp_inst;
typedef int (*Fn)(struct interp_inst const *ip, val *v, int end, void* ptr[]);
struct interp_inst {
    Fn  fn;
    int x,y,z,w;
};

struct umbra_interpreter {
    struct interp_inst *inst;
    val                *v;
    int                 preamble, nptr;
};

#define op(name) static int name(struct interp_inst const *ip, val *v, int end, void* ptr[])
#define next return ip[1].fn(ip+1, v+1, end, ptr)

op(imm_16) { v->i16 = (I16){0} + (int16_t)ip->x; next; }
op(imm_32) { v->i32 = (I32){0} +          ip->x; next; }

op(lane) {
    I32 const iota = {0,1,2,3,4,5,6,7};
    if (end & (K-1)) { v->i32 = iota + (end - 1); }
    else             { v->i32 = iota + (end - K); }
    next;
}

op(uni_16) {
    int16_t uni;
    __builtin_memcpy(&uni, (int16_t const*)ptr[ip->x] + ip->y, sizeof uni);
    v->i16 = (I16){0} + uni;
    next;
}
op(uni_32) {
    int32_t uni;
    __builtin_memcpy(&uni, (int32_t const*)ptr[ip->x] + ip->y, sizeof uni);
    v->i32 = (I32){0} + uni;
    next;
}

op(gather_16) {
    for (int l = 0; l < (end & (K-1) ? 1 : K); l++) {
        __builtin_memcpy((char*)&v->i16 + 2*l,
                         (char const*)ptr[ip->x] + 2*v[ip->y].i32[l], 2);
    }
    next;
}
op(gather_32) {
    for (int l = 0; l < (end & (K-1) ? 1 : K); l++) {
        __builtin_memcpy((char*)&v->i32 + 4*l,
                         (char const*)ptr[ip->x] + 4*v[ip->y].i32[l], 4);
    }
    next;
}

op(load_16) {
    int16_t const *src = ptr[ip->x];
    if (end & (K-1)) { __builtin_memcpy(v, src + end-1, 2  ); }
    else             { __builtin_memcpy(v, src + end-K, 2*K); }
    next;
}
op(load_32) {
    int32_t const *src = ptr[ip->x];
    if (end & (K-1)) { __builtin_memcpy(v, src + end-1, 4  ); }
    else             { __builtin_memcpy(v, src + end-K, 4*K); }
    next;
}

op(store_16) {
    int16_t *dst = ptr[ip->x];
    if (end & (K-1)) { __builtin_memcpy(dst + end-1, v + ip->y, 2  ); }
    else             { __builtin_memcpy(dst + end-K, v + ip->y, 2*K); }
    next;
}
op(store_32) {
    int32_t *dst = ptr[ip->x];
    if (end & (K-1)) { __builtin_memcpy(dst + end-1, v + ip->y, 4  ); }
    else             { __builtin_memcpy(dst + end-K, v + ip->y, 4*K); }
    next;
}

op(scatter_16) {
    for (int l = 0; l < (end & (K-1) ? 1 : K); l++) {
        __builtin_memcpy((char*)ptr[ip->x] + 2*v[ip->z].i32[l],
                         (char*)&v[ip->y].i16 + 2*l, 2);
    }
    next;
}
op(scatter_32) {
    for (int l = 0; l < (end & (K-1) ? 1 : K); l++) {
        __builtin_memcpy((char*)ptr[ip->x] + 4*v[ip->z].i32[l],
                         (char*)&v[ip->y].i32 + 4*l, 4);
    }
    next;
}

op(f32_from_i32) { v->f32 = cast(F32, v[ip->x].i32); next; }
op(i32_from_f32) { v->i32 = cast(I32, v[ip->x].f32); next; }
op(i16_from_i32) {
    for (int l = 0; l < K; l++) { v->i16[l] = (int16_t)v[ip->x].i32[l]; }
    next;
}
op(shr_narrow_u32) {
    for (int l = 0; l < K; l++) { v->u16[l] = (uint16_t)(v[ip->x].u32[l] >> (unsigned)ip->y); }
    next;
}
op(i32_from_i16) {
    for (int l = 0; l < K; l++) { v->i32[l] = (int32_t)v[ip->x].i16[l]; }
    next;
}

#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
    op(half_from_f32) { v->f16 = cast(F16, v[ip->x].f32); next; }
    op(half_from_i32) { v->f16 = cast(F16, v[ip->x].i32); next; }
    op(f32_from_half) { v->f32 = cast(F32, v[ip->x].f16); next; }
    op(i32_from_half) { v->i32 = cast(I32, v[ip->x].f16); next; }
    op(half_from_i16) { v->f16 = cast(F16, v[ip->x].i16); next; }
    op(i16_from_half) { v->i16 = cast(I16, v[ip->x].f16); next; }

    op( add_half) { v->f16 = v[ip->x].f16 + v[ip->y].f16                          ; next; }
    op( sub_half) { v->f16 = v[ip->x].f16 - v[ip->y].f16                          ; next; }
    op( mul_half) { v->f16 = v[ip->x].f16 * v[ip->y].f16                          ; next; }
    op( div_half) { v->f16 = v[ip->x].f16 / v[ip->y].f16                          ; next; }
    op( fma_half) { v->f16 = v[ip->x].f16 * v[ip->y].f16 + v[ip->z].f16           ; next; }
    op( fms_half) { v->f16 = v[ip->z].f16 - v[ip->x].f16 * v[ip->y].f16           ; next; }
    op( min_half) { v->f16 = __builtin_elementwise_min(v[ip->x].f16, v[ip->y].f16); next; }
    op( max_half) { v->f16 = __builtin_elementwise_max(v[ip->x].f16, v[ip->y].f16); next; }
    op(sqrt_half) { v->f16 = __builtin_elementwise_sqrt(v[ip->x].f16)             ; next; }

    // half bitwise: identical to i16 bitwise on native f16 (both use .i16)
    // dispatch table maps and_half->and_16, or_half->or_16, etc.

    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wfloat-equal"
    op(eq_half) { v->i16 = v[ip->x].f16 == v[ip->y].f16; next; }
    #pragma clang diagnostic pop
    op(lt_half) { v->i16 = v[ip->x].f16 <  v[ip->y].f16; next; }
    op(le_half) { v->i16 = v[ip->x].f16 <= v[ip->y].f16; next; }


#else

    #if defined(__F16C__)
        static F32 f16_to_f32(U16 h) {
            F16 tmp; __builtin_memcpy(&tmp, &h, sizeof h);
            return cast(F32, tmp);
        }
        static U16 f32_to_f16(F32 f) {
            F16 tmp = cast(F16, f);
            U16 result; __builtin_memcpy(&result, &tmp, sizeof tmp);
            return result;
        }
    #else
        static F32 f16_to_f32(U16 h) {
            U32 const sign = cast(U32, h >> 15) << 31;
            U32 const exp  = cast(U32, (h >> 10) & 0x1f);
            U32 const mant = cast(U32, h & 0x3ff);

            U32 const normal = sign | ((exp + 112) << 23) | (mant << 13);
            U32 const zero   = sign;
            U32 const infnan = sign | (0xffu << 23) | (mant << 13);

            U32 const is_zero   = (U32)(exp == 0);
            U32 const is_infnan = (U32)(exp == 31);

            U32 const bits = (is_zero & zero)
                     | (is_infnan & infnan)
                     | (~is_zero & ~is_infnan & normal);
            F32 result;
            __builtin_memcpy(&result, &bits, sizeof bits);
            return result;
        }

        static U16 f32_to_f16(F32 f) {
            U32 bits;
            __builtin_memcpy(&bits, &f, sizeof f);

            U32 const sign = (bits >> 31) << 15;
            I32 exp  = (I32)((bits >> 23) & 0xff) - 127 + 15;
            U32 mant = (bits >> 13) & 0x3ff;

            U32 const round_bit = (bits >> 12) & 1;
            U32 const sticky    = (U32)((bits & 0xfff) != 0);
            mant += round_bit & (sticky | (mant & 1));
            U32 const mant_overflow = mant >> 10;
            exp += (I32)mant_overflow;
            mant &= 0x3ff;

            U32 const normal        = sign | (U32)((U32)exp << 10) | mant;
            U32 const inf           = sign | 0x7c00;
            U32 const is_overflow   = (U32)(exp >= 31);
            U32 const is_underflow  = (U32)(exp <= 0);
            U32 const src_exp       = (bits >> 23) & 0xff;
            U32 const is_infnan     = (U32)(src_exp == 0xff);
            U32 const infnan        = sign | 0x7c00 | mant;

            U32 const result32 = (is_underflow & sign)
                          | (is_overflow & ~is_infnan & inf)
                          | (is_infnan & infnan)
                          | (~is_underflow & ~is_overflow & ~is_infnan & normal);
            return cast(U16, result32);
        }
    #endif

    op(imm_half) {
        int16_t imm = (int16_t)ip->x;
        __fp16 h; __builtin_memcpy(&h, &imm, 2);
        v->f32 = (F32){0} + (float)h;
        next;
    }

    op(load_half) {
        __fp16 const *src = ptr[ip->x];
        if (end & (K-1)) {
            v->f32 = (F32){0} + (float)src[end-1];
        } else {
            U16 tmp; __builtin_memcpy(&tmp, src + end-K, 2*K);
            v->f32 = f16_to_f32(tmp);
        }
        next;
    }

    op(store_half) {
        __fp16 *dst = ptr[ip->x];
        if (end & (K-1)) {
            U16 tmp = f32_to_f16(v[ip->y].f32);
            __builtin_memcpy(dst + end-1, &tmp, 2);
        } else {
            U16 tmp = f32_to_f16(v[ip->y].f32);
            __builtin_memcpy(dst + end-K, &tmp, 2*K);
        }
        next;
    }

    op(uni_half) {
        __fp16 h;
        __builtin_memcpy(&h, (__fp16 const*)ptr[ip->x] + ip->y, sizeof h);
        v->f32 = (F32){0} + (float)h;
        next;
    }

    op(gather_half) {
        for (int l = 0; l < (end & (K-1) ? 1 : K); l++) {
            __fp16 h;
            __builtin_memcpy(&h, (char const*)ptr[ip->x] + 2*v[ip->y].i32[l], 2);
            float f = (float)h;
            __builtin_memcpy((char*)&v->f32 + 4*l, &f, 4);
        }
        next;
    }

    op(scatter_half) {
        U16 tmp = f32_to_f16(v[ip->y].f32);
        for (int l = 0; l < (end & (K-1) ? 1 : K); l++) {
            __builtin_memcpy((char*)ptr[ip->x] + 2*v[ip->z].i32[l],
                             (char*)&tmp + 2*l, 2);
        }
        next;
    }

    op(half_from_f32) { v->f32 = v[ip->x].f32; next; }
    op(half_from_i32) { v->f32 = cast(F32, v[ip->x].i32); next; }
    op(f32_from_half) { v->f32 = v[ip->x].f32; next; }
    op(i32_from_half) { v->i32 = cast(I32, v[ip->x].f32); next; }
    op(half_from_i16) { v->f32 = cast(F32, v[ip->x].i16); next; }
    op(i16_from_half) { v->i16 = cast(I16, v[ip->x].f32); next; }

    op( add_half) { v->f32 = v[ip->x].f32 + v[ip->y].f32               ; next; }
    op( sub_half) { v->f32 = v[ip->x].f32 - v[ip->y].f32               ; next; }
    op( mul_half) { v->f32 = v[ip->x].f32 * v[ip->y].f32               ; next; }
    op( div_half) { v->f32 = v[ip->x].f32 / v[ip->y].f32               ; next; }
    op( fma_half) { v->f32 = v[ip->x].f32 * v[ip->y].f32 + v[ip->z].f32; next; }
    op( fms_half) { v->f32 = v[ip->z].f32 - v[ip->x].f32 * v[ip->y].f32; next; }
    op( min_half) { v->f32 = __builtin_elementwise_min(v[ip->x].f32, v[ip->y].f32); next; }
    op( max_half) { v->f32 = __builtin_elementwise_max(v[ip->x].f32, v[ip->y].f32); next; }
    op(sqrt_half) { v->f32 = __builtin_elementwise_sqrt(v[ip->x].f32)             ; next; }

    // half bitwise: identical to i32 bitwise on software f16 (both use .u32)
    // dispatch table maps and_half->and_32, or_half->or_32, etc.

    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wfloat-equal"
    op(eq_half) { v->u32 = cast(U32, v[ip->x].f32 == v[ip->y].f32); next; }
    #pragma clang diagnostic pop
    op(lt_half) { v->u32 = cast(U32, v[ip->x].f32 <  v[ip->y].f32); next; }
    op(le_half) { v->u32 = cast(U32, v[ip->x].f32 <= v[ip->y].f32); next; }

#endif

op( add_f32) { v->f32 = v[ip->x].f32 + v[ip->y].f32               ; next; }
op( sub_f32) { v->f32 = v[ip->x].f32 - v[ip->y].f32               ; next; }
op( mul_f32) { v->f32 = v[ip->x].f32 * v[ip->y].f32               ; next; }
op( div_f32) { v->f32 = v[ip->x].f32 / v[ip->y].f32               ; next; }
op( fma_f32) { v->f32 = v[ip->x].f32 * v[ip->y].f32 + v[ip->z].f32; next; }
op( fms_f32) { v->f32 = v[ip->z].f32 - v[ip->x].f32 * v[ip->y].f32; next; }
op(sqrt_f32) { v->f32 = __builtin_elementwise_sqrt(v[ip->x].f32)  ; next; }
op( min_f32) { v->f32 = __builtin_elementwise_min(v[ip->x].f32, v[ip->y].f32); next; }
op( max_f32) { v->f32 = __builtin_elementwise_max(v[ip->x].f32, v[ip->y].f32); next; }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
op(eq_f32) { v->i32 = (I32)(v[ip->x].f32 == v[ip->y].f32); next; }
#pragma clang diagnostic pop
op(lt_f32) { v->i32 = (I32)(v[ip->x].f32 <  v[ip->y].f32); next; }
op(le_f32) { v->i32 = (I32)(v[ip->x].f32 <= v[ip->y].f32); next; }


op(add_i16) { v->i16 = v[ip->x].i16 +  v[ip->y].i16; next; }
op(sub_i16) { v->i16 = v[ip->x].i16 -  v[ip->y].i16; next; }
op(mul_i16) { v->i16 = v[ip->x].i16 *  v[ip->y].i16; next; }
op(shl_i16) { v->i16 = v[ip->x].i16 << v[ip->y].i16; next; }
op(shr_u16) { v->u16 = v[ip->x].u16 >> v[ip->y].u16; next; }
op(shr_s16) { v->i16 = v[ip->x].i16 >> v[ip->y].i16; next; }
op(shl_i16_imm) { v->u16 = v[ip->x].u16 << (unsigned)ip->y; next; }
op(shr_u16_imm) { v->u16 = v[ip->x].u16 >> (unsigned)ip->y; next; }
op(shr_s16_imm) { v->i16 = v[ip->x].i16 >> ip->y; next; }
op(and_16)  { v->i16 = v[ip->x].i16 &  v[ip->y].i16; next; }
op( or_16)  { v->i16 = v[ip->x].i16 |  v[ip->y].i16; next; }
op(xor_16)  { v->i16 = v[ip->x].i16 ^  v[ip->y].i16; next; }
op(sel_16) {
    v->i16 = ( v[ip->x].i16 & v[ip->y].i16)
           | (~v[ip->x].i16 & v[ip->z].i16);
    next;
}

op(eq_i16) { v->i16 = (I16)(v[ip->x].i16 == v[ip->y].i16); next; }
op(lt_s16) { v->i16 = (I16)(v[ip->x].i16 <  v[ip->y].i16); next; }
op(le_s16) { v->i16 = (I16)(v[ip->x].i16 <= v[ip->y].i16); next; }

op(lt_u16) { v->i16 = (I16)(v[ip->x].u16 <  v[ip->y].u16); next; }
op(le_u16) { v->i16 = (I16)(v[ip->x].u16 <= v[ip->y].u16); next; }


op(add_i32) { v->i32 = v[ip->x].i32 +  v[ip->y].i32; next; }
op(sub_i32) { v->i32 = v[ip->x].i32 -  v[ip->y].i32; next; }
op(mul_i32) { v->i32 = v[ip->x].i32 *  v[ip->y].i32; next; }
op(shl_i32) { v->i32 = v[ip->x].i32 << v[ip->y].i32; next; }
op(shr_u32) { v->u32 = v[ip->x].u32 >> v[ip->y].u32; next; }
op(shr_s32) { v->i32 = v[ip->x].i32 >> v[ip->y].i32; next; }
op(shl_i32_imm) { v->u32 = v[ip->x].u32 << (unsigned)ip->y; next; }
op(shr_u32_imm) { v->u32 = v[ip->x].u32 >> (unsigned)ip->y; next; }
op(shr_s32_imm) { v->i32 = v[ip->x].i32 >> ip->y; next; }
op(and_32)  { v->i32 = v[ip->x].i32 &  v[ip->y].i32; next; }
op( or_32)  { v->i32 = v[ip->x].i32 |  v[ip->y].i32; next; }
op(xor_32)  { v->i32 = v[ip->x].i32 ^  v[ip->y].i32; next; }
op(sel_32) {
    v->i32 = ( v[ip->x].i32 & v[ip->y].i32)
           | (~v[ip->x].i32 & v[ip->z].i32);
    next;
}

op(eq_i32) { v->i32 = (I32)(v[ip->x].i32 == v[ip->y].i32); next; }
op(lt_s32) { v->i32 = (I32)(v[ip->x].i32 <  v[ip->y].i32); next; }
op(le_s32) { v->i32 = (I32)(v[ip->x].i32 <= v[ip->y].i32); next; }

op(lt_u32) { v->i32 = (I32)(v[ip->x].u32 <  v[ip->y].u32); next; }
op(le_u32) { v->i32 = (I32)(v[ip->x].u32 <= v[ip->y].u32); next; }



op(load_8x4) {
    uint8_t const *src = ptr[ip->x];
    if (end & (K-1)) {
        uint8_t const *px = src + (end-1)*4;
        for (int ch = 0; ch < 4; ch++) {
            v[ch].u16 = (U16){0};
            v[ch].u16[0] = px[ch];
        }
    } else {
        typedef uint8_t B32 __attribute__((vector_size(32)));
        typedef uint8_t B8  __attribute__((vector_size(K)));
        B32 all;
        __builtin_memcpy(&all, src + (end-K)*4, 32);
        B8 c0 = __builtin_shufflevector(all, all,  0, 4, 8,12,16,20,24,28);
        B8 c1 = __builtin_shufflevector(all, all,  1, 5, 9,13,17,21,25,29);
        B8 c2 = __builtin_shufflevector(all, all,  2, 6,10,14,18,22,26,30);
        B8 c3 = __builtin_shufflevector(all, all,  3, 7,11,15,19,23,27,31);
        v[0].u16 = __builtin_convertvector(c0, U16);
        v[1].u16 = __builtin_convertvector(c1, U16);
        v[2].u16 = __builtin_convertvector(c2, U16);
        v[3].u16 = __builtin_convertvector(c3, U16);
    }
    return ip[4].fn(ip+4, v+4, end, ptr);
}

op(store_8x4) {
    uint8_t *dst = ptr[ip->x];
    U16 r = v[ip->y].u16, g = v[ip->z].u16, b = v[ip->w].u16, a = v[(int)ip[1].x].u16;
    if (end & (K-1)) {
        dst[(end-1)*4+0] = (uint8_t)r[0];
        dst[(end-1)*4+1] = (uint8_t)g[0];
        dst[(end-1)*4+2] = (uint8_t)b[0];
        dst[(end-1)*4+3] = (uint8_t)a[0];
    } else {
        typedef uint8_t B8  __attribute__((vector_size(K)));
        B8 r8 = __builtin_convertvector(r, B8);
        B8 g8 = __builtin_convertvector(g, B8);
        B8 b8 = __builtin_convertvector(b, B8);
        B8 a8 = __builtin_convertvector(a, B8);
        typedef uint8_t B16 __attribute__((vector_size(16)));
        B16 rg = __builtin_shufflevector(r8, g8, 0,8, 1,9, 2,10, 3,11, 4,12, 5,13, 6,14, 7,15);
        B16 ba = __builtin_shufflevector(b8, a8, 0,8, 1,9, 2,10, 3,11, 4,12, 5,13, 6,14, 7,15);
        typedef uint8_t B32 __attribute__((vector_size(32)));
        B32 rgba = __builtin_shufflevector(rg, ba,
             0, 1,16,17,  2, 3,18,19,  4, 5,20,21,  6, 7,22,23,
             8, 9,24,25, 10,11,26,27, 12,13,28,29, 14,15,30,31);
        __builtin_memcpy(dst + (end-K)*4, &rgba, 32);
    }
    return ip[2].fn(ip+2, v+2, end, ptr);
}

op(done) { (void)ip; (void)v; (void)end; (void)ptr; return 0; }

#undef next
#undef op

static Fn const fn[] = {
    [op_add_f32] =  add_f32, [op_sub_f32] =  sub_f32,
    [op_mul_f32] =  mul_f32, [op_div_f32] =  div_f32,
    [op_min_f32] =  min_f32, [op_max_f32] =  max_f32,
    [op_sqrt_f32] = sqrt_f32, [op_fma_f32] = fma_f32, [op_fms_f32] = fms_f32,

    [op_add_i16] = add_i16, [op_sub_i16] = sub_i16,
    [op_mul_i16] = mul_i16,
    [op_shl_i16] = shl_i16, [op_shr_u16] = shr_u16,
    [op_shr_s16] = shr_s16,
    [op_shl_i16_imm] = shl_i16_imm, [op_shr_u16_imm] = shr_u16_imm,
    [op_shr_s16_imm] = shr_s16_imm,
    [op_and_16] = and_16, [op_or_16]  =  or_16,
    [op_xor_16] = xor_16, [op_sel_16] = sel_16,

    [op_add_i32] = add_i32, [op_sub_i32] = sub_i32,
    [op_mul_i32] = mul_i32,
    [op_shl_i32] = shl_i32, [op_shr_u32] = shr_u32,
    [op_shr_s32] = shr_s32,
    [op_shl_i32_imm] = shl_i32_imm, [op_shr_u32_imm] = shr_u32_imm,
    [op_shr_s32_imm] = shr_s32_imm,
    [op_and_32] = and_32, [op_or_32]  =  or_32,
    [op_xor_32] = xor_32, [op_sel_32] = sel_32,

    [op_f32_from_i32] = f32_from_i32,
    [op_i32_from_f32] = i32_from_f32,

    [op_eq_f32] = eq_f32,
    [op_lt_f32] = lt_f32, [op_le_f32] = le_f32,

    [op_eq_i16] = eq_i16,
    [op_lt_s16] = lt_s16, [op_le_s16] = le_s16,
    [op_lt_u16] = lt_u16, [op_le_u16] = le_u16,

    [op_eq_i32] = eq_i32,
    [op_lt_s32] = lt_s32, [op_le_s32] = le_s32,
    [op_lt_u32] = lt_u32, [op_le_u32] = le_u32,

    [op_half_from_f32] = half_from_f32, [op_half_from_i32] = half_from_i32,
    [op_half_from_i16] = half_from_i16, [op_i16_from_half] = i16_from_half,
    [op_f32_from_half] = f32_from_half, [op_i32_from_half] = i32_from_half,
    [op_i16_from_i32] = i16_from_i32, [op_shr_narrow_u32] = shr_narrow_u32,
    [op_i32_from_i16] = i32_from_i16,
    [op_load_8x4] = load_8x4,
    [op_store_8x4] = store_8x4,
    [op_add_half] =  add_half, [op_sub_half] =  sub_half,
    [op_mul_half] =  mul_half, [op_div_half] =  div_half,
    [op_min_half] =  min_half, [op_max_half] =  max_half,
    [op_sqrt_half] = sqrt_half, [op_fma_half] = fma_half, [op_fms_half] = fms_half,
#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
    // native f16: half bitwise is identical to i16 bitwise
    [op_and_half] = and_16, [op_or_half]  =  or_16,
    [op_xor_half] = xor_16, [op_sel_half] = sel_16,
#else
    // software f16: half promoted to f32, bitwise is identical to i32 bitwise
    [op_and_half] = and_32, [op_or_half]  =  or_32,
    [op_xor_half] = xor_32, [op_sel_half] = sel_32,
#endif
    [op_eq_half] = eq_half,
    [op_lt_half] = lt_half, [op_le_half] = le_half,
};

struct umbra_interpreter* umbra_interpreter(struct umbra_basic_block const *bb) {
    int *id = calloc((size_t)bb->insts, sizeof *id);

    struct umbra_interpreter *p = malloc(sizeof *p);
    int num_insts = bb->insts + 1;  // +1 for trailing done sentinel
    for (int i = 0; i < bb->insts; i++) {
        if (bb->inst[i].op == op_store_8x4) num_insts += 1;  // uses 2 slots (inst + overflow)
#if !defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
        if (bb->inst[i].op == op_half_from_f32 || bb->inst[i].op == op_f32_from_half) num_insts--;
#endif
    }
    p->inst = malloc((size_t)num_insts * sizeof *p->inst);
    p->v    = malloc((size_t)num_insts * sizeof *p->v);

    int n = 0;
    #define emit(...) p->inst[n] = (struct interp_inst){__VA_ARGS__}
    for (int pass = 0; pass < 2; pass++) {
        int const lo = pass ? bb->preamble : 0,
                  hi = pass ? bb->insts    : bb->preamble;
        if (pass) {
            p->preamble = n;
        }
        for (int i = lo; i < hi; i++) {
            {
                struct bb_inst const *inst = &bb->inst[i];
                int const X = id[inst->x] - n,
                          Y = id[inst->y] - n,
                          Z = id[inst->z] - n;
                switch (inst->op) {
                    case op_lane:   emit(.fn=lane);                 break;
                    case op_imm_16: emit(.fn=imm_16, .x=inst->imm); break;
                    case op_imm_32: emit(.fn=imm_32, .x=inst->imm); break;

                    case op_imm_half:
                    #if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
                        emit(.fn=imm_16, .x=inst->imm);
                    #else
                        emit(.fn=imm_half, .x=inst->imm);
                    #endif
                        break;

                    case op_uni_16:  emit(.fn=uni_16,  .x=inst->ptr, .y=bb->inst[inst->x].imm); break;
                    case op_uni_32:  emit(.fn=uni_32,  .x=inst->ptr, .y=bb->inst[inst->x].imm); break;
                    case op_load_16: emit(.fn=load_16, .x=inst->ptr); break;
                    case op_load_32: emit(.fn=load_32, .x=inst->ptr); break;
                    case op_gather_16: emit(.fn=gather_16, .x=inst->ptr, .y=X); break;
                    case op_gather_32: emit(.fn=gather_32, .x=inst->ptr, .y=X); break;
                    case op_store_16:   emit(.fn=store_16,   .x=inst->ptr, .y=Y); break;
                    case op_store_32:   emit(.fn=store_32,   .x=inst->ptr, .y=Y); break;
                    case op_scatter_16: emit(.fn=scatter_16, .x=inst->ptr, .y=Y, .z=X); break;
                    case op_scatter_32: emit(.fn=scatter_32, .x=inst->ptr, .y=Y, .z=X); break;

                    case op_uni_half:
                    #if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
                        emit(.fn=uni_16, .x=inst->ptr, .y=bb->inst[inst->x].imm);
                    #else
                        emit(.fn=uni_half, .x=inst->ptr, .y=bb->inst[inst->x].imm);
                    #endif
                        break;
                    case op_load_half:
                    #if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
                        emit(.fn=load_16, .x=inst->ptr);
                    #else
                        emit(.fn=load_half, .x=inst->ptr);
                    #endif
                        break;
                    case op_gather_half:
                    #if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
                        emit(.fn=gather_16, .x=inst->ptr, .y=X);
                    #else
                        emit(.fn=gather_half, .x=inst->ptr, .y=X);
                    #endif
                        break;
                    case op_store_half:
                    #if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
                        emit(.fn=store_16, .x=inst->ptr, .y=Y);
                    #else
                        emit(.fn=store_half, .x=inst->ptr, .y=Y);
                    #endif
                        break;
                    case op_scatter_half:
                    #if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
                        emit(.fn=scatter_16, .x=inst->ptr, .y=Y, .z=X);
                    #else
                        emit(.fn=scatter_half, .x=inst->ptr, .y=Y, .z=X);
                    #endif
                        break;

                    case op_load_8x4: {
                        if (inst->x) {
                            // continuation: alias to base's val slot + channel
                            id[i] = id[inst->x] + inst->imm;
                            continue;
                        }
                        emit(.fn=load_8x4, .x=inst->ptr);
                        id[i] = n;
                        n += 4;  // reserves 4 val slots
                        continue;  // skip the id[i]=n++ below
                    }

                    case op_store_8x4: {
                        emit(.fn=store_8x4,
                             .x=inst->ptr,
                             .y=id[inst->x] - n,
                             .z=id[inst->y] - n,
                             .w=id[inst->z] - n);
                        p->inst[n+1] = (struct interp_inst){.x=id[inst->w] - n};
                        n += 2;
                        continue;  // skip id[i]=n++ below
                    }

                    case op_shl_i32_imm: case op_shr_u32_imm: case op_shr_s32_imm:
                    case op_shl_i16_imm: case op_shr_u16_imm: case op_shr_s16_imm:
                    case op_shr_narrow_u32:
                        emit(.fn=fn[inst->op], .x=X, .y=inst->imm);
                        break;

                    case op_half_from_f32:
                    case op_f32_from_half:
                    #if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
                        emit(.fn=fn[inst->op], .x=X);
                        break;
                    #else
                        id[i] = id[inst->x];
                        continue;
                    #endif

                    default:
                        emit(.fn=fn[inst->op], .x=X, .y=Y, .z=Z);
                }
                id[i] = n++;
            }
        }
    }
    #undef emit
    p->inst[n] = (struct interp_inst){.fn=done};

    int max_ptr = -1;
    for (int i = 0; i < bb->insts; i++)
        if (has_ptr(bb->inst[i].op) && bb->inst[i].ptr > max_ptr)
            max_ptr = bb->inst[i].ptr;
    p->nptr = max_ptr + 1;

    free(id);
    return p;
}

void umbra_interpreter_run(struct umbra_interpreter *p, int n, umbra_buf buf[]) {
    void *ptr[16] = {0};
    for (int i = 0; i < p->nptr && i < 16; i++) ptr[i] = buf[i].ptr;
    struct interp_inst const *start = p->inst;
    val                      *v     = p->v;
    int const P = p->preamble;

    int i = 0;
    while (i+K <= n) { start->fn(start,v,i+K,ptr); i+= K; start = p->inst+P; v = p->v+P; }
    while (i+1 <= n) { start->fn(start,v,i+1,ptr); i+= 1; start = p->inst+P; v = p->v+P; }
}

void umbra_interpreter_free(struct umbra_interpreter *p) {
    free(p->inst);
    free(p->v);
    free(p);
}
