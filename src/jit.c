#include "assume.h"
#include "bb.h"

#if !defined(__aarch64__) && !defined(__AVX2__)

struct umbra_backend *umbra_backend_jit(void) { return 0; }

#else

#include "ra.h"

static void free_chan(struct ra *ra, val_ operand, int i) {
    int id = (int)operand.id, ch = (int)operand.chan;
    if (ch) {
        if (ra_chan_last_use(ra, id, ch) <= i) {
            int8_t r = ra_chan_reg(ra, id, ch);
            if (r >= 0) { ra_return_reg(ra, r); ra_set_chan_reg(ra, id, ch, -1); }
        }
    } else {
        if (ra_last_use(ra, id) <= i) { ra_free_reg(ra, id); }
    }
}

#if defined(__aarch64__)
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <pthread.h>
#include <unistd.h>

_Static_assert(sizeof(struct umbra_buf) == 32, "");
_Static_assert(offsetof(struct umbra_buf, ptr)       ==  0, "");
_Static_assert(offsetof(struct umbra_buf, sz)        ==  8, "");
_Static_assert(offsetof(struct umbra_buf, row_bytes) == 16, "");

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

typedef struct Buf Buf;

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

static void load_ptr(Buf *c, int p, int *last_ptr) {
    if (*last_ptr != p) {
        *last_ptr = p;
        int disp_ptr = p * (int)sizeof(struct umbra_buf);
        int disp_rb  = p * (int)sizeof(struct umbra_buf) + (int)__builtin_offsetof(struct umbra_buf, row_bytes);
        put(c, LDR_xi(XP, XBUF, disp_ptr / 8));
        put(c, LDR_xi(XT, XBUF, disp_rb  / 8));
        put(c, MADD_x(XP, XY, XT, XP));
    }
}

static void resolve_ptr(Buf *c, int p, int *last_ptr, int const *deref_gpr,
                        int const *deref_rb_gpr) {
    if (ptr_is_deref(p)) {
        *last_ptr = -1;
        int gpr = deref_gpr[ptr_ix(p)];
        int rbg = deref_rb_gpr[ptr_ix(p)];
        if (rbg > 0) {
            put(c, MADD_x(XP, XY, rbg, gpr));
        } else {
            put(c, ADD_xi(XP, gpr, 0));
        }
    } else {
        load_ptr(c, p, last_ptr);
    }
}

