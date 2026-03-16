#include "../umbra.h"
#include "bb.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct umbra_basic_block BB;
typedef umbra_val val;

static _Bool is_pow2(int x) {
    return __builtin_popcount((unsigned)x) == 1;
}
static _Bool is_pow2_or_zero(int x) {
    return __builtin_popcount((unsigned)x) <= 1;
}

static uint32_t bb_inst_hash(struct bb_inst const *inst) {
    uint32_t               h = (uint32_t)inst->op;
    __builtin_mul_overflow(h ^ (uint32_t)inst->x  ,
                           0x9e3779b9u, &h);
    __builtin_mul_overflow(h ^ (uint32_t)inst->y  ,
                           0x9e3779b9u, &h);
    __builtin_mul_overflow(h ^ (uint32_t)inst->z  ,
                           0x9e3779b9u, &h);
    __builtin_mul_overflow(h ^ (uint32_t)inst->w  ,
                           0x9e3779b9u, &h);
    __builtin_mul_overflow(h ^ (uint32_t)inst->ptr,
                           0x9e3779b9u, &h);
    __builtin_mul_overflow(h ^ (uint32_t)inst->imm,
                           0x9e3779b9u, &h);
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
            0 == __builtin_memcmp(
                     &inst,
                     &bb->inst[bb->ht[slot & bb->ht_mask].ix],
                     sizeof inst)) {
            return bb->ht[slot & bb->ht_mask].ix;
        }
    }

    if (is_pow2_or_zero(bb->insts)) {
        int const inst_cap = bb->insts ? 2*bb->insts : 1,
                    ht_cap = 2*inst_cap;
        bb->inst = realloc(bb->inst,
                           (size_t)inst_cap * sizeof *bb->inst);

        struct hash_slot *old     = bb->ht;
        int const         old_cap = bb->ht
                                  ? bb->ht_mask + 1
                                  : 0;
        bb->ht      = calloc((size_t)ht_cap, sizeof *bb->ht);
        bb->ht_mask = ht_cap - 1;
        for (int i = 0; i < old_cap; i++) {
            for (int slot = (int)old[i].hash;
                 old[i].hash;
                 slot++) {
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
                bb->ht[slot & bb->ht_mask] =
                    (struct hash_slot){h, id};
                break;
            }
        }
    }
    return id;
}
#define push(bb,...) push_(bb, (struct bb_inst){.op=__VA_ARGS__})


BB* umbra_basic_block(void) {
    BB *bb = calloc(1, sizeof *bb);
    // Simplifies liveness analysis to know id 0 is imm=0.
    push(bb, op_imm_32, .imm=0);
    return bb;
}

void umbra_basic_block_free(BB *bb) {
    free(bb->inst);
    free(bb->ht);
    free(bb);
}

val umbra_lane(BB *bb) {
    return (val){push(bb, op_lane)};
}

val umbra_imm(BB *bb, int bits) {
    return (val){push(bb, op_imm_32, .imm=bits)};
}

int umbra_reserve(BB *bb, int n) {
    bb->uni_len = (bb->uni_len + 3) & ~3;
    int ix = bb->uni_len / 4;
    bb->uni_len += n * 4;
    return ix;
}
int umbra_reserve_ptr(BB *bb) {
    bb->uni_len = (bb->uni_len + 7) & ~7;
    int off = bb->uni_len;
    bb->uni_len += 16;
    return off;
}
umbra_ptr umbra_deref_ptr(BB *bb, umbra_ptr buf,
                          int byte_off) {
    int id = push(bb, op_deref_ptr,
                  .ptr=buf.ix, .imm=byte_off);
    return (umbra_ptr){~id};
}
int umbra_uni_len(BB const *bb) {
    return bb->uni_len;
}
void umbra_set_uni_len(BB *bb, int len) {
    bb->uni_len = len;
}

static int lane_plus_off(BB *bb, int ix) {
    if (bb->inst[ix].op != op_add_i32) { return -1; }
    int a = bb->inst[ix].x, b = bb->inst[ix].y;
    if (bb->inst[a].op == op_lane) { return b; }
    if (bb->inst[b].op == op_lane) { return a; }
    return -1;
}

