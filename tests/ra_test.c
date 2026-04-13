#include "../src/ra.h"
#include "../src/flat_ir.h"
#include "test.h"
#include <stdlib.h>
#include <string.h>

struct spill_record {
    int reg, slot;
};
static struct spill_record spills[256], fills[256];
static int                 nspills, nfills;

static void test_spill(int reg, int slot, void *ctx) {
    (void)ctx;
    spills[nspills++] = (struct spill_record){reg, slot};
}
static void test_fill(int reg, int slot, void *ctx) {
    (void)ctx;
    fills[nfills++] = (struct spill_record){reg, slot};
}

static void reset_records(void) { nspills = nfills = 0; }

static struct umbra_flat_ir *make_bb(int n, int pre) {
    struct umbra_flat_ir *bb = malloc(sizeof *bb);
    bb->inst = calloc((size_t)n, sizeof *bb->inst);
    bb->insts = n;
    bb->preamble = pre;
    bb->loop_begin = -1;
    bb->loop_end   = -1;
    bb->n_vars     = 0;

    bb->inst[0].op = op_imm_32;
    bb->inst[0].imm = 42;

    for (int i = 1; i < n; i++) {
        bb->inst[i].op = op_add_i32;
        bb->inst[i].x = (val){.id = i - 1};
        bb->inst[i].y = (val){0};
    }
    return bb;
}

static void free_bb(struct umbra_flat_ir *bb) {
    free(bb->inst);
    free(bb);
}

TEST(test_basic_alloc_free) {
    static int8_t const pool[] = {10, 11, 12, 13};
    struct ra_config    cfg = {
        .pool = pool,
        .nregs = 4,
        .max_reg = 16,

        .spill = test_spill,
        .fill = test_fill,
        .ctx = 0,
    };
    struct umbra_flat_ir *bb = make_bb(3, 0);
    struct ra                *ra = ra_create(bb, &cfg);
    int                       sl[3] = {-1, -1, -1};
    int                       ns = 0;
    reset_records();

    // pool[0] popped first (LIFO)
    int8_t r0 = ra_alloc(ra, sl, &ns);
    int8_t r1 = ra_alloc(ra, sl, &ns);
    int8_t r2 = ra_alloc(ra, sl, &ns);

    r0 == 10 here;
    r1 == 11 here;
    r2 == 12 here;
    nspills == 0 here;

    ra_assign(ra, 1, r1);
    ra_free_reg(ra, 1);
    int8_t r3 = ra_alloc(ra, sl, &ns);
    r3 == r1 here;
    nspills == 0 here;

    ra_destroy(ra);
    free_bb(bb);
}

TEST(test_eviction_belady) {
    static int8_t const pool[] = {5, 6};
    struct ra_config    cfg = {
        .pool = pool,
        .nregs = 2,
        .max_reg = 8,

        .spill = test_spill,
        .fill = test_fill,
        .ctx = 0,
    };
    struct umbra_flat_ir *bb = make_bb(5, 0);
    struct ra                *ra = ra_create(bb, &cfg);
    int                       sl[5] = {-1, -1, -1, -1, -1};
    int                       ns = 0;
    reset_records();

    ra_set_last_use(ra, 0, 4);
    ra_set_last_use(ra, 1, 2);

    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 0, r0);
    int8_t r1 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 1, r1);

    // evicts value 0 (farthest last_use=4)
    int8_t r2 = ra_alloc(ra, sl, &ns);
    nspills == 1 here;
    spills[0].reg == r0 here;
    r2 == r0 here;
    ra_reg(ra, 0) == -1 here;
    sl[0] >= 0 here;

    ra_destroy(ra);
    free_bb(bb);
}

TEST(test_dead_value_evicted_first) {
    // dead values (last_use=-1) evicted first
    static int8_t const pool[] = {5, 6};
    struct ra_config    cfg = {
        .pool = pool,
        .nregs = 2,
        .max_reg = 8,

        .spill = test_spill,
        .fill = test_fill,
        .ctx = 0,
    };
    struct umbra_flat_ir *bb = make_bb(5, 0);
    struct ra                *ra = ra_create(bb, &cfg);
    int                       sl[5] = {-1, -1, -1, -1, -1};
    int                       ns = 0;
    reset_records();

    ra_set_last_use(ra, 0, -1);
    ra_set_last_use(ra, 1, 3);

    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 0, r0);
    int8_t r1 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 1, r1);

    int8_t r2 = ra_alloc(ra, sl, &ns);
    nspills == 1 here;
    spills[0].reg == r0 here;
    r2 == r0 here;
    ra_reg(ra, 1) == r1 here;

    ra_destroy(ra);
    free_bb(bb);
}

