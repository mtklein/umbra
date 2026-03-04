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
#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC) || defined(__F16C__)
    F16 f16;
#endif
    F32 f32;
} val;

struct interp_inst;
typedef int (*Fn)(struct interp_inst const *ip, val *v, int end, void* ptr[]);
struct interp_inst {
    Fn  fn;
    int x,y,z,:32;
};

struct umbra_interpreter {
    struct interp_inst *inst;
    val                *v;
    int                 preamble,:32;
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

#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC) || defined(__F16C__)
    op(f16_from_f32) { v->f16 = cast(F16, v[ip->x].f32); next; }
    op(f32_from_f16) { v->f32 = cast(F32, v[ip->x].f16); next; }

    op( add_f16) { v->f16 = v[ip->x].f16 + v[ip->y].f16                          ; next; }
    op( sub_f16) { v->f16 = v[ip->x].f16 - v[ip->y].f16                          ; next; }
    op( mul_f16) { v->f16 = v[ip->x].f16 * v[ip->y].f16                          ; next; }
    op( div_f16) { v->f16 = v[ip->x].f16 / v[ip->y].f16                          ; next; }
    op( fma_f16) { v->f16 = v[ip->x].f16 * v[ip->y].f16 + v[ip->z].f16           ; next; }
    op( min_f16) { v->f16 = __builtin_elementwise_min(v[ip->x].f16, v[ip->y].f16); next; }
    op( max_f16) { v->f16 = __builtin_elementwise_max(v[ip->x].f16, v[ip->y].f16); next; }
    op(sqrt_f16) { v->f16 = __builtin_elementwise_sqrt(v[ip->x].f16)             ; next; }

    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wfloat-equal"
    op(eq_f16) { v->i16 = v[ip->x].f16 == v[ip->y].f16; next; }
    op(ne_f16) { v->i16 = v[ip->x].f16 != v[ip->y].f16; next; }
    #pragma clang diagnostic pop
    op(lt_f16) { v->i16 = v[ip->x].f16 <  v[ip->y].f16; next; }
    op(le_f16) { v->i16 = v[ip->x].f16 <= v[ip->y].f16; next; }
    op(gt_f16) { v->i16 = v[ip->x].f16 >  v[ip->y].f16; next; }
    op(ge_f16) { v->i16 = v[ip->x].f16 >= v[ip->y].f16; next; }

#else

    static F32 f16_to_f32(U16 h) {
        U32 const sign = cast(U32, h >> 15) << 31;
        U32 const exp  = cast(U32, (h >> 10) & 0x1f);
        U32 const mant = cast(U32, h & 0x3ff);

        U32 const normal = sign | ((exp + 112) << 23) | (mant << 13);
        U32 const zero   = sign;
        U32 const infnan = sign | (0xffu << 23) | (mant << 13);

        U32 const is_zero   = (U32)-(exp == 0);
        U32 const is_infnan = (U32)-(exp == 31);

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
        U32 const sticky    = (U32)-((bits & 0xfff) != 0);
        mant += round_bit & (sticky | (mant & 1));
        U32 const mant_overflow = mant >> 10;
        exp += (I32)mant_overflow;
        mant &= 0x3ff;

        U32 const normal        = sign | (U32)((U32)exp << 10) | mant;
        U32 const inf           = sign | 0x7c00;
        U32 const is_overflow   = (U32)-(exp >= 31);
        U32 const is_underflow  = (U32)-(exp <= 0);
        U32 const src_exp       = (bits >> 23) & 0xff;
        U32 const is_infnan     = (U32)-(src_exp == 0xff);
        U32 const infnan        = sign | 0x7c00 | mant;

        U32 const result32 = (is_underflow & sign)
                      | (is_overflow & ~is_infnan & inf)
                      | (is_infnan & infnan)
                      | (~is_underflow & ~is_overflow & ~is_infnan & normal);
        return cast(U16, result32);
    }

