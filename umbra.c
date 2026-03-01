#include "umbra.h"
#include <stdlib.h>
#include <string.h>

#define K 8

typedef short          I16v __attribute__((vector_size(K*2)));
typedef int            I32v __attribute__((vector_size(K*4)));
typedef unsigned short U16v __attribute__((vector_size(K*2)));
typedef unsigned int   U32v __attribute__((vector_size(K*4)));
typedef __fp16         F16v __attribute__((vector_size(K*2)));
typedef float          F32v __attribute__((vector_size(K*4)));

typedef union {
    I16v i16;
    I32v i32;
    F16v f16;
    F32v f32;
} val;

struct inst {
    int  (*fn)(struct inst const *ip, val *v, int end, void* ptr[]);
    int    x,y,z;
    int    imm;
    size_t offset;
};

struct umbra_program {
    int         insts;
    int         :32;
    struct inst inst[];
};

#define op(name) static int name(struct inst const *ip, val *v, int end, void* ptr[])
#define next return ip[1].fn(ip+1, v+1, end, ptr)

// Tail helpers: when end & (K-1), we're processing a partial vector.
#define tail16(body) do {                               \
    int tail = end & (K-1);                             \
    if (tail) { v->i16 = (I16v){0}; body(tail, 2); }   \
    else      {                      body(K,    2); }   \
} while(0)

#define tail32(body) do {                               \
    int tail = end & (K-1);                             \
    if (tail) { v->i32 = (I32v){0}; body(tail, 4); }   \
    else      {                      body(K,    4); }   \
} while(0)

// --- 16-bit memory ops ---
op(imm_16) { v->i16 = (I16v){0} + (short)ip->imm; next; }

op(uni_16) {
    short tmp;
    memcpy(&tmp, ptr[ip->x], 2);
    v->i16 = (I16v){0} + tmp;
    next;
}

op(gather_16) {
    int tail = end & (K-1);
    int cnt  = tail ? tail : K;
    v->i16 = (I16v){0};
    for (int k = 0; k < cnt; k++) {
        memcpy((char*)&v->i16 + (size_t)k * 2,
               (char*)ptr[ip->x] + (size_t)v[ip->y].i32[k] * 2, 2);
    }
    next;
}

op(load_16) {
#define body(n,sz) memcpy(&v->i16, (char*)ptr[ip->x] + (size_t)(end-(n)) * (size_t)(sz), (size_t)(n) * (size_t)(sz))
    tail16(body);
#undef body
    next;
}

op(store_16) {
    int tail = end & (K-1);
    int cnt  = tail ? tail : K;
    memcpy((char*)ptr[ip->x] + (size_t)(end - cnt) * 2, &v[ip->y].i16, (size_t)cnt * 2);
    next;
}

// --- 32-bit memory ops ---
op(imm_32) { v->i32 = (I32v){0} + ip->imm; next; }

op(uni_32) {
    int tmp;
    memcpy(&tmp, ptr[ip->x], 4);
    v->i32 = (I32v){0} + tmp;
    next;
}

op(gather_32) {
    int tail = end & (K-1);
    int cnt  = tail ? tail : K;
    v->i32 = (I32v){0};
    for (int k = 0; k < cnt; k++) {
        memcpy((char*)&v->i32 + (size_t)k * 4,
               (char*)ptr[ip->x] + (size_t)v[ip->y].i32[k] * 4, 4);
    }
    next;
}

op(load_32) {
#define body(n,sz) memcpy(&v->i32, (char*)ptr[ip->x] + (size_t)(end-(n)) * (size_t)(sz), (size_t)(n) * (size_t)(sz))
    tail32(body);
#undef body
    next;
}

op(store_32) {
    int tail = end & (K-1);
    int cnt  = tail ? tail : K;
    memcpy((char*)ptr[ip->x] + (size_t)(end - cnt) * 4, &v[ip->y].i32, (size_t)cnt * 4);
    next;
}

// --- f16 <-> f32 conversion ---
#if !defined(__aarch64__) && !defined(__F16C__)

