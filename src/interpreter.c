#include "../umbra.h"
#include "bb.h"
#include <stdint.h>
#include <stdlib.h>

#define cast(T,v) __builtin_convertvector(v,T)

#define K 8
typedef  int32_t I32 __attribute__((vector_size(K*4)));
typedef uint32_t U32 __attribute__((vector_size(K*4)));
typedef    float F32 __attribute__((vector_size(K*4)));
typedef uint16_t U16 __attribute__((vector_size(K*2)));

#if !defined(__wasm__)
    typedef __fp16 F16 __attribute__((vector_size(K*2)));
    static F32 f16_to_f32(U16 h) {
        F16 tmp; __builtin_memcpy(&tmp, &h, sizeof h);
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
            __builtin_memcpy(&tmp, (char*)&h + 2*i, 2);
            r[i] = (float)tmp;
        }
        return r;
    }
    static U16 f32_to_f16(F32 f) {
        U16 r = {0};
        for (int i = 0; i < K; i++) {
            __fp16 tmp = (__fp16)f[i];
            __builtin_memcpy((char*)&r + 2*i, &tmp, 2);
        }
        return r;
    }
#endif

typedef union {
    I32 i32;
    U32 u32;
    F32 f32;
} val;

struct interp_inst;
typedef int (*Fn)(struct interp_inst const *ip,
                  val *v, int end,
                  void* ptr[], long sz[]);
struct interp_inst {
    Fn  fn;
    int x,y,z,w;
};

struct umbra_interpreter {
    struct interp_inst *inst;
    val                *v;
    int                 preamble, nptr, n_deref, pad_;
};

#define op(name) \
    static int name(struct interp_inst const *ip, \
                    val *v, int end, \
                    void* ptr[], long sz[])
#define next return ip[1].fn(ip+1, v+1, end, ptr, sz)

static I32 clamp_ix(I32 ix, long bytes, int elem) {
    I32 zero = {0};
    int hi = (int)(bytes / elem) - 1;
    if (hi < 0) { hi = 0; }
    I32 max_ix = zero + hi;
    ix = (I32)((ix > zero) & ix)
       | (I32)((ix <= zero) & zero);
    ix = (I32)((ix < max_ix) & ix)
       | (I32)((ix >= max_ix) & max_ix);
    return ix;
}

op(imm_32) { v->i32 = (I32){0} + ip->x; next; }

op(lane) {
    I32 const iota = {0,1,2,3,4,5,6,7};
    if (end & (K-1)) { v->i32 = iota + (end - 1); }
    else             { v->i32 = iota + (end - K); }
    next;
}

op(uni_16) {
    int16_t uni;
    __builtin_memcpy(&uni,
                     (int16_t const*)ptr[ip->x] + ip->y,
                     sizeof uni);
    v->i32 = (I32){0} + (int32_t)uni;
    next;
}
op(uni_32) {
    int32_t uni;
    __builtin_memcpy(&uni,
                     (int32_t const*)ptr[ip->x] + ip->y,
                     sizeof uni);
    v->i32 = (I32){0} + uni;
    next;
}

op(load_16) {
    int16_t const *src =
        (int16_t const*)ptr[ip->x] + v[ip->y].i32[0];
    if (end & (K-1)) {
        int16_t s;
        __builtin_memcpy(&s, src + end-1, 2);
        v->i32 = (I32){0} + (int32_t)s;
    } else {
        typedef int16_t S16
            __attribute__((vector_size(K*2)));
        S16 tmp;
        __builtin_memcpy(&tmp, src + end-K, 2*K);
        v->i32 = cast(I32, tmp);
    }
    next;
}
op(load_32) {
    int32_t const *src =
        (int32_t const*)ptr[ip->x] + v[ip->y].i32[0];
    if (end & (K-1)) {
        __builtin_memcpy(v, src + end-1, 4);
    } else {
        __builtin_memcpy(v, src + end-K, 4*K);
    }
    next;
}

