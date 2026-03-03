#include "umbra.h"
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
    op(eq_f16) { v->i16 = (I16)(v[ip->x].f16 == v[ip->y].f16); next; }
    op(ne_f16) { v->i16 = (I16)(v[ip->x].f16 != v[ip->y].f16); next; }
    #pragma clang diagnostic pop
    op(lt_f16) { v->i16 = (I16)(v[ip->x].f16 <  v[ip->y].f16); next; }
    op(le_f16) { v->i16 = (I16)(v[ip->x].f16 <= v[ip->y].f16); next; }
    op(gt_f16) { v->i16 = (I16)(v[ip->x].f16 >  v[ip->y].f16); next; }
    op(ge_f16) { v->i16 = (I16)(v[ip->x].f16 >= v[ip->y].f16); next; }

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
    #define a f16_to_f32(v[ip->x].u16)
    #define b f16_to_f32(v[ip->y].u16)
    op(eq_f16) { v->i16 = cast(I16, (I32)(a == b)); next; }
    op(ne_f16) { v->i16 = cast(I16, (I32)(a != b)); next; }
    #pragma clang diagnostic pop
    op(lt_f16) { v->i16 = cast(I16, (I32)(a <  b)); next; }
    op(le_f16) { v->i16 = cast(I16, (I32)(a <= b)); next; }
    op(gt_f16) { v->i16 = cast(I16, (I32)(a >  b)); next; }
    op(ge_f16) { v->i16 = cast(I16, (I32)(a >= b)); next; }
    #undef a
    #undef b
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

op( eq_i16) { v->i16 = (I16)(v[ip->x].i16 == v[ip->y].i16); next; }
op( ne_i16) { v->i16 = (I16)(v[ip->x].i16 != v[ip->y].i16); next; }
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

op( eq_i32) { v->i32 = (I32)(v[ip->x].i32 == v[ip->y].i32); next; }
op( ne_i32) { v->i32 = (I32)(v[ip->x].i32 != v[ip->y].i32); next; }
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

enum op {
    op_lane, op_imm_16, op_imm_32,
    op_load_16, op_load_32, op_store_16, op_store_32,

    op_add_f16, op_sub_f16, op_mul_f16, op_div_f16,
    op_min_f16, op_max_f16, op_sqrt_f16, op_fma_f16,
    op_add_f32, op_sub_f32, op_mul_f32, op_div_f32,
    op_min_f32, op_max_f32, op_sqrt_f32, op_fma_f32,

    op_add_i16, op_sub_i16, op_mul_i16,
    op_shl_i16, op_shr_u16, op_shr_s16,
    op_and_16, op_or_16, op_xor_16, op_sel_16,
    op_add_i32, op_sub_i32, op_mul_i32,
    op_shl_i32, op_shr_u32, op_shr_s32,
    op_and_32, op_or_32, op_xor_32, op_sel_32,

    op_f16_from_f32, op_f32_from_f16,
    op_f32_from_i32, op_i32_from_f32,

    op_eq_f16, op_ne_f16, op_lt_f16, op_le_f16, op_gt_f16, op_ge_f16,
    op_eq_f32, op_ne_f32, op_lt_f32, op_le_f32, op_gt_f32, op_ge_f32,
    op_eq_i16, op_ne_i16,
    op_lt_s16, op_le_s16, op_gt_s16, op_ge_s16,
    op_lt_u16, op_le_u16, op_gt_u16, op_ge_u16,
    op_eq_i32, op_ne_i32,
    op_lt_s32, op_le_s32, op_gt_s32, op_ge_s32,
    op_lt_u32, op_le_u32, op_gt_u32, op_ge_u32,
};

struct bb_inst {
    enum op op;
    int     x,y,z;
    union { int ptr; int immi; float immf; };
};

static struct umbra_interpreter* interp_from_inst(struct bb_inst const[], int);
static void interp_run(struct umbra_interpreter*, int, void*[]);
static void interp_free(struct umbra_interpreter*);


static _Bool is_store(enum op op) {
    return op == op_store_16 || op == op_store_32;
}

static int arity(enum op op) {
    switch (op) {
        case op_imm_16:
        case op_imm_32:
        case op_lane:
            return 0;

        case op_sqrt_f16: case op_sqrt_f32:
        case op_f32_from_i32: case op_i32_from_f32:
        case op_f16_from_f32: case op_f32_from_f16:
        case op_load_16: case op_load_32:
            return 1;

        case op_fma_f16: case op_fma_f32:
        case op_sel_16:  case op_sel_32:
            return 3;

        default:
            return 2;
    }
}

static _Bool is_16bit_result(enum op op) {
    switch (op) {
        case op_imm_16: case op_load_16: case op_store_16:
        case op_add_f16: case op_sub_f16: case op_mul_f16: case op_div_f16:
        case op_min_f16: case op_max_f16: case op_sqrt_f16: case op_fma_f16:
        case op_add_i16: case op_sub_i16: case op_mul_i16:
        case op_shl_i16: case op_shr_u16: case op_shr_s16:
        case op_and_16: case op_or_16: case op_xor_16: case op_sel_16:
        case op_eq_f16: case op_ne_f16: case op_lt_f16: case op_le_f16:
        case op_gt_f16: case op_ge_f16:
        case op_eq_i16: case op_ne_i16:
        case op_lt_s16: case op_le_s16: case op_gt_s16: case op_ge_s16:
        case op_lt_u16: case op_le_u16: case op_gt_u16: case op_ge_u16:
        case op_f16_from_f32:
            return 1;
        default:
            return 0;
    }
}