static F32v f16_to_f32(I16v h) {
    U16v uh = (U16v)h;
    U32v sign = __builtin_convertvector(uh >> 15, U32v) << 31;
    U32v exp  = __builtin_convertvector((uh >> 10) & 0x1f, U32v);
    U32v mant = __builtin_convertvector(uh & 0x3ff, U32v);

    // Normal: rebias exponent (127 - 15 = 112), shift mantissa
    U32v normal = sign | ((exp + 112) << 23) | (mant << 13);

    // Zero/denormal (exp==0): flush to signed zero
    U32v zero = sign;

    // Inf/NaN (exp==31): all-ones exponent in f32, keep mantissa
    U32v infnan = sign | (0xffu << 23) | (mant << 13);

    U32v is_zero   = (U32v)-(exp == 0);
    U32v is_infnan  = (U32v)-(exp == 31);

    U32v bits = (is_zero & zero) | (is_infnan & infnan) | (~is_zero & ~is_infnan & normal);

    F32v result;
    __builtin_memcpy(&result, &bits, sizeof bits);
    return result;
}

static I16v f32_to_f16(F32v f) {
    U32v bits;
    __builtin_memcpy(&bits, &f, sizeof f);

    U32v sign = (bits >> 31) << 15;
    I32v exp  = (I32v)((bits >> 23) & 0xff) - 127 + 15;
    U32v mant = (bits >> 13) & 0x3ff;

    // Round to nearest even: check bit 12 (round) and bits 0-12 (sticky+guard)
    U32v round_bit = (bits >> 12) & 1;
    U32v sticky    = (U32v)-((bits & 0xfff) != 0);
    // Round up if round_bit set and (sticky or mantissa LSB set)
    mant += round_bit & (sticky | (mant & 1));
    // Handle mantissa overflow from rounding
    U32v mant_overflow = mant >> 10;
    exp += (I32v)mant_overflow;
    mant &= 0x3ff;

    U32v normal = sign | (U32v)((U32v)exp << 10) | mant;

    // Overflow -> inf
    U32v inf = sign | 0x7c00;
    U32v is_overflow = (U32v)-(exp >= 31);

    // Underflow -> zero
    U32v is_underflow = (U32v)-(exp <= 0);

    // Source inf/nan (exp==255 in f32)
    U32v src_exp = (bits >> 23) & 0xff;
    U32v is_infnan = (U32v)-(src_exp == 0xff);
    U32v infnan = sign | 0x7c00 | mant;

    U32v result32 = (is_underflow & sign)
                  | (is_overflow & ~is_infnan & inf)
                  | (is_infnan & infnan)
                  | (~is_underflow & ~is_overflow & ~is_infnan & normal);

    return (I16v)__builtin_convertvector(result32, U16v);
}

#define to_F32v(v)    f16_to_f32((v).i16)
#define f16_store(dst, expr) (dst)->i16 = f32_to_f16(expr)

#else

#define to_F32v(v)    __builtin_convertvector((v).f16, F32v)
#define f16_store(dst, expr) (dst)->f16 = __builtin_convertvector(expr, F16v)

#endif

// --- f16 arithmetic ---

#define f16_bin(name, OP) \
    op(name) { \
        f16_store(v, to_F32v(v[ip->x]) OP to_F32v(v[ip->y])); \
        next; \
    }
f16_bin(add_f16, +)
f16_bin(sub_f16, -)
f16_bin(mul_f16, *)
f16_bin(div_f16, /)
#undef f16_bin

op(fma_f16) {
#if defined(__aarch64__)
    _Pragma("clang diagnostic push")
    _Pragma("clang diagnostic ignored \"-Wdouble-promotion\"")
    _Pragma("clang diagnostic ignored \"-Wimplicit-float-conversion\"")
    v->f16 = v[ip->x].f16 * v[ip->y].f16 + v[ip->z].f16;
    _Pragma("clang diagnostic pop")
#else
    f16_store(v, to_F32v(v[ip->x]) * to_F32v(v[ip->y]) + to_F32v(v[ip->z]));
#endif
    next;
}
#undef f16_store

