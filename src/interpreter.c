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
    for (int i = 0; i < K; i++) { r[i] = fminf(a[i], b[i]); }
    return r;
}
static F32 vec_max(F32 a, F32 b) {
    F32 r;
    for (int i = 0; i < K; i++) { r[i] = fmaxf(a[i], b[i]); }
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

struct interp_inst;
typedef int (*Fn)(struct interp_inst const *ip, val *v, int end, int n, int row,
                  umbra_buf buf[]);
struct interp_inst {
    Fn  fn;
    int x, y, z, : 32;
};

struct umbra_interpreter {
    struct interp_inst *inst;
    val                *v;
    umbra_buf          *buf;
    int                 preamble, nptr, n_deref, pad_;
};

#define op(name)                                                                       \
    static int name(struct interp_inst const *ip, val *v, int end, int n,              \
                    int row, umbra_buf buf[])
#define next return ip[1].fn(ip + 1, v + 1, end, n, row, buf)


op(imm_32) {
    v->i32 = (I32){0} + ip->x;
    next;
}

op(x_fn) {
    I32 const seq = {0, 1, 2, 3, 4, 5, 6, 7};
    v->i32 = seq + (end - K);
    next;
}
op(y_fn) {
    v->i32 = (I32){0} + row;
    next;
}

op(uniform_16) {
    assert(buf[ip->x].row_bytes == 0);
    uint16_t uni;
    __builtin_memcpy(&uni, (uint16_t const*)buf[ip->x].ptr + ip->y, sizeof uni);
    v->u32 = (U32){0} + (uint32_t)uni;
    next;
}
op(uniform_32) {
    assert(buf[ip->x].row_bytes == 0);
    int32_t uni;
    __builtin_memcpy(&uni, (int32_t const*)buf[ip->x].ptr + ip->y, sizeof uni);
    v->i32 = (I32){0} + uni;
    next;
}

op(load_16) {
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
        for (int l = 0; l < rem; l++) {
            uint16_t s;
            __builtin_memcpy(&s, src + i + l, 2);
            __builtin_memcpy((char*)v + 2 * l, &s, 2);
        }
    }
    next;
}
op(load_32) {
    void const    *base = (char*)buf[ip->x].ptr + (size_t)row * buf[ip->x].row_bytes;
    int32_t const *src = (int32_t const*)base;
    int const      i = end - K;
    int const      rem = n - i;
    if (rem >= K) {
        __builtin_memcpy(v, src + i, 4 * K);
    } else {
        v->i32 = (I32){0};
        for (int l = 0; l < rem; l++) {
            int32_t tmp;
            __builtin_memcpy(&tmp, src + i + l, 4);
            v->i32[l] = tmp;
        }
    }
    next;
}
op(load_64_lo) {
    char const *src = (char const*)buf[ip->x].ptr + (size_t)row * buf[ip->x].row_bytes;
    int const   i = end - K;
    int const   rem = n - i;
    v->i32 = (I32){0};
    for (int l = 0; l < (rem < K ? rem : K); l++) {
        int32_t tmp;
        __builtin_memcpy(&tmp, src + (i + l) * 8, 4);
        v->i32[l] = tmp;
    }
    next;
}
op(load_64_hi) {
    char const *src = (char const*)buf[ip->x].ptr + (size_t)row * buf[ip->x].row_bytes;
    int const   i = end - K;
    int const   rem = n - i;
    v->i32 = (I32){0};
    for (int l = 0; l < (rem < K ? rem : K); l++) {
        int32_t tmp;
        __builtin_memcpy(&tmp, src + (i + l) * 8 + 4, 4);
        v->i32[l] = tmp;
    }
    next;
}
op(load_64_fused) {
    char const *src = (char const*)buf[ip->x].ptr + (size_t)row * buf[ip->x].row_bytes;
    int const   i = end - K;
    int const   rem = n - i;
    v[0].i32 = (I32){0};
    v[1].i32 = (I32){0};
    for (int l = 0; l < (rem < K ? rem : K); l++) {
        int32_t lo, hi;
        __builtin_memcpy(&lo, src + (i + l) * 8, 4);
        __builtin_memcpy(&hi, src + (i + l) * 8 + 4, 4);
        v[0].i32[l] = lo;
        v[1].i32[l] = hi;
    }
    return ip[1].fn(ip + 1, v + 2, end, n, row, buf);
}
op(store_16) {
    void     *base = (char*)buf[ip->x].ptr + (size_t)row * buf[ip->x].row_bytes;
    uint16_t *dst = (uint16_t*)base;
    int const i = end - K;
    int const rem = n - i;
    if (rem >= K) {
        __builtin_memcpy(dst + i, &v[ip->y], 2 * K);
    } else {
        for (int l = 0; l < rem; l++) {
            uint16_t s;
            __builtin_memcpy(&s, (char*)&v[ip->y] + 2 * l, 2);
            __builtin_memcpy(dst + i + l, &s, 2);
        }
    }
    next;
}
op(store_32) {
    void    *base = (char*)buf[ip->x].ptr + (size_t)row * buf[ip->x].row_bytes;
    int32_t *dst = (int32_t*)base;
    int const i = end - K;
    int const rem = n - i;
    if (rem >= K) {
        __builtin_memcpy(dst + i, v + ip->y, 4 * K);
    } else {
        for (int l = 0; l < rem; l++) {
            int32_t tmp;
            __builtin_memcpy(&tmp, (char*)&v[ip->y].i32 + 4 * l, 4);
            __builtin_memcpy(dst + i + l, &tmp, 4);
        }
    }
    next;
}
op(store_64) {
    char *dst = (char*)buf[ip->x].ptr + (size_t)row * buf[ip->x].row_bytes;
    int const i = end - K;
    int const rem = n - i;
    for (int l = 0; l < (rem < K ? rem : K); l++) {
        int32_t lo, hi;
        __builtin_memcpy(&lo, (char*)&v[ip->y].i32 + 4 * l, 4);
        __builtin_memcpy(&hi, (char*)&v[ip->z].i32 + 4 * l, 4);
        __builtin_memcpy(dst + (i + l) * 8, &lo, 4);
        __builtin_memcpy(dst + (i + l) * 8 + 4, &hi, 4);
    }
    next;
}

