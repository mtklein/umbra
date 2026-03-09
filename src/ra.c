#include "ra.h"
#include <limits.h>
#include <stdlib.h>

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
                       && (op_type(bb->inst[i].op) == OP_32)
                       && (i >= bb->preamble);
    }
    for (int i = 0; i < cfg->max_reg; i++) ra->owner[i] = -1;

    for (int i = 0; i < n; i++) ra->last_use[i] = -1;
    for (int i = 0; i < n; i++) {
        struct bb_inst const *inst = &bb->inst[i];
        if (!(inst->op == op_load_8x4 && inst->x)) ra->last_use[inst->x] = i;
        ra->last_use[inst->y] = i;
        ra->last_use[inst->z] = i;
        ra->last_use[inst->w] = i;
    }
    for (int i = 0; i < bb->preamble; i++) {
        if (ra->last_use[i] >= bb->preamble) {
            ra->last_use[i] = n;
        }
    }

    ra->nfree = cfg->nregs;
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
    free(ra);
}

void ra_free_reg(struct ra *ra, int val) {
    int8_t r = ra->reg[val];
    if (r < 0) return;
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
    if (ra->nfree > 0) return ra->free_stack[--ra->nfree];

    // Evict: find register whose owner has farthest last_use (Belady-like).
    // Dead values (last_use < 0) are evicted first since they'll never be used.
    int best_r = -1, best_lu = -1;
    for (int r = 0; r < ra->cfg.max_reg; r++) {
        if (ra->owner[r] < 0) continue;
        int val = ra->owner[r];
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
    if (ra->reg[val] >= 0) return ra->reg[val];
    int8_t r = ra_alloc(ra, sl, ns);
    if (sl[val] >= 0) ra->cfg.fill(r, sl[val], ra->cfg.ctx);
    ra->reg[val] = r;
    ra->owner[(int)r] = val;
    if (ra->is_pair[val]) {
        int8_t rh = ra_alloc(ra, sl, ns);
        if (sl[val] >= 0) ra->cfg.fill(rh, sl[val]+1, ra->cfg.ctx);
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