// --- f32 arithmetic ---
op(add_f32) { v->f32 = v[ip->x].f32 + v[ip->y].f32; next; }
op(sub_f32) { v->f32 = v[ip->x].f32 - v[ip->y].f32; next; }
op(mul_f32) { v->f32 = v[ip->x].f32 * v[ip->y].f32; next; }
op(div_f32) { v->f32 = v[ip->x].f32 / v[ip->y].f32; next; }
op(fma_f32) { v->f32 = v[ip->x].f32 * v[ip->y].f32 + v[ip->z].f32; next; }

// --- i16 arithmetic ---
op(add_i16) { v->i16 = v[ip->x].i16 + v[ip->y].i16; next; }
op(sub_i16) { v->i16 = v[ip->x].i16 - v[ip->y].i16; next; }
op(mul_i16) { v->i16 = v[ip->x].i16 * v[ip->y].i16; next; }
op(shl_i16) { v->i16 = v[ip->x].i16 << v[ip->y].i16; next; }
op(shr_i16) { v->i16 = (I16v)((U16v)v[ip->x].i16 >> (U16v)v[ip->y].i16); next; }
op(sra_i16) { v->i16 = v[ip->x].i16 >> v[ip->y].i16; next; }
op(and_i16) { v->i16 = v[ip->x].i16 & v[ip->y].i16; next; }
op( or_i16) { v->i16 = v[ip->x].i16 | v[ip->y].i16; next; }
op(xor_i16) { v->i16 = v[ip->x].i16 ^ v[ip->y].i16; next; }
op(sel_i16) { v->i16 = (v[ip->x].i16 & v[ip->y].i16) | (~v[ip->x].i16 & v[ip->z].i16); next; }

// --- i32 arithmetic ---
op(add_i32) { v->i32 = v[ip->x].i32 + v[ip->y].i32; next; }
op(sub_i32) { v->i32 = v[ip->x].i32 - v[ip->y].i32; next; }
op(mul_i32) { v->i32 = v[ip->x].i32 * v[ip->y].i32; next; }
op(shl_i32) { v->i32 = v[ip->x].i32 << v[ip->y].i32; next; }
op(shr_i32) { v->i32 = (I32v)((U32v)v[ip->x].i32 >> (U32v)v[ip->y].i32); next; }
op(sra_i32) { v->i32 = v[ip->x].i32 >> v[ip->y].i32; next; }
op(and_i32) { v->i32 = v[ip->x].i32 & v[ip->y].i32; next; }
op( or_i32) { v->i32 = v[ip->x].i32 | v[ip->y].i32; next; }
op(xor_i32) { v->i32 = v[ip->x].i32 ^ v[ip->y].i32; next; }
op(sel_i32) { v->i32 = (v[ip->x].i32 & v[ip->y].i32) | (~v[ip->x].i32 & v[ip->z].i32); next; }

op(done) { (void)ip; (void)v; (void)end; (void)ptr; return 0; }

#undef next
#undef op
#undef tail32
#undef tail16

// --- compiler ---
#define binop(sz, name) \
    p->inst[i] = (struct inst){.fn=name##_##sz, .x=inst[i].x - i, .y=inst[i].y - i}; break
#define triop(sz, name) \
    p->inst[i] = (struct inst){.fn=name##_##sz, .x=inst[i].x - i, .y=inst[i].y - i, .z=inst[i].z - i}; break
#define memop(sz, name) \
    p->inst[i] = (struct inst){.fn=name##_##sz, .x=inst[i].ptr}; break
#define storeop(sz) \
    p->inst[i] = (struct inst){.fn=store_##sz, .x=inst[i].ptr, .y=inst[i].x - i}; break
#define immop(sz) \
    p->inst[i] = (struct inst){.fn=imm_##sz, .imm=inst[i].immi}; break
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
    for (int end = K; end <= n; end += K) {
        p->inst[0].fn(p->inst, v, end, ptr);
    }
    if (n & (K-1)) {
        p->inst[0].fn(p->inst, v, n, ptr);
    }
    free(v);
}

void umbra_program_free(struct umbra_program *p) {
    free(p);
}
