#include "umbra.h"
#include <stdint.h>
#include <stdlib.h>

#define K 8
typedef  int16_t I16 __attribute__((vector_size(K*2)));
typedef  int32_t I32 __attribute__((vector_size(K*4)));
typedef uint16_t U16 __attribute__((vector_size(K*2)));
typedef uint32_t U32 __attribute__((vector_size(K*4)));
typedef    float F32 __attribute__((vector_size(K*4)));

#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC) || defined(__F16C__)
    typedef _Float16 F16 __attribute__((vector_size(K*2)));
#else
    #define cast(T,v) __builtin_convertvector(v,T)
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

struct inst {
    int  (*fn)(struct inst const *ip, val *v, int end, void* ptr[]);
    int    x,y,z;
    int    :32;
};

struct umbra_program {
    int         insts;
    int         :32;
    struct inst inst[];
};

#define op(name) static int name(struct inst const *ip, val *v, int end, void* ptr[])
#define next return ip[1].fn(ip+1, v+1, end, ptr)

op(imm_16) { v->i16 = (I16){0} + (int16_t)ip->x; next; }
op(imm_32) { v->i32 = (I32){0} +          ip->x; next; }

op(lane) {
    if (end & (K-1)) { v->i32 = (I32){0} + (end - 1); }
    else {
        I32 iota = {0,1,2,3,4,5,6,7};
        v->i32 = iota + (end - K);
    }
    next;
}

op(uni_16) {
    int16_t uni;
    __builtin_memcpy(&uni, (int16_t*)ptr[ip->x] + ip->y, sizeof uni);
    v->i16 = (I16){0} + uni;
    next;
}
op(uni_32) {
    int32_t uni;
    __builtin_memcpy(&uni, (int32_t*)ptr[ip->x] + ip->y, sizeof uni);
    v->i32 = (I32){0} + uni;
    next;
}