TEST(test_ensure_and_fill) {
    static int8_t const pool[] = {5, 6};
    struct ra_config    cfg = {
        .pool = pool,
        .nregs = 2,
        .max_reg = 8,

        .spill = test_spill,
        .fill = test_fill,
        .ctx = 0,
    };
    struct umbra_flat_ir *bb = make_bb(5, 0);
    struct ra                *ra = ra_create(bb, &cfg);
    int                       sl[5] = {-1, -1, -1, -1, -1};
    int                       ns = 0;
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
    nspills == 1 here;

    int8_t r0b = ra_ensure(ra, sl, &ns, 0);
    nfills == 1 here;
    fills[0].slot == sl[0] here;
    r0b >= 0 here;
    ra_reg(ra, 0) == r0b here;

    int8_t r0c = ra_ensure(ra, sl, &ns, 0);
    r0c == r0b here;
    nfills == 1 here;

    ra_destroy(ra);
    free_bb(bb);
}

TEST(test_claim) {
    static int8_t const pool[] = {5, 6, 7};
    struct ra_config    cfg = {
        .pool = pool,
        .nregs = 3,
        .max_reg = 8,

        .spill = test_spill,
        .fill = test_fill,
        .ctx = 0,
    };
    struct umbra_flat_ir *bb = make_bb(4, 0);
    struct ra                *ra = ra_create(bb, &cfg);
    int                       sl[4] = {-1, -1, -1, -1};
    int                       ns = 0;
    reset_records();

    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 0, r0);

    int8_t r1 = ra_claim(ra, 0, 1);
    r1 == r0 here;
    ra_reg(ra, 0) == -1 here;
    ra_reg(ra, 1) == r0 here;
    nspills == 0 here;

    ra_destroy(ra);
    free_bb(bb);
}

TEST(test_last_use_preamble) {
    // preamble used in varying: last_use = n
    static int8_t const pool[] = {5, 6, 7};
    struct ra_config    cfg = {
        .pool = pool,
        .nregs = 3,
        .max_reg = 8,

        .spill = test_spill,
        .fill = test_fill,
        .ctx = 0,
    };
    struct umbra_flat_ir *bb = make_bb(4, 2);
    struct ra                *ra = ra_create(bb, &cfg);

    ra_last_use(ra, 0) == 4 here;
    ra_last_use(ra, 1) == 4 here;

    ra_destroy(ra);
    free_bb(bb);
}

TEST(test_many_values_stress) {
    // many values, few registers, lots of spills
    static int8_t const pool[] = {0, 1, 2};
    struct ra_config    cfg = {
        .pool = pool,
        .nregs = 3,
        .max_reg = 4,

        .spill = test_spill,
        .fill = test_fill,
        .ctx = 0,
    };
    int                       n = 20;
    struct umbra_flat_ir *bb = make_bb(n, 0);
    struct ra                *ra = ra_create(bb, &cfg);
    int                      *sl = calloc((size_t)n, sizeof *sl);
    for (int i = 0; i < n; i++) { sl[i] = -1; }
    int ns = 0;
    reset_records();

    for (int i = 0; i < n; i++) {
        int8_t r = ra_alloc(ra, sl, &ns);
        r >= 0 && r < 4 here;
        ra_assign(ra, i, r);
    }

    nspills == 17 here;

    int in_reg = 0;
    for (int i = 0; i < n; i++) {
        if (ra_reg(ra, i) >= 0) { in_reg++; }
    }
    in_reg == 3 here;

    // ensure all: 17 spilled trigger fills
    reset_records();
    for (int i = 0; i < n; i++) { ra_ensure(ra, sl, &ns, i); }
    nfills > 0 here;

    ra_destroy(ra);
    free(sl);
    free_bb(bb);
}

