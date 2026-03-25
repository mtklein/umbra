#include "../include/umbra.h"
#include "bb.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct umbra_builder builder;
typedef umbra_val            val;

_Bool is_store(enum op op) {
    return op == op_store_16
        || op == op_store_next_16
        || op == op_store_32
        || op == op_store_next_32
        || op == op_store_next_64
        || op == op_scatter_16
        || op == op_scatter_32;
}
_Bool has_ptr(enum op op) {
    return op == op_deref_ptr
        || (op >= op_uni_32 && op <= op_scatter_32)
        || (op >= op_uni_16 && op <= op_scatter_16);
}
_Bool is_varying(enum op op) {
    return op == op_iota
        || op == op_x
        || op == op_y
        || op == op_load_16
        || op == op_load_next_16
        || op == op_load_32
        || op == op_load_next_32
        || op == op_load_next_64_lo
        || op == op_load_next_64_hi
        || op == op_store_16
        || op == op_store_next_16
        || op == op_store_32
        || op == op_store_next_32
        || op == op_store_next_64;
}

static _Bool is_pow2(int x) { return __builtin_popcount((unsigned)x) == 1; }
static _Bool is_pow2_or_zero(int x) { return __builtin_popcount((unsigned)x) <= 1; }

static uint32_t mul_overflow(uint32_t x, uint32_t y) {
    __builtin_mul_overflow(x, y, &x);
    return x;
}

static uint32_t bb_inst_hash(struct bb_inst const *inst) {
    uint32_t h = (uint32_t)inst->op;
    h = mul_overflow(0x9e3779b9, h ^ (uint32_t)inst->x);
    h = mul_overflow(0x9e3779b9, h ^ (uint32_t)inst->y);
    h = mul_overflow(0x9e3779b9, h ^ (uint32_t)inst->z);
    h = mul_overflow(0x9e3779b9, h ^ (uint32_t)inst->ptr);
    h = mul_overflow(0x9e3779b9, h ^ (uint32_t)inst->imm);
    h ^= h >> 16;
    return h ? h : 1;
}

