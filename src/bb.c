#include "../include/umbra.h"
#include "bb.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#pragma clang diagnostic ignored "-Wfloat-equal"

static int       val_id  (umbra_val v)       { return ((val_){.bits = v.bits}).id; }
static umbra_val val_make(int id, int chan)  { return (umbra_val){.bits = ((val_){.id=(unsigned)id, .chan=(unsigned)chan}).bits}; }

typedef struct umbra_builder builder;
typedef umbra_val            val;

size_t umbra_fmt_size(umbra_fmt fmt) {
    switch (fmt) {
    case umbra_fmt_8888:       return 4;
    case umbra_fmt_565:        return 2;
    case umbra_fmt_1010102:    return 4;
    case umbra_fmt_fp16:       return 8;
    case umbra_fmt_fp16_planar: return 2;
    }
    return 0;
}

static int ptr_ix(umbra_ptr p) { return p.deref ? ~p.ix : p.ix; }

_Bool is_store(enum op op) {
    return op == op_store_16
        || op == op_store_32
        || op == op_store_fp16x4
        || op == op_store_fp16x4_planar;
}
_Bool has_ptr(enum op op) {
    return op == op_uniform_32
        || op == op_load_32
        || op == op_load_fp16x4
        || op == op_load_fp16x4_planar
        || op == op_gather_uniform_32
        || op == op_gather_32
        || op == op_store_32
        || op == op_store_fp16x4
        || op == op_store_fp16x4_planar
        || op == op_deref_ptr
        || op == op_load_16
        || op == op_store_16
        || op == op_gather_16;
}
_Bool is_fused_imm(enum op op) {
    return op == op_add_f32_imm
        || op == op_sub_f32_imm
        || op == op_mul_f32_imm
        || op == op_div_f32_imm
        || op == op_min_f32_imm
        || op == op_max_f32_imm
        || op == op_add_i32_imm
        || op == op_sub_i32_imm
        || op == op_mul_i32_imm
        || op == op_and_32_imm
        || op == op_or_32_imm
        || op == op_xor_32_imm
        || op == op_eq_f32_imm
        || op == op_lt_f32_imm
        || op == op_le_f32_imm
        || op == op_eq_i32_imm
        || op == op_lt_s32_imm
        || op == op_le_s32_imm;
}

_Bool is_varying(enum op op) {
    return op == op_x
        || op == op_y
        || op == op_load_16
        || op == op_load_32
        || op == op_load_fp16x4
        || op == op_load_fp16x4_planar
        || op == op_store_16
        || op == op_store_32
        || op == op_store_fp16x4
        || op == op_store_fp16x4_planar;
}

static _Bool is_pow2(int x) { return __builtin_popcount((unsigned)x) == 1; }
static _Bool is_pow2_or_zero(int x) { return __builtin_popcount((unsigned)x) <= 1; }

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