val umbra_load16(BB *bb, umbra_ptr src, val ix) {
    if (bb->inst[ix.id].op == op_lane) {
        return (val){push(bb, op_load_16,
                         .ptr=src.ix)};
    }
    if (bb->inst[ix.id].op == op_imm_32) {
        return (val){push(bb, op_uni_16,
                         .imm=bb->inst[ix.id].imm,
                         .ptr=src.ix)};
    }
    {
        int off = lane_plus_off(bb, ix.id);
        if (off >= 0) {
            return (val){push(bb, op_load_16,
                             .x=off, .ptr=src.ix)};
        }
    }
    return (val){push(bb, op_gather_16,
                      .x=ix.id, .ptr=src.ix)};
}
val umbra_load32(BB *bb, umbra_ptr src, val ix) {
    if (bb->inst[ix.id].op == op_lane) {
        return (val){push(bb, op_load_32,
                         .ptr=src.ix)};
    }
    if (bb->inst[ix.id].op == op_imm_32) {
        return (val){push(bb, op_uni_32,
                         .imm=bb->inst[ix.id].imm,
                         .ptr=src.ix)};
    }
    {
        int off = lane_plus_off(bb, ix.id);
        if (off >= 0) {
            return (val){push(bb, op_load_32,
                             .x=off, .ptr=src.ix)};
        }
    }
    return (val){push(bb, op_gather_32,
                      .x=ix.id, .ptr=src.ix)};
}
val umbra_load_f16(BB *bb, umbra_ptr src, val ix) {
    return umbra_htof(bb, umbra_load16(bb, src, ix));
}

void umbra_store16(BB *bb, umbra_ptr dst,
                   val ix, val v) {
    if (bb->inst[ix.id].op == op_lane) {
        push(bb, op_store_16,
             .y=v.id, .ptr=dst.ix);
        return;
    }
    {
        int off = lane_plus_off(bb, ix.id);
        if (off >= 0) {
            push(bb, op_store_16,
                 .x=off, .y=v.id, .ptr=dst.ix);
            return;
        }
    }
    push(bb, op_scatter_16,
         .x=ix.id, .y=v.id, .ptr=dst.ix);
}
void umbra_store32(BB *bb, umbra_ptr dst,
                   val ix, val v) {
    if (bb->inst[ix.id].op == op_lane) {
        push(bb, op_store_32,
             .y=v.id, .ptr=dst.ix);
        return;
    }
    {
        int off = lane_plus_off(bb, ix.id);
        if (off >= 0) {
            push(bb, op_store_32,
                 .x=off, .y=v.id, .ptr=dst.ix);
            return;
        }
    }
    push(bb, op_scatter_32,
         .x=ix.id, .y=v.id, .ptr=dst.ix);
}
void umbra_store_f16(BB *bb, umbra_ptr dst,
                     val ix, val v) {
    umbra_store16(bb, dst, ix, umbra_ftoh(bb, v));
}

val umbra_htof(BB *bb, val a) {
    return (val){push(bb, op_htof, .x=a.id)};
}
val umbra_ftoh(BB *bb, val a) {
    return (val){push(bb, op_ftoh, .x=a.id)};
}

static _Bool is_imm32(BB *bb, int id, int v) {
    return bb->inst[id].op  == op_imm_32
        && bb->inst[id].imm == v;
}

static _Bool is_imm(BB *bb, int id) {
    return bb->inst[id].op == op_imm_32;
}

static int math_(BB *bb, struct bb_inst inst) {
    if (is_imm(bb, inst.x)
     && is_imm(bb, inst.y)
     && is_imm(bb, inst.z)) {
        int const result = umbra_const_eval(
            inst.op,
            bb->inst[inst.x].imm,
            bb->inst[inst.y].imm,
            bb->inst[inst.z].imm);
        return umbra_imm(bb, result).id;
    }
    return push_(bb, inst);
}
#define math(bb,...) \
    math_(bb, (struct bb_inst){.op=__VA_ARGS__})

