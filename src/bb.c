#include "../umbra.h"
#include "bb.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int f32_bits(float f) { int i; memcpy(&i, &f, 4); return i; }
static float f32_from_bits(int i) { float f; memcpy(&f, &i, 4); return f; }

static float scalar_f16_to_f32(uint16_t h) {
    uint32_t sign = ((uint32_t)h >> 15) << 31;
    uint32_t exp  = ((uint32_t)h >> 10) & 0x1f;
    uint32_t mant = (uint32_t)h & 0x3ff;
    uint32_t bits;
    if (exp == 0) {
        bits = sign;
    } else if (exp == 31) {
        bits = sign | (0xffu << 23) | (mant << 13);
    } else {
        bits = sign | ((exp + 112) << 23) | (mant << 13);
    }
    float f; memcpy(&f, &bits, 4);
    return f;
}

static uint16_t scalar_f32_to_f16(float f) {
    uint32_t bits;
    memcpy(&bits, &f, 4);
    uint32_t sign = (bits >> 31) << 15;
    int32_t  exp  = (int32_t)((bits >> 23) & 0xff) - 127 + 15;
    uint32_t mant = (bits >> 13) & 0x3ff;
    uint32_t round_bit = (bits >> 12) & 1;
    uint32_t sticky    = (bits & 0xfff) != 0 ? 1 : 0;
    mant += round_bit & (sticky | (mant & 1));
    if (mant >> 10) { exp++; mant &= 0x3ff; }
    uint32_t src_exp = (bits >> 23) & 0xff;
    if (src_exp == 0xff) { return (uint16_t)(sign | 0x7c00 | mant); }
    if (exp >= 31)       { return (uint16_t)(sign | 0x7c00); }
    if (exp <= 0)        { return (uint16_t)sign; }
    return (uint16_t)(sign | ((uint32_t)exp << 10) | mant);
}