static val push_(builder *b, struct bb_inst inst) {
    {
        enum op const op = inst.op;
        if (op == op_imm_32 || op == op_uniform_32 || op == op_deref_ptr) {
            inst.uniform = 1;
        } else if (is_varying(op)
                   || op == op_gather_32
                   || op == op_gather_16) {
            inst.uniform = 0;
        } else {
            inst.uniform = (!inst.x.bits || b->inst[inst.x.id].uniform)
                && (!inst.y.bits || b->inst[inst.y.id].uniform)
                && (!inst.z.bits || b->inst[inst.z.id].uniform)
                && (!inst.w.bits || b->inst[inst.w.id].uniform);
        }
    }

    uint32_t const h = bb_inst_hash(&inst);

    for (int slot = (int)h; b->ht_mask; slot++) {
        if (b->ht[slot & b->ht_mask].hash == 0) { break; }
        if (h == b->ht[slot & b->ht_mask].hash
            && 0
                == __builtin_memcmp(&inst, &b->inst[b->ht[slot & b->ht_mask].ix],
                                    sizeof inst)) {
            return (val){.bits = ((val_){.id = (unsigned)b->ht[slot & b->ht_mask].ix}).bits};
        }
    }

    if (is_pow2_or_zero(b->insts)) {
        int const inst_cap = b->insts ? 2 * b->insts : 1, ht_cap = 2 * inst_cap;
        b->inst = realloc(b->inst, (size_t)inst_cap * sizeof *b->inst);

        struct hash_slot *old = b->ht;
        int const         old_cap = b->ht ? b->ht_mask + 1 : 0;
        b->ht = calloc((size_t)ht_cap, sizeof *b->ht);
        b->ht_mask = ht_cap - 1;
        for (int i = 0; i < old_cap; i++) {
            for (int slot = (int)old[i].hash; old[i].hash; slot++) {
                if (b->ht[slot & b->ht_mask].hash == 0) {
                    b->ht[slot & b->ht_mask] = old[i];
                    break;
                }
            }
        }
        free(old);
    }

    int const id = b->insts++;
    b->inst[id] = inst;
    if (!is_store(inst.op)) {
        for (int slot = (int)h;; slot++) {
            if (b->ht[slot & b->ht_mask].hash == 0) {
                b->ht[slot & b->ht_mask] = (struct hash_slot){h, id};
                break;
            }
        }
    }
    return (val){.bits = ((val_){.id = (unsigned)id}).bits};
}
#define push(b, ...) push_(b, (struct bb_inst){.op = __VA_ARGS__})
#define VX(v) .x = (val_){.bits = (v).bits}
#define VY(v) .y = (val_){.bits = (v).bits}
#define VZ(v) .z = (val_){.bits = (v).bits}
#define VW(v) .w = (val_){.bits = (v).bits}

builder* umbra_builder(void) {
    builder *b = calloc(1, sizeof *b);
    // Simplifies liveness analysis to know id 0 is imm=0.
    push(b, op_imm_32, .imm = 0);
    return b;
}

