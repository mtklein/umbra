#include "../umbra.h"
#include "bb.h"
#include <math.h>
#include <stdlib.h>

typedef struct umbra_basic_block BB;
typedef umbra_v16 v16;
typedef umbra_v32 v32;

static _Bool is_pow2        (int x) { return __builtin_popcount((unsigned)x) == 1; }
static _Bool is_pow2_or_zero(int x) { return __builtin_popcount((unsigned)x) <= 1; }

static int   f32_bits     (float v) { union { float f; int i; } u = {.f=v}; return u.i; }
static float f32_from_bits(int   v) { union { float f; int i; } u = {.i=v}; return u.f; }

static float scalar_f16_to_f32(uint16_t h) {
    union { __fp16 h; uint16_t u; } v = {.u = h};
    return (float)v.h;
}
static uint16_t scalar_f32_to_f16(float f) {
    union { __fp16 h; uint16_t u; } v = {.h = (__fp16)f};
    return v.u;
}

static int const_eval(enum op op, int xb, int yb, int zb) {
    float xf = f32_from_bits(xb), yf = f32_from_bits(yb), zf = f32_from_bits(zb);
    uint32_t xu = (uint32_t)xb, yu = (uint32_t)yb;
    uint16_t xh = (uint16_t)xb, yh = (uint16_t)yb, zh = (uint16_t)zb;
    switch (op) {
        case op_add_f32:  return f32_bits(xf + yf);
        case op_sub_f32:  return f32_bits(xf - yf);
        case op_mul_f32:  return f32_bits(xf * yf);
        case op_div_f32:  return f32_bits(xf / yf);
        case op_min_f32:  return f32_bits(fminf(xf, yf));
        case op_max_f32:  return f32_bits(fmaxf(xf, yf));
        case op_sqrt_f32: return f32_bits(sqrtf(xf));
        case op_fma_f32:  return f32_bits(xf * yf + zf);

        case op_add_f16:  return scalar_f32_to_f16(scalar_f16_to_f32(xh) + scalar_f16_to_f32(yh));
        case op_sub_f16:  return scalar_f32_to_f16(scalar_f16_to_f32(xh) - scalar_f16_to_f32(yh));
        case op_mul_f16:  return scalar_f32_to_f16(scalar_f16_to_f32(xh) * scalar_f16_to_f32(yh));
        case op_div_f16:  return scalar_f32_to_f16(scalar_f16_to_f32(xh) / scalar_f16_to_f32(yh));
        case op_min_f16:  return scalar_f32_to_f16(fminf(scalar_f16_to_f32(xh),
                                                         scalar_f16_to_f32(yh)));
        case op_max_f16:  return scalar_f32_to_f16(fmaxf(scalar_f16_to_f32(xh),
                                                         scalar_f16_to_f32(yh)));
        case op_sqrt_f16: return scalar_f32_to_f16(sqrtf(scalar_f16_to_f32(xh)));
        case op_fma_f16:  return scalar_f32_to_f16(scalar_f16_to_f32(xh) * scalar_f16_to_f32(yh)
                                                                         + scalar_f16_to_f32(zh));

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

        case op_f16_from_f32: return scalar_f32_to_f16(xf);
        case op_f32_from_f16: return f32_bits(scalar_f16_to_f32(xh));
        case op_f32_from_i32: return f32_bits((float)xb);
        case op_i32_from_f32: return (int)xf;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
        case op_eq_f32: return -(int)(xf == yf);
        case op_ne_f32: return -(int)(xf != yf);
#pragma clang diagnostic pop
        case op_lt_f32: return -(int)(xf <  yf);
        case op_le_f32: return -(int)(xf <= yf);
        case op_gt_f32: return -(int)(xf >  yf);
        case op_ge_f32: return -(int)(xf >= yf);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
        case op_eq_f16: return (uint16_t)-(int)(scalar_f16_to_f32(xh) == scalar_f16_to_f32(yh));
        case op_ne_f16: return (uint16_t)-(int)(scalar_f16_to_f32(xh) != scalar_f16_to_f32(yh));
#pragma clang diagnostic pop
        case op_lt_f16: return (uint16_t)-(int)(scalar_f16_to_f32(xh) <  scalar_f16_to_f32(yh));
        case op_le_f16: return (uint16_t)-(int)(scalar_f16_to_f32(xh) <= scalar_f16_to_f32(yh));
        case op_gt_f16: return (uint16_t)-(int)(scalar_f16_to_f32(xh) >  scalar_f16_to_f32(yh));
        case op_ge_f16: return (uint16_t)-(int)(scalar_f16_to_f32(xh) >= scalar_f16_to_f32(yh));

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

static uint32_t bb_inst_hash(struct bb_inst const *inst) {
    uint32_t               h = (uint32_t)inst->op;
    __builtin_mul_overflow(h ^ (uint32_t)inst->x  ,  0x9e3779b9u, &h);
    __builtin_mul_overflow(h ^ (uint32_t)inst->y  ,  0x9e3779b9u, &h);
    __builtin_mul_overflow(h ^ (uint32_t)inst->z  ,  0x9e3779b9u, &h);
    __builtin_mul_overflow(h ^ (uint32_t)inst->imm,  0x9e3779b9u, &h);
    h ^= h >> 16;
    return h ? h : 1;
}

static int push_(BB *bb, struct bb_inst inst) {
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

    if (is_pow2_or_zero(bb->insts)) {
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

BB* umbra_basic_block(void) {
    BB *bb = calloc(1, sizeof *bb);
    push(bb, op_imm_32, .imm=0);  // Simplifies liveness analysis to know id 0 is imm=0.
    return bb;
}

void umbra_basic_block_free(BB *bb) {
    free(bb->inst);
    free(bb->ht);
    free(bb);
}

v32 umbra_lane(BB *bb) {
    return (v32){push(bb, op_lane)};
}

v16 umbra_imm_16(BB *bb, uint16_t bits) { return (v16){push(bb, op_imm_16, .imm=(int16_t)bits)}; }
v32 umbra_imm_32(BB *bb, uint32_t bits) { return (v32){push(bb, op_imm_32, .imm=(int)    bits)}; }

v16 umbra_load_16(BB *bb, umbra_ptr src, v32 ix) {
    return (v16){push(bb, op_load_16, .x=ix.id, .ptr=src.ix)};
}
v32 umbra_load_32(BB *bb, umbra_ptr src, v32 ix) {
    return (v32){push(bb, op_load_32, .x=ix.id, .ptr=src.ix)};
}

void umbra_store_16(BB *bb, umbra_ptr dst, v32 ix, v16 val) {
    push(bb, op_store_16, .x=ix.id, .y=val.id, .ptr=dst.ix);
}
void umbra_store_32(BB *bb, umbra_ptr dst, v32 ix, v32 val) {
    push(bb, op_store_32, .x=ix.id, .y=val.id, .ptr=dst.ix);
}

static _Bool is_imm16(BB *bb, int id, int val) {
    return bb->inst[id].op  == op_imm_16
        && bb->inst[id].imm == val;
}
static _Bool is_imm32(BB *bb, int id, int val) {
    return bb->inst[id].op  == op_imm_32
        && bb->inst[id].imm == val;
}

static _Bool is_imm(BB *bb, int id) {
    return bb->inst[id].op == op_imm_16
        || bb->inst[id].op == op_imm_32;
}

static int math_(BB *bb, struct bb_inst inst) {
    if (is_imm(bb, inst.x) && is_imm(bb, inst.y) && is_imm(bb, inst.z)) {
        int const result = const_eval(inst.op,
                                      bb->inst[inst.x].imm,
                                      bb->inst[inst.y].imm,
                                      bb->inst[inst.z].imm);
        return is_16bit(inst.op) ? umbra_imm_16(bb, (uint16_t)result).id
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

v32 umbra_add_f32(BB *bb, v32 a, v32 b) {
    sort(&a.id, &b.id);
    int const x = a.id,
              y = b.id;
    if (bb->inst[x].op == op_mul_f32) {
        return (v32){math(bb, op_fma_f32, .x=bb->inst[x].x, .y=bb->inst[x].y, .z=y)};
    }
    if (bb->inst[y].op == op_mul_f32) {
        return (v32){math(bb, op_fma_f32, .x=bb->inst[y].x, .y=bb->inst[y].y, .z=x)};
    }
    return (v32){math(bb, op_add_f32, .x=x, .y=y)};
}

v32 umbra_sub_f32(BB *bb, v32 a, v32 b) {
    if (is_imm32(bb, b.id, 0)) { return a; }
    return (v32){math(bb, op_sub_f32, .x=a.id, .y=b.id)};
}

v32 umbra_mul_f32(BB *bb, v32 a, v32 b) {
    sort(&a.id, &b.id);
    if (is_imm32(bb, a.id, f32_bits(1.0f))) { return b; }
    if (is_imm32(bb, b.id, f32_bits(1.0f))) { return a; }
    return (v32){math(bb, op_mul_f32, .x=a.id, .y=b.id)};
}

v32 umbra_div_f32(BB *bb, v32 a, v32 b) {
    if (is_imm32(bb, b.id, f32_bits(1.0f))) { return a; }
    return (v32){math(bb, op_div_f32, .x=a.id, .y=b.id)};
}

v32 umbra_min_f32(BB *bb, v32 a, v32 b) {
    sort(&a.id, &b.id);
    return (v32){math(bb, op_min_f32, .x=a.id, .y=b.id)};
}

v32 umbra_max_f32(BB *bb, v32 a, v32 b) {
    sort(&a.id, &b.id);
    return (v32){math(bb, op_max_f32, .x=a.id, .y=b.id)};
}

v32 umbra_sqrt_f32(BB *bb, v32 a) {
    return (v32){math(bb, op_sqrt_f32, .x=a.id)};
}

v16 umbra_add_f16(BB *bb, v16 a, v16 b) {
    sort(&a.id, &b.id);
    int const x = a.id,
              y = b.id;
    if (bb->inst[x].op == op_mul_f16) {
        return (v16){math(bb, op_fma_f16, .x=bb->inst[x].x, .y=bb->inst[x].y, .z=y)};
    }
    if (bb->inst[y].op == op_mul_f16) {
        return (v16){math(bb, op_fma_f16, .x=bb->inst[y].x, .y=bb->inst[y].y, .z=x)};
    }
    return (v16){math(bb, op_add_f16, .x=x, .y=y)};
}

v16 umbra_sub_f16(BB *bb, v16 a, v16 b) {
    if (is_imm16(bb, b.id, 0)) { return a; }
    return (v16){math(bb, op_sub_f16, .x=a.id, .y=b.id)};
}

v16 umbra_mul_f16(BB *bb, v16 a, v16 b) {
    sort(&a.id, &b.id);
    if (is_imm16(bb, a.id, 0x3c00)) { return b; }
    if (is_imm16(bb, b.id, 0x3c00)) { return a; }
    return (v16){math(bb, op_mul_f16, .x=a.id, .y=b.id)};
}

v16 umbra_div_f16(BB *bb, v16 a, v16 b) {
    if (is_imm16(bb, b.id, 0x3c00)) { return a; }
    return (v16){math(bb, op_div_f16, .x=a.id, .y=b.id)};
}

v16 umbra_min_f16(BB *bb, v16 a, v16 b) {
    sort(&a.id, &b.id);
    return (v16){math(bb, op_min_f16, .x=a.id, .y=b.id)};
}

v16 umbra_max_f16(BB *bb, v16 a, v16 b) {
    sort(&a.id, &b.id);
    return (v16){math(bb, op_max_f16, .x=a.id, .y=b.id)};
}

v16 umbra_sqrt_f16(BB *bb, v16 a) {
    return (v16){math(bb, op_sqrt_f16, .x=a.id)};
}

v32 umbra_add_i32(BB *bb, v32 a, v32 b) {
    sort(&a.id, &b.id);
    if (is_imm32(bb, a.id, 0)) { return b; }
    if (is_imm32(bb, b.id, 0)) { return a; }
    return (v32){math(bb, op_add_i32, .x=a.id, .y=b.id)};
}

v32 umbra_sub_i32(BB *bb, v32 a, v32 b) {
    if (is_imm32(bb, b.id, 0)) { return a; }
    if (a.id == b.id) { return umbra_imm_32(bb, 0); }
    return (v32){math(bb, op_sub_i32, .x=a.id, .y=b.id)};
}

v32 umbra_mul_i32(BB *bb, v32 a, v32 b) {
    sort(&a.id, &b.id);
    if (is_imm32(bb, a.id, 1)) { return b; }
    if (is_imm32(bb, b.id, 1)) { return a; }
    if (is_imm32(bb, a.id, 0)) { return a; }
    if (is_imm32(bb, b.id, 0)) { return b; }
    if (bb->inst[a.id].op == op_imm_32 && is_pow2(bb->inst[a.id].imm)) {
        int const shift = __builtin_ctz((unsigned)bb->inst[a.id].imm);
        return umbra_shl_i32(bb, b, umbra_imm_32(bb, (uint32_t)shift));
    }
    if (bb->inst[b.id].op == op_imm_32 && is_pow2(bb->inst[b.id].imm)) {
        int const shift = __builtin_ctz((unsigned)bb->inst[b.id].imm);
        return umbra_shl_i32(bb, a, umbra_imm_32(bb, (uint32_t)shift));
    }
    return (v32){math(bb, op_mul_i32, .x=a.id, .y=b.id)};
}

v16 umbra_add_i16(BB *bb, v16 a, v16 b) {
    sort(&a.id, &b.id);
    if (is_imm16(bb, a.id, 0)) { return b; }
    if (is_imm16(bb, b.id, 0)) { return a; }
    return (v16){math(bb, op_add_i16, .x=a.id, .y=b.id)};
}

v16 umbra_sub_i16(BB *bb, v16 a, v16 b) {
    if (is_imm16(bb, b.id, 0)) { return a; }
    if (a.id == b.id) { return umbra_imm_16(bb, 0); }
    return (v16){math(bb, op_sub_i16, .x=a.id, .y=b.id)};
}

v16 umbra_mul_i16(BB *bb, v16 a, v16 b) {
    sort(&a.id, &b.id);
    if (is_imm16(bb, a.id, 1)) { return b; }
    if (is_imm16(bb, b.id, 1)) { return a; }
    if (is_imm16(bb, a.id, 0)) { return a; }
    if (is_imm16(bb, b.id, 0)) { return b; }
    if (bb->inst[a.id].op == op_imm_16 && is_pow2(bb->inst[a.id].imm)) {
        int const shift = __builtin_ctz((unsigned)bb->inst[a.id].imm);
        return umbra_shl_i16(bb, b, umbra_imm_16(bb, (uint16_t)shift));
    }
    if (bb->inst[b.id].op == op_imm_16 && is_pow2(bb->inst[b.id].imm)) {
        int const shift = __builtin_ctz((unsigned)bb->inst[b.id].imm);
        return umbra_shl_i16(bb, a, umbra_imm_16(bb, (uint16_t)shift));
    }
    return (v16){math(bb, op_mul_i16, .x=a.id, .y=b.id)};
}

v32 umbra_shl_i32(BB *bb, v32 a, v32 b) {
    if (is_imm32(bb, b.id, 0)) { return a; }
    return (v32){math(bb, op_shl_i32, .x=a.id, .y=b.id)};
}
v32 umbra_shr_u32(BB *bb, v32 a, v32 b) {
    if (is_imm32(bb, b.id, 0)) { return a; }
    return (v32){math(bb, op_shr_u32, .x=a.id, .y=b.id)};
}
v32 umbra_shr_s32(BB *bb, v32 a, v32 b) {
    if (is_imm32(bb, b.id, 0)) { return a; }
    return (v32){math(bb, op_shr_s32, .x=a.id, .y=b.id)};
}

v16 umbra_shl_i16(BB *bb, v16 a, v16 b) {
    if (is_imm16(bb, b.id, 0)) { return a; }
    return (v16){math(bb, op_shl_i16, .x=a.id, .y=b.id)};
}
v16 umbra_shr_u16(BB *bb, v16 a, v16 b) {
    if (is_imm16(bb, b.id, 0)) { return a; }
    return (v16){math(bb, op_shr_u16, .x=a.id, .y=b.id)};
}
v16 umbra_shr_s16(BB *bb, v16 a, v16 b) {
    if (is_imm16(bb, b.id, 0)) { return a; }
    return (v16){math(bb, op_shr_s16, .x=a.id, .y=b.id)};
}

v32 umbra_and_32(BB *bb, v32 a, v32 b) {
    sort(&a.id, &b.id);
    if (is_imm32(bb, a.id, -1)) { return b; }
    if (is_imm32(bb, b.id, -1)) { return a; }
    if (is_imm32(bb, a.id,  0)) { return a; }
    if (is_imm32(bb, b.id,  0)) { return b; }
    return (v32){math(bb, op_and_32, .x=a.id, .y=b.id)};
}
v32 umbra_or_32(BB *bb, v32 a, v32 b) {
    sort(&a.id, &b.id);
    if (is_imm32(bb, a.id,  0)) { return b; }
    if (is_imm32(bb, b.id,  0)) { return a; }
    if (is_imm32(bb, a.id, -1)) { return a; }
    if (is_imm32(bb, b.id, -1)) { return b; }
    return (v32){math(bb, op_or_32, .x=a.id, .y=b.id)};
}
v32 umbra_xor_32(BB *bb, v32 a, v32 b) {
    sort(&a.id, &b.id);
    if (is_imm32(bb, a.id, 0)) { return b; }
    if (is_imm32(bb, b.id, 0)) { return a; }
    if (a.id == b.id) { return umbra_imm_32(bb, 0); }
    return (v32){math(bb, op_xor_32, .x=a.id, .y=b.id)};
}
v32 umbra_sel_32(BB *bb, v32 c, v32 t, v32 f) {
    if (t.id == f.id) { return t; }
    if (is_imm32(bb, c.id, -1)) { return t; }
    if (is_imm32(bb, c.id,  0)) { return f; }
    return (v32){math(bb, op_sel_32, .x=c.id, .y=t.id, .z=f.id)};
}

v16 umbra_and_16(BB *bb, v16 a, v16 b) {
    sort(&a.id, &b.id);
    if (is_imm16(bb, a.id, -1)) { return b; }
    if (is_imm16(bb, b.id, -1)) { return a; }
    if (is_imm16(bb, a.id,  0)) { return a; }
    if (is_imm16(bb, b.id,  0)) { return b; }
    return (v16){math(bb, op_and_16, .x=a.id, .y=b.id)};
}
v16 umbra_or_16(BB *bb, v16 a, v16 b) {
    sort(&a.id, &b.id);
    if (is_imm16(bb, a.id,  0)) { return b; }
    if (is_imm16(bb, b.id,  0)) { return a; }
    if (is_imm16(bb, a.id, -1)) { return a; }
    if (is_imm16(bb, b.id, -1)) { return b; }
    return (v16){math(bb, op_or_16, .x=a.id, .y=b.id)};
}
v16 umbra_xor_16(BB *bb, v16 a, v16 b) {
    sort(&a.id, &b.id);
    if (is_imm16(bb, a.id, 0)) { return b; }
    if (is_imm16(bb, b.id, 0)) { return a; }
    if (a.id == b.id) { return umbra_imm_16(bb, 0); }
    return (v16){math(bb, op_xor_16, .x=a.id, .y=b.id)};
}
v16 umbra_sel_16(BB *bb, v16 c, v16 t, v16 f) {
    if (t.id == f.id) { return t; }
    if (is_imm16(bb, c.id, -1)) { return t; }
    if (is_imm16(bb, c.id,  0)) { return f; }
    return (v16){math(bb, op_sel_16, .x=c.id, .y=t.id, .z=f.id)};
}

v16 umbra_f16_from_f32(BB *bb, v32 a) {
    return (v16){math(bb, op_f16_from_f32, .x=a.id)};
}
v32 umbra_f32_from_f16(BB *bb, v16 a) {
    return (v32){math(bb, op_f32_from_f16, .x=a.id)};
}
v32 umbra_f32_from_i32(BB *bb, v32 a) {
    return (v32){math(bb, op_f32_from_i32, .x=a.id)};
}
v32 umbra_i32_from_f32(BB *bb, v32 a) {
    return (v32){math(bb, op_i32_from_f32, .x=a.id)};
}

v16 umbra_eq_f16(BB *bb, v16 a, v16 b) {
    sort(&a.id, &b.id);
    return (v16){math(bb, op_eq_f16, .x=a.id, .y=b.id)};
}
v16 umbra_ne_f16(BB *bb, v16 a, v16 b) {
    sort(&a.id, &b.id);
    return (v16){math(bb, op_ne_f16, .x=a.id, .y=b.id)};
}
v16 umbra_lt_f16(BB *bb, v16 a, v16 b) { return (v16){math(bb, op_lt_f16, .x=a.id, .y=b.id)}; }
v16 umbra_le_f16(BB *bb, v16 a, v16 b) { return (v16){math(bb, op_le_f16, .x=a.id, .y=b.id)}; }
v16 umbra_gt_f16(BB *bb, v16 a, v16 b) { return (v16){math(bb, op_gt_f16, .x=a.id, .y=b.id)}; }
v16 umbra_ge_f16(BB *bb, v16 a, v16 b) { return (v16){math(bb, op_ge_f16, .x=a.id, .y=b.id)}; }

v32 umbra_eq_f32(BB *bb, v32 a, v32 b) {
    sort(&a.id, &b.id);
    return (v32){math(bb, op_eq_f32, .x=a.id, .y=b.id)};
}
v32 umbra_ne_f32(BB *bb, v32 a, v32 b) {
    sort(&a.id, &b.id);
    return (v32){math(bb, op_ne_f32, .x=a.id, .y=b.id)};
}
v32 umbra_lt_f32(BB *bb, v32 a, v32 b) { return (v32){math(bb, op_lt_f32, .x=a.id, .y=b.id)}; }
v32 umbra_le_f32(BB *bb, v32 a, v32 b) { return (v32){math(bb, op_le_f32, .x=a.id, .y=b.id)}; }
v32 umbra_gt_f32(BB *bb, v32 a, v32 b) { return (v32){math(bb, op_gt_f32, .x=a.id, .y=b.id)}; }
v32 umbra_ge_f32(BB *bb, v32 a, v32 b) { return (v32){math(bb, op_ge_f32, .x=a.id, .y=b.id)}; }

v16 umbra_eq_i16(BB *bb, v16 a, v16 b) {
    sort(&a.id, &b.id);
    return (v16){math(bb, op_eq_i16, .x=a.id, .y=b.id)};
}
v16 umbra_ne_i16(BB *bb, v16 a, v16 b) {
    sort(&a.id, &b.id);
    return (v16){math(bb, op_ne_i16, .x=a.id, .y=b.id)};
}
v32 umbra_eq_i32(BB *bb, v32 a, v32 b) {
    sort(&a.id, &b.id);
    return (v32){math(bb, op_eq_i32, .x=a.id, .y=b.id)};
}
v32 umbra_ne_i32(BB *bb, v32 a, v32 b) {
    sort(&a.id, &b.id);
    return (v32){math(bb, op_ne_i32, .x=a.id, .y=b.id)};
}

v16 umbra_lt_s16(BB *bb, v16 a, v16 b) { return (v16){math(bb, op_lt_s16, .x=a.id, .y=b.id)}; }
v16 umbra_le_s16(BB *bb, v16 a, v16 b) { return (v16){math(bb, op_le_s16, .x=a.id, .y=b.id)}; }
v16 umbra_gt_s16(BB *bb, v16 a, v16 b) { return (v16){math(bb, op_gt_s16, .x=a.id, .y=b.id)}; }
v16 umbra_ge_s16(BB *bb, v16 a, v16 b) { return (v16){math(bb, op_ge_s16, .x=a.id, .y=b.id)}; }
v32 umbra_lt_s32(BB *bb, v32 a, v32 b) { return (v32){math(bb, op_lt_s32, .x=a.id, .y=b.id)}; }
v32 umbra_le_s32(BB *bb, v32 a, v32 b) { return (v32){math(bb, op_le_s32, .x=a.id, .y=b.id)}; }
v32 umbra_gt_s32(BB *bb, v32 a, v32 b) { return (v32){math(bb, op_gt_s32, .x=a.id, .y=b.id)}; }
v32 umbra_ge_s32(BB *bb, v32 a, v32 b) { return (v32){math(bb, op_ge_s32, .x=a.id, .y=b.id)}; }

v16 umbra_lt_u16(BB *bb, v16 a, v16 b) { return (v16){math(bb, op_lt_u16, .x=a.id, .y=b.id)}; }
v16 umbra_le_u16(BB *bb, v16 a, v16 b) { return (v16){math(bb, op_le_u16, .x=a.id, .y=b.id)}; }
v16 umbra_gt_u16(BB *bb, v16 a, v16 b) { return (v16){math(bb, op_gt_u16, .x=a.id, .y=b.id)}; }
v16 umbra_ge_u16(BB *bb, v16 a, v16 b) { return (v16){math(bb, op_ge_u16, .x=a.id, .y=b.id)}; }
v32 umbra_lt_u32(BB *bb, v32 a, v32 b) { return (v32){math(bb, op_lt_u32, .x=a.id, .y=b.id)}; }
v32 umbra_le_u32(BB *bb, v32 a, v32 b) { return (v32){math(bb, op_le_u32, .x=a.id, .y=b.id)}; }
v32 umbra_gt_u32(BB *bb, v32 a, v32 b) { return (v32){math(bb, op_gt_u32, .x=a.id, .y=b.id)}; }
v32 umbra_ge_u32(BB *bb, v32 a, v32 b) { return (v32){math(bb, op_ge_u32, .x=a.id, .y=b.id)}; }
