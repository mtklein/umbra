#include "../include/umbra.h"
#include "assume.h"
#include "bb.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#pragma clang diagnostic ignored "-Wfloat-equal"

// Bridge between public types (structs with bitfields) and internal val_ (union with .bits).
static val_ to_val_(umbra_val32 v) { val_ r; __builtin_memcpy(&r, &v, 4); return r; }
static umbra_val32 from_val_(val_ v) { umbra_val32 r; __builtin_memcpy(&r, &v, 4); return r; }

static int         val_id   (umbra_val32 v)    { return to_val_(v).id; }
static umbra_val32 val_make (int id, int chan) { return from_val_((val_){.id=id, .chan=(unsigned)chan}); }
static umbra_val16 val_make16(int id, int chan) { umbra_val16 r = {.id=id, .chan=(unsigned)chan}; return r; }
static umbra_val16 to_val16(umbra_val32 v)     { return (umbra_val16){.id=v.id, .chan=v.chan}; }

typedef umbra_val32          val;
typedef struct umbra_builder builder;

static int ptr_deref(int id)  { return (int)((unsigned)id | (1u << 31)); }
_Bool      ptr_is_deref(int ptr) { return ptr < 0; }
int        ptr_ix(int ptr)       { return (int)((unsigned)ptr & 0x7FFFFFFFu); }

static int ptr_bits(unsigned ix, unsigned deref) { return (int)(ix | (deref << 31)); }
static int p16(umbra_ptr16 p) { return ptr_bits(p.ix, p.deref); }
static int p32(umbra_ptr32 p) { return ptr_bits(p.ix, p.deref); }
static int p64(umbra_ptr64 p) { return ptr_bits(p.ix, p.deref); }

// Op flag table generated from X-macro flags at compile time.
enum { OP_FUSED_IMM = 1 << 4 };
#define      FLAG(name, flags) [op_##name] = (flags),
#define FUSED_FLAG(name, flags) [op_##name] = (flags) | OP_FUSED_IMM,
static uint8_t const op_flags[] = {
    OTHER_OPS(FLAG) BINARY_OPS(FLAG) UNARY_OPS(FLAG) IMM_OPS(FUSED_FLAG)
};
#undef FUSED_FLAG
#undef FLAG

_Bool is_store    (enum op op) { return !!(op_flags[op] & OP_STORE); }
_Bool has_ptr     (enum op op) { return !!(op_flags[op] & OP_PTR); }
_Bool is_varying  (enum op op) { return !!(op_flags[op] & OP_VARYING); }
_Bool is_fused_imm(enum op op) { return !!(op_flags[op] & OP_FUSED_IMM); }

static _Bool is_pow2(int x) { return __builtin_popcount((unsigned)x) == 1; }

static uint32_t mul_overflow(uint32_t x, uint32_t y) {
    __builtin_mul_overflow(x, y, &x);
    return x;
}

static uint32_t bb_inst_hash(struct bb_inst const *inst) {
    uint32_t h = (uint32_t)inst->op;
    h = mul_overflow(0x9e3779b9, h ^ (uint32_t)inst->x.bits);
    h = mul_overflow(0x9e3779b9, h ^ (uint32_t)inst->y.bits);
    h = mul_overflow(0x9e3779b9, h ^ (uint32_t)inst->z.bits);
    h = mul_overflow(0x9e3779b9, h ^ (uint32_t)inst->w.bits);
    h = mul_overflow(0x9e3779b9, h ^ (uint32_t)inst->ptr);
    h = mul_overflow(0x9e3779b9, h ^ (uint32_t)inst->imm);
    h ^= h >> 16;
    return h ? h : 1;
}

struct dedup_ctx {
    struct bb_inst const *probe;
    struct bb_inst const *insts;
    int                   hit, pad_;
};
static _Bool dedup_match(int id, void *ctx) {
    struct dedup_ctx *c = ctx;
    if (__builtin_memcmp(c->probe, &c->insts[id], sizeof *c->probe) == 0) {
        c->hit = id;
        return 1;
    }
    return 0;
}