static int f32_bits(float f) { int i; __builtin_memcpy(&i, &f, 4); return i; }


static uint32_t fnv1a(void const *data, int len) {
    uint32_t h = 2166136261u;
    unsigned char const *p = data;
    for (int i = 0; i < len; i++) {
        h ^= p[i];
        __builtin_mul_overflow(h, 16777619u, &h);
    }
    return h;
}

static int umbra_optimize(struct bb_inst inst[], int insts) {
    // 1. DCE + compaction.
    int *uses = calloc((size_t)insts, sizeof *uses);
    for (int i = 0; i < insts; i++) {
        int const ar = arity(inst[i].op);
        if (is_store(inst[i].op)) { uses[i]++; }
        if (ar >= 1) { uses[inst[i].x]++; }
        if (ar >= 2) { uses[inst[i].y]++; }
        if (ar >= 3) { uses[inst[i].z]++; }
    }
    for (int i = insts; i --> 0;) {
        if (uses[i] == 0 && !is_store(inst[i].op)) {
            int const ar = arity(inst[i].op);
            if (ar >= 1) { uses[inst[i].x]--; }
            if (ar >= 2) { uses[inst[i].y]--; }
            if (ar >= 3) { uses[inst[i].z]--; }
        }
    }

    int *newix = calloc((size_t)insts, sizeof *newix);
    int live = 0;
    for (int i = 0; i < insts; i++) {
        if (uses[i] > 0 || is_store(inst[i].op)) {
            newix[i] = live++;
        }
    }
    live = 0;
    for (int i = 0; i < insts; i++) {
        if (uses[i] == 0 && !is_store(inst[i].op)) { continue; }
        inst[live] = inst[i];
        inst[live].x = newix[inst[i].x];
        inst[live].y = newix[inst[i].y];
        inst[live].z = newix[inst[i].z];
        live++;
    }
    free(newix);
    free(uses);

    // 2. Loop invariant hoisting: stable partition uniforms before varyings.
    {
        _Bool *varying = calloc((size_t)live, sizeof *varying);
        for (int i = 0; i < live; i++) {
            if (inst[i].op == op_lane) { varying[i] = 1; continue; }
            if (is_store(inst[i].op))     { varying[i] = 1; continue; }
            if (inst[i].op == op_load_16 || inst[i].op == op_load_32) {
                varying[i] = varying[inst[i].x];
                continue;
            }
            int const ar = arity(inst[i].op);
            if (ar >= 1 && varying[inst[i].x]) { varying[i] = 1; }
            if (ar >= 2 && varying[inst[i].y]) { varying[i] = 1; }
            if (ar >= 3 && varying[inst[i].z]) { varying[i] = 1; }
        }

        int *order = malloc((size_t)live * sizeof *order);
        int *back  = malloc((size_t)live * sizeof *back);
        int j = 0;
        for (int i = 0; i < live; i++) {
            if (!varying[i]) { back[j]=i; order[i]=j; j++; }
        }
        for (int i = 0; i < live; i++) {
            if ( varying[i]) { back[j]=i; order[i]=j; j++; }
        }

        struct bb_inst *tmp = malloc((size_t)live * sizeof *tmp);
        for (int i = 0; i < live; i++) {
            tmp[i] = inst[back[i]];
            int const ar = arity(tmp[i].op);
            if (ar >= 1) { tmp[i].x = order[inst[back[i]].x]; }
            if (ar >= 2) { tmp[i].y = order[inst[back[i]].y]; }
            if (ar >= 3) { tmp[i].z = order[inst[back[i]].z]; }
        }
        __builtin_memcpy(inst, tmp, (size_t)live * sizeof *inst);
        free(tmp);
        free(back);
        free(order);
        free(varying);
    }

    return live;
}

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

static struct umbra_interpreter* interp_from_inst(struct bb_inst const inst[], int insts) {
    // Find the split point P: number of leading uniform (non-varying) instructions.
    int P = 0;
    {
        _Bool *varying = calloc((size_t)insts, sizeof *varying);
        for (int i = 0; i < insts; i++) {
            if (inst[i].op == op_lane) { varying[i] = 1; }
            else if (is_store(inst[i].op)) { varying[i] = 1; }
            else if (inst[i].op == op_load_16 || inst[i].op == op_load_32) {
                varying[i] = varying[inst[i].x];
            } else {
                int const ar = arity(inst[i].op);
                if (ar >= 1 && varying[inst[i].x]) { varying[i] = 1; }
                if (ar >= 2 && varying[inst[i].y]) { varying[i] = 1; }
                if (ar >= 3 && varying[inst[i].z]) { varying[i] = 1; }
            }
        }
        while (P < insts && !varying[P]) { P++; }
        free(varying);
    }

