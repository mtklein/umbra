#include "../src/ra.h"
#include "test.h"
#include <stdlib.h>
#include <string.h>

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

static struct umbra_basic_block* make_bb(int n, int pre) {
    struct umbra_basic_block *bb = malloc(sizeof *bb);
    bb->inst = calloc((size_t)n, sizeof *bb->inst);
    bb->insts = n;
    bb->preamble = pre;

    bb->inst[0].op = op_imm_32;
    bb->inst[0].imm = 42;

    for (int i = 1; i < n; i++) {
        bb->inst[i].op = op_add_i32;
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
    static const int8_t pool[] = {10, 11, 12, 13};
    struct ra_config cfg = {
        .pool = pool, .nregs = 4, .max_reg = 16,
        .has_pairs = 0,
        .spill = test_spill,
        .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = make_bb(3, 0);
    struct ra *ra = ra_create(bb, &cfg);
    int sl[3] = {-1,-1,-1};
    int ns = 0;
    reset_records();

    // pool[0] popped first (LIFO)
    int8_t r0 = ra_alloc(ra, sl, &ns);
    int8_t r1 = ra_alloc(ra, sl, &ns);
    int8_t r2 = ra_alloc(ra, sl, &ns);

    (r0 == 10) here;
    (r1 == 11) here;
    (r2 == 12) here;
    (nspills == 0) here;

    ra_assign(ra, 1, r1);
    ra_free_reg(ra, 1);
    int8_t r3 = ra_alloc(ra, sl, &ns);
    (r3 == r1) here;
    (nspills == 0) here;

    ra_destroy(ra);
    free_bb(bb);
}

static void test_eviction_belady(void) {
    static const int8_t pool[] = {5, 6};
    struct ra_config cfg = {
        .pool = pool, .nregs = 2, .max_reg = 8,
        .has_pairs = 0,
        .spill = test_spill,
        .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = make_bb(5, 0);
    struct ra *ra = ra_create(bb, &cfg);
    int sl[5] = {-1,-1,-1,-1,-1};
    int ns = 0;
    reset_records();

    ra_set_last_use(ra, 0, 4);
    ra_set_last_use(ra, 1, 2);

    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 0, r0);
    int8_t r1 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 1, r1);

    // evicts value 0 (farthest last_use=4)
    int8_t r2 = ra_alloc(ra, sl, &ns);
    (nspills == 1) here;
    (spills[0].reg == r0) here;
    (r2 == r0) here;
    (ra_reg(ra, 0) == -1) here;
    (sl[0] >= 0) here;

    ra_destroy(ra);
    free_bb(bb);
}

static void test_dead_value_evicted_first(void) {
    // dead values (last_use=-1) evicted first
    static const int8_t pool[] = {5, 6};
    struct ra_config cfg = {
        .pool = pool, .nregs = 2, .max_reg = 8,
        .has_pairs = 0,
        .spill = test_spill,
        .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = make_bb(5, 0);
    struct ra *ra = ra_create(bb, &cfg);
    int sl[5] = {-1,-1,-1,-1,-1};
    int ns = 0;
    reset_records();

    ra_set_last_use(ra, 0, -1);
    ra_set_last_use(ra, 1, 3);

    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 0, r0);
    int8_t r1 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 1, r1);

    int8_t r2 = ra_alloc(ra, sl, &ns);
    (nspills == 1) here;
    (spills[0].reg == r0) here;
    (r2 == r0) here;
    (ra_reg(ra, 1) == r1) here;

    ra_destroy(ra);
    free_bb(bb);
}

