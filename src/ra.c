#include "ra.h"
#include <limits.h>
#include <stdlib.h>

struct ra {
    int                  *last_use;
    int                 (*chan_last_use)[4];
    int8_t               *reg;
    int                  *owner;
    int8_t               *free_stack;
    int8_t               *loop_reg;
    int8_t              (*chan_reg)[4];
    struct bb_inst const *inst;
    struct ra_config      cfg;
    int                   nfree;
    int                   preamble;
    int                   insts;
    int                   npinned;
    int                   pinned[4];
};

int8_t ra_reg(struct ra const *ra, int val) { return ra->reg[val]; }
int8_t ra_chan_reg(struct ra const *ra, int val, int chan) { return ra->chan_reg[val][chan]; }
int    ra_last_use(struct ra const *ra, int val) { return ra->last_use[val]; }
int    ra_chan_last_use(struct ra const *ra, int val, int chan) { return ra->chan_last_use[val][chan]; }

void ra_set_last_use(struct ra *ra, int val, int lu) { ra->last_use[val] = lu; }

void ra_return_reg(struct ra *ra, int8_t r) { ra->free_stack[ra->nfree++] = r; }

void ra_assign(struct ra *ra, int val, int8_t r) {
    ra->reg[val] = r;
    ra->owner[(int)r] = val;
}
void ra_set_chan_reg(struct ra *ra, int val, int chan, int8_t r) {
    ra->chan_reg[val][chan] = r;
}

struct ra* ra_create(struct umbra_basic_block const *bb, struct ra_config const *cfg) {
    int const  n = bb->insts;
    struct ra *ra = malloc(sizeof *ra);
    ra->cfg = *cfg;
    ra->reg = malloc((size_t)n * sizeof *ra->reg);
    ra->last_use = malloc((size_t)n * sizeof *ra->last_use);
    ra->owner = malloc((size_t)cfg->max_reg * sizeof *ra->owner);
    ra->free_stack = malloc((size_t)cfg->max_reg * sizeof *ra->free_stack);

    ra->chan_reg = malloc((size_t)n * sizeof *ra->chan_reg);
    ra->chan_last_use = malloc((size_t)n * sizeof *ra->chan_last_use);
    for (int i = 0; i < n; i++) { ra->reg[i] = -1; }
    for (int i = 0; i < n; i++) {
        for (int c = 0; c < 4; c++) { ra->chan_reg[i][c] = -1; ra->chan_last_use[i][c] = -1; }
    }
    for (int i = 0; i < cfg->max_reg; i++) { ra->owner[i] = -1; }

    for (int i = 0; i < n; i++) { ra->last_use[i] = -1; }
    for (int i = 0; i < n; i++) {
        struct bb_inst const *inst = &bb->inst[i];
        ra->last_use[(int)inst->x.id] = i;
        ra->chan_last_use[(int)inst->x.id][(int)inst->x.chan] = i;
        if (!cfg->ignore_imm_y || !is_fused_imm(inst->op)) {
            ra->last_use[(int)inst->y.id] = i;
            ra->chan_last_use[(int)inst->y.id][(int)inst->y.chan] = i;
        }
        ra->last_use[(int)inst->z.id] = i;
        ra->chan_last_use[(int)inst->z.id][(int)inst->z.chan] = i;
        ra->last_use[(int)inst->w.id] = i;
        ra->chan_last_use[(int)inst->w.id][(int)inst->w.chan] = i;
    }
    for (int i = 0; i < bb->preamble; i++) {
        if (ra->last_use[i] >= bb->preamble) {
            ra->last_use[i] = n;
        }
    }

    ra->inst = bb->inst;
    ra->insts = n;
    ra->preamble = bb->preamble;
    ra->loop_reg = malloc((size_t)bb->preamble * sizeof *ra->loop_reg);

    ra->nfree = cfg->nregs;
    ra->npinned = 0;
    for (int i = 0; i < cfg->nregs; i++) {
        ra->free_stack[i] = cfg->pool[cfg->nregs - 1 - i];
    }

    return ra;
}

void ra_reset_pool(struct ra *ra) {
    ra->nfree = ra->cfg.nregs;
    for (int i = 0; i < ra->cfg.nregs; i++) {
        ra->free_stack[i] = ra->cfg.pool[ra->cfg.nregs - 1 - i];
    }
    for (int i = 0; i < ra->cfg.max_reg; i++) { ra->owner[i] = -1; }
    for (int i = 0; i < ra->insts; i++) { ra->reg[i] = -1; }
}

void ra_destroy(struct ra *ra) {
    free(ra->reg);
    free(ra->chan_reg);
    free(ra->chan_last_use);
    free(ra->last_use);
    free(ra->owner);
    free(ra->free_stack);
    free(ra->loop_reg);
    free(ra);
}

