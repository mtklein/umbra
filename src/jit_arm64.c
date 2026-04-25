#include "jit.h"
#if defined(__aarch64__)
#include "assume.h"
#include "ra.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

_Static_assert(sizeof(struct umbra_buf) == 16, "");
_Static_assert(offsetof(struct umbra_buf, ptr)    ==  0, "");
_Static_assert(offsetof(struct umbra_buf, count)  ==  8, "");
_Static_assert(offsetof(struct umbra_buf, stride) == 12, "");

struct pool_ref {
    int data_off, code_pos;
};
struct pool {
    uint8_t         *data;
    struct pool_ref *ref;
    int              bytes, refs, cap_data, cap_refs;
};
static int pool_add(struct pool *p, void const *src, int n) {
    for (int i = 0; i + n <= p->bytes; i += n) {
        if (__builtin_memcmp(p->data + i, src, (size_t)n) == 0) {
            return i;
        }
    }
    if (p->cap_data < p->bytes + n) {
        p->cap_data = p->cap_data ? 2 * p->cap_data : 256;
        p->data = realloc(p->data, (size_t)p->cap_data);
    }
    int off = p->bytes;
    __builtin_memcpy(p->data + off, src, (size_t)n);
    p->bytes += n;
    return off;
}
static void pool_ref_at(struct pool *p, int data_off, int code_pos) {
    if (p->refs == p->cap_refs) {
        p->cap_refs = p->cap_refs ? 2 * p->cap_refs : 32;
        p->ref = realloc(p->ref, (size_t)p->cap_refs * sizeof *p->ref);
    }
    p->ref[p->refs++] = (struct pool_ref){data_off, code_pos};
}
static void pool_free(struct pool *p) {
    free(p->data);
    free(p->ref);
}

#include "asm_arm64.h"

typedef struct asm_arm64 Buf;

// X0=l, X1=t, X2=r, X3=b, X4=buf.
// X0=col_end, X1=l, X2=buf, X9=col, X14=row.
enum {
    XWIDTH = 1,   // x0_col (reset XCOL at row boundary)
    XBUF   = 2,
    XP     = 8,
    XCOL   = 9,   // column counter
    XT     = 10,
    XH     = 11,
    XW     = 12,
    XM     = 13,
    XY     = 14,
    XS     = 15,
};

static void load_ptr(Buf *c, int p, int *last_ptr, int elem_shift) {
    if (*last_ptr != p) {
        *last_ptr = p;
        int const disp_ptr    = p * (int)sizeof(struct umbra_buf),
                  disp_stride = p * (int)sizeof(struct umbra_buf)
                                + (int)__builtin_offsetof(struct umbra_buf, stride);
        put(c, LDR_xi(XP, XBUF, disp_ptr / 8));
        put(c, LDR_wi(XT, XBUF, disp_stride / 4));
        if (elem_shift) {
            put(c, LSL_xi(XT, XT, elem_shift));
        }
        put(c, MADD_x(XP, XY, XT, XP));
    }
}

static void resolve_ptr(Buf *c, ptr p, int *last_ptr, int elem_shift) {
    load_ptr(c, p.bits, last_ptr, elem_shift);
}

static void load_count(Buf *c, ptr p) {
    int const disp = p.bits * (int)sizeof(struct umbra_buf)
                          + (int)__builtin_offsetof(struct umbra_buf, count);
    put(c, LDR_wi(XM, XBUF, disp / 4));
}

static int8_t const pair_lo[] = { 4,  6, 16, 18, 20, 22, 24, 26, 28, 30,  8, 10, 12, 14};
static int8_t const pair_hi[] = { 5,  7, 17, 19, 21, 23, 25, 27, 29, 31,  9, 11, 13, 15};
static int lo(int r) { return pair_lo[r]; }
static int hi(int r) { return pair_hi[r]; }

static void emit_alu_reg(Buf *c, enum op op, int d, int x, int y, int z, int scratch) {
    switch ((int)op) {
    case op_add_f32: put(c, FADD_4s(d, x, y)); break;
    case op_sub_f32: put(c, FSUB_4s(d, x, y)); break;
    case op_mul_f32: put(c, FMUL_4s(d, x, y)); break;
    case op_min_f32: put(c, FMINNM_4s(d, x, y)); break;
    case op_max_f32: put(c, FMAXNM_4s(d, x, y)); break;
    case op_abs_f32: put(c, FABS_4s(d, x)); break;
    case op_square_f32: put(c, FMUL_4s(d, x, x)); break;
    case op_round_f32: put(c, FRINTN_4s(d, x)); break;
    case op_floor_f32: put(c, FRINTM_4s(d, x)); break;
    case op_ceil_f32: put(c, FRINTP_4s(d, x)); break;
    case op_round_i32: put(c, FCVTNS_4s(d, x)); break;
    case op_floor_i32: put(c, FCVTMS_4s(d, x)); break;
    case op_ceil_i32: put(c, FCVTPS_4s(d, x)); break;
    case op_fma_f32:
        if (d == z) {
            put(c, FMLA_4s(d, x, y));
        } else if (d != x && d != y) {
            put(c, ORR_16b(d, z, z));
            put(c, FMLA_4s(d, x, y));
        } else {
            put(c, ORR_16b(scratch, z, z));
            put(c, FMLA_4s(scratch, x, y));
            put(c, ORR_16b(d, scratch, scratch));
        }
        break;
    case op_fms_f32:
        if (d == z) {
            put(c, FMLS_4s(d, x, y));
        } else if (d != x && d != y) {
            put(c, ORR_16b(d, z, z));
            put(c, FMLS_4s(d, x, y));
        } else {
            put(c, ORR_16b(scratch, z, z));
            put(c, FMLS_4s(scratch, x, y));
            put(c, ORR_16b(d, scratch, scratch));
        }
        break;
    case op_square_add_f32:
        // d = x*x + y: reuse FMLA on y (the accumulator side).
        if (d == y) {
            put(c, FMLA_4s(d, x, x));
        } else if (d != x) {
            put(c, ORR_16b(d, y, y));
            put(c, FMLA_4s(d, x, x));
        } else {
            put(c, ORR_16b(scratch, y, y));
            put(c, FMLA_4s(scratch, x, x));
            put(c, ORR_16b(d, scratch, scratch));
        }
        break;
    case op_square_sub_f32:
        // d = y - x*x: FMLS subtracts x*x from the accumulator.
        if (d == y) {
            put(c, FMLS_4s(d, x, x));
        } else if (d != x) {
            put(c, ORR_16b(d, y, y));
            put(c, FMLS_4s(d, x, x));
        } else {
            put(c, ORR_16b(scratch, y, y));
            put(c, FMLS_4s(scratch, x, x));
            put(c, ORR_16b(d, scratch, scratch));
        }
        break;

    case op_add_i32: put(c, ADD_4s(d, x, y)); break;
    case op_sub_i32: put(c, SUB_4s(d, x, y)); break;
    case op_mul_i32: put(c, MUL_4s(d, x, y)); break;
    case op_shl_i32: put(c, USHL_4s(d, x, y)); break;
    case op_shr_u32:
        put(c, NEG_4s(scratch, y));
        put(c, USHL_4s(d, x, scratch));
        break;
    case op_shr_s32:
        put(c, NEG_4s(scratch, y));
        put(c, SSHL_4s(d, x, scratch));
        break;

    case op_f32_from_i32: put(c, SCVTF_4s(d, x)); break;
    case op_i32_from_f32: put(c, FCVTZS_4s(d, x)); break;

    case op_eq_f32: put(c, FCMEQ_4s(d, x, y)); break;
    case op_lt_f32: put(c, FCMGT_4s(d, y, x)); break;
    case op_le_f32: put(c, FCMGE_4s(d, y, x)); break;

    case op_eq_i32: put(c, CMEQ_4s(d, x, y)); break;
    case op_lt_s32: put(c, CMGT_4s(d, y, x)); break;
    case op_le_s32: put(c, CMGE_4s(d, y, x)); break;
    case op_lt_u32: put(c, CMHI_4s(d, y, x)); break;
    case op_le_u32: put(c, CMHS_4s(d, y, x)); break;

    case op_and_32: put(c, AND_16b(d, x, y)); break;
    case op_or_32: put(c, ORR_16b(d, x, y)); break;
    case op_xor_32: put(c, EOR_16b(d, x, y)); break;
    case op_sel_32:
        if (d == x) {
            put(c, BSL_16b(d, y, z));
        } else if (d == y) {
            put(c, BIF_16b(d, z, x));
        } else if (d == z) {
            put(c, BIT_16b(d, y, x));
        } else {
            put(c, ORR_16b(d, z, z));
            put(c, BIT_16b(d, y, x));
        }
        break;
    }
}