// Scalar constprop: evaluate one op on scalar immediates.
// Returns result as int bits (32-bit for 32-bit ops, 16-bit value in low bits for 16-bit ops).
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
static int const_eval(enum op op, int xb, int yb, int zb) {
    float xf = f32_from_bits(xb), yf = f32_from_bits(yb), zf = f32_from_bits(zb);
    uint32_t xu = (uint32_t)xb, yu = (uint32_t)yb;
    uint16_t xh = (uint16_t)xb, yh = (uint16_t)yb, zh = (uint16_t)zb;
    switch (op) {
        // f32 arithmetic
        case op_add_f32:  return f32_bits(xf + yf);
        case op_sub_f32:  return f32_bits(xf - yf);
        case op_mul_f32:  return f32_bits(xf * yf);
        case op_div_f32:  return f32_bits(xf / yf);
        case op_min_f32:  return f32_bits(fminf(xf, yf));
        case op_max_f32:  return f32_bits(fmaxf(xf, yf));
        case op_sqrt_f32: return f32_bits(sqrtf(xf));
        case op_fma_f32:  return f32_bits(xf * yf + zf);

        // f16 arithmetic (promote, operate, demote)
        case op_add_f16:  return scalar_f32_to_f16(scalar_f16_to_f32(xh) + scalar_f16_to_f32(yh));
        case op_sub_f16:  return scalar_f32_to_f16(scalar_f16_to_f32(xh) - scalar_f16_to_f32(yh));
        case op_mul_f16:  return scalar_f32_to_f16(scalar_f16_to_f32(xh) * scalar_f16_to_f32(yh));
        case op_div_f16:  return scalar_f32_to_f16(scalar_f16_to_f32(xh) / scalar_f16_to_f32(yh));
        case op_min_f16:  return scalar_f32_to_f16(fminf(scalar_f16_to_f32(xh), scalar_f16_to_f32(yh)));
        case op_max_f16:  return scalar_f32_to_f16(fmaxf(scalar_f16_to_f32(xh), scalar_f16_to_f32(yh)));
        case op_sqrt_f16: return scalar_f32_to_f16(sqrtf(scalar_f16_to_f32(xh)));
        case op_fma_f16:  return scalar_f32_to_f16(scalar_f16_to_f32(xh) * scalar_f16_to_f32(yh)
                                                  + scalar_f16_to_f32(zh));

        // i32 arithmetic
        case op_add_i32: return xb + yb;
        case op_sub_i32: return xb - yb;
        case op_mul_i32: return xb * yb;
        case op_shl_i32: return (int)(xu << yu);
        case op_shr_u32: return (int)(xu >> yu);
        case op_shr_s32: return xb >> yb;
        case op_and_32:  return xb & yb;
        case op_or_32:   return xb | yb;
        case op_xor_32:  return xb ^ yb;
        case op_sel_32:  return (xb & yb) | (~xb & zb);

        // i16 arithmetic
        case op_add_i16: return (uint16_t)(xh + yh);
        case op_sub_i16: return (uint16_t)(xh - yh);
        case op_mul_i16: return (uint16_t)(xh * yh);
        case op_shl_i16: return (uint16_t)(xh << yh);
        case op_shr_u16: return (uint16_t)(xh >> yh);
        case op_shr_s16: return (uint16_t)((int16_t)xh >> yh);
        case op_and_16:  return (uint16_t)(xh & yh);
        case op_or_16:   return (uint16_t)(xh | yh);
        case op_xor_16:  return (uint16_t)(xh ^ yh);
        case op_sel_16:  return (uint16_t)((xh & yh) | (~xh & zh));

        // conversions
        case op_f16_from_f32: return scalar_f32_to_f16(xf);
        case op_f32_from_f16: return f32_bits(scalar_f16_to_f32(xh));
        case op_f32_from_i32: return f32_bits((float)xb);
        case op_i32_from_f32: return (int)xf;

        // f32 comparisons
        case op_eq_f32: return -(int)(xf == yf);
        case op_ne_f32: return -(int)(xf != yf);
        case op_lt_f32: return -(int)(xf <  yf);
        case op_le_f32: return -(int)(xf <= yf);
        case op_gt_f32: return -(int)(xf >  yf);
        case op_ge_f32: return -(int)(xf >= yf);

        // f16 comparisons
        case op_eq_f16: return (uint16_t)-(int)(scalar_f16_to_f32(xh) == scalar_f16_to_f32(yh));
        case op_ne_f16: return (uint16_t)-(int)(scalar_f16_to_f32(xh) != scalar_f16_to_f32(yh));
        case op_lt_f16: return (uint16_t)-(int)(scalar_f16_to_f32(xh) <  scalar_f16_to_f32(yh));
        case op_le_f16: return (uint16_t)-(int)(scalar_f16_to_f32(xh) <= scalar_f16_to_f32(yh));
        case op_gt_f16: return (uint16_t)-(int)(scalar_f16_to_f32(xh) >  scalar_f16_to_f32(yh));
        case op_ge_f16: return (uint16_t)-(int)(scalar_f16_to_f32(xh) >= scalar_f16_to_f32(yh));

        // i32 comparisons
        case op_eq_i32: return -(int)(xb == yb);
        case op_ne_i32: return -(int)(xb != yb);
        case op_lt_s32: return -(int)(xb <  yb);
        case op_le_s32: return -(int)(xb <= yb);
        case op_gt_s32: return -(int)(xb >  yb);
        case op_ge_s32: return -(int)(xb >= yb);
        case op_lt_u32: return -(int)(xu <  yu);
        case op_le_u32: return -(int)(xu <= yu);
        case op_gt_u32: return -(int)(xu >  yu);
        case op_ge_u32: return -(int)(xu >= yu);

        // i16 comparisons
        case op_eq_i16: return (uint16_t)-(int)((int16_t)xh == (int16_t)yh);
        case op_ne_i16: return (uint16_t)-(int)((int16_t)xh != (int16_t)yh);
        case op_lt_s16: return (uint16_t)-(int)((int16_t)xh <  (int16_t)yh);
        case op_le_s16: return (uint16_t)-(int)((int16_t)xh <= (int16_t)yh);
        case op_gt_s16: return (uint16_t)-(int)((int16_t)xh >  (int16_t)yh);
        case op_ge_s16: return (uint16_t)-(int)((int16_t)xh >= (int16_t)yh);
        case op_lt_u16: return (uint16_t)-(int)(xh <  yh);
        case op_le_u16: return (uint16_t)-(int)(xh <= yh);
        case op_gt_u16: return (uint16_t)-(int)(xh >  yh);
        case op_ge_u16: return (uint16_t)-(int)(xh >= yh);

        default: return 0;
    }
}
#pragma clang diagnostic pop