static void load_count(Buf *c, int p, int elem_shift, int const *deref_gpr) {
    if (ptr_is_deref(p)) {
        (void)deref_gpr;
        put(c, MOVZ_x_lsl16(XM, 0x7fff));
    } else {
        int disp = p * (int)sizeof(struct umbra_buf) + (int)__builtin_offsetof(struct umbra_buf, sz);
        put(c, LDR_xi(XM, XBUF, disp / 8));
        if (elem_shift) {
            put(c, LSR_wi(XM, XM, elem_shift));
        }
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
    struct umbra_basic_block const *bb;
    struct pool                     pool;
};

static void arm64_spill(int reg, int slot, void *ctx) {
    Buf *c = ((struct jit_ctx *)ctx)->c;
    put(c, STR_qi(lo(reg), XS, 2*slot));
    put(c, STR_qi(hi(reg), XS, 2*slot+1));
}
static void arm64_fill(int reg, int slot, void *ctx) {
    Buf *c = ((struct jit_ctx *)ctx)->c;
    put(c, LDR_qi(lo(reg), XS, 2*slot));
    put(c, LDR_qi(hi(reg), XS, 2*slot+1));
}
static _Bool arm64_movi(Buf *c, int d, uint32_t v) {
    if (v == 0) {
        put(c, MOVI_4s(d, 0, 0));
    } else if (v == (v & 0x000000ffu)) {
        put(c, MOVI_4s(d, (uint8_t)v, 0));
    } else if (v == (v & 0x0000ff00u)) {
        put(c, MOVI_4s(d, (uint8_t)(v >> 8), 8));
    } else if (v == (v & 0x00ff0000u)) {
        put(c, MOVI_4s(d, (uint8_t)(v >> 16), 16));
    } else if (v == (v & 0xff000000u)) {
        put(c, MOVI_4s(d, (uint8_t)(v >> 24), 24));
    } else if ((~v) == ((~v) & 0x000000ffu)) {
        put(c, MVNI_4s(d, (uint8_t)~v, 0));
    } else if ((~v) == ((~v) & 0x0000ff00u)) {
        put(c, MVNI_4s(d, (uint8_t)(~v >> 8), 8));
    } else if ((~v) == ((~v) & 0x00ff0000u)) {
        put(c, MVNI_4s(d, (uint8_t)(~v >> 16), 16));
    } else if ((~v) == ((~v) & 0xff000000u)) {
        put(c, MVNI_4s(d, (uint8_t)(~v >> 24), 24));
    } else {
        uint32_t s = v >> 31, e = (v >> 23) & 0xffu, f = v & 0x7fffffu;
        if ((f & 0x7ffffu) == 0 && e >= 124 && e <= 131) {
            uint32_t E = e - 124;
            uint32_t imm8 =
                (s << 7) | (((~E >> 2) & 1) << 6) | ((E & 3) << 4) | ((f >> 19) & 0xf);
            put(c, 0x4f00f400u | ((imm8 >> 5) << 16) | ((imm8 & 0x1fu) << 5) | (uint32_t)d);
        } else {
            return 0;
        }
    }
    return 1;
}
static void arm64_pool_load(Buf *c, struct pool *p, int d, uint32_t v) {
    if (arm64_movi(c, d, v)) {
        return;
    }
    uint32_t bcast[4] = {v, v, v, v};
    int      off = pool_add(p, bcast, 16);
    pool_ref_at(p, off, c->len);
    put(c, LDR_q_literal(d));
}
static void arm64_pool_load_wide(Buf *c, struct pool *p, int d, void const *data) {
    int off = pool_add(p, data, 16);
    pool_ref_at(p, off, c->len);
    put(c, LDR_q_literal(d));
}
static void arm64_remat(int reg, int val, void *ctx) {
    struct jit_ctx *j = ctx;
    arm64_pool_load(j->c, &j->pool, lo(reg), (uint32_t)j->bb->inst[val].imm);
    put(j->c, ORR_16b(hi(reg), lo(reg), lo(reg)));
}

static struct ra *ra_create_arm64(struct umbra_basic_block const *bb, struct jit_ctx *jc) {
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


static void emit_ops(Buf *c, struct umbra_basic_block const *bb, int from, int to,
                     int *sl, int *ns, struct ra *ra, _Bool scalar, int *deref_gpr,
                     int *deref_rb_gpr, struct jit_ctx *jc);

struct umbra_jit {
    struct umbra_program base;
    void  *code;
    size_t code_size;
    void (*entry)(int, int, int, int, struct umbra_buf *);
    int loop_start, loop_end;
};

static struct umbra_jit *umbra_jit(struct umbra_basic_block const *bb) {

    int *sl = malloc((size_t)bb->insts * sizeof(int));
    for (int i = 0; i < bb->insts; i++) { sl[i] = -1; }
    int  ns = 0;
    int *deref_gpr    = calloc((size_t)bb->insts, sizeof(int));
    int *deref_rb_gpr = calloc((size_t)bb->insts, sizeof(int));

    Buf            c = {0};
    struct jit_ctx jc = {.c = &c, .bb = bb, .pool = {0}};
    struct ra     *ra = ra_create_arm64(bb, &jc);

    put(&c, STP_pre(29, 30, 31, -2));
    put(&c, ADD_xi(29, 31, 0));

    // Save callee-saved Q8-Q15 (128 bytes).
    put(&c, STP_qi_pre(8, 9, 31, -8));
    put(&c, STP_qi(10, 11, 31, 2));
    put(&c, STP_qi(12, 13, 31, 4));
    put(&c, STP_qi(14, 15, 31, 6));

    int stack_patch = c.len;
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

    int loop_top = c.len;

    // remaining = col_end - XCOL
    put(&c, SUB_xr(XT, 0, XCOL));
    put(&c, SUBS_xi(31, XT, 8));
    int br_tail = c.len;
    put(&c, Bcond(0xb, 0));

    int loop_body_start = c.len;
    emit_ops(&c, bb, bb->preamble, bb->insts, sl, &ns, ra, 0, deref_gpr, deref_rb_gpr, &jc);

    ra_end_loop(ra, sl);

    int loop_body_end = c.len;

    put(&c, ADD_xi(XCOL, XCOL, 8));
    put(&c, B(loop_top - c.len));

    int tail_top = c.len;
    c.buf[br_tail] = Bcond(0xb, tail_top - br_tail);

    put(&c, CMP_xr(XCOL, 0));
    int br_row_done = c.len;
    put(&c, Bcond(0xa, 0));  // B.GE row_done

    ra_reset_pool(ra);
    for (int i = 0; i < bb->insts; i++) { sl[i] = -1; }

    emit_ops(&c, bb, 0, bb->preamble, sl, &ns, ra, 0, deref_gpr, deref_rb_gpr, &jc);
    emit_ops(&c, bb, bb->preamble, bb->insts, sl, &ns, ra, 1, deref_gpr, deref_rb_gpr, &jc);

    put(&c, ADD_xi(XCOL, XCOL, 1));
    put(&c, B(tail_top - c.len));

    int row_done = c.len;
    c.buf[br_row_done] = Bcond(0xa, row_done - br_row_done);

    put(&c, ADD_xi(XY, XY, 1));
    put(&c, ADD_xi(XCOL, XWIDTH, 0));    // XCOL = x0_col
    // LDR XT, [SP] — load saved end_row
    put(&c, LDR_xi(XT, 31, 0));
    put(&c, CMP_xr(XY, XT));
    int br_more_rows = c.len;
    put(&c, Bcond(0xb, 0));  // B.LT → more rows

    // Restore stack (saved end_row)
    put(&c, LDP_post(0, 15, 31, 2));

    // Patch: B.LT → loop_top
    c.buf[br_more_rows] = Bcond(0xb, loop_top - br_more_rows);

    put(&c, ADD_xi(31, 29, 0));
    // Restore callee-saved Q8-Q15 from [FP-128, FP) using negative offsets.
    put(&c, LDP_qi( 8,  9, 31, -8));
    put(&c, LDP_qi(10, 11, 31, -6));
    put(&c, LDP_qi(12, 13, 31, -4));
    put(&c, LDP_qi(14, 15, 31, -2));
    put(&c, LDP_post(29, 30, 31, 2));
    put(&c, RET());

    if (ns > 0) {
        c.buf[stack_patch] = SUB_xi(31, 31, ns * 32);
    }
    c.buf[stack_patch + 1] = ADD_xi(XS, 31, 0);
    while (c.len & 3) {
        put(&c, NOP());
    }
    int pool_start = c.len;
    for (int pi = 0; pi < jc.pool.nbytes; pi += 4) {
        uint32_t w;
        __builtin_memcpy(&w, jc.pool.data + pi, 4);
        put(&c, w);
    }
    for (int pi = 0; pi < jc.pool.nrefs; pi++) {
        struct pool_ref *r = &jc.pool.refs[pi];
        int              word_off = pool_start + r->data_off / 4;
        int              imm19 = word_off - r->code_pos;
        c.buf[r->code_pos] = 0x9c000000u
            | ((uint32_t)(imm19 & 0x7ffff) << 5)
            | (c.buf[r->code_pos] & 0x1fu);
    }
    pool_free(&jc.pool);

    ra_destroy(ra);
    free(sl);
    free(deref_gpr);
    free(deref_rb_gpr);

    size_t code_sz = (size_t)c.len * 4;
    size_t pg = 16384;
    size_t alloc = (code_sz + pg - 1) & ~(pg - 1);

    void *mem = mmap(NULL, alloc + pg, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANON | MAP_JIT, -1, 0);
    assume(mem != MAP_FAILED);
    mprotect((char *)mem + alloc, pg, PROT_NONE);

    pthread_jit_write_protect_np(0);
    __builtin_memcpy(mem, c.buf, code_sz);
    pthread_jit_write_protect_np(1);
    __builtin___clear_cache(mem, (char *)mem + alloc);
    free(c.buf);

    struct umbra_jit *j = malloc(sizeof *j);
    j->code = mem;
    j->code_size = alloc + pg;
    j->loop_start = loop_body_start;
    j->loop_end = loop_body_end;
    {
        union {
            void *p;
            void (*fn)(int, int, int, int, struct umbra_buf *);
        } u = {.p = mem};
        j->entry = u.fn;
    }
    return j;
}

static void emit_ops(Buf *c, struct umbra_basic_block const *bb, int from, int to,
                     int *sl, int *ns, struct ra *ra, _Bool scalar, int *deref_gpr,
                     int *deref_rb_gpr, struct jit_ctx *jc) {
    int last_ptr = -1;
    int dc = 0;

    for (int i = from; i < to; i++) {
        struct bb_inst const *inst = &bb->inst[i];

        switch (inst->op) {
        case op_deref_ptr: {
            load_ptr(c, inst->ptr, &last_ptr);
            int gpr    = 3 + dc;
            int rb_gpr = dc < 2 ? 6 + dc : 0;
            dc++;
            deref_gpr[i]    = gpr;
            deref_rb_gpr[i] = rb_gpr;
            int base = XP;
            if (inst->imm != 0) {
                load_imm_w(c, XT, (uint32_t)inst->imm);
                put(c, ADD_xr(XT, XP, XT));
                base = XT;
            }
            // LDR gpr, [base] — deref pointer
            put(c, LDR_xi(gpr, base, 0));
            // LDR rb_gpr, [base, #16] — deref row_bytes
            if (rb_gpr) {
                put(c, LDR_xi(rb_gpr, base, 2));
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
            int            p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
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
            int            p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
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
            int            p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            load_imm_w(c, XT, (uint32_t)inst->imm);
            put(c, ADD_xr(XT, XP, XT));
            put(c, LDR_si(lo(s.rd), XT, 0));
            put(c, DUP_4s_lane(lo(s.rd), lo(s.rd), 0));
            put(c, ORR_16b(hi(s.rd), lo(s.rd), lo(s.rd)));
        } break;

        case op_gather_uniform_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int8_t         rx = ra_ensure(ra, sl, ns, (int)inst->x.id);
            free_chan(ra, inst->x, i);
            int p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            load_count(c, p, 2, deref_gpr);
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
            int8_t         rx = ra_ensure(ra, sl, ns, (int)inst->x.id);
            free_chan(ra, inst->x, i);
            int p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            load_count(c, p, 2, deref_gpr);
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
            int8_t         rx = ra_ensure(ra, sl, ns, (int)inst->x.id);
            int8_t hi_r  = ra_alloc(ra, sl, ns);
            int8_t frac  = ra_alloc(ra, sl, ns);
            int    p     = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            load_count(c, p, 2, deref_gpr);
            if (scalar) {
                put(c, FRINTM_4s(lo(hi_r), lo(rx)));
                put(c, FSUB_4s(lo(frac), lo(rx), lo(hi_r)));
                free_chan(ra, inst->x, i);
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
                free_chan(ra, inst->x, i);
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
            int8_t         rx = ra_ensure(ra, sl, ns, (int)inst->x.id);
            free_chan(ra, inst->x, i);
            int p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            load_count(c, p, 1, deref_gpr);
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
            int8_t ry = ra_ensure(ra, sl, ns, (int)inst->y.id);
            int    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            if (scalar) {
                put(c, STR_sx(lo(ry), XP, XI));
            } else {
                put(c, LSL_xi(XT, XI, 2));
                put(c, ADD_xr(XT, XP, XT));
                put(c, STP_qi(lo(ry), hi(ry), XT, 0));
            }
            free_chan(ra, inst->y, i);
        } break;

        case op_load_16x4: {
            int    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            _Bool fuse = !scalar
                && i + 4 < to
                && bb->inst[i+1].op == op_f32_from_f16 && bb->inst[i+1].x.id == (unsigned)i && bb->inst[i+1].x.chan == 0
                && bb->inst[i+2].op == op_f32_from_f16 && bb->inst[i+2].x.id == (unsigned)i && bb->inst[i+2].x.chan == 1
                && bb->inst[i+3].op == op_f32_from_f16 && bb->inst[i+3].x.id == (unsigned)i && bb->inst[i+3].x.chan == 2
                && bb->inst[i+4].op == op_f32_from_f16 && bb->inst[i+4].x.id == (unsigned)i && bb->inst[i+4].x.chan == 3;
            if (fuse) {
                put(c, LSL_xi(XT, XI, 3));
                put(c, ADD_xr(XT, XP, XT));
                put(c, LD4_8h(0, XT));
                for (int k = 0; k < 4; k++) {
                    struct ra_step s = ra_step_alloc(ra, sl, ns, i + 1 + k);
                    put(c, FCVTL_4s(lo(s.rd), k));
                    put(c, W(FCVTL_4s(hi(s.rd), k)));
                }
                i += 4;
            } else {
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
            }
        } break;
        case op_load_16x4_planar: {
            struct ra_step s0 = ra_step_alloc(ra, sl, ns, i);
            int8_t r1 = ra_alloc(ra, sl, ns);
            int8_t r2 = ra_alloc(ra, sl, ns);
            int8_t r3 = ra_alloc(ra, sl, ns);
            int    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            int8_t px = ra_alloc(ra, sl, ns);
            {
                int const sz_off = p * (int)sizeof(struct umbra_buf)
                                 + (int)__builtin_offsetof(struct umbra_buf, sz);
                put(c, LDR_xi(XT, XBUF, sz_off / 8));
                put(c, LSR_xi(XT, XT, 2));
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
            int8_t rr   = ra_ensure_chan(ra, sl, ns, (int)inst->x.id, (int)inst->x.chan);
            int8_t rg   = ra_ensure_chan(ra, sl, ns, (int)inst->y.id, (int)inst->y.chan);
            int8_t rb_  = ra_ensure_chan(ra, sl, ns, (int)inst->z.id, (int)inst->z.chan);
            int8_t ra_v = ra_ensure_chan(ra, sl, ns, (int)inst->w.id, (int)inst->w.chan);
            int    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
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
            free_chan(ra, inst->x, i);
            free_chan(ra, inst->y, i);
            free_chan(ra, inst->z, i);
            free_chan(ra, inst->w, i);
        } break;
        case op_store_16x4_planar: {
            int8_t rr   = ra_ensure_chan(ra, sl, ns, (int)inst->x.id, (int)inst->x.chan);
            int8_t rg   = ra_ensure_chan(ra, sl, ns, (int)inst->y.id, (int)inst->y.chan);
            int8_t rb_  = ra_ensure_chan(ra, sl, ns, (int)inst->z.id, (int)inst->z.chan);
            int8_t ra_v = ra_ensure_chan(ra, sl, ns, (int)inst->w.id, (int)inst->w.chan);
            int    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            {
                int const sz_off = p * (int)sizeof(struct umbra_buf)
                                 + (int)__builtin_offsetof(struct umbra_buf, sz);
                put(c, LDR_xi(XT, XBUF, sz_off / 8));
                put(c, LSR_xi(XT, XT, 2));
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
            free_chan(ra, inst->x, i);
            free_chan(ra, inst->y, i);
            free_chan(ra, inst->z, i);
            free_chan(ra, inst->w, i);
        } break;

        case op_load_8x4: {
            struct ra_step s0 = ra_step_alloc(ra, sl, ns, i);
            int8_t r1 = ra_alloc(ra, sl, ns);
            int8_t r2 = ra_alloc(ra, sl, ns);
            int8_t r3 = ra_alloc(ra, sl, ns);
            int    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
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
                // V0=R(8xu8), V1=G, V2=B, V3=A.  Widen u8→u16→u32.
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
            int8_t rr   = ra_ensure_chan(ra, sl, ns, (int)inst->x.id, (int)inst->x.chan);
            int8_t rg   = ra_ensure_chan(ra, sl, ns, (int)inst->y.id, (int)inst->y.chan);
            int8_t rb_  = ra_ensure_chan(ra, sl, ns, (int)inst->z.id, (int)inst->z.chan);
            int8_t ra_v = ra_ensure_chan(ra, sl, ns, (int)inst->w.id, (int)inst->w.chan);
            int    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
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
                // Narrow u32→u16→u8 per channel into V0-V3, then ST4.8B.
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
            free_chan(ra, inst->x, i);
            free_chan(ra, inst->y, i);
            free_chan(ra, inst->z, i);
            free_chan(ra, inst->w, i);
        } break;

        case op_store_16: {
            int8_t ry = ra_ensure(ra, sl, ns, (int)inst->y.id);
            int    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            if (scalar) {
                put(c, STR_hx(lo(ry), XP, XI));
            } else {
                put(c, LSL_xi(XT, XI, 1));
                put(c, ADD_xr(XT, XP, XT));
                put(c, STR_di(lo(ry), XT, 0));
                put(c, STR_di(hi(ry), XT, 1));
            }
            free_chan(ra, inst->y, i);
        } break;

        case op_f32_from_f16: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            put(c, FCVTL_4s(lo(s.rd), lo(s.rx)));
            if (!scalar) { put(c, FCVTL_4s(hi(s.rd), hi(s.rx))); }
        } break;

        case op_f16_from_f32: {
            _Bool fuse_store = !scalar
                && i + 4 < to
                && bb->inst[i+1].op == op_f16_from_f32
                && bb->inst[i+2].op == op_f16_from_f32
                && bb->inst[i+3].op == op_f16_from_f32
                && bb->inst[i+4].op == op_store_16x4
                && bb->inst[i+4].x.id == (unsigned)i   && bb->inst[i+4].x.chan == 0
                && bb->inst[i+4].y.id == (unsigned)(i+1) && bb->inst[i+4].y.chan == 0
                && bb->inst[i+4].z.id == (unsigned)(i+2) && bb->inst[i+4].z.chan == 0
                && bb->inst[i+4].w.id == (unsigned)(i+3) && bb->inst[i+4].w.chan == 0;
            if (fuse_store) {
                for (int k = 0; k < 4; k++) {
                    int8_t rx = ra_ensure(ra, sl, ns, (int)bb->inst[i+k].x.id);
                    put(c, FCVTN_4h(k, lo(rx)));
                    put(c, W(FCVTN_4h(k, hi(rx))));
                    free_chan(ra, bb->inst[i+k].x, i+k);
                }
                struct bb_inst const *st = &bb->inst[i+4];
                int p = st->ptr;
                resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
                put(c, LSL_xi(XT, XI, 3));
                put(c, ADD_xr(XT, XP, XT));
                put(c, ST4_8h(0, XT));
                i += 4;
            } else {
                struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
                put(c, FCVTN_4h(lo(s.rd), lo(s.rx)));
                if (!scalar) { put(c, FCVTN_4h(hi(s.rd), hi(s.rx))); }
            }
        } break;

        case op_i32_from_s16: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            put(c, SXTL_4s(lo(s.rd), lo(s.rx)));
            if (!scalar) { put(c, SXTL_4s(hi(s.rd), hi(s.rx))); }
        } break;

        case op_i32_from_u16: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            put(c, UXTL_4s(lo(s.rd), lo(s.rx)));
            if (!scalar) { put(c, UXTL_4s(hi(s.rd), hi(s.rx))); }
        } break;

        case op_i16_from_i32: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
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
            struct ra_step s = ra_step_alu(ra, sl, ns, inst, i, scalar, nscratch);
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
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
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
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            int8_t ir = ra_ensure(ra, sl, ns, (int)inst->y.id);
            free_chan(ra, inst->y, i);
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
        }
    }
#undef lu
}

#if __clang__
__attribute__((no_sanitize("function")))
#endif
static void umbra_jit_run(struct umbra_jit *j, int l, int t, int r, int b, struct umbra_buf buf[]) {
    j->entry(l, t, r, b, buf);
}
static void umbra_jit_free(struct umbra_jit *j) {
    munmap(j->code, j->code_size);
    free(j);
}

static void umbra_dump_jit_mca(struct umbra_jit const *j, FILE *f) {
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

static void run_jit(struct umbra_program *prog, int l, int t, int r, int b, struct umbra_buf buf[]) {
    umbra_jit_run((struct umbra_jit*)prog, l, t, r, b, buf);
}
static void dump_jit(struct umbra_program const *prog, FILE *f) { umbra_dump_jit_mca((struct umbra_jit const*)prog, f); }
static void free_jit(struct umbra_program *prog) { umbra_jit_free((struct umbra_jit*)prog); }
static struct umbra_program *compile_jit(struct umbra_backend           *be,
                                         struct umbra_basic_block const *bb) {
    struct umbra_jit *const j = umbra_jit(bb);
    j->base = (struct umbra_program){
        .queue   = run_jit,
        .dump    = dump_jit,
        .free    = free_jit,
        .backend = be,
    };
    return &j->base;
}
static void flush_be_noop(struct umbra_backend *be) { (void)be; }
static void free_be_jit(struct umbra_backend *be) { free(be); }
struct umbra_backend *umbra_backend_jit(void) {
    struct umbra_backend *const be = malloc(sizeof *be);
    *be = (struct umbra_backend){
        .compile    = compile_jit,
        .flush      = flush_be_noop,
        .free    = free_be_jit,
        .threadsafe = 1,
    };
    return be;
}

#elif defined(__AVX2__)

#include "asm_x86.h"

typedef struct Buf Buf;

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

_Static_assert(sizeof(struct umbra_buf) == 32, "");
_Static_assert(offsetof(struct umbra_buf, ptr)       ==  0, "");
_Static_assert(offsetof(struct umbra_buf, sz)        ==  8, "");
_Static_assert(offsetof(struct umbra_buf, row_bytes) == 16, "");

struct pool_ref {
    int data_off, code_pos, extra;
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
static void pool_ref_at(struct pool *p, int data_off, int code_pos, int extra) {
    if (p->nrefs == p->cap_refs) {
        p->cap_refs = p->cap_refs ? 2 * p->cap_refs : 32;
        p->refs = realloc(p->refs, (size_t)p->cap_refs * sizeof *p->refs);
    }
    p->refs[p->nrefs++] = (struct pool_ref){data_off, code_pos, extra};
}
static void pool_free(struct pool *p) {
    free(p->data);
    free(p->refs);
}

// RDI=col_end, RSI=x0_col, RDX=buf, R10=col, R14=row, R12=end_row.
enum { XWIDTH = RSI, XBUF = RDX, XCOL_X86 = R10, XH_X86 = R12, XM = R13, XY = R14 };
#define XI XCOL_X86

static void patch_jcc(Buf *c, int fixup) {
    int32_t rel = (int32_t)(c->len - (fixup + 4));
    __builtin_memcpy(c->buf + fixup, &rel, 4);
}

static void load_count_x86(Buf *c, int p, int elem_shift) {
    if (ptr_is_deref(p)) {
        mov_ri(c, XM, 0x7fffffff);
    } else {
        mov_load(c, XM, XBUF, p * (int)sizeof(struct umbra_buf) + (int)__builtin_offsetof(struct umbra_buf, sz));
        if (elem_shift) {
            shr_ri(c, XM, (uint8_t)elem_shift);
        }
    }
}
static int load_ptr_x86(Buf *c, int p, int *last_ptr) {
    if (*last_ptr != p) {
        *last_ptr = p;
        mov_load(c, R11, XBUF, p * (int)sizeof(struct umbra_buf));
        mov_load(c, RAX, XBUF, p * (int)sizeof(struct umbra_buf) + (int)__builtin_offsetof(struct umbra_buf, row_bytes));
        rex_w(c, RAX, XY);
        emit1(c, 0x0f); emit1(c, 0xaf);
        emit1(c, (uint8_t)(0xc0 | ((RAX & 7) << 3) | (XY & 7)));
        rex_w(c, RAX, R11);
        emit1(c, 0x01);
        emit1(c, (uint8_t)(0xc0 | ((RAX & 7) << 3) | (R11 & 7)));
    }
    return R11;
}

static int8_t const ra_pool_x86[] = {0, 1, 2,  3,  4,  5,  6,  7,
                                     8, 9, 10, 11, 12, 13, 14, 15};

struct jit_ctx {
    Buf                            *c;
    struct umbra_basic_block const *bb;
    struct pool                     pool;
};

static void x86_spill(int reg, int slot, void *ctx) {
    vspill(((struct jit_ctx *)ctx)->c, reg, slot);
}
static void x86_fill(int reg, int slot, void *ctx) {
    vfill(((struct jit_ctx *)ctx)->c, reg, slot);
}

static void pool_broadcast(Buf *c, struct pool *p, int d, uint32_t v) {
    int shr = v ? __builtin_clz(v) : 0;
    int shl = v ? __builtin_ctz(v) : 0;
    if (v == 0) {
        vpxor(c, 1, d, d, d);
    } else if (v == 0xffffffffu) {
        vpcmpeqd(c, d, d, d);
    } else if (v == 0xffffffffu >> shr) {
        vpcmpeqd(c, d, d, d);
        vpsrld_i(c, d, d, (uint8_t)shr);
    } else if (v == (uint32_t)(UINT64_C(0xffffffff) << shl)) {
        vpcmpeqd(c, d, d, d);
        vpslld_i(c, d, d, (uint8_t)shl);
    } else {
        int off = pool_add(p, &v, 4);
        int pos = vex_rip(c, 1, 2, 0, 1, d, 0, 0x18);
        pool_ref_at(p, off, pos, 0);
    }
}

static void x86_remat(int reg, int val, void *ctx) {
    struct jit_ctx *j = ctx;
    pool_broadcast(j->c, &j->pool, reg, (uint32_t)j->bb->inst[val].imm);
}

static struct ra *ra_create_x86(struct umbra_basic_block const *bb, struct jit_ctx *jc) {
    struct ra_config cfg = {
        .pool = ra_pool_x86,
        .nregs = 16,
        .max_reg = 16,
        .spill = x86_spill,
        .fill = x86_fill,
        .remat = x86_remat,
        .ignore_imm_y = 1,
        .ctx = jc,
    };
    return ra_create(bb, &cfg);
}

static void emit_alu_reg(Buf *c, enum op op, int d, int x, int y, int z, int imm,
                         int scratch, int scratch2) {
    switch ((int)op) {
    case op_imm_32: broadcast_imm32(c, d, (uint32_t)imm); break;

    case op_add_f32: vaddps(c, d, x, y); break;
    case op_sub_f32: vsubps(c, d, x, y); break;
    case op_mul_f32: vmulps(c, d, x, y); break;
    case op_div_f32: vdivps(c, d, x, y); break;
    case op_min_f32: vminps(c, d, x, y); break;
    case op_max_f32: vmaxps(c, d, x, y); break;
    case op_sqrt_f32: vsqrtps(c, d, x); break;
    case op_round_f32: vroundps(c, d, x, 0); break;
    case op_floor_f32: vroundps(c, d, x, 1); break;
    case op_ceil_f32: vroundps(c, d, x, 2); break;
    case op_round_i32: vcvtps2dq(c, d, x); break;
    case op_floor_i32:
        vroundps(c, d, x, 1);
        vcvttps2dq(c, d, d);
        break;
    case op_ceil_i32:
        vroundps(c, d, x, 2);
        vcvttps2dq(c, d, d);
        break;
    case op_fma_f32:
        if (d == x) {
            vfmadd132ps(c, d, z, y);
        } else if (d == y) {
            vfmadd213ps(c, d, x, z);
        } else if (d == z) {
            vfmadd231ps(c, d, x, y);
        } else {
            vmovaps(c, d, z);
            vfmadd231ps(c, d, x, y);
        }
        break;
    case op_fms_f32:
        if (d == x) {
            vfnmadd132ps(c, d, z, y);
        } else if (d == y) {
            vfnmadd213ps(c, d, x, z);
        } else if (d == z) {
            vfnmadd231ps(c, d, x, y);
        } else {
            vmovaps(c, d, z);
            vfnmadd231ps(c, d, x, y);
        }
        break;

    case op_add_i32: vpaddd(c, d, x, y); break;
    case op_sub_i32: vpsubd(c, d, x, y); break;
    case op_mul_i32: vpmulld(c, d, x, y); break;
    case op_shl_i32: vpsllvd(c, d, x, y); break;
    case op_shr_u32: vpsrlvd(c, d, x, y); break;
    case op_shr_s32: vpsravd(c, d, x, y); break;

    case op_and_32: vpand(c, 1, d, x, y); break;
    case op_or_32: vpor(c, 1, d, x, y); break;
    case op_xor_32: vpxor_3(c, 1, d, x, y); break;
    case op_sel_32: vpblendvb(c, 1, d, z, y, x); break;

    case op_f32_from_i32: vcvtdq2ps(c, d, x); break;
    case op_i32_from_f32: vcvttps2dq(c, d, x); break;

    case op_eq_f32: vcmpps(c, d, x, y, 0); break;
    case op_lt_f32: vcmpps(c, d, x, y, 1); break;
    case op_le_f32: vcmpps(c, d, x, y, 2); break;

    case op_eq_i32: vpcmpeqd(c, d, x, y); break;
    case op_lt_s32: vpcmpgtd(c, d, y, x); break;
    case op_le_s32:
        vpcmpgtd(c, d, x, y);
        vpcmpeqd(c, scratch, scratch, scratch);
        vpxor_3(c, 1, d, d, scratch);
        break;
    case op_lt_u32:
        vex_rrr(c, 1, 2, 1, 0x3b, scratch2, x, y);
        vpcmpeqd(c, d, y, scratch2);
        vpcmpeqd(c, scratch, scratch, scratch);
        vpxor_3(c, 1, d, d, scratch);
        break;
    case op_le_u32:
        vex_rrr(c, 1, 2, 1, 0x3f, scratch, x, y);
        vpcmpeqd(c, d, y, scratch);
        break;
    case op_shl_i32_imm: vpslld_i(c, d, x, (uint8_t)imm); break;
    case op_shr_u32_imm: vpsrld_i(c, d, x, (uint8_t)imm); break;
    case op_shr_s32_imm: vpsrad_i(c, d, x, (uint8_t)imm); break;
    }
}

static void emit_ops(Buf *c, struct umbra_basic_block const *bb, int from, int to,
                     int *sl, int *ns, struct ra *ra, _Bool scalar, int *deref_gpr,
                     int *deref_rb_gpr, struct jit_ctx *jc);

static int resolve_ptr_x86(Buf *c, int p, int *last_ptr, int const *deref_gpr,
                           int const *deref_rb_gpr) {
    if (ptr_is_deref(p)) {
        int gpr = deref_gpr[ptr_ix(p)];
        int rb  = deref_rb_gpr[ptr_ix(p)];
        if (rb > 0) {
            mov_rr(c, R11, gpr);
            mov_rr(c, RAX, XY);
            rex_w(c, RAX, rb);
            emit1(c, 0x0f); emit1(c, 0xaf);
            emit1(c, (uint8_t)(0xc0 | ((RAX & 7) << 3) | (rb & 7)));
            rex_w(c, RAX, R11);
            emit1(c, 0x01);
            emit1(c, (uint8_t)(0xc0 | ((RAX & 7) << 3) | (R11 & 7)));
            return R11;
        }
        return gpr;
    }
    return load_ptr_x86(c, p, last_ptr);
}

struct umbra_jit {
    struct umbra_program base;
    void  *code;
    size_t code_size, code_len;
    void (*entry)(int, int, int, int, struct umbra_buf *);
    int loop_start, loop_end;
};

static struct umbra_jit *umbra_jit(struct umbra_basic_block const *bb) {
    int *sl = malloc((size_t)bb->insts * sizeof(int));
    for (int i = 0; i < bb->insts; i++) {
        sl[i] = -1;
    }
    int  ns = 0;
    int *deref_gpr    = calloc((size_t)bb->insts, sizeof(int));
    int *deref_rb_gpr = calloc((size_t)bb->insts, sizeof(int));

    Buf            c = {0};
    struct jit_ctx jc = {.c = &c, .bb = bb, .pool = {0}};
    struct ra     *ra = ra_create_x86(bb, &jc);

    push_r(&c, XM);
    push_r(&c, XY);
    push_r(&c, XH_X86);
    push_r(&c, R15);
    push_r(&c, RBX);

    int stack_patch = c.len;
    for (int i = 0; i < 7; i++) {
        nop(&c);
    }

    // Entry: RDI=l, RSI=t, RDX=r, RCX=b, R8=buf.
    mov_rr(&c, XI, RDX);             // XI = r (save before XBUF overwrite)
    mov_rr(&c, XH_X86, RCX);         // XH_X86 = b
    mov_rr(&c, XBUF, R8);            // XBUF(RDX) = buf

    emit_ops(&c, bb, 0, bb->preamble, sl, &ns, ra, 0, deref_gpr, deref_rb_gpr, &jc);

    ra_begin_loop(ra);

    // RDI=l, RSI=t survive preamble; XI=r, XH_X86=b saved above.
    mov_rr(&c, XY, RSI);             // XY = t
    mov_rr(&c, RSI, RDI);            // XWIDTH = l
    mov_rr(&c, RDI, XI);             // RDI = r = col_end
    mov_rr(&c, XI, RSI);             // XI = l

    int loop_top = c.len;

    // remaining = row_end (RDI) - XI
    mov_rr(&c, R11, RDI);
    rex_w(&c, XI, R11);
    emit1(&c, 0x29);
    emit1(&c, (uint8_t)(0xc0 | ((XI & 7) << 3) | (R11 & 7)));

    cmp_ri(&c, R11, 8);
    int br_tail = jcc(&c, 0x0c);

    int loop_body_start = c.len;
    emit_ops(&c, bb, bb->preamble, bb->insts, sl, &ns, ra, 0, deref_gpr, deref_rb_gpr, &jc);

    ra_end_loop(ra, sl);

    int loop_body_end = c.len;

    add_ri(&c, XI, 8);
    {
        int     j = jmp(&c);
        int32_t rel = (int32_t)(loop_top - (j + 4));
        __builtin_memcpy(c.buf + j, &rel, 4);
    }

    int tail_top = c.len;
    patch_jcc(&c, br_tail);

    cmp_rr(&c, XI, RDI);                 // xi >= row_end?
    int br_row_done = jcc(&c, 0x0d);     // JGE row_done

    ra_reset_pool(ra);
    for (int i = 0; i < bb->insts; i++) { sl[i] = -1; }

    emit_ops(&c, bb, 0, bb->preamble, sl, &ns, ra, 0, deref_gpr, deref_rb_gpr, &jc);
    emit_ops(&c, bb, bb->preamble, bb->insts, sl, &ns, ra, 1, deref_gpr, deref_rb_gpr, &jc);

    add_ri(&c, XI, 1);
    {
        int     j = jmp(&c);
        int32_t rel = (int32_t)(tail_top - (j + 4));
        __builtin_memcpy(c.buf + j, &rel, 4);
    }

    // row_done:
    patch_jcc(&c, br_row_done);

    add_ri(&c, XY, 1);
    mov_rr(&c, XI, XWIDTH);
    cmp_rr(&c, XY, XH_X86);
    int br_more_rows = jcc(&c, 0x0c);    // JL → more rows
    {
        int32_t rel = (int32_t)(loop_top - (br_more_rows + 4));
        __builtin_memcpy(c.buf + br_more_rows, &rel, 4);
    }

    if (ns > 0) {
        add_ri(&c, RSP, ns * 32);
    }
    pop_r(&c, RBX);
    pop_r(&c, R15);
    pop_r(&c, XH_X86);
    pop_r(&c, XY);
    pop_r(&c, XM);
    vzeroupper(&c);
    ret(&c);

    if (ns > 0) {
        int pos = stack_patch;
        c.buf[pos++] = 0x48;
        c.buf[pos++] = 0x81;
        c.buf[pos++] = (uint8_t)(0xc0 | (5 << 3) | (RSP & 7));
        int32_t sz = ns * 32;
        __builtin_memcpy(c.buf + pos, &sz, 4);
    }

    int pool_start = c.len;
    for (int i = 0; i < jc.pool.nbytes; i++) {
        emit1(&c, jc.pool.data[i]);
    }
    for (int i = 0; i < jc.pool.nrefs; i++) {
        struct pool_ref *r = &jc.pool.refs[i];
        int              entry_off = pool_start + r->data_off;
        int32_t          rel = (int32_t)(entry_off - (r->code_pos + 4 + r->extra));
        __builtin_memcpy(c.buf + r->code_pos, &rel, 4);
    }
    pool_free(&jc.pool);

    ra_destroy(ra);
    free(sl);
    free(deref_gpr);
    free(deref_rb_gpr);

    size_t code_sz = (size_t)c.len;
    size_t pg = (size_t)sysconf(_SC_PAGESIZE);
    size_t alloc = (code_sz + pg - 1) & ~(pg - 1);

    void *mem = mmap(NULL, alloc + pg, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    assume(mem != MAP_FAILED);
    mprotect((char *)mem + alloc, pg, PROT_NONE);
    __builtin_memcpy(mem, c.buf, code_sz);
    int ok = mprotect(mem, alloc, PROT_READ | PROT_EXEC);
    assume(ok == 0);
    free(c.buf);

    struct umbra_jit *j = malloc(sizeof *j);
    j->code = mem;
    j->code_size = alloc + pg;
    j->code_len = code_sz;
    j->loop_start = loop_body_start;
    j->loop_end = loop_body_end;
    {
        union {
            void *p;
            void (*fn)(int, int, int, int, struct umbra_buf *);
        } u = {.p = mem};
        j->entry = u.fn;
    }
    return j;
}

static void emit_ops(Buf *c, struct umbra_basic_block const *bb, int from, int to,
                     int *sl, int *ns, struct ra *ra, _Bool scalar, int *deref_gpr,
                     int *deref_rb_gpr, struct jit_ctx *jc) {
    int       last_ptr = -1;
    int       dc = 0;
    int const deref_gprs[] = {RCX, R8, R9};
    int const rb_gprs[]    = {R15, RBX, 0};

    for (int i = from; i < to; i++) {
        struct bb_inst const *inst = &bb->inst[i];
        switch (inst->op) {
        case op_deref_ptr: {
            int base   = load_ptr_x86(c, inst->ptr, &last_ptr);
            int gpr    = deref_gprs[dc];
            int rb_reg = rb_gprs[dc];
            dc++;
            mov_load(c, gpr, base, inst->imm);
            deref_gpr[i] = gpr;
            if (rb_reg) {
                mov_load(c, rb_reg, base, inst->imm + 16);
                deref_rb_gpr[i] = rb_reg;
            }
        } break;

        case op_x: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            vmovd_from_gpr(c, s.rd, XI);
            if (!scalar) {
                vbroadcastss(c, s.rd, s.rd);
                uint32_t iota8[8] = {0, 1, 2, 3, 4, 5, 6, 7};
                int      off = pool_add(&jc->pool, iota8, 32);
                int      pos = vex_rip(c, 1, 1, 0, 1, s.rd, s.rd, 0xfe);
                pool_ref_at(&jc->pool, off, pos, 0);
            }
        } break;

        case op_y: {
            // y = row counter XY, broadcast.
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            vmovd_from_gpr(c, s.rd, XY);
            if (!scalar) {
                vbroadcastss(c, s.rd, s.rd);
            }
        } break;

        case op_load_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int            p = inst->ptr;
            int            base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            if (scalar) {
                vmovd_load(c, s.rd, base, XI, 4, 0);
            } else {
                vmov_load(c, 1, s.rd, base, XI, 4, 0);
            }
        } break;

        case op_load_16: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int            p = inst->ptr;
            int            base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            if (scalar) {
                // MOVZX eax, word [base + R10*2]
                {
                    uint8_t rex = 0x40;
                    if (XI >= 8) { rex |= 0x02; }
                    if (base >= 8) { rex |= 0x01; }
                    if (rex != 0x40) { emit1(c, rex); }
                    emit1(c, 0x0f);
                    emit1(c, 0xb7);
                    emit1(c, (uint8_t)(((RAX & 7) << 3) | 4));
                    emit1(c, (uint8_t)((1 << 6) | ((XI & 7) << 3) | (base & 7)));
                }
                vmovd_from_gpr(c, s.rd, RAX);
            } else {
                // Load 128-bit (8 x u16)
                vmov_load(c, 0, s.rd, base, XI, 2, 0);
            }
        } break;

        case op_store_32: {
            int8_t ry = ra_ensure(ra, sl, ns, (int)inst->y.id);
            int    p = inst->ptr;
            int    base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            if (scalar) {
                vmovd_store(c, ry, base, XI, 4, 0);
            } else {
                vmov_store(c, 1, ry, base, XI, 4, 0);
            }
            free_chan(ra, inst->y, i);
        } break;

        case op_load_16x4: {
            struct ra_step s0 = ra_step_alloc(ra, sl, ns, i);
            int8_t r1 = ra_alloc(ra, sl, ns);
            int8_t r2 = ra_alloc(ra, sl, ns);
            int8_t r3 = ra_alloc(ra, sl, ns);
            int    p = inst->ptr;
            int    base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);

            int8_t px = ra_alloc(ra, sl, ns);
            int8_t t0 = ra_alloc(ra, sl, ns);
            int8_t t1 = ra_alloc(ra, sl, ns);

            if (scalar) {
                vmovq_load(c, px, base, XI, 8, 0);
                vmovaps(c, s0.rd, px);
                vpsrldq(c, r1, px, 2);
                vpsrldq(c, r2, px, 4);
                vpsrldq(c, r3, px, 6);
            } else {
                // Re-load raw data.
                vmov_load(c, 1, t0, base, XI, 8, 0);   // pixels 0-3 raw
                // VPSHUFB mask: in each 128-bit lane, extract one channel.
                uint8_t shuf_r[32], shuf_g[32], shuf_b[32], shuf_a[32];
                for (int j = 0; j < 32; j++) {
                    shuf_r[j] = 0x80; shuf_g[j] = 0x80;
                    shuf_b[j] = 0x80; shuf_a[j] = 0x80;
                }
                shuf_r[0]=0; shuf_r[1]=1; shuf_r[2]=8;  shuf_r[3]=9;
                shuf_g[0]=2; shuf_g[1]=3; shuf_g[2]=10; shuf_g[3]=11;
                shuf_b[0]=4; shuf_b[1]=5; shuf_b[2]=12; shuf_b[3]=13;
                shuf_a[0]=6; shuf_a[1]=7; shuf_a[2]=14; shuf_a[3]=15;
                shuf_r[16]=0; shuf_r[17]=1; shuf_r[18]=8;  shuf_r[19]=9;
                shuf_g[16]=2; shuf_g[17]=3; shuf_g[18]=10; shuf_g[19]=11;
                shuf_b[16]=4; shuf_b[17]=5; shuf_b[18]=12; shuf_b[19]=13;
                shuf_a[16]=6; shuf_a[17]=7; shuf_a[18]=14; shuf_a[19]=15;

                // For each channel: VPSHUFB extracts 2 u16 per 128-bit lane (4 total).
                // VEXTRACTI128 gets the high lane, VPUNPCKLDQ merges to 4 contiguous u16.
                // Repeat for pixels 4-7, then VPUNPCKLQDQ merges to 8 contiguous u16.
                int off_r = pool_add(&jc->pool, shuf_r, 32);
                int off_g = pool_add(&jc->pool, shuf_g, 32);
                int off_b = pool_add(&jc->pool, shuf_b, 32);
                int off_a = pool_add(&jc->pool, shuf_a, 32);

                vmov_load(c, 1, t1, base, XI, 8, 32);  // pixels 4-7

#define DEINTERLEAVE(dst, off)                                                  \
                {                                                               \
                    int ref = vex_rip(c, 1, 2, 0, 1, dst, t0, 0x00);           \
                    pool_ref_at(&jc->pool, off, ref, 0);                        \
                }                                                               \
                vextracti128(c, px, dst, 1);                                    \
                vex_rrr(c, 1, 1, 0, 0x62, dst, dst, px);                       \
                {                                                               \
                    int ref = vex_rip(c, 1, 2, 0, 1, px, t1, 0x00);            \
                    pool_ref_at(&jc->pool, off, ref, 0);                        \
                }                                                               \
                vextracti128(c, t0, px, 1);                                     \
                vex_rrr(c, 1, 1, 0, 0x62, px, px, t0);                         \
                vex_rrr(c, 1, 1, 0, 0x6c, dst, dst, px)

                vmov_load(c, 1, t0, base, XI, 8, 0);
                DEINTERLEAVE(s0.rd, off_r);
                vmov_load(c, 1, t0, base, XI, 8, 0);
                DEINTERLEAVE(r1, off_g);
                vmov_load(c, 1, t0, base, XI, 8, 0);
                DEINTERLEAVE(r2, off_b);
                vmov_load(c, 1, t0, base, XI, 8, 0);
                DEINTERLEAVE(r3, off_a);
#undef DEINTERLEAVE
            }

            ra_return_reg(ra, t1);
            ra_return_reg(ra, t0);
            ra_return_reg(ra, px);
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
            int    p = inst->ptr;
            int    base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);

            int8_t px = ra_alloc(ra, sl, ns);
            int8_t t0 = ra_alloc(ra, sl, ns);
            int8_t t1 = ra_alloc(ra, sl, ns);

            {
                int const sz_off = p * (int)sizeof(struct umbra_buf)
                                 + (int)__builtin_offsetof(struct umbra_buf, sz);
                mov_rr(c, R11, base);
                mov_load(c, RBX, XBUF, sz_off);
                shr_ri(c, RBX, 2);
                if (scalar) {
                    // Plane 0 (R): MOVZX eax, word [R11 + XI*2]; VMOVD xmm, eax
                    {
                        uint8_t rex = 0x40;
                        if (XI >= 8)   { rex |= 0x02; }
                        if (R11 >= 8)  { rex |= 0x01; }
                        if (rex != 0x40) { emit1(c, rex); }
                        emit1(c, 0x0f); emit1(c, 0xb7);
                        emit1(c, (uint8_t)(((RAX & 7) << 3) | 4));
                        emit1(c, (uint8_t)((1 << 6) | ((XI & 7) << 3) | (R11 & 7)));
                    }
                    vmovd_from_gpr(c, s0.rd, RAX);

                    // Plane 1 (G): advance R11 by plane_stride
                    rex_w(c, RBX, R11);
                    emit1(c, 0x01);
                    emit1(c, (uint8_t)(0xc0 | ((RBX & 7) << 3) | (R11 & 7)));
                    {
                        uint8_t rex = 0x40;
                        if (XI >= 8)   { rex |= 0x02; }
                        if (R11 >= 8)  { rex |= 0x01; }
                        if (rex != 0x40) { emit1(c, rex); }
                        emit1(c, 0x0f); emit1(c, 0xb7);
                        emit1(c, (uint8_t)(((RAX & 7) << 3) | 4));
                        emit1(c, (uint8_t)((1 << 6) | ((XI & 7) << 3) | (R11 & 7)));
                    }
                    vmovd_from_gpr(c, r1, RAX);

                    // Plane 2 (B): advance R11 by plane_stride
                    rex_w(c, RBX, R11);
                    emit1(c, 0x01);
                    emit1(c, (uint8_t)(0xc0 | ((RBX & 7) << 3) | (R11 & 7)));
                    {
                        uint8_t rex = 0x40;
                        if (XI >= 8)   { rex |= 0x02; }
                        if (R11 >= 8)  { rex |= 0x01; }
                        if (rex != 0x40) { emit1(c, rex); }
                        emit1(c, 0x0f); emit1(c, 0xb7);
                        emit1(c, (uint8_t)(((RAX & 7) << 3) | 4));
                        emit1(c, (uint8_t)((1 << 6) | ((XI & 7) << 3) | (R11 & 7)));
                    }
                    vmovd_from_gpr(c, r2, RAX);

                    // Plane 3 (A): advance R11 by plane_stride
                    rex_w(c, RBX, R11);
                    emit1(c, 0x01);
                    emit1(c, (uint8_t)(0xc0 | ((RBX & 7) << 3) | (R11 & 7)));
                    {
                        uint8_t rex = 0x40;
                        if (XI >= 8)   { rex |= 0x02; }
                        if (R11 >= 8)  { rex |= 0x01; }
                        if (rex != 0x40) { emit1(c, rex); }
                        emit1(c, 0x0f); emit1(c, 0xb7);
                        emit1(c, (uint8_t)(((RAX & 7) << 3) | 4));
                        emit1(c, (uint8_t)((1 << 6) | ((XI & 7) << 3) | (R11 & 7)));
                    }
                    vmovd_from_gpr(c, r3, RAX);
                } else {
                    // Plane 0 (R): load 8 x u16 (16 bytes)
                    vmov_load(c, 0, s0.rd, R11, XI, 2, 0);

                    // Plane 1 (G): advance R11 by plane_stride
                    rex_w(c, RBX, R11);
                    emit1(c, 0x01);
                    emit1(c, (uint8_t)(0xc0 | ((RBX & 7) << 3) | (R11 & 7)));
                    vmov_load(c, 0, r1, R11, XI, 2, 0);

                    // Plane 2 (B): advance R11 by plane_stride
                    rex_w(c, RBX, R11);
                    emit1(c, 0x01);
                    emit1(c, (uint8_t)(0xc0 | ((RBX & 7) << 3) | (R11 & 7)));
                    vmov_load(c, 0, r2, R11, XI, 2, 0);

                    // Plane 3 (A): advance R11 by plane_stride
                    rex_w(c, RBX, R11);
                    emit1(c, 0x01);
                    emit1(c, (uint8_t)(0xc0 | ((RBX & 7) << 3) | (R11 & 7)));
                    vmov_load(c, 0, r3, R11, XI, 2, 0);
                }
                last_ptr = -1;
            }

            ra_return_reg(ra, t1);
            ra_return_reg(ra, t0);
            ra_return_reg(ra, px);
            ra_set_chan_reg(ra, i, 0, s0.rd);
            ra_set_chan_reg(ra, i, 1, r1);
            ra_set_chan_reg(ra, i, 2, r2);
            ra_set_chan_reg(ra, i, 3, r3);
        } break;
        case op_store_16x4: {
            int8_t rr   = ra_ensure_chan(ra, sl, ns, (int)inst->x.id, (int)inst->x.chan);
            int8_t rg   = ra_ensure_chan(ra, sl, ns, (int)inst->y.id, (int)inst->y.chan);
            int8_t rb_  = ra_ensure_chan(ra, sl, ns, (int)inst->z.id, (int)inst->z.chan);
            int8_t ra_v = ra_ensure_chan(ra, sl, ns, (int)inst->w.id, (int)inst->w.chan);
            int    p = inst->ptr;
            int    base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            int8_t scale = ra_alloc(ra, sl, ns);
            int8_t px    = ra_alloc(ra, sl, ns);
            int8_t t     = ra_alloc(ra, sl, ns);
            int8_t z     = ra_alloc(ra, sl, ns);
            if (scalar) {
                // Pack 4 u16 channels into 64 bits via word interleave.
                vex_rrr(c, 1, 1, 0, 0x61, px, rr, rg);     // VPUNPCKLWD [R,G,?,?...]
                vex_rrr(c, 1, 1, 0, 0x61, t, rb_, ra_v);   // VPUNPCKLWD [B,A,?,?...]
                vex_rrr(c, 1, 1, 0, 0x62, z, px, t);       // VPUNPCKLDQ [R,G,B,A,?,?...]
                vmovq_store(c, z, base, XI, 8, 0);
            } else {
                // Inputs are 8 x u16 in XMM.
                // Interleave to pixel order. Process low 4 then high 4 pixels.
                vex_rrr(c, 1, 1, 0, 0x61, scale, rr, rg);  // VPUNPCKLWD [R0,G0,R1,G1,R2,G2,R3,G3]
                vex_rrr(c, 1, 1, 0, 0x61, px, rb_, ra_v);  // VPUNPCKLWD [B0,A0,B1,A1,B2,A2,B3,A3]
                vex_rrr(c, 1, 1, 0, 0x62, t, scale, px);   // VPUNPCKLDQ pixels 0,1
                vex_rrr(c, 1, 1, 0, 0x6a, z, scale, px);   // VPUNPCKHDQ pixels 2,3
                vex(c, 1, 3, 0, 1, t, t, z, 0x38); emit1(c, 1);
                vmov_store(c, 1, t, base, XI, 8, 0);
                // High 4:
                vex_rrr(c, 1, 1, 0, 0x69, scale, rr, rg);  // VPUNPCKHWD [R4,G4,R5,G5,R6,G6,R7,G7]
                vex_rrr(c, 1, 1, 0, 0x69, px, rb_, ra_v);  // VPUNPCKHWD [B4,A4,B5,A5,B6,A6,B7,A7]
                vex_rrr(c, 1, 1, 0, 0x62, t, scale, px);   // VPUNPCKLDQ
                vex_rrr(c, 1, 1, 0, 0x6a, z, scale, px);   // VPUNPCKHDQ
                vex(c, 1, 3, 0, 1, t, t, z, 0x38); emit1(c, 1);
                vmov_store(c, 1, t, base, XI, 8, 32);
            }
            ra_return_reg(ra, z);
            ra_return_reg(ra, t);
            ra_return_reg(ra, px);
            ra_return_reg(ra, scale);
            free_chan(ra, inst->x, i);
            free_chan(ra, inst->y, i);
            free_chan(ra, inst->z, i);
            free_chan(ra, inst->w, i);
        } break;
        case op_store_16x4_planar: {
            int8_t rr   = ra_ensure_chan(ra, sl, ns, (int)inst->x.id, (int)inst->x.chan);
            int8_t rg   = ra_ensure_chan(ra, sl, ns, (int)inst->y.id, (int)inst->y.chan);
            int8_t rb_  = ra_ensure_chan(ra, sl, ns, (int)inst->z.id, (int)inst->z.chan);
            int8_t ra_v = ra_ensure_chan(ra, sl, ns, (int)inst->w.id, (int)inst->w.chan);
            int    p = inst->ptr;
            int    base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            {
                int const sz_off = p * (int)sizeof(struct umbra_buf)
                                 + (int)__builtin_offsetof(struct umbra_buf, sz);
                mov_rr(c, R11, base);
                mov_load(c, RBX, XBUF, sz_off);
                shr_ri(c, RBX, 2);
                if (scalar) {
                    // Plane 0 (R): VMOVD eax, rr; MOV word [R11+XI*2], ax
                    vmovd_to_gpr(c, RAX, rr);
                    {
                        emit1(c, 0x66);
                        uint8_t rex = 0x40;
                        if (XI >= 8)   { rex |= 0x02; }
                        if (R11 >= 8)  { rex |= 0x01; }
                        if (rex != 0x40) { emit1(c, rex); }
                        emit1(c, 0x89);
                        emit1(c, (uint8_t)(((RAX & 7) << 3) | 4));
                        emit1(c, (uint8_t)((1 << 6) | ((XI & 7) << 3) | (R11 & 7)));
                    }

                    // Plane 1 (G): advance R11 by plane_stride
                    rex_w(c, RBX, R11);
                    emit1(c, 0x01);
                    emit1(c, (uint8_t)(0xc0 | ((RBX & 7) << 3) | (R11 & 7)));
                    vmovd_to_gpr(c, RAX, rg);
                    {
                        emit1(c, 0x66);
                        uint8_t rex = 0x40;
                        if (XI >= 8)   { rex |= 0x02; }
                        if (R11 >= 8)  { rex |= 0x01; }
                        if (rex != 0x40) { emit1(c, rex); }
                        emit1(c, 0x89);
                        emit1(c, (uint8_t)(((RAX & 7) << 3) | 4));
                        emit1(c, (uint8_t)((1 << 6) | ((XI & 7) << 3) | (R11 & 7)));
                    }

                    // Plane 2 (B): advance R11 by plane_stride
                    rex_w(c, RBX, R11);
                    emit1(c, 0x01);
                    emit1(c, (uint8_t)(0xc0 | ((RBX & 7) << 3) | (R11 & 7)));
                    vmovd_to_gpr(c, RAX, rb_);
                    {
                        emit1(c, 0x66);
                        uint8_t rex = 0x40;
                        if (XI >= 8)   { rex |= 0x02; }
                        if (R11 >= 8)  { rex |= 0x01; }
                        if (rex != 0x40) { emit1(c, rex); }
                        emit1(c, 0x89);
                        emit1(c, (uint8_t)(((RAX & 7) << 3) | 4));
                        emit1(c, (uint8_t)((1 << 6) | ((XI & 7) << 3) | (R11 & 7)));
                    }

                    // Plane 3 (A): advance R11 by plane_stride
                    rex_w(c, RBX, R11);
                    emit1(c, 0x01);
                    emit1(c, (uint8_t)(0xc0 | ((RBX & 7) << 3) | (R11 & 7)));
                    vmovd_to_gpr(c, RAX, ra_v);
                    {
                        emit1(c, 0x66);
                        uint8_t rex = 0x40;
                        if (XI >= 8)   { rex |= 0x02; }
                        if (R11 >= 8)  { rex |= 0x01; }
                        if (rex != 0x40) { emit1(c, rex); }
                        emit1(c, 0x89);
                        emit1(c, (uint8_t)(((RAX & 7) << 3) | 4));
                        emit1(c, (uint8_t)((1 << 6) | ((XI & 7) << 3) | (R11 & 7)));
                    }
                } else {
                    // Plane 0 (R): store 8 x u16 (16 bytes)
                    vmov_store(c, 0, rr, R11, XI, 2, 0);

                    // Plane 1 (G): advance R11 by plane_stride
                    rex_w(c, RBX, R11);
                    emit1(c, 0x01);
                    emit1(c, (uint8_t)(0xc0 | ((RBX & 7) << 3) | (R11 & 7)));
                    vmov_store(c, 0, rg, R11, XI, 2, 0);

                    // Plane 2 (B): advance R11 by plane_stride
                    rex_w(c, RBX, R11);
                    emit1(c, 0x01);
                    emit1(c, (uint8_t)(0xc0 | ((RBX & 7) << 3) | (R11 & 7)));
                    vmov_store(c, 0, rb_, R11, XI, 2, 0);

                    // Plane 3 (A): advance R11 by plane_stride
                    rex_w(c, RBX, R11);
                    emit1(c, 0x01);
                    emit1(c, (uint8_t)(0xc0 | ((RBX & 7) << 3) | (R11 & 7)));
                    vmov_store(c, 0, ra_v, R11, XI, 2, 0);
                }
                last_ptr = -1;
            }
            free_chan(ra, inst->x, i);
            free_chan(ra, inst->y, i);
            free_chan(ra, inst->z, i);
            free_chan(ra, inst->w, i);
        } break;

        case op_load_8x4: {
            struct ra_step s0 = ra_step_alloc(ra, sl, ns, i);
            int8_t r1 = ra_alloc(ra, sl, ns);
            int8_t r2 = ra_alloc(ra, sl, ns);
            int8_t r3 = ra_alloc(ra, sl, ns);
            int    p = inst->ptr;
            int    base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            int8_t px   = ra_alloc(ra, sl, ns);
            int8_t mask = ra_alloc(ra, sl, ns);
            broadcast_imm32(c, mask, 0xFF);
            if (scalar) {
                vmovd_load(c, px, base, XI, 4, 0);
            } else {
                vmov_load(c, 1, px, base, XI, 4, 0);
            }
            vpand(c, scalar ? 0 : 1, s0.rd, px, mask);
            vpsrld_i(c, r1, px, 8);  vpand(c, scalar ? 0 : 1, r1, r1, mask);
            vpsrld_i(c, r2, px, 16); vpand(c, scalar ? 0 : 1, r2, r2, mask);
            vpsrld_i(c, r3, px, 24);
            ra_return_reg(ra, mask);
            ra_return_reg(ra, px);
            ra_set_chan_reg(ra, i, 0, s0.rd);
            ra_set_chan_reg(ra, i, 1, r1);
            ra_set_chan_reg(ra, i, 2, r2);
            ra_set_chan_reg(ra, i, 3, r3);
        } break;
        case op_store_8x4: {
            int8_t rr   = ra_ensure_chan(ra, sl, ns, (int)inst->x.id, (int)inst->x.chan);
            int8_t rg   = ra_ensure_chan(ra, sl, ns, (int)inst->y.id, (int)inst->y.chan);
            int8_t rb_  = ra_ensure_chan(ra, sl, ns, (int)inst->z.id, (int)inst->z.chan);
            int8_t ra_v = ra_ensure_chan(ra, sl, ns, (int)inst->w.id, (int)inst->w.chan);
            int    p = inst->ptr;
            int    base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            int8_t px = ra_alloc(ra, sl, ns);
            int8_t t  = ra_alloc(ra, sl, ns);
            int L = scalar ? 0 : 1;
            vpslld_i(c, t, rg, 8);    vpor(c, L, px, rr, t);
            vpslld_i(c, t, rb_, 16);  vpor(c, L, px, px, t);
            vpslld_i(c, t, ra_v, 24); vpor(c, L, px, px, t);
            if (scalar) {
                vmovd_store(c, px, base, XI, 4, 0);
            } else {
                vmov_store(c, 1, px, base, XI, 4, 0);
            }
            ra_return_reg(ra, t);
            ra_return_reg(ra, px);
            free_chan(ra, inst->x, i);
            free_chan(ra, inst->y, i);
            free_chan(ra, inst->z, i);
            free_chan(ra, inst->w, i);
        } break;

        case op_store_16: {
            int8_t ry = ra_ensure(ra, sl, ns, (int)inst->y.id);
            int    p = inst->ptr;
            int    base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            if (scalar) {
                // VMOVD eax, xmm
                vmovd_to_gpr(c, RAX, ry);
                // MOV word [base + R10*2], ax
                {
                    emit1(c, 0x66);
                    uint8_t rex = 0x40;
                    if (XI >= 8) { rex |= 0x02; }
                    if (base >= 8) { rex |= 0x01; }
                    if (rex != 0x40) { emit1(c, rex); }
                    emit1(c, 0x89);
                    emit1(c, (uint8_t)(((RAX & 7) << 3) | 4));
                    emit1(c, (uint8_t)((1 << 6) | ((XI & 7) << 3) | (base & 7)));
                }
            } else {
                vmov_store(c, 0, ry, base, XI, 2, 0);
            }
            free_chan(ra, inst->y, i);
        } break;

        case op_uniform_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int            p = inst->ptr;
            int            base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            {
                int     disp = inst->imm;
                uint8_t R = (uint8_t)(~s.rd >> 3) & 1;
                uint8_t B = (uint8_t)(~base >> 3) & 1;
                emit1(c, 0xc4);
                emit1(c, (uint8_t)((R << 7) | (1 << 6) | (B << 5) | 0x02));
                emit1(c, 0x7d);
                emit1(c, 0x18);
                if (disp == 0 && (base & 7) != RBP) {
                    emit1(c, (uint8_t)(((s.rd & 7) << 3) | (base & 7)));
                    if ((base & 7) == RSP) { emit1(c, 0x24); }
                } else if (disp >= -128 && disp <= 127) {
                    emit1(c, (uint8_t)(0x40 | ((s.rd & 7) << 3) | (base & 7)));
                    if ((base & 7) == RSP) { emit1(c, 0x24); }
                    emit1(c, (uint8_t)disp);
                } else {
                    emit1(c, (uint8_t)(0x80 | ((s.rd & 7) << 3) | (base & 7)));
                    if ((base & 7) == RSP) { emit1(c, 0x24); }
                    emit4(c, (uint32_t)disp);
                }
            }
        } break;

        case op_gather_uniform_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int8_t         rx = ra_ensure(ra, sl, ns, (int)inst->x.id);
            int            p = inst->ptr;
            int            base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            vmovd_to_gpr(c, RAX, rx);
            free_chan(ra, inst->x, i);
            load_count_x86(c, p, 2);
            vpxor(c, scalar ? 0 : 1, s.rd, s.rd, s.rd);
            cmp_rr(c, RAX, XM);
            int skip = jcc(c, 0x03);
            vmovd_load(c, s.rd, base, RAX, 4, 0);
            if (!scalar) {
                vbroadcastss(c, s.rd, s.rd);
            }
            patch_jcc(c, skip);
        } break;

        case op_gather_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int8_t         rx = ra_ensure(ra, sl, ns, (int)inst->x.id);
            int            p = inst->ptr;
            int            base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            if (scalar) {
                vmovd_to_gpr(c, RAX, rx);
                free_chan(ra, inst->x, i);
                load_count_x86(c, p, 2);
                vpxor(c, 0, s.rd, s.rd, s.rd);
                cmp_rr(c, RAX, XM);
                int skip = jcc(c, 0x03);
                vmovd_load(c, s.rd, base, RAX, 4, 0);
                patch_jcc(c, skip);
            } else {
                int8_t mask = ra_alloc(ra, sl, ns);
                int8_t cnt = ra_alloc(ra, sl, ns);
                load_count_x86(c, p, 2);
                // Build in-bounds mask: (ix >= 0) AND (ix < count)
                vpxor(c, 1, mask, mask, mask);
                vpcmpgtd(c, mask, mask, rx);          // mask = (0 > ix) = neg lanes
                vmovd_from_gpr(c, cnt, XM);
                vbroadcastss(c, cnt, cnt);             // cnt = broadcast(count)
                vpcmpgtd(c, cnt, cnt, rx);             // cnt = (count > ix)
                // in_bounds = NOT(neg) AND (count > ix) = VPANDN(neg, count>ix)
                vex_rrr(c, 1, 1, 1, 0xDF, mask, mask, cnt); // mask = NOT(mask) AND cnt
                // Pre-zero dst, gather only in-bounds lanes
                vpxor(c, 1, s.rd, s.rd, s.rd);
                vpgatherdd(c, s.rd, base, rx, 4, mask);
                ra_return_reg(ra, cnt);
                ra_return_reg(ra, mask);
                free_chan(ra, inst->x, i);
            }
        } break;

        case op_sample_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int8_t         rx = ra_ensure(ra, sl, ns, (int)inst->x.id);
            int            p = inst->ptr;
            int            base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            int8_t frac_r = ra_alloc(ra, sl, ns);
            int8_t hi_r   = ra_alloc(ra, sl, ns);
            if (scalar) {
                // floor + frac
                vroundps(c, hi_r, rx, 1);                  // hi_r = floor(ix)
                vsubps(c, frac_r, rx, hi_r);                // frac_r = ix - floor
                vcvttps2dq(c, hi_r, hi_r);                  // hi_r = int(floor)
                free_chan(ra, inst->x, i);
                load_count_x86(c, p, 2);
                // gather lo
                vmovd_to_gpr(c, RAX, hi_r);   // vmovd eax, lo_i
                vpxor(c, 0, s.rd, s.rd, s.rd);
                cmp_rr(c, RAX, XM);
                int skip_lo = jcc(c, 0x03);
                vmovd_load(c, s.rd, base, RAX, 4, 0);
                patch_jcc(c, skip_lo);
                // gather hi (lo_i + 1)
                vmovd_to_gpr(c, RAX, hi_r);
                add_ri(c, RAX, 1);
                vpxor(c, 0, hi_r, hi_r, hi_r);
                cmp_rr(c, RAX, XM);
                int skip_hi = jcc(c, 0x03);
                vmovd_load(c, hi_r, base, RAX, 4, 0);
                patch_jcc(c, skip_hi);
                // lerp: s.rd += (hi - lo) * frac
                vsubps(c, hi_r, hi_r, s.rd);
                vfmadd231ps(c, s.rd, hi_r, frac_r);
            } else {
                // floor + frac + int index
                int8_t int_ix = ra_alloc(ra, sl, ns);
                vroundps(c, hi_r, rx, 1);                  // hi_r = floor(ix)
                vsubps(c, frac_r, rx, hi_r);                // frac_r = ix - floor
                vcvttps2dq(c, int_ix, hi_r);                // int_ix = int(floor)
                free_chan(ra, inst->x, i);
                load_count_x86(c, p, 2);
                // Load broadcast(count) once into cnt, keep it alive for both gathers.
                // Use hi_r as temp for comparison results (it's free until the hi gather).
                int8_t mask = ra_alloc(ra, sl, ns);
                int8_t cnt = ra_alloc(ra, sl, ns);
                vmovd_from_gpr(c, cnt, XM);       // vmovd cnt, XM
                vbroadcastss(c, cnt, cnt);                     // cnt = broadcast(count)
                // lo mask: !neg & (count > ix)
                vpxor(c, 1, mask, mask, mask);
                vpcmpgtd(c, mask, mask, int_ix);               // mask = (0 > ix) = neg
                vpcmpgtd(c, hi_r, cnt, int_ix);                // hi_r = (count > ix)
                vex_rrr(c, 1, 1, 1, 0xDF, mask, mask, hi_r);  // mask = !neg & (count>ix)
                // gather lo
                vpxor(c, 1, s.rd, s.rd, s.rd);
                vpgatherdd(c, s.rd, base, int_ix, 4, mask);
                // int_ix += 1
                vpcmpeqd(c, hi_r, hi_r, hi_r);
                vpsrld_i(c, hi_r, hi_r, 31);
                vpaddd(c, int_ix, int_ix, hi_r);
                // hi mask: !neg & (count > ix+1), reusing preserved cnt
                vpxor(c, 1, mask, mask, mask);
                vpcmpgtd(c, mask, mask, int_ix);               // mask = neg
                vpcmpgtd(c, hi_r, cnt, int_ix);                // hi_r = (count > ix+1)
                vex_rrr(c, 1, 1, 1, 0xDF, mask, mask, hi_r);  // mask = !neg & (count>ix+1)
                // gather hi
                vpxor(c, 1, hi_r, hi_r, hi_r);
                vpgatherdd(c, hi_r, base, int_ix, 4, mask);
                ra_return_reg(ra, cnt);
                ra_return_reg(ra, mask);
                ra_return_reg(ra, int_ix);
                // lerp: s.rd += (hi - lo) * frac
                vsubps(c, hi_r, hi_r, s.rd);
                vfmadd231ps(c, s.rd, hi_r, frac_r);
            }
            ra_return_reg(ra, hi_r);
            ra_return_reg(ra, frac_r);
        } break;

        case op_gather_16: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int8_t         rx = ra_ensure(ra, sl, ns, (int)inst->x.id);
            int            p = inst->ptr;
            int            base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            load_count_x86(c, p, 1);
            if (scalar) {
                vmovd_to_gpr(c, RAX, rx);
                free_chan(ra, inst->x, i);
                vpxor(c, 0, s.rd, s.rd, s.rd);
                cmp_rr(c, RAX, XM);
                int skip = jcc(c, 0x03);
                {
                    uint8_t rex = 0x40;
                    if (base >= 8) { rex |= 0x01; }
                    if (rex != 0x40) { emit1(c, rex); }
                    emit1(c, 0x0f);
                    emit1(c, 0xb7u);
                    emit1(c, (uint8_t)(((RAX & 7) << 3) | 4));
                    emit1(c, (uint8_t)((1 << 6) | ((RAX & 7) << 3) | (base & 7)));
                }
                vmovd_from_gpr(c, s.rd, RAX);
                patch_jcc(c, skip);
            } else {
                int8_t hi_idx = ra_alloc(ra, sl, ns);
                vextracti128(c, hi_idx, rx, 1);
                vpxor(c, 0, s.rd, s.rd, s.rd);
                for (int k = 0; k < 8; k++) {
                    int src = (k < 4) ? rx : hi_idx;
                    int lane = k & 3;
                    if (lane == 0) {
                        vmovd_to_gpr(c, RAX, src);
                    } else {
                        vpextrd(c, RAX, src, (uint8_t)lane);
                    }
                    cmp_rr(c, RAX, XM);
                    int skip = jcc(c, 0x03);
                    {
                        uint8_t rex2 = 0x40;
                        if (base >= 8) { rex2 |= 0x01; }
                        if (rex2 != 0x40) { emit1(c, rex2); }
                        emit1(c, 0x0f);
                        emit1(c, 0xb7);
                        emit1(c, (uint8_t)(((RAX & 7) << 3) | 4));
                        emit1(c, (uint8_t)((1 << 6) | ((RAX & 7) << 3) | (base & 7)));
                    }
                    vex(c, 1, 1, 0, 0, s.rd, s.rd, RAX, 0xC4);
                    emit1(c, (uint8_t)k);
                    patch_jcc(c, skip);
                }
                ra_return_reg(ra, hi_idx);
                free_chan(ra, inst->x, i);
            }
        } break;
        case op_f32_from_f16: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            vcvtph2ps(c, s.rd, s.rx);
        } break;

        case op_f16_from_f32: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            vcvtps2ph(c, s.rd, s.rx, 4);
        } break;

        case op_i32_from_s16: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            vpmovsxwd(c, s.rd, s.rx);
        } break;

        case op_i32_from_u16: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            vpmovzxwd(c, s.rd, s.rx);
        } break;

        case op_i16_from_i32: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            if (scalar) {
                vmovaps(c, s.rd, s.rx);
            } else {
                int8_t tmp = ra_alloc(ra, sl, ns);
                vextracti128(c, tmp, s.rx, 1);
                vex_rrr(c, 1, 2, 0, 0x2b, s.rd, s.rx, tmp);
                ra_return_reg(ra, tmp);
            }
        } break;

        case op_imm_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            pool_broadcast(c, &jc->pool, s.rd, (uint32_t)inst->imm);
        } break;

        case op_lt_s32_imm: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            int8_t tmp = ra_alloc(ra, sl, ns);
            pool_broadcast(c, &jc->pool, tmp, (uint32_t)inst->imm);
            vpcmpgtd(c, s.rd, tmp, s.rx);
            ra_return_reg(ra, tmp);
        } break;
        case op_le_s32_imm: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            int8_t tmp = ra_alloc(ra, sl, ns);
            pool_broadcast(c, &jc->pool, tmp, (uint32_t)inst->imm);
            vpcmpgtd(c, s.rd, s.rx, tmp);
            vpcmpeqd(c, tmp, tmp, tmp);
            vpxor_3(c, 1, s.rd, s.rd, tmp);
            ra_return_reg(ra, tmp);
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
        case op_eq_i32_imm: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            uint32_t       v = (uint32_t)inst->imm;
            uint32_t       bcast[8] = {v, v, v, v, v, v, v, v};
            int            off = pool_add(&jc->pool, bcast, 32);
            enum op        o = inst->op;
            int            pp = 0, mm = 1;
            uint8_t        vop = 0;
            if (o == op_and_32_imm) {
                pp = 1;
                vop = 0xdb;
            } else if (o == op_add_f32_imm) {
                vop = 0x58;
            } else if (o == op_sub_f32_imm) {
                vop = 0x5c;
            } else if (o == op_mul_f32_imm) {
                vop = 0x59;
            } else if (o == op_div_f32_imm) {
                vop = 0x5e;
            } else if (o == op_min_f32_imm) {
                vop = 0x5d;
            } else if (o == op_max_f32_imm) {
                vop = 0x5f;
            } else if (o == op_add_i32_imm) {
                pp = 1;
                vop = 0xfe;
            } else if (o == op_sub_i32_imm) {
                pp = 1;
                vop = 0xfa;
            } else if (o == op_mul_i32_imm) {
                pp = 1;
                mm = 2;
                vop = 0x40;
            } else if (o == op_or_32_imm) {
                pp = 1;
                vop = 0xeb;
            } else if (o == op_xor_32_imm) {
                pp = 1;
                vop = 0xef;
            } else if (o == op_eq_i32_imm) {
                pp = 1;
                vop = 0x76;
            } else {
                vop = 0xc2;
            }
            if (o == op_eq_f32_imm || o == op_lt_f32_imm || o == op_le_f32_imm) {
                uint8_t pred = o == op_eq_f32_imm ? 0 : o == op_lt_f32_imm ? 1 : 2;
                int     pos = vex_rip(c, pp, mm, 0, 1, s.rd, s.rx, vop);
                emit1(c, pred);
                pool_ref_at(&jc->pool, off, pos, 1);
            } else {
                int pos = vex_rip(c, pp, mm, 0, 1, s.rd, s.rx, vop);
                pool_ref_at(&jc->pool, off, pos, 0);
            }
        } break;

        case op_abs_f32: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            uint32_t       mask[8] = {0x7fffffffu, 0x7fffffffu, 0x7fffffffu, 0x7fffffffu,
                                      0x7fffffffu, 0x7fffffffu, 0x7fffffffu, 0x7fffffffu};
            int            off = pool_add(&jc->pool, mask, 32);
            int            pos = vex_rip(c, 0, 1, 0, 1, s.rd, s.rx, 0x54);
            pool_ref_at(&jc->pool, off, pos, 0);
        } break;
        case op_add_f32:
        case op_sub_f32:
        case op_mul_f32:
        case op_div_f32:
        case op_min_f32:
        case op_max_f32:
        case op_sqrt_f32:
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
        case op_le_u32: {
            int            nscratch = (inst->op == op_lt_u32)                 ? 2
                           : (inst->op == op_le_s32 || inst->op == op_le_u32) ? 1
                                                                              : 0;
            struct ra_step s = ra_step_alu(ra, sl, ns, inst, i, scalar, nscratch);
            emit_alu_reg(c, inst->op, s.rd, s.rx, s.ry, s.rz, inst->imm, s.scratch,
                         s.scratch2);
            if (s.scratch >= 0) { ra_return_reg(ra, s.scratch); }
            if (s.scratch2 >= 0) { ra_return_reg(ra, s.scratch2); }
        } break;

        case op_shl_i32_imm:
        case op_shr_u32_imm:
        case op_shr_s32_imm: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            emit_alu_reg(c, inst->op, s.rd, s.rx, 0, 0, inst->imm, -1, -1);
        } break;

        }
    }
