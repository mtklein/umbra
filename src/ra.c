#include "ra.h"
#include <limits.h>
#include <stdlib.h>

struct ra {
    int    *last_use;
    int8_t *reg;
    int8_t *reg_hi;
    _Bool  *is_pair;
    int    *owner;
    int8_t *free_stack;
    int8_t *loop_reg;       // snapshot of reg[0..preamble-1] at loop entry
    struct ra_config cfg;
    int     nfree;
    int     preamble;
    int     pinned[4];
    int     npinned, :32;
};

// ---- Accessors ----

int8_t ra_hi(struct ra const *ra, int val) {
    return ra->reg_hi[val] >= 0 ? ra->reg_hi[val] : ra->reg[val];
}

int8_t ra_reg(struct ra const *ra, int val) {
    return ra->reg[val];
}

int8_t ra_reg_hi(struct ra const *ra, int val) {
    return ra->reg_hi[val];
}

int ra_last_use(struct ra const *ra, int val) {
    return ra->last_use[val];
}

_Bool ra_is_pair(struct ra const *ra, int val) {
    return ra->is_pair[val];
}

void ra_set_last_use(struct ra *ra, int val, int lu) {
    ra->last_use[val] = lu;
}

void ra_set_pair(struct ra *ra, int val, _Bool p) {
    ra->is_pair[val] = p;
}

void ra_return_reg(struct ra *ra, int8_t r) {
    ra->free_stack[ra->nfree++] = r;
}

void ra_assign(struct ra *ra, int val, int8_t r) {
    ra->reg[val] = r;
    ra->owner[(int)r] = val;
}

void ra_assign_hi(struct ra *ra, int val, int8_t r) {
    ra->reg_hi[val] = r;
    ra->owner[(int)r] = val;
}

// ---- Core ----

struct ra* ra_create(struct umbra_basic_block const *bb, struct ra_config const *cfg) {
    int n = bb->insts;
    struct ra *ra = malloc(sizeof *ra);
    ra->cfg      = *cfg;
    ra->reg      = malloc((size_t)n * sizeof *ra->reg);
    ra->reg_hi   = malloc((size_t)n * sizeof *ra->reg_hi);
    ra->last_use = malloc((size_t)n * sizeof *ra->last_use);
    ra->is_pair  = malloc((size_t)n * sizeof *ra->is_pair);
    ra->owner    = malloc((size_t)cfg->max_reg * sizeof *ra->owner);
    ra->free_stack = malloc((size_t)cfg->max_reg * sizeof *ra->free_stack);

    for (int i = 0; i < n; i++) {
        ra->reg[i] = -1;
        ra->reg_hi[i] = -1;
        ra->is_pair[i] = cfg->has_pairs
                       && (i >= bb->preamble);
    }
    for (int i = 0; i < cfg->max_reg; i++) { ra->owner[i] = -1; }