op(gather_uniform_16) {
    int const ix = v[ip->y].i32[0];
    int const count = (int)(buf[ip->x].sz / 2);
    if (ix < 0 || ix >= count) { v->u32 = (U32){0}; next; }
    uint16_t s;
    __builtin_memcpy(&s, (char const*)buf[ip->x].ptr + 2 * ix, 2);
    U16 const packed = (U16){0} + s;
    v->u32 = (U32){0};
    __builtin_memcpy(v, &packed, sizeof packed);
    next;
}
op(gather_uniform_32) {
    int const ix = v[ip->y].i32[0];
    int const count = (int)(buf[ip->x].sz / 4);
    if (ix < 0 || ix >= count) { v->i32 = (I32){0}; next; }
    int32_t val;
    __builtin_memcpy(&val, (char const*)buf[ip->x].ptr + 4 * ix, 4);
    v->i32 = (I32){0} + val;
    next;
}

op(gather_16) {
    I32 const ix = v[ip->y].i32;
    int const count = (int)(buf[ip->x].sz / 2);
    int const rem = n - (end - K);
    v->u32 = (U32){0};
    for (int l = 0; l < (rem < K ? rem : K); l++) {
        if (ix[l] >= 0 && ix[l] < count) {
            uint16_t s;
            __builtin_memcpy(&s, (char const*)buf[ip->x].ptr + 2 * ix[l], 2);
            __builtin_memcpy((char*)v + 2 * l, &s, 2);
        }
    }
    next;
}
op(gather_32) {
    I32 const ix = v[ip->y].i32;
    int const count = (int)(buf[ip->x].sz / 4);
    int const rem = n - (end - K);
    v->i32 = (I32){0};
    for (int l = 0; l < (rem < K ? rem : K); l++) {
        if (ix[l] >= 0 && ix[l] < count) {
            int32_t tmp;
            __builtin_memcpy(&tmp, (char const*)buf[ip->x].ptr + 4 * ix[l], 4);
            v->i32[l] = tmp;
        }
    }
    next;
}

