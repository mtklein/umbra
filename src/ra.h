#pragma once
#include "flat_ir.h"
#include <stdint.h>

typedef void (*ra_spill_fn)(int reg, int slot, void *ctx);
typedef void (*ra_fill_fn)(int reg, int slot, void *ctx);
typedef void (*ra_remat_fn)(int reg, int val, void *ctx);

struct ra_config {
    int8_t const *pool;
    ra_spill_fn   spill;
    ra_fill_fn    fill;
    ra_remat_fn   remat;
    int           nregs, max_reg;
    void         *ctx;
};

struct ra;
struct ra* ra_create(struct umbra_flat_ir const *ir, struct ra_config const *cfg);
void       ra_destroy(struct ra *ra);
void       ra_reset_pool(struct ra *ra);

// Force-spill every dispatch-tier value (i < dispatch_end) that is alive past
// the dispatch tier (last_use >= dispatch_end) and currently in a register,
// freeing the register.  After this call:
//   - dispatch tier values are at slot[i].reg = -1, sl[i] >= 0 (valid slot).
//   - their data is stored at sl[i].
// Used at the end of the function-entry dispatch emit so the row tier emit
// starts from a state where all dispatch values are spilled, matching the
// state after a row transition.
void ra_spill_dispatch(struct ra *ra, int *sl, int *ns);

// Clear bookkeeping for all preamble values (i < preamble) that are
// currently in registers — slot[i].reg = -1, owner[r] = -1, free_set
// updated.  Does NOT emit any spill; assumes the caller has already
// preserved any data that needs preserving (dispatch tier via
// ra_spill_dispatch at function entry; row tier values are recomputed by
// the row-tier re-emit).  Used at row_done to put the RA back into the
// "post dispatch-spill" state from which the row tier can be re-emitted.
void ra_clear_preamble(struct ra *ra);

void   ra_free_chan(struct ra *ra, val operand, int i);
void   ra_free_reg(struct ra *ra, int val);
int8_t ra_alloc(struct ra *ra, int *sl, int *ns);
int8_t ra_ensure(struct ra *ra, int *sl, int *ns, int val);
int8_t ra_ensure_chan(struct ra *ra, int *sl, int *ns, int val, int chan);
int8_t ra_claim(struct ra *ra, int old_val, int new_val);
void   ra_begin_loop(struct ra *ra);
void   ra_end_loop(struct ra *ra, int *sl);
void   ra_evict_live_before(struct ra *ra, int *sl, int *ns, int before);

// Assert that every preamble value is back in the register that ra_begin_loop
// captured for it.  Call at compile-time on each edge that branches to the
// start of the SIMD body -- if it ever fires, the emitted code will compare
// x against whatever garbage the previous edge left in that register.
void   ra_assert_loop_invariant(struct ra const *ra);

int8_t ra_reg(struct ra const *ra, int val);
int8_t ra_chan_reg(struct ra const *ra, int val, int chan);
int    ra_last_use(struct ra const *ra, int val);
int    ra_chan_last_use(struct ra const *ra, int val, int chan);

void ra_set_last_use(struct ra *ra, int val, int lu);
void ra_return_reg(struct ra *ra, int8_t r);
void ra_assign(struct ra *ra, int val, int8_t r);
void ra_set_chan_reg(struct ra *ra, int val, int chan, int8_t r);

struct ra_step {
    int8_t rd;
    int8_t rx, ry, rz;
    int8_t scratch, scratch2;
};

// Allocate output register for instruction i (no inputs).
struct ra_step ra_step_alloc(struct ra *ra, int *sl, int *ns, int i);

// Unary conversion: ensure x, claim rd if x dead, else alloc rd.
struct ra_step ra_step_unary(struct ra *ra, int *sl, int *ns, struct ir_inst const *inst,
                             int i);

// Full ALU: ensure x/y/z, dead analysis, claim/alloc rd,
// alloc scratch if needed, free dead inputs.
// FMA accumulator targeting and sel mask claiming are handled internally.
struct ra_step ra_step_alu(struct ra *ra, int *sl, int *ns, struct ir_inst const *inst,
                           int i, int nscratch);
