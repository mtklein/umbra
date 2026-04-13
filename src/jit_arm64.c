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
    struct pool_ref *refs;
    int              nbytes, nrefs, cap_data, cap_refs;
};
static int pool_add(struct pool *p, void const *src, int n) {
    for (int i = 0; i + n <= p->nbytes; i += n) {
        if (__builtin_memcmp(p->data + i, src, (size_t)n) == 0) {
            return i;
        }
    }
    if (p->cap_data < p->nbytes + n) {
        p->cap_data = p->cap_data ? 2 * p->cap_data : 256;
        p->data = realloc(p->data, (size_t)p->cap_data);
    }
    int off = p->nbytes;
    __builtin_memcpy(p->data + off, src, (size_t)n);
    p->nbytes += n;
    return off;
}
static void pool_ref_at(struct pool *p, int data_off, int code_pos) {
    if (p->nrefs == p->cap_refs) {
        p->cap_refs = p->cap_refs ? 2 * p->cap_refs : 32;
        p->refs = realloc(p->refs, (size_t)p->cap_refs * sizeof *p->refs);
    }
    p->refs[p->nrefs++] = (struct pool_ref){data_off, code_pos};
}
static void pool_free(struct pool *p) {
    free(p->data);
    free(p->refs);
}

#include "asm_arm64.h"

typedef struct asm_arm64 Buf;

// X0=l, X1=t, X2=r, X3=b, X4=buf.
// X0=col_end, X1=l, X2=buf, X9=col, X14=row.
enum {
    XWIDTH = 1,   // x0_col (reset XCOL at row boundary)
    XBUF   = 2,
    XP     = 8,
    XCOL   = 9,   // column counter (was XI)
    XT     = 10,
    XH     = 11,
    XW     = 12,
    XM     = 13,
    XY     = 14,
    XS     = 15,
};
#define XI XCOL

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

static void resolve_ptr(Buf *c, ptr p, int *last_ptr, int const *deref_gpr,
                        int const *deref_rb_gpr, int elem_shift) {
    if (p.deref) {
        *last_ptr = -1;
        int const gpr = deref_gpr[p.ix],
                  rbg = deref_rb_gpr[p.ix];
        if (rbg > 0) {
            if (elem_shift) {
                put(c, LSL_xi(XT, rbg, elem_shift));
                put(c, MADD_x(XP, XY, XT, gpr));
            } else {
                put(c, MADD_x(XP, XY, rbg, gpr));
            }
        } else {
            put(c, ADD_xi(XP, gpr, 0));
        }
    } else {
        load_ptr(c, p.bits, last_ptr, elem_shift);
    }
}

static void load_count(Buf *c, ptr p) {
    if (p.deref) {
        put(c, MOVZ_x_lsl16(XM, 0x7fff));
    } else {
        int const disp = p.bits * (int)sizeof(struct umbra_buf)
                              + (int)__builtin_offsetof(struct umbra_buf, count);
        put(c, LDR_wi(XM, XBUF, disp / 4));
    }
}

static int8_t const pair_lo[] = { 4,  6, 16, 18, 20, 22, 24, 26, 28, 30,  8, 10, 12, 14};
static int8_t const pair_hi[] = { 5,  7, 17, 19, 21, 23, 25, 27, 29, 31,  9, 11, 13, 15};
static int lo(int r) { return pair_lo[r]; }
static int hi(int r) { return pair_hi[r]; }

static void emit_alu_reg(Buf *c, enum op op, int d, int x, int y, int z, int imm,
                         int scratch) {
    switch ((int)op) {
    case op_add_f32: put(c, FADD_4s(d, x, y)); break;
    case op_sub_f32: put(c, FSUB_4s(d, x, y)); break;
    case op_mul_f32: put(c, FMUL_4s(d, x, y)); break;
    case op_div_f32: put(c, FDIV_4s(d, x, y)); break;
    case op_min_f32: put(c, FMINNM_4s(d, x, y)); break;
    case op_max_f32: put(c, FMAXNM_4s(d, x, y)); break;
    case op_sqrt_f32: put(c, FSQRT_4s(d, x)); break;
    case op_abs_f32: put(c, FABS_4s(d, x)); break;
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
    case op_shl_i32_imm: put(c, SHL_4s_imm(d, x, imm)); break;
    case op_shr_u32_imm: put(c, USHR_4s_imm(d, x, imm)); break;
    case op_shr_s32_imm: put(c, SSHR_4s_imm(d, x, imm)); break;
    }
}

static int8_t const ra_pool[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};

struct jit_ctx {
    Buf                            *c;
    struct umbra_flat_ir const *bb;
    struct pool                     pool;
    int                             n_vars, loop_top, loop_br_skip, :32;
};