TEST(test_step_alloc) {
    static int8_t const pool[] = {5, 6, 7, 8};
    struct ra_config    cfg = {
        .pool = pool,
        .nregs = 4,
        .max_reg = 10,

        .spill = test_spill,
        .fill = test_fill,
        .ctx = 0,
    };
    struct umbra_flat_ir *bb = make_bb(3, 0);
    struct ra                *ra = ra_create(bb, &cfg);
    int                       sl[3] = {-1, -1, -1};
    int                       ns = 0;
    reset_records();

    struct ra_step s0 = ra_step_alloc(ra, sl, &ns, 0);
    s0.rd >= 0 here;
    ra_reg(ra, 0) == s0.rd here;

    struct ra_step s1 = ra_step_alloc(ra, sl, &ns, 1);
    s1.rd >= 0 here;
    s1.rd != s0.rd here;
    ra_reg(ra, 1) == s1.rd here;
    nspills == 0 here;

    ra_destroy(ra);
    free_bb(bb);
}

TEST(test_step_unary) {
    static int8_t const pool[] = {5, 6, 7, 8};
    struct ra_config    cfg = {
        .pool = pool,
        .nregs = 4,
        .max_reg = 10,

        .spill = test_spill,
        .fill = test_fill,
        .ctx = 0,
    };
    struct umbra_flat_ir *bb = malloc(sizeof *bb);
    bb->inst = calloc(2, sizeof *bb->inst);
    bb->insts = 2;
    bb->preamble = 0;

    bb->inst[0].op = op_imm_32;
    bb->inst[0].imm = 42;
    bb->inst[1].op = op_f32_from_i32;
    bb->inst[1].x = (val){0};

    struct ra *ra = ra_create(bb, &cfg);
    int        sl[2] = {-1, -1};
    int        ns = 0;
    reset_records();

    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 0, r0);

    // x dead at inst 1: claim x's register
    ra_set_last_use(ra, 0, 1);
    struct ra_step s = ra_step_unary(ra, sl, &ns, &bb->inst[1], 1);
    s.rx == r0 here;
    s.rd == r0 here;
    ra_reg(ra, 1) == s.rd here;
    ra_reg(ra, 0) == -1 here;
    nspills == 0 here;

    ra_destroy(ra);
    free(bb->inst);
    free(bb);
}

TEST(test_step_unary_alive) {
    static int8_t const pool[] = {5, 6, 7, 8};
    struct ra_config    cfg = {
        .pool = pool,
        .nregs = 4,
        .max_reg = 10,

        .spill = test_spill,
        .fill = test_fill,
        .ctx = 0,
    };
    struct umbra_flat_ir *bb = malloc(sizeof *bb);
    bb->inst = calloc(3, sizeof *bb->inst);
    bb->insts = 3;
    bb->preamble = 0;

    bb->inst[0].op = op_imm_32;
    bb->inst[0].imm = 42;
    bb->inst[1].op = op_f32_from_i32;
    bb->inst[1].x = (val){0};
    bb->inst[2].op = op_add_i32;
    bb->inst[2].x = (val){0};
    bb->inst[2].y = (val){0};

    struct ra *ra = ra_create(bb, &cfg);
    int        sl[3] = {-1, -1, -1};
    int        ns = 0;
    reset_records();

    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 0, r0);

    // x still alive: rd must differ
    ra_set_last_use(ra, 0, 2);
    struct ra_step s = ra_step_unary(ra, sl, &ns, &bb->inst[1], 1);
    s.rx == r0 here;
    s.rd != r0 here;
    ra_reg(ra, 0) == r0 here;
    ra_reg(ra, 1) == s.rd here;

    ra_destroy(ra);
    free(bb->inst);
    free(bb);
}

