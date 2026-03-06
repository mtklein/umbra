#include "../umbra.h"
#include "bb.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct umbra_basic_block BB;
typedef umbra_v16  v16;
typedef umbra_v32  v32;
typedef umbra_half vh;

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

        case op_add_half:  return scalar_f32_to_f16(scalar_f16_to_f32(xh) + scalar_f16_to_f32(yh));
        case op_sub_half:  return scalar_f32_to_f16(scalar_f16_to_f32(xh) - scalar_f16_to_f32(yh));
        case op_mul_half:  return scalar_f32_to_f16(scalar_f16_to_f32(xh) * scalar_f16_to_f32(yh));
        case op_div_half:  return scalar_f32_to_f16(scalar_f16_to_f32(xh) / scalar_f16_to_f32(yh));
        case op_min_half:  return scalar_f32_to_f16(fminf(scalar_f16_to_f32(xh),
                                                          scalar_f16_to_f32(yh)));
        case op_max_half:  return scalar_f32_to_f16(fmaxf(scalar_f16_to_f32(xh),
                                                          scalar_f16_to_f32(yh)));
        case op_sqrt_half: return scalar_f32_to_f16(sqrtf(scalar_f16_to_f32(xh)));
        case op_fma_half:  return scalar_f32_to_f16(scalar_f16_to_f32(xh) * scalar_f16_to_f32(yh)
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

        case op_and_half: return (uint16_t)(xh & yh);
        case op_or_half:  return (uint16_t)(xh | yh);
        case op_xor_half: return (uint16_t)(xh ^ yh);
        case op_sel_half: return (uint16_t)((xh & yh) | (~xh & zh));

        case op_half_from_f32: return scalar_f32_to_f16(xf);
        case op_half_from_i32: return scalar_f32_to_f16((float)xb);
        case op_half_from_i16: return scalar_f32_to_f16((float)(int16_t)xh);
        case op_i16_from_half: { float t = scalar_f16_to_f32(xh); return (int16_t)t; }
        case op_f32_from_half: return f32_bits(scalar_f16_to_f32(xh));
        case op_f32_from_i32: return f32_bits((float)xb);
        case op_i32_from_f32: return (int)xf;
        case op_i32_from_half: { float t = scalar_f16_to_f32(xh); return (int)t; }
        case op_i16_from_i32: return (uint16_t)xb;
        case op_i32_from_i16: return (int32_t)(int16_t)xh;

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
        case op_eq_half: return (uint16_t)-(int)(scalar_f16_to_f32(xh) == scalar_f16_to_f32(yh));
        case op_ne_half: return (uint16_t)-(int)(scalar_f16_to_f32(xh) != scalar_f16_to_f32(yh));
#pragma clang diagnostic pop
        case op_lt_half: return (uint16_t)-(int)(scalar_f16_to_f32(xh) <  scalar_f16_to_f32(yh));
        case op_le_half: return (uint16_t)-(int)(scalar_f16_to_f32(xh) <= scalar_f16_to_f32(yh));
        case op_gt_half: return (uint16_t)-(int)(scalar_f16_to_f32(xh) >  scalar_f16_to_f32(yh));
        case op_ge_half: return (uint16_t)-(int)(scalar_f16_to_f32(xh) >= scalar_f16_to_f32(yh));

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

v16 umbra_imm_16  (BB *bb, uint16_t bits) { return (v16){push(bb, op_imm_16  , .imm=(int)bits)}; }
v32 umbra_imm_32  (BB *bb, uint32_t bits) { return (v32){push(bb, op_imm_32  , .imm=(int)bits)}; }
vh  umbra_imm_half(BB *bb, uint16_t bits) { return (vh ){push(bb, op_imm_half, .imm=(int)bits)}; }

v16 umbra_load_16(BB *bb, umbra_ptr src, v32 ix) {
    if (bb->inst[ix.id].op == op_lane  ) return (v16){push(bb,op_load_16,           .ptr=src.ix)};
    if (bb->inst[ix.id].op == op_imm_32) return (v16){push(bb,op_uni_16,   .x=ix.id,.ptr=src.ix)};
    return                                      (v16){push(bb,op_gather_16,.x=ix.id,.ptr=src.ix)};
}
v32 umbra_load_32(BB *bb, umbra_ptr src, v32 ix) {
    if (bb->inst[ix.id].op == op_lane  ) return (v32){push(bb,op_load_32,           .ptr=src.ix)};
    if (bb->inst[ix.id].op == op_imm_32) return (v32){push(bb,op_uni_32,   .x=ix.id,.ptr=src.ix)};
    return                                      (v32){push(bb,op_gather_32,.x=ix.id,.ptr=src.ix)};
}
vh umbra_load_half(BB *bb, umbra_ptr src, v32 ix) {
    if (bb->inst[ix.id].op == op_lane  ) return (vh){push(bb,op_load_half,           .ptr=src.ix)};
    if (bb->inst[ix.id].op == op_imm_32) return (vh){push(bb,op_uni_half,   .x=ix.id,.ptr=src.ix)};
    return                                      (vh){push(bb,op_gather_half,.x=ix.id,.ptr=src.ix)};
}

void umbra_store_16(BB *bb, umbra_ptr dst, v32 ix, v16 val) {
    if (bb->inst[ix.id].op == op_lane) push(bb, op_store_16,             .y=val.id, .ptr=dst.ix);
    else                               push(bb, op_scatter_16, .x=ix.id, .y=val.id, .ptr=dst.ix);
}
void umbra_store_32(BB *bb, umbra_ptr dst, v32 ix, v32 val) {
    if (bb->inst[ix.id].op == op_lane) push(bb, op_store_32,             .y=val.id, .ptr=dst.ix);
    else                               push(bb, op_scatter_32, .x=ix.id, .y=val.id, .ptr=dst.ix);
}
void umbra_store_half(BB *bb, umbra_ptr dst, v32 ix, vh val) {
    if (bb->inst[ix.id].op == op_lane) push(bb, op_store_half,             .y=val.id, .ptr=dst.ix);
    else                               push(bb, op_scatter_half, .x=ix.id, .y=val.id, .ptr=dst.ix);
}

static _Bool is_imm16(BB *bb, int id, int val) {
    return bb->inst[id].op  == op_imm_16
        && bb->inst[id].imm == val;
}
static _Bool is_imm32(BB *bb, int id, int val) {
    return bb->inst[id].op  == op_imm_32
        && bb->inst[id].imm == val;
}
static _Bool is_imm_half(BB *bb, int id, int val) {
    return bb->inst[id].op  == op_imm_half
        && bb->inst[id].imm == val;
}

static _Bool is_imm(BB *bb, int id) {
    return bb->inst[id].op == op_imm_16
        || bb->inst[id].op == op_imm_32
        || bb->inst[id].op == op_imm_half;
}

static int math_(BB *bb, struct bb_inst inst) {
    if (is_imm(bb, inst.x) && is_imm(bb, inst.y) && is_imm(bb, inst.z)) {
        int const result = const_eval(inst.op, bb->inst[inst.x].imm
                                             , bb->inst[inst.y].imm
                                             , bb->inst[inst.z].imm);
        return op_type(inst.op) == OP_HALF ? umbra_imm_half(bb, (uint16_t)result).id
             : op_type(inst.op) == OP_16   ? umbra_imm_16  (bb, (uint16_t)result).id
             :                               umbra_imm_32  (bb, (uint32_t)result).id;
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

vh umbra_add_half(BB *bb, vh a, vh b) {
    sort(&a.id, &b.id);
    int const x = a.id,
              y = b.id;
    if (bb->inst[x].op == op_mul_half) {
        return (vh){math(bb, op_fma_half, .x=bb->inst[x].x, .y=bb->inst[x].y, .z=y)};
    }
    if (bb->inst[y].op == op_mul_half) {
        return (vh){math(bb, op_fma_half, .x=bb->inst[y].x, .y=bb->inst[y].y, .z=x)};
    }
    return (vh){math(bb, op_add_half, .x=x, .y=y)};
}

vh umbra_sub_half(BB *bb, vh a, vh b) {
    if (is_imm_half(bb, b.id, 0)) { return a; }
    return (vh){math(bb, op_sub_half, .x=a.id, .y=b.id)};
}

vh umbra_mul_half(BB *bb, vh a, vh b) {
    sort(&a.id, &b.id);
    if (is_imm_half(bb, a.id, 0x3c00)) { return b; }
    if (is_imm_half(bb, b.id, 0x3c00)) { return a; }
    return (vh){math(bb, op_mul_half, .x=a.id, .y=b.id)};
}

vh umbra_div_half(BB *bb, vh a, vh b) {
    if (is_imm_half(bb, b.id, 0x3c00)) { return a; }
    return (vh){math(bb, op_div_half, .x=a.id, .y=b.id)};
}

vh umbra_min_half(BB *bb, vh a, vh b) {
    sort(&a.id, &b.id);
    return (vh){math(bb, op_min_half, .x=a.id, .y=b.id)};
}

vh umbra_max_half(BB *bb, vh a, vh b) {
    sort(&a.id, &b.id);
    return (vh){math(bb, op_max_half, .x=a.id, .y=b.id)};
}

vh umbra_sqrt_half(BB *bb, vh a) {
    return (vh){math(bb, op_sqrt_half, .x=a.id)};
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
    if (is_imm(bb, b.id)) return (v32){push(bb, op_shl_i32_imm, .x=a.id, .imm=bb->inst[b.id].imm)};
    return (v32){math(bb, op_shl_i32, .x=a.id, .y=b.id)};
}
v32 umbra_shr_u32(BB *bb, v32 a, v32 b) {
    if (is_imm32(bb, b.id, 0)) { return a; }
    if (is_imm(bb, b.id)) return (v32){push(bb, op_shr_u32_imm, .x=a.id, .imm=bb->inst[b.id].imm)};
    return (v32){math(bb, op_shr_u32, .x=a.id, .y=b.id)};
}
v32 umbra_shr_s32(BB *bb, v32 a, v32 b) {
    if (is_imm32(bb, b.id, 0)) { return a; }
    if (is_imm(bb, b.id)) return (v32){push(bb, op_shr_s32_imm, .x=a.id, .imm=bb->inst[b.id].imm)};
    return (v32){math(bb, op_shr_s32, .x=a.id, .y=b.id)};
}

v16 umbra_shl_i16(BB *bb, v16 a, v16 b) {
    if (is_imm16(bb, b.id, 0)) { return a; }
    if (is_imm(bb, b.id)) return (v16){push(bb, op_shl_i16_imm, .x=a.id, .imm=bb->inst[b.id].imm)};
    return (v16){math(bb, op_shl_i16, .x=a.id, .y=b.id)};
}
v16 umbra_shr_u16(BB *bb, v16 a, v16 b) {
    if (is_imm16(bb, b.id, 0)) { return a; }
    if (is_imm(bb, b.id)) return (v16){push(bb, op_shr_u16_imm, .x=a.id, .imm=bb->inst[b.id].imm)};
    return (v16){math(bb, op_shr_u16, .x=a.id, .y=b.id)};
}
v16 umbra_shr_s16(BB *bb, v16 a, v16 b) {
    if (is_imm16(bb, b.id, 0)) { return a; }
    if (is_imm(bb, b.id)) return (v16){push(bb, op_shr_s16_imm, .x=a.id, .imm=bb->inst[b.id].imm)};
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
    if (is_imm16(bb, a.id, 0xffff)) { return b; }
    if (is_imm16(bb, b.id, 0xffff)) { return a; }
    if (is_imm16(bb, a.id,      0)) { return a; }
    if (is_imm16(bb, b.id,      0)) { return b; }
    return (v16){math(bb, op_and_16, .x=a.id, .y=b.id)};
}
v16 umbra_or_16(BB *bb, v16 a, v16 b) {
    sort(&a.id, &b.id);
    if (is_imm16(bb, a.id,      0)) { return b; }
    if (is_imm16(bb, b.id,      0)) { return a; }
    if (is_imm16(bb, a.id, 0xffff)) { return a; }
    if (is_imm16(bb, b.id, 0xffff)) { return b; }
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
    if (is_imm16(bb, c.id, 0xffff)) { return t; }
    if (is_imm16(bb, c.id,      0)) { return f; }
    return (v16){math(bb, op_sel_16, .x=c.id, .y=t.id, .z=f.id)};
}

vh umbra_and_half(BB *bb, vh a, vh b) {
    sort(&a.id, &b.id);
    if (is_imm_half(bb, a.id, 0xffff)) { return b; }
    if (is_imm_half(bb, b.id, 0xffff)) { return a; }
    if (is_imm_half(bb, a.id,      0)) { return a; }
    if (is_imm_half(bb, b.id,      0)) { return b; }
    return (vh){math(bb, op_and_half, .x=a.id, .y=b.id)};
}
vh umbra_or_half(BB *bb, vh a, vh b) {
    sort(&a.id, &b.id);
    if (is_imm_half(bb, a.id,      0)) { return b; }
    if (is_imm_half(bb, b.id,      0)) { return a; }
    if (is_imm_half(bb, a.id, 0xffff)) { return a; }
    if (is_imm_half(bb, b.id, 0xffff)) { return b; }
    return (vh){math(bb, op_or_half, .x=a.id, .y=b.id)};
}
vh umbra_xor_half(BB *bb, vh a, vh b) {
    sort(&a.id, &b.id);
    if (is_imm_half(bb, a.id, 0)) { return b; }
    if (is_imm_half(bb, b.id, 0)) { return a; }
    if (a.id == b.id) { return umbra_imm_half(bb, 0); }
    return (vh){math(bb, op_xor_half, .x=a.id, .y=b.id)};
}
vh umbra_sel_half(BB *bb, vh c, vh t, vh f) {
    if (t.id == f.id) { return t; }
    if (is_imm_half(bb, c.id, 0xffff)) { return t; }
    if (is_imm_half(bb, c.id,      0)) { return f; }
    return (vh){math(bb, op_sel_half, .x=c.id, .y=t.id, .z=f.id)};
}

vh umbra_half_from_f32(BB *bb, v32 a) {
    return (vh){math(bb, op_half_from_f32, .x=a.id)};
}
v32 umbra_f32_from_half(BB *bb, vh a) {
    return (v32){math(bb, op_f32_from_half, .x=a.id)};
}
v32 umbra_f32_from_i32(BB *bb, v32 a) {
    return (v32){math(bb, op_f32_from_i32, .x=a.id)};
}
v32 umbra_i32_from_f32(BB *bb, v32 a) {
    return (v32){math(bb, op_i32_from_f32, .x=a.id)};
}
vh umbra_half_from_i32(BB *bb, v32 a) {
    return (vh){math(bb, op_half_from_i32, .x=a.id)};
}
vh umbra_half_from_i16(BB *bb, v16 a) {
    return (vh){math(bb, op_half_from_i16, .x=a.id)};
}
v32 umbra_i32_from_half(BB *bb, vh a) {
    return (v32){math(bb, op_i32_from_half, .x=a.id)};
}
v16 umbra_i16_from_half(BB *bb, vh a) {
    return (v16){math(bb, op_i16_from_half, .x=a.id)};
}
v16 umbra_i16_from_i32(BB *bb, v32 a) {
    return (v16){math(bb, op_i16_from_i32, .x=a.id)};
}
v32 umbra_i32_from_i16(BB *bb, v16 a) {
    return (v32){math(bb, op_i32_from_i16, .x=a.id)};
}

vh umbra_eq_half(BB *bb, vh a, vh b) {
    sort(&a.id, &b.id);
    return (vh){math(bb, op_eq_half, .x=a.id, .y=b.id)};
}
vh umbra_ne_half(BB *bb, vh a, vh b) {
    sort(&a.id, &b.id);
    return (vh){math(bb, op_ne_half, .x=a.id, .y=b.id)};
}
vh umbra_lt_half(BB *bb, vh a, vh b) { return (vh){math(bb, op_lt_half, .x=a.id, .y=b.id)}; }
vh umbra_le_half(BB *bb, vh a, vh b) { return (vh){math(bb, op_le_half, .x=a.id, .y=b.id)}; }
vh umbra_gt_half(BB *bb, vh a, vh b) { return (vh){math(bb, op_gt_half, .x=a.id, .y=b.id)}; }
vh umbra_ge_half(BB *bb, vh a, vh b) { return (vh){math(bb, op_ge_half, .x=a.id, .y=b.id)}; }

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

static char const* op_name(enum op op) {
    switch (op) {
        case op_lane:       return "lane";
        case op_imm_32:     return "imm_32";
        case op_imm_16:     return "imm_16";
        case op_imm_half:   return "imm_half";
        case op_uni_32:     return "uni_32";
        case op_load_32:    return "load_32";
        case op_gather_32:  return "gather_32";
        case op_store_32:   return "store_32";
        case op_scatter_32: return "scatter_32";
        case op_uni_16:     return "uni_16";
        case op_load_16:    return "load_16";
        case op_gather_16:  return "gather_16";
        case op_store_16:   return "store_16";
        case op_scatter_16: return "scatter_16";
        case op_uni_half:     return "uni_half";
        case op_load_half:    return "load_half";
        case op_gather_half:  return "gather_half";
        case op_store_half:   return "store_half";
        case op_scatter_half: return "scatter_half";
        case op_add_f32:    return "add_f32";
        case op_sub_f32:    return "sub_f32";
        case op_mul_f32:    return "mul_f32";
        case op_div_f32:    return "div_f32";
        case op_min_f32:    return "min_f32";
        case op_max_f32:    return "max_f32";
        case op_sqrt_f32:   return "sqrt_f32";
        case op_fma_f32:    return "fma_f32";
        case op_add_i32:    return "add_i32";
        case op_sub_i32:    return "sub_i32";
        case op_mul_i32:    return "mul_i32";
        case op_shl_i32:     return "shl_i32";
        case op_shr_u32:     return "shr_u32";
        case op_shr_s32:     return "shr_s32";
        case op_shl_i32_imm: return "shl_i32_imm";
        case op_shr_u32_imm: return "shr_u32_imm";
        case op_shr_s32_imm: return "shr_s32_imm";
        case op_and_32:     return "and_32";
        case op_or_32:      return "or_32";
        case op_xor_32:     return "xor_32";
        case op_sel_32:     return "sel_32";
        case op_f32_from_i32:  return "f32_from_i32";
        case op_i32_from_f32:  return "i32_from_f32";
        case op_f32_from_half: return "f32_from_half";
        case op_i32_from_half: return "i32_from_half";
        case op_i32_from_i16: return "i32_from_i16";
        case op_half_from_f32: return "half_from_f32";
        case op_half_from_i32: return "half_from_i32";
        case op_half_from_i16: return "half_from_i16";
        case op_i16_from_half: return "i16_from_half";
        case op_eq_f32:     return "eq_f32";
        case op_ne_f32:     return "ne_f32";
        case op_lt_f32:     return "lt_f32";
        case op_le_f32:     return "le_f32";
        case op_gt_f32:     return "gt_f32";
        case op_ge_f32:     return "ge_f32";
        case op_eq_i32:     return "eq_i32";
        case op_ne_i32:     return "ne_i32";
        case op_lt_s32:     return "lt_s32";
        case op_le_s32:     return "le_s32";
        case op_gt_s32:     return "gt_s32";
        case op_ge_s32:     return "ge_s32";
        case op_lt_u32:     return "lt_u32";
        case op_le_u32:     return "le_u32";
        case op_gt_u32:     return "gt_u32";
        case op_ge_u32:     return "ge_u32";
        case op_add_i16:    return "add_i16";
        case op_sub_i16:    return "sub_i16";
        case op_mul_i16:    return "mul_i16";
        case op_shl_i16:     return "shl_i16";
        case op_shr_u16:     return "shr_u16";
        case op_shr_s16:     return "shr_s16";
        case op_shl_i16_imm: return "shl_i16_imm";
        case op_shr_u16_imm: return "shr_u16_imm";
        case op_shr_s16_imm: return "shr_s16_imm";
        case op_and_16:     return "and_16";
        case op_or_16:      return "or_16";
        case op_xor_16:     return "xor_16";
        case op_sel_16:     return "sel_16";
        case op_i16_from_i32: return "i16_from_i32";
        case op_eq_i16:     return "eq_i16";
        case op_ne_i16:     return "ne_i16";
        case op_lt_s16:     return "lt_s16";
        case op_le_s16:     return "le_s16";
        case op_gt_s16:     return "gt_s16";
        case op_ge_s16:     return "ge_s16";
        case op_lt_u16:     return "lt_u16";
        case op_le_u16:     return "le_u16";
        case op_gt_u16:     return "gt_u16";
        case op_ge_u16:     return "ge_u16";
        case op_add_half:   return "add_half";
        case op_sub_half:   return "sub_half";
        case op_mul_half:   return "mul_half";
        case op_div_half:   return "div_half";
        case op_min_half:   return "min_half";
        case op_max_half:   return "max_half";
        case op_sqrt_half:  return "sqrt_half";
        case op_fma_half:   return "fma_half";
        case op_and_half:   return "and_half";
        case op_or_half:    return "or_half";
        case op_xor_half:   return "xor_half";
        case op_sel_half:   return "sel_half";
        case op_eq_half:    return "eq_half";
        case op_ne_half:    return "ne_half";
        case op_lt_half:    return "lt_half";
        case op_le_half:    return "le_half";
        case op_gt_half:    return "gt_half";
        case op_ge_half:    return "ge_half";
    }
    return "?";
}

static void schedule(int id, struct bb_inst const *in, struct bb_inst *out,
                     int *old_to_new, int *j) {
    if (old_to_new[id] >= 0) { return; }

    // Schedule inputs first, in ascending original order for readability.
    int inputs[] = {in[id].x, in[id].y, in[id].z};
    if (inputs[0] > inputs[1]) { int t=inputs[0]; inputs[0]=inputs[1]; inputs[1]=t; }
    if (inputs[1] > inputs[2]) { int t=inputs[1]; inputs[1]=inputs[2]; inputs[2]=t; }
    if (inputs[0] > inputs[1]) { int t=inputs[0]; inputs[0]=inputs[1]; inputs[1]=t; }
    for (int k = 0; k < 3; k++) {
        schedule(inputs[k], in, out, old_to_new, j);
    }

    old_to_new[id] = *j;
    out[(*j)++] = in[id];
}

void umbra_basic_block_optimize(BB *bb) {
    int const n = bb->insts;

    _Bool *live    = calloc((size_t)n, 1);
    _Bool *varying = calloc((size_t)n, 1);

    for (int i = n; i --> 0;) {
        if (is_store(bb->inst[i].op)) { live[i] = 1; }
        if (live[i]) {
            live[bb->inst[i].x] = 1;
            live[bb->inst[i].y] = 1;
            live[bb->inst[i].z] = 1;
        }
    }
    for (int i = 0; i < n; i++) {
        varying[i] = is_varying(bb->inst[i].op)
                    |   varying[bb->inst[i].x]
                    |   varying[bb->inst[i].y]
                    |   varying[bb->inst[i].z];
    }

    int total = 0;
    for (int i = 0; i < n; i++) { total += live[i]; }

    struct bb_inst *out = malloc((size_t)total * sizeof *out);
    int *old_to_new = malloc((size_t)n * sizeof *old_to_new);
    for (int i = 0; i < n; i++) { old_to_new[i] = -1; }

    // Emit uniform (non-varying, non-store) instructions first.
    int j = 0;
    for (int i = 0; i < n; i++) {
        if (!live[i] || varying[i] || is_store(bb->inst[i].op)) { continue; }
        old_to_new[i] = j;
        out[j++] = bb->inst[i];
    }
    int const preamble = j;

    // Late-schedule varying instructions: walk stores in order,
    // recursively pulling each input in just before it's needed.
    for (int i = 0; i < n; i++) {
        if (live[i] && is_store(bb->inst[i].op)) {
            schedule(i, bb->inst, out, old_to_new, &j);
        }
    }

    for (int i = 0; i < total; i++) {
        out[i].x = old_to_new[out[i].x];
        out[i].y = old_to_new[out[i].y];
        out[i].z = old_to_new[out[i].z];
    }

    free(bb->inst);
    free(bb->ht);
    bb->inst     = out;
    bb->ht       = NULL;
    bb->ht_mask  = 0;
    bb->insts    = total;
    bb->preamble = preamble;

    free(live);
    free(varying);
    free(old_to_new);
}

void umbra_basic_block_dump(struct umbra_basic_block const *bb, FILE *f) {
    for (int i = 0; i < bb->insts; i++) {
        struct bb_inst const *inst = &bb->inst[i];
        enum op op = inst->op;

        if (is_store(op)) {
            fprintf(f, "      %-15s p%d", op_name(op), inst->ptr);
            if (op == op_scatter_16 || op == op_scatter_32 || op == op_scatter_half) {
                fprintf(f, " v%d", inst->x);
            }
            fprintf(f, " v%d\n", inst->y);
            continue;
        }

        fprintf(f, "  v%-3d = %-15s", i, op_name(op));

        switch (op) {
            case op_imm_32:   fprintf(f, " 0x%x",  (uint32_t)inst->imm); break;
            case op_imm_16:   fprintf(f, " 0x%x",  (uint16_t)inst->imm); break;
            case op_imm_half: fprintf(f, " 0x%x",  (uint16_t)inst->imm); break;
            case op_uni_32: case op_uni_16: case op_uni_half:
            case op_gather_32: case op_gather_16: case op_gather_half:
                fprintf(f, " p%d v%d", inst->ptr, inst->x);
                break;
            case op_load_32: case op_load_16: case op_load_half:
                fprintf(f, " p%d", inst->ptr);
                break;
            case op_shl_i32_imm: case op_shr_u32_imm: case op_shr_s32_imm:
            case op_shl_i16_imm: case op_shr_u16_imm: case op_shr_s16_imm:
                fprintf(f, " v%d %d", inst->x, inst->imm);
                break;
            case op_lane: break;
            default: {
                fprintf(f, " v%d", inst->x);
                if (inst->y) fprintf(f, " v%d", inst->y);
                if (inst->z) fprintf(f, " v%d", inst->z);
            } break;
        }
        fprintf(f, "\n");
    }
}