static val push_(builder *b, struct bb_inst inst) {
    {
        enum op const op = inst.op;
        if (op == op_imm_32 || op == op_uniform_32 || op == op_deref_ptr) {
            inst.uniform = 1;
        } else if (is_varying(op)
                   || op == op_gather_32
                   || op == op_gather_16
                   || op == op_sample_32) {
            inst.uniform = 0;
        } else {
            inst.uniform = (!inst.x.bits || b->inst[inst.x.id].uniform)
                && (!inst.y.bits || b->inst[inst.y.id].uniform)
                && (!inst.z.bits || b->inst[inst.z.id].uniform)
                && (!inst.w.bits || b->inst[inst.w.id].uniform);
        }
    }

    uint32_t const h = bb_inst_hash(&inst);

    struct dedup_ctx ctx = {.probe = &inst, .insts = b->inst};
    if (hash_lookup(b->ht, h, dedup_match, &ctx)) {
        return (val){.id = ctx.hit};
    }

    if (b->insts == b->inst_cap) {
        b->inst_cap = b->inst_cap ? 2 * b->inst_cap : 32;
        b->inst = realloc(b->inst, (size_t)b->inst_cap * sizeof *b->inst);
    }

    int const id = b->insts++;
    b->inst[id] = inst;
    if (!is_store(inst.op)) {
        hash_insert(&b->ht, h, id);
    }
    return (val){.id = id};
}
#define push(b, ...) push_(b, (struct bb_inst){.op = __VA_ARGS__})
#define VX(v) .x = (val_){.id = (v).id, .chan = (v).chan}
#define VY(v) .y = (val_){.id = (v).id, .chan = (v).chan}
#define VZ(v) .z = (val_){.id = (v).id, .chan = (v).chan}
#define VW(v) .w = (val_){.id = (v).id, .chan = (v).chan}

builder* umbra_builder(void) {
    builder *b = calloc(1, sizeof *b);
    b->inst_cap = 128;
    b->inst = malloc((size_t)b->inst_cap * sizeof *b->inst);
    b->ht = (struct hash){.slots = 128, .data = calloc(128, sizeof(unsigned) + sizeof(int))};
    // Simplifies liveness analysis to know id 0 is imm=0.
    push(b, op_imm_32, .imm = 0);
    return b;
}

void umbra_builder_free(builder *b) {
    if (b) {
        free(b->inst);
        free(b->ht.data);
        free(b);
    }
}

val umbra_x(builder *b) { return push(b, op_x); }
val umbra_y(builder *b) { return push(b, op_y); }

val umbra_imm_i32(builder *b, int bits) { return push(b, op_imm_32, .imm = bits); }
val umbra_imm_f32(builder *b, float v) {
    union {
        float f;
        int   i;
    } const u = {.f = v};
    return umbra_imm_i32(b, u.i);
}

umbra_ptr16 umbra_deref_ptr16(builder *b, umbra_ptr32 buf, size_t off) {
    val const v = push(b, op_deref_ptr, .ptr = p32(buf), .imm = (int)off);
    return (umbra_ptr16){.ix = (unsigned)val_id(v), .deref = 1};
}
umbra_ptr32 umbra_deref_ptr32(builder *b, umbra_ptr32 buf, size_t off) {
    val const v = push(b, op_deref_ptr, .ptr = p32(buf), .imm = (int)off);
    return (umbra_ptr32){.ix = (unsigned)val_id(v), .deref = 1};
}