TEST(test_step_unary_pins_inputs_and_dest) {
    // Regression: ra_step_unary used to leave both its input rx and the
    // newly-allocated dest rd unpinned. A backend (ARM64 _imm op handling)
    // that called ra_ensure(inst->y.id) right after ra_step_unary could
    // then evict either or both, with the freed register collapsing into
    // the imm's register. The result was FSUB v, v, v = 0 instead of
    // v - imm — observed in the perspective coverage shader where it made
    // the bw-1 / bh-1 clamp values revert to bw / bh, which let the gather
    // index reach bw*bh and run one element off the end of the bitmap.
    static int8_t const pool[] = {5, 6, 7};
    struct ra_config    cfg = {
        .pool = pool,
        .nregs = 3,
        .max_reg = 10,
        .spill = test_spill,
        .fill = test_fill,
        .ctx = 0,
    };

    // 4 values:
    //   inst[0] = imm_32 (the immediate, e.g. 1.0f)
    //   inst[1] = imm_32 (stand-in for the runtime input to be subtracted)
    //   inst[2] = imm_32 (filler so all 3 regs are occupied)
    //   inst[3] = sub_f32_imm inst[1] (with .y.id = 0 referencing the imm)
    struct umbra_flat_ir *bb = malloc(sizeof *bb);
    bb->inst = calloc(4, sizeof *bb->inst);
    bb->insts = 4;
    bb->preamble = 0;

    bb->inst[0].op = op_imm_32;
    bb->inst[0].imm = 0x3f800000;  // 1.0f
    bb->inst[1].op = op_imm_32;
    bb->inst[1].imm = 0x45800000;  // 4096.0f
    bb->inst[2].op = op_imm_32;
    bb->inst[2].imm = 0;
    bb->inst[3].op = op_sub_f32_imm;
    bb->inst[3].x = (val){.id = 1};
    bb->inst[3].y = (val){.id = 0};

    struct ra *ra = ra_create(bb, &cfg);
    int        sl[4] = {-1, -1, -1, -1};
    int        ns = 0;
    reset_records();

    // Last-use schedule: inst[0] used latest (so it's the eviction target
    // during the rd allocation), inst[1] (rx) alive past i=3, inst[3] (rd)
    // alive past i=3, inst[2] dies right after i=3. Without the pinning
    // fix, ra_step_unary's rd alloc evicts inst[0] (its lu is highest);
    // then the subsequent ra_ensure(inst[0]) allocates again and the only
    // remaining unpinned candidate is the freshly-allocated rd, which gets
    // evicted, collapsing rd onto the same register as ir.
    ra_set_last_use(ra, 0, 5);
    ra_set_last_use(ra, 1, 4);
    ra_set_last_use(ra, 2, 4);
    ra_set_last_use(ra, 3, 5);

    int8_t r0 = ra_alloc(ra, sl, &ns); ra_assign(ra, 0, r0);
    int8_t r1 = ra_alloc(ra, sl, &ns); ra_assign(ra, 1, r1);
    int8_t r2 = ra_alloc(ra, sl, &ns); ra_assign(ra, 2, r2);

    struct ra_step s = ra_step_unary(ra, sl, &ns, &bb->inst[3], 3);

    // Sanity: rx is the input register; rd is the new dest register.
    s.rx == r1 here;
    s.rd >= 0 here;
    s.rd != s.rx here;
    ra_reg(ra, 1) == r1 here;
    ra_reg(ra, 3) == s.rd here;

    // The crux: simulate the JIT _imm body's ra_ensure(inst->y.id) call.
    // Neither s.rx nor s.rd may collapse onto the returned register —
    // otherwise the FSUB rd, rx, ir would overwrite or self-cancel.
    int8_t ir = ra_ensure(ra, sl, &ns, 0);
    ir != s.rx here;
    ir != s.rd here;
    ra_reg(ra, 1) == r1 here;
    ra_reg(ra, 3) == s.rd here;

    ra_destroy(ra);
    free(bb->inst);
    free(bb);
}

TEST(test_step_alu) {
    static int8_t const pool[] = {5, 6, 7, 8};
    struct ra_config    cfg = {
        .pool = pool,
        .nregs = 4,
        .max_reg = 10,

        .spill = test_spill,
        .fill = test_fill,
        .ctx = 0,
    };
    struct umbra_flat_ir *bb = malloc(sizeof *bb);
    bb->inst = calloc(3, sizeof *bb->inst);
    bb->insts = 3;
    bb->preamble = 0;

    bb->inst[0].op = op_imm_32;
    bb->inst[0].imm = 1;
    bb->inst[1].op = op_imm_32;
    bb->inst[1].imm = 2;
    bb->inst[2].op = op_add_i32;
    bb->inst[2].x = (val){0};
    bb->inst[2].y = (val){.id = 1};

    struct ra *ra = ra_create(bb, &cfg);
    int        sl[3] = {-1, -1, -1};
    int        ns = 0;
    reset_records();

    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 0, r0);
    int8_t r1 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 1, r1);

    // both dead: rd claims one
    ra_set_last_use(ra, 0, 2);
    ra_set_last_use(ra, 1, 2);
    struct ra_step s = ra_step_alu(ra, sl, &ns, &bb->inst[2], 2, 0);
    s.rx == r0 here;
    s.ry == r1 here;
    s.rd >= 0 here;
    s.rd == r0 || s.rd == r1 here;
    s.scratch < 0 here;

    ra_destroy(ra);
    free(bb->inst);
    free(bb);
}