static void sort(int *a, int *b) {
    if (*a > *b) {
        int const t = *a;
        *a = *b;
        *b = t;
    }
}

val umbra_fadd(BB *bb, val a, val b) {
    sort(&a.id, &b.id);
    int const x = a.id,
              y = b.id;
    if (bb->inst[x].op == op_mul_f32) {
        return (val){math(bb, op_fma_f32,
            .x=bb->inst[x].x,
            .y=bb->inst[x].y, .z=y)};
    }
    if (bb->inst[y].op == op_mul_f32) {
        return (val){math(bb, op_fma_f32,
            .x=bb->inst[y].x,
            .y=bb->inst[y].y, .z=x)};
    }
    return (val){math(bb, op_add_f32, .x=x, .y=y)};
}

val umbra_fsub(BB *bb, val a, val b) {
    if (is_imm32(bb, b.id, 0)) { return a; }
    if (bb->inst[b.id].op == op_mul_f32) {
        return (val){math(bb, op_fms_f32,
            .x=bb->inst[b.id].x,
            .y=bb->inst[b.id].y, .z=a.id)};
    }
    return (val){math(bb, op_sub_f32,
                      .x=a.id, .y=b.id)};
}

val umbra_fmul(BB *bb, val a, val b) {
    sort(&a.id, &b.id);
    if (is_imm32(bb, a.id, 0x3f800000)) { return b; }
    if (is_imm32(bb, b.id, 0x3f800000)) { return a; }
    return (val){math(bb, op_mul_f32,
                      .x=a.id, .y=b.id)};
}

val umbra_fdiv(BB *bb, val a, val b) {
    if (is_imm32(bb, b.id, 0x3f800000)) { return a; }
    return (val){math(bb, op_div_f32,
                      .x=a.id, .y=b.id)};
}

val umbra_fmin(BB *bb, val a, val b) {
    sort(&a.id, &b.id);
    return (val){math(bb, op_min_f32,
                      .x=a.id, .y=b.id)};
}

val umbra_fmax(BB *bb, val a, val b) {
    sort(&a.id, &b.id);
    return (val){math(bb, op_max_f32,
                      .x=a.id, .y=b.id)};
}

val umbra_sqrt(BB *bb, val a) {
    return (val){math(bb, op_sqrt_f32, .x=a.id)};
}

val umbra_iadd(BB *bb, val a, val b) {
    sort(&a.id, &b.id);
    if (is_imm32(bb, a.id, 0)) { return b; }
    if (is_imm32(bb, b.id, 0)) { return a; }
    return (val){math(bb, op_add_i32,
                      .x=a.id, .y=b.id)};
}

val umbra_isub(BB *bb, val a, val b) {
    if (is_imm32(bb, b.id, 0)) { return a; }
    if (a.id == b.id) { return umbra_imm(bb, 0); }
    return (val){math(bb, op_sub_i32,
                      .x=a.id, .y=b.id)};
}

val umbra_imul(BB *bb, val a, val b) {
    sort(&a.id, &b.id);
    if (is_imm32(bb, a.id, 1)) { return b; }
    if (is_imm32(bb, b.id, 1)) { return a; }
    if (is_imm32(bb, a.id, 0)) { return a; }
    if (is_imm32(bb, b.id, 0)) { return b; }
    if (bb->inst[a.id].op == op_imm_32
     && is_pow2(bb->inst[a.id].imm)) {
        int const shift =
            __builtin_ctz((unsigned)bb->inst[a.id].imm);
        return umbra_ishl(bb, b,
                          umbra_imm(bb, shift));
    }
    if (bb->inst[b.id].op == op_imm_32
     && is_pow2(bb->inst[b.id].imm)) {
        int const shift =
            __builtin_ctz((unsigned)bb->inst[b.id].imm);
        return umbra_ishl(bb, a,
                          umbra_imm(bb, shift));
    }
    return (val){math(bb, op_mul_i32,
                      .x=a.id, .y=b.id)};
}