// murmur3-32, specialized for 20 bytes.
static uint32_t bb_inst_hash(struct bb_inst const *inst) {
    uint32_t const w[] = {
        (uint32_t)inst->op, (uint32_t)inst->x, (uint32_t)inst->y,
        (uint32_t)inst->z, (uint32_t)inst->imm,
    };
    uint32_t h = 0;
    for (int i = 0; i < 5; i++) {
        uint32_t k = w[i];
        __builtin_mul_overflow(k, 0xcc9e2d51u, &k);
        k = __builtin_rotateleft32(k, 15);
        __builtin_mul_overflow(k, 0x1b873593u, &k);
        h ^= k;
        h = __builtin_rotateleft32(h, 13);
        __builtin_mul_overflow(h, 5, &h);
        __builtin_add_overflow(h, 0xe6546b64u, &h);
    }
    h ^= 20;
    h ^= h >> 16; __builtin_mul_overflow(h, 0x85ebca6bu, &h);
    h ^= h >> 13; __builtin_mul_overflow(h, 0xc2b2ae35u, &h);
    h ^= h >> 16;
    return h ? h : 1;
}

static int push_(struct umbra_basic_block *bb, struct bb_inst inst) {
    uint32_t const h = bb_inst_hash(&inst);

    for (int slot = (int)h; bb->ht_mask; slot++) {
        if (bb->ht[slot & bb->ht_mask].hash == 0) {
            break;
        }
        if (h == bb->ht[slot & bb->ht_mask].hash &&
            0 == __builtin_memcmp(&inst,
                                  &bb->inst[bb->ht[slot & bb->ht_mask].ix],
                                  sizeof inst)) {
            return bb->ht[slot & bb->ht_mask].ix;
        }
    }

    if (__builtin_popcount((unsigned)bb->insts) < 2) {
        int const inst_cap = bb->insts ? 2*bb->insts : 1,
                    ht_cap = 2*inst_cap;
        bb->inst = realloc(bb->inst, (size_t)inst_cap * sizeof *bb->inst);

        struct hash_slot *old     = bb->ht;
        int const         old_cap = bb->ht ? bb->ht_mask + 1 : 0;
        bb->ht      = calloc((size_t)ht_cap, sizeof *bb->ht);
        bb->ht_mask = ht_cap - 1;
        for (int i = 0; i < old_cap; i++) {
            for (int slot = (int)old[i].hash; old[i].hash; slot++) {
                if (bb->ht[slot & bb->ht_mask].hash == 0) {
                    bb->ht[slot & bb->ht_mask] = old[i];
                    break;
                }
            }
        }
        free(old);
    }

    int const id = bb->insts++;
    bb->inst[id] = inst;
    if (!is_store(inst.op)) {
        for (int slot = (int)h; ; slot++) {
            if (bb->ht[slot & bb->ht_mask].hash == 0) {
                bb->ht[slot & bb->ht_mask] = (struct hash_slot){h, id};
                break;
            }
        }
    }
    return id;
}
#define push(bb,...) push_(bb, (struct bb_inst){.op=__VA_ARGS__})

struct umbra_basic_block* umbra_basic_block(void) {
    struct umbra_basic_block *bb = calloc(1, sizeof *bb);
    push(bb, op_imm_32, .imm=0);
    return bb;
}

void umbra_basic_block_free(struct umbra_basic_block *bb) {
    free(bb->inst);
    free(bb->ht);
    free(bb);
}

umbra_v32 umbra_lane(struct umbra_basic_block *bb) {
    return (umbra_v32){push(bb, op_lane)};
}

umbra_v16 umbra_imm_16(struct umbra_basic_block *bb, uint16_t bits) {
    return (umbra_v16){push(bb, op_imm_16, .imm=(int16_t)bits)};
}