static void test_ensure_and_fill(void) {
    static const int8_t pool[] = {5, 6};
    struct ra_config cfg = {
        .pool = pool, .nregs = 2, .max_reg = 8,
        .has_pairs = 0,
        .spill = test_spill,
        .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = make_bb(5, 0);
    struct ra *ra = ra_create(bb, &cfg);
    int sl[5] = {-1,-1,-1,-1,-1};
    int ns = 0;
    reset_records();

    ra_set_last_use(ra, 0, 4);
    ra_set_last_use(ra, 1, 2);
    ra_set_last_use(ra, 2, 3);

    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 0, r0);
    int8_t r1 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 1, r1);

    int8_t r2 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 2, r2);
    (nspills == 1) here;

    int8_t r0b = ra_ensure(ra, sl, &ns, 0);
    (nfills == 1) here;
    (fills[0].slot == sl[0]) here;
    (r0b >= 0) here;
    (ra_reg(ra, 0) == r0b) here;

    int8_t r0c = ra_ensure(ra, sl, &ns, 0);
    (r0c == r0b) here;
    (nfills == 1) here;

    ra_destroy(ra);
    free_bb(bb);
}

static void test_claim(void) {
    static const int8_t pool[] = {5, 6, 7};
    struct ra_config cfg = {
        .pool = pool, .nregs = 3, .max_reg = 8,
        .has_pairs = 0,
        .spill = test_spill,
        .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = make_bb(4, 0);
    struct ra *ra = ra_create(bb, &cfg);
    int sl[4] = {-1,-1,-1,-1};
    int ns = 0;
    reset_records();

    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 0, r0);

    int8_t r1 = ra_claim(ra, 0, 1);
    (r1 == r0) here;
    (ra_reg(ra, 0) == -1) here;
    (ra_reg(ra, 1) == r0) here;
    (nspills == 0) here;

    ra_destroy(ra);
    free_bb(bb);
}

static void test_pairs(void) {
    static const int8_t pool[] = {4, 5, 6, 7, 8, 9};
    struct ra_config cfg = {
        .pool = pool, .nregs = 6, .max_reg = 10,
        .has_pairs = 1,
        .spill = test_spill,
        .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = malloc(sizeof *bb);
    bb->inst = calloc(5, sizeof *bb->inst);
    bb->insts = 5;
    bb->preamble = 2;

    bb->inst[0].op = op_imm_32;
    bb->inst[0].imm = 1;
    bb->inst[1].op = op_imm_32;
    bb->inst[1].imm = 2;
    bb->inst[2].op = op_add_f32;
    bb->inst[2].x = 0; bb->inst[2].y = 1;
    bb->inst[3].op = op_add_f32;
    bb->inst[3].x = 2; bb->inst[3].y = 0;
    bb->inst[4].op = op_add_f32;
    bb->inst[4].x = 3; bb->inst[4].y = 1;

    struct ra *ra = ra_create(bb, &cfg);
    int sl[5] = {-1,-1,-1,-1,-1};
    int ns = 0;
    reset_records();

    (ra_is_pair(ra, 0) == 0) here;
    (ra_is_pair(ra, 1) == 0) here;
    (ra_is_pair(ra, 2) == 1) here;
    (ra_is_pair(ra, 3) == 1) here;
    (ra_is_pair(ra, 4) == 1) here;

    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 0, r0);

    // pair alloc: 2 regs
    ra_set_last_use(ra, 2, 4);
    int8_t r2 = ra_ensure(ra, sl, &ns, 2);
    (r2 >= 0) here;
    (ra_reg(ra, 2) >= 0) here;
    (ra_reg_hi(ra, 2) >= 0) here;
    (ra_reg(ra, 2) != ra_reg_hi(ra, 2)) here;

    ra_free_reg(ra, 2);
    (ra_reg(ra, 2) == -1) here;
    (ra_reg_hi(ra, 2) == -1) here;

    int8_t r3lo = ra_alloc(ra, sl, &ns);
    int8_t r3hi = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 3, r3lo);
    ra_assign_hi(ra, 3, r3hi);

    int8_t r4 = ra_claim(ra, 3, 4);
    (r4 == r3lo) here;
    (ra_reg(ra, 4) == r3lo) here;
    (ra_reg_hi(ra, 4) == r3hi) here;
    (ra_reg(ra, 3) == -1) here;
    (ra_reg_hi(ra, 3) == -1) here;

    ra_destroy(ra);
    free(bb->inst);
    free(bb);
}