#undef lu
}

#if __clang__
__attribute__((no_sanitize("function")))
#endif
static void umbra_jit_run(struct umbra_jit *j, int l, int t, int r, int b, struct umbra_buf buf[]) {
    j->entry(l, t, r, b, buf);
}

static void umbra_jit_free(struct umbra_jit *j) {
    munmap(j->code, j->code_size);
    free(j);
}

static _Bool x86_disasm(uint8_t const *code, size_t n, char const *spath,
                        char const *opath, FILE *f) {
    _Bool result = 0;
    FILE *fp = fopen(spath, "w");
    if (fp) {
        for (size_t i = 0; i < n; i++) {
            fprintf(fp, ".byte 0x%02x\n", code[i]);
        }
        fclose(fp);

        char cmd[1024];
        snprintf(cmd, sizeof cmd,
                 "/opt/homebrew/opt/llvm/bin/clang"
                 " -target x86_64-apple-macos13 -c %s -o %s 2>/dev/null &&"
                 " /opt/homebrew/opt/llvm/bin/llvm-objdump"
                 " -d --no-show-raw-insn --no-leading-addr %s 2>/dev/null",
                 spath, opath, opath);
        FILE *p = popen(cmd, "r");
        if (p) {
            char  line[256];
            _Bool ok = 0;
            while (fgets(line, (int)sizeof line, p)) {
                if (!ok && __builtin_strstr(line, "file format")) {
                    ok = 1;
                    continue;
                }
                if (f) {
                    fputs(line, f);
                }
            }
            result = pclose(p) == 0 && ok;
        }
    }
    return result;
}

