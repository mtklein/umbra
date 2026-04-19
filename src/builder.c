#include "../include/umbra.h"
#include "assume.h"
#include "flat_ir.h"
#include <stdint.h>
#include <stdlib.h>

typedef struct umbra_builder builder;

static val val_make(int id, int chan) { return (val){.id = id, .chan = (unsigned)chan}; }

static _Bool is_pow2(int x) { return __builtin_popcount((unsigned)x) == 1; }

static uint32_t mul_overflow(uint32_t x, uint32_t y) {
    __builtin_mul_overflow(x, y, &x);
    return x;
}

static uint32_t ir_inst_hash(struct ir_inst const *inst) {
    uint32_t h = (uint32_t)inst->op;
    h = mul_overflow(0x9e3779b9, h ^ (uint32_t)inst->x.bits);
    h = mul_overflow(0x9e3779b9, h ^ (uint32_t)inst->y.bits);
    h = mul_overflow(0x9e3779b9, h ^ (uint32_t)inst->z.bits);
    h = mul_overflow(0x9e3779b9, h ^ (uint32_t)inst->w.bits);
    h = mul_overflow(0x9e3779b9, h ^ (uint32_t)inst->ptr.bits);
    h = mul_overflow(0x9e3779b9, h ^ (uint32_t)inst->imm);
    h ^= h >> 16;
    return h ? h : 1;
}

struct dedup_ctx {
    struct ir_inst const *probe;
    struct ir_inst const *insts;
    int                   hit, pad;
};
static _Bool dedup_match(int id, void *ctx) {
    struct dedup_ctx *c = ctx;
    if (__builtin_memcmp(c->probe, &c->insts[id], sizeof *c->probe) == 0) {
        c->hit = id;
        return 1;
    }
    return 0;
}