static val push_(builder *b, struct bb_inst inst) {
    {
        enum op op = inst.op;
        if (op == op_imm_32 || op == op_uni_32 || op == op_uni_16 || op == op_deref_ptr) {
            inst.uniform = 1;
        } else if (is_varying(op)
                   || op == op_iota
                   || op == op_gather_32
                   || op == op_gather_16
                   || op == op_scatter_32
                   || op == op_scatter_16) {
            inst.uniform = 0;
        } else {
            inst.uniform = (!inst.x || b->inst[inst.x].uniform)
                && (!inst.y || b->inst[inst.y].uniform)
                && (!inst.z || b->inst[inst.z].uniform);
        }
    }

    uint32_t const h = bb_inst_hash(&inst);

    for (int slot = (int)h; b->ht_mask; slot++) {
        if (b->ht[slot & b->ht_mask].hash == 0) { break; }
        if (h == b->ht[slot & b->ht_mask].hash
            && 0
                == __builtin_memcmp(&inst, &b->inst[b->ht[slot & b->ht_mask].ix],
                                    sizeof inst)) {
            return (val){b->ht[slot & b->ht_mask].ix};
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
    return (val){id};
}
#define push(b, ...) push_(b, (struct bb_inst){.op = __VA_ARGS__})

builder *umbra_builder(void) {
    builder *b = calloc(1, sizeof *b);
    // Simplifies liveness analysis to know id 0 is imm=0.
    push(b, op_imm_32, .imm = 0);
    return b;
}

void umbra_builder_free(builder *b) {
    free(b->inst);
    free(b->ht);
    free(b);
}

val umbra_x(builder *b) { return push(b, op_x); }
val umbra_y(builder *b) { return push(b, op_y); }

val umbra_imm_i32(builder *b, int bits) { return push(b, op_imm_32, .imm = bits); }
val umbra_imm_f32(builder *b, float v) {
    union {
        float f;
        int   i;
    } u = {.f = v};
    return umbra_imm_i32(b, u.i);
}

int umbra_reserve(builder *b, int n) {
    b->uni_len = (b->uni_len + 3) & ~3;
    int ix = b->uni_len / 4;
    b->uni_len += n * 4;
    return ix;
}
int umbra_reserve_ptr(builder *b) {
    b->uni_len = (b->uni_len + 7) & ~7;
    int off = b->uni_len;
    b->uni_len += 16;
    return off;
}
umbra_ptr umbra_deref_ptr(builder *b, umbra_ptr buf, int byte_off) {
    val v = push(b, op_deref_ptr, .ptr = buf.ix, .imm = byte_off);
    return (umbra_ptr){~v.id};
}
int  umbra_uni_len(builder const *b) { return b->uni_len; }
void umbra_set_uni_len(builder *b, int len) { b->uni_len = len; }

int umbra_max_ptr(builder const *b) {
    int m = 0;
    for (int i = 0; i < b->insts; i++) {
        if (has_ptr(b->inst[i].op) && b->inst[i].ptr > m) { m = b->inst[i].ptr; }
    }
    return m;
}

// Recognize add(mul(op_y, uniform), op_x) as contiguous.
// Within a SIMD vector, y is constant and x advances by 1, so memory is contiguous.
static _Bool is_x_plus_y_stride(builder *b, int ix) {
    if (b->inst[ix].op != op_add_i32) { return 0; }
    int p = b->inst[ix].x, q = b->inst[ix].y;
    int mul = -1;
    if (b->inst[p].op == op_x) { mul = q; }
    if (b->inst[q].op == op_x) { mul = p; }
    if (mul < 0 || b->inst[mul].op != op_mul_i32) { return 0; }
    int a = b->inst[mul].x, c = b->inst[mul].y;
    return (b->inst[a].op == op_y && b->inst[c].uniform)
        || (b->inst[c].op == op_y && b->inst[a].uniform);
}

static _Bool is_contiguous(builder *b, int ix) {
    enum op op = b->inst[ix].op;
    return op == op_iota || op == op_x || is_x_plus_y_stride(b, ix);
}

// Recognize contiguous+offset as contiguous with offset.
static int contiguous_plus_off(builder *b, int ix) {
    if (b->inst[ix].op != op_add_i32) { return -1; }
    int p = b->inst[ix].x, q = b->inst[ix].y;
    if (is_contiguous(b, p)) { return q; }
    if (is_contiguous(b, q)) { return p; }
    return -1;
}

val umbra_load_i16(builder *b, umbra_ptr src, val ix) {
    if (is_contiguous(b, ix.id)) {
        return push(b, op_load_16, .ptr = src.ix);
    }
    if (b->inst[ix.id].op == op_imm_32) {
        return push(b, op_uni_16, .imm = b->inst[ix.id].imm, .ptr = src.ix);
    }
    {
        int off = contiguous_plus_off(b, ix.id);
        if (off >= 0) { return push(b, op_load_16, .x = off, .ptr = src.ix); }
    }
    if (b->inst[ix.id].uniform) { return push(b, op_uni_16, .x = ix.id, .ptr = src.ix); }
    return push(b, op_gather_16, .x = ix.id, .ptr = src.ix);
}
val umbra_load_next_i32(builder *b, umbra_ptr src) {
    return push(b, op_load_next_32, .ptr = src.ix);
}
val umbra_load_next_i64_lo(builder *b, umbra_ptr src) {
    return push(b, op_load_next_64_lo, .ptr = src.ix);
}
val umbra_load_next_i64_hi(builder *b, umbra_ptr src) {
    return push(b, op_load_next_64_hi, .ptr = src.ix);
}
val umbra_load_next_i16(builder *b, umbra_ptr src) {
    return push(b, op_load_next_16, .ptr = src.ix);
}
val umbra_load_i32(builder *b, umbra_ptr src, val ix) {
    if (is_contiguous(b, ix.id)) {
        return push(b, op_load_32, .ptr = src.ix);
    }
    if (b->inst[ix.id].op == op_imm_32) {
        return push(b, op_uni_32, .imm = b->inst[ix.id].imm, .ptr = src.ix);
    }
    {
        int off = contiguous_plus_off(b, ix.id);
        if (off >= 0) { return push(b, op_load_32, .x = off, .ptr = src.ix); }
    }
    if (b->inst[ix.id].uniform) { return push(b, op_uni_32, .x = ix.id, .ptr = src.ix); }
    return push(b, op_gather_32, .x = ix.id, .ptr = src.ix);
}
void umbra_store_i16(builder *b, umbra_ptr dst, val ix, val v) {
    if (is_contiguous(b, ix.id)) {
        push(b, op_store_16, .y = v.id, .ptr = dst.ix);
        return;
    }
    {
        int off = contiguous_plus_off(b, ix.id);
        if (off >= 0) {
            push(b, op_store_16, .x = off, .y = v.id, .ptr = dst.ix);
            return;
        }
    }
    push(b, op_scatter_16, .x = ix.id, .y = v.id, .ptr = dst.ix);
}
void umbra_store_next_i32(builder *b, umbra_ptr dst, val v) {
    push(b, op_store_next_32, .y = v.id, .ptr = dst.ix);
}
void umbra_store_next_i64(builder *b, umbra_ptr dst, val lo, val hi) {
    push(b, op_store_next_64, .x = lo.id, .y = hi.id, .ptr = dst.ix);
}
void umbra_store_next_i16(builder *b, umbra_ptr dst, val v) {
    push(b, op_store_next_16, .y = v.id, .ptr = dst.ix);
}
void umbra_store_i32(builder *b, umbra_ptr dst, val ix, val v) {
    if (is_contiguous(b, ix.id)) {
        push(b, op_store_32, .y = v.id, .ptr = dst.ix);
        return;
    }
    {
        int off = contiguous_plus_off(b, ix.id);
        if (off >= 0) {
            push(b, op_store_32, .x = off, .y = v.id, .ptr = dst.ix);
            return;
        }
    }
    push(b, op_scatter_32, .x = ix.id, .y = v.id, .ptr = dst.ix);
}
val umbra_widen_s16(builder *b, val x) { return push(b, op_widen_s16, .x = x.id); }
val umbra_widen_u16(builder *b, val x) { return push(b, op_widen_u16, .x = x.id); }
val umbra_narrow_i16(builder *b, val x) { return push(b, op_narrow_16, .x = x.id); }

val umbra_widen_f16(builder *b, val x) { return push(b, op_widen_f16, .x = x.id); }
val umbra_narrow_f32(builder *b, val x) { return push(b, op_narrow_f32, .x = x.id); }

static _Bool is_imm32(builder *b, int id, int v) {
    return b->inst[id].op == op_imm_32 && b->inst[id].imm == v;
}

static _Bool is_imm(builder *b, int id) { return b->inst[id].op == op_imm_32; }

static val math_(builder *b, struct bb_inst inst) {
    if (is_imm(b, inst.x) && is_imm(b, inst.y) && is_imm(b, inst.z)) {
        int const result = umbra_const_eval(inst.op, b->inst[inst.x].imm,
                                            b->inst[inst.y].imm, b->inst[inst.z].imm);
        return umbra_imm_i32(b, result);
    }
    return push_(b, inst);
}
#define math(b, ...) math_(b, (struct bb_inst){.op = __VA_ARGS__})

static val try_imm(builder *b, val d, enum op fused, int x, int y) {
    int imm_id = is_imm(b, x) ? x : is_imm(b, y) ? y : -1;
    if (imm_id >= 0) {
        int other = imm_id == x ? y : x;
        val f = push(b, fused, .x = other, .imm = b->inst[imm_id].imm);
        return push(b, op_join, .x = d.id, .y = f.id);
    }
    return (val){d.id};
}

static void sort(int *a, int *b) {
    if (*a > *b) {
        int const t = *a;
        *a = *b;
        *b = t;
    }
}

val umbra_add_f32(builder *b, val x, val y) {
    sort(&x.id, &y.id);
    if (is_imm32(b, x.id, 0)) { return y; }
    if (b->inst[x.id].op == op_mul_f32) {
        return math(b, op_fma_f32, .x = b->inst[x.id].x, .y = b->inst[x.id].y, .z = y.id);
    }
    if (b->inst[y.id].op == op_mul_f32) {
        return math(b, op_fma_f32, .x = b->inst[y.id].x, .y = b->inst[y.id].y, .z = x.id);
    }
    return try_imm(b, math(b, op_add_f32, .x = x.id, .y = y.id), op_add_f32_imm, x.id,
                   y.id);
}

val umbra_sub_f32(builder *b, val x, val y) {
    if (is_imm32(b, y.id, 0)) { return x; }
    if (is_imm32(b, x.id, 0)) { return umbra_neg_f32(b, y); }
    if (b->inst[y.id].op == op_mul_f32) {
        return math(b, op_fms_f32, .x = b->inst[y.id].x, .y = b->inst[y.id].y, .z = x.id);
    }
    val d = math(b, op_sub_f32, .x = x.id, .y = y.id);
    if (is_imm(b, y.id)) {
        val f = push(b, op_sub_f32_imm, .x = x.id, .imm = b->inst[y.id].imm);
        return push(b, op_join, .x = d.id, .y = f.id);
    }
    return (val){d.id};
}

val umbra_mul_f32(builder *b, val x, val y) {
    sort(&x.id, &y.id);
    if (is_imm32(b, x.id, 0x3f800000)) { return y; }
    if (is_imm32(b, y.id, 0x3f800000)) { return x; }
    return try_imm(b, math(b, op_mul_f32, .x = x.id, .y = y.id), op_mul_f32_imm, x.id,
                   y.id);
}

val umbra_div_f32(builder *b, val x, val y) {
    if (is_imm32(b, y.id, 0x3f800000)) { return x; }
    val d = math(b, op_div_f32, .x = x.id, .y = y.id);
    if (is_imm(b, y.id)) {
        val f = push(b, op_div_f32_imm, .x = x.id, .imm = b->inst[y.id].imm);
        return push(b, op_join, .x = d.id, .y = f.id);
    }
    return (val){d.id};
}

val umbra_min_f32(builder *b, val x, val y) {
    sort(&x.id, &y.id);
    return try_imm(b, math(b, op_min_f32, .x = x.id, .y = y.id), op_min_f32_imm, x.id,
                   y.id);
}

val umbra_max_f32(builder *b, val x, val y) {
    if (b->inst[x.id].op == op_neg_f32 && b->inst[x.id].x == y.id) {
        return umbra_abs_f32(b, y);
    }
    if (b->inst[y.id].op == op_neg_f32 && b->inst[y.id].x == x.id) {
        return umbra_abs_f32(b, x);
    }
    sort(&x.id, &y.id);
    return try_imm(b, math(b, op_max_f32, .x = x.id, .y = y.id), op_max_f32_imm, x.id,
                   y.id);
}

val umbra_sqrt_f32(builder *b, val x) { return math(b, op_sqrt_f32, .x = x.id); }
val umbra_abs_f32(builder *b, val x) { return math(b, op_abs_f32, .x = x.id); }
val umbra_neg_f32(builder *b, val x) { return math(b, op_neg_f32, .x = x.id); }
val umbra_round_f32(builder *b, val x) { return math(b, op_round_f32, .x = x.id); }
val umbra_floor_f32(builder *b, val x) { return math(b, op_floor_f32, .x = x.id); }
val umbra_ceil_f32(builder *b, val x) { return math(b, op_ceil_f32, .x = x.id); }
val umbra_round_i32(builder *b, val x) { return math(b, op_round_i32, .x = x.id); }
val umbra_floor_i32(builder *b, val x) { return math(b, op_floor_i32, .x = x.id); }
val umbra_ceil_i32(builder *b, val x) { return math(b, op_ceil_i32, .x = x.id); }
val umbra_sign_f32(builder *b, val x) {
    val z = umbra_imm_f32(b, 0.0f);
    return umbra_or_i32(b,
                        umbra_and_i32(b, umbra_gt_f32(b, x, z),
                                      umbra_imm_i32(b, 0x3f800000)),
                        umbra_and_i32(b, umbra_lt_f32(b, x, z),
                                      umbra_imm_i32(b, (int)0xbf800000)));
}

val umbra_add_i32(builder *b, val x, val y) {
    sort(&x.id, &y.id);
    if (is_imm32(b, x.id, 0)) { return y; }
    return try_imm(b, math(b, op_add_i32, .x = x.id, .y = y.id), op_add_i32_imm, x.id,
                   y.id);
}

val umbra_sub_i32(builder *b, val x, val y) {
    if (is_imm32(b, y.id, 0)) { return x; }
    if (x.id == y.id) { return umbra_imm_i32(b, 0); }
    val d = math(b, op_sub_i32, .x = x.id, .y = y.id);
    if (is_imm(b, y.id)) {
        val f = push(b, op_sub_i32_imm, .x = x.id, .imm = b->inst[y.id].imm);
        return push(b, op_join, .x = d.id, .y = f.id);
    }
    return (val){d.id};
}

val umbra_mul_i32(builder *b, val x, val y) {
    sort(&x.id, &y.id);
    if (is_imm32(b, x.id, 1)) { return y; }
    if (is_imm32(b, y.id, 1)) { return x; }
    if (is_imm32(b, x.id, 0)) { return x; }
    if (b->inst[x.id].op == op_imm_32 && is_pow2(b->inst[x.id].imm)) {
        int const shift = __builtin_ctz((unsigned)b->inst[x.id].imm);
        return umbra_shl_i32(b, y, umbra_imm_i32(b, shift));
    }
    if (b->inst[y.id].op == op_imm_32 && is_pow2(b->inst[y.id].imm)) {
        int const shift = __builtin_ctz((unsigned)b->inst[y.id].imm);
        return umbra_shl_i32(b, x, umbra_imm_i32(b, shift));
    }
    return try_imm(b, math(b, op_mul_i32, .x = x.id, .y = y.id), op_mul_i32_imm, x.id,
                   y.id);
}

val umbra_shl_i32(builder *b, val x, val y) {
    if (is_imm32(b, y.id, 0)) { return x; }
    if (is_imm(b, y.id)) {
        return push(b, op_shl_imm, .x = x.id, .imm = b->inst[y.id].imm);
    }
    return math(b, op_shl_i32, .x = x.id, .y = y.id);
}
val umbra_shr_u32(builder *b, val x, val y) {
    if (is_imm32(b, y.id, 0)) { return x; }
    if (is_imm(b, y.id)) {
        return push(b, op_shr_u32_imm, .x = x.id, .imm = b->inst[y.id].imm);
    }
    return math(b, op_shr_u32, .x = x.id, .y = y.id);
}
val umbra_shr_s32(builder *b, val x, val y) {
    if (is_imm32(b, y.id, 0)) { return x; }
    if (is_imm(b, y.id)) {
        return push(b, op_shr_s32_imm, .x = x.id, .imm = b->inst[y.id].imm);
    }
    return math(b, op_shr_s32, .x = x.id, .y = y.id);
}

val umbra_and_i32(builder *b, val x, val y) {
    sort(&x.id, &y.id);
    if (x.id == y.id) { return x; }
    if (is_imm32(b, x.id, -1)) { return y; }
    if (is_imm32(b, y.id, -1)) { return x; }
    if (is_imm32(b, x.id, 0)) { return x; }
    if (is_imm32(b, x.id, 0x7fffffff)) { return umbra_abs_f32(b, y); }
    if (is_imm32(b, y.id, 0x7fffffff)) { return umbra_abs_f32(b, x); }
    val d = math(b, op_and_32, .x = x.id, .y = y.id);
    if (is_imm(b, x.id)) {
        val f = push(b, op_and_imm, .x = y.id, .imm = b->inst[x.id].imm);
        return push(b, op_join, .x = d.id, .y = f.id);
    }
    if (is_imm(b, y.id)) {
        val f = push(b, op_and_imm, .x = x.id, .imm = b->inst[y.id].imm);
        return push(b, op_join, .x = d.id, .y = f.id);
    }
    return (val){d.id};
}
val umbra_or_i32(builder *b, val x, val y) {
    sort(&x.id, &y.id);
    if (x.id == y.id) { return x; }
    if (is_imm32(b, x.id, 0)) { return y; }
    if (is_imm32(b, x.id, -1)) { return x; }
    if (is_imm32(b, y.id, -1)) { return y; }
    return try_imm(b, math(b, op_or_32, .x = x.id, .y = y.id), op_or_32_imm, x.id, y.id);
}
val umbra_pack(builder *b, val base, val v, int shift) {
    val shifted = umbra_shl_i32(b, v, umbra_imm_i32(b, shift));
    val d = umbra_or_i32(b, base, shifted);
    val f = push(b, op_pack, .x = base.id, .y = v.id, .imm = shift);
    return push(b, op_join, .x = d.id, .y = f.id);
}
val umbra_xor_i32(builder *b, val x, val y) {
    sort(&x.id, &y.id);
    if (x.id == y.id) { return umbra_imm_i32(b, 0); }
    if (is_imm32(b, x.id, 0)) { return y; }
    return try_imm(b, math(b, op_xor_32, .x = x.id, .y = y.id), op_xor_32_imm, x.id, y.id);
}
val umbra_sel_i32(builder *b, val c, val t, val fv) {
    if (t.id == fv.id) { return t; }
    if (is_imm32(b, c.id, -1)) { return t; }
    if (is_imm32(b, c.id, 0)) { return fv; }
    return math(b, op_sel_32, .x = c.id, .y = t.id, .z = fv.id);
}

val umbra_cvt_f32_i32(builder *b, val x) { return math(b, op_f32_from_i32, .x = x.id); }
val umbra_cvt_i32_f32(builder *b, val x) { return math(b, op_i32_from_f32, .x = x.id); }

val umbra_eq_f32(builder *b, val x, val y) {
    sort(&x.id, &y.id);
    return try_imm(b, math(b, op_eq_f32, .x = x.id, .y = y.id), op_eq_f32_imm, x.id, y.id);
}
val umbra_ne_f32(builder *b, val x, val y) {
    return umbra_xor_i32(b, umbra_eq_f32(b, x, y), umbra_imm_i32(b, -1));
}
val umbra_lt_f32(builder *b, val x, val y) {
    val d = math(b, op_lt_f32, .x = x.id, .y = y.id);
    if (is_imm(b, y.id)) {
        val f = push(b, op_lt_f32_imm, .x = x.id, .imm = b->inst[y.id].imm);
        return push(b, op_join, .x = d.id, .y = f.id);
    }
    return (val){d.id};
}
val umbra_le_f32(builder *b, val x, val y) {
    val d = math(b, op_le_f32, .x = x.id, .y = y.id);
    if (is_imm(b, y.id)) {
        val f = push(b, op_le_f32_imm, .x = x.id, .imm = b->inst[y.id].imm);
        return push(b, op_join, .x = d.id, .y = f.id);
    }
    return (val){d.id};
}
val umbra_gt_f32(builder *b, val x, val y) { return umbra_lt_f32(b, y, x); }
val umbra_ge_f32(builder *b, val x, val y) { return umbra_le_f32(b, y, x); }

val umbra_eq_i32(builder *b, val x, val y) {
    sort(&x.id, &y.id);
    if (x.id == y.id) { return umbra_imm_i32(b, -1); }
    return try_imm(b, math(b, op_eq_i32, .x = x.id, .y = y.id), op_eq_i32_imm, x.id, y.id);
}
val umbra_ne_i32(builder *b, val x, val y) {
    return umbra_xor_i32(b, umbra_eq_i32(b, x, y), umbra_imm_i32(b, -1));
}

val umbra_lt_s32(builder *b, val x, val y) {
    val d = math(b, op_lt_s32, .x = x.id, .y = y.id);
    if (is_imm(b, y.id)) {
        val f = push(b, op_lt_s32_imm, .x = x.id, .imm = b->inst[y.id].imm);
        return push(b, op_join, .x = d.id, .y = f.id);
    }
    return (val){d.id};
}
val umbra_le_s32(builder *b, val x, val y) {
    val d = math(b, op_le_s32, .x = x.id, .y = y.id);
    if (is_imm(b, y.id)) {
        val f = push(b, op_le_s32_imm, .x = x.id, .imm = b->inst[y.id].imm);
        return push(b, op_join, .x = d.id, .y = f.id);
    }
    return (val){d.id};
}
val umbra_gt_s32(builder *b, val x, val y) { return umbra_lt_s32(b, y, x); }
val umbra_ge_s32(builder *b, val x, val y) { return umbra_le_s32(b, y, x); }

val umbra_lt_u32(builder *b, val x, val y) {
    return math(b, op_lt_u32, .x = x.id, .y = y.id);
}
val umbra_le_u32(builder *b, val x, val y) {
    return math(b, op_le_u32, .x = x.id, .y = y.id);
}
val umbra_gt_u32(builder *b, val x, val y) { return umbra_lt_u32(b, y, x); }
val umbra_ge_u32(builder *b, val x, val y) { return umbra_le_u32(b, y, x); }

static char const *op_name(enum op op) {
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
        int deps[] = {in[i].x, in[i].y, in[i].z};
        for (int k = 0; k < 3; k++) { last_use[deps[k]] = i; }
    }
    for (int i = 0; i < n; i++) {
        if (!body[i]) { continue; }
        int deps[] = {in[i].x, in[i].y, in[i].z};
        for (int k = 0; k < 3; k++) {
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
        int deps[] = {in[i].x, in[i].y, in[i].z};
        for (int k = 0; k < 3; k++) {
            if (body[deps[k]]) { users[user_off[deps[k]] + n_users[deps[k]]++] = i; }
        }
    }

    int *ready = calloc((size_t)n, sizeof *ready);
    int  nready = 0;
    for (int i = 0; i < n; i++) {
        if (body[i] && n_deps[i] == 0) { ready[nready++] = i; }
    }

    int j = preamble;
    while (nready > 0) {
        int best = 0, best_score = -9999;
        for (int r = 0; r < nready; r++) {
            int id = ready[r];
            int kills = 0;
            int deps[] = {in[id].x, in[id].y, in[id].z};
            for (int k = 0; k < 3; k++) {
                if (last_use[deps[k]] == id) { kills++; }
            }
            int defines = is_store(in[id].op) ? 0 : 1;
            int net = kills - defines;
            int lu = last_use[id] < 0 ? total : last_use[id];
            int score = net * total - lu;
            if (score > best_score) {
                best_score = score;
                best = r;
            }
        }
        int id = ready[best];
        ready[best] = ready[--nready];

        old_to_new[id] = j;
        out[j++] = in[id];

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

struct umbra_basic_block *umbra_basic_block(builder *b) {
    int const n = b->insts;

    _Bool *live = calloc((size_t)n, 1);
    _Bool *varying = calloc((size_t)n, 1);

    for (int i = n; i-- > 0;) {
        if (is_store(b->inst[i].op)) { live[i] = 1; }
        if (live[i]) {
            live[b->inst[i].x] = 1;
            live[b->inst[i].y] = 1;
            live[b->inst[i].z] = 1;
            if (b->inst[i].ptr < 0) { live[~b->inst[i].ptr] = 1; }
        }
    }
    for (int i = 0; i < n; i++) {
        varying[i] = is_varying(b->inst[i].op)
            | varying[b->inst[i].x]
            | varying[b->inst[i].y]
            | varying[b->inst[i].z];
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
        out[i].x = old_to_new[out[i].x];
        out[i].y = old_to_new[out[i].y];
        out[i].z = old_to_new[out[i].z];
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
    if (!bb) { return; }
    free(bb->inst);
    free(bb);
}

struct umbra_basic_block *umbra_resolve_joins(struct umbra_basic_block const *bb,
                                              join_chooser                    choose_y) {
    int const n = bb->insts;
    int      *remap = malloc((size_t)n * sizeof *remap);
    for (int i = 0; i < n; i++) { remap[i] = i; }

    for (int i = 0; i < n; i++) {
        if (bb->inst[i].op == op_join) {
            int pick = (choose_y && choose_y(bb->inst, i)) ? bb->inst[i].y : bb->inst[i].x;
            remap[i] = pick;
        }
    }
    for (int i = 0; i < n; i++) {
        if (remap[i] != remap[remap[i]]) { __builtin_trap(); }
    }

    _Bool *live = calloc((size_t)n, 1);
    for (int i = n; i-- > 0;) {
        if (is_store(bb->inst[i].op)) { live[i] = 1; }
        if (live[i] && remap[i] != i) {
            live[remap[i]] = 1;
            continue;
        }
        if (live[i]) {
            live[bb->inst[i].x] = 1;
            live[bb->inst[i].y] = 1;
            live[bb->inst[i].z] = 1;
            if (bb->inst[i].ptr < 0) { live[~bb->inst[i].ptr] = 1; }
        }
    }

    int total = 0;
    for (int i = 0; i < n; i++) {
        if (live[i] && remap[i] == i) { total++; }
    }

    struct bb_inst *out = malloc((size_t)total * sizeof *out);
    int            *old_to_new = malloc((size_t)n * sizeof *old_to_new);
    for (int i = 0; i < n; i++) { old_to_new[i] = -1; }

    int j = 0;
    for (int i = 0; i < bb->preamble; i++) {
        if (live[i] && remap[i] == i && !is_store(bb->inst[i].op)) {
            old_to_new[i] = j;
            out[j++] = bb->inst[i];
        }
    }
    int preamble = j;

    for (int i = bb->preamble; i < n; i++) {
        if (live[i] && remap[i] == i) {
            old_to_new[i] = j;
            out[j++] = bb->inst[i];
        }
    }

    for (int i = 0; i < n; i++) {
        if (remap[i] != i) { old_to_new[i] = old_to_new[remap[i]]; }
    }

    for (int i = 0; i < total; i++) {
        out[i].x = old_to_new[out[i].x];
        out[i].y = old_to_new[out[i].y];
        out[i].z = old_to_new[out[i].z];
        if (out[i].ptr < 0) { out[i].ptr = ~old_to_new[~out[i].ptr]; }
    }

    free(remap);
    free(live);
    free(old_to_new);

    struct umbra_basic_block *result = malloc(sizeof *result);
    result->inst = out;
    result->insts = total;
    result->preamble = preamble;
    result->uni_len = bb->uni_len;
    return result;
}

static void dump_insts(struct bb_inst const *inst, int insts, FILE *f) {
    for (int i = 0; i < insts; i++) {
        struct bb_inst const *ip = &inst[i];
        enum op               op = ip->op;

        if (is_store(op)) {
            {
                fprintf(f, "      %-15s p%d", op_name(op), ip->ptr);
                if (op == op_scatter_16 || op == op_scatter_32) {
                    fprintf(f, " v%d", ip->x);
                } else if (op == op_store_next_64) {
                    fprintf(f, " v%d v%d", ip->x, ip->y);
                } else if (ip->x && (op == op_store_16 || op == op_store_32)) {
                    fprintf(f, " +v%d", ip->x);
                }
                if (op != op_store_next_64) { fprintf(f, " v%d", ip->y); }
                fprintf(f, "\n");
            }
            continue;
        }

        fprintf(f, "  v%-3d = %-15s", i, op_name(op));

        switch (op) {
        case op_imm_32: fprintf(f, " 0x%x", (uint32_t)ip->imm); break;
        case op_uni_32:
        case op_uni_16:
            if (ip->x) {
                fprintf(f, " p%d[v%d]", ip->ptr, ip->x);
            } else {
                fprintf(f, " p%d[%d]", ip->ptr, ip->imm);
            }
            break;
        case op_gather_32:
        case op_gather_16: fprintf(f, " p%d v%d", ip->ptr, ip->x); break;
        case op_load_next_16:
        case op_load_next_32:
        case op_load_next_64_lo:
        case op_load_next_64_hi: fprintf(f, " p%d", ip->ptr); break;
        case op_load_32:
        case op_load_16:
            fprintf(f, " p%d", ip->ptr);
            if (ip->x) { fprintf(f, " +v%d", ip->x); }
            break;
        case op_deref_ptr: fprintf(f, " p%d byte%d", ip->ptr, ip->imm); break;
        case op_iota:
        case op_x:
        case op_y: break;

        case op_store_16:
        case op_store_next_16:
        case op_store_32:
        case op_store_next_32:
        case op_store_next_64:
        case op_scatter_16:
        case op_scatter_32: break;

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
        case op_widen_s16:
        case op_widen_u16:
        case op_narrow_16:
        case op_widen_f16:
        case op_narrow_f32: fprintf(f, " v%d", ip->x); break;

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
        case op_le_u32: fprintf(f, " v%d v%d", ip->x, ip->y); break;

        case op_fma_f32:
        case op_fms_f32:
        case op_sel_32: fprintf(f, " v%d v%d v%d", ip->x, ip->y, ip->z); break;

        case op_join: fprintf(f, " v%d v%d", ip->x, ip->y); break;

        case op_shl_imm:
        case op_shr_u32_imm:
        case op_shr_s32_imm: fprintf(f, " v%d %d", ip->x, ip->imm); break;
        case op_and_imm: fprintf(f, " v%d 0x%x", ip->x, (uint32_t)ip->imm); break;
        case op_pack: fprintf(f, " v%d v%d %d", ip->x, ip->y, ip->imm); break;
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
        case op_le_s32_imm: fprintf(f, " v%d 0x%x", ip->x, (uint32_t)ip->imm); break;
        }
        fprintf(f, "\n");
    }
}

void umbra_dump_builder(builder const *b, FILE *f) { dump_insts(b->inst, b->insts, f); }

void umbra_dump_basic_block(struct umbra_basic_block const *bb, FILE *f) {
    dump_insts(bb->inst, bb->insts, f);
}