umbra_val16 umbra_gather_16(builder *b, umbra_ptr16 src, val ix) {
    return to_val16(push(b, op_gather_16, VX(ix), .ptr = p16(src)));
}
val umbra_load_32(builder *b, umbra_ptr32 src) {
    return push(b, op_load_32, .ptr = p32(src));
}
umbra_val16 umbra_load_16(builder *b, umbra_ptr16 src) {
    return to_val16(push(b, op_load_16, .ptr = p16(src)));
}
val umbra_uniform_32(builder *b, umbra_ptr32 src, size_t off) {
    return push(b, op_uniform_32, .imm = (int)off, .ptr = p32(src));
}
val umbra_gather_32(builder *b, umbra_ptr32 src, val ix) {
    enum op const op = b->inst[val_id(ix)].uniform ? op_gather_uniform_32 : op_gather_32;
    return push(b, op, VX(ix), .ptr = p32(src));
}
val umbra_sample_32(builder *b, umbra_ptr32 src, val ix) {
    return push(b, op_sample_32, VX(ix), .ptr = p32(src));
}
void umbra_store_32(builder *b, umbra_ptr32 dst, val v) {
    push(b, op_store_32, VY(v), .ptr = p32(dst));
}
void umbra_store_16(builder *b, umbra_ptr16 dst, umbra_val16 v) {
    push(b, op_store_16, VY(v), .ptr = p16(dst));
}
static void x4_32(val px, val *r, val *g, val *b, val *a) {
    int const id = val_id(px);
    *r = val_make(id, 0); *g = val_make(id, 1);
    *b = val_make(id, 2); *a = val_make(id, 3);
}
static void x4_16(val px, umbra_val16 *r, umbra_val16 *g, umbra_val16 *b, umbra_val16 *a) {
    int const id = val_id(px);
    *r = val_make16(id, 0); *g = val_make16(id, 1);
    *b = val_make16(id, 2); *a = val_make16(id, 3);
}
void umbra_load_8x4(builder *b, umbra_ptr32 src, val *r, val *g, val *bl, val *a) {
    x4_32(push(b, op_load_8x4, .ptr = p32(src)), r, g, bl, a);
}
void umbra_store_8x4(builder *b, umbra_ptr32 dst, val r, val g, val bl, val a) {
    push(b, op_store_8x4, VX(r), VY(g), VZ(bl), VW(a), .ptr = p32(dst));
}
void umbra_load_16x4(builder *b, umbra_ptr64 src, umbra_val16 *r, umbra_val16 *g, umbra_val16 *bl, umbra_val16 *a) {
    x4_16(push(b, op_load_16x4, .ptr = p64(src)), r, g, bl, a);
}
void umbra_store_16x4(builder *b, umbra_ptr64 dst, umbra_val16 r, umbra_val16 g, umbra_val16 bl, umbra_val16 a) {
    push(b, op_store_16x4, VX(r), VY(g), VZ(bl), VW(a), .ptr = p64(dst));
}
void umbra_load_16x4_planar(builder *b, umbra_ptr16 src, umbra_val16 *r, umbra_val16 *g, umbra_val16 *bl, umbra_val16 *a) {
    x4_16(push(b, op_load_16x4_planar, .ptr = p16(src)), r, g, bl, a);
}
void umbra_store_16x4_planar(builder *b, umbra_ptr16 dst, umbra_val16 r, umbra_val16 g, umbra_val16 bl, umbra_val16 a) {
    push(b, op_store_16x4_planar, VX(r), VY(g), VZ(bl), VW(a), .ptr = p16(dst));
}
val umbra_i32_from_s16(builder *b, umbra_val16 x) { return push(b, op_i32_from_s16, VX(x)); }
val umbra_i32_from_u16(builder *b, umbra_val16 x) { return push(b, op_i32_from_u16, VX(x)); }
umbra_val16 umbra_i16_from_i32(builder *b, val x) { return to_val16(push(b, op_i16_from_i32, VX(x))); }

val umbra_f32_from_f16(builder *b, umbra_val16 x) { return push(b, op_f32_from_f16, VX(x)); }
umbra_val16 umbra_f16_from_f32(builder *b, val x) { return to_val16(push(b, op_f16_from_f32, VX(x))); }

static _Bool is_imm32(builder *b, int id, int v) {
    return b->inst[id].op == op_imm_32 && b->inst[id].imm == v;
}

static _Bool is_imm(builder *b, int id) { return b->inst[id].op == op_imm_32; }

static val math_(builder *b, struct bb_inst inst) {
    if (is_imm(b, inst.x.id) && is_imm(b, inst.y.id) && is_imm(b, inst.z.id)) {
        int const result = umbra_const_eval(inst.op, b->inst[inst.x.id].imm,
                                            b->inst[inst.y.id].imm, b->inst[inst.z.id].imm);
        return umbra_imm_i32(b, result);
    }
    return push_(b, inst);
}
#define math(b, ...) math_(b, (struct bb_inst){.op = __VA_ARGS__})

static val try_imm(builder *b, val d, enum op fused, val x, val y) {
    int const imm_id = is_imm(b, val_id(x)) ? val_id(x) : is_imm(b, val_id(y)) ? val_id(y) : -1;
    if (imm_id >= 0) {
        val const other = imm_id == val_id(x) ? y : x;
        return push(b, fused, VX(other), VY(val_make(imm_id, 0)), .imm = b->inst[imm_id].imm);
    }
    return d;
}

static void sort(val *a, val *b) {
    if (to_val_(*b).bits < to_val_(*a).bits) {
        val const t = *a;
        *a = *b;
        *b = t;
    }
}

val umbra_add_f32(builder *b, val x, val y) {
    sort(&x, &y);
    if (is_imm32(b, val_id(x), 0)) { return y; }
    if (b->inst[val_id(x)].op == op_mul_f32) {
        return math(b, op_fma_f32, .x = b->inst[val_id(x)].x, .y = b->inst[val_id(x)].y, VZ(y));
    }
    if (b->inst[val_id(y)].op == op_mul_f32) {
        return math(b, op_fma_f32, .x = b->inst[val_id(y)].x, .y = b->inst[val_id(y)].y, VZ(x));
    }
    return try_imm(b, math(b, op_add_f32, VX(x), VY(y)), op_add_f32_imm, x,
                   y);
}