    for (int i = 0; i < n; i++) { ra->last_use[i] = -1; }
    for (int i = 0; i < n; i++) {
        struct bb_inst const *inst = &bb->inst[i];
        if (!(inst->op == op_load_8x4 && inst->x)) { ra->last_use[inst->x] = i; }
        ra->last_use[inst->y] = i;
        ra->last_use[inst->z] = i;
        ra->last_use[inst->w] = i;
    }
    for (int i = 0; i < bb->preamble; i++) {
        if (ra->last_use[i] >= bb->preamble) {
            ra->last_use[i] = n;
        }
    }

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
    free(ra->reg_hi);
    free(ra->last_use);
    free(ra->is_pair);
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

void ra_end_loop(struct ra *ra, int *sl) {
    for (int i = 0; i < ra->preamble; i++) {
        int8_t target = ra->loop_reg[i];
        if (target < 0) { continue; }
        if (ra->reg[i] == target) { continue; }
        if (sl[i] < 0) { continue; }
        ra->cfg.fill(target, sl[i], ra->cfg.ctx);
    }
}

void ra_free_reg(struct ra *ra, int val) {
    int8_t r = ra->reg[val];
    if (r < 0) { return; }
    ra->free_stack[ra->nfree++] = r;
    ra->owner[(int)r] = -1;
    ra->reg[val] = -1;
    int8_t rh = ra->reg_hi[val];
    if (rh >= 0) {
        ra->free_stack[ra->nfree++] = rh;
        ra->owner[(int)rh] = -1;
        ra->reg_hi[val] = -1;
    }
}

int8_t ra_alloc(struct ra *ra, int *sl, int *ns) {
    if (ra->nfree > 0) { return ra->free_stack[--ra->nfree]; }

    // Evict: find register whose owner has farthest last_use (Belady-like).
    // Dead values (last_use < 0) are evicted first since they'll never be used.
    // Pinned values (inputs being ensured for the current instruction) are skipped.
    int best_r = -1, best_lu = -1;
    for (int r = 0; r < ra->cfg.max_reg; r++) {
        if (ra->owner[r] < 0) { continue; }
        int val = ra->owner[r];
        _Bool skip = 0;
        for (int p = 0; p < ra->npinned; p++) { if (ra->pinned[p] == val) { skip = 1; break; } }
        if (skip) { continue; }
        int lu = ra->last_use[val] < 0 ? INT_MAX : ra->last_use[val];
        if (lu > best_lu) { best_lu = lu; best_r = r; }
    }
    int evicted = ra->owner[best_r];
    if (sl[evicted] < 0) {
        if (ra->is_pair[evicted]) { sl[evicted] = *ns; *ns += 2; }
        else { sl[evicted] = (*ns)++; }
    }
    // Spill lo
    ra->cfg.spill(ra->reg[evicted], sl[evicted], ra->cfg.ctx);
    ra->owner[(int)ra->reg[evicted]] = -1;
    // Spill hi if pair
    if (ra->reg_hi[evicted] >= 0) {
        ra->cfg.spill(ra->reg_hi[evicted], sl[evicted]+1, ra->cfg.ctx);
        int8_t rh = ra->reg_hi[evicted];
        ra->owner[(int)rh] = -1;
        ra->reg_hi[evicted] = -1;
        ra->free_stack[ra->nfree++] = rh;
    }
    int8_t r = ra->reg[evicted];
    ra->reg[evicted] = -1;
    return r;
}

int8_t ra_ensure(struct ra *ra, int *sl, int *ns, int val) {
    if (ra->reg[val] >= 0) { return ra->reg[val]; }
    int8_t r = ra_alloc(ra, sl, ns);
    if (sl[val] >= 0) { ra->cfg.fill(r, sl[val], ra->cfg.ctx); }
    ra->reg[val] = r;
    ra->owner[(int)r] = val;
    if (ra->is_pair[val]) {
        int8_t rh = ra_alloc(ra, sl, ns);
        if (sl[val] >= 0) { ra->cfg.fill(rh, sl[val]+1, ra->cfg.ctx); }
        ra->reg_hi[val] = rh;
        ra->owner[(int)rh] = val;
    }
    return r;
}

int8_t ra_claim(struct ra *ra, int old_val, int new_val) {
    int8_t r = ra->reg[old_val];
    ra->reg[old_val] = -1;
    ra->reg[new_val] = r;
    ra->owner[(int)r] = new_val;
    if (ra->reg_hi[old_val] >= 0) {
        int8_t rh = ra->reg_hi[old_val];
        ra->reg_hi[old_val] = -1;
        ra->reg_hi[new_val] = rh;
        ra->owner[(int)rh] = new_val;
    }
    return r;
}

static struct ra_step step0(void) {
    return (struct ra_step){ .rd=-1,.rdh=-1, .rx=-1,.ry=-1,.rz=-1,
                             .rxh=-1,.ryh=-1,.rzh=-1, .scratch=-1,.scratch2=-1 };
}

struct ra_step ra_step_alloc(struct ra *ra, int *sl, int *ns, int i) {
    struct ra_step s = step0();
    s.rd = ra_alloc(ra, sl, ns);
    ra->reg[i] = s.rd; ra->owner[(int)s.rd] = i;
    if (ra->is_pair[i]) {
        s.rdh = ra_alloc(ra, sl, ns);
        ra->reg_hi[i] = s.rdh; ra->owner[(int)s.rdh] = i;
    }
    return s;
}

struct ra_step ra_step_unary(struct ra *ra, int *sl, int *ns,
                             struct bb_inst const *inst, int i, _Bool scalar) {
    struct ra_step s = step0();
    s.rx = ra_ensure(ra, sl, ns, inst->x);
    s.rxh = ra_hi(ra, inst->x);
    _Bool x_dead = ra->last_use[inst->x] <= i;

    _Bool out_pair  = ra->is_pair[i] && !scalar;
    _Bool in_pair   = ra->is_pair[inst->x] && !scalar;