val umbra_ishl(BB *bb, val a, val b) {
    if (is_imm32(bb, b.id, 0)) { return a; }
    if (is_imm(bb, b.id)) {
        return (val){push(bb, op_shl_i32_imm,
                         .x=a.id,
                         .imm=bb->inst[b.id].imm)};
    }
    return (val){math(bb, op_shl_i32,
                      .x=a.id, .y=b.id)};
}
val umbra_ushr(BB *bb, val a, val b) {
    if (is_imm32(bb, b.id, 0)) { return a; }
    if (is_imm(bb, b.id)) {
        return (val){push(bb, op_shr_u32_imm,
                         .x=a.id,
                         .imm=bb->inst[b.id].imm)};
    }
    return (val){math(bb, op_shr_u32,
                      .x=a.id, .y=b.id)};
}
val umbra_sshr(BB *bb, val a, val b) {
    if (is_imm32(bb, b.id, 0)) { return a; }
    if (is_imm(bb, b.id)) {
        return (val){push(bb, op_shr_s32_imm,
                         .x=a.id,
                         .imm=bb->inst[b.id].imm)};
    }
    return (val){math(bb, op_shr_s32,
                      .x=a.id, .y=b.id)};
}

val umbra_and(BB *bb, val a, val b) {
    sort(&a.id, &b.id);
    if (a.id == b.id)              { return a; }
    if (is_imm32(bb, a.id, -1))    { return b; }
    if (is_imm32(bb, b.id, -1))    { return a; }
    if (is_imm32(bb, a.id,  0))    { return a; }
    if (is_imm32(bb, b.id,  0))    { return b; }
    return (val){math(bb, op_and_32,
                      .x=a.id, .y=b.id)};
}
val umbra_or(BB *bb, val a, val b) {
    sort(&a.id, &b.id);
    if (a.id == b.id)              { return a; }
    if (is_imm32(bb, a.id,  0))    { return b; }
    if (is_imm32(bb, b.id,  0))    { return a; }
    if (is_imm32(bb, a.id, -1))    { return a; }
    if (is_imm32(bb, b.id, -1))    { return b; }
    return (val){math(bb, op_or_32,
                      .x=a.id, .y=b.id)};
}
val umbra_xor(BB *bb, val a, val b) {
    sort(&a.id, &b.id);
    if (is_imm32(bb, a.id, 0))    { return b; }
    if (is_imm32(bb, b.id, 0))    { return a; }
    if (a.id == b.id) { return umbra_imm(bb, 0); }
    return (val){math(bb, op_xor_32,
                      .x=a.id, .y=b.id)};
}
val umbra_sel(BB *bb, val c, val t, val fv) {
    if (t.id == fv.id)             { return t; }
    if (is_imm32(bb, c.id, -1))    { return t; }
    if (is_imm32(bb, c.id,  0))    { return fv; }
    return (val){math(bb, op_sel_32,
                      .x=c.id, .y=t.id, .z=fv.id)};
}

val umbra_itof(BB *bb, val a) {
    return (val){math(bb, op_f32_from_i32, .x=a.id)};
}
val umbra_ftoi(BB *bb, val a) {
    return (val){math(bb, op_i32_from_f32, .x=a.id)};
}

void umbra_load8x4(BB *bb, umbra_ptr src,
                   val ix, val out[4]) {
    (void)ix;
    int base = push(bb, op_load_8x4, .ptr=src.ix);
    for (int ch = 1; ch <= 3; ch++) {
        push(bb, op_load_8x4, .x=base, .imm=ch);
    }
    for (int ch = 0; ch < 4; ch++) {
        out[ch] = (val){base + ch};
    }
}

void umbra_store8x4(BB *bb, umbra_ptr dst,
                    val ix, val in[4]) {
    (void)ix;
    push(bb, op_store_8x4,
         .x=in[0].id, .y=in[1].id,
         .z=in[2].id, .w=in[3].id, .ptr=dst.ix);
}

