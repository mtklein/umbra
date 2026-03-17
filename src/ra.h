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

// Opaque register allocator.
struct ra;

struct ra* ra_create (struct umbra_basic_block const *bb, struct ra_config const *cfg);
void       ra_destroy(struct ra *ra);

// Core operations.
void       ra_free_reg  (struct ra *ra, int val);
int8_t     ra_alloc     (struct ra *ra, int *sl, int *ns);
int8_t     ra_ensure    (struct ra *ra, int *sl, int *ns, int val);
int8_t     ra_claim     (struct ra *ra, int old_val, int new_val);
void       ra_begin_loop(struct ra *ra);
void       ra_end_loop  (struct ra *ra, int *sl);

// Accessors.
int8_t     ra_hi       (struct ra const *ra, int val);  // hi reg, falls back to lo
int8_t     ra_reg      (struct ra const *ra, int val);  // current lo reg (-1 if none)
int8_t     ra_reg_hi   (struct ra const *ra, int val);  // current hi reg (-1 if none)
int        ra_last_use (struct ra const *ra, int val);
_Bool      ra_is_pair  (struct ra const *ra, int val);

// Mutation helpers.
void       ra_set_last_use(struct ra *ra, int val, int lu);
void       ra_set_pair  (struct ra *ra, int val, _Bool p);
void       ra_return_reg(struct ra *ra, int8_t r);     // return scratch to free pool
void       ra_assign    (struct ra *ra, int val,
                         int8_t r);   // reg[val]=r
void       ra_assign_hi (struct ra *ra, int val,
                         int8_t r);   // reg_hi[val]=r

struct ra_step {
    int8_t rd, rdh;
    int8_t rx, ry, rz;
    int8_t rxh, ryh, rzh;
    int8_t scratch, scratch2;
};

// Allocate output register(s) for instruction i (no inputs).
struct ra_step ra_step_alloc(struct ra *ra, int *sl, int *ns, int i);

// Unary conversion: ensure x, claim rd if x dead, else alloc rd.
// Handles pair narrowing (32→16) and widening (16→32).
struct ra_step ra_step_unary(struct ra *ra, int *sl, int *ns,
                             struct bb_inst const *inst, int i, _Bool scalar);

// Full ALU: ensure x/y/z, dead analysis, claim/alloc rd,
// alloc scratch if needed, free dead inputs.
// FMA accumulator targeting and sel mask claiming are handled internally.
struct ra_step ra_step_alu(struct ra *ra, int *sl, int *ns,
                           struct bb_inst const *inst, int i, _Bool scalar,
                           int nscratch);