op(gather_16) {
    for (int l = 0; l < (end & (K-1) ? 1 : K); l++) {
        __builtin_memcpy((char*)&v->i16 + 2*l,
                         (char*)ptr[ip->x] + 2*v[ip->y].i32[l], 2);
    }
    next;
}
op(gather_32) {
    for (int l = 0; l < (end & (K-1) ? 1 : K); l++) {
        __builtin_memcpy((char*)&v->i32 + 4*l,
                         (char*)ptr[ip->x] + 4*v[ip->y].i32[l], 4);
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

op(f32_from_i32) { v->f32 = __builtin_convertvector(v[ip->x].i32, F32); next; }
op(i32_from_f32) { v->i32 = __builtin_convertvector(v[ip->x].f32, I32); next; }

#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC) || defined(__F16C__)
    op(f16_from_f32) { v->f16 = __builtin_convertvector(v[ip->x].f32, F16); next; }
    op(f32_from_f16) { v->f32 = __builtin_convertvector(v[ip->x].f16, F32); next; }

    op( add_f16) { v->f16 = v[ip->x].f16 + v[ip->y].f16               ; next; }
    op( sub_f16) { v->f16 = v[ip->x].f16 - v[ip->y].f16               ; next; }
    op( mul_f16) { v->f16 = v[ip->x].f16 * v[ip->y].f16               ; next; }
    op( div_f16) { v->f16 = v[ip->x].f16 / v[ip->y].f16               ; next; }
    op( min_f16) { v->f16 = __builtin_elementwise_min(v[ip->x].f16, v[ip->y].f16); next; }
    op( max_f16) { v->f16 = __builtin_elementwise_max(v[ip->x].f16, v[ip->y].f16); next; }
    op(sqrt_f16) { v->f16 = __builtin_elementwise_sqrt(v[ip->x].f16)  ; next; }
    op( fma_f16) { v->f16 = v[ip->x].f16 * v[ip->y].f16 + v[ip->z].f16; next; }

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
        U32 sign = cast(U32, h >> 15) << 31;
        U32 exp  = cast(U32, (h >> 10) & 0x1f);
        U32 mant = cast(U32, h & 0x3ff);

        U32 normal = sign | ((exp + 112) << 23) | (mant << 13);
        U32 zero   = sign;
        U32 infnan = sign | (0xffu << 23) | (mant << 13);

        U32 is_zero   = (U32)-(exp == 0);
        U32 is_infnan = (U32)-(exp == 31);

        U32 bits = (is_zero & zero)
                 | (is_infnan & infnan)
                 | (~is_zero & ~is_infnan & normal);
        F32 result;
        __builtin_memcpy(&result, &bits, sizeof bits);
        return result;
    }

    static U16 f32_to_f16(F32 f) {
        U32 bits;
        __builtin_memcpy(&bits, &f, sizeof f);

        U32 sign = (bits >> 31) << 15;
        I32 exp  = (I32)((bits >> 23) & 0xff) - 127 + 15;
        U32 mant = (bits >> 13) & 0x3ff;

        U32 round_bit = (bits >> 12) & 1;
        U32 sticky    = (U32)-((bits & 0xfff) != 0);
        mant += round_bit & (sticky | (mant & 1));
        U32 mant_overflow = mant >> 10;
        exp += (I32)mant_overflow;
        mant &= 0x3ff;

        U32 normal        = sign | (U32)((U32)exp << 10) | mant;
        U32 inf           = sign | 0x7c00;
        U32 is_overflow   = (U32)-(exp >= 31);
        U32 is_underflow  = (U32)-(exp <= 0);
        U32 src_exp       = (bits >> 23) & 0xff;
        U32 is_infnan     = (U32)-(src_exp == 0xff);
        U32 infnan        = sign | 0x7c00 | mant;

        U32 result32 = (is_underflow & sign)
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
    op(fma_f16) {
        v->u16 = f32_to_f16(f16_to_f32(v[ip->x].u16) * f16_to_f32(v[ip->y].u16)
                                                     + f16_to_f32(v[ip->z].u16));
        next;
    }

    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wfloat-equal"
    op(eq_f16) { v->i16 = cast(I16, (I32)(f16_to_f32(v[ip->x].u16) == f16_to_f32(v[ip->y].u16))); next; }
    op(ne_f16) { v->i16 = cast(I16, (I32)(f16_to_f32(v[ip->x].u16) != f16_to_f32(v[ip->y].u16))); next; }
    #pragma clang diagnostic pop
    op(lt_f16) { v->i16 = cast(I16, (I32)(f16_to_f32(v[ip->x].u16) <  f16_to_f32(v[ip->y].u16))); next; }
    op(le_f16) { v->i16 = cast(I16, (I32)(f16_to_f32(v[ip->x].u16) <= f16_to_f32(v[ip->y].u16))); next; }
    op(gt_f16) { v->i16 = cast(I16, (I32)(f16_to_f32(v[ip->x].u16) >  f16_to_f32(v[ip->y].u16))); next; }
    op(ge_f16) { v->i16 = cast(I16, (I32)(f16_to_f32(v[ip->x].u16) >= f16_to_f32(v[ip->y].u16))); next; }
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

typedef enum umbra_op Op;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"
static _Bool commutative(Op op) {
    switch (op) {
        case umbra_add_f16: case umbra_mul_f16: case umbra_min_f16: case umbra_max_f16:
        case umbra_add_f32: case umbra_mul_f32: case umbra_min_f32: case umbra_max_f32:
        case umbra_add_i16: case umbra_mul_i16:
        case umbra_and_16:  case umbra_or_16:   case umbra_xor_16:
        case umbra_add_i32: case umbra_mul_i32:
        case umbra_and_32:  case umbra_or_32:   case umbra_xor_32:
        case umbra_eq_f16:  case umbra_ne_f16:
        case umbra_eq_f32:  case umbra_ne_f32:
        case umbra_eq_i16:  case umbra_ne_i16:
        case umbra_eq_i32:  case umbra_ne_i32:
            return 1;
        default:
            return 0;
    }
}
#pragma clang diagnostic pop

static _Bool is_store(Op op) {
    return op == umbra_store_16 || op == umbra_store_32;
}

static uint32_t fnv1a(void const *data, int len) {
    uint32_t h = 2166136261u;
    unsigned char const *p = data;
    for (int i = 0; i < len; i++) {
        h ^= p[i];
        __builtin_mul_overflow(h, 16777619u, &h);
    }
    return h;
}

int umbra_optimize(struct umbra_inst inst[], int insts) {
    int *remap = malloc((size_t)insts * sizeof *remap);
    for (int i = 0; i < insts; i++) { remap[i] = i; }

    // 1. Canonicalize commutative operands: ensure x <= y.
    for (int i = 0; i < insts; i++) {
        if (commutative(inst[i].op) && inst[i].x > inst[i].y) {
            int t = inst[i].x;
            inst[i].x = inst[i].y;
            inst[i].y = t;
        }
    }

    // 2. GVN (Global Value Numbering).
    {
        int cap = 16;
        while (cap < insts * 2) { cap *= 2; }
        struct { int key; int val; } *ht = calloc((size_t)cap, sizeof *ht);
        // key=0 means empty, so we store instruction index + 1 as key.

        for (int i = 0; i < insts; i++) {
            // Resolve operands through remap.
            inst[i].x = remap[inst[i].x];
            inst[i].y = remap[inst[i].y];
            inst[i].z = remap[inst[i].z];

            if (is_store(inst[i].op)) { continue; }

            struct { Op op; int x, y, z, immi, ptr; } key = {
                inst[i].op, inst[i].x, inst[i].y, inst[i].z, inst[i].immi, inst[i].ptr
            };
            uint32_t h = fnv1a(&key, sizeof key);

            for (int mask = cap - 1;;) {
                int slot = (int)(h & (uint32_t)mask);
                if (ht[slot].key == 0) {
                    ht[slot].key = i + 1;
                    ht[slot].val = i;
                    break;
                }
                int prev = ht[slot].val;
                if (inst[prev].op == inst[i].op
                 && inst[prev].x == inst[i].x && inst[prev].y == inst[i].y
                 && inst[prev].z == inst[i].z && inst[prev].immi == inst[i].immi
                 && inst[prev].ptr == inst[i].ptr) {
                    remap[i] = prev;
                    break;
                }
                h++;
            }
        }
        free(ht);
    }

    // 3. FMA fusion: add(mul(a,b), c) → fma(a, b, c).
    for (int i = 0; i < insts; i++) {
        if (remap[i] != i) { continue; }
        Op fma_op, mul_op;
        if      (inst[i].op == umbra_add_f32) { fma_op = umbra_fma_f32; mul_op = umbra_mul_f32; }
        else if (inst[i].op == umbra_add_f16) { fma_op = umbra_fma_f16; mul_op = umbra_mul_f16; }
        else { continue; }

        int ax = remap[inst[i].x], ay = remap[inst[i].y];

        int mul = -1, other = -1;
        if (inst[ax].op == mul_op && remap[ax] == ax) { mul = ax; other = ay; }
        else if (inst[ay].op == mul_op && remap[ay] == ay) { mul = ay; other = ax; }
        if (mul < 0) { continue; }

        inst[i].op = fma_op;
        inst[i].x = inst[mul].x;
        inst[i].y = inst[mul].y;
        inst[i].z = other;
    }

    // 4. DCE + compaction.
    int *uses = calloc((size_t)insts, sizeof *uses);
    // Count all uses from non-deduped instructions.
    for (int i = 0; i < insts; i++) {
        if (remap[i] != i) { continue; }
        if (is_store(inst[i].op)) { uses[i]++; }  // stores are always live
        uses[remap[inst[i].x]]++;
        uses[remap[inst[i].y]]++;
        uses[remap[inst[i].z]]++;
    }
    // Walk backward: dead instructions cascade.
    for (int i = insts; i --> 0;) {
        if (remap[i] != i) { continue; }
        if (uses[i] == 0 && !is_store(inst[i].op)) {
            uses[remap[inst[i].x]]--;
            uses[remap[inst[i].y]]--;
            uses[remap[inst[i].z]]--;
        }
    }

    // Compact: build old→new index map, then copy live instructions forward.
    int *newix = calloc((size_t)insts, sizeof *newix);
    int live = 0;
    for (int i = 0; i < insts; i++) {
        if (remap[i] == i && (uses[i] > 0 || is_store(inst[i].op))) {
            newix[i] = live++;
        } else {
            newix[i] = -1;
        }
    }
    // Resolve deduped/dead entries: follow remap chain to a live instruction.
    for (int i = 0; i < insts; i++) {
        if (newix[i] < 0) {
            newix[i] = newix[remap[i]];
        }
    }
    // Compact and renumber.
    live = 0;
    for (int i = 0; i < insts; i++) {
        if (remap[i] != i) { continue; }
        if (uses[i] == 0 && !is_store(inst[i].op)) { continue; }
        inst[live] = inst[i];
        inst[live].x = newix[inst[i].x];
        inst[live].y = newix[inst[i].y];
        inst[live].z = newix[inst[i].z];
        live++;
    }
    free(newix);

    free(uses);
    free(remap);
    return live;
}

struct umbra_program* umbra_program(struct umbra_inst const inst[], int insts) {
    struct umbra_program *p = malloc(sizeof *p + (size_t)(insts+1) * sizeof *p->inst);
    for (int i = 0; i < insts; i++) {
        switch (inst[i].op) {
            case umbra_imm_16: p->inst[i] = (struct inst){.fn=imm_16, .x=inst[i].immi}; break;
            case umbra_imm_32: p->inst[i] = (struct inst){.fn=imm_32, .x=inst[i].immi}; break;
            case umbra_lane:   p->inst[i] = (struct inst){.fn=lane};                    break;

            case umbra_load_16:
                if (inst[inst[i].x].op == umbra_lane) {
                    p->inst[i] = (struct inst){.fn=load_16,   .x=inst[i].ptr};
                } else if (inst[inst[i].x].op == umbra_imm_32) {
                    p->inst[i] = (struct inst){.fn=uni_16,    .x=inst[i].ptr, .y=inst[inst[i].x].immi};
                } else {
                    p->inst[i] = (struct inst){.fn=gather_16, .x=inst[i].ptr, .y=inst[i].x-i};
                } break;
            case umbra_load_32:
                if (inst[inst[i].x].op == umbra_lane) {
                    p->inst[i] = (struct inst){.fn=load_32,   .x=inst[i].ptr};
                } else if (inst[inst[i].x].op == umbra_imm_32) {
                    p->inst[i] = (struct inst){.fn=uni_32,    .x=inst[i].ptr, .y=inst[inst[i].x].immi};
                } else {
                    p->inst[i] = (struct inst){.fn=gather_32, .x=inst[i].ptr, .y=inst[i].x-i};
                } break;

            case umbra_store_16:
                if (inst[inst[i].x].op == umbra_lane) {
                    p->inst[i] = (struct inst){.fn=store_16,   .x=inst[i].ptr, .y=inst[i].y-i};
                } else {
                    p->inst[i] = (struct inst){.fn=scatter_16, .x=inst[i].ptr, .y=inst[i].y-i, .z=inst[i].x-i};
                } break;
            case umbra_store_32:
                if (inst[inst[i].x].op == umbra_lane) {
                    p->inst[i] = (struct inst){.fn=store_32,   .x=inst[i].ptr, .y=inst[i].y-i};
                } else {
                    p->inst[i] = (struct inst){.fn=scatter_32, .x=inst[i].ptr, .y=inst[i].y-i, .z=inst[i].x-i};
                } break;

            case umbra_f32_from_i32: p->inst[i] = (struct inst){.fn=f32_from_i32, .x=inst[i].x-i}; break;
            case umbra_i32_from_f32: p->inst[i] = (struct inst){.fn=i32_from_f32, .x=inst[i].x-i}; break;
            case umbra_f16_from_f32: p->inst[i] = (struct inst){.fn=f16_from_f32, .x=inst[i].x-i}; break;
            case umbra_f32_from_f16: p->inst[i] = (struct inst){.fn=f32_from_f16, .x=inst[i].x-i}; break;

            case umbra_add_f16:  p->inst[i] = (struct inst){.fn= add_f16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_sub_f16:  p->inst[i] = (struct inst){.fn= sub_f16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_mul_f16:  p->inst[i] = (struct inst){.fn= mul_f16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_div_f16:  p->inst[i] = (struct inst){.fn= div_f16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_min_f16:  p->inst[i] = (struct inst){.fn= min_f16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_max_f16:  p->inst[i] = (struct inst){.fn= max_f16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_sqrt_f16: p->inst[i] = (struct inst){.fn=sqrt_f16, .x=inst[i].x-i}; break;
            case umbra_fma_f16:  p->inst[i] = (struct inst){.fn= fma_f16, .x=inst[i].x-i, .y=inst[i].y-i, .z=inst[i].z-i}; break;

            case umbra_add_f32:  p->inst[i] = (struct inst){.fn= add_f32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_sub_f32:  p->inst[i] = (struct inst){.fn= sub_f32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_mul_f32:  p->inst[i] = (struct inst){.fn= mul_f32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_div_f32:  p->inst[i] = (struct inst){.fn= div_f32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_min_f32:  p->inst[i] = (struct inst){.fn= min_f32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_max_f32:  p->inst[i] = (struct inst){.fn= max_f32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_sqrt_f32: p->inst[i] = (struct inst){.fn=sqrt_f32, .x=inst[i].x-i}; break;
            case umbra_fma_f32:  p->inst[i] = (struct inst){.fn= fma_f32, .x=inst[i].x-i, .y=inst[i].y-i, .z=inst[i].z-i}; break;

            case umbra_add_i16: p->inst[i] = (struct inst){.fn=add_i16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_sub_i16: p->inst[i] = (struct inst){.fn=sub_i16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_mul_i16: p->inst[i] = (struct inst){.fn=mul_i16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_shl_i16: p->inst[i] = (struct inst){.fn=shl_i16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_shr_u16: p->inst[i] = (struct inst){.fn=shr_u16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_shr_s16: p->inst[i] = (struct inst){.fn=shr_s16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_and_16: p->inst[i] = (struct inst){.fn=and_16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_or_16:  p->inst[i] = (struct inst){.fn= or_16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_xor_16: p->inst[i] = (struct inst){.fn=xor_16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_sel_16: p->inst[i] = (struct inst){.fn=sel_16, .x=inst[i].x-i, .y=inst[i].y-i, .z=inst[i].z-i}; break;

            case umbra_add_i32: p->inst[i] = (struct inst){.fn=add_i32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_sub_i32: p->inst[i] = (struct inst){.fn=sub_i32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_mul_i32: p->inst[i] = (struct inst){.fn=mul_i32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_shl_i32: p->inst[i] = (struct inst){.fn=shl_i32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_shr_u32: p->inst[i] = (struct inst){.fn=shr_u32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_shr_s32: p->inst[i] = (struct inst){.fn=shr_s32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_and_32: p->inst[i] = (struct inst){.fn=and_32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_or_32:  p->inst[i] = (struct inst){.fn= or_32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_xor_32: p->inst[i] = (struct inst){.fn=xor_32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_sel_32: p->inst[i] = (struct inst){.fn=sel_32, .x=inst[i].x-i, .y=inst[i].y-i, .z=inst[i].z-i}; break;

            case umbra_eq_f16: p->inst[i] = (struct inst){.fn=eq_f16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_ne_f16: p->inst[i] = (struct inst){.fn=ne_f16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_lt_f16: p->inst[i] = (struct inst){.fn=lt_f16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_le_f16: p->inst[i] = (struct inst){.fn=le_f16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_gt_f16: p->inst[i] = (struct inst){.fn=gt_f16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_ge_f16: p->inst[i] = (struct inst){.fn=ge_f16, .x=inst[i].x-i, .y=inst[i].y-i}; break;

            case umbra_eq_f32: p->inst[i] = (struct inst){.fn=eq_f32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_ne_f32: p->inst[i] = (struct inst){.fn=ne_f32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_lt_f32: p->inst[i] = (struct inst){.fn=lt_f32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_le_f32: p->inst[i] = (struct inst){.fn=le_f32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_gt_f32: p->inst[i] = (struct inst){.fn=gt_f32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_ge_f32: p->inst[i] = (struct inst){.fn=ge_f32, .x=inst[i].x-i, .y=inst[i].y-i}; break;

            case umbra_eq_i16: p->inst[i] = (struct inst){.fn=eq_i16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_ne_i16: p->inst[i] = (struct inst){.fn=ne_i16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_lt_s16: p->inst[i] = (struct inst){.fn=lt_s16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_le_s16: p->inst[i] = (struct inst){.fn=le_s16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_gt_s16: p->inst[i] = (struct inst){.fn=gt_s16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_ge_s16: p->inst[i] = (struct inst){.fn=ge_s16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_lt_u16: p->inst[i] = (struct inst){.fn=lt_u16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_le_u16: p->inst[i] = (struct inst){.fn=le_u16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_gt_u16: p->inst[i] = (struct inst){.fn=gt_u16, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_ge_u16: p->inst[i] = (struct inst){.fn=ge_u16, .x=inst[i].x-i, .y=inst[i].y-i}; break;

            case umbra_eq_i32: p->inst[i] = (struct inst){.fn=eq_i32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_ne_i32: p->inst[i] = (struct inst){.fn=ne_i32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_lt_s32: p->inst[i] = (struct inst){.fn=lt_s32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_le_s32: p->inst[i] = (struct inst){.fn=le_s32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_gt_s32: p->inst[i] = (struct inst){.fn=gt_s32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_ge_s32: p->inst[i] = (struct inst){.fn=ge_s32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_lt_u32: p->inst[i] = (struct inst){.fn=lt_u32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_le_u32: p->inst[i] = (struct inst){.fn=le_u32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_gt_u32: p->inst[i] = (struct inst){.fn=gt_u32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
            case umbra_ge_u32: p->inst[i] = (struct inst){.fn=ge_u32, .x=inst[i].x-i, .y=inst[i].y-i}; break;
        }
    }
    p->inst[insts] = (struct inst){.fn=done};
    p->insts = insts+1;
    return p;
}

void umbra_program_run(struct umbra_program const *p, int n, void *ptr[]) {
    val *v = malloc((size_t)p->insts * sizeof *v);
    int i = 0;
    while (i+K <= n) { p->inst->fn(p->inst, v, i+K, ptr); i += K; }
    while (i+1 <= n) { p->inst->fn(p->inst, v, i+1, ptr); i += 1; }
    free(v);
}

void umbra_program_free(struct umbra_program *p) {
    free(p);
}
