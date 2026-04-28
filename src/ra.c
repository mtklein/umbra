#include "ra.h"
#include "assume.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

// Per-instruction state. Indexed by val (an ir_inst id).
struct ra_slot {
    int    last_use;
    int    chan_last_use[4];
    int8_t reg;
    int8_t chan_reg[4];
    int    :24;
};

// pool_mask, pinned_set, and free_set are bitmaps indexed by *pool
// position* (cfg.pool index 0..nregs-1), not by physical register
// id, so the bit math works even when cfg.pool is sparse like
// {5,6,7,8}. pool_inv inverts cfg.pool: pool_inv[reg] = bit position,
// or -1 for registers not in the pool. Real backends use nregs ≤ 16;
// the cap of 32 below leaves headroom while keeping each bitmap in a
// single word.
struct ra {
    struct ra_slot       *slot;       // n entries (one per ir_inst)
    int                  *owner;      // max_reg entries (val owning physical reg, -1 if free)
    int8_t               *pool_inv;   // max_reg entries (reg id -> pool bit, or -1)
    int8_t               *loop_reg;   // row_end entries (snapshot of slot[i].reg at SIMD-loop top)
    struct ir_inst const *inst;
    struct ra_config      cfg;
    uint32_t              pool_mask;  // (1 << nregs) - 1
    uint32_t              free_set;   // bit i set => cfg.pool[i] is free
    uint32_t              pinned_set; // bit i set => cfg.pool[i] is held this step
    int                   dispatch_end;
    int                   row_end;
    int                   insts;
};

int8_t ra_reg(struct ra const *ra, int val) { return ra->slot[val].reg; }
int8_t ra_chan_reg(struct ra const *ra, int val, int chan) { return ra->slot[val].chan_reg[chan]; }
int    ra_last_use(struct ra const *ra, int val) { return ra->slot[val].last_use; }
int    ra_chan_last_use(struct ra const *ra, int val, int chan) {
    return ra->slot[val].chan_last_use[chan];
}

void ra_set_last_use(struct ra *ra, int val, int lu) { ra->slot[val].last_use = lu; }

void ra_return_reg(struct ra *ra, int8_t r) {
    int8_t const bit = ra->pool_inv[(int)r];
    ra->free_set |= (uint32_t)1 << bit;
}

void ra_assign(struct ra *ra, int val, int8_t r) {
    ra->slot[val].reg = r;
    ra->owner[(int)r] = val;
}
void ra_set_chan_reg(struct ra *ra, int val, int chan, int8_t r) {
    ra->slot[val].chan_reg[chan] = r;
}

struct ra* ra_create(struct umbra_flat_ir const *ir, struct ra_config const *cfg) {
    int const  n  = ir->insts;
    assume(cfg->nregs <= 32);
    struct ra *ra = malloc(sizeof *ra);
    ra->cfg        = *cfg;
    ra->slot       = malloc((size_t)n            * sizeof *ra->slot);
    ra->owner      = malloc((size_t)cfg->max_reg * sizeof *ra->owner);
    ra->pool_inv   = malloc((size_t)cfg->max_reg * sizeof *ra->pool_inv);
    for (int i = 0; i < n; i++) {
        ra->slot[i].reg      = -1;
        ra->slot[i].last_use = -1;
        for (int c = 0; c < 4; c++) {
            ra->slot[i].chan_reg     [c] = -1;
            ra->slot[i].chan_last_use[c] = -1;
        }
    }
    for (int i = 0; i < cfg->max_reg; i++) { ra->owner[i] = -1; ra->pool_inv[i] = -1; }
    for (int i = 0; i < cfg->nregs;   i++) { ra->pool_inv[cfg->pool[i]] = (int8_t)i; }
    ra->pool_mask = ~0u >> (32 - cfg->nregs);