val umbra_sub_f32(builder *b, val x, val y) {
    if (is_imm32(b, val_id(y), 0)) { return x; }
    if (b->inst[val_id(y)].op == op_mul_f32) {
        return math(b, op_fms_f32, .x = b->inst[val_id(y)].x, .y = b->inst[val_id(y)].y, VZ(x));
    }
    val const d = math(b, op_sub_f32, VX(x), VY(y));
    if (is_imm(b, val_id(y))) {
        return push(b, op_sub_f32_imm, VX(x), VY(y), .imm = b->inst[val_id(y)].imm);
    }
    return d;
}

val umbra_mul_f32(builder *b, val x, val y) {
    sort(&x, &y);
    if (is_imm32(b, val_id(x), 0x3f800000)) { return y; }
    if (is_imm32(b, val_id(y), 0x3f800000)) { return x; }
    return try_imm(b, math(b, op_mul_f32, VX(x), VY(y)), op_mul_f32_imm, x,
                   y);
}

val umbra_div_f32(builder *b, val x, val y) {
    if (is_imm32(b, val_id(y), 0x3f800000)) { return x; }
    val const d = math(b, op_div_f32, VX(x), VY(y));
    if (is_imm(b, val_id(y))) {
        return push(b, op_div_f32_imm, VX(x), VY(y), .imm = b->inst[val_id(y)].imm);
    }
    return d;
}

val umbra_min_f32(builder *b, val x, val y) {
    sort(&x, &y);
    return try_imm(b, math(b, op_min_f32, VX(x), VY(y)), op_min_f32_imm, x,
                   y);
}

val umbra_max_f32(builder *b, val x, val y) {
    sort(&x, &y);
    return try_imm(b, math(b, op_max_f32, VX(x), VY(y)), op_max_f32_imm, x,
                   y);
}

val umbra_sqrt_f32(builder *b, val x) { return math(b, op_sqrt_f32, VX(x)); }
val umbra_abs_f32(builder *b, val x) { return math(b, op_abs_f32, VX(x)); }
val umbra_round_f32(builder *b, val x) { return math(b, op_round_f32, VX(x)); }
val umbra_floor_f32(builder *b, val x) { return math(b, op_floor_f32, VX(x)); }
val umbra_ceil_f32(builder *b, val x) { return math(b, op_ceil_f32, VX(x)); }
val umbra_round_i32(builder *b, val x) { return math(b, op_round_i32, VX(x)); }
val umbra_floor_i32(builder *b, val x) { return math(b, op_floor_i32, VX(x)); }
val umbra_ceil_i32(builder *b, val x) { return math(b, op_ceil_i32, VX(x)); }
val umbra_add_i32(builder *b, val x, val y) {
    sort(&x, &y);
    if (is_imm32(b, val_id(x), 0)) { return y; }
    return try_imm(b, math(b, op_add_i32, VX(x), VY(y)), op_add_i32_imm, x,
                   y);
}

val umbra_sub_i32(builder *b, val x, val y) {
    if (is_imm32(b, val_id(y), 0)) { return x; }
    if (val_id(x) == val_id(y)) { return umbra_imm_i32(b, 0); }
    val const d = math(b, op_sub_i32, VX(x), VY(y));
    if (is_imm(b, val_id(y))) {
        return push(b, op_sub_i32_imm, VX(x), VY(y), .imm = b->inst[val_id(y)].imm);
    }
    return d;
}

val umbra_mul_i32(builder *b, val x, val y) {
    sort(&x, &y);
    if (is_imm32(b, val_id(x), 1)) { return y; }
    if (is_imm32(b, val_id(y), 1)) { return x; }
    if (is_imm32(b, val_id(x), 0)) { return x; }
    if (b->inst[val_id(x)].op == op_imm_32 && is_pow2(b->inst[val_id(x)].imm)) {
        int const shift = __builtin_ctz((unsigned)b->inst[val_id(x)].imm);
        return umbra_shl_i32(b, y, umbra_imm_i32(b, shift));
    }
    if (b->inst[val_id(y)].op == op_imm_32 && is_pow2(b->inst[val_id(y)].imm)) {
        int const shift = __builtin_ctz((unsigned)b->inst[val_id(y)].imm);
        return umbra_shl_i32(b, x, umbra_imm_i32(b, shift));
    }
    return try_imm(b, math(b, op_mul_i32, VX(x), VY(y)), op_mul_i32_imm, x,
                   y);
}

