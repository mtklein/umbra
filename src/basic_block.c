#include "../include/umbra.h"
#include "assume.h"
#include "basic_block.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#pragma clang diagnostic ignored "-Wfloat-equal"

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
    int last_use, n_deps, n_users, user_off, ready_idx;
};

static _Bool is_body(struct bb_inst const *inst) {
    return inst->live && inst->varying;
}

static int sched_score(struct bb_inst const *in, struct sched const *meta, int c, int live) {
    int kills = 0;
    int const deps[] = {in[c].x.id, in[c].y.id, in[c].z.id, in[c].w.id};
    for (int k = 0; k < 4; k++) {
        int const d = deps[k];
        kills += meta[d].last_use == c;
    }
    int const defines = is_store(in[c].op) ? 0 : 1;
    int const last_use = meta[c].last_use < 0 ? live : meta[c].last_use;
    // Prefer instructions that decrease register pressure.  (kills-defines)
    // Break ties in favor of instructions that die soon.    (last_use)
    return (kills - defines)*live - last_use;
}

static void schedule(struct bb_inst *in, int n, struct bb_inst *out, int preamble, int live) {
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

    for (int i = 0; i < nready; i++) {
        meta[ready[i]].ready_idx = i;
    }

    int j = preamble;
    int prev_scheduled = -1;
    while (nready > 0) {
        int pick = -1;
        // Prefer chaining: pick any ready user of the previously scheduled instruction.
        if (prev_scheduled >= 0) {
            for (int u = meta[prev_scheduled].user_off;
                     u < meta[prev_scheduled].user_off + meta[prev_scheduled].n_users; u++) {
                if (meta[users[u]].n_deps == 0) {
                    pick = meta[users[u]].ready_idx;
                    break;
                }
            }
        }
        // No chain: pick the best-scoring ready instruction.
        if (pick < 0) {
            int best_score = sched_score(in, meta, ready[0], live);
            pick = 0;
            for (int r = 1; r < nready; r++) {
                int const s = sched_score(in, meta, ready[r], live);
                if (best_score < s) {
                    best_score = s;
                    pick = r;
                }
            }
        }
        assume(pick >= 0);

        // Remove ready[pick], maintaining ready_idx bookkeeping.
        int const id = ready[pick];
        {
            int const last = --nready;
            if (pick != last) {
                ready[pick] = ready[last];
                meta[ready[pick]].ready_idx = pick;
            }
        }

        in[id].final_id = j;
        out[j++] = in[id];
        prev_scheduled = id;

        for (int u = meta[id].user_off; u < meta[id].user_off + meta[id].n_users; u++) {
            if (--meta[users[u]].n_deps == 0) {
                meta[users[u]].ready_idx = nready;
                ready[nready++] = users[u];
            }
        }
    }

    free(meta);
    free(buf);
}

static int ptr_deref(int id) { return (int)((unsigned)id | (1u << 31)); }

struct umbra_basic_block* umbra_basic_block(struct umbra_builder *b) {
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
        out[i].x = (val){.id = b->inst[out[i].x.id].final_id, .chan = out[i].x.chan};
        out[i].y = (val){.id = b->inst[out[i].y.id].final_id, .chan = out[i].y.chan};
        out[i].z = (val){.id = b->inst[out[i].z.id].final_id, .chan = out[i].z.chan};
        out[i].w = (val){.id = b->inst[out[i].w.id].final_id, .chan = out[i].w.chan};
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

void umbra_builder_dump(struct umbra_builder const *b, FILE *f) {
    dump_insts(b->inst, b->insts, f);
}

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