    for (int i = 0; i < n; i++) {
        struct ir_inst const *inst = &ir->inst[i];
        ra->slot[inst->x.id].last_use                          = i;
        ra->slot[inst->x.id].chan_last_use[(int)inst->x.chan]  = i;
        ra->slot[inst->y.id].last_use                          = i;
        ra->slot[inst->y.id].chan_last_use[(int)inst->y.chan]  = i;
        ra->slot[inst->z.id].last_use                          = i;
        ra->slot[inst->z.id].chan_last_use[(int)inst->z.chan]  = i;
        ra->slot[inst->w.id].last_use                          = i;
        ra->slot[inst->w.id].chan_last_use[(int)inst->w.chan]  = i;
    }
    // Extend each masked-path if_begin's condition val through its matching
    // if_end so the mask register is not evicted from under the body's
    // store_vars.  flat_ir populates if_end.x.id with the matching if_begin's
    // inst id.  Uniform-cond if_begins lower as a real branch: the cond is
    // consumed at if_begin (one element extracted to a GPR) and never read
    // inside the body, so no extension is needed.
    for (int i = 0; i < n; i++) {
        if (ir->inst[i].op == op_if_end) {
            int const ib      = ir->inst[i].x.id;
            int const cond_id = ir->inst[ib].x.id;
            if (!ir->inst[cond_id].uniform) {
                ra->slot[cond_id].last_use                                = i;
                ra->slot[cond_id].chan_last_use[(int)ir->inst[ib].x.chan] = i;
            }
        }
    }
    for (int i = 0; i < ir->row_end; i++) {
        if (ra->slot[i].last_use >= ir->row_end) {
            ra->slot[i].last_use = n;
        }
    }

    ra->inst = ir->inst;
    ra->insts = n;
    ra->dispatch_end = ir->dispatch_end;
    ra->row_end = ir->row_end;
    ra->loop_reg = malloc((size_t)ir->row_end * sizeof *ra->loop_reg);

    ra->free_set   = ra->pool_mask;
    ra->pinned_set = 0;

    return ra;
}

void ra_reset_pool(struct ra *ra) {
    ra->free_set   = ra->pool_mask;
    ra->pinned_set = 0;
    for (int i = 0; i < ra->cfg.max_reg; i++) { ra->owner[i] = -1; }
    for (int i = 0; i < ra->insts; i++) { ra->slot[i].reg = -1; }
}

static _Bool can_remat(struct ra const *ra, int val);

void ra_spill_dispatch(struct ra *ra, int *sl, int *ns) {
    for (int V = 0; V < ra->dispatch_end; V++) {
        int8_t const r = ra->slot[V].reg;
        if (r >= 0) {
            if (!can_remat(ra, V) && ra->slot[V].last_use >= ra->dispatch_end) {
                if (sl[V] < 0) { sl[V] = (*ns)++; }
                ra->cfg.spill(r, sl[V], ra->cfg.ctx);
            }
            ra_free_reg(ra, V);
        }
    }
}

void ra_destroy(struct ra *ra) {
    free(ra->slot);
    free(ra->owner);
    free(ra->pool_inv);
    free(ra->loop_reg);
    free(ra);
}

void ra_begin_loop(struct ra *ra) {
    for (int i = 0; i < ra->row_end; i++) {
        ra->loop_reg[i] = ra->slot[i].reg;
    }
}

static _Bool can_remat(struct ra const *ra, int val) {
    return ra->cfg.remat && ra->inst[val].op == op_imm_32;
}

void ra_assert_loop_invariant(struct ra const *ra) {
    for (int i = 0; i < ra->row_end; i++) {
        assume(ra->slot[i].reg == ra->loop_reg[i]);
    }
}

void ra_end_loop(struct ra *ra, int *sl) {
    for (int i = 0; i < ra->row_end; i++) {
        int8_t const target = ra->loop_reg[i];
        if (ra->slot[i].reg != target) {
            if (target < 0) {
                // Val i had no register at SIMD-loop top (evicted during the
                // dispatch or row tier emit, its data lives in sl[i]).  Body
                // emit may have ra_ensure'd it back into some register;
                // release that so next iteration's first read performs the
                // same fill as this iteration did.  No emitted instruction
                // needed — the data is already in sl[i].
                int8_t const old_r = ra->slot[i].reg;
                if (old_r >= 0 && ra->owner[(int)old_r] == i) {
                    ra->owner[(int)old_r] = -1;
                    int8_t const bit = ra->pool_inv[(int)old_r];
                    if (bit >= 0) { ra->free_set |= (uint32_t)1 << bit; }
                }
                ra->slot[i].reg = -1;
            } else {
                // Drop the owner record for val i's current register (if any)
                // before reassigning to target.  Otherwise a later iteration
                // that needs to restore a different val into that old register
                // would see val i still listed as the occupant and clobber
                // slot[i].reg back to -1.
                int8_t const old_r = ra->slot[i].reg;
                if (old_r >= 0 && ra->owner[(int)old_r] == i) {
                    ra->owner[(int)old_r] = -1;
                }
                int const occ = ra->owner[(int)target];
                if (occ >= 0 && occ != i) {
                    ra->slot[occ].reg = -1;
                    ra->owner[(int)target] = -1;
                }
                if (sl[i] >= 0) {
                    ra->cfg.fill(target, sl[i], ra->cfg.ctx);
                } else if (can_remat(ra, i)) {
                    ra->cfg.remat(target, i, ra->cfg.ctx);
                }
                ra->slot[i].reg = target;
                ra->owner[(int)target] = i;
            }
        }
    }
}