static void umbra_dump_jit_mca(struct umbra_jit const *j, FILE *f) {
    if (j->loop_start >= j->loop_end) { return; }

    uint8_t const *code = (uint8_t const*)j->code;
    size_t const   n    = (size_t)(j->loop_end - j->loop_start);

    char spath[]     = "/tmp/umbra_mca_XXXXXX.s";
    char asmpath[]   = "/tmp/umbra_mca_asm_XXXXXX.s";
    char cleanpath[] = "/tmp/umbra_mca_clean_XXXXXX.s";
    char opath[sizeof spath + 2];
    _Bool have_s = 0, have_asm = 0, have_clean = 0;

    int   fd, afd, cfd;
    FILE *afp, *cfp;

    fd = mkstemps(spath, 2);
    if (fd < 0) { goto done; }
    have_s = 1;
    close(fd);
    snprintf(opath, sizeof opath, "%.*s.o", (int)(sizeof spath - 3), spath);

    afd = mkstemps(asmpath, 2);
    if (afd < 0) { goto done; }
    have_asm = 1;
    close(afd);

    afp = fopen(asmpath, "w");
    if (!afp)                                                    { goto done; }
    if (!x86_disasm(code + j->loop_start, n, spath, opath, afp)) { fclose(afp); goto done; }
    fclose(afp);

    cfd = mkstemps(cleanpath, 2);
    if (cfd < 0) { goto done; }
    have_clean = 1;
    cfp = fdopen(cfd, "w");
    if (!cfp) { close(cfd); goto done; }

    afp = fopen(asmpath, "r");
    if (afp) {
        char  line[256];
        _Bool past_header = 0;
        while (fgets(line, (int)sizeof line, afp)) {
            if (past_header) {
                if (line[0] != '\n' && line[0] != '<') {
                    char *angle = __builtin_strchr(line, '<');
                    if (angle) {
                        *angle = '\n';
                        angle[1] = '\0';
                    }
                    fputs(line, cfp);
                }
            } else if (__builtin_strstr(line, "<")) {
                past_header = 1;
            }
        }
        fclose(afp);
    }
    fclose(cfp);

    {
        char cmd[1024];
        snprintf(cmd, sizeof cmd,
                 "/opt/homebrew/opt/llvm/bin/llvm-mca"
                 " -mcpu=znver4 -iterations=100 -bottleneck-analysis"
                 " -mtriple=x86_64 %s 2>&1",
                 cleanpath);
        FILE *p = popen(cmd, "r");
        if (p) {
            int const cplen = (int)__builtin_strlen(cleanpath);
            char      line[256];
            while (fgets(line, (int)sizeof line, p)) {
                char *s = line;
                if (__builtin_strncmp(s, cleanpath, (size_t)cplen) == 0) {
                    s += cplen;
                }
                fputs(s, f);
            }
            pclose(p);
        }
    }

done:
    if (have_s)     { remove(spath); remove(opath); }
    if (have_asm)   { remove(asmpath); }
    if (have_clean) { remove(cleanpath); }
}