    int const total = insts + 1;
    struct umbra_interpreter *p = malloc(sizeof *p);
    p->inst = malloc((size_t)total * sizeof *p->inst);
    p->v    = malloc((size_t)total * sizeof *p->v);
    #define emit(...) p->inst[i] = (struct interp_inst){__VA_ARGS__}
    for (int i = 0; i < insts; i++) {
        int const X = inst[i].x-i,
                  Y = inst[i].y-i,
                  Z = inst[i].z-i;
        switch (inst[i].op) {
            case op_imm_16: emit(.fn=imm_16, .x=inst[i].immi); break;
            case op_imm_32: emit(.fn=imm_32, .x=inst[i].immi); break;
            case op_lane:   emit(.fn=lane);                     break;

            case op_load_16:
                if (inst[inst[i].x].op == op_lane) {
                    emit(.fn=load_16, .x=inst[i].ptr);
                } else if (inst[inst[i].x].op == op_imm_32) {
                    emit(.fn=uni_16, .x=inst[i].ptr,
                                     .y=inst[inst[i].x].immi);
                } else {
                    emit(.fn=gather_16, .x=inst[i].ptr, .y=X);
                } break;
            case op_load_32:
                if (inst[inst[i].x].op == op_lane) {
                    emit(.fn=load_32, .x=inst[i].ptr);
                } else if (inst[inst[i].x].op == op_imm_32) {
                    emit(.fn=uni_32, .x=inst[i].ptr,
                                     .y=inst[inst[i].x].immi);
                } else {
                    emit(.fn=gather_32, .x=inst[i].ptr, .y=X);
                } break;

            case op_store_16:
                if (inst[inst[i].x].op == op_lane) {
                    emit(.fn=store_16, .x=inst[i].ptr, .y=Y);
                } else {
                    emit(.fn=scatter_16, .x=inst[i].ptr,
                                         .y=Y, .z=X);
                } break;
            case op_store_32:
                if (inst[inst[i].x].op == op_lane) {
                    emit(.fn=store_32, .x=inst[i].ptr, .y=Y);
                } else {
                    emit(.fn=scatter_32, .x=inst[i].ptr,
                                         .y=Y, .z=X);
                } break;

            default: switch (arity(inst[i].op)) {
                case 1: emit(.fn=fn[inst[i].op], .x=X);              break;
                case 2: emit(.fn=fn[inst[i].op], .x=X, .y=Y);       break;
                case 3: emit(.fn=fn[inst[i].op], .x=X, .y=Y, .z=Z); break;
            }
        }
    }
    #undef emit
    p->inst[insts] = (struct interp_inst){.fn=done};
    p->preamble = P;
    return p;
}

static void interp_run(struct umbra_interpreter *p, int n, void *ptr[]) {
    struct interp_inst const *start = p->inst;
    val                      *v     = p->v;
    int const P = p->preamble;

    int i = 0;
    while (i+K <= n) { start->fn(start,v,i+K,ptr); i+= K; start = p->inst+P; v = p->v+P; }
    while (i+1 <= n) { start->fn(start,v,i+1,ptr); i+= 1; start = p->inst+P; v = p->v+P; }
}

static void interp_free(struct umbra_interpreter *p) {
    free(p->inst);
    free(p->v);
    free(p);
}

// ── basic block builder ──────────────────────────────────────────────

struct umbra_basic_block {
    struct bb_inst *inst;
    int            *ht;
    int             insts, ht_mask;
};

// Grow inst[] and ht[] when insts is zero or a power of two.
// Capacity goes 0,1,2,4,8,...; ht is always 2x inst capacity.
static void bb_grow(struct umbra_basic_block *bb) {
    if (__builtin_popcount((unsigned)bb->insts) > 1) { return; }
    int inst_cap = bb->insts ? bb->insts * 2 : 1,
         ht_cap  = inst_cap * 2;
    bb->inst = realloc(bb->inst, (size_t)inst_cap * sizeof *bb->inst);
    bb->ht   = realloc(bb->ht,   (size_t) ht_cap  * sizeof *bb->ht);
    for (int i = 0; i < ht_cap; i++) { bb->ht[i] = -1; }
    bb->ht_mask = ht_cap - 1;
    for (int i = 0; i < bb->insts; i++) {
        if (is_store(bb->inst[i].op)) { continue; }
        struct { enum op op; int x, y, z, immi; } key = {
            bb->inst[i].op,
            bb->inst[i].x ? i + bb->inst[i].x : 0,
            bb->inst[i].y ? i + bb->inst[i].y : 0,
            bb->inst[i].z ? i + bb->inst[i].z : 0,
            bb->inst[i].immi,
        };
        uint32_t h = fnv1a(&key, sizeof key);
        for (;;) {
            int slot = (int)(h & (uint32_t)bb->ht_mask);
            if (bb->ht[slot] < 0) { bb->ht[slot] = i; break; }
            h++;
        }
    }
}

