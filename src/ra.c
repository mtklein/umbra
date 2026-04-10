#include "ra.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

// Per-instruction state. Indexed by val (a bb_inst id).
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
    struct ra_slot       *slot;       // n entries (one per bb_inst)
    int                  *owner;      // max_reg entries (val owning physical reg, -1 if free)
    int8_t               *pool_inv;   // max_reg entries (reg id -> pool bit, or -1)
    int8_t               *loop_reg;   // preamble entries (snapshot of slot[i].reg at loop top)
    struct bb_inst const *inst;
    struct ra_config      cfg;
    uint32_t              pool_mask;  // (1 << nregs) - 1
    uint32_t              free_set;   // bit i set => cfg.pool[i] is free
    uint32_t              pinned_set; // bit i set => cfg.pool[i] is pinned
    int                   preamble;
    int                   insts;
    int                   :32;
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

struct ra* ra_create(struct umbra_basic_block const *bb, struct ra_config const *cfg) {
    int const  n  = bb->insts;
    assert(cfg->nregs <= 32);
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
    ra->pool_mask = (cfg->nregs == 32) ? ~(uint32_t)0
                                       : (((uint32_t)1 << cfg->nregs) - 1);

    for (int i = 0; i < n; i++) {
        struct bb_inst const *inst = &bb->inst[i];
        ra->slot[inst->x.id].last_use                          = i;
        ra->slot[inst->x.id].chan_last_use[(int)inst->x.chan]  = i;
        if (!cfg->ignore_imm_y || !op_is_fused_imm(inst->op)) {
            ra->slot[inst->y.id].last_use                          = i;
            ra->slot[inst->y.id].chan_last_use[(int)inst->y.chan]  = i;
        }
        ra->slot[inst->z.id].last_use                          = i;
        ra->slot[inst->z.id].chan_last_use[(int)inst->z.chan]  = i;
        ra->slot[inst->w.id].last_use                          = i;
        ra->slot[inst->w.id].chan_last_use[(int)inst->w.chan]  = i;
    }
    for (int i = 0; i < bb->preamble; i++) {
        if (ra->slot[i].last_use >= bb->preamble) {
            ra->slot[i].last_use = n;
        }
    }

    ra->inst = bb->inst;
    ra->insts = n;
    ra->preamble = bb->preamble;
    ra->loop_reg = malloc((size_t)bb->preamble * sizeof *ra->loop_reg);

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

void ra_destroy(struct ra *ra) {
    free(ra->slot);
    free(ra->owner);
    free(ra->pool_inv);
    free(ra->loop_reg);
    free(ra);
}

void ra_begin_loop(struct ra *ra) {
    for (int i = 0; i < ra->preamble; i++) {
        ra->loop_reg[i] = ra->slot[i].reg;
    }
}

static _Bool can_remat(struct ra const *ra, int val) {
    return ra->cfg.remat && ra->inst[val].op == op_imm_32;
}

void ra_end_loop(struct ra *ra, int *sl) {
    for (int i = 0; i < ra->preamble; i++) {
        int8_t const target = ra->loop_reg[i];
        if (target < 0) { continue; }
        if (ra->slot[i].reg == target) { continue; }
        if (sl[i] >= 0) {
            ra->cfg.fill(target, sl[i], ra->cfg.ctx);
        } else if (can_remat(ra, i)) {
            ra->cfg.remat(target, i, ra->cfg.ctx);
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
        if (val < 0) { continue; }
        int const lu = ra->slot[val].last_use < 0 ? INT_MAX : ra->slot[val].last_use;
        if (best_lu < lu) {
            best_lu = lu;
            best_r = r;
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

int8_t ra_ensure_chan(struct ra *ra, int *sl, int *ns, int val, int chan) {
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
    return ra->slot[val].reg;
}

int8_t ra_claim(struct ra *ra, int old_val, int new_val) {
    int8_t const r = ra->slot[old_val].reg;
    ra->slot[old_val].reg = -1;
    ra->slot[new_val].reg = r;
    ra->owner[(int)r] = new_val;
    return r;
}

// Pin the pool bit currently holding val so the eviction scan in
// ra_alloc / ra_ensure can't pick it. A val whose slot[].reg is -1
// (chan-only or not yet materialized) has nothing to pin in the main
// pool, matching the old "owner[r]==pinned_val never matched" no-op.
static inline void pin_val(struct ra *ra, int val) {
    int8_t const r = ra->slot[val].reg;
    if (r >= 0) {
        int8_t const bit = ra->pool_inv[(int)r];
        if (bit >= 0) { ra->pinned_set |= (uint32_t)1 << bit; }
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

struct ra_step ra_step_unary(struct ra *ra, int *sl, int *ns, struct bb_inst const *inst,
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
    // Pin the new value so any subsequent ra_alloc / ra_ensure (e.g. the
    // backend's ra_ensure for inst->y.id in _imm op handling) cannot evict
    // it. Stays pinned until the next ra_step_* resets pinned_set.
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

struct ra_step ra_step_alu(struct ra *ra, int *sl, int *ns, struct bb_inst const *inst,
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

    enum op const op = inst->op;
    _Bool const   fma = op == op_fma_f32 || op == op_fms_f32;
    _Bool const   destructive = fma || op == op_sel_32;

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
    if (nscratch >= 1 || fma_scratch) { s.scratch = ra_alloc(ra, sl, ns); }
    if (nscratch >= 2) { s.scratch2 = ra_alloc(ra, sl, ns); }

    ra->pinned_set = 0;
    if (x_dead) { ra_free_reg(ra, inst->x.id); }
    if (y_dead) { ra_free_reg(ra, inst->y.id); }
    if (z_dead) { ra_free_reg(ra, inst->z.id); }

    // Free channel registers whose per-channel last use has expired.
#define FREE_CHAN_RA(op) do {                                                            \
    struct ra_slot *s_ = &ra->slot[(op).id];                                        \
    if ((op).chan && (op).id < i && s_->chan_last_use[(int)(op).chan] <= i) {      \
        int8_t r_ = s_->chan_reg[(int)(op).chan];                                        \
        if (r_ >= 0) { ra_return_reg(ra, r_); s_->chan_reg[(int)(op).chan] = -1; }      \
    }                                                                                    \
} while(0)
    FREE_CHAN_RA(inst->x);
    FREE_CHAN_RA(inst->y);
    FREE_CHAN_RA(inst->z);
#undef FREE_CHAN_RA

    return s;
}