umbra_v32 umbra_imm_32(struct umbra_basic_block *bb, uint32_t bits) {
    return (umbra_v32){push(bb, op_imm_32, .imm=(int)bits)};
}

umbra_v16 umbra_load_16(struct umbra_basic_block *bb, umbra_ptr src, umbra_v32 ix) {
    return (umbra_v16){push(bb, op_load_16, .x=ix.id, .ptr=src.ix)};
}

umbra_v32 umbra_load_32(struct umbra_basic_block *bb, umbra_ptr src, umbra_v32 ix) {
    return (umbra_v32){push(bb, op_load_32, .x=ix.id, .ptr=src.ix)};
}

void umbra_store_16(struct umbra_basic_block *bb,
                    umbra_ptr dst, umbra_v32 ix, umbra_v16 val) {
    push(bb, op_store_16, .x=ix.id, .y=val.id, .ptr=dst.ix);
}

void umbra_store_32(struct umbra_basic_block *bb,
                    umbra_ptr dst, umbra_v32 ix, umbra_v32 val) {
    push(bb, op_store_32, .x=ix.id, .y=val.id, .ptr=dst.ix);
}

static _Bool is_imm32(struct umbra_basic_block *bb, int id, int val) {
    return bb->inst[id].op == op_imm_32 && bb->inst[id].imm == val;
}
static _Bool is_imm16(struct umbra_basic_block *bb, int id, int val) {
    return bb->inst[id].op == op_imm_16 && bb->inst[id].imm == val;
}

static _Bool is_imm(struct umbra_basic_block *bb, int id) {
    return bb->inst[id].op == op_imm_32 || bb->inst[id].op == op_imm_16;
}

static int math_(struct umbra_basic_block *bb, struct bb_inst inst) {
    if (is_imm(bb, inst.x) && is_imm(bb, inst.y) && is_imm(bb, inst.z)) {
        int result = const_eval(inst.op,
                               bb->inst[inst.x].imm,
                               bb->inst[inst.y].imm,
                               bb->inst[inst.z].imm);
        return is_16bit_result(inst.op) ? umbra_imm_16(bb, (uint16_t)result).id
                                        : umbra_imm_32(bb, (uint32_t)result).id;
    }
    return push_(bb, inst);
}
#define math(bb,...) math_(bb, (struct bb_inst){.op=__VA_ARGS__})

static void sort(int *a, int *b) {
    if (*a > *b) {
        int const t = *a;
        *a = *b;
        *b = t;
    }
}

umbra_v32 umbra_add_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    sort(&a.id, &b.id);
    int x = a.id, y = b.id;
    if (bb->inst[x].op == op_mul_f32) {
        return (umbra_v32){math(bb, op_fma_f32, .x=bb->inst[x].x, .y=bb->inst[x].y, .z=y)};
    }
    if (bb->inst[y].op == op_mul_f32) {
        return (umbra_v32){math(bb, op_fma_f32, .x=bb->inst[y].x, .y=bb->inst[y].y, .z=x)};
    }
    return (umbra_v32){math(bb, op_add_f32, .x=x, .y=y)};
}

umbra_v32 umbra_sub_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    if (is_imm32(bb, b.id, 0)) { return a; }
    return (umbra_v32){math(bb, op_sub_f32, .x=a.id, .y=b.id)};
}

umbra_v32 umbra_mul_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    sort(&a.id, &b.id);
    if (is_imm32(bb, a.id, f32_bits(1.0f))) { return b; }
    if (is_imm32(bb, b.id, f32_bits(1.0f))) { return a; }
    return (umbra_v32){math(bb, op_mul_f32, .x=a.id, .y=b.id)};
}

umbra_v32 umbra_div_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    if (is_imm32(bb, b.id, f32_bits(1.0f))) { return a; }
    return (umbra_v32){math(bb, op_div_f32, .x=a.id, .y=b.id)};
}

umbra_v32 umbra_min_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    sort(&a.id, &b.id);
    return (umbra_v32){math(bb, op_min_f32, .x=a.id, .y=b.id)};
}

