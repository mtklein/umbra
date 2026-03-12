#include "../src/ra.h"
#include "test.h"
#include <stdlib.h>
#include <string.h>

// Track spill/fill calls for verification.
struct spill_record { int reg, slot; };
static struct spill_record spills[256], fills[256];
static int nspills, nfills;

static void test_spill(int reg, int slot, void *ctx) {
    (void)ctx;
    spills[nspills++] = (struct spill_record){reg, slot};
}
static void test_fill(int reg, int slot, void *ctx) {
    (void)ctx;
    fills[nfills++] = (struct spill_record){reg, slot};
}

static void reset_records(void) { nspills = nfills = 0; }

// Build a minimal BB with n instructions, all varying 16-bit ALU ops.
// preamble_n of them are preamble (uniforms), rest are varying.
static struct umbra_basic_block* make_bb(int n, int preamble_n) {
    struct umbra_basic_block *bb = malloc(sizeof *bb);
    bb->inst = calloc((size_t)n, sizeof *bb->inst);
    bb->insts = n;
    bb->preamble = preamble_n;
    bb->ht = 0;
    bb->ht_mask = 0;

    // inst 0: imm_16 (preamble or varying depending on preamble_n)
    bb->inst[0].op = op_imm_16;
    bb->inst[0].imm = 42;

    // remaining: add_i16 chaining
    for (int i = 1; i < n; i++) {
        bb->inst[i].op = op_add_i16;
        bb->inst[i].x = i-1;
        bb->inst[i].y = 0;
    }
    return bb;
}

static void free_bb(struct umbra_basic_block *bb) {
    free(bb->inst);
    free(bb);
}