void ra_begin_loop(struct ra *ra) {
    for (int i = 0; i < ra->preamble; i++) {
        ra->loop_reg[i] = ra->reg[i];
    }
}

static _Bool can_remat(struct ra const *ra, int val) {
    return ra->cfg.remat && ra->inst[val].op == op_imm_32;
}

void ra_end_loop(struct ra *ra, int *sl) {
    for (int i = 0; i < ra->preamble; i++) {
        int8_t const target = ra->loop_reg[i];
        if (target < 0) { continue; }
        if (ra->reg[i] == target) { continue; }
        if (sl[i] >= 0) {
            ra->cfg.fill(target, sl[i], ra->cfg.ctx);
        } else if (can_remat(ra, i)) {
            ra->cfg.remat(target, i, ra->cfg.ctx);
        }
    }
}

void ra_free_reg(struct ra *ra, int val) {
    int8_t const r = ra->reg[val];
    if (r >= 0) {
        ra->free_stack[ra->nfree++] = r;
        ra->owner[(int)r] = -1;
        ra->reg[val] = -1;
    }
}

int8_t ra_alloc(struct ra *ra, int *sl, int *ns) {
    if (ra->nfree > 0) {
        return ra->free_stack[--ra->nfree];
    }

    int best_r = -1, best_lu = -1;
    for (int r = 0; r < ra->cfg.max_reg; r++) {
        if (ra->owner[r] < 0) { continue; }
        int const val = ra->owner[r];
        _Bool skip = 0;
        for (int p = 0; p < ra->npinned; p++) {
            if (ra->pinned[p] == val) {
                skip = 1;
                break;
            }
        }
        if (skip) { continue; }
        int const lu = ra->last_use[val] < 0 ? INT_MAX : ra->last_use[val];
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
        ra->cfg.spill(ra->reg[evicted], sl[evicted], ra->cfg.ctx);
    }
    ra->owner[(int)ra->reg[evicted]] = -1;
    int8_t const r = ra->reg[evicted];
    ra->reg[evicted] = -1;
    return r;
}

int8_t ra_ensure_chan(struct ra *ra, int *sl, int *ns, int val, int chan) {
    if (chan != 0) { return ra->chan_reg[val][chan]; }
    return ra_ensure(ra, sl, ns, val);
}
int8_t ra_ensure(struct ra *ra, int *sl, int *ns, int val) {
    if (ra->reg[val] < 0) {
        int8_t const r = ra_alloc(ra, sl, ns);
        if (sl[val] >= 0) {
            ra->cfg.fill(r, sl[val], ra->cfg.ctx);
        } else if (can_remat(ra, val)) {
            ra->cfg.remat(r, val, ra->cfg.ctx);
        }
        ra->reg[val] = r;
        ra->owner[(int)r] = val;
    }
    return ra->reg[val];
}

int8_t ra_claim(struct ra *ra, int old_val, int new_val) {
    int8_t const r = ra->reg[old_val];
    ra->reg[old_val] = -1;
    ra->reg[new_val] = r;
    ra->owner[(int)r] = new_val;
    return r;
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
    ra->reg[i] = s.rd;
    ra->owner[(int)s.rd] = i;
    return s;
}

struct ra_step ra_step_unary(struct ra *ra, int *sl, int *ns, struct bb_inst const *inst,
                             int i) {
    struct ra_step s = step0();
    ra->npinned = 0;
    s.rx = ra_ensure_chan(ra, sl, ns, (int)inst->x.id, (int)inst->x.chan);
    // Pin the input so the rd allocation below cannot evict it.
    if ((int)inst->x.id < i && !inst->x.chan) {
        ra->pinned[ra->npinned++] = (int)inst->x.id;
    }
    _Bool const x_dead = inst->x.chan
        ? ra->chan_last_use[(int)inst->x.id][(int)inst->x.chan] <= i
        : ra->last_use[(int)inst->x.id] <= i;
    if (x_dead && !inst->x.chan) {
        s.rd = ra_claim(ra, (int)inst->x.id, i);
    } else {
        s.rd = ra_alloc(ra, sl, ns);
        ra->reg[i] = s.rd;
        ra->owner[(int)s.rd] = i;
    }
    // Pin the new value so any subsequent ra_alloc / ra_ensure (e.g. the
    // backend's ra_ensure for inst->y.id in _imm op handling) cannot evict
    // it. Stays pinned until the next ra_step_* resets npinned.
    ra->pinned[ra->npinned++] = i;
    if (x_dead && inst->x.chan) {
        int8_t r = ra->chan_reg[(int)inst->x.id][(int)inst->x.chan];
        if (r >= 0) { ra_return_reg(ra, r); ra->chan_reg[(int)inst->x.id][(int)inst->x.chan] = -1; }
    }
    return s;
}