op(f32_from_f16_fn) {
    U16 h;
    __builtin_memcpy(&h, &v[ip->x], sizeof h);
    v->f32 = f16_to_f32(h);
    next;
}

op(f16_from_f32_fn) {
    U16 const h = f32_to_f16(v[ip->x].f32);
    v->u32 = (U32){0};
    __builtin_memcpy(v, &h, sizeof h);
    next;
}

op(i32_from_s16) {
    U16 tmp;
    __builtin_memcpy(&tmp, &v[ip->x], sizeof tmp);
    typedef int16_t S16 __attribute__((vector_size(K * 2)));
    S16             stmp;
    __builtin_memcpy(&stmp, &tmp, sizeof tmp);
    v->i32 = cast(I32, stmp);
    next;
}
op(i32_from_u16) {
    U16 tmp;
    __builtin_memcpy(&tmp, &v[ip->x], sizeof tmp);
    v->u32 = cast(U32, tmp);
    next;
}
op(i16_from_i32) {
    U16 tmp;
    for (int l = 0; l < K; l++) { tmp[l] = (uint16_t)v[ip->x].u32[l]; }
    v->u32 = (U32){0};
    __builtin_memcpy(v, &tmp, sizeof tmp);
    next;
}

op(f32_from_i32) {
    v->f32 = cast(F32, v[ip->x].i32);
    next;
}
op(i32_from_f32) {
    v->i32 = cast(I32, v[ip->x].f32);
    next;
}