static val push_(builder *b, struct ir_inst inst) {
    {
        enum op const op = inst.op;
        if (op == op_imm_32 || op == op_uniform_32) {
            inst.uniform = 1;
        } else if (op == op_load_var) {
            inst.uniform = b->var_uniform[inst.imm];
        } else if (op == op_store_var) {
            inst.uniform = 0;
            b->var_uniform[inst.imm] = b->inst[inst.y.id].uniform;
        } else if (op_is_varying(op)
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

    uint32_t const h = ir_inst_hash(&inst);

    struct dedup_ctx ctx = {.probe = &inst, .insts = b->inst};
    if (hash_lookup(b->ht, h, dedup_match, &ctx)) {
        return (val){.id = ctx.hit};
    }

    if (b->insts == b->cap) {
        b->cap *= 2;
        b->inst = realloc(b->inst, (size_t)b->cap * sizeof *b->inst);
    }

    int const id = b->insts++;
    b->inst[id] = inst;
    if (!op_is_store(inst.op) && inst.op != op_load_var
            && inst.op != op_if_begin && inst.op != op_if_end) {
        hash_insert(&b->ht, h, id);
    }
    return (val){.id = id};
}
#define push(b, ...) push_(b, (struct ir_inst){.op = __VA_ARGS__})
#define push32(b, ...) push(b, __VA_ARGS__).v32
#define push16(b, ...) push(b, __VA_ARGS__).v16
#define VX(v) .x = (val){.id = (v).id, .chan = (v).chan}
#define VY(v) .y = (val){.id = (v).id, .chan = (v).chan}
#define VZ(v) .z = (val){.id = (v).id, .chan = (v).chan}
#define VW(v) .w = (val){.id = (v).id, .chan = (v).chan}

builder* umbra_builder(void) {
    builder *b = calloc(1, sizeof *b);
    b->cap = 128;
    b->inst = malloc((size_t)b->cap * sizeof *b->inst);
    b->ht = (struct hash){.slots = 128, .data = calloc(128, sizeof(unsigned) + sizeof(int))};
    // Simplifies liveness analysis to know id 0 is imm=0.
    push(b, op_imm_32, .imm = 0);
    return b;
}

void umbra_builder_free(builder *b) {
    if (b) {
        free(b->inst);
        free(b->ht.data);
        free(b->var_uniform);
        free(b->uniforms);
        free(b);
    }
}

static int reserve_uniform(builder *b) {
    if (b->n_uniforms == b->cap_uniforms) {
        b->cap_uniforms = b->cap_uniforms ? 2 * b->cap_uniforms : 4;
        b->uniforms = realloc(b->uniforms,
                              (size_t)b->cap_uniforms * sizeof *b->uniforms);
    }
    return b->n_uniforms;
}

umbra_ptr32 umbra_bind_uniforms32(builder *b, void const *slot, int slots) {
    assume(((uintptr_t)slot & 3u) == 0);
    assume(slots >= 0);
    int const ix = reserve_uniform(b);
    b->uniforms[b->n_uniforms++] = (struct umbra_uniform_reg){
        .buf     = NULL,
        .storage = {.ptr = (void*)(uintptr_t)slot, .count = slots, .stride = 0},
        .ix      = ix,
    };
    return (umbra_ptr32){.ix = ix};
}

umbra_ptr32 umbra_bind_buf32(builder *b, struct umbra_buf const *buf) {
    int const ix = reserve_uniform(b);
    b->uniforms[b->n_uniforms++] = (struct umbra_uniform_reg){.buf = buf, .ix = ix};
    return (umbra_ptr32){.ix = ix};
}
umbra_ptr16 umbra_bind_buf16(builder *b, struct umbra_buf const *buf) {
    int const ix = reserve_uniform(b);
    b->uniforms[b->n_uniforms++] = (struct umbra_uniform_reg){.buf = buf, .ix = ix};
    return (umbra_ptr16){.ix = ix};
}
umbra_ptr64 umbra_bind_buf64(builder *b, struct umbra_buf const *buf) {
    int const ix = reserve_uniform(b);
    b->uniforms[b->n_uniforms++] = (struct umbra_uniform_reg){.buf = buf, .ix = ix};
    return (umbra_ptr64){.ix = ix};
}

umbra_val32 umbra_x(builder *b) { return push32(b, op_x); }
umbra_val32 umbra_y(builder *b) { return push32(b, op_y); }

umbra_val32 umbra_imm_i32(builder *b, int bits) { return push32(b, op_imm_32, .imm = bits); }
umbra_val32 umbra_imm_f32(builder *b, float v) {
    union {
        float f;
        int   i;
    } const u = {.f = v};
    return umbra_imm_i32(b, u.i);
}

umbra_val16 umbra_gather_16(builder *b, umbra_ptr16 src, umbra_val32 ix) {
    return push16(b, op_gather_16, VX(ix), .ptr = {.p16 = src});
}
umbra_val32 umbra_load_32(builder *b, umbra_ptr32 src) {
    return push32(b, op_load_32, .ptr = {.p32 = src});
}
umbra_val16 umbra_load_16(builder *b, umbra_ptr16 src) {
    return push16(b, op_load_16, .ptr = {.p16 = src});
}
umbra_val32 umbra_uniform_32(builder *b, umbra_ptr32 src, int slot) {
    return push32(b, op_uniform_32, .imm = slot, .ptr = {.p32 = src});
}
umbra_val32 umbra_gather_32(builder *b, umbra_ptr32 src, umbra_val32 ix) {
    enum op const op = b->inst[ix.id].uniform ? op_gather_uniform_32 : op_gather_32;
    return push32(b, op, VX(ix), .ptr = {.p32 = src});
}
void umbra_store_32(builder *b, umbra_ptr32 dst, umbra_val32 v) {
    push(b, op_store_32, VY(v), .ptr = {.p32 = dst});
}
void umbra_store_16(builder *b, umbra_ptr16 dst, umbra_val16 v) {
    push(b, op_store_16, VY(v), .ptr = {.p16 = dst});
}
static void x4_32(val px, umbra_val32 *r, umbra_val32 *g, umbra_val32 *b, umbra_val32 *a) {
    int const id = px.id;
    *r = val_make(id, 0).v32; *g = val_make(id, 1).v32;
    *b = val_make(id, 2).v32; *a = val_make(id, 3).v32;
}
static void x4_16(val px, umbra_val16 *r, umbra_val16 *g, umbra_val16 *b, umbra_val16 *a) {
    int const id = px.id;
    *r = val_make(id, 0).v16; *g = val_make(id, 1).v16;
    *b = val_make(id, 2).v16; *a = val_make(id, 3).v16;
}
void umbra_load_8x4(builder *b, umbra_ptr32 src,
                    umbra_val32 *r, umbra_val32 *g, umbra_val32 *bl, umbra_val32 *a) {
    x4_32(push(b, op_load_8x4, .ptr = {.p32 = src}), r, g, bl, a);
}
void umbra_store_8x4(builder *b, umbra_ptr32 dst,
                     umbra_val32 r, umbra_val32 g, umbra_val32 bl, umbra_val32 a) {
    push(b, op_store_8x4, VX(r), VY(g), VZ(bl), VW(a), .ptr = {.p32 = dst});
}
void umbra_load_16x4(builder *b, umbra_ptr64 src,
                     umbra_val16 *r, umbra_val16 *g, umbra_val16 *bl, umbra_val16 *a) {
    x4_16(push(b, op_load_16x4, .ptr = {.p64 = src}), r, g, bl, a);
}
void umbra_store_16x4(builder *b, umbra_ptr64 dst,
                      umbra_val16 r, umbra_val16 g, umbra_val16 bl, umbra_val16 a) {
    push(b, op_store_16x4, VX(r), VY(g), VZ(bl), VW(a), .ptr = {.p64 = dst});
}
void umbra_load_16x4_planar(builder *b, umbra_ptr16 src,
                            umbra_val16 *r, umbra_val16 *g, umbra_val16 *bl, umbra_val16 *a) {
    x4_16(push(b, op_load_16x4_planar, .ptr = {.p16 = src}), r, g, bl, a);
}
void umbra_store_16x4_planar(builder *b, umbra_ptr16 dst,
                             umbra_val16 r, umbra_val16 g, umbra_val16 bl, umbra_val16 a) {
    push(b, op_store_16x4_planar, VX(r), VY(g), VZ(bl), VW(a), .ptr = {.p16 = dst});
}

umbra_val32 umbra_i32_from_s16(builder *b, umbra_val16 x) {
    return push32(b, op_i32_from_s16, VX(x));
}
umbra_val32 umbra_i32_from_u16(builder *b, umbra_val16 x) {
    return push32(b, op_i32_from_u16, VX(x));
}
umbra_val16 umbra_i16_from_i32(builder *b, umbra_val32 x) {
    return push16(b, op_i16_from_i32, VX(x));
}

umbra_val32 umbra_f32_from_f16(builder *b, umbra_val16 x) {
    return push32(b, op_f32_from_f16, VX(x));
}
umbra_val16 umbra_f16_from_f32(builder *b, umbra_val32 x) {
    return push16(b, op_f16_from_f32, VX(x));
}

static _Bool is_imm32(builder *b, int id, int v) {
    return b->inst[id].op == op_imm_32 && b->inst[id].imm == v;
}

static _Bool is_imm(builder *b, int id) { return b->inst[id].op == op_imm_32; }

static val math_(builder *b, struct ir_inst inst) {
    if (is_imm(b, inst.x.id) && is_imm(b, inst.y.id) && is_imm(b, inst.z.id)) {
        int const result = op_eval(inst.op, b->inst[inst.x.id].imm,
                                            b->inst[inst.y.id].imm,
                                            b->inst[inst.z.id].imm,
                                            b->inst[inst.w.id].imm);
        return (val){.v32 = umbra_imm_i32(b, result)};
    }
    return push_(b, inst);
}
#define math(b, ...) math_(b, (struct ir_inst){.op = __VA_ARGS__})

static val try_join_imm(builder *b, val d, enum op fused, val x, val y) {
    if (is_imm(b, d.id)) { return d; }
    int const imm_id = is_imm(b, x.id) ? x.id : is_imm(b, y.id) ? y.id : -1;
    if (imm_id >= 0) {
        val const other = imm_id == x.id ? y : x;
        val const f = push(b, fused, VX(other), .imm = b->inst[imm_id].imm);
        return push(b, op_join, .x = d, .y = f);
    }
    return d;
}

static val join_imm_y(builder *b, val d, enum op fused, val x, val y) {
    if (!is_imm(b, d.id) && is_imm(b, y.id)) {
        val const f = push(b, fused, VX(x), .imm = b->inst[y.id].imm);
        return push(b, op_join, .x = d, .y = f);
    }
    return d;
}

static void sort(umbra_val32 *a, umbra_val32 *b) {
    if ((val){.v32 = *b}.bits < (val){.v32 = *a}.bits) {
        umbra_val32 const t = *a;
        *a = *b;
        *b = t;
    }
}

umbra_val32 umbra_add_f32(builder *b, umbra_val32 x, umbra_val32 y) {
    sort(&x, &y);
    if (is_imm32(b, x.id, 0)) { return y; }
    if (b->inst[x.id].op == op_mul_f32) {
        return math(b, op_fma_f32, .x = b->inst[x.id].x, .y = b->inst[x.id].y, VZ(y)).v32;
    }
    if (b->inst[y.id].op == op_mul_f32) {
        return math(b, op_fma_f32, .x = b->inst[y.id].x, .y = b->inst[y.id].y, VZ(x)).v32;
    }
    if (b->inst[x.id].op == op_square_f32) {
        return math(b, op_square_add_f32, .x = b->inst[x.id].x, VY(y)).v32;
    }
    if (b->inst[y.id].op == op_square_f32) {
        return math(b, op_square_add_f32, .x = b->inst[y.id].x, VY(x)).v32;
    }
    return try_join_imm(b, math(b, op_add_f32, VX(x), VY(y)), op_add_f32_imm, (val){.v32 = x},
                        (val){.v32 = y}).v32;
}

umbra_val32 umbra_sub_f32(builder *b, umbra_val32 x, umbra_val32 y) {
    if (is_imm32(b, y.id, 0)) { return x; }
    if (b->inst[y.id].op == op_mul_f32) {
        return math(b, op_fms_f32, .x = b->inst[y.id].x, .y = b->inst[y.id].y, VZ(x)).v32;
    }
    if (b->inst[y.id].op == op_square_f32) {
        return math(b, op_square_sub_f32, .x = b->inst[y.id].x, VY(x)).v32;
    }
    return join_imm_y(b, math(b, op_sub_f32, VX(x), VY(y)), op_sub_f32_imm,
                      (val){.v32 = x}, (val){.v32 = y}).v32;
}

umbra_val32 umbra_mul_f32(builder *b, umbra_val32 x, umbra_val32 y) {
    sort(&x, &y);
    if (is_imm32(b, x.id, 0x3f800000)) { return y; }
    if (is_imm32(b, y.id, 0x3f800000)) { return x; }
    if ((val){.v32 = x}.bits == (val){.v32 = y}.bits) {
        return math(b, op_square_f32, VX(x)).v32;
    }
    return try_join_imm(b, math(b, op_mul_f32, VX(x), VY(y)), op_mul_f32_imm, (val){.v32 = x},
                   (val){.v32 = y}).v32;
}

umbra_val32 umbra_div_f32(builder *b, umbra_val32 x, umbra_val32 y) {
    if (is_imm32(b, y.id, 0x3f800000)) { return x; }
    return join_imm_y(b, math(b, op_div_f32, VX(x), VY(y)), op_div_f32_imm,
                      (val){.v32 = x}, (val){.v32 = y}).v32;
}

umbra_val32 umbra_min_f32(builder *b, umbra_val32 x, umbra_val32 y) {
    sort(&x, &y);
    // min(v, v) = v — legal across the entire f32 domain: min(NaN, NaN) = NaN,
    // min(±inf, ±inf) = ±inf, min(finite, finite) = v.
    if ((val){.v32 = x}.bits == (val){.v32 = y}.bits) { return x; }
    return try_join_imm(b, math(b, op_min_f32, VX(x), VY(y)), op_min_f32_imm, (val){.v32 = x},
                   (val){.v32 = y}).v32;
}

umbra_val32 umbra_max_f32(builder *b, umbra_val32 x, umbra_val32 y) {
    sort(&x, &y);
    // max(v, v) = v — same reasoning as min_f32 above.
    if ((val){.v32 = x}.bits == (val){.v32 = y}.bits) { return x; }
    return try_join_imm(b, math(b, op_max_f32, VX(x), VY(y)), op_max_f32_imm, (val){.v32 = x},
                   (val){.v32 = y}).v32;
}

umbra_val32 umbra_sqrt_f32(builder *b, umbra_val32 x) { return math(b, op_sqrt_f32, VX(x)).v32; }
umbra_val32 umbra_abs_f32(builder *b, umbra_val32 x) { return math(b, op_abs_f32, VX(x)).v32; }
umbra_val32 umbra_round_f32(builder *b, umbra_val32 x) { return math(b, op_round_f32, VX(x)).v32; }
umbra_val32 umbra_floor_f32(builder *b, umbra_val32 x) { return math(b, op_floor_f32, VX(x)).v32; }
umbra_val32 umbra_ceil_f32(builder *b, umbra_val32 x) { return math(b, op_ceil_f32, VX(x)).v32; }
umbra_val32 umbra_round_i32(builder *b, umbra_val32 x) { return math(b, op_round_i32, VX(x)).v32; }
umbra_val32 umbra_floor_i32(builder *b, umbra_val32 x) { return math(b, op_floor_i32, VX(x)).v32; }
umbra_val32 umbra_ceil_i32(builder *b, umbra_val32 x) { return math(b, op_ceil_i32, VX(x)).v32; }
umbra_val32 umbra_add_i32(builder *b, umbra_val32 x, umbra_val32 y) {
    sort(&x, &y);
    if (is_imm32(b, x.id, 0)) { return y; }
    return try_join_imm(b, math(b, op_add_i32, VX(x), VY(y)), op_add_i32_imm, (val){.v32 = x},
                   (val){.v32 = y}).v32;
}

umbra_val32 umbra_sub_i32(builder *b, umbra_val32 x, umbra_val32 y) {
    if (is_imm32(b, y.id, 0)) { return x; }
    if (x.id == y.id) { return umbra_imm_i32(b, 0); }
    return join_imm_y(b, math(b, op_sub_i32, VX(x), VY(y)), op_sub_i32_imm,
                      (val){.v32 = x}, (val){.v32 = y}).v32;
}

umbra_val32 umbra_mul_i32(builder *b, umbra_val32 x, umbra_val32 y) {
    sort(&x, &y);
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
    return try_join_imm(b, math(b, op_mul_i32, VX(x), VY(y)), op_mul_i32_imm, (val){.v32 = x},
                   (val){.v32 = y}).v32;
}

umbra_val32 umbra_shl_i32(builder *b, umbra_val32 x, umbra_val32 y) {
    if (is_imm32(b, y.id, 0)) { return x; }
    return join_imm_y(b, math(b, op_shl_i32, VX(x), VY(y)), op_shl_i32_imm,
                      (val){.v32 = x}, (val){.v32 = y}).v32;
}
umbra_val32 umbra_shr_u32(builder *b, umbra_val32 x, umbra_val32 y) {
    if (is_imm32(b, y.id, 0)) { return x; }
    return join_imm_y(b, math(b, op_shr_u32, VX(x), VY(y)), op_shr_u32_imm,
                      (val){.v32 = x}, (val){.v32 = y}).v32;
}
umbra_val32 umbra_shr_s32(builder *b, umbra_val32 x, umbra_val32 y) {
    if (is_imm32(b, y.id, 0)) { return x; }
    return join_imm_y(b, math(b, op_shr_s32, VX(x), VY(y)), op_shr_s32_imm,
                      (val){.v32 = x}, (val){.v32 = y}).v32;
}

umbra_val32 umbra_and_32(builder *b, umbra_val32 x, umbra_val32 y) {
    sort(&x, &y);
    if (x.id == y.id) { return x; }
    if (is_imm32(b, x.id, -1)) { return y; }
    if (is_imm32(b, y.id, -1)) { return x; }
    if (is_imm32(b, x.id, 0)) { return x; }
    if (is_imm32(b, x.id, 0x7fffffff)) { return umbra_abs_f32(b, y); }
    if (is_imm32(b, y.id, 0x7fffffff)) { return umbra_abs_f32(b, x); }
    return try_join_imm(b, math(b, op_and_32, VX(x), VY(y)), op_and_32_imm, (val){.v32 = x},
                        (val){.v32 = y}).v32;
}
umbra_val32 umbra_or_32(builder *b, umbra_val32 x, umbra_val32 y) {
    sort(&x, &y);
    if (x.id == y.id) { return x; }
    if (is_imm32(b, x.id, 0)) { return y; }
    if (is_imm32(b, x.id, -1)) { return x; }
    if (is_imm32(b, y.id, -1)) { return y; }
    return try_join_imm(b, math(b, op_or_32, VX(x), VY(y)), op_or_32_imm, (val){.v32 = x},
                   (val){.v32 = y}).v32;
}
umbra_val32 umbra_xor_32(builder *b, umbra_val32 x, umbra_val32 y) {
    sort(&x, &y);
    if (x.id == y.id) { return umbra_imm_i32(b, 0); }
    if (is_imm32(b, x.id, 0)) { return y; }
    return try_join_imm(b, math(b, op_xor_32, VX(x), VY(y)), op_xor_32_imm, (val){.v32 = x},
                   (val){.v32 = y}).v32;
}
umbra_val32 umbra_sel_32(builder *b, umbra_val32 c, umbra_val32 t, umbra_val32 fv) {
    if (t.id == fv.id) { return t; }
    if (is_imm32(b, c.id, -1)) { return t; }
    if (is_imm32(b, c.id, 0)) { return fv; }
    return math(b, op_sel_32, VX(c), VY(t), VZ(fv)).v32;
}

umbra_val32 umbra_f32_from_i32(builder *b, umbra_val32 x) {
    return math(b, op_f32_from_i32, VX(x)).v32;
}
umbra_val32 umbra_i32_from_f32(builder *b, umbra_val32 x) {
    return math(b, op_i32_from_f32, VX(x)).v32;
}

umbra_val32 umbra_eq_f32(builder *b, umbra_val32 x, umbra_val32 y) {
    sort(&x, &y);
    return try_join_imm(b, math(b, op_eq_f32, VX(x), VY(y)), op_eq_f32_imm, (val){.v32 = x},
                   (val){.v32 = y}).v32;
}
umbra_val32 umbra_lt_f32(builder *b, umbra_val32 x, umbra_val32 y) {
    // lt(v, v) = 0 — legal across the entire f32 domain: IEEE `<` is false
    // whenever either operand is NaN, and x < x is false for every non-NaN x,
    // so 0 (all-bits-clear false mask) covers both.  Note le_f32(v, v) is
    // NOT similarly foldable: NaN ≤ NaN is false but finite x ≤ x is true,
    // so no single constant covers both cases.
    if ((val){.v32 = x}.bits == (val){.v32 = y}.bits) { return umbra_imm_i32(b, 0); }
    return join_imm_y(b, math(b, op_lt_f32, VX(x), VY(y)), op_lt_f32_imm,
                      (val){.v32 = x}, (val){.v32 = y}).v32;
}
umbra_val32 umbra_le_f32(builder *b, umbra_val32 x, umbra_val32 y) {
    return join_imm_y(b, math(b, op_le_f32, VX(x), VY(y)), op_le_f32_imm,
                      (val){.v32 = x}, (val){.v32 = y}).v32;
}
umbra_val32 umbra_eq_i32(builder *b, umbra_val32 x, umbra_val32 y) {
    sort(&x, &y);
    if (x.id == y.id) { return umbra_imm_i32(b, -1); }
    return try_join_imm(b, math(b, op_eq_i32, VX(x), VY(y)), op_eq_i32_imm, (val){.v32 = x},
                   (val){.v32 = y}).v32;
}
umbra_val32 umbra_lt_s32(builder *b, umbra_val32 x, umbra_val32 y) {
    return join_imm_y(b, math(b, op_lt_s32, VX(x), VY(y)), op_lt_s32_imm,
                      (val){.v32 = x}, (val){.v32 = y}).v32;
}
umbra_val32 umbra_le_s32(builder *b, umbra_val32 x, umbra_val32 y) {
    return join_imm_y(b, math(b, op_le_s32, VX(x), VY(y)), op_le_s32_imm,
                      (val){.v32 = x}, (val){.v32 = y}).v32;
}
umbra_val32 umbra_lt_u32(builder *b, umbra_val32 x, umbra_val32 y) {
    return math(b, op_lt_u32, VX(x), VY(y)).v32;
}
umbra_val32 umbra_le_u32(builder *b, umbra_val32 x, umbra_val32 y) {
    return math(b, op_le_u32, VX(x), VY(y)).v32;
}

umbra_val32 umbra_loop(builder *b, umbra_val32 n) {
    assume(!b->has_loop);
    assume(b->inst[n.id].uniform);
    b->has_loop   = 1;
    b->loop_trip  = n;
    b->loop_var   = umbra_declare_var32(b);
    push(b, op_loop_begin, VX(n), .imm = b->loop_var.id);
    return umbra_load_var32(b, b->loop_var);
}

void umbra_end_loop(builder *b) {
    assume(b->has_loop && !b->loop_closed);
    b->loop_closed = 1;
    umbra_val32 cur  = umbra_load_var32(b, b->loop_var);
    umbra_val32 next = umbra_add_i32(b, cur, umbra_imm_i32(b, 1));
    umbra_store_var32(b, b->loop_var, next);
    push(b, op_loop_end, VX(b->loop_trip), .imm = b->loop_var.id);
}

umbra_var32 umbra_declare_var32(builder *b) {
    int const id = b->n_vars++;
    b->var_uniform = realloc(b->var_uniform, (size_t)b->n_vars * sizeof *b->var_uniform);
    b->var_uniform[id] = 1;
    return (umbra_var32){.id = id};
}

umbra_val32 umbra_load_var32(builder *b, umbra_var32 v) {
    return push32(b, op_load_var, .imm = v.id);
}

void umbra_store_var32(builder *b, umbra_var32 var, umbra_val32 x) {
    push(b, op_store_var, VY(x), .imm = var.id);
}

void umbra_if(builder *b, umbra_val32 cond) {
    push(b, op_if_begin, VX(cond));
    b->if_depth++;
}

void umbra_end_if(builder *b) {
    assume(b->if_depth > 0);
    b->if_depth--;
    push(b, op_if_end);
}

void umbra_backend_free(struct umbra_backend *b) {
    if (b) { b->free(b); }
}
void umbra_program_free(struct umbra_program *p) {
    if (p) { p->free(p); }
}