umbra_v32 umbra_max_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    sort(&a.id, &b.id);
    return (umbra_v32){math(bb, op_max_f32, .x=a.id, .y=b.id)};
}

umbra_v32 umbra_sqrt_f32(struct umbra_basic_block *bb, umbra_v32 a) {
    return (umbra_v32){math(bb, op_sqrt_f32, .x=a.id)};
}

// ── f16 arithmetic ───────────────────────────────────────────────────

umbra_v16 umbra_add_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    sort(&a.id, &b.id);
    int x = a.id, y = b.id;
    if (bb->inst[x].op == op_mul_f16) {
        return (umbra_v16){math(bb, op_fma_f16, .x=bb->inst[x].x, .y=bb->inst[x].y, .z=y)};
    }
    if (bb->inst[y].op == op_mul_f16) {
        return (umbra_v16){math(bb, op_fma_f16, .x=bb->inst[y].x, .y=bb->inst[y].y, .z=x)};
    }
    return (umbra_v16){math(bb, op_add_f16, .x=x, .y=y)};
}

umbra_v16 umbra_sub_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    if (is_imm16(bb, b.id, 0)) { return a; }
    return (umbra_v16){math(bb, op_sub_f16, .x=a.id, .y=b.id)};
}

umbra_v16 umbra_mul_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    sort(&a.id, &b.id);
    if (is_imm16(bb, a.id, 0x3c00)) { return b; }
    if (is_imm16(bb, b.id, 0x3c00)) { return a; }
    return (umbra_v16){math(bb, op_mul_f16, .x=a.id, .y=b.id)};
}

umbra_v16 umbra_div_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    if (is_imm16(bb, b.id, 0x3c00)) { return a; }
    return (umbra_v16){math(bb, op_div_f16, .x=a.id, .y=b.id)};
}

umbra_v16 umbra_min_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    sort(&a.id, &b.id);
    return (umbra_v16){math(bb, op_min_f16, .x=a.id, .y=b.id)};
}

umbra_v16 umbra_max_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    sort(&a.id, &b.id);
    return (umbra_v16){math(bb, op_max_f16, .x=a.id, .y=b.id)};
}

umbra_v16 umbra_sqrt_f16(struct umbra_basic_block *bb, umbra_v16 a) {
    return (umbra_v16){math(bb, op_sqrt_f16, .x=a.id)};
}

// ── i32 arithmetic ───────────────────────────────────────────────────

umbra_v32 umbra_add_i32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    sort(&a.id, &b.id);
    if (is_imm32(bb, a.id, 0)) { return b; }
    if (is_imm32(bb, b.id, 0)) { return a; }
    return (umbra_v32){math(bb, op_add_i32, .x=a.id, .y=b.id)};
}

umbra_v32 umbra_sub_i32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    if (is_imm32(bb, b.id, 0)) { return a; }
    if (a.id == b.id) { return umbra_imm_32(bb, 0); }
    return (umbra_v32){math(bb, op_sub_i32, .x=a.id, .y=b.id)};
}

umbra_v32 umbra_mul_i32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    sort(&a.id, &b.id);
    if (is_imm32(bb, a.id, 1)) { return b; }
    if (is_imm32(bb, b.id, 1)) { return a; }
    if (is_imm32(bb, a.id, 0)) { return a; }
    if (is_imm32(bb, b.id, 0)) { return b; }
    if (bb->inst[a.id].op == op_imm_32 && bb->inst[a.id].imm > 0
        && (bb->inst[a.id].imm & (bb->inst[a.id].imm - 1)) == 0) {
        umbra_v32 shift = umbra_imm_32(bb, (uint32_t)__builtin_ctz(
            (unsigned)bb->inst[a.id].imm));
        return (umbra_v32){math(bb, op_shl_i32, .x=b.id, .y=shift.id)};
    }
    if (bb->inst[b.id].op == op_imm_32 && bb->inst[b.id].imm > 0
        && (bb->inst[b.id].imm & (bb->inst[b.id].imm - 1)) == 0) {
        umbra_v32 shift = umbra_imm_32(bb, (uint32_t)__builtin_ctz(
            (unsigned)bb->inst[b.id].imm));
        return (umbra_v32){math(bb, op_shl_i32, .x=a.id, .y=shift.id)};
    }
    return (umbra_v32){math(bb, op_mul_i32, .x=a.id, .y=b.id)};
}