void umbra_builder_free(builder *b) {
    if (b) {
        free(b->inst);
        free(b->ht);
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

int umbra_reserve(builder *b, int n) {
    b->uni_len = (b->uni_len + 3) & ~3;
    int const ix = b->uni_len / 4;
    b->uni_len += n * 4;
    return ix;
}
int umbra_reserve_ptr(builder *b) {
    b->uni_len = (b->uni_len + 7) & ~7;
    int const off = b->uni_len;
    b->uni_len += 24;
    return off;
}
umbra_ptr umbra_deref_ptr(builder *b, umbra_ptr buf, int byte_off) {
    val const v = push(b, op_deref_ptr, .ptr = ptr_ix(buf), .imm = byte_off);
    return (umbra_ptr){.ix = val_id(v), .deref = 1};
}
int  umbra_uni_len(builder const *b) { return b->uni_len; }
void umbra_set_uni_len(builder *b, int len) { b->uni_len = len; }


int umbra_max_ptr(builder const *b) {
    int m = 0;
    for (int i = 0; i < b->insts; i++) {
        if (has_ptr(b->inst[i].op) && m < b->inst[i].ptr) { m = b->inst[i].ptr; }
    }
    return m;
}

val umbra_gather_16(builder *b, umbra_ptr src, val ix) {
    return push(b, op_gather_16, VX(ix), .ptr = ptr_ix(src));
}
val umbra_load_32(builder *b, umbra_ptr src) {
    return push(b, op_load_32, .ptr = ptr_ix(src));
}
val umbra_load_16(builder *b, umbra_ptr src) {
    return push(b, op_load_16, .ptr = ptr_ix(src));
}
val umbra_uniform_32(builder *b, umbra_ptr src, int slot) {
    return push(b, op_uniform_32, .imm = slot, .ptr = ptr_ix(src));
}
val umbra_gather_32(builder *b, umbra_ptr src, val ix) {
    enum op const op = b->inst[val_id(ix)].uniform ? op_gather_uniform_32 : op_gather_32;
    return push(b, op, VX(ix), .ptr = ptr_ix(src));
}
void umbra_store_32(builder *b, umbra_ptr dst, val v) {
    push(b, op_store_32, VY(v), .ptr = ptr_ix(dst));
}
void umbra_store_16(builder *b, umbra_ptr dst, val v) {
    push(b, op_store_16, VY(v), .ptr = ptr_ix(dst));
}
static umbra_color fp16x4_color(val px) {
    int const id = val_id((umbra_val){.bits = px.bits});
    return (umbra_color){
        val_make(id, 0), val_make(id, 1),
        val_make(id, 2), val_make(id, 3),
    };
}
umbra_color umbra_load_color(builder *b, umbra_ptr src, umbra_fmt fmt) {
    switch (fmt) {
    case umbra_fmt_8888: {
        val const px   = umbra_load_32(b, src);
        val const mask = umbra_imm_i32(b, 0xFF);
        val const inv  = umbra_imm_f32(b, 1.0f/255);
        val const ri   = umbra_and_i32(b, px, mask);
        val const gi   = umbra_and_i32(b, umbra_shr_u32(b, px, umbra_imm_i32(b,  8)), mask);
        val const bi   = umbra_and_i32(b, umbra_shr_u32(b, px, umbra_imm_i32(b, 16)), mask);
        val const ai   = umbra_shr_u32(b, px, umbra_imm_i32(b, 24));
        return (umbra_color){
            umbra_mul_f32(b, umbra_f32_from_i32(b, ri), inv),
            umbra_mul_f32(b, umbra_f32_from_i32(b, gi), inv),
            umbra_mul_f32(b, umbra_f32_from_i32(b, bi), inv),
            umbra_mul_f32(b, umbra_f32_from_i32(b, ai), inv),
        };
    }
    case umbra_fmt_565: {
        val const px = umbra_i32_from_u16(b, umbra_load_16(b, src));
        val const r5 = umbra_shr_u32(b, px, umbra_imm_i32(b, 11));
        val const g6 = umbra_and_i32(b, umbra_shr_u32(b, px, umbra_imm_i32(b, 5)),
                                     umbra_imm_i32(b, 0x3F));
        val const b5 = umbra_and_i32(b, px, umbra_imm_i32(b, 0x1F));
        return (umbra_color){
            umbra_mul_f32(b, umbra_f32_from_i32(b, r5), umbra_imm_f32(b, 1.0f/31)),
            umbra_mul_f32(b, umbra_f32_from_i32(b, g6), umbra_imm_f32(b, 1.0f/63)),
            umbra_mul_f32(b, umbra_f32_from_i32(b, b5), umbra_imm_f32(b, 1.0f/31)),
            umbra_imm_f32(b, 1.0f),
        };
    }
    case umbra_fmt_1010102: {
        val const px   = umbra_load_32(b, src);
        val const mask = umbra_imm_i32(b, 0x3FF);
        val const ri   = umbra_and_i32(b, px, mask);
        val const gi   = umbra_and_i32(b, umbra_shr_u32(b, px, umbra_imm_i32(b, 10)), mask);
        val const bi   = umbra_and_i32(b, umbra_shr_u32(b, px, umbra_imm_i32(b, 20)), mask);
        val const ai   = umbra_shr_u32(b, px, umbra_imm_i32(b, 30));
        return (umbra_color){
            umbra_mul_f32(b, umbra_f32_from_i32(b, ri), umbra_imm_f32(b, 1.0f/1023)),
            umbra_mul_f32(b, umbra_f32_from_i32(b, gi), umbra_imm_f32(b, 1.0f/1023)),
            umbra_mul_f32(b, umbra_f32_from_i32(b, bi), umbra_imm_f32(b, 1.0f/1023)),
            umbra_mul_f32(b, umbra_f32_from_i32(b, ai), umbra_imm_f32(b, 1.0f/3)),
        };
    }
    case umbra_fmt_fp16:
        return fp16x4_color(push(b, op_load_fp16x4, .ptr = ptr_ix(src)));
    case umbra_fmt_fp16_planar:
        return fp16x4_color(push(b, op_load_fp16x4_planar, .ptr = ptr_ix(src)));
    }
    return (umbra_color){0};
}
static val pack_unorm(builder *b, val ch, val scale) {
    val zero = umbra_imm_f32(b, 0.0f), one = umbra_imm_f32(b, 1.0f);
    return umbra_round_i32(b, umbra_mul_f32(b,
        umbra_min_f32(b, umbra_max_f32(b, ch, zero), one), scale));
}
static val pack(builder *b, val base, val v, int shift) {
    return push(b, op_pack, VX(base), VY(v), .imm = shift);
}
void umbra_store_color(builder *b, umbra_ptr dst, umbra_color c, umbra_fmt fmt) {
    switch (fmt) {
    case umbra_fmt_8888: {
        val s = umbra_imm_f32(b, 255.0f);
        val px = pack_unorm(b, c.r, s);
        px = pack(b, px, pack_unorm(b, c.g, s),  8);
        px = pack(b, px, pack_unorm(b, c.b, s), 16);
        px = pack(b, px, pack_unorm(b, c.a, s), 24);
        umbra_store_32(b, dst, px);
    } break;
    case umbra_fmt_565: {
        val px = pack_unorm(b, c.b, umbra_imm_f32(b, 31.0f));
        px = pack(b, px, pack_unorm(b, c.g, umbra_imm_f32(b, 63.0f)), 5);
        px = pack(b, px, pack_unorm(b, c.r, umbra_imm_f32(b, 31.0f)), 11);
        umbra_store_16(b, dst, umbra_i16_from_i32(b, px));
    } break;
    case umbra_fmt_1010102: {
        val s10 = umbra_imm_f32(b, 1023.0f);
        val px = pack_unorm(b, c.r, s10);
        px = pack(b, px, pack_unorm(b, c.g, s10), 10);
        px = pack(b, px, pack_unorm(b, c.b, s10), 20);
        px = pack(b, px, pack_unorm(b, c.a, umbra_imm_f32(b, 3.0f)), 30);
        umbra_store_32(b, dst, px);
    } break;
    case umbra_fmt_fp16:
        push(b, op_store_fp16x4, VX(c.r), VY(c.g), VZ(c.b), VW(c.a),
             .ptr = ptr_ix(dst));
        break;
    case umbra_fmt_fp16_planar:
        push(b, op_store_fp16x4_planar, VX(c.r), VY(c.g), VZ(c.b), VW(c.a),
             .ptr = ptr_ix(dst));
        break;
    }
}
val umbra_i32_from_s16(builder *b, val x) { return push(b, op_i32_from_s16, VX(x)); }
val umbra_i32_from_u16(builder *b, val x) { return push(b, op_i32_from_u16, VX(x)); }
val umbra_i16_from_i32(builder *b, val x) { return push(b, op_i16_from_i32, VX(x)); }

val umbra_f32_from_f16(builder *b, val x) { return push(b, op_f32_from_f16, VX(x)); }
val umbra_f16_from_f32(builder *b, val x) { return push(b, op_f16_from_f32, VX(x)); }

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
    return (val){.bits = d.bits};
}

static void sort(val *a, val *b) {
    if (b->bits < a->bits) {
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
    if (is_imm32(b, val_id(x), 0)) { return umbra_neg_f32(b, y); }
    if (b->inst[val_id(y)].op == op_mul_f32) {
        return math(b, op_fms_f32, .x = b->inst[val_id(y)].x, .y = b->inst[val_id(y)].y, VZ(x));
    }
    val const d = math(b, op_sub_f32, VX(x), VY(y));
    if (is_imm(b, val_id(y))) {
        return push(b, op_sub_f32_imm, VX(x), VY(y), .imm = b->inst[val_id(y)].imm);
    }
    return (val){.bits = d.bits};
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
    return (val){.bits = d.bits};
}

val umbra_min_f32(builder *b, val x, val y) {
    sort(&x, &y);
    return try_imm(b, math(b, op_min_f32, VX(x), VY(y)), op_min_f32_imm, x,
                   y);
}

val umbra_max_f32(builder *b, val x, val y) {
    if (b->inst[val_id(x)].op == op_neg_f32 && b->inst[val_id(x)].x.id == (unsigned)val_id(y)) {
        return umbra_abs_f32(b, y);
    }
    if (b->inst[val_id(y)].op == op_neg_f32 && b->inst[val_id(y)].x.id == (unsigned)val_id(x)) {
        return umbra_abs_f32(b, x);
    }
    sort(&x, &y);
    return try_imm(b, math(b, op_max_f32, VX(x), VY(y)), op_max_f32_imm, x,
                   y);
}

val umbra_sqrt_f32(builder *b, val x) { return math(b, op_sqrt_f32, VX(x)); }
val umbra_abs_f32(builder *b, val x) { return math(b, op_abs_f32, VX(x)); }
val umbra_neg_f32(builder *b, val x) { return math(b, op_neg_f32, VX(x)); }
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
    return (val){.bits = d.bits};
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

val umbra_and_i32(builder *b, val x, val y) {
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
    return (val){.bits = d.bits};
}
val umbra_or_i32(builder *b, val x, val y) {
    sort(&x, &y);
    if (val_id(x) == val_id(y)) { return x; }
    if (is_imm32(b, val_id(x), 0)) { return y; }
    if (is_imm32(b, val_id(x), -1)) { return x; }
    if (is_imm32(b, val_id(y), -1)) { return y; }
    return try_imm(b, math(b, op_or_32, VX(x), VY(y)), op_or_32_imm, x, y);
}
val umbra_xor_i32(builder *b, val x, val y) {
    sort(&x, &y);
    if (val_id(x) == val_id(y)) { return umbra_imm_i32(b, 0); }
    if (is_imm32(b, val_id(x), 0)) { return y; }
    return try_imm(b, math(b, op_xor_32, VX(x), VY(y)), op_xor_32_imm, x, y);
}
val umbra_sel_i32(builder *b, val c, val t, val fv) {
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
    return (val){.bits = d.bits};
}
val umbra_le_f32(builder *b, val x, val y) {
    val const d = math(b, op_le_f32, VX(x), VY(y));
    if (is_imm(b, val_id(y))) {
        return push(b, op_le_f32_imm, VX(x), VY(y), .imm = b->inst[val_id(y)].imm);
    }
    return (val){.bits = d.bits};
}
val umbra_gt_f32(builder *b, val x, val y) { return umbra_lt_f32(b, y, x); }
val umbra_ge_f32(builder *b, val x, val y) { return umbra_le_f32(b, y, x); }

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
    return (val){.bits = d.bits};
}
val umbra_le_s32(builder *b, val x, val y) {
    val const d = math(b, op_le_s32, VX(x), VY(y));
    if (is_imm(b, val_id(y))) {
        return push(b, op_le_s32_imm, VX(x), VY(y), .imm = b->inst[val_id(y)].imm);
    }
    return (val){.bits = d.bits};
}
val umbra_lt_u32(builder *b, val x, val y) {
    return math(b, op_lt_u32, VX(x), VY(y));
}
val umbra_le_u32(builder *b, val x, val y) {
    return math(b, op_le_u32, VX(x), VY(y));
}
static char const* op_name(enum op op) {
    static char const *names[] = {
#define OP_NAME(name) [op_##name] = #name,
        OP_LIST(OP_NAME)
#undef OP_NAME
    };
    if ((unsigned)op < sizeof names / sizeof *names && names[op]) { return names[op]; }
    return "?";
}

static void schedule(struct bb_inst const *in, int n, _Bool const *body,
                     struct bb_inst *out, int *old_to_new, int preamble, int total) {
    int *last_use = calloc((size_t)n, sizeof *last_use);
    int *n_deps = calloc((size_t)n, sizeof *n_deps);
    int *n_users = calloc((size_t)n, sizeof *n_users);

    for (int i = 0; i < n; i++) { last_use[i] = -1; }
    for (int i = 0; i < n; i++) {
        if (!body[i]) { continue; }
        int const deps[] = {(int)in[i].x.id, (int)in[i].y.id, (int)in[i].z.id, (int)in[i].w.id};
        for (int k = 0; k < 4; k++) { last_use[deps[k]] = i; }
    }
    for (int i = 0; i < n; i++) {
        if (!body[i]) { continue; }
        int const deps[] = {(int)in[i].x.id, (int)in[i].y.id, (int)in[i].z.id, (int)in[i].w.id};
        for (int k = 0; k < 4; k++) {
            if (body[deps[k]]) {
                n_deps[i]++;
                n_users[deps[k]]++;
            }
        }
    }

    int *user_off = calloc((size_t)(n + 1), sizeof *user_off);
    for (int i = 0; i < n; i++) {
        user_off[i + 1] = user_off[i] + n_users[i];
        n_users[i] = 0;
    }
    int *users = calloc((size_t)user_off[n], sizeof *users);
    for (int i = 0; i < n; i++) {
        if (!body[i]) { continue; }
        n_users[i] = 0;
    }
    for (int i = 0; i < n; i++) {
        if (!body[i]) { continue; }
        int const deps[] = {(int)in[i].x.id, (int)in[i].y.id, (int)in[i].z.id, (int)in[i].w.id};
        for (int k = 0; k < 4; k++) {
            if (body[deps[k]]) { users[user_off[deps[k]] + n_users[deps[k]]++] = i; }
        }
    }

    int *ready = calloc((size_t)n, sizeof *ready);
    int  nready = 0;
    for (int i = 0; i < n; i++) {
        if (body[i] && n_deps[i] == 0) { ready[nready++] = i; }
    }

    int j = preamble;
    int prev_scheduled = -1;
    while (nready > 0) {
        int best = 0, best_score = -9999;
        for (int r = 0; r < nready; r++) {
            int const id = ready[r];
            int       kills = 0;
            int const deps[] = {(int)in[id].x.id, (int)in[id].y.id, (int)in[id].z.id, (int)in[id].w.id};
            for (int k = 0; k < 4; k++) {
                if (last_use[deps[k]] == id) { kills++; }
            }
            int const defines = is_store(in[id].op) ? 0 : 1;
            int const net = kills - defines;
            int const lu = last_use[id] < 0 ? total : last_use[id];
            // Bonus for chaining: if this op consumes the previous output,
            // the interpreter can keep the value in a register.
            int chain = 0;
            for (int k = 0; k < 3; k++) {
                if (deps[k] == prev_scheduled) { chain = 1; }
            }
            int const score = net * total - lu + chain * total * 2;
            if (best_score < score) {
                best_score = score;
                best = r;
            }
        }
        int const id = ready[best];
        ready[best] = ready[--nready];

        old_to_new[id] = j;
        out[j++] = in[id];
        prev_scheduled = id;

        for (int u = user_off[id]; u < user_off[id] + n_users[id]; u++) {
            if (--n_deps[users[u]] == 0) { ready[nready++] = users[u]; }
        }
    }

    free(last_use);
    free(n_deps);
    free(users);
    free(user_off);
    free(n_users);
    free(ready);
}

struct umbra_basic_block* umbra_basic_block(builder *b) {
    int const n = b->insts;

    _Bool *live = calloc((size_t)n, 1);
    _Bool *varying = calloc((size_t)n, 1);

    for (int i = n; i-- > 0;) {
        if (is_store(b->inst[i].op)) { live[i] = 1; }
        if (live[i]) {
            live[(int)b->inst[i].x.id] = 1;
            live[(int)b->inst[i].y.id] = 1;
            live[(int)b->inst[i].z.id] = 1;
            live[(int)b->inst[i].w.id] = 1;
            if (b->inst[i].ptr < 0) { live[~b->inst[i].ptr] = 1; }
        }
    }
    for (int i = 0; i < n; i++) {
        varying[i] = is_varying(b->inst[i].op)
            || varying[(int)b->inst[i].x.id]
            || varying[(int)b->inst[i].y.id]
            || varying[(int)b->inst[i].z.id]
            || varying[(int)b->inst[i].w.id];
    }

    int total = 0;
    for (int i = 0; i < n; i++) { total += live[i]; }

    struct bb_inst *out = malloc((size_t)total * sizeof *out);
    int            *old_to_new = malloc((size_t)n * sizeof *old_to_new);
    for (int i = 0; i < n; i++) { old_to_new[i] = -1; }

    int j = 0;
    for (int i = 0; i < n; i++) {
        if (!live[i] || varying[i] || is_store(b->inst[i].op)) { continue; }
        old_to_new[i] = j;
        out[j++] = b->inst[i];
    }
    int const preamble = j;

    _Bool *body = calloc((size_t)n, 1);
    for (int i = 0; i < n; i++) { body[i] = live[i] && varying[i]; }
    schedule(b->inst, n, body, out, old_to_new, preamble, total);
    free(body);

    for (int i = 0; i < total; i++) {
        out[i].x = (val_){.id = (unsigned)old_to_new[out[i].x.id], .chan = out[i].x.chan};
        out[i].y = (val_){.id = (unsigned)old_to_new[out[i].y.id], .chan = out[i].y.chan};
        out[i].z = (val_){.id = (unsigned)old_to_new[out[i].z.id], .chan = out[i].z.chan};
        out[i].w = (val_){.id = (unsigned)old_to_new[out[i].w.id], .chan = out[i].w.chan};
        if (out[i].ptr < 0) { out[i].ptr = ~old_to_new[~out[i].ptr]; }
    }

    free(live);
    free(varying);
    free(old_to_new);

    struct umbra_basic_block *result = malloc(sizeof *result);
    result->inst = out;
    result->insts = total;
    result->preamble = preamble;
    result->uni_len = b->uni_len;
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
            fprintf(f, "      %-15s p%d v%d\n", op_name(op), ip->ptr, (int)ip->y.id);
            continue;
        }

        fprintf(f, "  v%-3d = %-15s", i, op_name(op));

        switch (op) {
        case op_imm_32: fprintf(f, " 0x%x", (uint32_t)ip->imm); break;
        case op_uniform_32:
            fprintf(f, " p%d[%d]", ip->ptr, ip->imm);
            break;
        case op_gather_uniform_32:
        case op_gather_32:
        case op_gather_16: fprintf(f, " p%d v%d", ip->ptr, (int)ip->x.id); break;
        case op_load_16:
        case op_load_32:
        case op_load_fp16x4:
        case op_load_fp16x4_planar: fprintf(f, " p%d", ip->ptr); break;
        case op_deref_ptr: fprintf(f, " p%d byte%d", ip->ptr, ip->imm); break;
        case op_x:
        case op_y: break;

        case op_store_16:
        case op_store_32:
        case op_store_fp16x4:
        case op_store_fp16x4_planar: break;

        case op_sqrt_f32:
        case op_abs_f32:
        case op_neg_f32:
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
        case op_f16_from_f32: fprintf(f, " v%d", (int)ip->x.id); break;

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
        case op_le_u32: fprintf(f, " v%d v%d", (int)ip->x.id, (int)ip->y.id); break;

        case op_fma_f32:
        case op_fms_f32:
        case op_sel_32: fprintf(f, " v%d v%d v%d", (int)ip->x.id, (int)ip->y.id, (int)ip->z.id); break;

        case op_shl_i32_imm:
        case op_shr_u32_imm:
        case op_shr_s32_imm: fprintf(f, " v%d %d", (int)ip->x.id, ip->imm); break;
        case op_pack: fprintf(f, " v%d v%d %d", (int)ip->x.id, (int)ip->y.id, ip->imm); break;
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
            fprintf(f, " v%d 0x%x", (int)ip->x.id, (uint32_t)ip->imm);
            if (ip->y.bits) { fprintf(f, " (a.k.a. v%d)", (int)ip->y.id); }
            break;
        }
        fprintf(f, "\n");
    }
}