static void test_pair_spill_fill(void) {
    // evicting a pair spills both lo and hi
    static const int8_t pool[] = {4, 5, 6, 7};
    struct ra_config cfg = {
        .pool = pool, .nregs = 4, .max_reg = 8,
        .has_pairs = 1,
        .spill = test_spill,
        .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = malloc(sizeof *bb);
    bb->inst = calloc(4, sizeof *bb->inst);
    bb->insts = 4;
    bb->preamble = 1;

    bb->inst[0].op = op_imm_32;
    bb->inst[1].op = op_add_f32;
    bb->inst[1].x = 0; bb->inst[1].y = 0;
    bb->inst[2].op = op_add_f32;
    bb->inst[2].x = 1; bb->inst[2].y = 0;
    bb->inst[3].op = op_add_f32;
    bb->inst[3].x = 2; bb->inst[3].y = 0;

    struct ra *ra = ra_create(bb, &cfg);
    int sl[4] = {-1,-1,-1,-1};
    int ns = 0;
    reset_records();

    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 0, r0);
    int8_t r1lo = ra_alloc(ra, sl, &ns);
    int8_t r1hi = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 1, r1lo);
    ra_assign_hi(ra, 1, r1hi);

    ra_set_last_use(ra, 0, 3);
    ra_set_last_use(ra, 1, 3);

    int8_t r_last = ra_alloc(ra, sl, &ns);
    ra_return_reg(ra, r_last);

    (void)ra_alloc(ra, sl, &ns);

    reset_records();
    int8_t r2 = ra_alloc(ra, sl, &ns);
    (nspills >= 1) here;
    (r2 >= 0) here;

    if (nspills == 2) {
        (spills[0].slot + 1 == spills[1].slot) here;
    }

    ra_destroy(ra);
    free(bb->inst);
    free(bb);
}