// ── i16 arithmetic ───────────────────────────────────────────────────

umbra_v16 umbra_add_i16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    sort(&a.id, &b.id);
    if (is_imm16(bb, a.id, 0)) { return b; }
    if (is_imm16(bb, b.id, 0)) { return a; }
    return (umbra_v16){math(bb, op_add_i16, .x=a.id, .y=b.id)};
}

umbra_v16 umbra_sub_i16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    if (is_imm16(bb, b.id, 0)) { return a; }
    if (a.id == b.id) { return umbra_imm_16(bb, 0); }
    return (umbra_v16){math(bb, op_sub_i16, .x=a.id, .y=b.id)};
}

umbra_v16 umbra_mul_i16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    sort(&a.id, &b.id);
    if (is_imm16(bb, a.id, 1)) { return b; }
    if (is_imm16(bb, b.id, 1)) { return a; }
    if (is_imm16(bb, a.id, 0)) { return a; }
    if (is_imm16(bb, b.id, 0)) { return b; }
    {
        int16_t av = (int16_t)bb->inst[a.id].imm;
        if (bb->inst[a.id].op == op_imm_16 && av > 0 && (av & (av - 1)) == 0) {
            umbra_v16 shift = umbra_imm_16(bb, (uint16_t)__builtin_ctz((unsigned)(uint16_t)av));
            return (umbra_v16){math(bb, op_shl_i16, .x=b.id, .y=shift.id)};
        }
    }
    {
        int16_t bv = (int16_t)bb->inst[b.id].imm;
        if (bb->inst[b.id].op == op_imm_16 && bv > 0 && (bv & (bv - 1)) == 0) {
            umbra_v16 shift = umbra_imm_16(bb, (uint16_t)__builtin_ctz((unsigned)(uint16_t)bv));
            return (umbra_v16){math(bb, op_shl_i16, .x=a.id, .y=shift.id)};
        }
    }
    return (umbra_v16){math(bb, op_mul_i16, .x=a.id, .y=b.id)};
}

// ── shifts ───────────────────────────────────────────────────────────

umbra_v32 umbra_shl_i32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    if (is_imm32(bb, b.id, 0)) { return a; }
    return (umbra_v32){math(bb, op_shl_i32, .x=a.id, .y=b.id)};
}
umbra_v32 umbra_shr_u32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    if (is_imm32(bb, b.id, 0)) { return a; }
    return (umbra_v32){math(bb, op_shr_u32, .x=a.id, .y=b.id)};
}
umbra_v32 umbra_shr_s32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    if (is_imm32(bb, b.id, 0)) { return a; }
    return (umbra_v32){math(bb, op_shr_s32, .x=a.id, .y=b.id)};
}

umbra_v16 umbra_shl_i16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    if (is_imm16(bb, b.id, 0)) { return a; }
    return (umbra_v16){math(bb, op_shl_i16, .x=a.id, .y=b.id)};
}
umbra_v16 umbra_shr_u16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    if (is_imm16(bb, b.id, 0)) { return a; }
    return (umbra_v16){math(bb, op_shr_u16, .x=a.id, .y=b.id)};
}
umbra_v16 umbra_shr_s16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    if (is_imm16(bb, b.id, 0)) { return a; }
    return (umbra_v16){math(bb, op_shr_s16, .x=a.id, .y=b.id)};
}

// ── bitwise ──────────────────────────────────────────────────────────