val umbra_feq(BB *bb, val a, val b) {
    sort(&a.id, &b.id);
    return (val){math(bb, op_eq_f32,
                      .x=a.id, .y=b.id)};
}
val umbra_fne(BB *bb, val a, val b) {
    return umbra_xor(bb, umbra_feq(bb, a, b),
                     umbra_imm(bb, -1));
}
val umbra_flt(BB *bb, val a, val b) {
    return (val){math(bb, op_lt_f32,
                      .x=a.id, .y=b.id)};
}
val umbra_fle(BB *bb, val a, val b) {
    return (val){math(bb, op_le_f32,
                      .x=a.id, .y=b.id)};
}
val umbra_fgt(BB *bb, val a, val b) {
    return umbra_flt(bb, b, a);
}
val umbra_fge(BB *bb, val a, val b) {
    return umbra_fle(bb, b, a);
}

val umbra_ieq(BB *bb, val a, val b) {
    sort(&a.id, &b.id);
    if (a.id == b.id) { return umbra_imm(bb, -1); }
    return (val){math(bb, op_eq_i32,
                      .x=a.id, .y=b.id)};
}
val umbra_ine(BB *bb, val a, val b) {
    return umbra_xor(bb, umbra_ieq(bb, a, b),
                     umbra_imm(bb, -1));
}

val umbra_slt(BB *bb, val a, val b) {
    return (val){math(bb, op_lt_s32,
                      .x=a.id, .y=b.id)};
}
val umbra_sle(BB *bb, val a, val b) {
    return (val){math(bb, op_le_s32,
                      .x=a.id, .y=b.id)};
}
val umbra_sgt(BB *bb, val a, val b) {
    return umbra_slt(bb, b, a);
}
val umbra_sge(BB *bb, val a, val b) {
    return umbra_sle(bb, b, a);
}

val umbra_ult(BB *bb, val a, val b) {
    return (val){math(bb, op_lt_u32,
                      .x=a.id, .y=b.id)};
}
val umbra_ule(BB *bb, val a, val b) {
    return (val){math(bb, op_le_u32,
                      .x=a.id, .y=b.id)};
}
val umbra_ugt(BB *bb, val a, val b) {
    return umbra_ult(bb, b, a);
}
val umbra_uge(BB *bb, val a, val b) {
    return umbra_ule(bb, b, a);
}

static char const* op_name(enum op op) {
    static char const *names[] = {
        #define OP_NAME(name) [op_##name] = #name,
        OP_LIST(OP_NAME)
        #undef OP_NAME
    };
    if ((unsigned)op < sizeof names/sizeof *names
     && names[op]) {
        return names[op];
    }
    return "?";
}

