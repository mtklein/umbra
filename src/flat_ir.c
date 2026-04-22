#include "../include/umbra.h"
#include "assume.h"
#include "count.h"
#include "flat_ir.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

_Bool binding_is_uniform(enum binding_kind k) {
    return k == BIND_EARLY_UNIFORMS || k == BIND_LATE_UNIFORMS;
}

_Bool flat_ir_has_early_writes(struct umbra_flat_ir const *ir) {
    _Bool is_early_buf[32] = {0};
    for (int k = 0; k < ir->bindings; k++) {
        if (ir->binding[k].kind == BIND_EARLY_BUF) {
            int const ix = ir->binding[k].ix;
            assume(0 <= ix && ix < (int)count(is_early_buf));
            is_early_buf[ix] = 1;
        }
    }
    for (int i = 0; i < ir->insts; i++) {
        if (op_is_store(ir->inst[i].op)) {
            int const ix = ir->inst[i].ptr.bits;
            if (0 <= ix && ix < (int)count(is_early_buf) && is_early_buf[ix]) {
                return 1;
            }
        }
    }
    return 0;
}

void resolve_bindings(struct umbra_buf *out,
                      struct buffer_binding const *binding, int bindings,
                      struct umbra_late_binding const *late, int lates) {
    for (int i = 0; i < bindings; i++) {
        struct buffer_binding const *bb = &binding[i];
        if      (bb->kind == BIND_EARLY_BUF)      { out[bb->ix] = *bb->buf; }
        else if (bb->kind == BIND_EARLY_UNIFORMS) { out[bb->ix] =  bb->uniforms; }
        else                                      { out[bb->ix] = (struct umbra_buf){0}; }
    }
    for (int i = 0; i < lates; i++) {
        int const ix = late[i].ptr.ix;
        int bi = 0;
        while (bi < bindings && binding[bi].ix != ix) { bi++; }
        assume(bi < bindings);
        if (binding[bi].kind == BIND_LATE_BUF) {
            out[ix] = late[i].buf;
        } else {
            assume(binding[bi].kind == BIND_LATE_UNIFORMS);
            out[ix] = (struct umbra_buf){
                .ptr    = (void*)(uintptr_t)late[i].uniforms,
                .count  = binding[bi].uniforms.count,
                .stride = 0,
            };
        }
    }
}

struct sched {
    int last_use[4], // Per-channel: latest instruction that reads this value, or -1.
        n_deps,      // Unscheduled instructions this one still waits on.  Ready at 0.
        n_users,     // Instructions that read this one's values.  Decremented as scheduled.
        user_off,    // Start index into the users[] array for this instruction's user list.
        ready_idx;   // This instruction's position in the ready[] worklist.
};

static int sched_score(struct ir_inst const *in, struct sched const *meta,
                       int const *users, int c, int reg_pressure, int prev) {
    int score = 0;

    {
        val const deps[] = {in[c].x, in[c].y, in[c].z, in[c].w};
        int dead_regs = 0;
        for (int k = 0; k < 4; k++) {
            _Bool first = 1;
            for (int j = 0; j < k; j++) {
                if (deps[j].bits == deps[k].bits) { first = 0; }
            }
            if (first) {
                dead_regs += meta[deps[k].id].last_use[deps[k].chan] == c;
            }
        }

        int const born_regs = op_is_store(in[c].op) ? 0
                                                     : op_values(in[c].op);

        int const decrease_in_reg_pressure = dead_regs - born_regs;
        score += decrease_in_reg_pressure * reg_pressure * reg_pressure;
    }

    {
        int readied_ops = 0;
        for (int u = meta[c].user_off; u < meta[c].user_off + meta[c].n_users; u++) {
            readied_ops += meta[users[u]].n_deps == 1/*i.e. this op*/;
        }

        score += readied_ops * reg_pressure;
    }

    {
        int max_last_use = meta[c].last_use[0];
        for (int ch = 1; ch < op_values(in[c].op); ch++) {
            if (meta[c].last_use[ch] > max_last_use) {
                max_last_use = meta[c].last_use[ch];
            }
        }
        score -= max_last_use >= 0 ? max_last_use : reg_pressure;
    }

    {
        // A small tie-breaking bonus to preserving the original program order.
        int const chaining_bonus = prev >= 0 && (in[c].x.id == prev ||
                                                 in[c].y.id == prev ||
                                                 in[c].z.id == prev ||
                                                 in[c].w.id == prev);
        score += chaining_bonus;
    }

    return score;
}