umbra_v32 umbra_and_32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    sort(&a.id, &b.id);
    if (is_imm32(bb, a.id, -1)) { return b; }
    if (is_imm32(bb, b.id, -1)) { return a; }
    if (is_imm32(bb, a.id,  0)) { return a; }
    if (is_imm32(bb, b.id,  0)) { return b; }
    return (umbra_v32){math(bb, op_and_32, .x=a.id, .y=b.id)};
}
umbra_v32 umbra_or_32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    sort(&a.id, &b.id);
    if (is_imm32(bb, a.id,  0)) { return b; }
    if (is_imm32(bb, b.id,  0)) { return a; }
    if (is_imm32(bb, a.id, -1)) { return a; }
    if (is_imm32(bb, b.id, -1)) { return b; }
    return (umbra_v32){math(bb, op_or_32, .x=a.id, .y=b.id)};
}
umbra_v32 umbra_xor_32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) {
    sort(&a.id, &b.id);
    if (is_imm32(bb, a.id, 0)) { return b; }
    if (is_imm32(bb, b.id, 0)) { return a; }
    if (a.id == b.id) { return umbra_imm_32(bb, 0); }
    return (umbra_v32){math(bb, op_xor_32, .x=a.id, .y=b.id)};
}
umbra_v32 umbra_sel_32(struct umbra_basic_block *bb, umbra_v32 c, umbra_v32 t, umbra_v32 f) {
    if (t.id == f.id) { return t; }
    if (is_imm32(bb, c.id, -1)) { return t; }
    if (is_imm32(bb, c.id,  0)) { return f; }
    return (umbra_v32){math(bb, op_sel_32, .x=c.id, .y=t.id, .z=f.id)};
}

umbra_v16 umbra_and_16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    sort(&a.id, &b.id);
    if (is_imm16(bb, a.id, -1)) { return b; }
    if (is_imm16(bb, b.id, -1)) { return a; }
    if (is_imm16(bb, a.id,  0)) { return a; }
    if (is_imm16(bb, b.id,  0)) { return b; }
    return (umbra_v16){math(bb, op_and_16, .x=a.id, .y=b.id)};
}
umbra_v16 umbra_or_16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    sort(&a.id, &b.id);
    if (is_imm16(bb, a.id,  0)) { return b; }
    if (is_imm16(bb, b.id,  0)) { return a; }
    if (is_imm16(bb, a.id, -1)) { return a; }
    if (is_imm16(bb, b.id, -1)) { return b; }
    return (umbra_v16){math(bb, op_or_16, .x=a.id, .y=b.id)};
}
umbra_v16 umbra_xor_16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) {
    sort(&a.id, &b.id);
    if (is_imm16(bb, a.id, 0)) { return b; }
    if (is_imm16(bb, b.id, 0)) { return a; }
    if (a.id == b.id) { return umbra_imm_16(bb, 0); }
    return (umbra_v16){math(bb, op_xor_16, .x=a.id, .y=b.id)};
}
umbra_v16 umbra_sel_16(struct umbra_basic_block *bb, umbra_v16 c, umbra_v16 t, umbra_v16 f) {
    if (t.id == f.id) { return t; }
    if (is_imm16(bb, c.id, -1)) { return t; }
    if (is_imm16(bb, c.id,  0)) { return f; }
    return (umbra_v16){math(bb, op_sel_16, .x=c.id, .y=t.id, .z=f.id)};
}

// ── conversions ──────────────────────────────────────────────────────

umbra_v16 umbra_f16_from_f32(struct umbra_basic_block *bb, umbra_v32 a) {
    return (umbra_v16){math(bb, op_f16_from_f32, .x=a.id)};
}
umbra_v32 umbra_f32_from_f16(struct umbra_basic_block *bb, umbra_v16 a) {
    return (umbra_v32){math(bb, op_f32_from_f16, .x=a.id)};
}
umbra_v32 umbra_f32_from_i32(struct umbra_basic_block *bb, umbra_v32 a) {
    return (umbra_v32){math(bb, op_f32_from_i32, .x=a.id)};
}
umbra_v32 umbra_i32_from_f32(struct umbra_basic_block *bb, umbra_v32 a) {
    return (umbra_v32){math(bb, op_i32_from_f32, .x=a.id)};
}

// ── comparisons ──────────────────────────────────────────────────────

