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

op(uni_16) {
    int16_t uni;
    __builtin_memcpy(&uni, ptr[ip->x], sizeof uni);
    v->i16 = (I16){0} + uni;
    next;
}
op(uni_32) {
    int32_t uni;
    __builtin_memcpy(&uni, ptr[ip->x], sizeof uni);
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

#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC) || defined(__F16C__)
    op(add_f16) { v->f16 = v[ip->x].f16 + v[ip->y].f16               ; next; }
    op(sub_f16) { v->f16 = v[ip->x].f16 - v[ip->y].f16               ; next; }
    op(mul_f16) { v->f16 = v[ip->x].f16 * v[ip->y].f16               ; next; }
    op(div_f16) { v->f16 = v[ip->x].f16 / v[ip->y].f16               ; next; }
    op(fma_f16) { v->f16 = v[ip->x].f16 * v[ip->y].f16 + v[ip->z].f16; next; }

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
#endif

op(add_f32) { v->f32 = v[ip->x].f32 + v[ip->y].f32               ; next; }
op(sub_f32) { v->f32 = v[ip->x].f32 - v[ip->y].f32               ; next; }
op(mul_f32) { v->f32 = v[ip->x].f32 * v[ip->y].f32               ; next; }
op(div_f32) { v->f32 = v[ip->x].f32 / v[ip->y].f32               ; next; }
op(fma_f32) { v->f32 = v[ip->x].f32 * v[ip->y].f32 + v[ip->z].f32; next; }

op(add_i16) { v->i16 = v[ip->x].i16 +  v[ip->y].i16; next; }
op(sub_i16) { v->i16 = v[ip->x].i16 -  v[ip->y].i16; next; }
op(mul_i16) { v->i16 = v[ip->x].i16 *  v[ip->y].i16; next; }
op(shl_i16) { v->i16 = v[ip->x].i16 << v[ip->y].i16; next; }
op(shr_i16) { v->u16 = v[ip->x].u16 >> v[ip->y].u16; next; }
op(sra_i16) { v->i16 = v[ip->x].i16 >> v[ip->y].i16; next; }
op(and_i16) { v->i16 = v[ip->x].i16 &  v[ip->y].i16; next; }
op( or_i16) { v->i16 = v[ip->x].i16 |  v[ip->y].i16; next; }
op(xor_i16) { v->i16 = v[ip->x].i16 ^  v[ip->y].i16; next; }
op(sel_i16) {
    v->i16 = ( v[ip->x].i16 & v[ip->y].i16)
           | (~v[ip->x].i16 & v[ip->z].i16);
    next;
}

op(add_i32) { v->i32 = v[ip->x].i32 +  v[ip->y].i32; next; }
op(sub_i32) { v->i32 = v[ip->x].i32 -  v[ip->y].i32; next; }
op(mul_i32) { v->i32 = v[ip->x].i32 *  v[ip->y].i32; next; }
op(shl_i32) { v->i32 = v[ip->x].i32 << v[ip->y].i32; next; }
op(shr_i32) { v->u32 = v[ip->x].u32 >> v[ip->y].u32; next; }
op(sra_i32) { v->i32 = v[ip->x].i32 >> v[ip->y].i32; next; }
op(and_i32) { v->i32 = v[ip->x].i32 &  v[ip->y].i32; next; }
op( or_i32) { v->i32 = v[ip->x].i32 |  v[ip->y].i32; next; }
op(xor_i32) { v->i32 = v[ip->x].i32 ^  v[ip->y].i32; next; }
op(sel_i32) {
    v->i32 = ( v[ip->x].i32 & v[ip->y].i32)
           | (~v[ip->x].i32 & v[ip->z].i32);
    next;
}

op(done) { (void)ip; (void)v; (void)end; (void)ptr; return 0; }

#undef next
#undef op

#define binop(sz, name) \
    p->inst[i] = (struct inst){.fn=name##_##sz, .x=inst[i].x - i, .y=inst[i].y - i}; break
#define triop(sz, name) \
    p->inst[i] = (struct inst){.fn=name##_##sz, .x=inst[i].x - i, .y=inst[i].y - i, .z=inst[i].z - i}; break
#define memop(sz, name) \
    p->inst[i] = (struct inst){.fn=name##_##sz, .x=inst[i].ptr}; break
#define storeop(sz) \
    p->inst[i] = (struct inst){.fn=store_##sz, .x=inst[i].ptr, .y=inst[i].x - i}; break
#define immop(sz) \
    p->inst[i] = (struct inst){.fn=imm_##sz, .x=inst[i].immi}; break
#define gatherop(sz) \
    p->inst[i] = (struct inst){.fn=gather_##sz, .x=inst[i].ptr, .y=inst[i].y - i}; break
#define uniop(sz) \
    p->inst[i] = (struct inst){.fn=uni_##sz, .x=inst[i].ptr}; break

// Peephole: fuse mul+add into fma.  Operates on compiled struct inst[].
static void peephole(struct inst p[], int insts) {
    for (int i = 0; i < insts; i++) {
        // add_f32 + mul_f32 -> fma_f32
        if (p[i].fn == add_f32) {
            int mx = i + p[i].x, my = i + p[i].y;
            if (mx >= 0 && mx < insts && p[mx].fn == mul_f32) {
                int old_y = p[i].y;
                p[i].fn = fma_f32;
                p[i].z  = old_y;
                p[i].y  = p[i].x + p[mx].y;
                p[i].x  = p[i].x + p[mx].x;
            } else if (my >= 0 && my < insts && p[my].fn == mul_f32) {
                int old_x = p[i].x;
                p[i].fn = fma_f32;
                p[i].z  = old_x;
                p[i].x  = p[i].y + p[my].x;
                p[i].y  = p[i].y + p[my].y;
            }
        }
        // add_f16 + mul_f16 -> fma_f16
        if (p[i].fn == add_f16) {
            int mx = i + p[i].x, my = i + p[i].y;
            if (mx >= 0 && mx < insts && p[mx].fn == mul_f16) {
                int old_y = p[i].y;
                p[i].fn = fma_f16;
                p[i].z  = old_y;
                p[i].y  = p[i].x + p[mx].y;
                p[i].x  = p[i].x + p[mx].x;
            } else if (my >= 0 && my < insts && p[my].fn == mul_f16) {
                int old_x = p[i].x;
                p[i].fn = fma_f16;
                p[i].z  = old_x;
                p[i].x  = p[i].y + p[my].x;
                p[i].y  = p[i].y + p[my].y;
            }
        }
    }
}

struct umbra_program* umbra_program(struct umbra_inst const inst[], int insts) {
    struct umbra_program *p = malloc(sizeof *p + (size_t)(insts+1) * sizeof *p->inst);
    for (int i = 0; i < insts; i++) {
        switch (inst[i].op) {
            case umbra_imm_16:    immop(16);
            case umbra_uni_16:    uniop(16);
            case umbra_gather_16: gatherop(16);
            case umbra_load_16:   memop(16, load);
            case umbra_store_16:  storeop(16);

            case umbra_imm_32:    immop(32);
            case umbra_uni_32:    uniop(32);
            case umbra_gather_32: gatherop(32);
            case umbra_load_32:   memop(32, load);
            case umbra_store_32:  storeop(32);

            case umbra_add_f16: binop(f16, add);
            case umbra_sub_f16: binop(f16, sub);
            case umbra_mul_f16: binop(f16, mul);
            case umbra_div_f16: binop(f16, div);

            case umbra_add_f32: binop(f32, add);
            case umbra_sub_f32: binop(f32, sub);
            case umbra_mul_f32: binop(f32, mul);
            case umbra_div_f32: binop(f32, div);

            case umbra_add_i16: binop(i16, add);
            case umbra_sub_i16: binop(i16, sub);
            case umbra_mul_i16: binop(i16, mul);
            case umbra_shl_i16: binop(i16, shl);
            case umbra_shr_i16: binop(i16, shr);
            case umbra_sra_i16: binop(i16, sra);
            case umbra_and_i16: binop(i16, and);
            case umbra_or_i16:  binop(i16, or);
            case umbra_xor_i16: binop(i16, xor);
            case umbra_sel_i16: triop(i16, sel);

            case umbra_add_i32: binop(i32, add);
            case umbra_sub_i32: binop(i32, sub);
            case umbra_mul_i32: binop(i32, mul);
            case umbra_shl_i32: binop(i32, shl);
            case umbra_shr_i32: binop(i32, shr);
            case umbra_sra_i32: binop(i32, sra);
            case umbra_and_i32: binop(i32, and);
            case umbra_or_i32:  binop(i32, or);
            case umbra_xor_i32: binop(i32, xor);
            case umbra_sel_i32: triop(i32, sel);
        }
    }
    p->inst[insts] = (struct inst){.fn=done};
    p->insts = insts+1;

    peephole(p->inst, insts);

    return p;
}
#undef uniop
#undef gatherop
#undef immop
#undef storeop
#undef memop
#undef triop
#undef binop

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