void ra_evict_live_before(struct ra *ra, int *sl, int *ns, int before) {
    for (int v = 0; v < before; v++) {
        int8_t const r = ra->slot[v].reg;
        if (r >= 0 && ra->slot[v].last_use > before) {
            if (!can_remat(ra, v) && sl[v] < 0) {
                sl[v] = (*ns)++;
            }
            if (sl[v] >= 0) {
                ra->cfg.spill(r, sl[v], ra->cfg.ctx);
            }
            ra_free_reg(ra, v);
        }
    }
}

void ra_free_chan(struct ra *ra, val operand, int i) {
    int id = operand.id, ch = (int)operand.chan;
    if (ch) {
        if (ra_chan_last_use(ra, id, ch) <= i) {
            int8_t r = ra_chan_reg(ra, id, ch);
            if (r >= 0) { ra_return_reg(ra, r); ra_set_chan_reg(ra, id, ch, -1); }
        }
    } else {
        if (ra_last_use(ra, id) <= i) { ra_free_reg(ra, id); }
    }
}

void ra_free_reg(struct ra *ra, int val) {
    int8_t const r = ra->slot[val].reg;
    if (r >= 0) {
        int8_t const bit = ra->pool_inv[(int)r];
        ra->free_set |= (uint32_t)1 << bit;
        ra->owner[(int)r] = -1;
        ra->slot[val].reg = -1;
    }
}

int8_t ra_alloc(struct ra *ra, int *sl, int *ns) {
    // Fast path: pop the lowest free pool bit. Allocations come out
    // in cfg.pool order (pool[0] first), not LIFO; this is fine for
    // every existing backend since their pools are dense ascending
    // ranges anyway.
    if (ra->free_set) {
        int const bit = __builtin_ctz(ra->free_set);
        ra->free_set &= ra->free_set - 1;
        return ra->cfg.pool[bit];
    }

    // Slow path: every pool register is allocated. Walk the
    // non-pinned pool bits with ctz, look up each candidate's owner,
    // and pick the val with the farthest last_use (Belady). Bits
    // with owner < 0 are scratch registers that were popped but not
    // yet bound to a val (a backend mid-emit) and are skipped.
    int best_r = -1, best_lu = -1;
    for (uint32_t cand = ra->pool_mask & ~ra->pinned_set; cand; cand &= cand - 1) {
        int const bit = __builtin_ctz(cand);
        int const r   = ra->cfg.pool[bit];
        int const val = ra->owner[r];
        if (val >= 0) {
            int const lu = ra->slot[val].last_use < 0 ? INT_MAX : ra->slot[val].last_use;
            if (best_lu < lu) {
                best_lu = lu;
                best_r = r;
            }
        }
    }
    int const evicted = ra->owner[best_r];
    if (can_remat(ra, evicted)) {
        sl[evicted] = -1;
    } else {
        if (sl[evicted] < 0) {
            sl[evicted] = (*ns)++;
        }
        ra->cfg.spill(ra->slot[evicted].reg, sl[evicted], ra->cfg.ctx);
    }
    int8_t const r = ra->slot[evicted].reg;
    ra->owner[(int)r] = -1;
    ra->slot[evicted].reg = -1;
    return r;
}

// Pin the pool bit currently holding val so the eviction scan in
// ra_alloc / ra_ensure can't pick it. A val whose slot[].reg is -1
// (chan-only or not yet materialized) has nothing to pin in the main
// pool, matching the old "owner[r]==pinned_val never matched" no-op.
static void pin_val(struct ra *ra, int val) {
    int8_t const r = ra->slot[val].reg;
    if (r >= 0) {
        int8_t const bit = ra->pool_inv[(int)r];
        if (bit >= 0) { ra->pinned_set |= (uint32_t)1 << bit; }
    }
}

void ra_step(struct ra *ra) { ra->pinned_set = 0; }