static void schedule(int id,
                     struct bb_inst const *in,
                     struct bb_inst *out,
                     int *old_to_new, int *j) {
    if (old_to_new[id] >= 0) { return; }

    int inputs[] = {
        in[id].x, in[id].y, in[id].z, in[id].w,
    };
    for (int a = 0; a < 3; a++) {
        for (int b = a+1; b < 4; b++) {
            if (inputs[a] > inputs[b]) {
                int t = inputs[a];
                inputs[a] = inputs[b];
                inputs[b] = t;
            }
        }
    }
    for (int k = 0; k < 4; k++) {
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
            live[bb->inst[i].w] = 1;
            if (bb->inst[i].ptr < 0) {
                live[~bb->inst[i].ptr] = 1;
            }
        }
    }
    for (int i = 0; i < n; i++) {
        varying[i] = is_varying(bb->inst[i].op)
                   | varying[bb->inst[i].x]
                   | varying[bb->inst[i].y]
                   | varying[bb->inst[i].z]
                   | varying[bb->inst[i].w];
    }

    int total = 0;
    for (int i = 0; i < n; i++) { total += live[i]; }

    struct bb_inst *out =
        malloc((size_t)total * sizeof *out);
    int *old_to_new =
        malloc((size_t)n * sizeof *old_to_new);
    for (int i = 0; i < n; i++) {
        old_to_new[i] = -1;
    }

    int j = 0;
    for (int i = 0; i < n; i++) {
        if (!live[i] || varying[i]
         || is_store(bb->inst[i].op)) {
            continue;
        }
        old_to_new[i] = j;
        out[j++] = bb->inst[i];
    }
    int const preamble = j;

    for (int i = 0; i < n; i++) {
        if (live[i] && is_store(bb->inst[i].op)) {
            schedule(i, bb->inst, out,
                     old_to_new, &j);
        }
    }

    for (int i = 0; i < total; i++) {
        out[i].x = old_to_new[out[i].x];
        out[i].y = old_to_new[out[i].y];
        out[i].z = old_to_new[out[i].z];
        out[i].w = old_to_new[out[i].w];
        if (out[i].ptr < 0) {
            out[i].ptr = ~old_to_new[~out[i].ptr];
        }
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

void umbra_basic_block_dump(BB const *bb, FILE *f) {
    for (int i = 0; i < bb->insts; i++) {
        struct bb_inst const *inst = &bb->inst[i];
        enum op op = inst->op;

        if (is_store(op)) {
            if (op == op_store_8x4) {
                fprintf(f,
                    "      %-15s p%d"
                    " v%d v%d v%d v%d\n",
                    op_name(op), inst->ptr,
                    inst->x, inst->y,
                    inst->z, inst->w);
            } else {
                fprintf(f, "      %-15s p%d",
                        op_name(op), inst->ptr);
                if (op == op_scatter_16
                 || op == op_scatter_32) {
                    fprintf(f, " v%d", inst->x);
                } else if (inst->x
                        && (op == op_store_16
                         || op == op_store_32)) {
                    fprintf(f, " +v%d", inst->x);
                }
                fprintf(f, " v%d\n", inst->y);
            }
            continue;
        }

        fprintf(f, "  v%-3d = %-15s", i,
                op_name(op));

        switch (op) {
            case op_imm_32:
                fprintf(f, " 0x%x",
                        (uint32_t)inst->imm);
                break;
            case op_uni_32:
            case op_uni_16:
                fprintf(f, " p%d[%d]",
                        inst->ptr, inst->imm);
                break;
            case op_gather_32:
            case op_gather_16:
                fprintf(f, " p%d v%d",
                        inst->ptr, inst->x);
                break;
            case op_load_32:
            case op_load_16:
                fprintf(f, " p%d", inst->ptr);
                if (inst->x) {
                    fprintf(f, " +v%d", inst->x);
                }
                break;
            case op_load_8x4:
                if (inst->x == 0) {
                    fprintf(f, " p%d", inst->ptr);
                } else {
                    fprintf(f, " v%d ch%d",
                            inst->x, inst->imm);
                }
                break;
            case op_deref_ptr:
                fprintf(f, " p%d byte%d",
                        inst->ptr, inst->imm);
                break;
            case op_shl_i32_imm:
            case op_shr_u32_imm:
            case op_shr_s32_imm:
                fprintf(f, " v%d %d",
                        inst->x, inst->imm);
                break;
            case op_lane: break;

            case op_store_16:
            case op_store_32:
            case op_scatter_16:
            case op_scatter_32:
            case op_store_8x4:
                break;

            case op_sqrt_f32:
            case op_f32_from_i32:
            case op_i32_from_f32:
            case op_htof:
            case op_ftoh:
                fprintf(f, " v%d", inst->x);
                break;

            case op_add_f32: case op_sub_f32:
            case op_mul_f32: case op_div_f32:
            case op_min_f32: case op_max_f32:
            case op_add_i32: case op_sub_i32:
            case op_mul_i32:
            case op_shl_i32: case op_shr_u32:
            case op_shr_s32:
            case op_and_32: case op_or_32:
            case op_xor_32:
            case op_eq_f32: case op_lt_f32:
            case op_le_f32:
            case op_eq_i32: case op_lt_s32:
            case op_le_s32:
            case op_lt_u32: case op_le_u32:
                fprintf(f, " v%d v%d",
                        inst->x, inst->y);
                break;

            case op_fma_f32: case op_fms_f32:
            case op_sel_32:
                fprintf(f, " v%d v%d v%d",
                        inst->x, inst->y, inst->z);
                break;
        }
        fprintf(f, "\n");
    }
}