static void test_basic_alloc_free(void) {
    // 4 registers in the pool, 3 values — no spills needed.
    static const int8_t pool[] = {10, 11, 12, 13};
    struct ra_config cfg = {
        .pool = pool, .nregs = 4, .max_reg = 16,
        .has_pairs = 0, .spill = test_spill, .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = make_bb(3, 0);
    struct ra *ra = ra_create(bb, &cfg);
    int sl[3] = {-1,-1,-1};
    int ns = 0;
    reset_records();

    // Allocate 3 registers — pool[0] is popped first (LIFO: pool order preserved)
    int8_t r0 = ra_alloc(ra, sl, &ns);
    int8_t r1 = ra_alloc(ra, sl, &ns);
    int8_t r2 = ra_alloc(ra, sl, &ns);

    (r0 == 10) here;
    (r1 == 11) here;
    (r2 == 12) here;
    (nspills == 0) here;

    // Free r1, reallocate — should get r1 back
    ra->owner[(int)r1] = 1;
    ra->reg[1] = r1;
    ra_free_reg(ra, 1);
    int8_t r3 = ra_alloc(ra, sl, &ns);
    (r3 == r1) here;
    (nspills == 0) here;

    ra_destroy(ra);
    free_bb(bb);
}

static void test_eviction_belady(void) {
    // 2 registers in pool, 5 values — forces eviction.
    // Values have different last_use: we expect Belady to evict the farthest.
    static const int8_t pool[] = {5, 6};
    struct ra_config cfg = {
        .pool = pool, .nregs = 2, .max_reg = 8,
        .has_pairs = 0, .spill = test_spill, .fill = test_fill, .ctx = 0,
    };
    // BB: 5 values, value 0 used at inst 4, value 1 used at inst 2.
    struct umbra_basic_block *bb = make_bb(5, 0);
    struct ra *ra = ra_create(bb, &cfg);
    int sl[5] = {-1,-1,-1,-1,-1};
    int ns = 0;
    reset_records();

    // Manually set last_use to control eviction behavior.
    ra->last_use[0] = 4;  // used far away
    ra->last_use[1] = 2;  // used soon

    // Allocate 2 registers and assign them.
    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra->reg[0] = r0; ra->owner[(int)r0] = 0;
    int8_t r1 = ra_alloc(ra, sl, &ns);
    ra->reg[1] = r1; ra->owner[(int)r1] = 1;

    // Pool is now empty. Next alloc should evict value 0 (farthest last_use=4).
    int8_t r2 = ra_alloc(ra, sl, &ns);
    (nspills == 1) here;
    (spills[0].reg == r0) here;  // value 0's register was spilled
    (r2 == r0) here;             // got back the same physical register
    (ra->reg[0] == -1) here;     // value 0 is no longer in a register
    (sl[0] >= 0) here;           // value 0 got a spill slot

    ra_destroy(ra);
    free_bb(bb);
}

static void test_dead_value_evicted_first(void) {
    // This is the exact bug we fixed: dead values (last_use=-1) should be evicted first.
    static const int8_t pool[] = {5, 6};
    struct ra_config cfg = {
        .pool = pool, .nregs = 2, .max_reg = 8,
        .has_pairs = 0, .spill = test_spill, .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = make_bb(5, 0);
    struct ra *ra = ra_create(bb, &cfg);
    int sl[5] = {-1,-1,-1,-1,-1};
    int ns = 0;
    reset_records();

    // Value 0: dead (last_use=-1)  Value 1: used at inst 3.
    ra->last_use[0] = -1;
    ra->last_use[1] = 3;

    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra->reg[0] = r0; ra->owner[(int)r0] = 0;
    int8_t r1 = ra_alloc(ra, sl, &ns);
    ra->reg[1] = r1; ra->owner[(int)r1] = 1;

    // Pool empty. Should evict the dead value 0, NOT the live value 1.
    int8_t r2 = ra_alloc(ra, sl, &ns);
    (nspills == 1) here;
    (spills[0].reg == r0) here;  // dead value's register was spilled
    (r2 == r0) here;
    (ra->reg[1] == r1) here;    // live value 1 is still in its register

    ra_destroy(ra);
    free_bb(bb);
}

static void test_ensure_and_fill(void) {
    static const int8_t pool[] = {5, 6};
    struct ra_config cfg = {
        .pool = pool, .nregs = 2, .max_reg = 8,
        .has_pairs = 0, .spill = test_spill, .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = make_bb(5, 0);
    struct ra *ra = ra_create(bb, &cfg);
    int sl[5] = {-1,-1,-1,-1,-1};
    int ns = 0;
    reset_records();

    ra->last_use[0] = 4;
    ra->last_use[1] = 2;
    ra->last_use[2] = 3;

    // Allocate and assign value 0 and 1.
    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra->reg[0] = r0; ra->owner[(int)r0] = 0;
    int8_t r1 = ra_alloc(ra, sl, &ns);
    ra->reg[1] = r1; ra->owner[(int)r1] = 1;

    // Allocate for value 2 — evicts value 0.
    int8_t r2 = ra_alloc(ra, sl, &ns);
    ra->reg[2] = r2; ra->owner[(int)r2] = 2;
    (nspills == 1) here;

    // Now ensure value 0 — should trigger fill from spill slot.
    int8_t r0b = ra_ensure(ra, sl, &ns, 0);
    (nfills == 1) here;
    (fills[0].slot == sl[0]) here;
    (r0b >= 0) here;
    (ra->reg[0] == r0b) here;
    (ra->owner[(int)r0b] == 0) here;

    // Ensure value 0 again — should be a no-op (already in register).
    int8_t r0c = ra_ensure(ra, sl, &ns, 0);
    (r0c == r0b) here;
    (nfills == 1) here;  // no additional fill

    ra_destroy(ra);
    free_bb(bb);
}

static void test_claim(void) {
    static const int8_t pool[] = {5, 6, 7};
    struct ra_config cfg = {
        .pool = pool, .nregs = 3, .max_reg = 8,
        .has_pairs = 0, .spill = test_spill, .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = make_bb(4, 0);
    struct ra *ra = ra_create(bb, &cfg);
    int sl[4] = {-1,-1,-1,-1};
    int ns = 0;
    reset_records();

    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra->reg[0] = r0; ra->owner[(int)r0] = 0;

    // Claim: transfer value 0's register to value 1.
    int8_t r1 = ra_claim(ra, 0, 1);
    (r1 == r0) here;
    (ra->reg[0] == -1) here;
    (ra->reg[1] == r0) here;
    (ra->owner[(int)r0] == 1) here;
    (nspills == 0) here;

    ra_destroy(ra);
    free_bb(bb);
}

static void test_pairs(void) {
    // Test ARM64-style pairs: OP_32 values in varying section get 2 registers.
    static const int8_t pool[] = {4, 5, 6, 7, 8, 9};
    struct ra_config cfg = {
        .pool = pool, .nregs = 6, .max_reg = 10,
        .has_pairs = 1, .spill = test_spill, .fill = test_fill, .ctx = 0,
    };
    // BB: 2 preamble + 3 varying, all OP_32 for pair behavior.
    struct umbra_basic_block *bb = malloc(sizeof *bb);
    bb->inst = calloc(5, sizeof *bb->inst);
    bb->insts = 5;
    bb->preamble = 2;
    bb->ht = 0;
    bb->ht_mask = 0;

    bb->inst[0].op = op_imm_32;
    bb->inst[0].imm = 1;
    bb->inst[1].op = op_imm_32;
    bb->inst[1].imm = 2;
    // Varying OP_32 ops (these should be pairs)
    bb->inst[2].op = op_add_f32; bb->inst[2].x = 0; bb->inst[2].y = 1;
    bb->inst[3].op = op_add_f32; bb->inst[3].x = 2; bb->inst[3].y = 0;
    bb->inst[4].op = op_add_f32; bb->inst[4].x = 3; bb->inst[4].y = 1;

    struct ra *ra = ra_create(bb, &cfg);
    int sl[5] = {-1,-1,-1,-1,-1};
    int ns = 0;
    reset_records();

    // Preamble values (inst 0, 1) should NOT be pairs.
    (ra->is_pair[0] == 0) here;
    (ra->is_pair[1] == 0) here;
    // Varying OP_32 values should be pairs.
    (ra->is_pair[2] == 1) here;
    (ra->is_pair[3] == 1) here;
    (ra->is_pair[4] == 1) here;

    // Allocate for a non-pair value: should get 1 register.
    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra->reg[0] = r0; ra->owner[(int)r0] = 0;
    (ra->nfree == 5) here;

    // Ensure a pair value that was never allocated — triggers alloc of 2 regs.
    ra->last_use[2] = 4;
    int8_t r2 = ra_ensure(ra, sl, &ns, 2);
    (r2 >= 0) here;
    (ra->reg[2] >= 0) here;
    (ra->reg_hi[2] >= 0) here;
    (ra->reg[2] != ra->reg_hi[2]) here;
    (ra->nfree == 3) here;  // 5 - 2 = 3

    // Free a pair: both registers return to pool.
    ra_free_reg(ra, 2);
    (ra->nfree == 5) here;  // 3 + 2 = 5
    (ra->reg[2] == -1) here;
    (ra->reg_hi[2] == -1) here;

    // Claim a pair: both registers transfer.
    int8_t r3lo = ra_alloc(ra, sl, &ns);
    int8_t r3hi = ra_alloc(ra, sl, &ns);
    ra->reg[3] = r3lo; ra->reg_hi[3] = r3hi;
    ra->owner[(int)r3lo] = 3; ra->owner[(int)r3hi] = 3;

    int8_t r4 = ra_claim(ra, 3, 4);
    (r4 == r3lo) here;
    (ra->reg[4] == r3lo) here;
    (ra->reg_hi[4] == r3hi) here;
    (ra->reg[3] == -1) here;
    (ra->reg_hi[3] == -1) here;
    (ra->owner[(int)r3lo] == 4) here;
    (ra->owner[(int)r3hi] == 4) here;

    ra_destroy(ra);
    free(bb->inst);
    free(bb);
}

static void test_pair_spill_fill(void) {
    // When a pair is evicted, both lo and hi should be spilled with consecutive slots.
    static const int8_t pool[] = {4, 5, 6, 7};
    struct ra_config cfg = {
        .pool = pool, .nregs = 4, .max_reg = 8,
        .has_pairs = 1, .spill = test_spill, .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = malloc(sizeof *bb);
    bb->inst = calloc(4, sizeof *bb->inst);
    bb->insts = 4;
    bb->preamble = 1;
    bb->ht = 0;
    bb->ht_mask = 0;

    bb->inst[0].op = op_imm_32;
    bb->inst[1].op = op_add_f32; bb->inst[1].x = 0; bb->inst[1].y = 0;
    bb->inst[2].op = op_add_f32; bb->inst[2].x = 1; bb->inst[2].y = 0;
    bb->inst[3].op = op_add_f32; bb->inst[3].x = 2; bb->inst[3].y = 0;

    struct ra *ra = ra_create(bb, &cfg);
    int sl[4] = {-1,-1,-1,-1};
    int ns = 0;
    reset_records();

    // Value 0 is preamble (not a pair), takes 1 reg.
    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra->reg[0] = r0; ra->owner[(int)r0] = 0;
    // 3 regs left. Value 1 is a varying pair, takes 2 regs.
    int8_t r1lo = ra_alloc(ra, sl, &ns);
    int8_t r1hi = ra_alloc(ra, sl, &ns);
    ra->reg[1] = r1lo; ra->reg_hi[1] = r1hi;
    ra->owner[(int)r1lo] = 1; ra->owner[(int)r1hi] = 1;

    ra->last_use[0] = 3;
    ra->last_use[1] = 3;

    // 1 reg left.
    (ra->nfree == 1) here;

    // Use the last free, then force eviction.
    int8_t r_last = ra_alloc(ra, sl, &ns);
    (ra->nfree == 0) here;
    (r_last >= 0) here;
    ra->free_stack[ra->nfree++] = r_last; // put it back
    ra->nfree--;                          // take it back out

    // Pool empty. Next alloc forces eviction.
    reset_records();
    int8_t r2 = ra_alloc(ra, sl, &ns);
    (nspills >= 1) here;
    (r2 >= 0) here;

    // If a pair was evicted, it used 2 consecutive spill slots.
    if (nspills == 2) {
        (spills[0].slot + 1 == spills[1].slot) here;
    }

    ra_destroy(ra);
    free(bb->inst);
    free(bb);
}

static void test_last_use_preamble(void) {
    // Preamble values used in varying section should have last_use = n (pinned).
    static const int8_t pool[] = {5, 6, 7};
    struct ra_config cfg = {
        .pool = pool, .nregs = 3, .max_reg = 8,
        .has_pairs = 0, .spill = test_spill, .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = make_bb(4, 2);
    // inst 0: imm_16 (preamble)
    // inst 1: add_i16 x=0 y=0 (preamble)
    // inst 2: add_i16 x=1 y=0 (varying, uses preamble values 0 and 1)
    // inst 3: add_i16 x=2 y=0 (varying, uses preamble value 0)
    struct ra *ra = ra_create(bb, &cfg);

    // Value 0 is preamble, used in varying (inst 2 and 3) → last_use should be n=4.
    (ra->last_use[0] == 4) here;
    // Value 1 is preamble, used in varying (inst 2) → last_use should be n=4.
    (ra->last_use[1] == 4) here;

    ra_destroy(ra);
    free_bb(bb);
}

static void test_many_values_stress(void) {
    // Stress test: many values, very few registers, forces lots of spills/fills.
    static const int8_t pool[] = {0, 1, 2};
    struct ra_config cfg = {
        .pool = pool, .nregs = 3, .max_reg = 4,
        .has_pairs = 0, .spill = test_spill, .fill = test_fill, .ctx = 0,
    };
    int n = 20;
    struct umbra_basic_block *bb = make_bb(n, 0);
    struct ra *ra = ra_create(bb, &cfg);
    int *sl = calloc((size_t)n, sizeof *sl);
    for (int i = 0; i < n; i++) sl[i] = -1;
    int ns = 0;
    reset_records();

    // Simulate: allocate a register for each value, assign ownership.
    for (int i = 0; i < n; i++) {
        int8_t r = ra_alloc(ra, sl, &ns);
        (r >= 0 && r < 4) here;
        ra->reg[i] = r;
        ra->owner[(int)r] = i;
    }

    // After 20 allocs with 3 regs, we should have had 17 spills.
    (nspills == 17) here;

    // All values should have a spill slot except the last 3 (still in regs).
    int in_reg = 0;
    for (int i = 0; i < n; i++) {
        if (ra->reg[i] >= 0) in_reg++;
    }
    (in_reg == 3) here;

    // Ensure every value — the 3 in regs are no-ops, the 17 spilled trigger fills.
    reset_records();
    for (int i = 0; i < n; i++) {
        ra_ensure(ra, sl, &ns, i);
    }
    // At least some fills should have happened.
    (nfills > 0) here;

    ra_destroy(ra);
    free(sl);
    free_bb(bb);
}

static void test_step_alloc(void) {
    // ra_step_alloc: allocate output register(s) for instruction i.
    static const int8_t pool[] = {5, 6, 7, 8};
    struct ra_config cfg = {
        .pool = pool, .nregs = 4, .max_reg = 10,
        .has_pairs = 0, .spill = test_spill, .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = make_bb(3, 0);
    struct ra *ra = ra_create(bb, &cfg);
    int sl[3] = {-1,-1,-1};
    int ns = 0;
    reset_records();

    struct ra_step s0 = ra_step_alloc(ra, sl, &ns, 0);
    (s0.rd >= 0) here;
    (ra->reg[0] == s0.rd) here;
    (ra->owner[(int)s0.rd] == 0) here;

    struct ra_step s1 = ra_step_alloc(ra, sl, &ns, 1);
    (s1.rd >= 0) here;
    (s1.rd != s0.rd) here;
    (ra->reg[1] == s1.rd) here;
    (nspills == 0) here;

    ra_destroy(ra);
    free_bb(bb);
}

static void test_step_alloc_pairs(void) {
    // ra_step_alloc with pairs: varying OP_32 values get 2 registers.
    static const int8_t pool[] = {4, 5, 6, 7};
    struct ra_config cfg = {
        .pool = pool, .nregs = 4, .max_reg = 8,
        .has_pairs = 1, .spill = test_spill, .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = malloc(sizeof *bb);
    bb->inst = calloc(3, sizeof *bb->inst);
    bb->insts = 3; bb->preamble = 1;
    bb->ht = 0; bb->ht_mask = 0;
    bb->inst[0].op = op_imm_32; bb->inst[0].imm = 1;
    bb->inst[1].op = op_add_f32; bb->inst[1].x = 0; bb->inst[1].y = 0;
    bb->inst[2].op = op_add_f32; bb->inst[2].x = 1; bb->inst[2].y = 0;

    struct ra *ra = ra_create(bb, &cfg);
    int sl[3] = {-1,-1,-1};
    int ns = 0;
    reset_records();

    // Preamble value: single register.
    struct ra_step s0 = ra_step_alloc(ra, sl, &ns, 0);
    (s0.rd >= 0) here;
    (ra->is_pair[0] == 0) here;

    // Varying OP_32 value: pair.
    struct ra_step s1 = ra_step_alloc(ra, sl, &ns, 1);
    (s1.rd >= 0) here;
    (s1.rdh >= 0) here;
    (s1.rd != s1.rdh) here;
    (ra->reg[1] == s1.rd) here;
    (ra->reg_hi[1] == s1.rdh) here;

    ra_destroy(ra);
    free(bb->inst); free(bb);
}

static void test_step_unary(void) {
    // ra_step_unary: ensure input, claim if dead, else alloc.
    static const int8_t pool[] = {5, 6, 7, 8};
    struct ra_config cfg = {
        .pool = pool, .nregs = 4, .max_reg = 10,
        .has_pairs = 0, .spill = test_spill, .fill = test_fill, .ctx = 0,
    };
    // BB: inst 0 = imm_16, inst 1 = i32_from_i16(v0) — unary conversion.
    struct umbra_basic_block *bb = malloc(sizeof *bb);
    bb->inst = calloc(2, sizeof *bb->inst);
    bb->insts = 2; bb->preamble = 0;
    bb->ht = 0; bb->ht_mask = 0;
    bb->inst[0].op = op_imm_16; bb->inst[0].imm = 42;
    bb->inst[1].op = op_i32_from_i16; bb->inst[1].x = 0;

    struct ra *ra = ra_create(bb, &cfg);
    int sl[2] = {-1,-1};
    int ns = 0;
    reset_records();

    // Allocate and assign value 0.
    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra->reg[0] = r0; ra->owner[(int)r0] = 0;

    // Case 1: x is dead at inst 1 → should claim x's register.
    ra->last_use[0] = 1;
    struct ra_step s = ra_step_unary(ra, sl, &ns, &bb->inst[1], 1, 0);
    (s.rx == r0) here;  // ensured input
    (s.rd == r0) here;  // claimed (dead input reused)
    (ra->reg[1] == s.rd) here;
    (ra->reg[0] == -1) here;
    (nspills == 0) here;

    ra_destroy(ra);
    free(bb->inst); free(bb);
}

static void test_step_unary_alive(void) {
    // ra_step_unary: input still alive → must alloc new register.
    static const int8_t pool[] = {5, 6, 7, 8};
    struct ra_config cfg = {
        .pool = pool, .nregs = 4, .max_reg = 10,
        .has_pairs = 0, .spill = test_spill, .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = malloc(sizeof *bb);
    bb->inst = calloc(3, sizeof *bb->inst);
    bb->insts = 3; bb->preamble = 0;
    bb->ht = 0; bb->ht_mask = 0;
    bb->inst[0].op = op_imm_16; bb->inst[0].imm = 42;
    bb->inst[1].op = op_i32_from_i16; bb->inst[1].x = 0;
    bb->inst[2].op = op_add_i16; bb->inst[2].x = 0; bb->inst[2].y = 0;

    struct ra *ra = ra_create(bb, &cfg);
    int sl[3] = {-1,-1,-1};
    int ns = 0;
    reset_records();

    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra->reg[0] = r0; ra->owner[(int)r0] = 0;

    // x still alive at inst 2 → rd must be a different register.
    ra->last_use[0] = 2;
    struct ra_step s = ra_step_unary(ra, sl, &ns, &bb->inst[1], 1, 0);
    (s.rx == r0) here;
    (s.rd != r0) here;  // allocated new (input still alive)
    (ra->reg[0] == r0) here;  // original still mapped
    (ra->reg[1] == s.rd) here;

    ra_destroy(ra);
    free(bb->inst); free(bb);
}

static void test_step_alu(void) {
    // ra_step_alu: ensure two inputs, dead analysis, claim/alloc rd.
    static const int8_t pool[] = {5, 6, 7, 8};
    struct ra_config cfg = {
        .pool = pool, .nregs = 4, .max_reg = 10,
        .has_pairs = 0, .spill = test_spill, .fill = test_fill, .ctx = 0,
    };
    // BB: inst 0 = imm_16(1), inst 1 = imm_16(2), inst 2 = add_i16(v0, v1)
    struct umbra_basic_block *bb = malloc(sizeof *bb);
    bb->inst = calloc(3, sizeof *bb->inst);
    bb->insts = 3; bb->preamble = 0;
    bb->ht = 0; bb->ht_mask = 0;
    bb->inst[0].op = op_imm_16; bb->inst[0].imm = 1;
    bb->inst[1].op = op_imm_16; bb->inst[1].imm = 2;
    bb->inst[2].op = op_add_i16; bb->inst[2].x = 0; bb->inst[2].y = 1;

    struct ra *ra = ra_create(bb, &cfg);
    int sl[3] = {-1,-1,-1};
    int ns = 0;
    reset_records();

    // Allocate inputs.
    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra->reg[0] = r0; ra->owner[(int)r0] = 0;
    int8_t r1 = ra_alloc(ra, sl, &ns);
    ra->reg[1] = r1; ra->owner[(int)r1] = 1;

    // Both inputs dead at inst 2 → rd should claim one of them.
    ra->last_use[0] = 2;
    ra->last_use[1] = 2;
    struct ra_step s = ra_step_alu(ra, sl, &ns, &bb->inst[2], 2, 0, 0);
    (s.rx == r0) here;
    (s.ry == r1) here;
    (s.rd >= 0) here;
    (s.rd == r0 || s.rd == r1) here;  // claimed from dead input
    (s.scratch < 0) here;  // no arch_scratch requested

    ra_destroy(ra);
    free(bb->inst); free(bb);
}

static void test_step_alu_scratch(void) {
    // ra_step_alu with arch_scratch=1: should allocate a scratch register.
    static const int8_t pool[] = {5, 6, 7, 8};
    struct ra_config cfg = {
        .pool = pool, .nregs = 4, .max_reg = 10,
        .has_pairs = 0, .spill = test_spill, .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = malloc(sizeof *bb);
    bb->inst = calloc(3, sizeof *bb->inst);
    bb->insts = 3; bb->preamble = 0;
    bb->ht = 0; bb->ht_mask = 0;
    bb->inst[0].op = op_imm_16; bb->inst[0].imm = 1;
    bb->inst[1].op = op_imm_16; bb->inst[1].imm = 2;
    bb->inst[2].op = op_add_i16; bb->inst[2].x = 0; bb->inst[2].y = 1;

    struct ra *ra = ra_create(bb, &cfg);
    int sl[3] = {-1,-1,-1};
    int ns = 0;
    reset_records();

    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra->reg[0] = r0; ra->owner[(int)r0] = 0;
    int8_t r1 = ra_alloc(ra, sl, &ns);
    ra->reg[1] = r1; ra->owner[(int)r1] = 1;

    ra->last_use[0] = 2;
    ra->last_use[1] = 2;
    struct ra_step s = ra_step_alu(ra, sl, &ns, &bb->inst[2], 2, 0, 1);
    (s.rd >= 0) here;
    (s.scratch >= 0) here;  // scratch was allocated
    (s.scratch != s.rd) here;
    (s.scratch != s.rx) here;
    (s.scratch != s.ry) here;

    ra_destroy(ra);
    free(bb->inst); free(bb);
}

int main(void) {
    test_basic_alloc_free();
    test_eviction_belady();
    test_dead_value_evicted_first();
    test_ensure_and_fill();
    test_claim();
    test_pairs();
    test_pair_spill_fill();
    test_last_use_preamble();
    test_many_values_stress();
    test_step_alloc();
    test_step_alloc_pairs();
    test_step_unary();
    test_step_unary_alive();
    test_step_alu();
    test_step_alu_scratch();
    return 0;
}