struct ra_step ra_step_alu(struct ra *ra, int *sl, int *ns, struct bb_inst const *inst,
                           int i, int nscratch) {
    int           *lu = ra->last_use;
    struct ra_step s = step0();

    ra->npinned = 0;
    if ((int)inst->x.id < i) {
        s.rx = ra_ensure_chan(ra, sl, ns, (int)inst->x.id, (int)inst->x.chan);
        ra->pinned[ra->npinned++] = (int)inst->x.id;
    }
    if ((int)inst->y.id < i) {
        s.ry = ra_ensure_chan(ra, sl, ns, (int)inst->y.id, (int)inst->y.chan);
        if (inst->y.bits != inst->x.bits) { ra->pinned[ra->npinned++] = (int)inst->y.id; }
    }
    if ((int)inst->z.id < i) {
        s.rz = ra_ensure_chan(ra, sl, ns, (int)inst->z.id, (int)inst->z.chan);
        if (inst->z.bits != inst->x.bits &&
            inst->z.bits != inst->y.bits) {
            ra->pinned[ra->npinned++] = (int)inst->z.id;
        }
    }

    // Dead analysis: a scalar operand is dead when last_use <= i.
    // Channel operands can't be claimed (claim uses reg[id], not chan_reg),
    // so only mark scalar operands as dead for the claim logic.
    // Channel registers are freed separately below.
    _Bool x_dead = !inst->x.chan && (int)inst->x.id < i && lu[(int)inst->x.id] <= i;
    _Bool y_dead = !inst->y.chan && (int)inst->y.id < i && lu[(int)inst->y.id] <= i;
    _Bool z_dead = !inst->z.chan && (int)inst->z.id < i && lu[(int)inst->z.id] <= i;
    if (inst->y.bits == inst->x.bits) { y_dead = 0; }
    if (inst->z.bits == inst->x.bits) { z_dead = 0; }
    if (inst->z.bits == inst->y.bits) { z_dead = 0; }

    enum op const op = inst->op;
    _Bool const   fma = op == op_fma_f32 || op == op_fms_f32;
    _Bool const   destructive = fma || op == op_sel_32;

    if (fma && z_dead) {
        s.rd = ra_claim(ra, (int)inst->z.id, i);
        z_dead = 0;
    } else if (fma && x_dead) {
        s.rd = ra_claim(ra, (int)inst->x.id, i);
        x_dead = 0;
    } else if (fma && y_dead) {
        s.rd = ra_claim(ra, (int)inst->y.id, i);
        y_dead = 0;
    } else if (fma && !z_dead) {
        s.rd = ra_alloc(ra, sl, ns);
        ra->reg[i] = s.rd;
        ra->owner[(int)s.rd] = i;
    } else if (op == op_sel_32 && x_dead) {
        s.rd = ra_claim(ra, (int)inst->x.id, i);
        x_dead = 0;
    } else if (op == op_sel_32 && y_dead) {
        s.rd = ra_claim(ra, (int)inst->y.id, i);
        y_dead = 0;
    } else if (op == op_sel_32 && z_dead) {
        s.rd = ra_claim(ra, (int)inst->z.id, i);
        z_dead = 0;
    }

    if (!destructive) {
        if (s.rd < 0 && x_dead) {
            s.rd = ra_claim(ra, (int)inst->x.id, i);
            x_dead = 0;
        }
        if (s.rd < 0 && y_dead) {
            s.rd = ra_claim(ra, (int)inst->y.id, i);
            y_dead = 0;
        }
    }

    if (s.rd < 0) {
        s.rd = ra_alloc(ra, sl, ns);
        ra->reg[i] = s.rd;
        ra->owner[(int)s.rd] = i;
    }

    _Bool const fma_scratch = fma && s.rd != s.rz && (s.rd == s.rx || s.rd == s.ry);
    if (nscratch >= 1 || fma_scratch) { s.scratch = ra_alloc(ra, sl, ns); }
    if (nscratch >= 2) { s.scratch2 = ra_alloc(ra, sl, ns); }

    ra->npinned = 0;
    if (x_dead) { ra_free_reg(ra, (int)inst->x.id); }
    if (y_dead) { ra_free_reg(ra, (int)inst->y.id); }
    if (z_dead) { ra_free_reg(ra, (int)inst->z.id); }

    // Free channel registers whose per-channel last use has expired.
#define FREE_CHAN_RA(op) do {                                                           \
    if ((op).chan && (int)(op).id < i &&                                                \
        ra->chan_last_use[(int)(op).id][(int)(op).chan] <= i) {                         \
        int8_t r_ = ra->chan_reg[(int)(op).id][(int)(op).chan];                         \
        if (r_ >= 0) { ra_return_reg(ra, r_); ra->chan_reg[(int)(op).id][(int)(op).chan] = -1; } \
    }                                                                                   \
} while(0)
    FREE_CHAN_RA(inst->x);
    FREE_CHAN_RA(inst->y);
    FREE_CHAN_RA(inst->z);
#undef FREE_CHAN_RA

    return s;
}