    op(f16_from_f32) { v->u16 = f32_to_f16(v[ip->x].f32); next; }
    op(f32_from_f16) { v->f32 = f16_to_f32(v[ip->x].u16); next; }

    op(add_f16) {
        v->u16 = f32_to_f16(f16_to_f32(v[ip->x].u16) + f16_to_f32(v[ip->y].u16));
        next;
    }
    op(sub_f16) {
        v->u16 = f32_to_f16(f16_to_f32(v[ip->x].u16) - f16_to_f32(v[ip->y].u16));
        next;
    }
    op(mul_f16) {
        v->u16 = f32_to_f16(f16_to_f32(v[ip->x].u16) * f16_to_f32(v[ip->y].u16));
        next;
    }
    op(div_f16) {
        v->u16 = f32_to_f16(f16_to_f32(v[ip->x].u16) / f16_to_f32(v[ip->y].u16));
        next;
    }
    op(fma_f16) {
        v->u16 = f32_to_f16(f16_to_f32(v[ip->x].u16) * f16_to_f32(v[ip->y].u16)
                                                     + f16_to_f32(v[ip->z].u16));
        next;
    }
    op(min_f16) {
        v->u16 = f32_to_f16(__builtin_elementwise_min(f16_to_f32(v[ip->x].u16),
                                                      f16_to_f32(v[ip->y].u16)));
        next;
    }
    op(max_f16) {
        v->u16 = f32_to_f16(__builtin_elementwise_max(f16_to_f32(v[ip->x].u16),
                                                      f16_to_f32(v[ip->y].u16)));
        next;
    }
    op(sqrt_f16) {
        v->u16 = f32_to_f16(__builtin_elementwise_sqrt(f16_to_f32(v[ip->x].u16)));
        next;
    }

    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wfloat-equal"
    op(eq_f16) { v->i16 = cast(I16, f16_to_f32(v[ip->x].u16) == f16_to_f32(v[ip->y].u16)); next; }
    op(ne_f16) { v->i16 = cast(I16, f16_to_f32(v[ip->x].u16) != f16_to_f32(v[ip->y].u16)); next; }
    #pragma clang diagnostic pop
    op(lt_f16) { v->i16 = cast(I16, f16_to_f32(v[ip->x].u16) <  f16_to_f32(v[ip->y].u16)); next; }
    op(le_f16) { v->i16 = cast(I16, f16_to_f32(v[ip->x].u16) <= f16_to_f32(v[ip->y].u16)); next; }
    op(gt_f16) { v->i16 = cast(I16, f16_to_f32(v[ip->x].u16) >  f16_to_f32(v[ip->y].u16)); next; }
    op(ge_f16) { v->i16 = cast(I16, f16_to_f32(v[ip->x].u16) >= f16_to_f32(v[ip->y].u16)); next; }
#endif

op( add_f32) { v->f32 = v[ip->x].f32 + v[ip->y].f32               ; next; }
op( sub_f32) { v->f32 = v[ip->x].f32 - v[ip->y].f32               ; next; }
op( mul_f32) { v->f32 = v[ip->x].f32 * v[ip->y].f32               ; next; }
op( div_f32) { v->f32 = v[ip->x].f32 / v[ip->y].f32               ; next; }
op( min_f32) { v->f32 = __builtin_elementwise_min(v[ip->x].f32, v[ip->y].f32); next; }
op( max_f32) { v->f32 = __builtin_elementwise_max(v[ip->x].f32, v[ip->y].f32); next; }
op(sqrt_f32) { v->f32 = __builtin_elementwise_sqrt(v[ip->x].f32)  ; next; }
op( fma_f32) { v->f32 = v[ip->x].f32 * v[ip->y].f32 + v[ip->z].f32; next; }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
op(eq_f32) { v->i32 = (I32)(v[ip->x].f32 == v[ip->y].f32); next; }
op(ne_f32) { v->i32 = (I32)(v[ip->x].f32 != v[ip->y].f32); next; }
#pragma clang diagnostic pop
op(lt_f32) { v->i32 = (I32)(v[ip->x].f32 <  v[ip->y].f32); next; }
op(le_f32) { v->i32 = (I32)(v[ip->x].f32 <= v[ip->y].f32); next; }
op(gt_f32) { v->i32 = (I32)(v[ip->x].f32 >  v[ip->y].f32); next; }
op(ge_f32) { v->i32 = (I32)(v[ip->x].f32 >= v[ip->y].f32); next; }

op(add_i16) { v->i16 = v[ip->x].i16 +  v[ip->y].i16; next; }
op(sub_i16) { v->i16 = v[ip->x].i16 -  v[ip->y].i16; next; }
op(mul_i16) { v->i16 = v[ip->x].i16 *  v[ip->y].i16; next; }
op(shl_i16) { v->i16 = v[ip->x].i16 << v[ip->y].i16; next; }
op(shr_u16) { v->u16 = v[ip->x].u16 >> v[ip->y].u16; next; }
op(shr_s16) { v->i16 = v[ip->x].i16 >> v[ip->y].i16; next; }
op(and_16) { v->i16 = v[ip->x].i16 &  v[ip->y].i16; next; }
op( or_16) { v->i16 = v[ip->x].i16 |  v[ip->y].i16; next; }
op(xor_16) { v->i16 = v[ip->x].i16 ^  v[ip->y].i16; next; }
op(sel_16) {
    v->i16 = ( v[ip->x].i16 & v[ip->y].i16)
           | (~v[ip->x].i16 & v[ip->z].i16);
    next;
}

op(eq_i16) { v->i16 = (I16)(v[ip->x].i16 == v[ip->y].i16); next; }
op(ne_i16) { v->i16 = (I16)(v[ip->x].i16 != v[ip->y].i16); next; }
op(lt_s16) { v->i16 = (I16)(v[ip->x].i16 <  v[ip->y].i16); next; }
op(le_s16) { v->i16 = (I16)(v[ip->x].i16 <= v[ip->y].i16); next; }
op(gt_s16) { v->i16 = (I16)(v[ip->x].i16 >  v[ip->y].i16); next; }
op(ge_s16) { v->i16 = (I16)(v[ip->x].i16 >= v[ip->y].i16); next; }
op(lt_u16) { v->i16 = (I16)(v[ip->x].u16 <  v[ip->y].u16); next; }
op(le_u16) { v->i16 = (I16)(v[ip->x].u16 <= v[ip->y].u16); next; }
op(gt_u16) { v->i16 = (I16)(v[ip->x].u16 >  v[ip->y].u16); next; }
op(ge_u16) { v->i16 = (I16)(v[ip->x].u16 >= v[ip->y].u16); next; }

op(add_i32) { v->i32 = v[ip->x].i32 +  v[ip->y].i32; next; }
op(sub_i32) { v->i32 = v[ip->x].i32 -  v[ip->y].i32; next; }
op(mul_i32) { v->i32 = v[ip->x].i32 *  v[ip->y].i32; next; }
op(shl_i32) { v->i32 = v[ip->x].i32 << v[ip->y].i32; next; }
op(shr_u32) { v->u32 = v[ip->x].u32 >> v[ip->y].u32; next; }
op(shr_s32) { v->i32 = v[ip->x].i32 >> v[ip->y].i32; next; }
op(and_32) { v->i32 = v[ip->x].i32 &  v[ip->y].i32; next; }
op( or_32) { v->i32 = v[ip->x].i32 |  v[ip->y].i32; next; }
op(xor_32) { v->i32 = v[ip->x].i32 ^  v[ip->y].i32; next; }
op(sel_32) {
    v->i32 = ( v[ip->x].i32 & v[ip->y].i32)
           | (~v[ip->x].i32 & v[ip->z].i32);
    next;
}

op(eq_i32) { v->i32 = (I32)(v[ip->x].i32 == v[ip->y].i32); next; }
op(ne_i32) { v->i32 = (I32)(v[ip->x].i32 != v[ip->y].i32); next; }
op(lt_s32) { v->i32 = (I32)(v[ip->x].i32 <  v[ip->y].i32); next; }
op(le_s32) { v->i32 = (I32)(v[ip->x].i32 <= v[ip->y].i32); next; }
op(gt_s32) { v->i32 = (I32)(v[ip->x].i32 >  v[ip->y].i32); next; }
op(ge_s32) { v->i32 = (I32)(v[ip->x].i32 >= v[ip->y].i32); next; }
op(lt_u32) { v->i32 = (I32)(v[ip->x].u32 <  v[ip->y].u32); next; }
op(le_u32) { v->i32 = (I32)(v[ip->x].u32 <= v[ip->y].u32); next; }
op(gt_u32) { v->i32 = (I32)(v[ip->x].u32 >  v[ip->y].u32); next; }
op(ge_u32) { v->i32 = (I32)(v[ip->x].u32 >= v[ip->y].u32); next; }

op(done) { (void)ip; (void)v; (void)end; (void)ptr; return 0; }

#undef next
#undef op

static Fn const fn[] = {
    [op_add_f16] =  add_f16, [op_sub_f16] =  sub_f16,
    [op_mul_f16] =  mul_f16, [op_div_f16] =  div_f16,
    [op_min_f16] =  min_f16, [op_max_f16] =  max_f16,
    [op_sqrt_f16] = sqrt_f16, [op_fma_f16] = fma_f16,

    [op_add_f32] =  add_f32, [op_sub_f32] =  sub_f32,
    [op_mul_f32] =  mul_f32, [op_div_f32] =  div_f32,
    [op_min_f32] =  min_f32, [op_max_f32] =  max_f32,
    [op_sqrt_f32] = sqrt_f32, [op_fma_f32] = fma_f32,

    [op_add_i16] = add_i16, [op_sub_i16] = sub_i16,
    [op_mul_i16] = mul_i16,
    [op_shl_i16] = shl_i16, [op_shr_u16] = shr_u16,
    [op_shr_s16] = shr_s16,
    [op_and_16] = and_16, [op_or_16]  =  or_16,
    [op_xor_16] = xor_16, [op_sel_16] = sel_16,

    [op_add_i32] = add_i32, [op_sub_i32] = sub_i32,
    [op_mul_i32] = mul_i32,
    [op_shl_i32] = shl_i32, [op_shr_u32] = shr_u32,
    [op_shr_s32] = shr_s32,
    [op_and_32] = and_32, [op_or_32]  =  or_32,
    [op_xor_32] = xor_32, [op_sel_32] = sel_32,

    [op_f32_from_i32] = f32_from_i32,
    [op_i32_from_f32] = i32_from_f32,
    [op_f16_from_f32] = f16_from_f32,
    [op_f32_from_f16] = f32_from_f16,

    [op_eq_f16] = eq_f16, [op_ne_f16] = ne_f16,
    [op_lt_f16] = lt_f16, [op_le_f16] = le_f16,
    [op_gt_f16] = gt_f16, [op_ge_f16] = ge_f16,

    [op_eq_f32] = eq_f32, [op_ne_f32] = ne_f32,
    [op_lt_f32] = lt_f32, [op_le_f32] = le_f32,
    [op_gt_f32] = gt_f32, [op_ge_f32] = ge_f32,

    [op_eq_i16] = eq_i16, [op_ne_i16] = ne_i16,
    [op_lt_s16] = lt_s16, [op_le_s16] = le_s16,
    [op_gt_s16] = gt_s16, [op_ge_s16] = ge_s16,
    [op_lt_u16] = lt_u16, [op_le_u16] = le_u16,
    [op_gt_u16] = gt_u16, [op_ge_u16] = ge_u16,

    [op_eq_i32] = eq_i32, [op_ne_i32] = ne_i32,
    [op_lt_s32] = lt_s32, [op_le_s32] = le_s32,
    [op_gt_s32] = gt_s32, [op_ge_s32] = ge_s32,
    [op_lt_u32] = lt_u32, [op_le_u32] = le_u32,
    [op_gt_u32] = gt_u32, [op_ge_u32] = ge_u32,
};

struct umbra_interpreter* umbra_interpreter(struct umbra_basic_block const *bb) {
    struct {
        _Bool live, varying, unused[2];
        int id;
    } *meta = calloc((size_t)bb->insts, sizeof *meta);

