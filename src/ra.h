#pragma once
#include "bb.h"
#include <stdint.h>

typedef void (*ra_spill_fn)(int reg, int slot, void *ctx);
typedef void (*ra_fill_fn) (int reg, int slot, void *ctx);

struct ra_config {
    int8_t const *pool;
    ra_spill_fn   spill;
    ra_fill_fn    fill;
    void         *ctx;
    int           nregs, max_reg, has_pairs, :32;
};

struct ra {
    int    *last_use;
    int8_t *reg;
    int8_t *reg_hi;
    _Bool  *is_pair;
    int    *owner;
    int8_t *free_stack;
    struct ra_config cfg;
    int     nfree;
    int     pinned[4];
    int     npinned;
};

struct ra* ra_create (struct umbra_basic_block const *bb, struct ra_config const *cfg);
void       ra_destroy(struct ra *ra);
void       ra_free_reg(struct ra *ra, int val);
int8_t     ra_alloc  (struct ra *ra, int *sl, int *ns);
int8_t     ra_ensure (struct ra *ra, int *sl, int *ns, int val);
int8_t     ra_claim  (struct ra *ra, int old_val, int new_val);

static inline int8_t ra_hi(struct ra const *ra, int val) {
    return ra->reg_hi[val] >= 0 ? ra->reg_hi[val] : ra->reg[val];
}

struct ra_step {
    int8_t rd, rdh;
    int8_t rx, ry, rz;
    int8_t rxh, ryh, rzh;
    int8_t scratch;
    int8_t :8;
};

// Allocate output register(s) for instruction i (no inputs).
struct ra_step ra_step_alloc(struct ra *ra, int *sl, int *ns, int i);

// Unary conversion: ensure x, claim rd if x dead, else alloc rd.
// Handles pair narrowing (32→16) and widening (16→32).
struct ra_step ra_step_unary(struct ra *ra, int *sl, int *ns,
                             struct bb_inst const *inst, int i, _Bool scalar);

// Full ALU: ensure x/y/z, dead analysis, claim/alloc rd,
// alloc scratch if arch_scratch, free dead inputs.
// FMA accumulator targeting and sel mask claiming are handled internally.
struct ra_step ra_step_alu(struct ra *ra, int *sl, int *ns,
                           struct bb_inst const *inst, int i, _Bool scalar,
                           _Bool arch_scratch);