TEST(test_step_alu_scratch_does_not_evict_dest) {
    // Regression: ra_step_alu used to leave the freshly-claimed dest
    // unpinned. With every pool register in use, a subsequent scratch
    // alloc would walk Belady and pick whichever non-pinned val had
    // the farthest last_use — potentially the dest itself, since the
    // dest's val id was never added to pinned[]. The eviction would
    // then "spill" the dest's register (which the backend hadn't
    // written yet) and return that same register as the scratch,
    // collapsing s.rd onto s.scratch and silently corrupting the
    // would-be-spilled value.
    static int8_t const pool[] = {0, 1, 2};
    struct ra_config    cfg = {
        .pool = pool,
        .nregs = 3,
        .max_reg = 3,
        .spill = test_spill,
        .fill = test_fill,
        .ctx = 0,
    };

    // 4 vals: three imms occupy the pool, then a shr_u32 (op that
    // takes nscratch=1 in jit.c) consumes val 0 (so it can claim its
    // register) and reads val 1 (which gets pinned).
    struct umbra_flat_ir *bb = malloc(sizeof *bb);
    bb->inst = calloc(4, sizeof *bb->inst);
    bb->insts = 4;
    bb->preamble = 0;

    bb->inst[0].op = op_imm_32;
    bb->inst[0].imm = 16;
    bb->inst[1].op = op_imm_32;
    bb->inst[1].imm = 1;
    bb->inst[2].op = op_imm_32;
    bb->inst[2].imm = 0;
    bb->inst[3].op = op_shr_u32;
    bb->inst[3].x = (val){.id = 0};
    bb->inst[3].y = (val){.id = 1};

    struct ra *ra = ra_create(bb, &cfg);
    int        sl[4] = {-1, -1, -1, -1};
    int        ns = 0;
    reset_records();

    int8_t r0 = ra_alloc(ra, sl, &ns); ra_assign(ra, 0, r0);
    int8_t r1 = ra_alloc(ra, sl, &ns); ra_assign(ra, 1, r1);
    int8_t r2 = ra_alloc(ra, sl, &ns); ra_assign(ra, 2, r2);

    // Last-use schedule:
    //   val 0: dies at i=3 → step claims its register for the dest
    //   val 1: alive past 3 and farthest out → pinned, never evicted
    //   val 2: alive past 3 but nearer than val 3
    //   val 3 (the dest): alive farther than val 2
    // So among non-pinned candidates {dest val 3 in r0, val 2 in r2},
    // val 3 has the higher last_use — Belady picks it for eviction
    // unless the dest is pinned.
    ra_set_last_use(ra, 0, 3);
    ra_set_last_use(ra, 1, 9);
    ra_set_last_use(ra, 2, 4);
    ra_set_last_use(ra, 3, 6);

    struct ra_step s = ra_step_alu(ra, sl, &ns, &bb->inst[3], 3, 1);

    s.rd >= 0 here;
    s.scratch >= 0 here;
    // The crux: the scratch must NOT collapse onto the dest. The
    // backend will emit `op rd, rx, ry; <use scratch>` and would
    // clobber rd otherwise.
    s.rd != s.scratch here;
    // The dest val must still be live in its register.
    ra_reg(ra, 3) == s.rd here;
    // val 1 was pinned and must still hold its register.
    ra_reg(ra, 1) == r1 here;

    ra_destroy(ra);
    free(bb->inst);
    free(bb);
}