// Push a pre-filled instruction. Returns its absolute index.
// Stores skip dedup. Others get deduped via hash table.
static int bb_push(struct umbra_basic_block *bb, struct bb_inst inst) {
    bb_grow(bb);

    if (is_store(inst.op)) {
        int id = bb->insts++;
        bb->inst[id] = inst;
        return id;
    }

    // Convert relative→absolute for hashing.
    int cur = bb->insts;
    struct { enum op op; int x, y, z, immi; } key = {
        inst.op,
        inst.x ? cur + inst.x : 0,
        inst.y ? cur + inst.y : 0,
        inst.z ? cur + inst.z : 0,
        inst.immi,
    };
    uint32_t h = fnv1a(&key, sizeof key);
    int mask = bb->ht_mask;
    for (;;) {
        int slot = (int)(h & (uint32_t)mask);
        if (bb->ht[slot] < 0) {
            int id = bb->insts++;
            bb->inst[id] = inst;
            bb->ht[slot] = id;
            return id;
        }
        int prev = bb->ht[slot];
        struct bb_inst *pi = &bb->inst[prev];
        struct { enum op op; int x, y, z, immi; } pkey = {
            pi->op,
            pi->x ? prev + pi->x : 0,
            pi->y ? prev + pi->y : 0,
            pi->z ? prev + pi->z : 0,
            pi->immi,
        };
        if (key.op == pkey.op && key.x == pkey.x && key.y == pkey.y
         && key.z == pkey.z && key.immi == pkey.immi) {
            return prev;
        }
        h++;
    }
}

struct umbra_basic_block* umbra_basic_block(void) {
    return calloc(1, sizeof(struct umbra_basic_block));
}

void umbra_basic_block_free(struct umbra_basic_block *bb) {
    free(bb->inst);
    free(bb->ht);
    free(bb);
}

// Make a relative instruction from absolute handle IDs.
static struct bb_inst bb_rel(int cur, enum op op, int x, int y, int z) {
    return (struct bb_inst){
        .op = op,
        .x  = x ? x - cur : 0,
        .y  = y ? y - cur : 0,
        .z  = z ? z - cur : 0,
    };
}

umbra_v32 umbra_lane(struct umbra_basic_block *bb) {
    struct bb_inst inst = {.op = op_lane};
    return (umbra_v32){bb_push(bb, inst)};
}

umbra_v16 umbra_imm_16(struct umbra_basic_block *bb, uint16_t bits) {
    struct bb_inst inst = {.op = op_imm_16, .immi = (int16_t)bits};
    return (umbra_v16){bb_push(bb, inst)};
}

umbra_v32 umbra_imm_32(struct umbra_basic_block *bb, uint32_t bits) {
    int immi;
    __builtin_memcpy(&immi, &bits, 4);
    struct bb_inst inst = {.op = op_imm_32, .immi = immi};
    return (umbra_v32){bb_push(bb, inst)};
}

umbra_v16 umbra_load_16(struct umbra_basic_block *bb, umbra_ptr src, umbra_v32 ix) {
    int cur = bb->insts;
    struct bb_inst inst = bb_rel(cur, op_load_16, ix.id, 0, 0);
    inst.ptr = src.ix;
    return (umbra_v16){bb_push(bb, inst)};
}

umbra_v32 umbra_load_32(struct umbra_basic_block *bb, umbra_ptr src, umbra_v32 ix) {
    int cur = bb->insts;
    struct bb_inst inst = bb_rel(cur, op_load_32, ix.id, 0, 0);
    inst.ptr = src.ix;
    return (umbra_v32){bb_push(bb, inst)};
}

void umbra_store_16(struct umbra_basic_block *bb, umbra_ptr dst, umbra_v32 ix, umbra_v16 val) {
    int cur = bb->insts;
    struct bb_inst inst = bb_rel(cur, op_store_16, ix.id, val.id, 0);
    inst.ptr = dst.ix;
    bb_push(bb, inst);
}

void umbra_store_32(struct umbra_basic_block *bb, umbra_ptr dst, umbra_v32 ix, umbra_v32 val) {
    int cur = bb->insts;
    struct bb_inst inst = bb_rel(cur, op_store_32, ix.id, val.id, 0);
    inst.ptr = dst.ix;
    bb_push(bb, inst);
}

// Helpers for builder-time const prop and strength reduction.
static _Bool bb_is_imm32(struct umbra_basic_block *bb, int id, int val) {
    return bb->inst[id].op == op_imm_32 && bb->inst[id].immi == val;
}
static _Bool bb_is_imm16(struct umbra_basic_block *bb, int id, int val) {
    return bb->inst[id].op == op_imm_16
        && (int16_t)bb->inst[id].immi == (int16_t)val;
}
static _Bool bb_is_imm(struct umbra_basic_block *bb, int id) {
    return bb->inst[id].op == op_imm_32 || bb->inst[id].op == op_imm_16;
}

// Const-prop: evaluate op(imm,...) → imm at build time.
static int bb_constprop(struct umbra_basic_block *bb, enum op op,
                        int ax, int ay, int az) {
    int ar = arity(op);
    if (ar == 0 || is_store(op) || op == op_load_16 || op == op_load_32) { return -1; }
    if (ar >= 1 && !bb_is_imm(bb, ax)) { return -1; }
    if (ar >= 2 && !bb_is_imm(bb, ay)) { return -1; }
    if (ar >= 3 && !bb_is_imm(bb, az)) { return -1; }

    _Bool wide = !is_16bit_result(op);
    struct bb_inst tmp[8];
    __builtin_memset(tmp, 0, sizeof tmp);
    int t = 0;
    int ox = 0, oy = 0, oz = 0;
    if (ar >= 1) { ox = t; tmp[t++] = bb->inst[ax]; }
    if (ar >= 2) { oy = t; tmp[t++] = bb->inst[ay]; }
    if (ar >= 3) { oz = t; tmp[t++] = bb->inst[az]; }
    int const op_slot = t;
    tmp[t] = (struct bb_inst){.op = op, .x = ox, .y = oy, .z = oz};
    t++;
    int const lane_slot = t;
    tmp[t++].op = op_lane;
    tmp[t].op  = wide ? op_store_32 : op_store_16;
    tmp[t].ptr = 0;
    tmp[t].x   = lane_slot;
    tmp[t].y   = op_slot;
    t++;

    struct umbra_interpreter *prog = interp_from_inst(tmp, t);
    int32_t result = 0;
    interp_run(prog, 1, (void*[]){&result});
    interp_free(prog);

    if (wide) {
        uint32_t bits;
        __builtin_memcpy(&bits, &result, 4);
        return umbra_imm_32(bb, bits).id;
    }
    return umbra_imm_16(bb, (uint16_t)(int16_t)result).id;
}