op(store_16) {
    I32 src32 = v[ip->y].i32;
    int16_t *dst =
        (int16_t*)ptr[ip->x] + v[ip->z].i32[0];
    if (end & (K-1)) {
        int16_t s = (int16_t)src32[0];
        __builtin_memcpy(dst + end-1, &s, 2);
    } else {
        for (int l = 0; l < K; l++) {
            int16_t s = (int16_t)src32[l];
            __builtin_memcpy(dst + end-K+l, &s, 2);
        }
    }
    next;
}
op(store_32) {
    int32_t *dst =
        (int32_t*)ptr[ip->x] + v[ip->z].i32[0];
    if (end & (K-1)) {
        __builtin_memcpy(dst + end-1, v + ip->y, 4);
    } else {
        __builtin_memcpy(dst + end-K, v + ip->y, 4*K);
    }
    next;
}

op(gather_16) {
    I32 ix = clamp_ix(v[ip->y].i32, sz[ip->x], 2);
    for (int l = 0; l < (end & (K-1) ? 1 : K); l++) {
        int16_t s;
        __builtin_memcpy(&s,
            (char const*)ptr[ip->x] + 2*ix[l], 2);
        v->i32[l] = (int32_t)s;
    }
    next;
}
op(gather_32) {
    I32 ix = clamp_ix(v[ip->y].i32, sz[ip->x], 4);
    for (int l = 0; l < (end & (K-1) ? 1 : K); l++) {
        __builtin_memcpy(
            (char*)&v->i32 + 4*l,
            (char const*)ptr[ip->x] + 4*ix[l], 4);
    }
    next;
}

op(scatter_16) {
    I32 ix = clamp_ix(v[ip->z].i32, sz[ip->x], 2);
    for (int l = 0; l < (end & (K-1) ? 1 : K); l++) {
        int16_t s = (int16_t)v[ip->y].i32[l];
        __builtin_memcpy(
            (char*)ptr[ip->x] + 2*ix[l], &s, 2);
    }
    next;
}
op(scatter_32) {
    I32 ix = clamp_ix(v[ip->z].i32, sz[ip->x], 4);
    for (int l = 0; l < (end & (K-1) ? 1 : K); l++) {
        __builtin_memcpy(
            (char*)ptr[ip->x] + 4*ix[l],
            (char*)&v[ip->y].i32 + 4*l, 4);
    }
    next;
}

op(htof_fn) {
    if (end & (K-1)) {
        uint16_t h = (uint16_t)(uint32_t)v[ip->x].i32[0];
        __fp16 tmp;
        __builtin_memcpy(&tmp, &h, 2);
        v->f32 = (F32){0} + (float)tmp;
    } else {
        U16 h = {0};
        for (int l = 0; l < K; l++) {
            h[l] = (uint16_t)(uint32_t)v[ip->x].i32[l];
        }
        v->f32 = f16_to_f32(h);
    }
    next;
}

op(ftoh_fn) {
    if (end & (K-1)) {
        __fp16 tmp = (__fp16)v[ip->x].f32[0];
        uint16_t h;
        __builtin_memcpy(&h, &tmp, 2);
        v->u32 = (U32){0} + (uint32_t)h;
    } else {
        U16 h = f32_to_f16(v[ip->x].f32);
        for (int l = 0; l < K; l++) {
            v->u32[l] = (uint32_t)h[l];
        }
    }
    next;
}

op(f32_from_i32) {
    v->f32 = cast(F32, v[ip->x].i32); next;
}
op(i32_from_f32) {
    v->i32 = cast(I32, v[ip->x].f32); next;
}

op( add_f32) {
    v->f32 = v[ip->x].f32 + v[ip->y].f32; next;
}
op( sub_f32) {
    v->f32 = v[ip->x].f32 - v[ip->y].f32; next;
}
op( mul_f32) {
    v->f32 = v[ip->x].f32 * v[ip->y].f32; next;
}
op( div_f32) {
    v->f32 = v[ip->x].f32 / v[ip->y].f32; next;
}
#if defined(__ARM_FEATURE_FMA) || defined(__FMA__)
    op( fma_f32) {
        v->f32 = v[ip->z].f32 + v[ip->x].f32 * v[ip->y].f32;
        next;
    }
    op( fms_f32) {
        v->f32 = v[ip->z].f32 - v[ip->x].f32 * v[ip->y].f32;
        next;
    }