int8_t ra_ensure_chan(struct ra *ra, int *sl, int *ns, int val, int chan) {
    // chan != 0 returns chan_reg directly: those registers always have
    // ra->owner[reg] = -1 (ra_set_chan_reg doesn't claim ownership), so
    // ra_alloc's Belady scan already skips them and no hold is needed.
    if (chan != 0) { return ra->slot[val].chan_reg[chan]; }
    return ra_ensure(ra, sl, ns, val);
}
int8_t ra_ensure(struct ra *ra, int *sl, int *ns, int val) {
    if (ra->slot[val].reg < 0) {
        int8_t const r = ra_alloc(ra, sl, ns);
        if (sl[val] >= 0) {
            ra->cfg.fill(r, sl[val], ra->cfg.ctx);
        } else if (can_remat(ra, val)) {
            ra->cfg.remat(r, val, ra->cfg.ctx);
        }
        ra->slot[val].reg = r;
        ra->owner[(int)r] = val;
    }
    // Auto-hold: the caller obviously needs val's register on the
    // current instruction (otherwise why ensure?), so subsequent
    // ra_alloc calls in the same step must not evict it.  Released by
    // the next ra_step() call.
    pin_val(ra, val);
    return ra->slot[val].reg;
}

int8_t ra_claim(struct ra *ra, int old_val, int new_val) {
    int8_t const r = ra->slot[old_val].reg;
    ra->slot[old_val].reg = -1;
    ra->slot[new_val].reg = r;
    ra->owner[(int)r] = new_val;
    return r;
}

// Free a per-channel register if its last use has expired.
static void free_chan_if_dead(struct ra *ra, val op, int i) {
    struct ra_slot *slot = &ra->slot[op.id];
    if (op.chan && op.id < i && slot->chan_last_use[(int)op.chan] <= i) {
        int8_t const reg = slot->chan_reg[(int)op.chan];
        if (reg >= 0) {
            ra_return_reg(ra, reg);
            slot->chan_reg[(int)op.chan] = -1;
        }
    }
}

static struct ra_step step0(void) {
    return (struct ra_step){
        .rd = -1,
        .rx = -1,
        .ry = -1,
        .rz = -1,
        .scratch = -1,
        .scratch2 = -1,
    };
}

struct ra_step ra_step_alloc(struct ra *ra, int *sl, int *ns, int i) {
    struct ra_step s = step0();
    s.rd = ra_alloc(ra, sl, ns);
    ra->slot[i].reg = s.rd;
    ra->owner[(int)s.rd] = i;
    return s;
}

struct ra_step ra_step_unary(struct ra *ra, int *sl, int *ns, struct ir_inst const *inst,
                             int i) {
    struct ra_step s = step0();
    ra->pinned_set = 0;
    s.rx = ra_ensure_chan(ra, sl, ns, inst->x.id, (int)inst->x.chan);
    // Pin the input so the rd allocation below cannot evict it.
    if (inst->x.id < i && !inst->x.chan) {
        pin_val(ra, inst->x.id);
    }
    _Bool const x_dead = inst->x.chan
        ? ra->slot[inst->x.id].chan_last_use[(int)inst->x.chan] <= i
        : ra->slot[inst->x.id].last_use <= i;
    if (x_dead && !inst->x.chan) {
        s.rd = ra_claim(ra, inst->x.id, i);
    } else {
        s.rd = ra_alloc(ra, sl, ns);
        ra->slot[i].reg = s.rd;
        ra->owner[(int)s.rd] = i;
    }
    // Pin the new value so any subsequent ra_alloc / ra_ensure inside the
    // same step (e.g. for a scratch register or another operand the backend
    // loads after ra_step_unary) cannot evict it.  Stays pinned until the
    // next ra_step_* resets pinned_set.
    pin_val(ra, i);
    if (x_dead && inst->x.chan) {
        int8_t const r = ra->slot[inst->x.id].chan_reg[(int)inst->x.chan];
        if (r >= 0) {
            ra_return_reg(ra, r);
            ra->slot[inst->x.id].chan_reg[(int)inst->x.chan] = -1;
        }
    }
    return s;
}