val umbra_shl_i32(builder *b, val x, val y) {
    if (is_imm32(b, val_id(y), 0)) { return x; }
    if (is_imm(b, val_id(y))) {
        return push(b, op_shl_i32_imm, VX(x), .imm = b->inst[val_id(y)].imm);
    }
    return math(b, op_shl_i32, VX(x), VY(y));
}
val umbra_shr_u32(builder *b, val x, val y) {
    if (is_imm32(b, val_id(y), 0)) { return x; }
    if (is_imm(b, val_id(y))) {
        return push(b, op_shr_u32_imm, VX(x), .imm = b->inst[val_id(y)].imm);
    }
    return math(b, op_shr_u32, VX(x), VY(y));
}
val umbra_shr_s32(builder *b, val x, val y) {
    if (is_imm32(b, val_id(y), 0)) { return x; }
    if (is_imm(b, val_id(y))) {
        return push(b, op_shr_s32_imm, VX(x), .imm = b->inst[val_id(y)].imm);
    }
    return math(b, op_shr_s32, VX(x), VY(y));
}

val umbra_and_32(builder *b, val x, val y) {
    sort(&x, &y);
    if (val_id(x) == val_id(y)) { return x; }
    if (is_imm32(b, val_id(x), -1)) { return y; }
    if (is_imm32(b, val_id(y), -1)) { return x; }
    if (is_imm32(b, val_id(x), 0)) { return x; }
    if (is_imm32(b, val_id(x), 0x7fffffff)) { return umbra_abs_f32(b, y); }
    if (is_imm32(b, val_id(y), 0x7fffffff)) { return umbra_abs_f32(b, x); }
    val const d = math(b, op_and_32, VX(x), VY(y));
    if (is_imm(b, val_id(x))) {
        return push(b, op_and_32_imm, VX(y), VY(x), .imm = b->inst[val_id(x)].imm);
    }
    if (is_imm(b, val_id(y))) {
        return push(b, op_and_32_imm, VX(x), VY(y), .imm = b->inst[val_id(y)].imm);
    }
    return d;
}
val umbra_or_32(builder *b, val x, val y) {
    sort(&x, &y);
    if (val_id(x) == val_id(y)) { return x; }
    if (is_imm32(b, val_id(x), 0)) { return y; }
    if (is_imm32(b, val_id(x), -1)) { return x; }
    if (is_imm32(b, val_id(y), -1)) { return y; }
    return try_imm(b, math(b, op_or_32, VX(x), VY(y)), op_or_32_imm, x, y);
}
val umbra_xor_32(builder *b, val x, val y) {
    sort(&x, &y);
    if (val_id(x) == val_id(y)) { return umbra_imm_i32(b, 0); }
    if (is_imm32(b, val_id(x), 0)) { return y; }
    return try_imm(b, math(b, op_xor_32, VX(x), VY(y)), op_xor_32_imm, x, y);
}
val umbra_sel_32(builder *b, val c, val t, val fv) {
    if (val_id(t) == val_id(fv)) { return t; }
    if (is_imm32(b, val_id(c), -1)) { return t; }
    if (is_imm32(b, val_id(c), 0)) { return fv; }
    return math(b, op_sel_32, VX(c), VY(t), VZ(fv));
}

val umbra_f32_from_i32(builder *b, val x) { return math(b, op_f32_from_i32, VX(x)); }
val umbra_i32_from_f32(builder *b, val x) { return math(b, op_i32_from_f32, VX(x)); }

val umbra_eq_f32(builder *b, val x, val y) {
    sort(&x, &y);
    return try_imm(b, math(b, op_eq_f32, VX(x), VY(y)), op_eq_f32_imm, x, y);
}
val umbra_lt_f32(builder *b, val x, val y) {
    val const d = math(b, op_lt_f32, VX(x), VY(y));
    if (is_imm(b, val_id(y))) {
        return push(b, op_lt_f32_imm, VX(x), VY(y), .imm = b->inst[val_id(y)].imm);
    }
    return d;
}
val umbra_le_f32(builder *b, val x, val y) {
    val const d = math(b, op_le_f32, VX(x), VY(y));
    if (is_imm(b, val_id(y))) {
        return push(b, op_le_f32_imm, VX(x), VY(y), .imm = b->inst[val_id(y)].imm);
    }
    return d;
}
val umbra_eq_i32(builder *b, val x, val y) {
    sort(&x, &y);
    if (val_id(x) == val_id(y)) { return umbra_imm_i32(b, -1); }
    return try_imm(b, math(b, op_eq_i32, VX(x), VY(y)), op_eq_i32_imm, x, y);
}
val umbra_lt_s32(builder *b, val x, val y) {
    val const d = math(b, op_lt_s32, VX(x), VY(y));
    if (is_imm(b, val_id(y))) {
        return push(b, op_lt_s32_imm, VX(x), VY(y), .imm = b->inst[val_id(y)].imm);
    }
    return d;
}
val umbra_le_s32(builder *b, val x, val y) {
    val const d = math(b, op_le_s32, VX(x), VY(y));
    if (is_imm(b, val_id(y))) {
        return push(b, op_le_s32_imm, VX(x), VY(y), .imm = b->inst[val_id(y)].imm);
    }
    return d;
}
val umbra_lt_u32(builder *b, val x, val y) {
    return math(b, op_lt_u32, VX(x), VY(y));
}
val umbra_le_u32(builder *b, val x, val y) {
    return math(b, op_le_u32, VX(x), VY(y));
}
static char const* op_name(enum op op) {
    static char const *names[] = {
#define OP_NAME(name, ...) [op_##name] = #name,
        OTHER_OPS(OP_NAME)
        BINARY_OPS(OP_NAME)
        UNARY_OPS(OP_NAME)
        IMM_OPS(OP_NAME)
#undef OP_NAME
    };
    if ((unsigned)op < sizeof names / sizeof *names && names[op]) {
        return names[op];
    }
    return "?";
}