static int8_t const ra_pool[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};

struct jit_ctx {
    Buf                            *c;
    struct umbra_flat_ir const *ir;
    struct pool                     pool;
    int                             vars, loop_top, loop_br_skip;
    int                             if_depth;
    int                             if_cond_val[8];
};

static void arm64_spill(int reg, int slot, void *ctx) {
    struct jit_ctx *j = ctx;
    int const off = 2 * (slot + j->vars);
    put(j->c, STR_qi(lo(reg), XS, off));
    put(j->c, STR_qi(hi(reg), XS, off + 1));
}
static void arm64_fill(int reg, int slot, void *ctx) {
    struct jit_ctx *j = ctx;
    int const off = 2 * (slot + j->vars);
    put(j->c, LDR_qi(lo(reg), XS, off));
    put(j->c, LDR_qi(hi(reg), XS, off + 1));
}
static void arm64_pool_load(Buf *c, struct pool *p, int d, uint32_t v) {
    if (movi_4s(c, d, v)) {
        return;
    }
    uint32_t  const bcast[4] = {v, v, v, v};
    int       const off      = pool_add(p, bcast, 16);
    pool_ref_at(p, off, c->words);
    put(c, LDR_q_literal(d));
}
static void arm64_pool_load_wide(Buf *c, struct pool *p, int d, void const *data) {
    int const off = pool_add(p, data, 16);
    pool_ref_at(p, off, c->words);
    put(c, LDR_q_literal(d));
}
static void arm64_remat(int reg, int val, void *ctx) {
    struct jit_ctx *j = ctx;
    arm64_pool_load(j->c, &j->pool, lo(reg), (uint32_t)j->ir->inst[val].imm);
    put(j->c, ORR_16b(hi(reg), lo(reg), lo(reg)));
}

static struct ra* ra_create_arm64(struct umbra_flat_ir const *ir, struct jit_ctx *jc) {
    struct ra_config cfg = {
        .pool = ra_pool,
        .nregs = 14,
        .max_reg = 14,
        .spill = arm64_spill,
        .fill = arm64_fill,
        .remat = arm64_remat,
        .ctx = jc,
    };
    return ra_create(ir, &cfg);
}

static void emit_ops(Buf *c, struct umbra_flat_ir const *ir, int from, int to,
                     int *sl, int *ns, struct ra *ra, _Bool scalar,
                     struct jit_ctx *jc);