TEST(test_step_alu_scratch) {
    static int8_t const pool[] = {5, 6, 7, 8};
    struct ra_config    cfg = {
        .pool = pool,
        .nregs = 4,
        .max_reg = 10,

        .spill = test_spill,
        .fill = test_fill,
        .ctx = 0,
    };
    struct umbra_flat_ir *bb = malloc(sizeof *bb);
    bb->inst = calloc(3, sizeof *bb->inst);
    bb->insts = 3;
    bb->preamble = 0;

    bb->inst[0].op = op_imm_32;
    bb->inst[0].imm = 1;
    bb->inst[1].op = op_imm_32;
    bb->inst[1].imm = 2;
    bb->inst[2].op = op_add_i32;
    bb->inst[2].x = (val){0};
    bb->inst[2].y = (val){.id = 1};

    struct ra *ra = ra_create(bb, &cfg);
    int        sl[3] = {-1, -1, -1};
    int        ns = 0;
    reset_records();

    int8_t r0 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 0, r0);
    int8_t r1 = ra_alloc(ra, sl, &ns);
    ra_assign(ra, 1, r1);

    ra_set_last_use(ra, 0, 2);
    ra_set_last_use(ra, 1, 2);
    struct ra_step s = ra_step_alu(ra, sl, &ns, &bb->inst[2], 2, 1);
    s.rd >= 0 here;
    s.scratch >= 0 here;
    s.scratch != s.rd here;
    s.scratch != s.rx here;
    s.scratch != s.ry here;

    ra_destroy(ra);
    free(bb->inst);
    free(bb);
}

TEST(test_sparse_pool_eviction) {
    // Slow-path eviction projects pinned vals through pool_inv to a
    // pool-bit set and decodes ctz(bit) back through cfg.pool. This
    // test uses a non-contiguous pool {1, 4, 9} so any place that
    // confused a register id with its pool index would mis-skip an
    // eviction candidate or pick a register outside the pool.
    static int8_t const pool[] = {1, 4, 9};
    struct ra_config    cfg = {
        .pool = pool,
        .nregs = 3,
        .max_reg = 16,
        .spill = test_spill,
        .fill = test_fill,
        .ctx = 0,
    };
    struct umbra_flat_ir *bb = make_bb(4, 0);
    struct ra                *ra = ra_create(bb, &cfg);
    int                       sl[4] = {-1, -1, -1, -1};
    int                       ns = 0;
    reset_records();

    // LIFO fast-path: pool[0]=1 first, then 4, then 9.
    int8_t r0 = ra_alloc(ra, sl, &ns); ra_assign(ra, 0, r0);
    int8_t r1 = ra_alloc(ra, sl, &ns); ra_assign(ra, 1, r1);
    int8_t r2 = ra_alloc(ra, sl, &ns); ra_assign(ra, 2, r2);
    r0 == 1 here;
    r1 == 4 here;
    r2 == 9 here;

    // Belady picks the val with max last_use. Set val 1 (in reg 4) as
    // the farthest so the slow path's ctz iteration must walk past
    // bit 0 (reg 1) and select bit 1 (reg 4) — i.e., the answer
    // depends on cfg.pool[bit] decoding the bit back to the right
    // physical register.
    ra_set_last_use(ra, 0, 2);
    ra_set_last_use(ra, 1, 5);
    ra_set_last_use(ra, 2, 3);

    int8_t r3 = ra_alloc(ra, sl, &ns);
    nspills == 1 here;
    spills[0].reg == 4 here;
    r3 == 4 here;
    ra_reg(ra, 1) == -1 here;
    sl[1] >= 0 here;

    ra_destroy(ra);
    free_bb(bb);
}

TEST(test_evict_live_before_loop) {
    static int8_t const pool[] = {0, 1, 2};

    struct umbra_flat_ir *bb = make_bb(8, 1);
    bb->loop_begin = 4;
    bb->loop_end   = 7;
    bb->inst[5].x = (val){.id = 2};
    bb->inst[5].y = (val){.id = 3};

    struct ra_config cfg = {
        .pool    = pool,
        .nregs   = 3,
        .max_reg = 3,
        .spill   = test_spill,
        .fill    = test_fill,
    };
    int sl[8];
    __builtin_memset(sl, -1, sizeof sl);
    int ns = 0;
    struct ra *ra = ra_create(bb, &cfg);

    struct ra_step s1 = ra_step_alloc(ra, sl, &ns, 1);
    struct ra_step s2 = ra_step_alu(ra, sl, &ns, &bb->inst[2], 2, 0);
    struct ra_step s3 = ra_step_alu(ra, sl, &ns, &bb->inst[3], 3, 0);
    (void)s1; (void)s2; (void)s3;

    ra_reg(ra, 2) >= 0 here;
    ra_reg(ra, 3) >= 0 here;

    reset_records();
    ra_evict_live_before(ra, sl, &ns, 4);

    ra_reg(ra, 2) == -1 here;
    sl[2] >= 0 here;
    nspills >= 1 here;

    ra_destroy(ra);
    free_bb(bb);
}