static void sort(int *a, int *b) {
    if (*a > *b) { int t = *a; *a = *b; *b = t; }
}

// Generic binary op builder: constprop, push.
static int bb_binop(struct umbra_basic_block *bb, enum op op, int x, int y) {
    int cp = bb_constprop(bb, op, x, y, 0);
    if (cp >= 0) { return cp; }
    return bb_push(bb, bb_rel(bb->insts, op, x, y, 0));
}

// Generic unary op builder.
static int bb_unop(struct umbra_basic_block *bb, enum op op, int x) {
    int cp = bb_constprop(bb, op, x, 0, 0);
    if (cp >= 0) { return cp; }
    return bb_push(bb, bb_rel(bb->insts, op, x, 0, 0));
}

// Generic ternary op builder.
static int bb_ternop(struct umbra_basic_block *bb, enum op op, int x, int y, int z) {
    int cp = bb_constprop(bb, op, x, y, z);
    if (cp >= 0) { return cp; }
    return bb_push(bb, bb_rel(bb->insts, op, x, y, z));
}

// ── f32 arithmetic ───────────────────────────────────────────────────

umbra_v32 umbra_add_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    sort(&a.id, &b.id);
    int x = a.id, y = b.id;
    // FMA peephole: add(mul(a,b), c) → fma(a,b,c)
    if (bb->inst[x].op == op_mul_f32) {
        int mx = x + bb->inst[x].x, my = x + bb->inst[x].y;
        return (umbra_v32){bb_ternop(bb, op_fma_f32, mx, my, y)};
    }
    if (bb->inst[y].op == op_mul_f32) {
        int mx = y + bb->inst[y].x, my = y + bb->inst[y].y;
        return (umbra_v32){bb_ternop(bb, op_fma_f32, mx, my, x)};
    }
    return (umbra_v32){bb_binop(bb, op_add_f32, x, y)};
}

umbra_v32 umbra_sub_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    if (bb_is_imm32(bb, b.id, 0)) { return a; }
    return (umbra_v32){bb_binop(bb, op_sub_f32, a.id, b.id)};
}

umbra_v32 umbra_mul_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    sort(&a.id, &b.id);
    if (bb_is_imm32(bb, a.id, f32_bits(1.0f))) { return b; }
    if (bb_is_imm32(bb, b.id, f32_bits(1.0f))) { return a; }
    return (umbra_v32){bb_binop(bb, op_mul_f32, a.id, b.id)};
}

umbra_v32 umbra_div_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    if (bb_is_imm32(bb, b.id, f32_bits(1.0f))) { return a; }
    return (umbra_v32){bb_binop(bb, op_div_f32, a.id, b.id)};
}

umbra_v32 umbra_min_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    sort(&a.id, &b.id);
    return (umbra_v32){bb_binop(bb, op_min_f32, a.id, b.id)};
}

umbra_v32 umbra_max_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    sort(&a.id, &b.id);
    return (umbra_v32){bb_binop(bb, op_max_f32, a.id, b.id)};
}

umbra_v32 umbra_sqrt_f32(struct umbra_basic_block *bb, umbra_v32 a) {
    return (umbra_v32){bb_unop(bb, op_sqrt_f32, a.id)};
}

// ── f16 arithmetic ───────────────────────────────────────────────────

umbra_v16 umbra_add_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    sort(&a.id, &b.id);
    int x = a.id, y = b.id;
    if (bb->inst[x].op == op_mul_f16) {
        int mx = x + bb->inst[x].x, my = x + bb->inst[x].y;
        return (umbra_v16){bb_ternop(bb, op_fma_f16, mx, my, y)};
    }
    if (bb->inst[y].op == op_mul_f16) {
        int mx = y + bb->inst[y].x, my = y + bb->inst[y].y;
        return (umbra_v16){bb_ternop(bb, op_fma_f16, mx, my, x)};
    }
    return (umbra_v16){bb_binop(bb, op_add_f16, x, y)};
}

umbra_v16 umbra_sub_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    if (bb_is_imm16(bb, b.id, 0)) { return a; }
    return (umbra_v16){bb_binop(bb, op_sub_f16, a.id, b.id)};
}

umbra_v16 umbra_mul_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    sort(&a.id, &b.id);
    if (bb_is_imm16(bb, a.id, 0x3c00)) { return b; }
    if (bb_is_imm16(bb, b.id, 0x3c00)) { return a; }
    return (umbra_v16){bb_binop(bb, op_mul_f16, a.id, b.id)};
}