struct sched {
    int last_use, n_deps, n_users, user_off;
};

static _Bool is_body(struct bb_inst const *inst) {
    return inst->live && inst->varying;
}

static void schedule(struct bb_inst *in, int n, struct bb_inst *out, int preamble, int total) {
    struct sched *meta = calloc((size_t)(n + 1), sizeof *meta);

    for (int i = 0; i < n; i++) {
        meta[i].last_use = -1;
        if (is_body(in + i)) {
            int const deps[] = {in[i].x.id, in[i].y.id, in[i].z.id, in[i].w.id};
            for (int k = 0; k < 4; k++) {
                int const d = deps[k];
                meta[d].last_use = i;
                if (is_body(in + d)) {
                    meta[i].n_deps++;
                    meta[d].n_users++;
                }
            }
        }
    }

    for (int i = 0; i < n; i++) {
        meta[i + 1].user_off = meta[i].user_off + meta[i].n_users;
        meta[i].n_users = 0;
    }
    int *buf = calloc((size_t)(meta[n].user_off + n), sizeof *buf);
    int *users = buf;
    int *ready = buf + meta[n].user_off;

    for (int i = 0; i < n; i++) {
        if (is_body(in + i)) {
            int const deps[] = {in[i].x.id, in[i].y.id, in[i].z.id, in[i].w.id};
            for (int k = 0; k < 4; k++) {
                if (is_body(in + deps[k])) {
                    int const d = deps[k];
                    users[meta[d].user_off + meta[d].n_users++] = i;
                }
            }
        }
    }

    int nready = 0;
    for (int i = 0; i < n; i++) {
        if (is_body(in + i) && meta[i].n_deps == 0) {
            ready[nready++] = i;
        }
    }

    int j = preamble;
    int prev_scheduled = -1;
    while (nready > 0) {
        int best = 0, best_score = -9999;
        for (int r = 0; r < nready; r++) {
            int const id = ready[r];
            int const deps[] = {in[id].x.id, in[id].y.id, in[id].z.id, in[id].w.id};

            int kills = 0;
            for (int k = 0; k < 4; k++) {
                if (meta[deps[k]].last_use == id) {
                    kills++;
                }
            }

            int const defines = is_store(in[id].op) ? 0 : 1;
            int const net = kills - defines;
            int const lu = meta[id].last_use < 0 ? total : meta[id].last_use;
            // Bonus for chaining: if this op consumes the previous output,
            // the interpreter can keep the value in a register.
            int chain = 0;
            for (int k = 0; k < 3; k++) {
                if ((int)deps[k] == prev_scheduled) {
                    chain = 1;
                }
            }
            int const score = net * total - lu + chain * total * 2;
            if (best_score < score) {
                best_score = score;
                best = r;
            }
        }
        int const id = ready[best];
        ready[best] = ready[--nready];

        in[id].final_id = j;
        out[j++] = in[id];
        prev_scheduled = id;

        for (int u = meta[id].user_off; u < meta[id].user_off + meta[id].n_users; u++) {
            if (--meta[users[u]].n_deps == 0) { ready[nready++] = users[u]; }
        }
    }

    free(meta);
    free(buf);
}

struct umbra_basic_block* umbra_basic_block(builder *b) {
    int const n = b->insts;