umbra_v16 umbra_eq_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { sort(&a.id, &b.id); return (umbra_v16){math(bb, op_eq_f16, .x=a.id, .y=b.id)}; }
umbra_v16 umbra_ne_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { sort(&a.id, &b.id); return (umbra_v16){math(bb, op_ne_f16, .x=a.id, .y=b.id)}; }
umbra_v16 umbra_lt_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){math(bb, op_lt_f16, .x=a.id, .y=b.id)}; }
umbra_v16 umbra_le_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){math(bb, op_le_f16, .x=a.id, .y=b.id)}; }
umbra_v16 umbra_gt_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){math(bb, op_gt_f16, .x=a.id, .y=b.id)}; }
umbra_v16 umbra_ge_f16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){math(bb, op_ge_f16, .x=a.id, .y=b.id)}; }

umbra_v32 umbra_eq_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { sort(&a.id, &b.id); return (umbra_v32){math(bb, op_eq_f32, .x=a.id, .y=b.id)}; }
umbra_v32 umbra_ne_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { sort(&a.id, &b.id); return (umbra_v32){math(bb, op_ne_f32, .x=a.id, .y=b.id)}; }
umbra_v32 umbra_lt_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){math(bb, op_lt_f32, .x=a.id, .y=b.id)}; }
umbra_v32 umbra_le_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){math(bb, op_le_f32, .x=a.id, .y=b.id)}; }
umbra_v32 umbra_gt_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){math(bb, op_gt_f32, .x=a.id, .y=b.id)}; }
umbra_v32 umbra_ge_f32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){math(bb, op_ge_f32, .x=a.id, .y=b.id)}; }

umbra_v16 umbra_eq_i16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { sort(&a.id, &b.id); return (umbra_v16){math(bb, op_eq_i16, .x=a.id, .y=b.id)}; }
umbra_v16 umbra_ne_i16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { sort(&a.id, &b.id); return (umbra_v16){math(bb, op_ne_i16, .x=a.id, .y=b.id)}; }
umbra_v32 umbra_eq_i32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { sort(&a.id, &b.id); return (umbra_v32){math(bb, op_eq_i32, .x=a.id, .y=b.id)}; }
umbra_v32 umbra_ne_i32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { sort(&a.id, &b.id); return (umbra_v32){math(bb, op_ne_i32, .x=a.id, .y=b.id)}; }

umbra_v16 umbra_lt_s16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){math(bb, op_lt_s16, .x=a.id, .y=b.id)}; }
umbra_v16 umbra_le_s16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){math(bb, op_le_s16, .x=a.id, .y=b.id)}; }
umbra_v16 umbra_gt_s16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){math(bb, op_gt_s16, .x=a.id, .y=b.id)}; }
umbra_v16 umbra_ge_s16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){math(bb, op_ge_s16, .x=a.id, .y=b.id)}; }
umbra_v32 umbra_lt_s32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){math(bb, op_lt_s32, .x=a.id, .y=b.id)}; }
umbra_v32 umbra_le_s32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){math(bb, op_le_s32, .x=a.id, .y=b.id)}; }
umbra_v32 umbra_gt_s32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){math(bb, op_gt_s32, .x=a.id, .y=b.id)}; }
umbra_v32 umbra_ge_s32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){math(bb, op_ge_s32, .x=a.id, .y=b.id)}; }

umbra_v16 umbra_lt_u16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){math(bb, op_lt_u16, .x=a.id, .y=b.id)}; }
umbra_v16 umbra_le_u16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){math(bb, op_le_u16, .x=a.id, .y=b.id)}; }
umbra_v16 umbra_gt_u16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){math(bb, op_gt_u16, .x=a.id, .y=b.id)}; }
umbra_v16 umbra_ge_u16(struct umbra_basic_block *bb, umbra_v16 a, umbra_v16 b) { return (umbra_v16){math(bb, op_ge_u16, .x=a.id, .y=b.id)}; }
umbra_v32 umbra_lt_u32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){math(bb, op_lt_u32, .x=a.id, .y=b.id)}; }
umbra_v32 umbra_le_u32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){math(bb, op_le_u32, .x=a.id, .y=b.id)}; }
umbra_v32 umbra_gt_u32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){math(bb, op_gt_u32, .x=a.id, .y=b.id)}; }
umbra_v32 umbra_ge_u32(struct umbra_basic_block *bb, umbra_v32 a, umbra_v32 b) { return (umbra_v32){math(bb, op_ge_u32, .x=a.id, .y=b.id)}; }