umbra_v16 umbra_div_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    if (bb_is_imm16(bb, b.id, 0x3c00)) { return a; }
    return (umbra_v16){bb_binop(bb, op_div_f16, a.id, b.id)};
}

umbra_v16 umbra_min_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    sort(&a.id, &b.id);
    return (umbra_v16){bb_binop(bb, op_min_f16, a.id, b.id)};
}

umbra_v16 umbra_max_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    sort(&a.id, &b.id);
    return (umbra_v16){bb_binop(bb, op_max_f16, a.id, b.id)};
}

umbra_v16 umbra_sqrt_f16(struct umbra_basic_block *bb, umbra_v16 a) {
    return (umbra_v16){bb_unop(bb, op_sqrt_f16, a.id)};
}

// ── i32 arithmetic ───────────────────────────────────────────────────

umbra_v32 umbra_add_i32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    sort(&a.id, &b.id);
    if (bb_is_imm32(bb, a.id, 0)) { return b; }
    if (bb_is_imm32(bb, b.id, 0)) { return a; }
    return (umbra_v32){bb_binop(bb, op_add_i32, a.id, b.id)};
}

umbra_v32 umbra_sub_i32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    if (bb_is_imm32(bb, b.id, 0)) { return a; }
    if (a.id == b.id) { return umbra_imm_32(bb, 0); }
    return (umbra_v32){bb_binop(bb, op_sub_i32, a.id, b.id)};
}

umbra_v32 umbra_mul_i32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    sort(&a.id, &b.id);
    if (bb_is_imm32(bb, a.id, 1)) { return b; }
    if (bb_is_imm32(bb, b.id, 1)) { return a; }
    if (bb_is_imm32(bb, a.id, 0)) { return a; }
    if (bb_is_imm32(bb, b.id, 0)) { return b; }
    // mul by power-of-2 → shl
    if (bb->inst[a.id].op == op_imm_32 && bb->inst[a.id].immi > 0
        && (bb->inst[a.id].immi & (bb->inst[a.id].immi - 1)) == 0) {
        umbra_v32 shift = umbra_imm_32(bb, (uint32_t)__builtin_ctz(
            (unsigned)bb->inst[a.id].immi));
        return (umbra_v32){bb_binop(bb, op_shl_i32, b.id, shift.id)};
    }
    if (bb->inst[b.id].op == op_imm_32 && bb->inst[b.id].immi > 0
        && (bb->inst[b.id].immi & (bb->inst[b.id].immi - 1)) == 0) {
        umbra_v32 shift = umbra_imm_32(bb, (uint32_t)__builtin_ctz(
            (unsigned)bb->inst[b.id].immi));
        return (umbra_v32){bb_binop(bb, op_shl_i32, a.id, shift.id)};
    }
    return (umbra_v32){bb_binop(bb, op_mul_i32, a.id, b.id)};
}

// ── i16 arithmetic ───────────────────────────────────────────────────

umbra_v16 umbra_add_i16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    sort(&a.id, &b.id);
    if (bb_is_imm16(bb, a.id, 0)) { return b; }
    if (bb_is_imm16(bb, b.id, 0)) { return a; }
    return (umbra_v16){bb_binop(bb, op_add_i16, a.id, b.id)};
}

umbra_v16 umbra_sub_i16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    if (bb_is_imm16(bb, b.id, 0)) { return a; }
    if (a.id == b.id) { return umbra_imm_16(bb, 0); }
    return (umbra_v16){bb_binop(bb, op_sub_i16, a.id, b.id)};
}

umbra_v16 umbra_mul_i16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    sort(&a.id, &b.id);
    if (bb_is_imm16(bb, a.id, 1)) { return b; }
    if (bb_is_imm16(bb, b.id, 1)) { return a; }
    if (bb_is_imm16(bb, a.id, 0)) { return a; }
    if (bb_is_imm16(bb, b.id, 0)) { return b; }
    // mul by power-of-2 → shl
    {
        int16_t av = (int16_t)bb->inst[a.id].immi;
        if (bb->inst[a.id].op == op_imm_16 && av > 0 && (av & (av - 1)) == 0) {
            umbra_v16 shift = umbra_imm_16(bb, (uint16_t)__builtin_ctz((unsigned)(uint16_t)av));
            return (umbra_v16){bb_binop(bb, op_shl_i16, b.id, shift.id)};
        }
    }
    {
        int16_t bv = (int16_t)bb->inst[b.id].immi;
        if (bb->inst[b.id].op == op_imm_16 && bv > 0 && (bv & (bv - 1)) == 0) {
            umbra_v16 shift = umbra_imm_16(bb, (uint16_t)__builtin_ctz((unsigned)(uint16_t)bv));
            return (umbra_v16){bb_binop(bb, op_shl_i16, a.id, shift.id)};
        }
    }
    return (umbra_v16){bb_binop(bb, op_mul_i16, a.id, b.id)};
}

// ── shifts ───────────────────────────────────────────────────────────

umbra_v32 umbra_shl_i32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    if (bb_is_imm32(bb, b.id, 0)) { return a; }
    return (umbra_v32){bb_binop(bb, op_shl_i32, a.id, b.id)};
}
umbra_v32 umbra_shr_u32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    if (bb_is_imm32(bb, b.id, 0)) { return a; }
    return (umbra_v32){bb_binop(bb, op_shr_u32, a.id, b.id)};
}
umbra_v32 umbra_shr_s32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    if (bb_is_imm32(bb, b.id, 0)) { return a; }
    return (umbra_v32){bb_binop(bb, op_shr_s32, a.id, b.id)};
}