static _Bool is_body(struct ir_inst const *inst) {
    return inst->live && inst->varying;
}

static void schedule(struct ir_inst *in, int n, struct ir_inst *out, int at, int live,
                     int region_lo, int region_hi) {
    struct sched *meta = calloc((size_t)(n + 1), sizeof *meta);

    for (int i = 0; i < n; i++) {
        for (int ch = 0; ch < 4; ch++) {
            meta[i].last_use[ch] = -1;
        }
        if (is_body(in + i) && i >= region_lo && i < region_hi) {
            val const deps[] = {in[i].x, in[i].y, in[i].z, in[i].w};
            for (int k = 0; k < 4; k++) {
                int const d = deps[k].id;
                meta[d].last_use[deps[k].chan] = i;
                if (is_body(in + d) && d >= region_lo && d < region_hi) {
                    meta[i].n_deps++;
                    meta[d].n_users++;
                }
            }
            if (in[i].op == op_store_var) {
                for (int j = region_lo; j < i; j++) {
                    if (in[j].op == op_load_var && in[j].imm == in[i].imm
                            && is_body(in + j)) {
                        meta[i].n_deps++;
                        meta[j].n_users++;
                        meta[j].last_use[0] = i;
                    }
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
        if (is_body(in + i) && i >= region_lo && i < region_hi) {
            int const deps[] = {in[i].x.id, in[i].y.id, in[i].z.id, in[i].w.id};
            for (int k = 0; k < 4; k++) {
                if (is_body(in + deps[k]) && deps[k] >= region_lo && deps[k] < region_hi) {
                    int const d = deps[k];
                    users[meta[d].user_off + meta[d].n_users++] = i;
                }
            }
            if (in[i].op == op_store_var) {
                for (int j = region_lo; j < i; j++) {
                    if (in[j].op == op_load_var && in[j].imm == in[i].imm
                            && is_body(in + j)) {
                        users[meta[j].user_off + meta[j].n_users++] = i;
                    }
                }
            }
        }
    }

    int nready = 0;
    for (int i = 0; i < n; i++) {
        if (is_body(in + i) && i >= region_lo && i < region_hi && meta[i].n_deps == 0) {
            ready[nready++] = i;
        }
    }
    for (int i = 0; i < nready; i++) {
        meta[ready[i]].ready_idx = i;
    }

    int j = at, prev = -1;
    while (nready > 0) {
        int best_score = sched_score(in, meta, users, ready[0], live, prev);
        int pick = 0;
        for (int r = 1; r < nready; r++) {
            int const s = sched_score(in, meta, users, ready[r], live, prev);
            if (s > best_score) {
                best_score = s;
                pick = r;
            }
        }

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
        prev = id;

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

static _Bool is_cf(enum op op) {
    return op == op_loop_begin || op == op_loop_end
        || op == op_if_begin   || op == op_if_end;
}

struct umbra_flat_ir* umbra_flat_ir(struct umbra_builder *b) {
    assume(!b->has_loop || b->loop_closed);
    assume(b->if_depth == 0);

    int const n = b->insts;

    int live = 0;
    for (int i = n; i-- > 0;) {
        if (op_is_store(b->inst[i].op) || is_cf(b->inst[i].op)) {
            b->inst[i].live = 1;
        }
        if (b->inst[i].live) {
            live++;
            b->inst[b->inst[i].x.id].live = 1;
            b->inst[b->inst[i].y.id].live = 1;
            b->inst[b->inst[i].z.id].live = 1;
            b->inst[b->inst[i].w.id].live = 1;
        }
    }

    for (int i = 0; i < n; i++) {
        b->inst[i].varying = op_is_varying(b->inst[i].op)
                          || b->inst[b->inst[i].x.id].varying
                          || b->inst[b->inst[i].y.id].varying
                          || b->inst[b->inst[i].z.id].varying
                          || b->inst[b->inst[i].w.id].varying;
    }
    {
        int depth = 0;
        for (int i = 0; i < n; i++) {
            if (b->inst[i].op == op_loop_begin || b->inst[i].op == op_if_begin) { depth++; }
            if (depth > 0) { b->inst[i].varying = 1; }
            if (b->inst[i].op == op_loop_end || b->inst[i].op == op_if_end) { depth--; }
        }
    }

    struct ir_inst *out = malloc((size_t)live * sizeof *out);
    int preamble = 0;
    for (int i = 0; i < n; i++) {
        if (b->inst[i].live && !b->inst[i].varying && !op_is_store(b->inst[i].op)) {
            b->inst[i].final_id = preamble;
            out[preamble++] = b->inst[i];
        }
    }

    int *cf = malloc((size_t)n * sizeof *cf);
    int n_cf = 0;
    for (int i = 0; i < n; i++) {
        if (b->inst[i].live && b->inst[i].varying && is_cf(b->inst[i].op)) {
            cf[n_cf++] = i;
        }
    }

    int j = preamble;
    int region_lo = 0;
    for (int ci = 0; ci < n_cf; ci++) {
        schedule(b->inst, n, out, j, live, region_lo, cf[ci]);
        for (int i = region_lo; i < cf[ci]; i++) {
            if (is_body(b->inst + i)) { j++; }
        }
        b->inst[cf[ci]].final_id = j;
        out[j++] = b->inst[cf[ci]];
        region_lo = cf[ci] + 1;
    }
    schedule(b->inst, n, out, j, live, region_lo, n);

    free(cf);

    for (int i = 0; i < live; i++) {
        out[i].x = (val){.id = b->inst[out[i].x.id].final_id, .chan = out[i].x.chan};
        out[i].y = (val){.id = b->inst[out[i].y.id].final_id, .chan = out[i].y.chan};
        out[i].z = (val){.id = b->inst[out[i].z.id].final_id, .chan = out[i].z.chan};
        out[i].w = (val){.id = b->inst[out[i].w.id].final_id, .chan = out[i].w.chan};
    }

    struct umbra_flat_ir *result = calloc(1, sizeof *result);
    result->inst     = out;
    result->insts    = live;
    result->preamble = preamble;
    result->vars   = b->vars;
    result->loop_begin = -1;
    result->loop_end   = -1;
    if (b->bindings) {
        size_t const sz = (size_t)b->bindings * sizeof *b->binding;
        result->binding  = malloc(sz);
        result->bindings = b->bindings;
        __builtin_memcpy(result->binding, b->binding, sz);
    }
    {
        int if_stack[16];
        int if_sp = 0;
        for (int i = 0; i < live; i++) {
            if (out[i].op == op_loop_begin) { result->loop_begin = i; }
            if (out[i].op == op_loop_end)   { result->loop_end   = i; }
            if (out[i].op == op_if_begin)   { if_stack[if_sp++] = i; }
            if (out[i].op == op_if_end) {
                int ib = if_stack[--if_sp];
                out[i].x = (val){.id = ib};
            }
        }
    }
    return result;
}

static val remap_val(val v, int const *remap) {
    return (val){.id = remap[v.id], .chan = v.chan};
}

struct umbra_flat_ir* flat_ir_resolve(struct umbra_flat_ir const *ir,
                                            enum join_policy jp) {
    int const n = ir->insts;
    struct ir_inst *inst = malloc((size_t)n * sizeof *inst);
    __builtin_memcpy(inst, ir->inst, (size_t)n * sizeof *inst);

    for (int i = 0; i < n; i++) {
        struct ir_inst *ip = inst + i;
        if (ip->op == op_join) {
            if (jp == JOIN_PREFER_IMM && op_is_fused_imm(inst[ip->y.id].op)) {
                ip->x = ip->y;
            }
            ip->y = (val){0};
        }
    }

    for (int i = 0; i < n; i++) {
        struct ir_inst *ip = inst + i;
        if (inst[ip->x.id].op == op_join) { ip->x.id = inst[ip->x.id].x.id; }
        if (inst[ip->y.id].op == op_join) { ip->y.id = inst[ip->y.id].x.id; }
        if (inst[ip->z.id].op == op_join) { ip->z.id = inst[ip->z.id].x.id; }
        if (inst[ip->w.id].op == op_join) { ip->w.id = inst[ip->w.id].x.id; }
        assume(inst[ip->x.id].op != op_join);
        assume(inst[ip->y.id].op != op_join);
        assume(inst[ip->z.id].op != op_join);
        assume(inst[ip->w.id].op != op_join);
    }

    _Bool *live = calloc((size_t)(n + 1), sizeof *live);
    for (int i = n; i-- > 0;) {
        struct ir_inst const *ip = &inst[i];
        if (op_is_store(ip->op) || ip->op == op_loop_begin || ip->op == op_loop_end
                                || ip->op == op_if_begin   || ip->op == op_if_end) {
            live[i] = 1;
        }
        if (live[i]) {
            live[ip->x.id] = 1;
            live[ip->y.id] = 1;
            live[ip->z.id] = 1;
            live[ip->w.id] = 1;
        }
    }

    int *remap = calloc((size_t)(n + 1), sizeof *remap);
    int live_count = 0,
        new_preamble = 0;
    for (int i = 0; i < n; i++) {
        if (i == ir->preamble) {
            new_preamble = live_count;
        }
        if (live[i]) {
            remap[i] = live_count++;
        }
    }

    struct ir_inst *out = malloc((size_t)live_count * sizeof *out);
    int j = 0;
    for (int i = 0; i < n; i++) {
        if (live[i]) {
            out[j]   = inst[i];
            out[j].x = remap_val(inst[i].x, remap);
            out[j].y = remap_val(inst[i].y, remap);
            out[j].z = remap_val(inst[i].z, remap);
            out[j].w = remap_val(inst[i].w, remap);
            j++;
        }
    }

    struct umbra_flat_ir *result = calloc(1, sizeof *result);
    result->inst       = out;
    result->insts      = live_count;
    result->vars     = ir->vars;
    result->preamble   = new_preamble;
    result->loop_begin = ir->loop_begin >= 0 ? remap[ir->loop_begin] : -1;
    result->loop_end   = ir->loop_end   >= 0 ? remap[ir->loop_end]   : -1;
    if (ir->bindings) {
        size_t const sz = (size_t)ir->bindings * sizeof *ir->binding;
        result->binding  = malloc(sz);
        result->bindings = ir->bindings;
        __builtin_memcpy(result->binding, ir->binding, sz);
    }

    free(remap);
    free(live);
    free(inst);
    return result;
}

void umbra_flat_ir_free(struct umbra_flat_ir *ir) {
    if (ir) {
        free(ir->inst);
        free(ir->binding);
        free(ir);
    }
}

static void dump_insts(struct ir_inst const *inst, int insts, FILE *f) {
    for (int i = 0; i < insts; i++) {
        struct ir_inst const *ip = &inst[i];
        enum op const         op = ip->op;

        if (op == op_loop_end) {
            fprintf(f, "      loop_end\n");
            continue;
        }
        if (op == op_if_end) {
            fprintf(f, "      if_end\n");
            continue;
        }
        if (op_is_store(op)) {
            if (op == op_store_8x4 || op == op_store_16x4 || op == op_store_16x4_planar) {
                fprintf(f, "      %-15s p%d v%d v%d v%d v%d\n", op_name(op), ip->ptr.bits,
                        ip->x.id, ip->y.id, ip->z.id, ip->w.id);
            } else if (op == op_store_var) {
                fprintf(f, "      %-15s var%d v%d\n", op_name(op), ip->imm, ip->y.id);
            } else {
                fprintf(f, "      %-15s p%d v%d\n", op_name(op), ip->ptr.bits, ip->y.id);
            }
            continue;
        }

        fprintf(f, "  v%-3d = %-15s", i, op_name(op));

        switch (op) {
        case op_imm_32: fprintf(f, " 0x%x", (uint32_t)ip->imm); break;
        case op_join: fprintf(f, " v%d v%d", ip->x.id, ip->y.id); break;
        case op_uniform_32:
            fprintf(f, " p%d [%d]", ip->ptr.bits, ip->imm);
            break;
        case op_gather_uniform_32:
        case op_gather_32:
        case op_gather_16:
        case op_gather_8:  fprintf(f, " p%d v%d", ip->ptr.bits, ip->x.id); break;
        case op_load_16:
        case op_load_32:
        case op_load_16x4:
        case op_load_16x4_planar:
        case op_load_8x4:
        case op_load_8:    fprintf(f, " p%d", ip->ptr.bits); break;
        case op_loop_begin: fprintf(f, " v%d", ip->x.id); break;
        case op_if_begin: fprintf(f, " v%d", ip->x.id); break;
        case op_load_var: fprintf(f, " var%d", ip->imm); break;
        case op_x:
        case op_y:
        case op_loop_end:
        case op_if_end:
        case op_store_16:
        case op_store_32:
        case op_store_var:
        case op_store_16x4:
        case op_store_16x4_planar:
        case op_store_8x4:
        case op_store_8:   break;

        case op_sqrt_f32:
        case op_abs_f32:
        case op_square_f32:
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
        case op_square_add_f32:
        case op_square_sub_f32:
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

void umbra_flat_ir_dump(struct umbra_flat_ir const *ir, FILE *f) {
    dump_insts(ir->inst, ir->insts, f);
}