struct jit_program* jit_program(struct jit_backend *be,
                                           struct umbra_flat_ir const *ir) {
    int *sl = malloc((size_t)ir->insts * sizeof(int));
    for (int i = 0; i < ir->insts; i++) { sl[i] = -1; }
    int  ns = 0;

    size_t const pg  = 16384,
                 est = (size_t)(ir->insts * 40 + 256);
    void  *buf_mem;
    size_t buf_size;
    jit_acquire_code_buf(be, &buf_mem, &buf_size, est + pg);

    Buf c = {
        .word      = buf_mem,
        .words     = 0,
        .cap       = (int)((buf_size - pg) / 4),
        .mmap_size = buf_size,
    };
    struct jit_ctx jc = {.c = &c, .ir = ir, .pool = {0}, .vars = ir->vars};
    struct ra     *ra = ra_create_arm64(ir, &jc);

    put(&c, STP_pre(29, 30, 31, -2));
    put(&c, ADD_xi(29, 31, 0));

    // Save callee-saved Q8-Q15 (128 bytes).
    put(&c, STP_qi_pre(8, 9, 31, -8));
    put(&c, STP_qi(10, 11, 31, 2));
    put(&c, STP_qi(12, 13, 31, 4));
    put(&c, STP_qi(14, 15, 31, 6));

    int const stack_patch = c.words;
    put(&c, NOP());
    put(&c, NOP());

    // Entry: x0=l, x1=t, x2=r, x3=b, x4=buf.
    // Save before preamble clobbers args (and XM via load_count).
    put(&c, ADD_xi(XH, 0, 0));        // XH = l
    put(&c, ADD_xi(XCOL, 1, 0));      // XCOL = t (temp)
    put(&c, ADD_xi(XW, 2, 0));        // XW = r = col_end
    put(&c, ADD_xi(XY, 3, 0));        // XY = b (temp)
    put(&c, ADD_xi(XBUF, 4, 0));      // XBUF = buf

    int const preamble_off = c.words * 4;
    emit_ops(&c, ir, 0, ir->preamble, sl, &ns, ra, 0,
             &jc);

    ra_begin_loop(ra);

    // 2D loop: XCOL = column, XY = row, X0 = col_end.
    put(&c, ADD_xi(XM, XY, 0));       // XM = b = end_row (XY saved pre-preamble)
    put(&c, ADD_xi(0, XW, 0));        // x0 = col_end = r
    put(&c, ADD_xi(XWIDTH, XH, 0));   // XWIDTH = l
    put(&c, ADD_xi(XY, XCOL, 0));     // XY = t
    put(&c, ADD_xi(XCOL, XH, 0));     // XCOL = l
    put(&c, STP_pre(XM, 15, 31, -2)); // push end_row

    int const loop_top = c.words;
    ra_assert_loop_invariant(ra);

    // remaining = col_end - XCOL
    put(&c, SUB_xr(XT, 0, XCOL));
    put(&c, SUBS_xi(31, XT, 8));
    int const br_tail = c.words;
    put(&c, Bcond(0xb, 0));

    int const simd_body_off = c.words * 4;
    emit_ops(&c, ir, ir->preamble, ir->insts, sl, &ns, ra, 0,
             &jc);

    ra_end_loop(ra, sl);
    ra_assert_loop_invariant(ra);

    put(&c, ADD_xi(XCOL, XCOL, 8));
    put(&c, B(loop_top - c.words));

    int const tail_top = c.words;
    c.word[br_tail] = Bcond(0xb, tail_top - br_tail);

    put(&c, CMP_xr(XCOL, 0));
    int const br_row_done = c.words;
    put(&c, Bcond(0xa, 0));  // B.GE row_done

    // TODO: once every memory op consults the if_mask (umbra.h umbra_if TODO),
    //       the scalar tail can go: enter the SIMD body once with an if_mask
    //       seeded so the first remaining-count lanes are true, stamp out one
    //       K-lane iteration, done.  Removes a whole second copy of the body.
    ra_reset_pool(ra);
    for (int i = 0; i < ir->insts; i++) { sl[i] = -1; }

    emit_ops(&c, ir, 0, ir->preamble, sl, &ns, ra, 0,
             &jc);
    emit_ops(&c, ir, ir->preamble, ir->insts, sl, &ns, ra, 1,
             &jc);

    put(&c, ADD_xi(XCOL, XCOL, 1));
    put(&c, B(tail_top - c.words));

    int const row_done = c.words;
    c.word[br_row_done] = Bcond(0xa, row_done - br_row_done);

    put(&c, ADD_xi(XY, XY, 1));
    put(&c, ADD_xi(XCOL, XWIDTH, 0));    // XCOL = x0_col
    // LDR XT, [SP] -- load saved end_row
    put(&c, LDR_xi(XT, 31, 0));
    put(&c, CMP_xr(XY, XT));
    int const br_done_all = c.words;
    put(&c, Bcond(0xa, 0));  // B.GE -> done_all (no more rows)

    // Re-emit preamble so the next row's SIMD iter starts with uniforms in
    // the registers it expects: the tail body clobbers them, and the SIMD
    // body's end-of-loop fills are skipped when we enter the tail.
    int const next_row_off = c.words * 4;
    ra_reset_pool(ra);
    for (int i = 0; i < ir->insts; i++) { sl[i] = -1; }
    emit_ops(&c, ir, 0, ir->preamble, sl, &ns, ra, 0, &jc);
    ra_assert_loop_invariant(ra);
    put(&c, B(loop_top - c.words));

    int const done_all = c.words;
    c.word[br_done_all] = Bcond(0xa, done_all - br_done_all);

    // Restore stack (saved end_row)
    put(&c, LDP_post(0, 15, 31, 2));

    put(&c, ADD_xi(31, 29, 0));
    // Restore callee-saved Q8-Q15 from [FP-128, FP) using negative offsets.
    put(&c, LDP_qi( 8,  9, 31, -8));
    put(&c, LDP_qi(10, 11, 31, -6));
    put(&c, LDP_qi(12, 13, 31, -4));
    put(&c, LDP_qi(14, 15, 31, -2));
    put(&c, LDP_post(29, 30, 31, 2));
    put(&c, RET());

    if (ns + ir->vars > 0) {
        c.word[stack_patch] = SUB_xi(31, 31, (ns + ir->vars) * 32);
    }
    c.word[stack_patch + 1] = ADD_xi(XS, 31, 0);
    while (c.words & 3) {
        put(&c, NOP());
    }
    int const pool_start = c.words;
    for (int pi = 0; pi < jc.pool.bytes; pi += 4) {
        uint32_t w;
        __builtin_memcpy(&w, jc.pool.data + pi, 4);
        put(&c, w);
    }
    for (int pi = 0; pi < jc.pool.refs; pi++) {
        struct pool_ref *r = &jc.pool.ref[pi];
        int const word_off = pool_start + r->data_off / 4,
                  imm19    = word_off - r->code_pos;
        c.word[r->code_pos] = 0x9c000000u
            | ((uint32_t)(imm19 & 0x7ffff) << 5)
            | (c.word[r->code_pos] & 0x1fu);
    }
    pool_free(&jc.pool);

    struct jit_program *j = calloc(1, sizeof *j);
    if (ir->bindings) {
        j->bindings = ir->bindings;
        size_t const sz = (size_t)j->bindings * sizeof *j->binding;
        j->binding = malloc(sz);
        __builtin_memcpy(j->binding, ir->binding, sz);
    }

    ra_destroy(ra);
    free(sl);

    size_t const code_sz = (size_t)c.words * 4,
                 alloc   = (code_sz + pg - 1) & ~(pg - 1);

    if (alloc + pg < c.mmap_size) {
        mprotect((char *)c.word + alloc + pg, c.mmap_size - alloc - pg, PROT_NONE);
    }
    mprotect((char *)c.word + alloc, pg, PROT_NONE);
    { int const ok = mprotect(c.word, alloc, PROT_READ | PROT_EXEC); assume(ok == 0); }
    __builtin___clear_cache(c.word, (char *)c.word + code_sz);

    j->code = c.word;
    j->code_size = c.mmap_size;
    j->code_bytes = pool_start * 4;
    j->labels = 0;
    j->label[j->labels++] = (struct jit_label){.name = "preamble",  .byte_off = preamble_off};
    j->label[j->labels++] = (struct jit_label){.name = "loop_top",  .byte_off = loop_top * 4};
    j->label[j->labels++] = (struct jit_label){.name = "simd_body", .byte_off = simd_body_off};
    j->label[j->labels++] = (struct jit_label){.name = "tail_top",  .byte_off = tail_top * 4};
    j->label[j->labels++] = (struct jit_label){.name = "row_done",  .byte_off = row_done * 4};
    j->label[j->labels++] = (struct jit_label){.name = "next_row",  .byte_off = next_row_off};
    j->label[j->labels++] = (struct jit_label){.name = "done_all",  .byte_off = done_all * 4};
    {
        union {
            void *p;
            void (*fn)(int, int, int, int, struct umbra_buf *);
        } u = {.p = c.word};
        j->entry = u.fn;
    }
    return j;
}