void umbra_dump_builder(builder const *b, FILE *f) { dump_insts(b->inst, b->insts, f); }

void umbra_dump_basic_block(struct umbra_basic_block const *bb, FILE *f) {
    dump_insts(bb->inst, bb->insts, f);
}

int umbra_const_eval(enum op op, int xb, int yb, int zb) {
    typedef union { float f; int32_t i; uint32_t u; } s32;
    s32 x, y, z, r = {.i = 0};
    __builtin_memcpy(&x, &xb, 4);
    __builtin_memcpy(&y, &yb, 4);
    __builtin_memcpy(&z, &zb, 4);

    switch ((int)op) {
    case op_add_f32: r.f = x.f + y.f; break;
    case op_sub_f32: r.f = x.f - y.f; break;
    case op_mul_f32: r.f = x.f * y.f; break;
    case op_div_f32: r.f = x.f / y.f; break;
    case op_min_f32: r.f = x.f < y.f ? x.f : y.f; break;
    case op_max_f32: r.f = x.f > y.f ? x.f : y.f; break;
    case op_sqrt_f32:  r.f = sqrtf(x.f); break;
    case op_abs_f32:   r.f = fabsf(x.f); break;
    case op_neg_f32:   r.f = -x.f; break;
    case op_round_f32: r.f = rintf(x.f); break;
    case op_floor_f32: r.f = floorf(x.f); break;
    case op_ceil_f32:  r.f = ceilf(x.f); break;
    case op_round_i32: { float t = rintf(x.f);  r.i = (int32_t)t; } break;
    case op_floor_i32: { float t = floorf(x.f); r.i = (int32_t)t; } break;
    case op_ceil_i32:  { float t = ceilf(x.f);  r.i = (int32_t)t; } break;
    case op_fma_f32:   r.f = z.f + x.f * y.f; break;
    case op_fms_f32:   r.f = z.f - x.f * y.f; break;
    case op_add_i32: r.i = x.i + y.i; break;
    case op_sub_i32: r.i = x.i - y.i; break;
    case op_mul_i32: r.i = x.i * y.i; break;
    case op_shl_i32: r.i = x.i << y.i; break;
    case op_shr_u32: r.u = x.u >> y.u; break;
    case op_shr_s32: r.i = x.i >> y.i; break;
    case op_and_32:  r.i = x.i & y.i; break;
    case op_or_32:   r.i = x.i | y.i; break;
    case op_xor_32:  r.i = x.i ^ y.i; break;
    case op_sel_32:  r.i = (x.i & y.i) | (~x.i & z.i); break;
    case op_f32_from_i32: r.f = (float)x.i; break;
    case op_i32_from_f32: r.i = (int32_t)x.f; break;
    case op_eq_f32: r.i = x.f == y.f ? -1 : 0; break;
    case op_lt_f32: r.i = x.f <  y.f ? -1 : 0; break;
    case op_le_f32: r.i = x.f <= y.f ? -1 : 0; break;
    case op_eq_i32: r.i = x.i == y.i ? -1 : 0; break;
    case op_lt_s32: r.i = x.i <  y.i ? -1 : 0; break;
    case op_le_s32: r.i = x.i <= y.i ? -1 : 0; break;
    case op_lt_u32: r.i = x.u <  y.u ? -1 : 0; break;
    case op_le_u32: r.i = x.u <= y.u ? -1 : 0; break;

    default: assert(0); break;
    }

    int result;
    __builtin_memcpy(&result, &r, 4);
    return result;
}