    int live = 0;
    for (int i = n; i-- > 0;) {
        if (is_store(b->inst[i].op)) {
            b->inst[i].live = 1;
        }
        if (b->inst[i].live) {
            live++;
            b->inst[b->inst[i].x.id].live = 1;
            b->inst[b->inst[i].y.id].live = 1;
            b->inst[b->inst[i].z.id].live = 1;
            b->inst[b->inst[i].w.id].live = 1;
            if (ptr_is_deref(b->inst[i].ptr)) {
                b->inst[ptr_ix(b->inst[i].ptr)].live = 1;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        b->inst[i].varying = is_varying(b->inst[i].op)
                          || b->inst[b->inst[i].x.id].varying
                          || b->inst[b->inst[i].y.id].varying
                          || b->inst[b->inst[i].z.id].varying
                          || b->inst[b->inst[i].w.id].varying;
    }

    struct bb_inst *out = malloc((size_t)live * sizeof *out);
    int preamble = 0;
    for (int i = 0; i < n; i++) {
        if (b->inst[i].live && !b->inst[i].varying && !is_store(b->inst[i].op)) {
            b->inst[i].final_id = preamble;
            out[preamble++] = b->inst[i];
        }
    }

    schedule(b->inst, n, out, preamble, live);

    for (int i = 0; i < live; i++) {
        out[i].x = (val_){.id = b->inst[out[i].x.id].final_id, .chan = out[i].x.chan};
        out[i].y = (val_){.id = b->inst[out[i].y.id].final_id, .chan = out[i].y.chan};
        out[i].z = (val_){.id = b->inst[out[i].z.id].final_id, .chan = out[i].z.chan};
        out[i].w = (val_){.id = b->inst[out[i].w.id].final_id, .chan = out[i].w.chan};
        if (ptr_is_deref(out[i].ptr)) {
            out[i].ptr = ptr_deref(b->inst[ptr_ix(out[i].ptr)].final_id);
        }
    }

    struct umbra_basic_block *result = malloc(sizeof *result);
    result->inst     = out;
    result->insts    = live;
    result->preamble = preamble;
    return result;
}

void umbra_basic_block_free(struct umbra_basic_block *bb) {
    if (bb) {
        free(bb->inst);
        free(bb);
    }
}

static void dump_insts(struct bb_inst const *inst, int insts, FILE *f) {
    for (int i = 0; i < insts; i++) {
        struct bb_inst const *ip = &inst[i];
        enum op const         op = ip->op;

        if (is_store(op)) {
            if (op == op_store_8x4 || op == op_store_16x4 || op == op_store_16x4_planar) {
                fprintf(f, "      %-15s p%d v%d v%d v%d v%d\n", op_name(op), ip->ptr,
                        ip->x.id, ip->y.id, ip->z.id, ip->w.id);
            } else {
                fprintf(f, "      %-15s p%d v%d\n", op_name(op), ip->ptr, ip->y.id);
            }
            continue;
        }

        fprintf(f, "  v%-3d = %-15s", i, op_name(op));

        switch (op) {
        case op_imm_32: fprintf(f, " 0x%x", (uint32_t)ip->imm); break;
        case op_uniform_32:
            fprintf(f, " p%d byte%d", ip->ptr, ip->imm);
            break;
        case op_gather_uniform_32:
        case op_gather_32:
        case op_gather_16:
        case op_sample_32: fprintf(f, " p%d v%d", ip->ptr, ip->x.id); break;
        case op_load_16:
        case op_load_32:
        case op_load_16x4:
        case op_load_16x4_planar:
        case op_load_8x4: fprintf(f, " p%d", ip->ptr); break;
        case op_deref_ptr: fprintf(f, " p%d byte%d", ip->ptr, ip->imm); break;
        case op_x:
        case op_y:
        case op_store_16:
        case op_store_32:
        case op_store_16x4:
        case op_store_16x4_planar:
        case op_store_8x4: break;

        case op_sqrt_f32:
        case op_abs_f32:
        case op_round_f32:
        case op_floor_f32:
        case op_ceil_f32:
        case op_round_i32:
        case op_floor_i32:
        case op_ceil_i32:
        case op_f32_from_i32:
        case op_i32_from_f32:
        case op_i32_from_s16:
        case op_i32_from_u16:
        case op_i16_from_i32:
        case op_f32_from_f16:
        case op_f16_from_f32: fprintf(f, " v%d", ip->x.id); break;

        case op_add_f32:
        case op_sub_f32:
        case op_mul_f32:
        case op_div_f32:
        case op_min_f32:
        case op_max_f32:
        case op_add_i32:
        case op_sub_i32:
        case op_mul_i32:
        case op_shl_i32:
        case op_shr_u32:
        case op_shr_s32:
        case op_and_32:
        case op_or_32:
        case op_xor_32:
        case op_eq_f32:
        case op_lt_f32:
        case op_le_f32:
        case op_eq_i32:
        case op_lt_s32:
        case op_le_s32:
        case op_lt_u32:
        case op_le_u32: fprintf(f, " v%d v%d", ip->x.id, ip->y.id); break;

        case op_fma_f32:
        case op_fms_f32:
        case op_sel_32: fprintf(f, " v%d v%d v%d", ip->x.id, ip->y.id, ip->z.id); break;

        case op_shl_i32_imm:
        case op_shr_u32_imm:
        case op_shr_s32_imm: fprintf(f, " v%d %d", ip->x.id, ip->imm); break;
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
            fprintf(f, " v%d 0x%x", ip->x.id, (uint32_t)ip->imm);
            if (ip->y.bits) { fprintf(f, " (a.k.a. v%d)", ip->y.id); }
            break;
        }
        fprintf(f, "\n");
    }
}

void umbra_builder_dump(builder const *b, FILE *f) { dump_insts(b->inst, b->insts, f); }

void umbra_basic_block_dump(struct umbra_basic_block const *bb, FILE *f) {
    dump_insts(bb->inst, bb->insts, f);
}

int umbra_const_eval(enum op op, int xb, int yb, int zb) {
    typedef union { int i; float f; uint32_t u; } s32;
    s32 x = {xb},
        y = {yb},
        z = {zb},
        r = { 0};

    switch ((int)op) {
    case op_add_f32: r.f = x.f + y.f; return r.i;
    case op_sub_f32: r.f = x.f - y.f; return r.i;
    case op_mul_f32: r.f = x.f * y.f; return r.i;
    case op_div_f32: r.f = x.f / y.f; return r.i;
    case op_min_f32: r.f = x.f < y.f ? x.f : y.f; return r.i;
    case op_max_f32: r.f = x.f > y.f ? x.f : y.f; return r.i;
    case op_sqrt_f32:  r.f = sqrtf(x.f); return r.i;
    case op_abs_f32:   r.f = fabsf(x.f); return r.i;
    case op_round_f32: r.f = rintf(x.f); return r.i;
    case op_floor_f32: r.f = floorf(x.f); return r.i;
    case op_ceil_f32:  r.f = ceilf(x.f); return r.i;
    case op_round_i32: { float t = rintf(x.f);  r.i = (int32_t)t; } return r.i;
    case op_floor_i32: { float t = floorf(x.f); r.i = (int32_t)t; } return r.i;
    case op_ceil_i32:  { float t = ceilf(x.f);  r.i = (int32_t)t; } return r.i;
    // op_fma_f32 / op_fms_f32: only produced by the mul→fma rewrite in
    // umbra_add_f32 / umbra_sub_f32, which only fires when an operand's
    // op == op_mul_f32. By the time we'd want to fold a fully-imm fma,
    // the inner mul has already been folded to op_imm_32, so we never
    // reach a const_eval with op_fma_f32 or op_fms_f32.
    //
    // op_shl_i32 / op_shr_u32 / op_shr_s32: the builders short-circuit
    // to op_*_imm whenever the shift amount is imm, so the non-imm shift
    // forms never reach const_eval with all-imm inputs either.
    case op_add_i32: r.i = x.i + y.i; return r.i;
    case op_sub_i32: r.i = x.i - y.i; return r.i;
    case op_mul_i32: r.i = x.i * y.i; return r.i;
    case op_and_32:  r.i = x.i & y.i; return r.i;
    case op_or_32:   r.i = x.i | y.i; return r.i;
    case op_xor_32:  r.i = x.i ^ y.i; return r.i;
    case op_sel_32:  r.i = (x.i & y.i) | (~x.i & z.i); return r.i;
    case op_f32_from_i32: r.f = (float)x.i; return r.i;
    case op_i32_from_f32: r.i = (int32_t)x.f; return r.i;
    case op_eq_f32: r.i = x.f == y.f ? -1 : 0; return r.i;
    case op_lt_f32: r.i = x.f <  y.f ? -1 : 0; return r.i;
    case op_le_f32: r.i = x.f <= y.f ? -1 : 0; return r.i;
    case op_eq_i32: r.i = x.i == y.i ? -1 : 0; return r.i;
    case op_lt_s32: r.i = x.i <  y.i ? -1 : 0; return r.i;
    case op_le_s32: r.i = x.i <= y.i ? -1 : 0; return r.i;
    case op_lt_u32: r.i = x.u <  y.u ? -1 : 0; return r.i;
    case op_le_u32: r.i = x.u <= y.u ? -1 : 0; return r.i;
    }
    assume(0);
}