static void emit_ops(Buf *c, struct umbra_flat_ir const *ir, int from, int to,
                     int *sl, int *ns, struct ra *ra, _Bool scalar,
                     struct jit_ctx *jc) {
    int last_ptr = -1;

    for (int i = from; i < to; i++) {
        struct ir_inst const *inst = &ir->inst[i];

        switch (inst->op) {
        case op_x: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            put(c, DUP_4s_w(lo(s.rd), XCOL));
            if (!scalar) {
                put(c, ORR_16b(hi(s.rd), lo(s.rd), lo(s.rd)));
                int8_t   tmp = ra_alloc(ra, sl, ns);
                uint32_t iota_lo[4] = {0, 1, 2, 3};
                uint32_t iota_hi[4] = {4, 5, 6, 7};
                arm64_pool_load_wide(c, &jc->pool, lo(tmp), iota_lo);
                put(c, ADD_4s(lo(s.rd), lo(s.rd), lo(tmp)));
                arm64_pool_load_wide(c, &jc->pool, lo(tmp), iota_hi);
                put(c, ADD_4s(hi(s.rd), hi(s.rd), lo(tmp)));
                ra_return_reg(ra, tmp);
            }
        } break;

        case op_y: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            put(c, DUP_4s_w(lo(s.rd), XY));
            if (!scalar) { put(c, ORR_16b(hi(s.rd), lo(s.rd), lo(s.rd))); }
        } break;

        case op_load_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            ptr            p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, 2);
            if (scalar) {
                put(c, LDR_sx(lo(s.rd), XP, XCOL));
            } else {
                put(c, LSL_xi(XT, XCOL, 2));
                put(c, ADD_xr(XT, XP, XT));
                put(c, LDP_qi(lo(s.rd), hi(s.rd), XT, 0));
            }
        } break;

        case op_load_16: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            ptr            p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, 1);
            if (scalar) {
                put(c, LDRH_wr(XT, XP, XCOL));
                put(c, DUP_4s_w(lo(s.rd), XT));
            } else {
                put(c, LSL_xi(XT, XCOL, 1));
                put(c, ADD_xr(XT, XP, XT));
                put(c, LDR_di(lo(s.rd), XT, 0));
                put(c, LDR_di(hi(s.rd), XT, 1));
            }
        } break;

        case op_uniform_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            ptr            p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, 2);
            put(c, LDR_si(lo(s.rd), XP, inst->imm));
            put(c, DUP_4s_lane(lo(s.rd), lo(s.rd), 0));
            put(c, ORR_16b(hi(s.rd), lo(s.rd), lo(s.rd)));
        } break;

        case op_gather_uniform_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int8_t         rx = ra_ensure(ra, sl, ns, inst->x.id);
            ra_free_chan(ra, inst->x, i);
            ptr p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, 2);
            load_count(c, p);
            put(c, UMOV_ws(XT, lo(rx)));
            put(c, MOVI_4s(lo(s.rd), 0, 0));
            if (!scalar) { put(c, ORR_16b(hi(s.rd), lo(s.rd), lo(s.rd))); }
            put(c, CMP_wr(XT, XM));
            int skip = scalar ? 3 : 4;
            put(c, Bcond(0x2, skip));
            put(c, LDR_wr(XT, XP, XT));
            put(c, DUP_4s_w(lo(s.rd), XT));
            if (!scalar) { put(c, ORR_16b(hi(s.rd), lo(s.rd), lo(s.rd))); }
        } break;

        case op_gather_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int8_t         rx = ra_ensure(ra, sl, ns, inst->x.id);
            ra_free_chan(ra, inst->x, i);
            ptr p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, 2);
            load_count(c, p);
            if (scalar) {
                put(c, MOVI_4s(lo(s.rd), 0, 0));
                put(c, UMOV_ws(XT, lo(rx)));
                put(c, CMP_wr(XT, XM));
                put(c, Bcond(0x2, 2));
                put(c, LDR_sx(lo(s.rd), XP, XT));
            } else {
                put(c, MOVI_4s(lo(s.rd), 0, 0));
                put(c, MOVI_4s(hi(s.rd), 0, 0));
                for (int k = 0; k < 4; k++) {
                    put(c, UMOV_ws_lane(XT, lo(rx), k));
                    put(c, CMP_wr(XT, XM));
                    put(c, Bcond(0x2, 4));
                    put(c, LSL_xi(XT, XT, 2));
                    put(c, ADD_xr(XT, XP, XT));
                    put(c, LD1_s(lo(s.rd), k, XT));
                }
                for (int k = 0; k < 4; k++) {
                    put(c, UMOV_ws_lane(XT, hi(rx), k));
                    put(c, CMP_wr(XT, XM));
                    put(c, Bcond(0x2, 4));
                    put(c, LSL_xi(XT, XT, 2));
                    put(c, ADD_xr(XT, XP, XT));
                    put(c, LD1_s(hi(s.rd), k, XT));
                }
            }
        } break;

        case op_gather_16: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int8_t         rx = ra_ensure(ra, sl, ns, inst->x.id);
            ra_free_chan(ra, inst->x, i);
            ptr p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, 1);
            load_count(c, p);
            if (scalar) {
                put(c, MOVI_4s(lo(s.rd), 0, 0));
                put(c, UMOV_ws(XT, lo(rx)));
                put(c, CMP_wr(XT, XM));
                put(c, Bcond(0x2, 5));
                put(c, LSL_xi(XT, XT, 1));
                put(c, ADD_xr(XT, XP, XT));
                put(c, LDRH_wi(XT, XT, 0));
                put(c, DUP_4s_w(lo(s.rd), XT));
            } else {
                put(c, MOVI_4s(lo(s.rd), 0, 0));
                for (int k = 0; k < 4; k++) {
                    put(c, UMOV_ws_lane(XT, lo(rx), k));
                    put(c, CMP_wr(XT, XM));
                    put(c, Bcond(0x2, 5));
                    put(c, LSL_xi(XT, XT, 1));
                    put(c, ADD_xr(XT, XP, XT));
                    put(c, LDRH_wi(XT, XT, 0));
                    put(c, INS_s(lo(s.rd), k, XT));
                }
                put(c, XTN_4h(lo(s.rd), lo(s.rd)));
                put(c, MOVI_4s(hi(s.rd), 0, 0));
                for (int k = 0; k < 4; k++) {
                    put(c, UMOV_ws_lane(XT, hi(rx), k));
                    put(c, CMP_wr(XT, XM));
                    put(c, Bcond(0x2, 5));
                    put(c, LSL_xi(XT, XT, 1));
                    put(c, ADD_xr(XT, XP, XT));
                    put(c, LDRH_wi(XT, XT, 0));
                    put(c, INS_s(hi(s.rd), k, XT));
                }
                put(c, XTN_4h(hi(s.rd), hi(s.rd)));
            }
        } break;

        case op_store_32: {
            int8_t ry = ra_ensure(ra, sl, ns, inst->y.id);
            ptr    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, 2);
            if (jc->if_depth > 0) {
                int8_t rm  = ra_ensure(ra, sl, ns, jc->if_cond_val[jc->if_depth - 1]);
                int8_t tmp = ra_alloc(ra, sl, ns);
                if (scalar) {
                    put(c, LDR_sx(lo(tmp), XP, XCOL));
                    put(c, BIT_16b(lo(tmp), lo(ry), lo(rm)));
                    put(c, STR_sx(lo(tmp), XP, XCOL));
                } else {
                    put(c, LSL_xi(XT, XCOL, 2));
                    put(c, ADD_xr(XT, XP, XT));
                    put(c, LDP_qi(lo(tmp), hi(tmp), XT, 0));
                    put(c, BIT_16b(lo(tmp), lo(ry), lo(rm)));
                    put(c, BIT_16b(hi(tmp), hi(ry), hi(rm)));
                    put(c, STP_qi(lo(tmp), hi(tmp), XT, 0));
                }
                ra_return_reg(ra, tmp);
            } else {
                if (scalar) {
                    put(c, STR_sx(lo(ry), XP, XCOL));
                } else {
                    put(c, LSL_xi(XT, XCOL, 2));
                    put(c, ADD_xr(XT, XP, XT));
                    put(c, STP_qi(lo(ry), hi(ry), XT, 0));
                }
            }
            ra_free_chan(ra, inst->y, i);
        } break;

        case op_load_16x4: {
            ptr    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, 3);
            struct ra_step s0 = ra_step_alloc(ra, sl, ns, i);
            int8_t r1 = ra_alloc(ra, sl, ns);
            int8_t r2 = ra_alloc(ra, sl, ns);
            int8_t r3 = ra_alloc(ra, sl, ns);
            if (scalar) {
                put(c, LSL_xi(XT, XCOL, 3));
                put(c, ADD_xr(XT, XP, XT));
                put(c, LDR_hi(lo(s0.rd), XT, 0));
                put(c, LDR_hi(lo(r1),    XT, 1));
                put(c, LDR_hi(lo(r2),    XT, 2));
                put(c, LDR_hi(lo(r3),    XT, 3));
            } else {
                put(c, LSL_xi(XT, XCOL, 3));
                put(c, ADD_xr(XT, XP, XT));
                put(c, LD4_4h(0, XT));
                put(c, ORR_16b(lo(s0.rd), 0, 0));
                put(c, ORR_16b(lo(r1),    1, 1));
                put(c, ORR_16b(lo(r2),    2, 2));
                put(c, ORR_16b(lo(r3),    3, 3));
                put(c, ADD_xi(XT, XT, 32));
                put(c, LD4_4h(0, XT));
                put(c, ORR_16b(hi(s0.rd), 0, 0));
                put(c, ORR_16b(hi(r1),    1, 1));
                put(c, ORR_16b(hi(r2),    2, 2));
                put(c, ORR_16b(hi(r3),    3, 3));
            }
            ra_set_chan_reg(ra, i, 0, s0.rd);
            ra_set_chan_reg(ra, i, 1, r1);
            ra_set_chan_reg(ra, i, 2, r2);
            ra_set_chan_reg(ra, i, 3, r3);
        } break;
        case op_load_16x4_planar: {
            struct ra_step s0 = ra_step_alloc(ra, sl, ns, i);
            int8_t r1 = ra_alloc(ra, sl, ns);
            int8_t r2 = ra_alloc(ra, sl, ns);
            int8_t r3 = ra_alloc(ra, sl, ns);
            ptr    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, 1);
            int8_t px = ra_alloc(ra, sl, ns);
            {
                int const count_off = p.bits * (int)sizeof(struct umbra_buf)
                                    + (int)__builtin_offsetof(struct umbra_buf, count);
                put(c, LDR_wi(XT, XBUF, count_off / 4));
                put(c, LSR_xi(XT, XT, 1));
            }
            if (scalar) {
                put(c, LDR_hx(lo(s0.rd), XP, XCOL));
                put(c, ADD_xr(XP, XP, XT));
                put(c, LDR_hx(lo(r1), XP, XCOL));
                put(c, ADD_xr(XP, XP, XT));
                put(c, LDR_hx(lo(r2), XP, XCOL));
                put(c, ADD_xr(XP, XP, XT));
                put(c, LDR_hx(lo(r3), XP, XCOL));
            } else {
                put(c, LSL_xi(XM, XCOL, 1));
                put(c, ADD_xr(XM, XP, XM));
                put(c, LDR_di(lo(s0.rd), XM, 0));
                put(c, LDR_di(hi(s0.rd), XM, 1));
                put(c, ADD_xr(XM, XM, XT));
                put(c, LDR_di(lo(r1), XM, 0));
                put(c, LDR_di(hi(r1), XM, 1));
                put(c, ADD_xr(XM, XM, XT));
                put(c, LDR_di(lo(r2), XM, 0));
                put(c, LDR_di(hi(r2), XM, 1));
                put(c, ADD_xr(XM, XM, XT));
                put(c, LDR_di(lo(r3), XM, 0));
                put(c, LDR_di(hi(r3), XM, 1));
            }
            last_ptr = -1;
            ra_return_reg(ra, px);
            ra_set_chan_reg(ra, i, 0, s0.rd);
            ra_set_chan_reg(ra, i, 1, r1);
            ra_set_chan_reg(ra, i, 2, r2);
            ra_set_chan_reg(ra, i, 3, r3);
        } break;
        case op_store_16x4: {
            int8_t rr   = ra_ensure_chan(ra, sl, ns, inst->x.id, (int)inst->x.chan);
            int8_t rg   = ra_ensure_chan(ra, sl, ns, inst->y.id, (int)inst->y.chan);
            int8_t rb  = ra_ensure_chan(ra, sl, ns, inst->z.id, (int)inst->z.chan);
            int8_t ra_v = ra_ensure_chan(ra, sl, ns, inst->w.id, (int)inst->w.chan);
            ptr    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, 3);
            if (jc->if_depth > 0) {
                int8_t rm = ra_ensure(ra, sl, ns, jc->if_cond_val[jc->if_depth - 1]);
                int8_t t  = ra_alloc(ra, sl, ns);
                int8_t z  = ra_alloc(ra, sl, ns);
                int8_t mw = ra_alloc(ra, sl, ns);
                int8_t o  = ra_alloc(ra, sl, ns);
                if (scalar) {
                    put(c, ZIP1_8h(lo(t),  lo(rr), lo(rg)));
                    put(c, ZIP1_8h(hi(t),  lo(rb), lo(ra_v)));
                    put(c, ZIP1_4s(lo(z),  lo(t),  hi(t)));
                    put(c, ZIP1_4s(lo(mw), lo(rm), lo(rm)));
                    put(c, LSL_xi(XT, XCOL, 3));
                    put(c, ADD_xr(XT, XP, XT));
                    put(c, LDR_di(lo(o), XT, 0));
                    put(c, BIT_16b(lo(o), lo(z), lo(mw)));
                    put(c, STR_di(lo(o), XT, 0));
                } else {
                    put(c, LSL_xi(XT, XCOL, 3));
                    put(c, ADD_xr(XT, XP, XT));
                    put(c, ZIP1_8h(lo(t),  lo(rr), lo(rg)));
                    put(c, ZIP1_8h(hi(t),  lo(rb), lo(ra_v)));
                    put(c, ZIP1_4s(lo(z),  lo(t),  hi(t)));
                    put(c, ZIP2_4s(hi(z),  lo(t),  hi(t)));
                    put(c, ZIP1_4s(lo(mw), lo(rm), lo(rm)));
                    put(c, ZIP2_4s(hi(mw), lo(rm), lo(rm)));
                    put(c, LDP_qi(lo(o), hi(o), XT, 0));
                    put(c, BIT_16b(lo(o), lo(z), lo(mw)));
                    put(c, BIT_16b(hi(o), hi(z), hi(mw)));
                    put(c, STP_qi(lo(o), hi(o), XT, 0));
                    put(c, ZIP1_8h(lo(t),  hi(rr), hi(rg)));
                    put(c, ZIP1_8h(hi(t),  hi(rb), hi(ra_v)));
                    put(c, ZIP1_4s(lo(z),  lo(t),  hi(t)));
                    put(c, ZIP2_4s(hi(z),  lo(t),  hi(t)));
                    put(c, ZIP1_4s(lo(mw), hi(rm), hi(rm)));
                    put(c, ZIP2_4s(hi(mw), hi(rm), hi(rm)));
                    put(c, ADD_xi(XT, XT, 32));
                    put(c, LDP_qi(lo(o), hi(o), XT, 0));
                    put(c, BIT_16b(lo(o), lo(z), lo(mw)));
                    put(c, BIT_16b(hi(o), hi(z), hi(mw)));
                    put(c, STP_qi(lo(o), hi(o), XT, 0));
                }
                ra_return_reg(ra, o);
                ra_return_reg(ra, mw);
                ra_return_reg(ra, z);
                ra_return_reg(ra, t);
            } else if (scalar) {
                int8_t px = ra_alloc(ra, sl, ns);
                int8_t t  = ra_alloc(ra, sl, ns);
                int8_t z  = ra_alloc(ra, sl, ns);
                put(c, ZIP1_8h(lo(t), lo(rr), lo(rg)));
                put(c, ZIP1_8h(lo(px), lo(rb), lo(ra_v)));
                put(c, ZIP1_4s(lo(z), lo(t), lo(px)));
                put(c, LSL_xi(XT, XCOL, 3));
                put(c, ADD_xr(XT, XP, XT));
                put(c, STR_di(lo(z), XT, 0));
                ra_return_reg(ra, z);
                ra_return_reg(ra, t);
                ra_return_reg(ra, px);
            } else {
                put(c, ORR_16b(0, lo(rr),   lo(rr)));
                put(c, ORR_16b(1, lo(rg),   lo(rg)));
                put(c, ORR_16b(2, lo(rb),  lo(rb)));
                put(c, ORR_16b(3, lo(ra_v), lo(ra_v)));
                put(c, LSL_xi(XT, XCOL, 3));
                put(c, ADD_xr(XT, XP, XT));
                put(c, ST4_4h(0, XT));
                put(c, ORR_16b(0, hi(rr),   hi(rr)));
                put(c, ORR_16b(1, hi(rg),   hi(rg)));
                put(c, ORR_16b(2, hi(rb),  hi(rb)));
                put(c, ORR_16b(3, hi(ra_v), hi(ra_v)));
                put(c, ADD_xi(XT, XT, 32));
                put(c, ST4_4h(0, XT));
            }
            ra_free_chan(ra, inst->x, i);
            ra_free_chan(ra, inst->y, i);
            ra_free_chan(ra, inst->z, i);
            ra_free_chan(ra, inst->w, i);
        } break;
        case op_store_16x4_planar: {
            int8_t rr   = ra_ensure_chan(ra, sl, ns, inst->x.id, (int)inst->x.chan);
            int8_t rg   = ra_ensure_chan(ra, sl, ns, inst->y.id, (int)inst->y.chan);
            int8_t rb  = ra_ensure_chan(ra, sl, ns, inst->z.id, (int)inst->z.chan);
            int8_t ra_v = ra_ensure_chan(ra, sl, ns, inst->w.id, (int)inst->w.chan);
            ptr    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, 1);
            {
                int const count_off = p.bits * (int)sizeof(struct umbra_buf)
                                    + (int)__builtin_offsetof(struct umbra_buf, count);
                put(c, LDR_wi(XT, XBUF, count_off / 4));
                put(c, LSR_xi(XT, XT, 1));
            }
            if (scalar) {
                put(c, STR_hx(lo(rr), XP, XCOL));
                put(c, ADD_xr(XP, XP, XT));
                put(c, STR_hx(lo(rg), XP, XCOL));
                put(c, ADD_xr(XP, XP, XT));
                put(c, STR_hx(lo(rb), XP, XCOL));
                put(c, ADD_xr(XP, XP, XT));
                put(c, STR_hx(lo(ra_v), XP, XCOL));
            } else {
                put(c, LSL_xi(XM, XCOL, 1));
                put(c, ADD_xr(XM, XP, XM));
                put(c, STR_di(lo(rr), XM, 0));
                put(c, STR_di(hi(rr), XM, 1));
                put(c, ADD_xr(XM, XM, XT));
                put(c, STR_di(lo(rg), XM, 0));
                put(c, STR_di(hi(rg), XM, 1));
                put(c, ADD_xr(XM, XM, XT));
                put(c, STR_di(lo(rb), XM, 0));
                put(c, STR_di(hi(rb), XM, 1));
                put(c, ADD_xr(XM, XM, XT));
                put(c, STR_di(lo(ra_v), XM, 0));
                put(c, STR_di(hi(ra_v), XM, 1));
            }
            last_ptr = -1;
            ra_free_chan(ra, inst->x, i);
            ra_free_chan(ra, inst->y, i);
            ra_free_chan(ra, inst->z, i);
            ra_free_chan(ra, inst->w, i);
        } break;

        case op_load_8x4: {
            struct ra_step s0 = ra_step_alloc(ra, sl, ns, i);
            int8_t r1 = ra_alloc(ra, sl, ns);
            int8_t r2 = ra_alloc(ra, sl, ns);
            int8_t r3 = ra_alloc(ra, sl, ns);
            ptr    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, 2);
            if (scalar) {
                int8_t px   = ra_alloc(ra, sl, ns);
                int8_t mask = ra_alloc(ra, sl, ns);
                put(c, MOVI_4s(lo(mask), 0xFF, 0));
                put(c, LDR_sx(lo(px), XP, XCOL));
                put(c, AND_16b(lo(s0.rd), lo(px), lo(mask)));
                put(c, USHR_4s_imm(lo(r1), lo(px), 8));  put(c, AND_16b(lo(r1), lo(r1), lo(mask)));
                put(c, USHR_4s_imm(lo(r2), lo(px), 16)); put(c, AND_16b(lo(r2), lo(r2), lo(mask)));
                put(c, USHR_4s_imm(lo(r3), lo(px), 24));
                ra_return_reg(ra, mask);
                ra_return_reg(ra, px);
            } else {
                put(c, LSL_xi(XT, XCOL, 2));
                put(c, ADD_xr(XT, XP, XT));
                put(c, LD4_8b(0, XT));
                // V0=R(8xu8), V1=G, V2=B, V3=A.  Widen u8->u16->u32.
                put(c, UXTL_8h(0, 0));
                put(c, UXTL_4s(lo(s0.rd), 0));
                put(c, W(UXTL_4s(hi(s0.rd), 0)));
                put(c, UXTL_8h(1, 1));
                put(c, UXTL_4s(lo(r1), 1));
                put(c, W(UXTL_4s(hi(r1), 1)));
                put(c, UXTL_8h(2, 2));
                put(c, UXTL_4s(lo(r2), 2));
                put(c, W(UXTL_4s(hi(r2), 2)));
                put(c, UXTL_8h(3, 3));
                put(c, UXTL_4s(lo(r3), 3));
                put(c, W(UXTL_4s(hi(r3), 3)));
            }
            ra_set_chan_reg(ra, i, 0, s0.rd);
            ra_set_chan_reg(ra, i, 1, r1);
            ra_set_chan_reg(ra, i, 2, r2);
            ra_set_chan_reg(ra, i, 3, r3);
        } break;
        case op_store_8x4: {
            int8_t rr   = ra_ensure_chan(ra, sl, ns, inst->x.id, (int)inst->x.chan);
            int8_t rg   = ra_ensure_chan(ra, sl, ns, inst->y.id, (int)inst->y.chan);
            int8_t rb  = ra_ensure_chan(ra, sl, ns, inst->z.id, (int)inst->z.chan);
            int8_t ra_v = ra_ensure_chan(ra, sl, ns, inst->w.id, (int)inst->w.chan);
            ptr    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, 2);
            if (jc->if_depth > 0) {
                int8_t rm  = ra_ensure(ra, sl, ns, jc->if_cond_val[jc->if_depth - 1]);
                int8_t px  = ra_alloc(ra, sl, ns);
                int8_t t   = ra_alloc(ra, sl, ns);
                int8_t old = ra_alloc(ra, sl, ns);
                if (scalar) {
                    put(c, SHL_4s_imm(lo(t), lo(rg),    8)); put(c, ORR_16b(lo(px), lo(rr), lo(t)));
                    put(c, SHL_4s_imm(lo(t), lo(rb),   16)); put(c, ORR_16b(lo(px), lo(px), lo(t)));
                    put(c, SHL_4s_imm(lo(t), lo(ra_v), 24)); put(c, ORR_16b(lo(px), lo(px), lo(t)));
                    put(c, LDR_sx(lo(old), XP, XCOL));
                    put(c, BIT_16b(lo(old), lo(px), lo(rm)));
                    put(c, STR_sx(lo(old), XP, XCOL));
                } else {
                    put(c, SHL_4s_imm(lo(t), lo(rg),    8)); put(c, ORR_16b(lo(px), lo(rr), lo(t)));
                    put(c, SHL_4s_imm(hi(t), hi(rg),    8)); put(c, ORR_16b(hi(px), hi(rr), hi(t)));
                    put(c, SHL_4s_imm(lo(t), lo(rb),   16)); put(c, ORR_16b(lo(px), lo(px), lo(t)));
                    put(c, SHL_4s_imm(hi(t), hi(rb),   16)); put(c, ORR_16b(hi(px), hi(px), hi(t)));
                    put(c, SHL_4s_imm(lo(t), lo(ra_v), 24)); put(c, ORR_16b(lo(px), lo(px), lo(t)));
                    put(c, SHL_4s_imm(hi(t), hi(ra_v), 24)); put(c, ORR_16b(hi(px), hi(px), hi(t)));
                    put(c, LSL_xi(XT, XCOL, 2));
                    put(c, ADD_xr(XT, XP, XT));
                    put(c, LDP_qi(lo(old), hi(old), XT, 0));
                    put(c, BIT_16b(lo(old), lo(px), lo(rm)));
                    put(c, BIT_16b(hi(old), hi(px), hi(rm)));
                    put(c, STP_qi(lo(old), hi(old), XT, 0));
                }
                ra_return_reg(ra, old);
                ra_return_reg(ra, t);
                ra_return_reg(ra, px);
            } else if (scalar) {
                int8_t px = ra_alloc(ra, sl, ns);
                int8_t t  = ra_alloc(ra, sl, ns);
                put(c, SHL_4s_imm(lo(t), lo(rg), 8));    put(c, ORR_16b(lo(px), lo(rr), lo(t)));
                put(c, SHL_4s_imm(lo(t), lo(rb), 16));  put(c, ORR_16b(lo(px), lo(px), lo(t)));
                put(c, SHL_4s_imm(lo(t), lo(ra_v), 24)); put(c, ORR_16b(lo(px), lo(px), lo(t)));
                put(c, STR_sx(lo(px), XP, XCOL));
                ra_return_reg(ra, t);
                ra_return_reg(ra, px);
            } else {
                put(c, XTN_4h(0, lo(rr)));   put(c, W(XTN_4h(0, hi(rr))));
                put(c, XTN_8b(0, 0));
                put(c, XTN_4h(1, lo(rg)));   put(c, W(XTN_4h(1, hi(rg))));
                put(c, XTN_8b(1, 1));
                put(c, XTN_4h(2, lo(rb)));  put(c, W(XTN_4h(2, hi(rb))));
                put(c, XTN_8b(2, 2));
                put(c, XTN_4h(3, lo(ra_v))); put(c, W(XTN_4h(3, hi(ra_v))));
                put(c, XTN_8b(3, 3));
                put(c, LSL_xi(XT, XCOL, 2));
                put(c, ADD_xr(XT, XP, XT));
                put(c, ST4_8b(0, XT));
            }
            ra_free_chan(ra, inst->x, i);
            ra_free_chan(ra, inst->y, i);
            ra_free_chan(ra, inst->z, i);
            ra_free_chan(ra, inst->w, i);
        } break;

        case op_store_16: {
            int8_t ry = ra_ensure(ra, sl, ns, inst->y.id);
            ptr    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, 1);
            if (jc->if_depth > 0) {
                int8_t rm  = ra_ensure(ra, sl, ns, jc->if_cond_val[jc->if_depth - 1]);
                int8_t tmp = ra_alloc(ra, sl, ns);
                int8_t mn  = ra_alloc(ra, sl, ns);
                if (scalar) {
                    put(c, LDR_hx(lo(tmp), XP, XCOL));
                    put(c, XTN_4h(lo(mn), lo(rm)));
                    put(c, BIT_16b(lo(tmp), lo(ry), lo(mn)));
                    put(c, STR_hx(lo(tmp), XP, XCOL));
                } else {
                    put(c, LSL_xi(XT, XCOL, 1));
                    put(c, ADD_xr(XT, XP, XT));
                    put(c, LDR_di(lo(tmp), XT, 0));
                    put(c, LDR_di(hi(tmp), XT, 1));
                    put(c, XTN_4h(lo(mn), lo(rm)));
                    put(c, XTN_4h(hi(mn), hi(rm)));
                    put(c, BIT_16b(lo(tmp), lo(ry), lo(mn)));
                    put(c, BIT_16b(hi(tmp), hi(ry), hi(mn)));
                    put(c, STR_di(lo(tmp), XT, 0));
                    put(c, STR_di(hi(tmp), XT, 1));
                }
                ra_return_reg(ra, mn);
                ra_return_reg(ra, tmp);
            } else {
                if (scalar) {
                    put(c, STR_hx(lo(ry), XP, XCOL));
                } else {
                    put(c, LSL_xi(XT, XCOL, 1));
                    put(c, ADD_xr(XT, XP, XT));
                    put(c, STR_di(lo(ry), XT, 0));
                    put(c, STR_di(hi(ry), XT, 1));
                }
            }
            ra_free_chan(ra, inst->y, i);
        } break;

        case op_f32_from_f16: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i);
            put(c, FCVTL_4s(lo(s.rd), lo(s.rx)));
            if (!scalar) { put(c, FCVTL_4s(hi(s.rd), hi(s.rx))); }
        } break;

        case op_f16_from_f32: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i);
            put(c, FCVTN_4h(lo(s.rd), lo(s.rx)));
            if (!scalar) { put(c, FCVTN_4h(hi(s.rd), hi(s.rx))); }
        } break;

        case op_i32_from_s16: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i);
            put(c, SXTL_4s(lo(s.rd), lo(s.rx)));
            if (!scalar) { put(c, SXTL_4s(hi(s.rd), hi(s.rx))); }
        } break;

        case op_i32_from_u16: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i);
            put(c, UXTL_4s(lo(s.rd), lo(s.rx)));
            if (!scalar) { put(c, UXTL_4s(hi(s.rd), hi(s.rx))); }
        } break;

        case op_i16_from_i32: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i);
            put(c, XTN_4h(lo(s.rd), lo(s.rx)));
            if (!scalar) { put(c, XTN_4h(hi(s.rd), hi(s.rx))); }
        } break;

        case op_imm_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            arm64_pool_load(c, &jc->pool, lo(s.rd), (uint32_t)inst->imm);
            put(c, ORR_16b(hi(s.rd), lo(s.rd), lo(s.rd)));
        } break;

        case op_add_f32:
        case op_sub_f32:
        case op_mul_f32:
        case op_min_f32:
        case op_max_f32:
        case op_abs_f32:
        case op_square_f32:
        case op_round_f32:
        case op_floor_f32:
        case op_ceil_f32:
        case op_round_i32:
        case op_floor_i32:
        case op_ceil_i32:
        case op_fma_f32:
        case op_fms_f32:
        case op_square_add_f32:
        case op_square_sub_f32:
        case op_add_i32:
        case op_sub_i32:
        case op_mul_i32:
        case op_shl_i32:
        case op_shr_u32:
        case op_shr_s32:
        case op_and_32:
        case op_or_32:
        case op_xor_32:
        case op_sel_32:
        case op_f32_from_i32:
        case op_i32_from_f32:
        case op_eq_f32:
        case op_lt_f32:
        case op_le_f32:
        case op_eq_i32:
        case op_lt_s32:
        case op_le_s32:
        case op_lt_u32:
        case op_le_u32:
        {
            int nscratch = (inst->op == op_shr_u32 || inst->op == op_shr_s32) ? 1 : 0;
            struct ra_step s = ra_step_alu(ra, sl, ns, inst, i, nscratch);
            int sc_lo = s.scratch >= 0 ? lo(s.scratch) : -1;
            emit_alu_reg(c, inst->op, lo(s.rd), lo(s.rx), lo(s.ry), lo(s.rz), sc_lo);
            if (!scalar) {
                int sc_hi = s.scratch >= 0 ? hi(s.scratch) : -1;
                emit_alu_reg(c, inst->op, hi(s.rd), hi(s.rx), hi(s.ry), hi(s.rz), sc_hi);
            }
            if (s.scratch >= 0) { ra_return_reg(ra, s.scratch); }
        } break;

        case op_load_var: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int const off = 2 * inst->imm;
            put(c, LDR_qi(lo(s.rd), XS, off));
            if (!scalar) { put(c, LDR_qi(hi(s.rd), XS, off + 1)); }
        } break;

        case op_store_var: {
            int8_t ry = ra_ensure(ra, sl, ns, inst->y.id);
            if (jc->if_depth > 0) {
                // Resolve the mask register through ra_ensure every time so a
                // spill/fill between if_begin and this store refills the
                // correct (possibly AND-combined nested) mask back into a
                // register before we use it.
                int8_t rm = ra_ensure(ra, sl, ns, jc->if_cond_val[jc->if_depth - 1]);
                int8_t tmp = ra_alloc(ra, sl, ns);
                int const off = 2 * inst->imm;
                put(c, LDR_qi(lo(tmp), XS, off));
                if (!scalar) { put(c, LDR_qi(hi(tmp), XS, off + 1)); }
                put(c, BIT_16b(lo(tmp), lo(ry), lo(rm)));
                if (!scalar) { put(c, BIT_16b(hi(tmp), hi(ry), hi(rm))); }
                put(c, STR_qi(lo(tmp), XS, off));
                if (!scalar) { put(c, STR_qi(hi(tmp), XS, off + 1)); }
                ra_return_reg(ra, tmp);
            } else {
                int const off = 2 * inst->imm;
                put(c, STR_qi(lo(ry), XS, off));
                if (!scalar) { put(c, STR_qi(hi(ry), XS, off + 1)); }
            }
            ra_free_chan(ra, inst->y, i);
        } break;

        case op_if_begin: {
            int8_t rx = ra_ensure(ra, sl, ns, inst->x.id);
            if (jc->if_depth > 0) {
                // Nested if: AND the inner condition with the outer mask in
                // place so the mask val in the register is the full chain,
                // matching the interpreter's if_mask_stack AND accumulation.
                // Overwrite cond's register content; ra will spill the AND'd
                // result on eviction and ra_ensure will refill it correctly.
                int8_t outer = ra_ensure(ra, sl, ns, jc->if_cond_val[jc->if_depth - 1]);
                put(c, AND_16b(lo(rx), lo(rx), lo(outer)));
                if (!scalar) { put(c, AND_16b(hi(rx), hi(rx), hi(outer))); }
            }
            jc->if_cond_val[jc->if_depth++] = inst->x.id;
        } break;
        case op_if_end: {
            --jc->if_depth;
            // Cond val's last_use was extended to this instruction by ra_create
            // so the mask val survives the body; free it here.
            ra_free_chan(ra, ir->inst[inst->x.id].x, i);
        } break;

        case op_loop_begin: {
            int8_t rx = ra_ensure(ra, sl, ns, inst->x.id);
            put(c, UMOV_ws(XT, lo(rx)));
            ra_free_chan(ra, inst->x, i);
            ra_evict_live_before(ra, sl, ns, i);
            put(c, SUBS_xi(31, XT, 0));
            jc->loop_br_skip = c->words;
            put(c, Bcond(0xd, 0));
            jc->loop_top = c->words;
        } break;

        case op_loop_end: {
            int8_t rx = ra_ensure(ra, sl, ns, inst->x.id);
            int const off = 2 * inst->imm;
            put(c, UMOV_ws(XT, lo(rx)));
            ra_free_chan(ra, inst->x, i);
            int8_t tmp = ra_alloc(ra, sl, ns);
            put(c, LDR_qi(lo(tmp), XS, off));
            put(c, UMOV_ws(XM, lo(tmp)));
            ra_return_reg(ra, tmp);
            put(c, CMP_xr(XM, XT));
            put(c, Bcond(0xb, jc->loop_top - c->words));
            c->word[jc->loop_br_skip] = Bcond(0xd, c->words - jc->loop_br_skip);
        } break;
        }
    }
}

#if __clang__
__attribute__((no_sanitize("function")))
#endif
void jit_program_run(struct jit_program *j, int l, int t, int r, int b, struct umbra_buf buf[]) {
    j->entry(l, t, r, b, buf);
}
void jit_program_dump(struct jit_program const *j, FILE *f) {
    uint32_t const *words = (uint32_t const*)j->code;
    int      const  nw    = j->code_bytes / 4;

    char spath[] = "/tmp/umbra_dump_XXXXXX.s";
    char opath[sizeof spath + 2];
    int  fd      = mkstemps(spath, 2);
    if (fd < 0) { return; }
    FILE *sfp = fdopen(fd, "w");
    if (!sfp) { close(fd); remove(spath); return; }
    for (int i = 0; i < nw; i++) {
        fprintf(sfp, ".inst 0x%08x\n", words[i]);
    }
    fclose(sfp);
    snprintf(opath, sizeof opath, "%.*s.o", (int)(sizeof spath - 3), spath);

    char cmd[512];
    snprintf(cmd, sizeof cmd, "as -o %s %s 2>/dev/null", opath, spath);
    if (system(cmd) == 0) {
        jit_dump_with_labels(f, opath, j->label, j->labels);
    }
    remove(spath);
    remove(opath);
}

#endif