struct ra_step ra_step_alu(struct ra *ra, int *sl, int *ns, struct ir_inst const *inst,
                           int i, int nscratch) {
    struct ra_slot const *slot = ra->slot;
    struct ra_step s = step0();

    ra->pinned_set = 0;
    if (inst->x.id < i) {
        s.rx = ra_ensure_chan(ra, sl, ns, inst->x.id, (int)inst->x.chan);
        pin_val(ra, inst->x.id);
    }
    if (inst->y.id < i) {
        s.ry = ra_ensure_chan(ra, sl, ns, inst->y.id, (int)inst->y.chan);
        if (inst->y.bits != inst->x.bits) { pin_val(ra, inst->y.id); }
    }
    if (inst->z.id < i) {
        s.rz = ra_ensure_chan(ra, sl, ns, inst->z.id, (int)inst->z.chan);
        if (inst->z.bits != inst->x.bits &&
            inst->z.bits != inst->y.bits) {
            pin_val(ra, inst->z.id);
        }
    }

    // Dead analysis: a scalar operand is dead when last_use <= i.
    // Channel operands can't be claimed (claim uses reg[id], not chan_reg),
    // so only mark scalar operands as dead for the claim logic.
    // Channel registers are freed separately below.
    _Bool x_dead = !inst->x.chan && inst->x.id < i && slot[inst->x.id].last_use <= i;
    _Bool y_dead = !inst->y.chan && inst->y.id < i && slot[inst->y.id].last_use <= i;
    _Bool z_dead = !inst->z.chan && inst->z.id < i && slot[inst->z.id].last_use <= i;
    if (inst->y.bits == inst->x.bits) { y_dead = 0; }
    if (inst->z.bits == inst->x.bits) { z_dead = 0; }
    if (inst->z.bits == inst->y.bits) { z_dead = 0; }

    enum op const op  = inst->op;
    _Bool const   fma = op == op_fma_f32 || op == op_fms_f32;
    // square_add/sub(x, y) = x*x + y (or y - x*x): y is the accumulator,
    // so its RA treatment mirrors z in fma.  x plays the role of fma's x and y.
    _Bool const   sqa = op == op_square_add_f32 || op == op_square_sub_f32;
    _Bool const   destructive = fma || sqa || op == op_sel_32;

    if (fma && z_dead) {
        s.rd = ra_claim(ra, inst->z.id, i);
        z_dead = 0;
    } else if (fma && x_dead) {
        s.rd = ra_claim(ra, inst->x.id, i);
        x_dead = 0;
    } else if (fma && y_dead) {
        s.rd = ra_claim(ra, inst->y.id, i);
        y_dead = 0;
    } else if (fma && !z_dead) {
        s.rd = ra_alloc(ra, sl, ns);
        ra->slot[i].reg = s.rd;
        ra->owner[(int)s.rd] = i;
    } else if (sqa && y_dead) {
        s.rd = ra_claim(ra, inst->y.id, i);
        y_dead = 0;
    } else if (sqa && x_dead) {
        s.rd = ra_claim(ra, inst->x.id, i);
        x_dead = 0;
    } else if (sqa && !y_dead) {
        s.rd = ra_alloc(ra, sl, ns);
        ra->slot[i].reg = s.rd;
        ra->owner[(int)s.rd] = i;
    } else if (op == op_sel_32 && x_dead) {
        s.rd = ra_claim(ra, inst->x.id, i);
        x_dead = 0;
    } else if (op == op_sel_32 && y_dead) {
        s.rd = ra_claim(ra, inst->y.id, i);
        y_dead = 0;
    } else if (op == op_sel_32 && z_dead) {
        s.rd = ra_claim(ra, inst->z.id, i);
        z_dead = 0;
    }

    if (!destructive) {
        if (s.rd < 0 && x_dead) {
            s.rd = ra_claim(ra, inst->x.id, i);
            x_dead = 0;
        }
        if (s.rd < 0 && y_dead) {
            s.rd = ra_claim(ra, inst->y.id, i);
            y_dead = 0;
        }
    }

    if (s.rd < 0) {
        s.rd = ra_alloc(ra, sl, ns);
        ra->slot[i].reg = s.rd;
        ra->owner[(int)s.rd] = i;
    }

    // Pin the dest before any scratch allocation. Without this, a
    // scratch ra_alloc that hits the slow path can pick the dest's
    // register (Belady would prefer it whenever last_use(i) is the
    // farthest among unpinned candidates), spilling its uninitialized
    // contents to a slot and then handing the same register back as
    // s.scratch — collapsing s.rd onto s.scratch and corrupting the
    // dest val's would-be spill slot.
    pin_val(ra, i);

    _Bool const fma_scratch = fma && s.rd != s.rz && (s.rd == s.rx || s.rd == s.ry);
    _Bool const sqa_scratch = sqa && s.rd != s.ry &&  s.rd == s.rx;
    if (nscratch >= 1 || fma_scratch || sqa_scratch) { s.scratch = ra_alloc(ra, sl, ns); }
    if (nscratch >= 2) { s.scratch2 = ra_alloc(ra, sl, ns); }

    ra->pinned_set = 0;
    if (x_dead) { ra_free_reg(ra, inst->x.id); }
    if (y_dead) { ra_free_reg(ra, inst->y.id); }
    if (z_dead) { ra_free_reg(ra, inst->z.id); }

    // Free channel registers whose per-channel last use has expired.
    free_chan_if_dead(ra, inst->x, i);
    free_chan_if_dead(ra, inst->y, i);
    free_chan_if_dead(ra, inst->z, i);

    return s;
}
