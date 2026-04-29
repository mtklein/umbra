#include "../include/umbra.h"
#include "assume.h"
#include "count.h"
#include "flat_ir.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static _Bool kind_is_buf(enum binding_kind k) {
    return k == BIND_BUF || k == BIND_SEALED;
}

_Bool flat_ir_has_early_writes(struct umbra_flat_ir const *ir) {
    _Bool is_early_buf[32] = {0};
    for (int k = 0; k < ir->bindings; k++) {
        struct buffer_binding const *bb = &ir->binding[k];
        if (kind_is_buf(bb->kind) && bb->buf) {
            int const ix = bb->ix;
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
        if (kind_is_buf(bb->kind)) {
            out[bb->ix] = bb->buf ? *bb->buf : (struct umbra_buf){0};
        } else {
            out[bb->ix] = bb->uniforms;
        }
    }
    for (int i = 0; i < lates; i++) {
        int const ix = late[i].ptr.ix;
        int bi = 0;
        while (bi < bindings && binding[bi].ix != ix) { bi++; }
        assume(bi < bindings);
        if (kind_is_buf(binding[bi].kind)) {
            out[ix] = late[i].buf;
        } else {
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
        deps,        // Unscheduled instructions this one still waits on.  Ready at 0.
        users,       // Instructions that read this one's values.  Decremented as scheduled.
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
        // deps == 1 means c is the sole remaining dep; scheduling c readies that user.
        int readied_ops = 0;
        for (int u = meta[c].user_off; u < meta[c].user_off + meta[c].users; u++) {
            readied_ops += meta[users[u]].deps == 1;
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
    return inst->live && inst->scope >= SCOPE_BATCH;
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
                    meta[i].deps++;
                    meta[d].users++;
                }
            }
            if (in[i].op == op_store_var) {
                for (int j = region_lo; j < i; j++) {
                    if (in[j].op == op_load_var && in[j].imm == in[i].imm
                            && is_body(in + j)) {
                        meta[i].deps++;
                        meta[j].users++;
                        meta[j].last_use[0] = i;
                    }
                }
            }
            // load_var must read after any prior store_var of the same var so
            // the user observes writes in program order.
            if (in[i].op == op_load_var) {
                for (int j = region_lo; j < i; j++) {
                    if (in[j].op == op_store_var && in[j].imm == in[i].imm
                            && is_body(in + j)) {
                        meta[i].deps++;
                        meta[j].users++;
                    }
                }
            }
            // Stores serialize globally in program order, regardless of which
            // var or buffer they write.  Anything weaker can let the scheduler
            // drop a "second write wins" semantically — including two
            // store_vars of the same var, or two store_32s into the same buf.
            if (op_is_store(in[i].op)) {
                for (int j = region_lo; j < i; j++) {
                    if (op_is_store(in[j].op) && is_body(in + j)) {
                        meta[i].deps++;
                        meta[j].users++;
                    }
                }
            }
        }
    }

    for (int i = 0; i < n; i++) {
        meta[i + 1].user_off = meta[i].user_off + meta[i].users;
        meta[i].users = 0;
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
                    users[meta[d].user_off + meta[d].users++] = i;
                }
            }
            if (in[i].op == op_store_var) {
                for (int j = region_lo; j < i; j++) {
                    if (in[j].op == op_load_var && in[j].imm == in[i].imm
                            && is_body(in + j)) {
                        users[meta[j].user_off + meta[j].users++] = i;
                    }
                }
            }
            if (in[i].op == op_load_var) {
                for (int j = region_lo; j < i; j++) {
                    if (in[j].op == op_store_var && in[j].imm == in[i].imm
                            && is_body(in + j)) {
                        users[meta[j].user_off + meta[j].users++] = i;
                    }
                }
            }
            if (op_is_store(in[i].op)) {
                for (int j = region_lo; j < i; j++) {
                    if (op_is_store(in[j].op) && is_body(in + j)) {
                        users[meta[j].user_off + meta[j].users++] = i;
                    }
                }
            }
        }
    }

    int nready = 0;
    for (int i = 0; i < n; i++) {
        if (is_body(in + i) && i >= region_lo && i < region_hi && meta[i].deps == 0) {
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

        for (int u = meta[id].user_off; u < meta[id].user_off + meta[id].users; u++) {
            if (--meta[users[u]].deps == 0) {
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

static _Bool is_reduction(enum op op) {
    return op == op_all_32 || op == op_any_32;
}

// Per-op intrinsic scope.  Source ops introduce scope; everything else is
// SCOPE_COMPILE intrinsically and inherits from operands via narrow().
// Control-flow ops have an intrinsic floor of SCOPE_BATCH — their structural
// placement delineates regions that must execute per-batch regardless of
// operand scope.
//
// op_y is SCOPE_ROW: constant within a row, varies per row.  The JITs set
// up XY (the row-state register) before emitting the row tier so op_y
// broadcasts the right row, and the row-tier re-emit at each row
// transition refreshes the broadcast for the new row.  Anything purely
// derived from op_y + uniforms therefore hoists out of the per-batch
// body into the row tier.
static enum scope intrinsic_scope(enum op op) {
    if (is_cf(op))                  { return SCOPE_BATCH; }
    if (is_reduction(op))           { return SCOPE_BATCH; }
    if (op == op_imm_32)            { return SCOPE_COMPILE; }
    // op_uniform_32 reads a uniform-bound buffer at a compile-time index;
    // by API contract that buffer's stride is zero, so the read is purely
    // dispatch-scope.  Gathers (uniform_32, _32, _16 variants) take any
    // ptr — including BIND_BUF buffers with stride>0 — but address them
    // flat (base + idx*elem_size, ignoring Y*stride), so they intrinsically
    // sit at dispatch scope too.  Index scope narrows the result the rest
    // of the way: uniform index keeps DISPATCH; lane-varying narrows to
    // LANE.
    if (op == op_uniform_32
            || op == op_gather_uniform_32
            || op == op_gather_32
            || op == op_gather_16)         { return SCOPE_DISPATCH; }
    if (op == op_y)                        { return SCOPE_ROW; }
    if (op_is_varying(op))                 { return SCOPE_LANE; }
    return SCOPE_COMPILE;
}
static enum scope narrow(enum scope a, enum scope b) {
    return a > b ? a : b;
}

static void compute_buf_meta(struct umbra_flat_ir *ir);

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

    // Scope propagation.  An op inside a loop or if region keeps its
    // data-flow scope — there is no region cap.  Pure ops with broader
    // input scopes hoist out of regions into the dispatch tier or row
    // tier; only ops that truly depend on per-iteration / per-lane state
    // (loads, vars, lane index) end up in the body.  Stores stay at their
    // textual position because op_is_store excludes them from tier
    // extraction below.
    for (int i = 0; i < n; i++) {
        enum scope s = intrinsic_scope(b->inst[i].op);
        if (!is_reduction(b->inst[i].op)) {
            s = narrow(s, (enum scope)b->inst[b->inst[i].x.id].scope);
            s = narrow(s, (enum scope)b->inst[b->inst[i].y.id].scope);
            s = narrow(s, (enum scope)b->inst[b->inst[i].z.id].scope);
            s = narrow(s, (enum scope)b->inst[b->inst[i].w.id].scope);
        }
        b->inst[i].scope = (int8_t)s;
    }

    // Two-pass tier extraction so the result is a clean partition:
    // out[0..dispatch_end) holds scope ≤ SCOPE_DISPATCH ops (the dispatch
    // tier — emit once per queue() call), out[dispatch_end..row_end) holds
    // scope == SCOPE_ROW ops (the row tier — emit at each row's entry),
    // and out[row_end..insts) is the per-batch body.  Within each tier we
    // preserve textual order, so dependency ordering is intact.
    struct ir_inst *out = malloc((size_t)live * sizeof *out);
    int dispatch_end = 0;
    for (int i = 0; i < n; i++) {
        if (b->inst[i].live && b->inst[i].scope <= SCOPE_DISPATCH
                && !op_is_store(b->inst[i].op)) {
            b->inst[i].final_id = dispatch_end;
            out[dispatch_end++] = b->inst[i];
        }
    }
    int row_end = dispatch_end;
    for (int i = 0; i < n; i++) {
        if (b->inst[i].live && b->inst[i].scope == SCOPE_ROW
                && !op_is_store(b->inst[i].op)) {
            b->inst[i].final_id = row_end;
            out[row_end++] = b->inst[i];
        }
    }

    int *cf = malloc((size_t)n * sizeof *cf);
    int cfs = 0;
    for (int i = 0; i < n; i++) {
        if (b->inst[i].live && b->inst[i].scope >= SCOPE_BATCH && is_cf(b->inst[i].op)) {
            cf[cfs++] = i;
        }
    }

    int j = row_end;
    int region_lo = 0;
    for (int ci = 0; ci < cfs; ci++) {
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
    result->inst         = out;
    result->insts        = live;
    result->dispatch_end = dispatch_end;
    result->row_end      = row_end;
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
    compute_buf_meta(result);
    return result;
}

static void compute_buf_meta(struct umbra_flat_ir *ir) {
    int max_ptr = -1;
    for (int i = 0; i < ir->insts; i++) {
        if (op_has_ptr(ir->inst[i].op) && ir->inst[i].ptr.bits > max_ptr) {
            max_ptr = ir->inst[i].ptr.bits;
        }
    }

    int const total = max_ptr + 1;
    ir->total_bufs  = total;
    ir->buf         = calloc((size_t)total, sizeof *ir->buf);
    for (int i = 0; i < ir->insts; i++) {
        enum op const op = ir->inst[i].op;
        if (op_has_ptr(op)) {
            int const p = ir->inst[i].ptr.bits;
            ir->buf[p].shift = (uint8_t)op_elem_shift(op);
            ir->buf[p].rw   |= op_is_store(op) ? BUF_WRITTEN : BUF_READ;
        }
    }
    for (int i = 0; i < ir->bindings; i++) {
        int const p = ir->binding[i].ix;
        if (0 <= p && p < total) {
            ir->buf[p].binding_kind = ir->binding[i].kind;
            if (ir->binding[i].kind == BIND_UNIFORMS) {
                ir->buf[p].uniform_slots = ir->binding[i].uniforms.count;
            }
        }
    }
}

void umbra_flat_ir_free(struct umbra_flat_ir *ir) {
    if (ir) {
        free(ir->inst);
        free(ir->binding);
        free(ir->buf);
        free(ir);
    }
}

static char scope_letter(int8_t scope) {
    switch (scope) {
        case SCOPE_COMPILE:  return 'C';
        case SCOPE_DISPATCH: return 'D';
        case SCOPE_ROW:      return 'R';
        case SCOPE_BATCH:    return 'B';
        case SCOPE_LANE:     return 'L';
    }
    return '?';
}

static void dump_insts(struct ir_inst const *inst, int insts, FILE *f) {
    for (int i = 0; i < insts; i++) {
        struct ir_inst const *ip = &inst[i];
        enum op const         op = ip->op;
        char           const  s  = scope_letter(ip->scope);

        if (op == op_loop_end) {
            fprintf(f, "    %c loop_end\n", s);
            continue;
        }
        if (op == op_if_end) {
            fprintf(f, "    %c if_end\n", s);
            continue;
        }
        if (op_is_store(op)) {
            if (op == op_store_8x4 || op == op_store_16x4 || op == op_store_16x4_planar) {
                fprintf(f, "    %c %-15s p%d v%d v%d v%d v%d\n", s, op_name(op), ip->ptr.bits,
                        ip->x.id, ip->y.id, ip->z.id, ip->w.id);
            } else if (op == op_store_var) {
                fprintf(f, "    %c %-15s var%d v%d\n", s, op_name(op), ip->imm, ip->y.id);
            } else {
                fprintf(f, "    %c %-15s p%d v%d\n", s, op_name(op), ip->ptr.bits, ip->y.id);
            }
            continue;
        }

        fprintf(f, "  v%-3d %c = %-15s", i, s, op_name(op));

        switch (op) {
        case op_imm_32: fprintf(f, " 0x%x", (uint32_t)ip->imm); break;
        case op_uniform_32:
            fprintf(f, " p%d [%d]", ip->ptr.bits, ip->imm);
            break;
        case op_gather_uniform_32:
        case op_gather_32:
        case op_gather_16: fprintf(f, " p%d v%d", ip->ptr.bits, ip->x.id); break;
        case op_load_16:
        case op_load_32:
        case op_load_16x4:
        case op_load_16x4_planar:
        case op_load_8x4: fprintf(f, " p%d", ip->ptr.bits); break;
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
        case op_store_8x4: break;

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
        case op_f16_from_f32:
        case op_all_32:
        case op_any_32: fprintf(f, " v%d", ip->x.id); break;

        case op_add_f32:
        case op_sub_f32:
        case op_mul_f32:
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