umbra_v16 umbra_shl_i16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    if (bb_is_imm16(bb, b.id, 0)) { return a; }
    return (umbra_v16){bb_binop(bb, op_shl_i16, a.id, b.id)};
}
umbra_v16 umbra_shr_u16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    if (bb_is_imm16(bb, b.id, 0)) { return a; }
    return (umbra_v16){bb_binop(bb, op_shr_u16, a.id, b.id)};
}
umbra_v16 umbra_shr_s16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    if (bb_is_imm16(bb, b.id, 0)) { return a; }
    return (umbra_v16){bb_binop(bb, op_shr_s16, a.id, b.id)};
}

// ── bitwise ──────────────────────────────────────────────────────────

umbra_v32 umbra_and_32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    sort(&a.id, &b.id);
    if (bb_is_imm32(bb, a.id, -1)) { return b; }
    if (bb_is_imm32(bb, b.id, -1)) { return a; }
    if (bb_is_imm32(bb, a.id,  0)) { return a; }
    if (bb_is_imm32(bb, b.id,  0)) { return b; }
    return (umbra_v32){bb_binop(bb, op_and_32, a.id, b.id)};
}
umbra_v32 umbra_or_32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    sort(&a.id, &b.id);
    if (bb_is_imm32(bb, a.id,  0)) { return b; }
    if (bb_is_imm32(bb, b.id,  0)) { return a; }
    if (bb_is_imm32(bb, a.id, -1)) { return a; }
    if (bb_is_imm32(bb, b.id, -1)) { return b; }
    return (umbra_v32){bb_binop(bb, op_or_32, a.id, b.id)};
}
umbra_v32 umbra_xor_32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    sort(&a.id, &b.id);
    if (bb_is_imm32(bb, a.id, 0)) { return b; }
    if (bb_is_imm32(bb, b.id, 0)) { return a; }
    if (a.id == b.id) { return umbra_imm_32(bb, 0); }
    return (umbra_v32){bb_binop(bb, op_xor_32, a.id, b.id)};
}
umbra_v32 umbra_sel_32(struct umbra_basic_block *bb, umbra_v32 c, umbra_v32 t, umbra_v32 f) {
    if (t.id == f.id) { return t; }
    if (bb_is_imm32(bb, c.id, -1)) { return t; }
    if (bb_is_imm32(bb, c.id,  0)) { return f; }
    return (umbra_v32){bb_ternop(bb, op_sel_32, c.id, t.id, f.id)};
}

umbra_v16 umbra_and_16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    sort(&a.id, &b.id);
    if (bb_is_imm16(bb, a.id, -1)) { return b; }
    if (bb_is_imm16(bb, b.id, -1)) { return a; }
    if (bb_is_imm16(bb, a.id,  0)) { return a; }
    if (bb_is_imm16(bb, b.id,  0)) { return b; }
    return (umbra_v16){bb_binop(bb, op_and_16, a.id, b.id)};
}
umbra_v16 umbra_or_16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    sort(&a.id, &b.id);
    if (bb_is_imm16(bb, a.id,  0)) { return b; }
    if (bb_is_imm16(bb, b.id,  0)) { return a; }
    if (bb_is_imm16(bb, a.id, -1)) { return a; }
    if (bb_is_imm16(bb, b.id, -1)) { return b; }
    return (umbra_v16){bb_binop(bb, op_or_16, a.id, b.id)};
}
umbra_v16 umbra_xor_16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    sort(&a.id, &b.id);
    if (bb_is_imm16(bb, a.id, 0)) { return b; }
    if (bb_is_imm16(bb, b.id, 0)) { return a; }
    if (a.id == b.id) { return umbra_imm_16(bb, 0); }
    return (umbra_v16){bb_binop(bb, op_xor_16, a.id, b.id)};
}
umbra_v16 umbra_sel_16(struct umbra_basic_block *bb, umbra_v16 c, umbra_v16 t, umbra_v16 f) {
    if (t.id == f.id) { return t; }
    if (bb_is_imm16(bb, c.id, -1)) { return t; }
    if (bb_is_imm16(bb, c.id,  0)) { return f; }
    return (umbra_v16){bb_ternop(bb, op_sel_16, c.id, t.id, f.id)};
}

// ── conversions ──────────────────────────────────────────────────────

umbra_v16 umbra_f16_from_f32(struct umbra_basic_block *bb, umbra_v32 a) {
    return (umbra_v16){bb_unop(bb, op_f16_from_f32, a.id)};
}
umbra_v32 umbra_f32_from_f16(struct umbra_basic_block *bb, umbra_v16 a) {
    return (umbra_v32){bb_unop(bb, op_f32_from_f16, a.id)};
}
umbra_v32 umbra_f32_from_i32(struct umbra_basic_block *bb, umbra_v32 a) {
    return (umbra_v32){bb_unop(bb, op_f32_from_i32, a.id)};
}
umbra_v32 umbra_i32_from_f32(struct umbra_basic_block *bb, umbra_v32 a) {
    return (umbra_v32){bb_unop(bb, op_i32_from_f32, a.id)};
}