    int live = 0;
    for (int i = bb->insts; i --> 0;) {
        if (is_store(bb->inst[i].op)) {
            meta[i].live = 1;
        }
        if (meta[i].live) {
            meta[bb->inst[i].x].live = 1;
            meta[bb->inst[i].y].live = 1;
            meta[bb->inst[i].z].live = 1;
        }
        live += meta[i].live;
    }

    for (int i = 0; i < bb->insts; i++) {
        meta[i].varying = (bb->inst[i].op == op_lane)
                        | meta[bb->inst[i].x].varying
                        | meta[bb->inst[i].y].varying
                        | meta[bb->inst[i].z].varying;
    }

    struct umbra_interpreter *p = malloc(sizeof *p);
    p->inst = malloc((size_t)(live + 1) * sizeof *p->inst);
    p->v    = malloc((size_t)(live + 1) * sizeof *p->v);

    int n = 0;
    #define emit(...) p->inst[n] = (struct interp_inst){__VA_ARGS__}
    for (int varying = 0; varying < 2; varying++) {
        if (varying) {
            p->preamble = n;
        }
        for (int i = 0; i < bb->insts; i++) {
            if (meta[i].live && meta[i].varying == varying) {
                struct bb_inst const *inst = &bb->inst[i];
                int const X = meta[inst->x].id - n,
                          Y = meta[inst->y].id - n,
                          Z = meta[inst->z].id - n;
                switch (inst->op) {
                    case op_lane:   emit(.fn=lane);                 break;
                    case op_imm_16: emit(.fn=imm_16, .x=inst->imm); break;
                    case op_imm_32: emit(.fn=imm_32, .x=inst->imm); break;

                    case op_load_16:
                        if (bb->inst[inst->x].op == op_lane) {
                            emit(.fn=load_16, .x=inst->ptr);
                        } else if (bb->inst[inst->x].op == op_imm_32) {
                            emit(.fn=uni_16, .x=inst->ptr
                                           , .y=bb->inst[inst->x].imm);
                        } else {
                            emit(.fn=gather_16, .x=inst->ptr, .y=X);
                        } break;
                    case op_load_32:
                        if (bb->inst[inst->x].op == op_lane) {
                            emit(.fn=load_32, .x=inst->ptr);
                        } else if (bb->inst[inst->x].op == op_imm_32) {
                            emit(.fn=uni_32, .x=inst->ptr
                                           , .y=bb->inst[inst->x].imm);
                        } else {
                            emit(.fn=gather_32, .x=inst->ptr, .y=X);
                        } break;

                    case op_store_16:
                        if (bb->inst[inst->x].op == op_lane) {
                            emit(.fn=store_16, .x=inst->ptr, .y=Y);
                        } else {
                            emit(.fn=scatter_16, .x=inst->ptr, .y=Y, .z=X);
                        } break;
                    case op_store_32:
                        if (bb->inst[inst->x].op == op_lane) {
                            emit(.fn=store_32, .x=inst->ptr, .y=Y);
                        } else {
                            emit(.fn=scatter_32, .x=inst->ptr, .y=Y, .z=X);
                        } break;

                    default: emit(.fn=fn[inst->op], .x=X, .y=Y, .z=Z);
                }
                meta[i].id = n++;
            }
        }
    }
    #undef emit
    p->inst[n] = (struct interp_inst){.fn=done};
    free(meta);
    return p;
}

void umbra_interpreter_run(struct umbra_interpreter *p, int n, void *ptr[]) {
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
