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
    int x,y,z, :32;
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
    uint16_t uni;
    __builtin_memcpy(&uni,
                     (uint16_t const*)ptr[ip->x] + ip->y,
                     sizeof uni);
    v->u32 = (U32){0} + (uint32_t)uni;
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
    uint16_t const *src =
        (uint16_t const*)ptr[ip->x] + v[ip->y].i32[0];
    if (end & (K-1)) {
        uint16_t s;
        __builtin_memcpy(&s, src + end-1, 2);
        v->u32 = (U32){0};
        v->u32[0] = (uint32_t)s;
    } else {
        U16 tmp;
        __builtin_memcpy(&tmp, src + end-K, 2*K);
        v->u32 = (U32){0};
        __builtin_memcpy(v, &tmp, sizeof tmp);
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
    uint16_t *dst =
        (uint16_t*)ptr[ip->x] + v[ip->z].i32[0];
    if (end & (K-1)) {
        uint16_t s;
        __builtin_memcpy(&s, &v[ip->y], 2);
        __builtin_memcpy(dst + end-1, &s, 2);
    } else {
        __builtin_memcpy(dst + end-K, &v[ip->y], 2*K);
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
    v->u32 = (U32){0};
    for (int l = 0; l < (end & (K-1) ? 1 : K); l++) {
        uint16_t s;
        __builtin_memcpy(&s,
            (char const*)ptr[ip->x] + 2*ix[l], 2);
        __builtin_memcpy(
            (char*)v + 2*l, &s, 2);
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
        uint16_t s;
        __builtin_memcpy(&s,
            (char*)&v[ip->y] + 2*l, 2);
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

op(widen_f16_fn) {
    if (end & (K-1)) {
        uint16_t h;
        __builtin_memcpy(&h, &v[ip->x], 2);
        __fp16 tmp;
        __builtin_memcpy(&tmp, &h, 2);
        v->f32 = (F32){0} + (float)tmp;
    } else {
        U16 h;
        __builtin_memcpy(&h, &v[ip->x], sizeof h);
        v->f32 = f16_to_f32(h);
    }
    next;
}

op(narrow_f32_fn) {
    if (end & (K-1)) {
        __fp16 tmp = (__fp16)v[ip->x].f32[0];
        uint16_t h;
        __builtin_memcpy(&h, &tmp, 2);
        v->u32 = (U32){0};
        __builtin_memcpy(v, &h, 2);
    } else {
        U16 h = f32_to_f16(v[ip->x].f32);
        v->u32 = (U32){0};
        __builtin_memcpy(v, &h, sizeof h);
    }
    next;
}

op(widen_s16) {
    if (end & (K-1)) {
        int16_t s;
        __builtin_memcpy(&s, &v[ip->x], 2);
        v->i32 = (I32){0} + (int32_t)s;
    } else {
        U16 tmp;
        __builtin_memcpy(&tmp, &v[ip->x], sizeof tmp);
        typedef int16_t S16
            __attribute__((vector_size(K*2)));
        S16 stmp;
        __builtin_memcpy(&stmp, &tmp, sizeof tmp);
        v->i32 = cast(I32, stmp);
    }
    next;
}
op(widen_u16) {
    if (end & (K-1)) {
        uint16_t s;
        __builtin_memcpy(&s, &v[ip->x], 2);
        v->u32 = (U32){0} + (uint32_t)s;
    } else {
        U16 tmp;
        __builtin_memcpy(&tmp, &v[ip->x], sizeof tmp);
        v->u32 = cast(U32, tmp);
    }
    next;
}
op(narrow_16) {
    if (end & (K-1)) {
        uint16_t s = (uint16_t)v[ip->x].u32[0];
        v->u32 = (U32){0};
        __builtin_memcpy(v, &s, 2);
    } else {
        U16 tmp;
        for (int l = 0; l < K; l++) {
            tmp[l] = (uint16_t)v[ip->x].u32[l];
        }
        v->u32 = (U32){0};
        __builtin_memcpy(v, &tmp, sizeof tmp);
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

op(join_fn) { v->i32 = v[ip->x].i32; next; }
op(shl_imm_fn) {
    I32 sh = (I32){0} + ip->y;
    v->i32 = v[ip->x].i32 << sh;
    next;
}
op(shr_u32_imm_fn) {
    U32 sh = (U32){0} + (uint32_t)ip->y;
    v->u32 = v[ip->x].u32 >> sh;
    next;
}
op(shr_s32_imm_fn) {
    I32 sh = (I32){0} + ip->y;
    v->i32 = v[ip->x].i32 >> sh;
    next;
}
op(and_imm_fn) {
    U32 m = (U32){0} + (uint32_t)ip->y;
    v->u32 = v[ip->x].u32 & m;
    next;
}
op(pack_fn) {
    I32 sh = (I32){0} + ip->z;
    v->u32 = v[ip->x].u32 | (U32)(v[ip->y].i32 << sh);
    next;
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

    [op_widen_f16] = widen_f16_fn,
    [op_narrow_f32] = narrow_f32_fn,

    [op_widen_s16] = widen_s16,
    [op_widen_u16] = widen_u16,
    [op_narrow_16] = narrow_16,

    [op_eq_f32] = eq_f32,
    [op_lt_f32] = lt_f32,
    [op_le_f32] = le_f32,

    [op_eq_i32] = eq_i32,
    [op_lt_s32] = lt_s32,
    [op_le_s32] = le_s32,
    [op_lt_u32] = lt_u32,
    [op_le_u32] = le_u32,

    [op_deref_ptr]  = deref_ptr_handler,
    [op_join]       = join_fn,

    [op_shl_imm]     = shl_imm_fn,
    [op_shr_u32_imm] = shr_u32_imm_fn,
    [op_shr_s32_imm] = shr_s32_imm_fn,
    [op_pack]        = pack_fn,
    [op_and_imm]     = and_imm_fn,
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

static _Bool interp_chooser(struct bb_inst const *insts,
                            int join_id) {
    enum op y_op = insts[insts[join_id].y].op;
    return y_op == op_pack
        || y_op == op_and_imm;
}

struct umbra_interpreter* umbra_interpreter(
    struct umbra_basic_block const *bb)
{
    struct umbra_basic_block *resolved =
        umbra_resolve_joins(bb, interp_chooser);
    bb = resolved;

    int *id = calloc((size_t)bb->insts, sizeof *id);

    struct umbra_interpreter *p = malloc(sizeof *p);
    int num_insts = bb->insts + 1;
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

                    case op_widen_f16:
                    case op_narrow_f32:

                    case op_widen_s16:
                    case op_widen_u16:
                    case op_narrow_16:

                    case op_eq_f32:
                    case op_lt_f32:
                    case op_le_f32:
                    case op_eq_i32:
                    case op_lt_s32:
                    case op_le_s32:
                    case op_lt_u32:
                    case op_le_u32:
                    case op_join:
                        emit(.fn=fn[inst->op],
                             .x=X, .y=Y, .z=Z);
                        break;

                    case op_shl_imm:
                    case op_shr_u32_imm:
                    case op_shr_s32_imm:
                    case op_and_imm:
                        emit(.fn=fn[inst->op],
                             .x=X, .y=inst->imm);
                        break;
                    case op_pack:
                        emit(.fn=fn[inst->op],
                             .x=X, .y=Y,
                             .z=inst->imm);
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
    umbra_basic_block_free(resolved);
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

void umbra_interpreter_run_m(
    struct umbra_interpreter *p,
    int n, int m, int loop_off, umbra_buf buf[])
{
    for (int j = 0; j < m; j++) {
        int32_t j32 = j;
        __builtin_memcpy(
            (char*)buf[1].ptr + loop_off,
            &j32, 4);
        umbra_interpreter_run(p, n, buf);
    }
}

void umbra_interpreter_free(
    struct umbra_interpreter *p)
{
    free(p->inst);
    free(p->v);
    free(p);
}