    if (out_pair && !in_pair) {
        // Widening (16→32): can't claim (need 2 regs from 1). Alloc both.
        s.rd  = ra_alloc(ra, sl, ns);
        s.rdh = ra_alloc(ra, sl, ns);
        ra->reg[i] = s.rd; ra->reg_hi[i] = s.rdh;
        ra->owner[(int)s.rd] = i; ra->owner[(int)s.rdh] = i;
        if (x_dead) { ra_free_reg(ra, inst->x); }
    } else if (x_dead) {
        s.rd = ra_claim(ra, inst->x, i);
        s.rdh = ra->reg_hi[i];
        // For narrowing (pair→single), free the unneeded hi register.
        if (in_pair && !out_pair && s.rdh >= 0) {
            ra->free_stack[ra->nfree++] = s.rdh;
            ra->owner[(int)s.rdh] = -1;
            ra->reg_hi[i] = -1;
            s.rdh = -1;
        }
    } else {
        s.rd = ra_alloc(ra, sl, ns);
        ra->reg[i] = s.rd; ra->owner[(int)s.rd] = i;
        if (out_pair) {
            s.rdh = ra_alloc(ra, sl, ns);
            ra->reg_hi[i] = s.rdh; ra->owner[(int)s.rdh] = i;
        }
    }
    return s;
}

struct ra_step ra_step_alu(struct ra *ra, int *sl, int *ns,
                           struct bb_inst const *inst, int i, _Bool scalar,
                           int nscratch) {
    int *lu = ra->last_use;
    struct ra_step s = step0();
    _Bool pair = ra->is_pair[i] && !scalar;

    // 1. Ensure inputs, pinning each so later ensures/allocs can't evict them.
    //    Pins stay active through step 5 so that hi-half register captures
    //    (rxh/ryh/rzh) remain valid across rd/rdh/scratch allocation.
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
    s.rxh = inst->x < i ? ra_hi(ra, inst->x) : 0;
    s.ryh = inst->y < i ? ra_hi(ra, inst->y) : 0;
    s.rzh = inst->z < i ? ra_hi(ra, inst->z) : 0;

    // 2. Dead analysis (prevent double-free of aliased operands).
    _Bool x_dead = inst->x < i && lu[inst->x] <= i;
    _Bool y_dead = inst->y < i && lu[inst->y] <= i;
    _Bool z_dead = inst->z < i && lu[inst->z] <= i;
    if (inst->y == inst->x) { y_dead = 0; }
    if (inst->z == inst->x) { z_dead = 0; }
    if (inst->z == inst->y) { z_dead = 0; }

    // 3. Claim/alloc rd with op-specific preferences.
    //    When the output is paired, don't claim a non-paired input's register —
    //    it would alias rxh/ryh/rzh, causing the hi-half emit to read stale data
    //    after the lo-half emit overwrites rd.
    enum op op = inst->op;
    _Bool fma = op==op_fma_f32 || op==op_fms_f32;
    _Bool destructive = fma || op==op_sel_32;

    _Bool can_claim_x = !pair || ra->is_pair[inst->x];
    _Bool can_claim_y = !pair || ra->is_pair[inst->y];
    _Bool can_claim_z = !pair || ra->is_pair[inst->z];

    if (fma && z_dead && can_claim_z) {
        s.rd = ra_claim(ra, inst->z, i); z_dead = 0;
    } else if (fma && !z_dead) {
        // Pre-allocate rd to avoid rd aliasing rx/ry (avoids scratch+3-MOV path).
        s.rd = ra_alloc(ra, sl, ns);
        ra->reg[i] = s.rd; ra->owner[(int)s.rd] = i;
    } else if (op==op_sel_32 && x_dead && can_claim_x) {
        s.rd = ra_claim(ra, inst->x, i); x_dead = 0;
    }

    if (!destructive) {
        if (s.rd < 0 && x_dead && can_claim_x) { s.rd = ra_claim(ra, inst->x, i); x_dead = 0; }
        if (s.rd < 0 && y_dead && can_claim_y) { s.rd = ra_claim(ra, inst->y, i); y_dead = 0; }
        if (s.rd < 0 && z_dead && can_claim_z) { s.rd = ra_claim(ra, inst->z, i); z_dead = 0; }
    }

    // 4. Alloc rd if not yet assigned.
    if (s.rd < 0) {
        s.rd = ra_alloc(ra, sl, ns);
        ra->reg[i] = s.rd; ra->owner[(int)s.rd] = i;
    }
    if (pair && ra->reg_hi[i] < 0) {
        s.rdh = ra_alloc(ra, sl, ns);
        ra->reg_hi[i] = s.rdh; ra->owner[(int)s.rdh] = i;
    } else if (pair) {
        s.rdh = ra->reg_hi[i];
    }

    // 5. Scratch allocation (before freeing dead inputs to prevent aliasing).
    _Bool fma_scratch = fma && s.rd != s.rz && (s.rd == s.rx || s.rd == s.ry);
    if (nscratch >= 1 || fma_scratch) {
        s.scratch = ra_alloc(ra, sl, ns);
    }
    if (nscratch >= 2) {
        s.scratch2 = ra_alloc(ra, sl, ns);
    }

    // 6. Clear pins and free dead inputs.
    ra->npinned = 0;
    if (x_dead) { ra_free_reg(ra, inst->x); }
    if (y_dead) { ra_free_reg(ra, inst->y); }
    if (z_dead) { ra_free_reg(ra, inst->z); }

    return s;
}
