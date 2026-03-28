#include "ra.h"
#include <limits.h>
#include <stdlib.h>

struct ra {
    int                  *last_use;
    int8_t               *reg;
    int                  *owner;
    int8_t               *free_stack;
    int8_t               *loop_reg;
    struct bb_inst const *inst;
    struct ra_config      cfg;
    int                   nfree;
    int                   preamble;
    int                   pinned[3];
    int                   npinned, : 32, : 32;
};

int8_t ra_reg(struct ra const *ra, int val) { return ra->reg[val]; }
int    ra_last_use(struct ra const *ra, int val) { return ra->last_use[val]; }

void ra_set_last_use(struct ra *ra, int val, int lu) { ra->last_use[val] = lu; }

void ra_return_reg(struct ra *ra, int8_t r) { ra->free_stack[ra->nfree++] = r; }

void ra_assign(struct ra *ra, int val, int8_t r) {
    ra->reg[val] = r;
    ra->owner[(int)r] = val;
}

struct ra* ra_create(struct umbra_basic_block const *bb, struct ra_config const *cfg) {
    int const  n = bb->insts;
    struct ra *ra = malloc(sizeof *ra);
    ra->cfg = *cfg;
    ra->reg = malloc((size_t)n * sizeof *ra->reg);
    ra->last_use = malloc((size_t)n * sizeof *ra->last_use);
    ra->owner = malloc((size_t)cfg->max_reg * sizeof *ra->owner);
    ra->free_stack = malloc((size_t)cfg->max_reg * sizeof *ra->free_stack);

    for (int i = 0; i < n; i++) { ra->reg[i] = -1; }
    for (int i = 0; i < cfg->max_reg; i++) { ra->owner[i] = -1; }

    for (int i = 0; i < n; i++) { ra->last_use[i] = -1; }
    for (int i = 0; i < n; i++) {
        struct bb_inst const *inst = &bb->inst[i];
        ra->last_use[inst->x] = i;
        if (!cfg->ignore_imm_y || !is_fused_imm(inst->op)) { ra->last_use[inst->y] = i; }
        ra->last_use[inst->z] = i;
        ra->last_use[inst->w] = i;
    }
    for (int i = 0; i < bb->preamble; i++) {
        if (ra->last_use[i] >= bb->preamble) {
            ra->last_use[i] = n;
        }
    }

    ra->inst = bb->inst;
    ra->preamble = bb->preamble;
    ra->loop_reg = malloc((size_t)bb->preamble * sizeof *ra->loop_reg);

    ra->nfree = cfg->nregs;
    ra->npinned = 0;
    for (int i = 0; i < cfg->nregs; i++) {
        ra->free_stack[i] = cfg->pool[cfg->nregs - 1 - i];
    }

    return ra;
}

void ra_destroy(struct ra *ra) {
    free(ra->reg);
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
                             int i, _Bool scalar) {
    (void)scalar;
    struct ra_step s = step0();
    s.rx = ra_ensure(ra, sl, ns, inst->x);
    _Bool const x_dead = ra->last_use[inst->x] <= i;
    if (x_dead) {
        s.rd = ra_claim(ra, inst->x, i);
    } else {
        s.rd = ra_alloc(ra, sl, ns);
        ra->reg[i] = s.rd;
        ra->owner[(int)s.rd] = i;
    }
    return s;
}

struct ra_step ra_step_alu(struct ra *ra, int *sl, int *ns, struct bb_inst const *inst,
                           int i, _Bool scalar, int nscratch) {
    (void)scalar;
    int           *lu = ra->last_use;
    struct ra_step s = step0();

    ra->npinned = 0;
    if (inst->x < i) {
        s.rx = ra_ensure(ra, sl, ns, inst->x);
        ra->pinned[ra->npinned++] = inst->x;
    }
    if (inst->y < i) {
        s.ry = ra_ensure(ra, sl, ns, inst->y);
        if (inst->y != inst->x) { ra->pinned[ra->npinned++] = inst->y; }
    }
    if (inst->z < i) {
        s.rz = ra_ensure(ra, sl, ns, inst->z);
        if (inst->z != inst->x && inst->z != inst->y) {
            ra->pinned[ra->npinned++] = inst->z;
        }
    }

    _Bool x_dead = inst->x < i && lu[inst->x] <= i;
    _Bool y_dead = inst->y < i && lu[inst->y] <= i;
    _Bool z_dead = inst->z < i && lu[inst->z] <= i;
    if (inst->y == inst->x) { y_dead = 0; }
    if (inst->z == inst->x) { z_dead = 0; }
    if (inst->z == inst->y) { z_dead = 0; }

    enum op const op = inst->op;
    _Bool const   fma = op == op_fma_f32 || op == op_fms_f32;
    _Bool const   destructive = fma || op == op_sel_32;

    if (fma && z_dead) {
        s.rd = ra_claim(ra, inst->z, i);
        z_dead = 0;
    } else if (fma && x_dead) {
        s.rd = ra_claim(ra, inst->x, i);
        x_dead = 0;
    } else if (fma && y_dead) {
        s.rd = ra_claim(ra, inst->y, i);
        y_dead = 0;
    } else if (fma && !z_dead) {
        s.rd = ra_alloc(ra, sl, ns);
        ra->reg[i] = s.rd;
        ra->owner[(int)s.rd] = i;
    } else if (op == op_sel_32 && x_dead) {
        s.rd = ra_claim(ra, inst->x, i);
        x_dead = 0;
    } else if (op == op_sel_32 && y_dead) {
        s.rd = ra_claim(ra, inst->y, i);
        y_dead = 0;
    } else if (op == op_sel_32 && z_dead) {
        s.rd = ra_claim(ra, inst->z, i);
        z_dead = 0;
    }

    if (!destructive) {
        if (s.rd < 0 && x_dead) {
            s.rd = ra_claim(ra, inst->x, i);
            x_dead = 0;
        }
        if (s.rd < 0 && y_dead) {
            s.rd = ra_claim(ra, inst->y, i);
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
    if (x_dead) { ra_free_reg(ra, inst->x); }
    if (y_dead) { ra_free_reg(ra, inst->y); }
    if (z_dead) { ra_free_reg(ra, inst->z); }

    return s;
}