// ── comparisons ──────────────────────────────────────────────────────

umbra_v16 umbra_eq_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { sort(&a.id, &b.id); return (umbra_v16){bb_binop(bb, op_eq_f16, a.id, b.id)}; }
umbra_v16 umbra_ne_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { sort(&a.id, &b.id); return (umbra_v16){bb_binop(bb, op_ne_f16, a.id, b.id)}; }
umbra_v16 umbra_lt_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){bb_binop(bb, op_lt_f16, a.id, b.id)}; }
umbra_v16 umbra_le_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){bb_binop(bb, op_le_f16, a.id, b.id)}; }
umbra_v16 umbra_gt_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){bb_binop(bb, op_gt_f16, a.id, b.id)}; }
umbra_v16 umbra_ge_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){bb_binop(bb, op_ge_f16, a.id, b.id)}; }

umbra_v32 umbra_eq_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { sort(&a.id, &b.id); return (umbra_v32){bb_binop(bb, op_eq_f32, a.id, b.id)}; }
umbra_v32 umbra_ne_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { sort(&a.id, &b.id); return (umbra_v32){bb_binop(bb, op_ne_f32, a.id, b.id)}; }
umbra_v32 umbra_lt_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){bb_binop(bb, op_lt_f32, a.id, b.id)}; }
umbra_v32 umbra_le_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){bb_binop(bb, op_le_f32, a.id, b.id)}; }
umbra_v32 umbra_gt_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){bb_binop(bb, op_gt_f32, a.id, b.id)}; }
umbra_v32 umbra_ge_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){bb_binop(bb, op_ge_f32, a.id, b.id)}; }

umbra_v16 umbra_eq_i16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { sort(&a.id, &b.id); return (umbra_v16){bb_binop(bb, op_eq_i16, a.id, b.id)}; }
umbra_v16 umbra_ne_i16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { sort(&a.id, &b.id); return (umbra_v16){bb_binop(bb, op_ne_i16, a.id, b.id)}; }
umbra_v32 umbra_eq_i32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { sort(&a.id, &b.id); return (umbra_v32){bb_binop(bb, op_eq_i32, a.id, b.id)}; }
umbra_v32 umbra_ne_i32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { sort(&a.id, &b.id); return (umbra_v32){bb_binop(bb, op_ne_i32, a.id, b.id)}; }

umbra_v16 umbra_lt_s16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){bb_binop(bb, op_lt_s16, a.id, b.id)}; }
umbra_v16 umbra_le_s16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){bb_binop(bb, op_le_s16, a.id, b.id)}; }
umbra_v16 umbra_gt_s16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){bb_binop(bb, op_gt_s16, a.id, b.id)}; }
umbra_v16 umbra_ge_s16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){bb_binop(bb, op_ge_s16, a.id, b.id)}; }
umbra_v32 umbra_lt_s32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){bb_binop(bb, op_lt_s32, a.id, b.id)}; }
umbra_v32 umbra_le_s32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){bb_binop(bb, op_le_s32, a.id, b.id)}; }
umbra_v32 umbra_gt_s32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){bb_binop(bb, op_gt_s32, a.id, b.id)}; }
umbra_v32 umbra_ge_s32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){bb_binop(bb, op_ge_s32, a.id, b.id)}; }

umbra_v16 umbra_lt_u16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){bb_binop(bb, op_lt_u16, a.id, b.id)}; }
umbra_v16 umbra_le_u16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){bb_binop(bb, op_le_u16, a.id, b.id)}; }
umbra_v16 umbra_gt_u16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){bb_binop(bb, op_gt_u16, a.id, b.id)}; }
umbra_v16 umbra_ge_u16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){bb_binop(bb, op_ge_u16, a.id, b.id)}; }
umbra_v32 umbra_lt_u32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){bb_binop(bb, op_lt_u32, a.id, b.id)}; }
umbra_v32 umbra_le_u32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){bb_binop(bb, op_le_u32, a.id, b.id)}; }
umbra_v32 umbra_gt_u32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){bb_binop(bb, op_gt_u32, a.id, b.id)}; }
umbra_v32 umbra_ge_u32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){bb_binop(bb, op_ge_u32, a.id, b.id)}; }

struct umbra_interpreter* umbra_interpreter(struct umbra_basic_block const *bb) {
    // Copy and convert relative → absolute.
    int insts = bb->insts;
    struct bb_inst *copy = malloc((size_t)insts * sizeof *copy);
    __builtin_memcpy(copy, bb->inst, (size_t)insts * sizeof *copy);
    for (int i = 0; i < insts; i++) {
        if (copy[i].x) { copy[i].x += i; }
        if (copy[i].y) { copy[i].y += i; }
        if (copy[i].z) { copy[i].z += i; }
    }

    // DCE + hoisting. Canonicalize/constprop/strength-reduction already done by builder.
    int live = umbra_optimize(copy, insts);

    struct umbra_interpreter *p = interp_from_inst(copy, live);
    free(copy);
    return p;
}

void umbra_interpreter_run(struct umbra_interpreter *p, int n, void *ptr[]) {
    interp_run(p, n, ptr);
}

void umbra_interpreter_free(struct umbra_interpreter *p) {
    interp_free(p);
}