static void arm64_spill(int reg, int slot, void *ctx) {
    struct jit_ctx *j = ctx;
    int const off = 2 * (slot + j->n_vars);
    put(j->c, STR_qi(lo(reg), XS, off));
    put(j->c, STR_qi(hi(reg), XS, off + 1));
}
static void arm64_fill(int reg, int slot, void *ctx) {
    struct jit_ctx *j = ctx;
    int const off = 2 * (slot + j->n_vars);
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
    arm64_pool_load(j->c, &j->pool, lo(reg), (uint32_t)j->bb->inst[val].imm);
    put(j->c, ORR_16b(hi(reg), lo(reg), lo(reg)));
}

static struct ra *ra_create_arm64(struct umbra_flat_ir const *bb, struct jit_ctx *jc) {
    struct ra_config cfg = {
        .pool = ra_pool,
        .nregs = 14,
        .max_reg = 14,
        .spill = arm64_spill,
        .fill = arm64_fill,
        .remat = arm64_remat,
        .ctx = jc,
    };
    return ra_create(bb, &cfg);
}

static void emit_ops(Buf *c, struct umbra_flat_ir const *bb, int from, int to,
                     int *sl, int *ns, struct ra *ra, _Bool scalar, int *deref_gpr,
                     int *deref_rb_gpr, struct jit_ctx *jc);

struct jit_program *jit_program(struct jit_backend *be,
                                           struct umbra_flat_ir const *bb) {

    int *sl = malloc((size_t)bb->insts * sizeof(int));
    for (int i = 0; i < bb->insts; i++) { sl[i] = -1; }
    int  ns = 0;
    int *deref_gpr    = calloc((size_t)bb->insts, sizeof(int));
    int *deref_rb_gpr = calloc((size_t)bb->insts, sizeof(int));

    size_t const pg  = 16384,
                 est = (size_t)(bb->insts * 40 + 256);
    void  *buf_mem;
    size_t buf_size;
    acquire_code_buf(be, &buf_mem, &buf_size, est + pg);

    Buf c = {
        .word      = buf_mem,
        .words     = 0,
        .cap       = (int)((buf_size - pg) / 4),
        .mmap_size = buf_size,
    };
    struct jit_ctx jc = {.c = &c, .bb = bb, .pool = {0}, .n_vars = bb->n_vars};
    struct ra     *ra = ra_create_arm64(bb, &jc);

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

    emit_ops(&c, bb, 0, bb->preamble, sl, &ns, ra, 0, deref_gpr, deref_rb_gpr, &jc);

    ra_begin_loop(ra);

    // 2D loop: XCOL = column, XY = row, X0 = col_end.
    put(&c, ADD_xi(XM, XY, 0));       // XM = b = end_row (XY saved pre-preamble)
    put(&c, ADD_xi(0, XW, 0));        // x0 = col_end = r
    put(&c, ADD_xi(XWIDTH, XH, 0));   // XWIDTH = l
    put(&c, ADD_xi(XY, XCOL, 0));     // XY = t
    put(&c, ADD_xi(XCOL, XH, 0));     // XCOL = l
    put(&c, STP_pre(XM, 15, 31, -2)); // push end_row

    int const loop_top = c.words;

    // remaining = col_end - XCOL
    put(&c, SUB_xr(XT, 0, XCOL));
    put(&c, SUBS_xi(31, XT, 8));
    int const br_tail = c.words;
    put(&c, Bcond(0xb, 0));

    if (bb->n_vars > 0) {
        int8_t zr = ra_alloc(ra, sl, &ns);
        put(&c, EOR_16b(lo(zr), lo(zr), lo(zr)));
        for (int vi = 0; vi < bb->n_vars; vi++) {
            put(&c, STP_qi(lo(zr), lo(zr), XS, 2 * vi));
        }
        ra_return_reg(ra, zr);
    }

    int const loop_body_start = c.words;
    emit_ops(&c, bb, bb->preamble, bb->insts, sl, &ns, ra, 0, deref_gpr, deref_rb_gpr, &jc);

    ra_end_loop(ra, sl);

    int const loop_body_end = c.words;

    put(&c, ADD_xi(XCOL, XCOL, 8));
    put(&c, B(loop_top - c.words));

    int const tail_top = c.words;
    c.word[br_tail] = Bcond(0xb, tail_top - br_tail);

    put(&c, CMP_xr(XCOL, 0));
    int const br_row_done = c.words;
    put(&c, Bcond(0xa, 0));  // B.GE row_done

    ra_reset_pool(ra);
    for (int i = 0; i < bb->insts; i++) { sl[i] = -1; }

    emit_ops(&c, bb, 0, bb->preamble, sl, &ns, ra, 0, deref_gpr, deref_rb_gpr, &jc);
    if (bb->n_vars > 0) {
        int8_t zr = ra_alloc(ra, sl, &ns);
        put(&c, EOR_16b(lo(zr), lo(zr), lo(zr)));
        for (int vi = 0; vi < bb->n_vars; vi++) {
            put(&c, STP_qi(lo(zr), lo(zr), XS, 2 * vi));
        }
        ra_return_reg(ra, zr);
    }
    emit_ops(&c, bb, bb->preamble, bb->insts, sl, &ns, ra, 1, deref_gpr, deref_rb_gpr, &jc);

    put(&c, ADD_xi(XCOL, XCOL, 1));
    put(&c, B(tail_top - c.words));

    int const row_done = c.words;
    c.word[br_row_done] = Bcond(0xa, row_done - br_row_done);

    put(&c, ADD_xi(XY, XY, 1));
    put(&c, ADD_xi(XCOL, XWIDTH, 0));    // XCOL = x0_col
    // LDR XT, [SP] -- load saved end_row
    put(&c, LDR_xi(XT, 31, 0));
    put(&c, CMP_xr(XY, XT));
    int const br_more_rows = c.words;
    put(&c, Bcond(0xb, 0));  // B.LT -> more rows

    // Restore stack (saved end_row)
    put(&c, LDP_post(0, 15, 31, 2));

    // Patch: B.LT -> loop_top
    c.word[br_more_rows] = Bcond(0xb, loop_top - br_more_rows);

    put(&c, ADD_xi(31, 29, 0));
    // Restore callee-saved Q8-Q15 from [FP-128, FP) using negative offsets.
    put(&c, LDP_qi( 8,  9, 31, -8));
    put(&c, LDP_qi(10, 11, 31, -6));
    put(&c, LDP_qi(12, 13, 31, -4));
    put(&c, LDP_qi(14, 15, 31, -2));
    put(&c, LDP_post(29, 30, 31, 2));
    put(&c, RET());

    if (ns + bb->n_vars > 0) {
        c.word[stack_patch] = SUB_xi(31, 31, (ns + bb->n_vars) * 32);
    }
    c.word[stack_patch + 1] = ADD_xi(XS, 31, 0);
    while (c.words & 3) {
        put(&c, NOP());
    }
    int const pool_start = c.words;
    for (int pi = 0; pi < jc.pool.nbytes; pi += 4) {
        uint32_t w;
        __builtin_memcpy(&w, jc.pool.data + pi, 4);
        put(&c, w);
    }
    for (int pi = 0; pi < jc.pool.nrefs; pi++) {
        struct pool_ref *r = &jc.pool.refs[pi];
        int const word_off = pool_start + r->data_off / 4,
                  imm19    = word_off - r->code_pos;
        c.word[r->code_pos] = 0x9c000000u
            | ((uint32_t)(imm19 & 0x7ffff) << 5)
            | (c.word[r->code_pos] & 0x1fu);
    }
    pool_free(&jc.pool);

    ra_destroy(ra);
    free(sl);
    free(deref_gpr);
    free(deref_rb_gpr);

    size_t const code_sz = (size_t)c.words * 4,
                 alloc   = (code_sz + pg - 1) & ~(pg - 1);

    if (alloc + pg < c.mmap_size) {
        mprotect((char *)c.word + alloc + pg, c.mmap_size - alloc - pg, PROT_NONE);
    }
    mprotect((char *)c.word + alloc, pg, PROT_NONE);
    { int const ok = mprotect(c.word, alloc, PROT_READ | PROT_EXEC); assume(ok == 0); }
    __builtin___clear_cache(c.word, (char *)c.word + code_sz);

    struct jit_program *j = malloc(sizeof *j);
    j->code = c.word;
    j->code_size = c.mmap_size;
    j->loop_start = loop_body_start;
    j->loop_end = loop_body_end;
    {
        union {
            void *p;
            void (*fn)(int, int, int, int, struct umbra_buf *);
        } u = {.p = c.word};
        j->entry = u.fn;
    }
    return j;
}