op(add_f32) {
    v->f32 = v[ip->x].f32 + v[ip->y].f32;
    next;
}
op(sub_f32) {
    v->f32 = v[ip->x].f32 - v[ip->y].f32;
    next;
}
op(mul_f32) {
    v->f32 = v[ip->x].f32 * v[ip->y].f32;
    next;
}
op(div_f32) {
    v->f32 = v[ip->x].f32 / v[ip->y].f32;
    next;
}
#if defined(__ARM_FEATURE_FMA) || defined(__FMA__)
op(fma_f32) {
    v->f32 = v[ip->z].f32 + v[ip->x].f32 * v[ip->y].f32;
    next;
}
op(fms_f32) {
    v->f32 = v[ip->z].f32 - v[ip->x].f32 * v[ip->y].f32;
    next;
}
#else
typedef double F64 __attribute__((vector_size(K * 8)));
op(fma_f32) {
    F64 const x = cast(F64, v[ip->x].f32), y = cast(F64, v[ip->y].f32),
              z = cast(F64, v[ip->z].f32);
    v->f32 = cast(F32, x * y + z);
    next;
}
op(fms_f32) {
    F64 const x = cast(F64, v[ip->x].f32), y = cast(F64, v[ip->y].f32),
              z = cast(F64, v[ip->z].f32);
    v->f32 = cast(F32, z - x * y);
    next;
}
#endif
op(sqrt_f32) {
    v->f32 = vec_sqrt(v[ip->x].f32);
    next;
}
op(abs_f32) {
    v->f32 = vec_abs(v[ip->x].f32);
    next;
}
op(neg_f32) {
    v->f32 = -v[ip->x].f32;
    next;
}
op(round_f32) {
    v->f32 = vec_round(v[ip->x].f32);
    next;
}
op(floor_f32) {
    v->f32 = vec_floor(v[ip->x].f32);
    next;
}
op(ceil_f32) {
    v->f32 = vec_ceil(v[ip->x].f32);
    next;
}
op(round_i32) {
    v->i32 = __builtin_convertvector(vec_round(v[ip->x].f32), I32);
    next;
}
op(floor_i32) {
    v->i32 = __builtin_convertvector(vec_floor(v[ip->x].f32), I32);
    next;
}
op(ceil_i32) {
    v->i32 = __builtin_convertvector(vec_ceil(v[ip->x].f32), I32);
    next;
}
op(min_f32) {
    v->f32 = vec_min(v[ip->x].f32, v[ip->y].f32);
    next;
}
op(max_f32) {
    v->f32 = vec_max(v[ip->x].f32, v[ip->y].f32);
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
    v->i32 = v[ip->x].i32 + v[ip->y].i32;
    next;
}
op(sub_i32) {
    v->i32 = v[ip->x].i32 - v[ip->y].i32;
    next;
}
op(mul_i32) {
    v->i32 = v[ip->x].i32 * v[ip->y].i32;
    next;
}
op(shl_i32) {
    v->i32 = v[ip->x].i32 << v[ip->y].i32;
    next;
}
op(shr_u32) {
    v->u32 = v[ip->x].u32 >> v[ip->y].u32;
    next;
}
op(shr_s32) {
    v->i32 = v[ip->x].i32 >> v[ip->y].i32;
    next;
}
op(and_32) {
    v->i32 = v[ip->x].i32 & v[ip->y].i32;
    next;
}
op(or_32) {
    v->i32 = v[ip->x].i32 | v[ip->y].i32;
    next;
}
op(xor_32) {
    v->i32 = v[ip->x].i32 ^ v[ip->y].i32;
    next;
}
op(sel_32) {
    v->i32 = (v[ip->x].i32 & v[ip->y].i32) | (~v[ip->x].i32 & v[ip->z].i32);
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

op(shl_imm_fn) {
    I32 const sh = (I32){0} + ip->y;
    v->i32 = v[ip->x].i32 << sh;
    next;
}
op(shr_u32_imm_fn) {
    U32 const sh = (U32){0} + (uint32_t)ip->y;
    v->u32 = v[ip->x].u32 >> sh;
    next;
}
op(shr_s32_imm_fn) {
    I32 const sh = (I32){0} + ip->y;
    v->i32 = v[ip->x].i32 >> sh;
    next;
}
op(and_imm_fn) {
    U32 const m = (U32){0} + (uint32_t)ip->y;
    v->u32 = v[ip->x].u32 & m;
    next;
}
op(add_f32_imm_fn) {
    union {
        int   i;
        float f;
    } const u = {.i = ip->y};
    F32 const imm = (F32){0} + u.f;
    v->f32 = v[ip->x].f32 + imm;
    next;
}
op(sub_f32_imm_fn) {
    union {
        int   i;
        float f;
    } const u = {.i = ip->y};
    F32 const imm = (F32){0} + u.f;
    v->f32 = v[ip->x].f32 - imm;
    next;
}
op(mul_f32_imm_fn) {
    union {
        int   i;
        float f;
    } const u = {.i = ip->y};
    F32 const imm = (F32){0} + u.f;
    v->f32 = v[ip->x].f32 * imm;
    next;
}
op(div_f32_imm_fn) {
    union {
        int   i;
        float f;
    } const u = {.i = ip->y};
    F32 const imm = (F32){0} + u.f;
    v->f32 = v[ip->x].f32 / imm;
    next;
}
op(min_f32_imm_fn) {
    union {
        int   i;
        float f;
    } const u = {.i = ip->y};
    F32 const imm = (F32){0} + u.f;
    v->f32 = vec_min(v[ip->x].f32, imm);
    next;
}
op(max_f32_imm_fn) {
    union {
        int   i;
        float f;
    } const u = {.i = ip->y};
    F32 const imm = (F32){0} + u.f;
    v->f32 = vec_max(v[ip->x].f32, imm);
    next;
}
op(add_i32_imm_fn) {
    I32 const imm = (I32){0} + ip->y;
    v->i32 = v[ip->x].i32 + imm;
    next;
}
op(sub_i32_imm_fn) {
    I32 const imm = (I32){0} + ip->y;
    v->i32 = v[ip->x].i32 - imm;
    next;
}
op(mul_i32_imm_fn) {
    I32 const imm = (I32){0} + ip->y;
    v->i32 = v[ip->x].i32 * imm;
    next;
}
op(or_32_imm_fn) {
    U32 const imm = (U32){0} + (uint32_t)ip->y;
    v->u32 = v[ip->x].u32 | imm;
    next;
}
op(xor_32_imm_fn) {
    U32 const imm = (U32){0} + (uint32_t)ip->y;
    v->u32 = v[ip->x].u32 ^ imm;
    next;
}
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
op(eq_f32_imm_fn) {
    union {
        int   i;
        float f;
    } const u = {.i = ip->y};
    F32 const imm = (F32){0} + u.f;
    v->i32 = (I32)(v[ip->x].f32 == imm);
    next;
}
#pragma clang diagnostic pop
op(lt_f32_imm_fn) {
    union {
        int   i;
        float f;
    } const u = {.i = ip->y};
    F32 const imm = (F32){0} + u.f;
    v->i32 = (I32)(v[ip->x].f32 < imm);
    next;
}
op(le_f32_imm_fn) {
    union {
        int   i;
        float f;
    } const u = {.i = ip->y};
    F32 const imm = (F32){0} + u.f;
    v->i32 = (I32)(v[ip->x].f32 <= imm);
    next;
}
op(eq_i32_imm_fn) {
    I32 const imm = (I32){0} + ip->y;
    v->i32 = (I32)(v[ip->x].i32 == imm);
    next;
}
op(lt_s32_imm_fn) {
    I32 const imm = (I32){0} + ip->y;
    v->i32 = (I32)(v[ip->x].i32 < imm);
    next;
}
op(le_s32_imm_fn) {
    I32 const imm = (I32){0} + ip->y;
    v->i32 = (I32)(v[ip->x].i32 <= imm);
    next;
}
op(pack_fn) {
    I32 const sh = (I32){0} + ip->z;
    v->u32 = v[ip->x].u32 | (U32)(v[ip->y].i32 << sh);
    next;
}

op(done) {
    (void)ip;
    (void)v;
    (void)end;
    (void)n;
    (void)row;
    (void)buf;
    return 0;
}

op(deref_ptr_handler) {
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
    next;
}

#undef next
#undef op

static Fn const fn[] = {
    [op_add_f32] = add_f32,
    [op_sub_f32] = sub_f32,
    [op_mul_f32] = mul_f32,
    [op_div_f32] = div_f32,
    [op_min_f32] = min_f32,
    [op_max_f32] = max_f32,
    [op_sqrt_f32] = sqrt_f32,
    [op_abs_f32] = abs_f32,
    [op_neg_f32] = neg_f32,
    [op_round_f32] = round_f32,
    [op_floor_f32] = floor_f32,
    [op_ceil_f32] = ceil_f32,
    [op_round_i32] = round_i32,
    [op_floor_i32] = floor_i32,
    [op_ceil_i32] = ceil_i32,
    [op_fma_f32] = fma_f32,
    [op_fms_f32] = fms_f32,

    [op_add_i32] = add_i32,
    [op_sub_i32] = sub_i32,
    [op_mul_i32] = mul_i32,
    [op_shl_i32] = shl_i32,
    [op_shr_u32] = shr_u32,
    [op_shr_s32] = shr_s32,
    [op_and_32] = and_32,
    [op_or_32] = or_32,
    [op_xor_32] = xor_32,
    [op_sel_32] = sel_32,

    [op_f32_from_i32] = f32_from_i32,
    [op_i32_from_f32] = i32_from_f32,

    [op_f32_from_f16] = f32_from_f16_fn,
    [op_f16_from_f32] = f16_from_f32_fn,

    [op_i32_from_s16] = i32_from_s16,
    [op_i32_from_u16] = i32_from_u16,
    [op_i16_from_i32] = i16_from_i32,

    [op_eq_f32] = eq_f32,
    [op_lt_f32] = lt_f32,
    [op_le_f32] = le_f32,

    [op_eq_i32] = eq_i32,
    [op_lt_s32] = lt_s32,
    [op_le_s32] = le_s32,
    [op_lt_u32] = lt_u32,
    [op_le_u32] = le_u32,

    [op_deref_ptr] = deref_ptr_handler,

    [op_shl_i32_imm] = shl_imm_fn,
    [op_shr_u32_imm] = shr_u32_imm_fn,
    [op_shr_s32_imm] = shr_s32_imm_fn,
    [op_pack] = pack_fn,
    [op_and_32_imm] = and_imm_fn,
    [op_add_f32_imm] = add_f32_imm_fn,
    [op_sub_f32_imm] = sub_f32_imm_fn,
    [op_mul_f32_imm] = mul_f32_imm_fn,
    [op_div_f32_imm] = div_f32_imm_fn,
    [op_min_f32_imm] = min_f32_imm_fn,
    [op_max_f32_imm] = max_f32_imm_fn,
    [op_add_i32_imm] = add_i32_imm_fn,
    [op_sub_i32_imm] = sub_i32_imm_fn,
    [op_mul_i32_imm] = mul_i32_imm_fn,
    [op_or_32_imm] = or_32_imm_fn,
    [op_xor_32_imm] = xor_32_imm_fn,
    [op_eq_f32_imm] = eq_f32_imm_fn,
    [op_lt_f32_imm] = lt_f32_imm_fn,
    [op_le_f32_imm] = le_f32_imm_fn,
    [op_eq_i32_imm] = eq_i32_imm_fn,
    [op_lt_s32_imm] = lt_s32_imm_fn,
    [op_le_s32_imm] = le_s32_imm_fn,
};

int umbra_const_eval(enum op op, int xb, int yb, int zb) {
    val                v[4] = {0};
    struct interp_inst const inst[2] = {
        {.fn = fn[op], .x = -3, .y = -2, .z = -1},
        {.fn = done},
    };

    __builtin_memcpy(&v[0], &xb, 4);
    __builtin_memcpy(&v[1], &yb, 4);
    __builtin_memcpy(&v[2], &zb, 4);

    inst[0].fn(inst, v + 3, K, K, 0, (umbra_buf[]){{0}});

    int r;
    __builtin_memcpy(&r, &v[3], 4);
    return r;
}

struct umbra_interpreter* umbra_interpreter(struct umbra_basic_block const *bb) {
    int *id = calloc((size_t)bb->insts, sizeof *id);

    struct umbra_interpreter *p = malloc(sizeof *p);
    int const                 num_insts = bb->insts + 1;
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
#define emit(...) \
    p->inst[n] = (struct interp_inst) { __VA_ARGS__ }
    for (int pass = 0; pass < 2; pass++) {
        int const lo = pass ? bb->preamble : 0, hi = pass ? bb->insts : bb->preamble;
        if (pass) { p->preamble = n; }
        for (int i = lo; i < hi; i++) {
            {
                struct bb_inst const *inst = &bb->inst[i];
                int const X = id[inst->x] - n, Y = id[inst->y] - n, Z = id[inst->z] - n;
                switch (inst->op) {
                case op_x: emit(.fn = x_fn); break;
                case op_y: emit(.fn = y_fn); break;
                case op_imm_32: emit(.fn = imm_32, .x = inst->imm); break;

                case op_deref_ptr:
                    emit(.fn = deref_ptr_handler, .x = inst->ptr, .y = inst->imm,
                         .z = deref_slot[i]);
                    break;

#define RESOLVE_PTR(inst) ((inst)->ptr < 0 ? deref_slot[~(inst)->ptr] : (inst)->ptr)
                case op_uniform_16:
                    emit(.fn = uniform_16, .x = RESOLVE_PTR(inst), .y = inst->imm);
                    break;
                case op_uniform_32:
                    emit(.fn = uniform_32, .x = RESOLVE_PTR(inst), .y = inst->imm);
                    break;

                case op_load_16:
                    emit(.fn = load_16, .x = RESOLVE_PTR(inst));
                    break;
                case op_load_32:
                    emit(.fn = load_32, .x = RESOLVE_PTR(inst));
                    break;
                case op_load_64_lo:
                    if (i + 1 < hi
                        && bb->inst[i + 1].op == op_load_64_hi
                        && bb->inst[i + 1].ptr == inst->ptr) {
                        emit(.fn = load_64_fused, .x = RESOLVE_PTR(inst));
                        id[i] = n - 1;
                        id[i + 1] = n;
                        i++;
                        n++;
                        continue;
                    }
                    emit(.fn = load_64_lo, .x = RESOLVE_PTR(inst));
                    break;
                case op_load_64_hi:
                    emit(.fn = load_64_hi, .x = RESOLVE_PTR(inst));
                    break;

                case op_gather_uniform_16:
                    emit(.fn = gather_uniform_16, .x = RESOLVE_PTR(inst), .y = X);
                    break;
                case op_gather_16:
                    emit(.fn = gather_16, .x = RESOLVE_PTR(inst), .y = X);
                    break;
                case op_gather_uniform_32:
                    emit(.fn = gather_uniform_32, .x = RESOLVE_PTR(inst), .y = X);
                    break;
                case op_gather_32:
                    emit(.fn = gather_32, .x = RESOLVE_PTR(inst), .y = X);
                    break;

                case op_store_16:
                    emit(.fn = store_16, .x = RESOLVE_PTR(inst), .y = Y);
                    break;
                case op_store_32:
                    emit(.fn = store_32, .x = RESOLVE_PTR(inst), .y = Y);
                    break;
                case op_store_64:
                    emit(.fn = store_64, .x = RESOLVE_PTR(inst), .y = X, .z = Y);
                    break;

#undef RESOLVE_PTR

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
                case op_le_u32: emit(.fn = fn[inst->op], .x = X, .y = Y, .z = Z); break;

                case op_shl_i32_imm:
                case op_shr_u32_imm:
                case op_shr_s32_imm:
                case op_and_32_imm:
                case op_add_f32_imm:
                case op_sub_f32_imm:
                case op_mul_f32_imm:
                case op_div_f32_imm:
                case op_min_f32_imm:
                case op_max_f32_imm:
                case op_add_i32_imm:
                case op_sub_i32_imm:
                case op_mul_i32_imm:
                case op_or_32_imm:
                case op_xor_32_imm:
                case op_eq_f32_imm:
                case op_lt_f32_imm:
                case op_le_f32_imm:
                case op_eq_i32_imm:
                case op_lt_s32_imm:
                case op_le_s32_imm:
                    emit(.fn = fn[inst->op], .x = X, .y = inst->imm);
                    break;
                case op_pack:
                    emit(.fn = fn[inst->op], .x = X, .y = Y, .z = inst->imm);
                    break;
                }
                id[i] = n++;
            }
        }
    }
#undef emit
    p->inst[n] = (struct interp_inst){.fn = done};

    free(deref_slot);
    free(id);
    return p;
}

void umbra_interpreter_run(struct umbra_interpreter *p, int l, int t, int r, int b,
                           umbra_buf caller_buf[]) {
    int const nall = p->nptr + p->n_deref;
    for (int i = 0; i < p->nptr; i++) { p->buf[i] = caller_buf[i]; }
    for (int i = p->nptr; i < nall; i++) { p->buf[i] = (umbra_buf){0}; }

    int const P = p->preamble;

    struct interp_inst const *s = p->inst;
    val                      *v = p->v;
    for (int row = t; row < b; row++) {
        for (int col = l; col < r; col += K) {
            s->fn(s, v, col + K, r, row, p->buf);
            s = p->inst + P;
            v = p->v + P;
        }
    }
}

void umbra_interpreter_free(struct umbra_interpreter *p) {
    free(p->buf);
    free(p->inst);
    free(p->v);
    free(p);
}