static void run_jit(struct umbra_program *prog, int l, int t, int r, int b, struct umbra_buf buf[]) {
    umbra_jit_run((struct umbra_jit*)prog, l, t, r, b, buf);
}
static void dump_jit(struct umbra_program const *prog, FILE *f) { umbra_dump_jit_mca((struct umbra_jit const*)prog, f); }
static void free_jit(struct umbra_program *prog) { umbra_jit_free((struct umbra_jit*)prog); }
static struct umbra_program *compile_jit(struct umbra_backend           *be,
                                         struct umbra_basic_block const *bb) {
    struct umbra_jit *const j = umbra_jit(bb);
    j->base = (struct umbra_program){
        .queue   = run_jit,
        .dump    = dump_jit,
        .free    = free_jit,
        .backend = be,
    };
    return &j->base;
}
static void flush_be_noop(struct umbra_backend *be) { (void)be; }
static void free_be_jit(struct umbra_backend *be) { free(be); }
struct umbra_backend *umbra_backend_jit(void) {
    struct umbra_backend *const be = malloc(sizeof *be);
    *be = (struct umbra_backend){
        .compile    = compile_jit,
        .flush      = flush_be_noop,
        .free    = free_be_jit,
        .threadsafe = 1,
    };
    return be;
}

#endif // __aarch64__ || __AVX2__
#endif // outer guard