static void emit_ops(Buf *c, struct umbra_flat_ir const *bb, int from, int to,
                     int *sl, int *ns, struct ra *ra, _Bool scalar, int *deref_gpr,
                     int *deref_rb_gpr, struct jit_ctx *jc) {
    int last_ptr = -1;
    int dc = 0;

    for (int i = from; i < to; i++) {
        struct ir_inst const *inst = &bb->inst[i];

        switch (inst->op) {
        case op_deref_ptr: {
            load_ptr(c, inst->ptr.bits, &last_ptr, 2);
            int gpr    = 3 + dc;
            int rb_gpr = dc < 2 ? 6 + dc : 0;
            dc++;
            deref_gpr[i]    = gpr;
            deref_rb_gpr[i] = rb_gpr;
            put(c, LDR_xi(gpr, XP, inst->imm / 2));
            if (rb_gpr) {
                put(c, LDR_wi(rb_gpr, XP, inst->imm + 3));
            }
        } break;

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
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr, 2);
            if (scalar) {
                put(c, LDR_sx(lo(s.rd), XP, XI));
            } else {
                put(c, LSL_xi(XT, XI, 2));
                put(c, ADD_xr(XT, XP, XT));
                put(c, LDP_qi(lo(s.rd), hi(s.rd), XT, 0));
            }
        } break;

        case op_load_16: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            ptr            p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr, 1);
            if (scalar) {
                put(c, LDRH_wr(XT, XP, XI));
                put(c, DUP_4s_w(lo(s.rd), XT));
            } else {
                put(c, LSL_xi(XT, XI, 1));
                put(c, ADD_xr(XT, XP, XT));
                put(c, LDR_di(lo(s.rd), XT, 0));
                put(c, LDR_di(hi(s.rd), XT, 1));
            }
        } break;

        case op_uniform_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            ptr            p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr, 2);
            put(c, LDR_si(lo(s.rd), XP, inst->imm));
            put(c, DUP_4s_lane(lo(s.rd), lo(s.rd), 0));
            put(c, ORR_16b(hi(s.rd), lo(s.rd), lo(s.rd)));
        } break;

        case op_gather_uniform_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int8_t         rx = ra_ensure(ra, sl, ns, inst->x.id);
            ra_free_chan(ra, inst->x, i);
            ptr p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr, 2);
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
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr, 2);
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

        case op_sample_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int8_t         rx = ra_ensure(ra, sl, ns, inst->x.id);
            int8_t hi_r  = ra_alloc(ra, sl, ns);
            int8_t frac  = ra_alloc(ra, sl, ns);
            ptr    p     = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr, 2);
            load_count(c, p);
            if (scalar) {
                put(c, FRINTM_4s(lo(hi_r), lo(rx)));
                put(c, FSUB_4s(lo(frac), lo(rx), lo(hi_r)));
                ra_free_chan(ra, inst->x, i);
                put(c, FCVTZS_4s(lo(hi_r), lo(hi_r)));
                put(c, UMOV_ws(XT, lo(hi_r)));
                put(c, MOVI_4s(lo(s.rd), 0, 0));
                put(c, CMP_wr(XT, XM));
                put(c, Bcond(0x2, 2));
                put(c, LDR_sx(lo(s.rd), XP, XT));
                put(c, ADD_xi(XT, XT, 1));
                put(c, MOVI_4s(lo(hi_r), 0, 0));
                put(c, CMP_wr(XT, XM));
                put(c, Bcond(0x2, 2));
                put(c, LDR_sx(lo(hi_r), XP, XT));
                put(c, FSUB_4s(lo(hi_r), lo(hi_r), lo(s.rd)));
                put(c, FMLA_4s(lo(s.rd), lo(hi_r), lo(frac)));
            } else {
                put(c, FRINTM_4s(lo(hi_r), lo(rx)));
                put(c, FSUB_4s(lo(frac), lo(rx), lo(hi_r)));
                put(c, FRINTM_4s(hi(hi_r), hi(rx)));
                put(c, FSUB_4s(hi(frac), hi(rx), hi(hi_r)));
                ra_free_chan(ra, inst->x, i);
                int8_t int_ix = ra_alloc(ra, sl, ns);
                put(c, FCVTZS_4s(lo(int_ix), lo(hi_r)));
                put(c, FCVTZS_4s(hi(int_ix), hi(hi_r)));
                put(c, MOVI_4s(lo(s.rd), 0, 0));
                put(c, MOVI_4s(lo(hi_r), 0, 0));
                for (int k = 0; k < 4; k++) {
                    put(c, UMOV_ws_lane(XT, lo(int_ix), k));
                    put(c, CMP_wr(XT, XM));
                    put(c, Bcond(0x2, 4));
                    put(c, LSL_xi(XT, XT, 2));
                    put(c, ADD_xr(XT, XP, XT));
                    put(c, LD1_s(lo(s.rd), k, XT));
                    put(c, UMOV_ws_lane(XT, lo(int_ix), k));
                    put(c, ADD_xi(XT, XT, 1));
                    put(c, CMP_wr(XT, XM));
                    put(c, Bcond(0x2, 4));
                    put(c, LSL_xi(XT, XT, 2));
                    put(c, ADD_xr(XT, XP, XT));
                    put(c, LD1_s(lo(hi_r), k, XT));
                }
                put(c, FSUB_4s(lo(hi_r), lo(hi_r), lo(s.rd)));
                put(c, FMLA_4s(lo(s.rd), lo(hi_r), lo(frac)));
                put(c, MOVI_4s(hi(s.rd), 0, 0));
                put(c, MOVI_4s(hi(hi_r), 0, 0));
                for (int k = 0; k < 4; k++) {
                    put(c, UMOV_ws_lane(XT, hi(int_ix), k));
                    put(c, CMP_wr(XT, XM));
                    put(c, Bcond(0x2, 4));
                    put(c, LSL_xi(XT, XT, 2));
                    put(c, ADD_xr(XT, XP, XT));
                    put(c, LD1_s(hi(s.rd), k, XT));
                    put(c, UMOV_ws_lane(XT, hi(int_ix), k));
                    put(c, ADD_xi(XT, XT, 1));
                    put(c, CMP_wr(XT, XM));
                    put(c, Bcond(0x2, 4));
                    put(c, LSL_xi(XT, XT, 2));
                    put(c, ADD_xr(XT, XP, XT));
                    put(c, LD1_s(hi(hi_r), k, XT));
                }
                ra_return_reg(ra, int_ix);
                put(c, FSUB_4s(hi(hi_r), hi(hi_r), hi(s.rd)));
                put(c, FMLA_4s(hi(s.rd), hi(hi_r), hi(frac)));
            }
            ra_return_reg(ra, hi_r);
            ra_return_reg(ra, frac);
        } break;

        case op_gather_16: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int8_t         rx = ra_ensure(ra, sl, ns, inst->x.id);
            ra_free_chan(ra, inst->x, i);
            ptr p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr, 1);
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
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr, 2);
            if (scalar) {
                put(c, STR_sx(lo(ry), XP, XI));
            } else {
                put(c, LSL_xi(XT, XI, 2));
                put(c, ADD_xr(XT, XP, XT));
                put(c, STP_qi(lo(ry), hi(ry), XT, 0));
            }
            ra_free_chan(ra, inst->y, i);
        } break;

        case op_load_16x4: {
            ptr    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr, 3);
            struct ra_step s0 = ra_step_alloc(ra, sl, ns, i);
            int8_t r1 = ra_alloc(ra, sl, ns);
            int8_t r2 = ra_alloc(ra, sl, ns);
            int8_t r3 = ra_alloc(ra, sl, ns);
            if (scalar) {
                put(c, LSL_xi(XT, XI, 3));
                put(c, ADD_xr(XT, XP, XT));
                put(c, LDR_hi(lo(s0.rd), XT, 0));
                put(c, LDR_hi(lo(r1),    XT, 1));
                put(c, LDR_hi(lo(r2),    XT, 2));
                put(c, LDR_hi(lo(r3),    XT, 3));
            } else {
                put(c, LSL_xi(XT, XI, 3));
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
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr, 1);
            int8_t px = ra_alloc(ra, sl, ns);
            {
                int const count_off = p.bits * (int)sizeof(struct umbra_buf)
                                    + (int)__builtin_offsetof(struct umbra_buf, count);
                put(c, LDR_wi(XT, XBUF, count_off / 4));
                put(c, LSR_xi(XT, XT, 1));
            }
            if (scalar) {
                put(c, LDR_hx(lo(s0.rd), XP, XI));
                put(c, ADD_xr(XP, XP, XT));
                put(c, LDR_hx(lo(r1), XP, XI));
                put(c, ADD_xr(XP, XP, XT));
                put(c, LDR_hx(lo(r2), XP, XI));
                put(c, ADD_xr(XP, XP, XT));
                put(c, LDR_hx(lo(r3), XP, XI));
            } else {
                put(c, LSL_xi(XM, XI, 1));
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
            int8_t rb_  = ra_ensure_chan(ra, sl, ns, inst->z.id, (int)inst->z.chan);
            int8_t ra_v = ra_ensure_chan(ra, sl, ns, inst->w.id, (int)inst->w.chan);
            ptr    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr, 3);
            if (scalar) {
                int8_t px = ra_alloc(ra, sl, ns);
                int8_t t  = ra_alloc(ra, sl, ns);
                int8_t z  = ra_alloc(ra, sl, ns);
                put(c, ZIP1_8h(lo(t), lo(rr), lo(rg)));
                put(c, ZIP1_8h(lo(px), lo(rb_), lo(ra_v)));
                put(c, ZIP1_4s(lo(z), lo(t), lo(px)));
                put(c, LSL_xi(XT, XI, 3));
                put(c, ADD_xr(XT, XP, XT));
                put(c, STR_di(lo(z), XT, 0));
                ra_return_reg(ra, z);
                ra_return_reg(ra, t);
                ra_return_reg(ra, px);
            } else {
                put(c, ORR_16b(0, lo(rr),   lo(rr)));
                put(c, ORR_16b(1, lo(rg),   lo(rg)));
                put(c, ORR_16b(2, lo(rb_),  lo(rb_)));
                put(c, ORR_16b(3, lo(ra_v), lo(ra_v)));
                put(c, LSL_xi(XT, XI, 3));
                put(c, ADD_xr(XT, XP, XT));
                put(c, ST4_4h(0, XT));
                put(c, ORR_16b(0, hi(rr),   hi(rr)));
                put(c, ORR_16b(1, hi(rg),   hi(rg)));
                put(c, ORR_16b(2, hi(rb_),  hi(rb_)));
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
            int8_t rb_  = ra_ensure_chan(ra, sl, ns, inst->z.id, (int)inst->z.chan);
            int8_t ra_v = ra_ensure_chan(ra, sl, ns, inst->w.id, (int)inst->w.chan);
            ptr    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr, 1);
            {
                int const count_off = p.bits * (int)sizeof(struct umbra_buf)
                                    + (int)__builtin_offsetof(struct umbra_buf, count);
                put(c, LDR_wi(XT, XBUF, count_off / 4));
                put(c, LSR_xi(XT, XT, 1));
            }
            if (scalar) {
                put(c, STR_hx(lo(rr), XP, XI));
                put(c, ADD_xr(XP, XP, XT));
                put(c, STR_hx(lo(rg), XP, XI));
                put(c, ADD_xr(XP, XP, XT));
                put(c, STR_hx(lo(rb_), XP, XI));
                put(c, ADD_xr(XP, XP, XT));
                put(c, STR_hx(lo(ra_v), XP, XI));
            } else {
                put(c, LSL_xi(XM, XI, 1));
                put(c, ADD_xr(XM, XP, XM));
                put(c, STR_di(lo(rr), XM, 0));
                put(c, STR_di(hi(rr), XM, 1));
                put(c, ADD_xr(XM, XM, XT));
                put(c, STR_di(lo(rg), XM, 0));
                put(c, STR_di(hi(rg), XM, 1));
                put(c, ADD_xr(XM, XM, XT));
                put(c, STR_di(lo(rb_), XM, 0));
                put(c, STR_di(hi(rb_), XM, 1));
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
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr, 2);
            if (scalar) {
                int8_t px   = ra_alloc(ra, sl, ns);
                int8_t mask = ra_alloc(ra, sl, ns);
                put(c, MOVI_4s(lo(mask), 0xFF, 0));
                put(c, LDR_sx(lo(px), XP, XI));
                put(c, AND_16b(lo(s0.rd), lo(px), lo(mask)));
                put(c, USHR_4s_imm(lo(r1), lo(px), 8));  put(c, AND_16b(lo(r1), lo(r1), lo(mask)));
                put(c, USHR_4s_imm(lo(r2), lo(px), 16)); put(c, AND_16b(lo(r2), lo(r2), lo(mask)));
                put(c, USHR_4s_imm(lo(r3), lo(px), 24));
                ra_return_reg(ra, mask);
                ra_return_reg(ra, px);
            } else {
                put(c, LSL_xi(XT, XI, 2));
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
            int8_t rb_  = ra_ensure_chan(ra, sl, ns, inst->z.id, (int)inst->z.chan);
            int8_t ra_v = ra_ensure_chan(ra, sl, ns, inst->w.id, (int)inst->w.chan);
            ptr    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr, 2);
            if (scalar) {
                int8_t px = ra_alloc(ra, sl, ns);
                int8_t t  = ra_alloc(ra, sl, ns);
                put(c, SHL_4s_imm(lo(t), lo(rg), 8));    put(c, ORR_16b(lo(px), lo(rr), lo(t)));
                put(c, SHL_4s_imm(lo(t), lo(rb_), 16));  put(c, ORR_16b(lo(px), lo(px), lo(t)));
                put(c, SHL_4s_imm(lo(t), lo(ra_v), 24)); put(c, ORR_16b(lo(px), lo(px), lo(t)));
                put(c, STR_sx(lo(px), XP, XI));
                ra_return_reg(ra, t);
                ra_return_reg(ra, px);
            } else {
                // Narrow u32->u16->u8 per channel into V0-V3, then ST4.8B.
                put(c, XTN_4h(0, lo(rr)));   put(c, W(XTN_4h(0, hi(rr))));
                put(c, XTN_8b(0, 0));
                put(c, XTN_4h(1, lo(rg)));   put(c, W(XTN_4h(1, hi(rg))));
                put(c, XTN_8b(1, 1));
                put(c, XTN_4h(2, lo(rb_)));  put(c, W(XTN_4h(2, hi(rb_))));
                put(c, XTN_8b(2, 2));
                put(c, XTN_4h(3, lo(ra_v))); put(c, W(XTN_4h(3, hi(ra_v))));
                put(c, XTN_8b(3, 3));
                put(c, LSL_xi(XT, XI, 2));
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
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr, 1);
            if (scalar) {
                put(c, STR_hx(lo(ry), XP, XI));
            } else {
                put(c, LSL_xi(XT, XI, 1));
                put(c, ADD_xr(XT, XP, XT));
                put(c, STR_di(lo(ry), XT, 0));
                put(c, STR_di(hi(ry), XT, 1));
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
        case op_div_f32:
        case op_min_f32:
        case op_max_f32:
        case op_sqrt_f32:
        case op_abs_f32:
        case op_round_f32:
        case op_floor_f32:
        case op_ceil_f32:
        case op_round_i32:
        case op_floor_i32:
        case op_ceil_i32:
        case op_fma_f32:
        case op_fms_f32:
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
            emit_alu_reg(c, inst->op, lo(s.rd), lo(s.rx), lo(s.ry), lo(s.rz),
                         inst->imm, sc_lo);
            if (!scalar) {
                int sc_hi = s.scratch >= 0 ? hi(s.scratch) : -1;
                emit_alu_reg(c, inst->op, hi(s.rd), hi(s.rx), hi(s.ry), hi(s.rz),
                             inst->imm, sc_hi);
            }
            if (s.scratch >= 0) { ra_return_reg(ra, s.scratch); }
        } break;

        case op_shl_i32_imm:
        case op_shr_u32_imm:
        case op_shr_s32_imm: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i);
            emit_alu_reg(c, inst->op, lo(s.rd), lo(s.rx), 0, 0, inst->imm, -1);
            if (!scalar) {
                emit_alu_reg(c, inst->op, hi(s.rd), hi(s.rx), 0, 0, inst->imm, -1);
            }
        } break;

        case op_and_32_imm:
        case op_add_f32_imm:
        case op_sub_f32_imm:
        case op_mul_f32_imm:
        case op_div_f32_imm:
        case op_min_f32_imm:
        case op_max_f32_imm:
        case op_add_i32_imm:
        case op_sub_i32_imm:
        case op_mul_i32_imm:
        case op_or_32_imm:
        case op_xor_32_imm:
        case op_eq_f32_imm:
        case op_lt_f32_imm:
        case op_le_f32_imm:
        case op_eq_i32_imm:
        case op_lt_s32_imm:
        case op_le_s32_imm: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i);
            int8_t ir = ra_ensure(ra, sl, ns, inst->y.id);
            ra_free_chan(ra, inst->y, i);
            enum op o = inst->op;
            uint32_t w =
                o == op_add_f32_imm ? FADD_4s(lo(s.rd), lo(s.rx), lo(ir))  :
                o == op_sub_f32_imm ? FSUB_4s(lo(s.rd), lo(s.rx), lo(ir))  :
                o == op_mul_f32_imm ? FMUL_4s(lo(s.rd), lo(s.rx), lo(ir))  :
                o == op_div_f32_imm ? FDIV_4s(lo(s.rd), lo(s.rx), lo(ir))  :
                o == op_min_f32_imm ? FMINNM_4s(lo(s.rd), lo(s.rx), lo(ir)):
                o == op_max_f32_imm ? FMAXNM_4s(lo(s.rd), lo(s.rx), lo(ir)):
                o == op_add_i32_imm ? ADD_4s(lo(s.rd), lo(s.rx), lo(ir))   :
                o == op_sub_i32_imm ? SUB_4s(lo(s.rd), lo(s.rx), lo(ir))   :
                o == op_mul_i32_imm ? MUL_4s(lo(s.rd), lo(s.rx), lo(ir))   :
                o == op_and_32_imm  ? AND_16b(lo(s.rd), lo(s.rx), lo(ir))  :
                o == op_or_32_imm   ? ORR_16b(lo(s.rd), lo(s.rx), lo(ir))  :
                o == op_xor_32_imm  ? EOR_16b(lo(s.rd), lo(s.rx), lo(ir))  :
                o == op_eq_f32_imm  ? FCMEQ_4s(lo(s.rd), lo(ir), lo(s.rx)) :
                o == op_lt_f32_imm  ? FCMGT_4s(lo(s.rd), lo(ir), lo(s.rx)) :
                o == op_le_f32_imm  ? FCMGE_4s(lo(s.rd), lo(ir), lo(s.rx)) :
                o == op_eq_i32_imm  ? CMEQ_4s(lo(s.rd), lo(ir), lo(s.rx))  :
                o == op_lt_s32_imm  ? CMGT_4s(lo(s.rd), lo(ir), lo(s.rx))  :
                                      CMGE_4s(lo(s.rd), lo(ir), lo(s.rx));
            put(c, w);
            if (!scalar) {
                uint32_t w2 =
                    o == op_add_f32_imm ? FADD_4s(hi(s.rd), hi(s.rx), hi(ir))  :
                    o == op_sub_f32_imm ? FSUB_4s(hi(s.rd), hi(s.rx), hi(ir))  :
                    o == op_mul_f32_imm ? FMUL_4s(hi(s.rd), hi(s.rx), hi(ir))  :
                    o == op_div_f32_imm ? FDIV_4s(hi(s.rd), hi(s.rx), hi(ir))  :
                    o == op_min_f32_imm ? FMINNM_4s(hi(s.rd), hi(s.rx), hi(ir)):
                    o == op_max_f32_imm ? FMAXNM_4s(hi(s.rd), hi(s.rx), hi(ir)):
                    o == op_add_i32_imm ? ADD_4s(hi(s.rd), hi(s.rx), hi(ir))   :
                    o == op_sub_i32_imm ? SUB_4s(hi(s.rd), hi(s.rx), hi(ir))   :
                    o == op_mul_i32_imm ? MUL_4s(hi(s.rd), hi(s.rx), hi(ir))   :
                    o == op_and_32_imm  ? AND_16b(hi(s.rd), hi(s.rx), hi(ir))  :
                    o == op_or_32_imm   ? ORR_16b(hi(s.rd), hi(s.rx), hi(ir))  :
                    o == op_xor_32_imm  ? EOR_16b(hi(s.rd), hi(s.rx), hi(ir))  :
                    o == op_eq_f32_imm  ? FCMEQ_4s(hi(s.rd), hi(ir), hi(s.rx)) :
                    o == op_lt_f32_imm  ? FCMGT_4s(hi(s.rd), hi(ir), hi(s.rx)) :
                    o == op_le_f32_imm  ? FCMGE_4s(hi(s.rd), hi(ir), hi(s.rx)) :
                    o == op_eq_i32_imm  ? CMEQ_4s(hi(s.rd), hi(ir), hi(s.rx))  :
                    o == op_lt_s32_imm  ? CMGT_4s(hi(s.rd), hi(ir), hi(s.rx))  :
                                          CMGE_4s(hi(s.rd), hi(ir), hi(s.rx));
                put(c, w2);
            }
        } break;

        case op_load_var: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int const off = 2 * inst->imm;
            put(c, LDR_qi(lo(s.rd), XS, off));
            if (!scalar) { put(c, LDR_qi(hi(s.rd), XS, off + 1)); }
        } break;

        case op_store_var: {
            int8_t ry = ra_ensure(ra, sl, ns, inst->y.id);
            int const off = 2 * inst->imm;
            put(c, STR_qi(lo(ry), XS, off));
            if (!scalar) { put(c, STR_qi(hi(ry), XS, off + 1)); }
            ra_free_chan(ra, inst->y, i);
        } break;

        case op_if_begin: {
            ra_free_chan(ra, inst->x, i);
        } break;
        case op_if_end: break;

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
#undef lu
}