#else
    typedef double F64 __attribute__((vector_size(K*8)));
    op( fma_f32) {
        F64 x = cast(F64, v[ip->x].f32),
            y = cast(F64, v[ip->y].f32),
            z = cast(F64, v[ip->z].f32);
        v->f32 = cast(F32, x * y + z);
        next;
    }
    op( fms_f32) {
        F64 x = cast(F64, v[ip->x].f32),
            y = cast(F64, v[ip->y].f32),
            z = cast(F64, v[ip->z].f32);
        v->f32 = cast(F32, z - x * y);
        next;
    }
#endif
op(sqrt_f32) {
    v->f32 = __builtin_elementwise_sqrt(v[ip->x].f32);
    next;
}
op( min_f32) {
    v->f32 = __builtin_elementwise_min(
        v[ip->x].f32, v[ip->y].f32);
    next;
}
op( max_f32) {
    v->f32 = __builtin_elementwise_max(
        v[ip->x].f32, v[ip->y].f32);
    next;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
op(eq_f32) {
    v->i32 = (I32)(v[ip->x].f32 == v[ip->y].f32);
    next;
}
#pragma clang diagnostic pop
op(lt_f32) {
    v->i32 = (I32)(v[ip->x].f32 < v[ip->y].f32);
    next;
}
op(le_f32) {
    v->i32 = (I32)(v[ip->x].f32 <= v[ip->y].f32);
    next;
}

op(add_i32) {
    v->i32 = v[ip->x].i32 + v[ip->y].i32; next;
}
op(sub_i32) {
    v->i32 = v[ip->x].i32 - v[ip->y].i32; next;
}
op(mul_i32) {
    v->i32 = v[ip->x].i32 * v[ip->y].i32; next;
}
op(shl_i32) {
    v->i32 = v[ip->x].i32 << v[ip->y].i32; next;
}
op(shr_u32) {
    v->u32 = v[ip->x].u32 >> v[ip->y].u32; next;
}
op(shr_s32) {
    v->i32 = v[ip->x].i32 >> v[ip->y].i32; next;
}
op(and_32) {
    v->i32 = v[ip->x].i32 & v[ip->y].i32; next;
}
op( or_32) {
    v->i32 = v[ip->x].i32 | v[ip->y].i32; next;
}
op(xor_32) {
    v->i32 = v[ip->x].i32 ^ v[ip->y].i32; next;
}
op(sel_32) {
    v->i32 = ( v[ip->x].i32 & v[ip->y].i32)
           | (~v[ip->x].i32 & v[ip->z].i32);
    next;
}

op(eq_i32) {
    v->i32 = (I32)(v[ip->x].i32 == v[ip->y].i32);
    next;
}
op(lt_s32) {
    v->i32 = (I32)(v[ip->x].i32 < v[ip->y].i32);
    next;
}
op(le_s32) {
    v->i32 = (I32)(v[ip->x].i32 <= v[ip->y].i32);
    next;
}

op(lt_u32) {
    v->i32 = (I32)(v[ip->x].u32 < v[ip->y].u32);
    next;
}
op(le_u32) {
    v->i32 = (I32)(v[ip->x].u32 <= v[ip->y].u32);
    next;
}

op(load_8x4) {
    uint8_t const *src = ptr[ip->x];
    if (end & (K-1)) {
        uint8_t const *px = src + (end-1)*4;
        for (int ch = 0; ch < 4; ch++) {
            v[ch].u32 = (U32){0};
            v[ch].u32[0] = px[ch];
        }
    } else {
        typedef uint8_t B32
            __attribute__((vector_size(32)));
        typedef uint8_t B8
            __attribute__((vector_size(K)));
        B32 all;
        __builtin_memcpy(&all, src + (end-K)*4, 32);
        B8 c0 = __builtin_shufflevector(
            all, all,  0, 4, 8,12,16,20,24,28);
        B8 c1 = __builtin_shufflevector(
            all, all,  1, 5, 9,13,17,21,25,29);
        B8 c2 = __builtin_shufflevector(
            all, all,  2, 6,10,14,18,22,26,30);
        B8 c3 = __builtin_shufflevector(
            all, all,  3, 7,11,15,19,23,27,31);
        v[0].u32 = __builtin_convertvector(c0, U32);
        v[1].u32 = __builtin_convertvector(c1, U32);
        v[2].u32 = __builtin_convertvector(c2, U32);
        v[3].u32 = __builtin_convertvector(c3, U32);
    }
    return ip[4].fn(ip+4, v+4, end, ptr, sz);
}

op(store_8x4) {
    uint8_t *dst = ptr[ip->x];
    U32 r = v[ip->y].u32, g = v[ip->z].u32,
        b = v[ip->w].u32, a = v[(int)ip[1].x].u32;
    if (end & (K-1)) {
        dst[(end-1)*4+0] = (uint8_t)r[0];
        dst[(end-1)*4+1] = (uint8_t)g[0];
        dst[(end-1)*4+2] = (uint8_t)b[0];
        dst[(end-1)*4+3] = (uint8_t)a[0];
    } else {
        typedef uint8_t B8
            __attribute__((vector_size(K)));
        B8 r8 = __builtin_convertvector(
            cast(U16, r), B8);
        B8 g8 = __builtin_convertvector(
            cast(U16, g), B8);
        B8 b8 = __builtin_convertvector(
            cast(U16, b), B8);
        B8 a8 = __builtin_convertvector(
            cast(U16, a), B8);
        typedef uint8_t B16
            __attribute__((vector_size(16)));
        B16 rg = __builtin_shufflevector(
            r8, g8,
            0,8, 1,9, 2,10, 3,11,
            4,12, 5,13, 6,14, 7,15);
        B16 ba = __builtin_shufflevector(
            b8, a8,
            0,8, 1,9, 2,10, 3,11,
            4,12, 5,13, 6,14, 7,15);
        typedef uint8_t B32
            __attribute__((vector_size(32)));
        B32 rgba = __builtin_shufflevector(
            rg, ba,
             0, 1,16,17,  2, 3,18,19,
             4, 5,20,21,  6, 7,22,23,
             8, 9,24,25, 10,11,26,27,
            12,13,28,29, 14,15,30,31);
        __builtin_memcpy(dst + (end-K)*4, &rgba, 32);
    }
    return ip[2].fn(ip+2, v+2, end, ptr, sz);
}

op(done) {
    (void)ip; (void)v; (void)end;
    (void)ptr; (void)sz;
    return 0;
}

op(deref_ptr_handler) {
    void *base = ptr[ip->x];
    void *derived;
    long dsz;
    __builtin_memcpy(&derived,
                     (char*)base + ip->y,
                     sizeof derived);
    __builtin_memcpy(&dsz,
                     (char*)base + ip->y + 8,
                     sizeof dsz);
    ptr[ip->z] = derived;
    sz[ip->z] = dsz;
    next;
}

#undef next
#undef op

static Fn const fn[] = {
    [op_add_f32]  = add_f32,
    [op_sub_f32]  = sub_f32,
    [op_mul_f32]  = mul_f32,
    [op_div_f32]  = div_f32,
    [op_min_f32]  = min_f32,
    [op_max_f32]  = max_f32,
    [op_sqrt_f32] = sqrt_f32,
    [op_fma_f32]  = fma_f32,
    [op_fms_f32]  = fms_f32,

    [op_add_i32] = add_i32,
    [op_sub_i32] = sub_i32,
    [op_mul_i32] = mul_i32,
    [op_shl_i32] = shl_i32,
    [op_shr_u32] = shr_u32,
    [op_shr_s32] = shr_s32,
    [op_and_32]  = and_32,
    [op_or_32]   = or_32,
    [op_xor_32]  = xor_32,
    [op_sel_32]  = sel_32,

    [op_f32_from_i32] = f32_from_i32,
    [op_i32_from_f32] = i32_from_f32,

    [op_htof] = htof_fn,
    [op_ftoh] = ftoh_fn,

    [op_eq_f32] = eq_f32,
    [op_lt_f32] = lt_f32,
    [op_le_f32] = le_f32,

    [op_eq_i32] = eq_i32,
    [op_lt_s32] = lt_s32,
    [op_le_s32] = le_s32,
    [op_lt_u32] = lt_u32,
    [op_le_u32] = le_u32,

    [op_deref_ptr]  = deref_ptr_handler,
    [op_load_8x4]   = load_8x4,
    [op_store_8x4]  = store_8x4,
};

int umbra_const_eval(enum op op, int xb,
                     int yb, int zb) {
    val v[4] = {0};
    struct interp_inst inst[2] = {
        {.fn=fn[op], .x=-3, .y=-2, .z=-1},
        {.fn=done},
    };

    __builtin_memcpy(&v[0], &xb, 4);
    __builtin_memcpy(&v[1], &yb, 4);
    __builtin_memcpy(&v[2], &zb, 4);

    inst[0].fn(inst, v+3, K,
               (void*[]){0}, (long[]){0});

    int r;
    __builtin_memcpy(&r, &v[3], 4);
    return r;
}

struct umbra_interpreter* umbra_interpreter(
    struct umbra_basic_block const *bb)
{
    int *id = calloc((size_t)bb->insts, sizeof *id);

    struct umbra_interpreter *p = malloc(sizeof *p);
    int num_insts = bb->insts + 1;
    for (int i = 0; i < bb->insts; i++) {
        if (bb->inst[i].op == op_store_8x4) {
            num_insts += 1;
        }
    }
    p->inst = malloc(
        (size_t)num_insts * sizeof *p->inst);
    p->v = malloc(
        (size_t)num_insts * sizeof *p->v);

    int max_ptr = -1;
    int n_deref = 0;
    for (int i = 0; i < bb->insts; i++) {
        if (has_ptr(bb->inst[i].op)
            && bb->inst[i].ptr >= 0
            && bb->inst[i].ptr > max_ptr) {
            max_ptr = bb->inst[i].ptr;
        }
        if (bb->inst[i].op == op_deref_ptr) {
            n_deref++;
        }
    }
    p->nptr    = max_ptr + 1;
    p->n_deref = n_deref;

    int *deref_slot =
        calloc((size_t)bb->insts, sizeof *deref_slot);
    {
        int di = 0;
        for (int i = 0; i < bb->insts; i++) {
            if (bb->inst[i].op == op_deref_ptr) {
                deref_slot[i] = p->nptr + di++;
            }
        }
    }

    int n = 0;
    #define emit(...) \
        p->inst[n] = (struct interp_inst){__VA_ARGS__}
    for (int pass = 0; pass < 2; pass++) {
        int const lo = pass ? bb->preamble : 0,
                  hi = pass ? bb->insts : bb->preamble;
        if (pass) {
            p->preamble = n;
        }
        for (int i = lo; i < hi; i++) {
            {
                struct bb_inst const *inst =
                    &bb->inst[i];
                int const X = id[inst->x] - n,
                          Y = id[inst->y] - n,
                          Z = id[inst->z] - n;
                switch (inst->op) {
                    case op_lane:
                        emit(.fn=lane);
                        break;
                    case op_imm_32:
                        emit(.fn=imm_32,
                             .x=inst->imm);
                        break;

                    case op_deref_ptr:
                        emit(.fn=deref_ptr_handler,
                             .x=inst->ptr,
                             .y=inst->imm,
                             .z=deref_slot[i]);
                        break;

                    #define RESOLVE_PTR(inst) \
                        ((inst)->ptr < 0      \
                            ? deref_slot[~(inst)->ptr] \
                            : (inst)->ptr)
                    case op_uni_16:
                        emit(.fn=uni_16,
                             .x=RESOLVE_PTR(inst),
                             .y=inst->imm);
                        break;
                    case op_uni_32:
                        emit(.fn=uni_32,
                             .x=RESOLVE_PTR(inst),
                             .y=inst->imm);
                        break;

                    case op_load_16:
                        emit(.fn=load_16,
                             .x=RESOLVE_PTR(inst),
                             .y=X);
                        break;
                    case op_load_32:
                        emit(.fn=load_32,
                             .x=RESOLVE_PTR(inst),
                             .y=X);
                        break;

                    case op_gather_16:
                        emit(.fn=gather_16,
                             .x=RESOLVE_PTR(inst),
                             .y=X);
                        break;
                    case op_gather_32:
                        emit(.fn=gather_32,
                             .x=RESOLVE_PTR(inst),
                             .y=X);
                        break;

                    case op_store_16:
                        emit(.fn=store_16,
                             .x=RESOLVE_PTR(inst),
                             .y=Y, .z=X);
                        break;
                    case op_store_32:
                        emit(.fn=store_32,
                             .x=RESOLVE_PTR(inst),
                             .y=Y, .z=X);
                        break;

                    case op_scatter_16:
                        emit(.fn=scatter_16,
                             .x=RESOLVE_PTR(inst),
                             .y=Y, .z=X);
                        break;
                    case op_scatter_32:
                        emit(.fn=scatter_32,
                             .x=RESOLVE_PTR(inst),
                             .y=Y, .z=X);
                        break;

                    case op_load_8x4: {
                        if (inst->x) {
                            id[i] = id[inst->x]
                                  + inst->imm;
                            continue;
                        }
                        emit(.fn=load_8x4,
                             .x=RESOLVE_PTR(inst));
                        id[i] = n;
                        n += 4;
                        continue;
                    }

                    case op_store_8x4: {
                        emit(.fn=store_8x4,
                             .x=RESOLVE_PTR(inst),
                             .y=id[inst->x] - n,
                             .z=id[inst->y] - n,
                             .w=id[inst->z] - n);
                        p->inst[n+1] =
                            (struct interp_inst){
                                .x=id[inst->w] - n};
                        n += 2;
                        continue;
                    }
                    #undef RESOLVE_PTR

                    case op_add_f32:
                    case op_sub_f32:
                    case op_mul_f32:
                    case op_div_f32:
                    case op_min_f32:
                    case op_max_f32:
                    case op_sqrt_f32:
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

                    case op_htof:
                    case op_ftoh:

                    case op_eq_f32:
                    case op_lt_f32:
                    case op_le_f32:
                    case op_eq_i32:
                    case op_lt_s32:
                    case op_le_s32:
                    case op_lt_u32:
                    case op_le_u32:
                        emit(.fn=fn[inst->op],
                             .x=X, .y=Y, .z=Z);
                        break;
                }
                id[i] = n++;
            }
        }
    }
    #undef emit
    p->inst[n] = (struct interp_inst){.fn=done};

    free(deref_slot);
    free(id);
    return p;
}

void umbra_interpreter_run(
    struct umbra_interpreter *p,
    int n, umbra_buf buf[])
{
    void *ptr[32] = {0};
    long  sz[32]  = {0};
    for (int i = 0; i < p->nptr && i < 32; i++) {
        ptr[i] = buf[i].ptr;
        sz[i]  = buf[i].sz < 0
               ? -buf[i].sz : buf[i].sz;
    }
    struct interp_inst const *start = p->inst;
    val                      *v     = p->v;
    int const P = p->preamble;

    int i = 0;
    while (i+K <= n) {
        start->fn(start, v, i+K, ptr, sz);
        i += K;
        start = p->inst+P;
        v     = p->v+P;
    }
    while (i+1 <= n) {
        start->fn(start, v, i+1, ptr, sz);
        i += 1;
        start = p->inst+P;
        v     = p->v+P;
    }
}

void umbra_interpreter_free(
    struct umbra_interpreter *p)
{
    free(p->inst);
    free(p->v);
    free(p);
}