static void test_last_use_preamble(void) {
    // preamble used in varying: last_use = n
    static const int8_t pool[] = {5, 6, 7};
    struct ra_config cfg = {
        .pool = pool, .nregs = 3, .max_reg = 8,
        .has_pairs = 0,
        .spill = test_spill,
        .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = make_bb(4, 2);
    struct ra *ra = ra_create(bb, &cfg);

    (ra_last_use(ra, 0) == 4) here;
    (ra_last_use(ra, 1) == 4) here;

    ra_destroy(ra);
    free_bb(bb);
}

static void test_many_values_stress(void) {
    // many values, few registers, lots of spills
    static const int8_t pool[] = {0, 1, 2};
    struct ra_config cfg = {
        .pool = pool, .nregs = 3, .max_reg = 4,
        .has_pairs = 0,
        .spill = test_spill,
        .fill = test_fill, .ctx = 0,
    };
    int n = 20;
    struct umbra_basic_block *bb = make_bb(n, 0);
    struct ra *ra = ra_create(bb, &cfg);
    int *sl = calloc((size_t)n, sizeof *sl);
    for (int i = 0; i < n; i++) { sl[i] = -1; }
    int ns = 0;
    reset_records();

    for (int i = 0; i < n; i++) {
        int8_t r = ra_alloc(ra, sl, &ns);
        (r >= 0 && r < 4) here;
        ra_assign(ra, i, r);
    }

    (nspills == 17) here;

    int in_reg = 0;
    for (int i = 0; i < n; i++) {
        if (ra_reg(ra, i) >= 0) { in_reg++; }
    }
    (in_reg == 3) here;

    // ensure all: 17 spilled trigger fills
    reset_records();
    for (int i = 0; i < n; i++) {
        ra_ensure(ra, sl, &ns, i);
    }
    (nfills > 0) here;

    ra_destroy(ra);
    free(sl);
    free_bb(bb);
}

static void test_step_alloc(void) {
    static const int8_t pool[] = {5, 6, 7, 8};
    struct ra_config cfg = {
        .pool = pool, .nregs = 4, .max_reg = 10,
        .has_pairs = 0,
        .spill = test_spill,
        .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = make_bb(3, 0);
    struct ra *ra = ra_create(bb, &cfg);
    int sl[3] = {-1,-1,-1};
    int ns = 0;
    reset_records();

    struct ra_step s0 = ra_step_alloc(ra, sl, &ns, 0);
    (s0.rd >= 0) here;
    (ra_reg(ra, 0) == s0.rd) here;

    struct ra_step s1 = ra_step_alloc(ra, sl, &ns, 1);
    (s1.rd >= 0) here;
    (s1.rd != s0.rd) here;
    (ra_reg(ra, 1) == s1.rd) here;
    (nspills == 0) here;

    ra_destroy(ra);
    free_bb(bb);
}

static void test_step_alloc_pairs(void) {
    static const int8_t pool[] = {4, 5, 6, 7};
    struct ra_config cfg = {
        .pool = pool, .nregs = 4, .max_reg = 8,
        .has_pairs = 1,
        .spill = test_spill,
        .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = malloc(sizeof *bb);
    bb->inst = calloc(3, sizeof *bb->inst);
    bb->insts = 3; bb->preamble = 1;

    bb->inst[0].op = op_imm_32;
    bb->inst[0].imm = 1;
    bb->inst[1].op = op_add_f32;
    bb->inst[1].x = 0; bb->inst[1].y = 0;
    bb->inst[2].op = op_add_f32;
    bb->inst[2].x = 1; bb->inst[2].y = 0;

    struct ra *ra = ra_create(bb, &cfg);
    int sl[3] = {-1,-1,-1};
    int ns = 0;
    reset_records();

    struct ra_step s0 =
        ra_step_alloc(ra, sl, &ns, 0);
    (s0.rd >= 0) here;
    (ra_is_pair(ra, 0) == 0) here;

    struct ra_step s1 =
        ra_step_alloc(ra, sl, &ns, 1);
    (s1.rd >= 0) here;
    (s1.rdh >= 0) here;
    (s1.rd != s1.rdh) here;
    (ra_reg(ra, 1) == s1.rd) here;
    (ra_reg_hi(ra, 1) == s1.rdh) here;

    ra_destroy(ra);
    free(bb->inst); free(bb);
}

static void test_step_unary(void) {
    static const int8_t pool[] = {5, 6, 7, 8};
    struct ra_config cfg = {
        .pool = pool, .nregs = 4, .max_reg = 10,
        .has_pairs = 0,
        .spill = test_spill,
        .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = malloc(sizeof *bb);
    bb->inst = calloc(2, sizeof *bb->inst);
    bb->insts = 2; bb->preamble = 0;

    bb->inst[0].op = op_imm_32;
    bb->inst[0].imm = 42;
    bb->inst[1].op = op_f32_from_i32;
    bb->inst[1].x = 0;

    struct ra *ra = ra_create(bb, &cfg);
    int sl[2] = {-1,-1};
    int ns = 0;
    reset_records();

    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 0, r0);

    // x dead at inst 1: claim x's register
    ra_set_last_use(ra, 0, 1);
    struct ra_step s =
        ra_step_unary(ra, sl, &ns,
                      &bb->inst[1], 1, 0);
    (s.rx == r0) here;
    (s.rd == r0) here;
    (ra_reg(ra, 1) == s.rd) here;
    (ra_reg(ra, 0) == -1) here;
    (nspills == 0) here;

    ra_destroy(ra);
    free(bb->inst); free(bb);
}

static void test_step_unary_alive(void) {
    static const int8_t pool[] = {5, 6, 7, 8};
    struct ra_config cfg = {
        .pool = pool, .nregs = 4, .max_reg = 10,
        .has_pairs = 0,
        .spill = test_spill,
        .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = malloc(sizeof *bb);
    bb->inst = calloc(3, sizeof *bb->inst);
    bb->insts = 3; bb->preamble = 0;

    bb->inst[0].op = op_imm_32;
    bb->inst[0].imm = 42;
    bb->inst[1].op = op_f32_from_i32;
    bb->inst[1].x = 0;
    bb->inst[2].op = op_add_i32;
    bb->inst[2].x = 0; bb->inst[2].y = 0;

    struct ra *ra = ra_create(bb, &cfg);
    int sl[3] = {-1,-1,-1};
    int ns = 0;
    reset_records();

    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 0, r0);

    // x still alive: rd must differ
    ra_set_last_use(ra, 0, 2);
    struct ra_step s =
        ra_step_unary(ra, sl, &ns,
                      &bb->inst[1], 1, 0);
    (s.rx == r0) here;
    (s.rd != r0) here;
    (ra_reg(ra, 0) == r0) here;
    (ra_reg(ra, 1) == s.rd) here;

    ra_destroy(ra);
    free(bb->inst); free(bb);
}

static void test_step_alu(void) {
    static const int8_t pool[] = {5, 6, 7, 8};
    struct ra_config cfg = {
        .pool = pool, .nregs = 4, .max_reg = 10,
        .has_pairs = 0,
        .spill = test_spill,
        .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = malloc(sizeof *bb);
    bb->inst = calloc(3, sizeof *bb->inst);
    bb->insts = 3; bb->preamble = 0;

    bb->inst[0].op = op_imm_32;
    bb->inst[0].imm = 1;
    bb->inst[1].op = op_imm_32;
    bb->inst[1].imm = 2;
    bb->inst[2].op = op_add_i32;
    bb->inst[2].x = 0; bb->inst[2].y = 1;

    struct ra *ra = ra_create(bb, &cfg);
    int sl[3] = {-1,-1,-1};
    int ns = 0;
    reset_records();

    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 0, r0);
    int8_t r1 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 1, r1);

    // both dead: rd claims one
    ra_set_last_use(ra, 0, 2);
    ra_set_last_use(ra, 1, 2);
    struct ra_step s =
        ra_step_alu(ra, sl, &ns,
                    &bb->inst[2], 2, 0, 0);
    (s.rx == r0) here;
    (s.ry == r1) here;
    (s.rd >= 0) here;
    (s.rd == r0 || s.rd == r1) here;
    (s.scratch < 0) here;

    ra_destroy(ra);
    free(bb->inst); free(bb);
}

static void test_step_alu_scratch(void) {
    static const int8_t pool[] = {5, 6, 7, 8};
    struct ra_config cfg = {
        .pool = pool, .nregs = 4, .max_reg = 10,
        .has_pairs = 0,
        .spill = test_spill,
        .fill = test_fill, .ctx = 0,
    };
    struct umbra_basic_block *bb = malloc(sizeof *bb);
    bb->inst = calloc(3, sizeof *bb->inst);
    bb->insts = 3; bb->preamble = 0;

    bb->inst[0].op = op_imm_32;
    bb->inst[0].imm = 1;
    bb->inst[1].op = op_imm_32;
    bb->inst[1].imm = 2;
    bb->inst[2].op = op_add_i32;
    bb->inst[2].x = 0; bb->inst[2].y = 1;

    struct ra *ra = ra_create(bb, &cfg);
    int sl[3] = {-1,-1,-1};
    int ns = 0;
    reset_records();

    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 0, r0);
    int8_t r1 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 1, r1);

    ra_set_last_use(ra, 0, 2);
    ra_set_last_use(ra, 1, 2);
    struct ra_step s =
        ra_step_alu(ra, sl, &ns,
                    &bb->inst[2], 2, 0, 1);
    (s.rd >= 0) here;
    (s.scratch >= 0) here;
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