#if __clang__
__attribute__((no_sanitize("function")))
#endif
void jit_program_run(struct jit_program *j, int l, int t, int r, int b, struct umbra_buf buf[]) {
    j->entry(l, t, r, b, buf);
}
void jit_program_dump(struct jit_program const *j, FILE *f) {
    if (j->loop_start >= j->loop_end) { return; }

    uint32_t const *words = (uint32_t const*)j->code;

    char tmp[]      = "/tmp/umbra_mca_XXXXXX.s";
    char asm_path[] = "/tmp/umbra_mca_loop_XXXXXX.s";
    char opath[sizeof tmp + 2];
    _Bool have_tmp = 0, have_asm = 0;

    int   fd, afd;
    FILE *fp, *afp;

    fd = mkstemps(tmp, 2);
    if (fd < 0) { goto done; }
    have_tmp = 1;
    fp = fdopen(fd, "w");
    if (!fp) { close(fd); goto done; }
    for (int i = j->loop_start; i < j->loop_end; i++) {
        fprintf(fp, ".inst 0x%08x\n", words[i]);
    }
    fclose(fp);
    snprintf(opath, sizeof opath, "%.*s.o", (int)(sizeof tmp - 3), tmp);

    afd = mkstemps(asm_path, 2);
    if (afd < 0) { goto done; }
    have_asm = 1;

    {
        char cmd[1024];
        snprintf(cmd, sizeof cmd,
                 "as -o %s %s 2>/dev/null &&"
                 " /opt/homebrew/opt/llvm/bin/llvm-objdump -d"
                 " --no-show-raw-insn --no-leading-addr %s 2>/dev/null",
                 opath, tmp, opath);
        FILE *p = popen(cmd, "r");
        if (!p) { close(afd); goto done; }
        afp = fdopen(afd, "w");
        char  line[256];
        _Bool past_header = 0;
        while (fgets(line, (int)sizeof line, p)) {
            if (past_header) {
                if (line[0] != '\n' && line[0] != '<') {
                    char *angle = __builtin_strchr(line, '<');
                    if (angle) {
                        *angle = '\n';
                        angle[1] = '\0';
                    }
                    fputs(line, afp);
                }
            } else if (__builtin_strstr(line, "<")) {
                past_header = 1;
            }
        }
        pclose(p);
        fclose(afp);
    }

    {
        char cmd[1024];
        snprintf(cmd, sizeof cmd,
                 "/opt/homebrew/opt/llvm/bin/llvm-mca"
                 " -mcpu=apple-m4 -iterations=100 -bottleneck-analysis"
                 " %s 2>&1",
                 asm_path);
        FILE *p = popen(cmd, "r");
        if (p) {
            char line[256];
            while (fgets(line, (int)sizeof line, p)) {
                fputs(line, f);
            }
            pclose(p);
        }
    }

done:
    if (have_tmp) { remove(tmp); remove(opath); }
    if (have_asm) { remove(asm_path); }
}

#endif
