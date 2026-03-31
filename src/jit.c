#include "bb.h"
#include <assert.h>

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

_Static_assert(sizeof(umbra_buf) == 32, "");
_Static_assert(offsetof(umbra_buf, ptr)       ==  0, "");
_Static_assert(offsetof(umbra_buf, sz)        ==  8, "");
_Static_assert(offsetof(umbra_buf, row_bytes) == 16, "");

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
        int disp_ptr = p * (int)sizeof(umbra_buf);
        int disp_rb  = p * (int)sizeof(umbra_buf) + (int)__builtin_offsetof(umbra_buf, row_bytes);
        put(c, LDR_xi(XP, XBUF, disp_ptr / 8));
        put(c, LDR_xi(XT, XBUF, disp_rb  / 8));
        put(c, MADD_x(XP, XY, XT, XP));
    }
}

static void resolve_ptr(Buf *c, int p, int *last_ptr, int const *deref_gpr,
                        int const *deref_rb_gpr) {
    if (p < 0) {
        *last_ptr = -1;
        int gpr = deref_gpr[~p];
        int rbg = deref_rb_gpr[~p];
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
    if (p < 0) {
        (void)deref_gpr;
        put(c, 0xd2a00000u | (0x7fffu << 5) | (uint32_t)XM);
    } else {
        int disp = p * (int)sizeof(umbra_buf) + (int)__builtin_offsetof(umbra_buf, sz);
        put(c, LDR_xi(XM, XBUF, disp / 8));
        if (elem_shift) {
            put(c,
                0x53000000u
                    | ((uint32_t)elem_shift << 16)
                    | (31u << 10)
                    | ((uint32_t)XM << 5)
                    | (uint32_t)XM);
        }
    }
}

static void vld(Buf *c, int vd, int s) { put(c, LDR_qi(vd, XS, s)); }
static void vst(Buf *c, int vd, int s) { put(c, STR_qi(vd, XS, s)); }

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
    case op_neg_f32: put(c, FNEG_4s(d, x)); break;
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
    case op_pack:
        if (d == x) {
            put(c, SLI_4s_imm(d, y, imm));
        } else if (d != y) {
            put(c, ORR_16b(d, x, x));
            put(c, SLI_4s_imm(d, y, imm));
        } else {
            put(c, ORR_16b(scratch, x, x));
            put(c, SLI_4s_imm(scratch, y, imm));
            put(c, ORR_16b(d, scratch, scratch));
        }
        break;

    default: assert(0); break;
    }
}

static int8_t const ra_pool[] = {
    4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

struct jit_ctx {
    Buf                            *c;
    struct umbra_basic_block const *bb;
    struct pool                     pool;
};

static void arm64_spill(int reg, int slot, void *ctx) {
    vst(((struct jit_ctx *)ctx)->c, reg, slot);
}
static void arm64_fill(int reg, int slot, void *ctx) {
    vld(((struct jit_ctx *)ctx)->c, reg, slot);
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
    put(c, 0x9c000000u | (uint32_t)d);
}
static void arm64_pool_load_wide(Buf *c, struct pool *p, int d, void const *data) {
    int off = pool_add(p, data, 16);
    pool_ref_at(p, off, c->len);
    put(c, 0x9c000000u | (uint32_t)d);
}
static void arm64_remat(int reg, int val, void *ctx) {
    struct jit_ctx *j = ctx;
    arm64_pool_load(j->c, &j->pool, reg, (uint32_t)j->bb->inst[val].imm);
}

static struct ra *ra_create_arm64(struct umbra_basic_block const *bb, struct jit_ctx *jc) {
    struct ra_config cfg = {
        .pool = ra_pool,
        .nregs = 20,
        .max_reg = 32,
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
    void (*entry)(int, int, int, int, umbra_buf *);
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
    int stack_patch = c.len;
    put(&c, 0xd503201fu);
    put(&c, 0xd503201fu);

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
    put(&c, 0xcb000000u | ((uint32_t)XCOL << 16) | (0u << 5) | (uint32_t)XT); // SUB XT,X0,XCOL
    put(&c, SUBS_xi(31, XT, 4));
    int br_tail = c.len;
    put(&c, Bcond(0xb, 0));
    put(&c, LSL_xi(XH, XCOL, 1));
    put(&c, LSL_xi(XW, XCOL, 2));

    int loop_body_start = c.len;
    emit_ops(&c, bb, bb->preamble, bb->insts, sl, &ns, ra, 0, deref_gpr, deref_rb_gpr, &jc);

    ra_end_loop(ra, sl);

    int loop_body_end = c.len;

    put(&c, ADD_xi(XCOL, XCOL, 4));
    put(&c, B(loop_top - c.len));

    int tail_top = c.len;
    c.buf[br_tail] = Bcond(0xb, tail_top - br_tail);

    // CMP XCOL, X0 (col_end): xcol >= col_end means row is done
    put(&c, 0xeb00001fu | ((uint32_t)XCOL << 5));
    int br_row_done = c.len;
    put(&c, Bcond(0xa, 0));  // B.GE row_done

    for (int i = 0; i < bb->insts; i++) { ra_free_reg(ra, i); }
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
    // CMP XY, XT — XY >= end_row means done
    put(&c, 0xeb000000u | ((uint32_t)XT << 16) | ((uint32_t)XY << 5) | 0x1fu);
    int br_more_rows = c.len;
    put(&c, Bcond(0xb, 0));  // B.LT → more rows

    // Restore stack (saved end_row)
    put(&c, LDP_post(0, 15, 31, 2));

    // Patch: B.LT → loop_top
    c.buf[br_more_rows] = Bcond(0xb, loop_top - br_more_rows);

    put(&c, ADD_xi(31, 29, 0));
    put(&c, LDP_post(29, 30, 31, 2));
    put(&c, RET());

    if (ns > 0) {
        c.buf[stack_patch] = SUB_xi(31, 31, ns * 16);
    }
    c.buf[stack_patch + 1] = ADD_xi(XS, 31, 0);
    while (c.len & 3) {
        put(&c, 0xd503201fu);
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
    if (mem == MAP_FAILED) {
        free(c.buf);
        return 0;
    }
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
            void (*fn)(int, int, int, int, umbra_buf *);
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
            put(c, DUP_4s_w(s.rd, XCOL));
            if (!scalar) {
                int8_t   tmp = ra_alloc(ra, sl, ns);
                uint32_t iota4[4] = {0, 1, 2, 3};
                arm64_pool_load_wide(c, &jc->pool, tmp, iota4);
                put(c, ADD_4s(s.rd, s.rd, tmp));
                ra_return_reg(ra, tmp);
            }
        } break;

        case op_y: {
            // y = row counter XY, broadcast.
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            put(c, DUP_4s_w(s.rd, XY));
        } break;

        case op_load_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int            p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            if (scalar) {
                put(c, LDR_sx(s.rd, XP, XI));
            } else {
                put(c, LDR_q(s.rd, XP, XW));
            }
        } break;

        case op_load_16: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int            p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            if (scalar) {
                put(c, LDRH_wr(XT, XP, XI));
                put(c, DUP_4s_w(s.rd, XT));
            } else {
                put(c, LDR_d(s.rd, XP, XH));
            }
        } break;

        case op_uniform_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int            p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            load_imm_w(c, XT, (uint32_t)inst->imm);
            put(c, LDR_sx(s.rd, XP, XT));
            put(c, DUP_4s_lane(s.rd, s.rd, 0));
        } break;

        case op_gather_uniform_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int8_t         rx = ra_ensure(ra, sl, ns, (int)inst->x.id);
            free_chan(ra, inst->x, i);
            int p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            load_count(c, p, 2, deref_gpr);
            put(c, UMOV_ws(XT, rx));
            put(c, MOVI_4s(s.rd, 0, 0));
            put(c, CMP_wr(XT, XM));
            put(c, Bcond(0x2, 3));
            put(c, LDR_wr(XT, XP, XT));
            put(c, DUP_4s_w(s.rd, XT));
        } break;

        case op_gather_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int8_t         rx = ra_ensure(ra, sl, ns, (int)inst->x.id);
            free_chan(ra, inst->x, i);
            int p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            load_count(c, p, 2, deref_gpr);
            if (scalar) {
                put(c, MOVI_4s(s.rd, 0, 0));
                put(c, UMOV_ws(XT, rx));
                put(c, CMP_wr(XT, XM));
                put(c, Bcond(0x2, 2));
                put(c, LDR_sx(s.rd, XP, XT));
            } else {
                put(c, MOVI_4s(s.rd, 0, 0));
                for (int k = 0; k < 4; k++) {
                    put(c, UMOV_ws_lane(XT, rx, k));
                    put(c, CMP_wr(XT, XM));
                    put(c, Bcond(0x2, 4));
                    put(c, LSL_xi(XT, XT, 2));
                    put(c, ADD_xr(XT, XP, XT));
                    put(c, LD1_s(s.rd, k, XT));
                }
            }
        } break;

        case op_gather_16: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int8_t         rx = ra_ensure(ra, sl, ns, (int)inst->x.id);
            free_chan(ra, inst->x, i);
            int p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            load_count(c, p, 1, deref_gpr);
            if (scalar) {
                put(c, MOVI_4s(s.rd, 0, 0));
                put(c, UMOV_ws(XT, rx));
                put(c, CMP_wr(XT, XM));
                put(c, Bcond(0x2, 5));
                put(c, LSL_xi(XT, XT, 1));
                put(c, ADD_xr(XT, XP, XT));
                put(c, LDRH_wi(XT, XT, 0));
                put(c, DUP_4s_w(s.rd, XT));
            } else {
                put(c, MOVI_4s(s.rd, 0, 0));
                for (int k = 0; k < 4; k++) {
                    put(c, UMOV_ws_lane(XT, rx, k));
                    put(c, CMP_wr(XT, XM));
                    put(c, Bcond(0x2, 5));
                    put(c, LSL_xi(XT, XT, 1));
                    put(c, ADD_xr(XT, XP, XT));
                    put(c, LDRH_wi(XT, XT, 0));
                    put(c, INS_s(s.rd, k, XT));
                }
                put(c, XTN_4h(s.rd, s.rd));
            }
        } break;

        case op_store_32: {
            int8_t ry = ra_ensure(ra, sl, ns, (int)inst->y.id);
            int    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            if (scalar) {
                put(c, STR_sx(ry, XP, XI));
            } else {
                put(c, STR_q(ry, XP, XW));
            }
            free_chan(ra, inst->y, i);
        } break;

        case op_load_fp16x4: {
            struct ra_step s0 = ra_step_alloc(ra, sl, ns, i);
            int8_t r1 = ra_alloc(ra, sl, ns);
            int8_t r2 = ra_alloc(ra, sl, ns);
            int8_t r3 = ra_alloc(ra, sl, ns);
            int    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            int8_t px = ra_alloc(ra, sl, ns);
            int8_t t0 = ra_alloc(ra, sl, ns);
            int8_t t1 = ra_alloc(ra, sl, ns);
            if (scalar) {
                put(c, LSL_xi(XT, XI, 3));
                put(c, ADD_xr(XT, XP, XT));
                put(c, LDR_di(px, XT, 0));
                put(c, FCVTL_4s(px, px));
                put(c, DUP_4s_lane(s0.rd, px, 0));
                put(c, DUP_4s_lane(r1,    px, 1));
                put(c, DUP_4s_lane(r2,    px, 2));
                put(c, DUP_4s_lane(r3,    px, 3));
            } else {
                put(c, LSL_xi(XT, XI, 3));
                put(c, ADD_xr(XT, XP, XT));
                put(c, LDR_qi(px, XT, 0));
                put(c, LDR_qi(t0, XT, 1));
                put(c, UZP1_8h(t1, px, t0));
                put(c, UZP2_8h(px, px, t0));
                put(c, UZP1_8h(t0, t1, px));
                put(c, UZP2_8h(t1, t1, px));
                put(c, FCVTL_4s(s0.rd, t0));
                put(c, FCVTL_4s(r2, t1));
                put(c, EXT_16b(t0, t0, t0, 8));
                put(c, FCVTL_4s(r1, t0));
                put(c, EXT_16b(t1, t1, t1, 8));
                put(c, FCVTL_4s(r3, t1));
            }
            ra_return_reg(ra, t1);
            ra_return_reg(ra, t0);
            ra_return_reg(ra, px);
            ra_set_chan_reg(ra, i, 0, s0.rd);
            ra_set_chan_reg(ra, i, 1, r1);
            ra_set_chan_reg(ra, i, 2, r2);
            ra_set_chan_reg(ra, i, 3, r3);
        } break;
        case op_load_fp16x4_planar: {
            struct ra_step s0 = ra_step_alloc(ra, sl, ns, i);
            int8_t r1 = ra_alloc(ra, sl, ns);
            int8_t r2 = ra_alloc(ra, sl, ns);
            int8_t r3 = ra_alloc(ra, sl, ns);
            int    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            int8_t px = ra_alloc(ra, sl, ns);
            {
                int const sz_off = p * (int)sizeof(umbra_buf)
                                 + (int)__builtin_offsetof(umbra_buf, sz);
                put(c, LDR_xi(XT, XBUF, sz_off / 8));
                put(c, LSR_xi(XT, XT, 2));
            }
            if (scalar) {
                put(c, LDR_hx(px, XP, XI));
                put(c, FCVTL_4s(s0.rd, px));
                put(c, DUP_4s_lane(s0.rd, s0.rd, 0));
                put(c, ADD_xr(XP, XP, XT));
                put(c, LDR_hx(px, XP, XI));
                put(c, FCVTL_4s(r1, px));
                put(c, DUP_4s_lane(r1, r1, 0));
                put(c, ADD_xr(XP, XP, XT));
                put(c, LDR_hx(px, XP, XI));
                put(c, FCVTL_4s(r2, px));
                put(c, DUP_4s_lane(r2, r2, 0));
                put(c, ADD_xr(XP, XP, XT));
                put(c, LDR_hx(px, XP, XI));
                put(c, FCVTL_4s(r3, px));
                put(c, DUP_4s_lane(r3, r3, 0));
            } else {
                put(c, LDR_d(s0.rd, XP, XH));
                put(c, FCVTL_4s(s0.rd, s0.rd));
                put(c, ADD_xr(XP, XP, XT));
                put(c, LDR_d(r1, XP, XH));
                put(c, FCVTL_4s(r1, r1));
                put(c, ADD_xr(XP, XP, XT));
                put(c, LDR_d(r2, XP, XH));
                put(c, FCVTL_4s(r2, r2));
                put(c, ADD_xr(XP, XP, XT));
                put(c, LDR_d(r3, XP, XH));
                put(c, FCVTL_4s(r3, r3));
            }
            last_ptr = -1;
            ra_return_reg(ra, px);
            ra_set_chan_reg(ra, i, 0, s0.rd);
            ra_set_chan_reg(ra, i, 1, r1);
            ra_set_chan_reg(ra, i, 2, r2);
            ra_set_chan_reg(ra, i, 3, r3);
        } break;
        case op_store_fp16x4: {
            int8_t rr   = ra_ensure_chan(ra, sl, ns, (int)inst->x.id, (int)inst->x.chan);
            int8_t rg   = ra_ensure_chan(ra, sl, ns, (int)inst->y.id, (int)inst->y.chan);
            int8_t rb_  = ra_ensure_chan(ra, sl, ns, (int)inst->z.id, (int)inst->z.chan);
            int8_t ra_v = ra_ensure_chan(ra, sl, ns, (int)inst->w.id, (int)inst->w.chan);
            int    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            int8_t px = ra_alloc(ra, sl, ns);
            int8_t t  = ra_alloc(ra, sl, ns);
            int8_t z  = ra_alloc(ra, sl, ns);
            int8_t one = ra_alloc(ra, sl, ns);
            int8_t scale = ra_alloc(ra, sl, ns);
            if (scalar) {
                put(c, ORR_16b(t, rr, rr));
                put(c, INS_elem_s(t, 1, rg,  0));
                put(c, INS_elem_s(t, 2, rb_, 0));
                put(c, INS_elem_s(t, 3, ra_v, 0));
                put(c, FCVTN_4h(px, t));
                put(c, LSL_xi(XT, XI, 3));
                put(c, ADD_xr(XT, XP, XT));
                put(c, STR_di(px, XT, 0));
            } else {
                put(c, FCVTN_4h(px, rr));
                put(c, FCVTN_4h(t, rg));
                put(c, FCVTN_4h(z, rb_));
                put(c, FCVTN_4h(one, ra_v));
                put(c, ZIP1_8h(scale, px, t));
                put(c, ZIP1_8h(px, z, one));
                put(c, ZIP1_4s(t, scale, px));
                put(c, ZIP2_4s(z, scale, px));
                put(c, LSL_xi(XT, XI, 3));
                put(c, ADD_xr(XT, XP, XT));
                put(c, STR_qi(t, XT, 0));
                put(c, STR_qi(z, XT, 1));
            }
            ra_return_reg(ra, scale);
            ra_return_reg(ra, one);
            ra_return_reg(ra, z);
            ra_return_reg(ra, t);
            ra_return_reg(ra, px);
            free_chan(ra, inst->x, i);
            free_chan(ra, inst->y, i);
            free_chan(ra, inst->z, i);
            free_chan(ra, inst->w, i);
        } break;
        case op_store_fp16x4_planar: {
            int8_t rr   = ra_ensure_chan(ra, sl, ns, (int)inst->x.id, (int)inst->x.chan);
            int8_t rg   = ra_ensure_chan(ra, sl, ns, (int)inst->y.id, (int)inst->y.chan);
            int8_t rb_  = ra_ensure_chan(ra, sl, ns, (int)inst->z.id, (int)inst->z.chan);
            int8_t ra_v = ra_ensure_chan(ra, sl, ns, (int)inst->w.id, (int)inst->w.chan);
            int    p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            int8_t px = ra_alloc(ra, sl, ns);
            {
                int const sz_off = p * (int)sizeof(umbra_buf)
                                 + (int)__builtin_offsetof(umbra_buf, sz);
                put(c, LDR_xi(XT, XBUF, sz_off / 8));
                put(c, LSR_xi(XT, XT, 2));
            }
            if (scalar) {
                put(c, FCVTN_4h(px, rr));  put(c, STR_hx(px, XP, XI));
                put(c, ADD_xr(XP, XP, XT));
                put(c, FCVTN_4h(px, rg));  put(c, STR_hx(px, XP, XI));
                put(c, ADD_xr(XP, XP, XT));
                put(c, FCVTN_4h(px, rb_)); put(c, STR_hx(px, XP, XI));
                put(c, ADD_xr(XP, XP, XT));
                put(c, FCVTN_4h(px, ra_v)); put(c, STR_hx(px, XP, XI));
            } else {
                put(c, FCVTN_4h(px, rr));  put(c, STR_d(px, XP, XH));
                put(c, ADD_xr(XP, XP, XT));
                put(c, FCVTN_4h(px, rg));  put(c, STR_d(px, XP, XH));
                put(c, ADD_xr(XP, XP, XT));
                put(c, FCVTN_4h(px, rb_)); put(c, STR_d(px, XP, XH));
                put(c, ADD_xr(XP, XP, XT));
                put(c, FCVTN_4h(px, ra_v)); put(c, STR_d(px, XP, XH));
            }
            last_ptr = -1;
            ra_return_reg(ra, px);
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
                put(c, STR_hx(ry, XP, XI));
            } else {
                put(c, STR_d(ry, XP, XH));
            }
            free_chan(ra, inst->y, i);
        } break;

        case op_f32_from_f16: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            put(c, FCVTL_4s(s.rd, s.rx));
        } break;

        case op_f16_from_f32: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            put(c, FCVTN_4h(s.rd, s.rx));
        } break;

        case op_i32_from_s16: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            put(c, SXTL_4s(s.rd, s.rx));
        } break;

        case op_i32_from_u16: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            put(c, UXTL_4s(s.rd, s.rx));
        } break;

        case op_i16_from_i32: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            put(c, XTN_4h(s.rd, s.rx));
        } break;

        case op_imm_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            arm64_pool_load(c, &jc->pool, s.rd, (uint32_t)inst->imm);
        } break;

        case op_add_f32:
        case op_sub_f32:
        case op_mul_f32:
        case op_div_f32:
        case op_min_f32:
        case op_max_f32:
        case op_sqrt_f32:
        case op_abs_f32:
        case op_neg_f32:
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
            emit_alu_reg(c, inst->op, s.rd, s.rx, s.ry, s.rz, inst->imm, s.scratch);
            if (s.scratch >= 0) { ra_return_reg(ra, s.scratch); }
        } break;

        case op_shl_i32_imm:
        case op_shr_u32_imm:
        case op_shr_s32_imm: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            emit_alu_reg(c, inst->op, s.rd, s.rx, 0, 0, inst->imm, -1);
        } break;

        case op_pack: {
            struct ra_step s = ra_step_alu(ra, sl, ns, inst, i, scalar, 1);
            emit_alu_reg(c, inst->op, s.rd, s.rx, s.ry, 0, inst->imm, s.scratch);
            if (s.scratch >= 0) { ra_return_reg(ra, s.scratch); }
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
                o == op_add_f32_imm ? FADD_4s(s.rd, s.rx, ir)  :
                o == op_sub_f32_imm ? FSUB_4s(s.rd, s.rx, ir)  :
                o == op_mul_f32_imm ? FMUL_4s(s.rd, s.rx, ir)  :
                o == op_div_f32_imm ? FDIV_4s(s.rd, s.rx, ir)  :
                o == op_min_f32_imm ? FMINNM_4s(s.rd, s.rx, ir):
                o == op_max_f32_imm ? FMAXNM_4s(s.rd, s.rx, ir):
                o == op_add_i32_imm ? ADD_4s(s.rd, s.rx, ir)   :
                o == op_sub_i32_imm ? SUB_4s(s.rd, s.rx, ir)   :
                o == op_mul_i32_imm ? MUL_4s(s.rd, s.rx, ir)   :
                o == op_and_32_imm     ? AND_16b(s.rd, s.rx, ir)  :
                o == op_or_32_imm   ? ORR_16b(s.rd, s.rx, ir)  :
                o == op_xor_32_imm  ? EOR_16b(s.rd, s.rx, ir)  :
                o == op_eq_f32_imm  ? FCMEQ_4s(s.rd, ir, s.rx) :
                o == op_lt_f32_imm  ? FCMGT_4s(s.rd, ir, s.rx) :
                o == op_le_f32_imm  ? FCMGE_4s(s.rd, ir, s.rx) :
                o == op_eq_i32_imm  ? CMEQ_4s(s.rd, ir, s.rx)  :
                o == op_lt_s32_imm  ? CMGT_4s(s.rd, ir, s.rx)  :
                                      CMGE_4s(s.rd, ir, s.rx);
            put(c, w);
        } break;
        }
    }
#undef lu
}

#if __clang__
__attribute__((no_sanitize("function")))
#endif
static void umbra_jit_run(struct umbra_jit *j, int l, int t, int r, int b, umbra_buf buf[]) {
    assert(j);
    j->entry(l, t, r, b, buf);
}
static void umbra_jit_free(struct umbra_jit *j) {
    assert(j);
    munmap(j->code, j->code_size);
    free(j);
}

static void umbra_dump_jit_mca(struct umbra_jit const *j, FILE *f) {
    assert(j);
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

static void run_jit(struct umbra_program *prog, int l, int t, int r, int b, umbra_buf buf[]) {
    umbra_jit_run((struct umbra_jit*)prog, l, t, r, b, buf);
}
static void dump_jit(struct umbra_program const *prog, FILE *f) { umbra_dump_jit_mca((struct umbra_jit const*)prog, f); }
static void free_jit(struct umbra_program *prog) { umbra_jit_free((struct umbra_jit*)prog); }
static struct umbra_program *compile_jit(struct umbra_backend           *be,
                                         struct umbra_basic_block const *bb) {
    struct umbra_jit *const j = umbra_jit(bb);
    assert(j);
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
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

_Static_assert(sizeof(umbra_buf) == 32, "");
_Static_assert(offsetof(umbra_buf, ptr)       ==  0, "");
_Static_assert(offsetof(umbra_buf, sz)        ==  8, "");
_Static_assert(offsetof(umbra_buf, row_bytes) == 16, "");

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
    if (p < 0) {
        mov_ri(c, XM, 0x7fffffff);
    } else {
        mov_load(c, XM, XBUF, p * (int)sizeof(umbra_buf) + (int)__builtin_offsetof(umbra_buf, sz));
        if (elem_shift) {
            shr_ri(c, XM, (uint8_t)elem_shift);
        }
    }
}
static int load_ptr_x86(Buf *c, int p, int *last_ptr) {
    if (*last_ptr != p) {
        *last_ptr = p;
        mov_load(c, R11, XBUF, p * (int)sizeof(umbra_buf));
        mov_load(c, RAX, XBUF, p * (int)sizeof(umbra_buf) + (int)__builtin_offsetof(umbra_buf, row_bytes));
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
    case op_pack:
    case op_and_32_imm:
    case op_add_f32_imm:
    case op_sub_f32_imm:
    case op_mul_f32_imm:
    case op_div_f32_imm:
    case op_min_f32_imm:
    case op_max_f32_imm:
    case op_add_i32_imm:
    default: assert(0); break;
    }
}

static void emit_ops(Buf *c, struct umbra_basic_block const *bb, int from, int to,
                     int *sl, int *ns, struct ra *ra, _Bool scalar, int *deref_gpr,
                     int *deref_rb_gpr, struct jit_ctx *jc);

static int resolve_ptr_x86(Buf *c, int p, int *last_ptr, int const *deref_gpr,
                           int const *deref_rb_gpr) {
    if (p < 0) {
        int gpr = deref_gpr[~p];
        int rb  = deref_rb_gpr[~p];
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
    void (*entry)(int, int, int, int, umbra_buf *);
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

    for (int i = 0; i < bb->insts; i++) { ra_free_reg(ra, i); }
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
    if (mem == MAP_FAILED) {
        free(c.buf);
        return 0;
    }
    mprotect((char *)mem + alloc, pg, PROT_NONE);
    __builtin_memcpy(mem, c.buf, code_sz);
    if (mprotect(mem, alloc, PROT_READ | PROT_EXEC) != 0) {
        munmap(mem, alloc);
        free(c.buf);
        return 0;
    }
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
            void (*fn)(int, int, int, int, umbra_buf *);
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
            vex(c, 1, 1, 0, 0, s.rd, 0, XI, 0x6e);
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
            vex(c, 1, 1, 0, 0, s.rd, 0, XY, 0x6e);  // VMOVD xmm, XY
            if (!scalar) {
                vbroadcastss(c, s.rd, s.rd);
            }
        } break;

        case op_load_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int            p = inst->ptr;
            int            base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            if (scalar) {
                vex_mem(c, 1, 1, 0, 0, s.rd, 0, 0x6e, base, XI, 4, 0);
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
                vex(c, 1, 1, 0, 0, s.rd, 0, RAX, 0x6e);
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
                vex_mem(c, 1, 1, 0, 0, ry, 0, 0x7e, base, XI, 4, 0);
            } else {
                vmov_store(c, 1, ry, base, XI, 4, 0);
            }
            free_chan(ra, inst->y, i);
        } break;

        case op_load_fp16x4: {
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
                // Load 8 bytes (1 pixel, 4xfp16) via VMOVQ.
                // VMOVQ xmm, m64: VEX.128 F3.0F 7E /r
                vex_mem(c, 2, 1, 0, 0, px, 0, 0x7e, base, XI, 8, 0);
                // VCVTPH2PS: convert 4 x fp16 -> 4 x fp32
                vcvtph2ps(c, px, px);
                // Broadcast each lane.
                vex_rrr(c, 1, 1, 1, 0x70, s0.rd, 0, px); emit1(c, 0x00);
                vex_rrr(c, 1, 1, 1, 0x70, r1,    0, px); emit1(c, 0x55);
                vex_rrr(c, 1, 1, 1, 0x70, r2,    0, px); emit1(c, 0xAA);
                vex_rrr(c, 1, 1, 1, 0x70, r3,    0, px); emit1(c, 0xFF);
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

                // Low half (pixels 0-3):
                int off_r = pool_add(&jc->pool, shuf_r, 32);
                {
                    int ref = vex_rip(c, 1, 2, 0, 1, s0.rd, t0, 0x00);
                    pool_ref_at(&jc->pool, off_r, ref, 0);
                }
                // VPERMQ imm=0x08: [q0,q2,q0,q0] = [R0R1, R2R3, ...]
                vex(c, 1, 3, 1, 1, s0.rd, 0, s0.rd, 0x00); emit1(c, 0x08);

                // High half (pixels 4-7):
                vmov_load(c, 1, t1, base, XI, 8, 32);
                {
                    int ref = vex_rip(c, 1, 2, 0, 1, t0, t1, 0x00);
                    pool_ref_at(&jc->pool, off_r, ref, 0);
                }
                vex(c, 1, 3, 1, 1, t0, 0, t0, 0x00); emit1(c, 0x08);

                // Combine: VPUNPCKLQDQ to merge low 64 bits.
                vex_rrr(c, 1, 1, 0, 0x6c, s0.rd, s0.rd, t0);
                vcvtph2ps(c, s0.rd, s0.rd);

                // Repeat for G, B, A. Reload low half for each.
                int off_g = pool_add(&jc->pool, shuf_g, 32);
                int off_b = pool_add(&jc->pool, shuf_b, 32);
                int off_a = pool_add(&jc->pool, shuf_a, 32);

                // G: low half
                vmov_load(c, 1, t0, base, XI, 8, 0);
                {
                    int ref = vex_rip(c, 1, 2, 0, 1, r1, t0, 0x00);
                    pool_ref_at(&jc->pool, off_g, ref, 0);
                }
                vex(c, 1, 3, 1, 1, r1, 0, r1, 0x00); emit1(c, 0x08);
                // G: high half
                {
                    int ref = vex_rip(c, 1, 2, 0, 1, t0, t1, 0x00);
                    pool_ref_at(&jc->pool, off_g, ref, 0);
                }
                vex(c, 1, 3, 1, 1, t0, 0, t0, 0x00); emit1(c, 0x08);
                vex_rrr(c, 1, 1, 0, 0x6c, r1, r1, t0);
                vcvtph2ps(c, r1, r1);

                // B: low half
                vmov_load(c, 1, t0, base, XI, 8, 0);
                {
                    int ref = vex_rip(c, 1, 2, 0, 1, r2, t0, 0x00);
                    pool_ref_at(&jc->pool, off_b, ref, 0);
                }
                vex(c, 1, 3, 1, 1, r2, 0, r2, 0x00); emit1(c, 0x08);
                // B: high half
                {
                    int ref = vex_rip(c, 1, 2, 0, 1, t0, t1, 0x00);
                    pool_ref_at(&jc->pool, off_b, ref, 0);
                }
                vex(c, 1, 3, 1, 1, t0, 0, t0, 0x00); emit1(c, 0x08);
                vex_rrr(c, 1, 1, 0, 0x6c, r2, r2, t0);
                vcvtph2ps(c, r2, r2);

                // A: low half
                vmov_load(c, 1, t0, base, XI, 8, 0);
                {
                    int ref = vex_rip(c, 1, 2, 0, 1, r3, t0, 0x00);
                    pool_ref_at(&jc->pool, off_a, ref, 0);
                }
                vex(c, 1, 3, 1, 1, r3, 0, r3, 0x00); emit1(c, 0x08);
                // A: high half
                {
                    int ref = vex_rip(c, 1, 2, 0, 1, t0, t1, 0x00);
                    pool_ref_at(&jc->pool, off_a, ref, 0);
                }
                vex(c, 1, 3, 1, 1, t0, 0, t0, 0x00); emit1(c, 0x08);
                vex_rrr(c, 1, 1, 0, 0x6c, r3, r3, t0);
                vcvtph2ps(c, r3, r3);
            }

            ra_return_reg(ra, t1);
            ra_return_reg(ra, t0);
            ra_return_reg(ra, px);
            ra_set_chan_reg(ra, i, 0, s0.rd);
            ra_set_chan_reg(ra, i, 1, r1);
            ra_set_chan_reg(ra, i, 2, r2);
            ra_set_chan_reg(ra, i, 3, r3);
        } break;
        case op_load_fp16x4_planar: {
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
                int const sz_off = p * (int)sizeof(umbra_buf)
                                 + (int)__builtin_offsetof(umbra_buf, sz);
                mov_rr(c, R11, base);
                mov_load(c, RBX, XBUF, sz_off);
                shr_ri(c, RBX, 2);
                if (scalar) {
                    // Plane 0 (R): MOVZX eax, word [R11 + XI*2]
                    {
                        uint8_t rex = 0x40;
                        if (XI >= 8)   { rex |= 0x02; }
                        if (R11 >= 8)  { rex |= 0x01; }
                        if (rex != 0x40) { emit1(c, rex); }
                        emit1(c, 0x0f); emit1(c, 0xb7);
                        emit1(c, (uint8_t)(((RAX & 7) << 3) | 4));
                        emit1(c, (uint8_t)((1 << 6) | ((XI & 7) << 3) | (R11 & 7)));
                    }
                    vex(c, 1, 1, 0, 0, px, 0, RAX, 0x6e);
                    vcvtph2ps(c, s0.rd, px);
                    vbroadcastss(c, s0.rd, s0.rd);

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
                    vex(c, 1, 1, 0, 0, px, 0, RAX, 0x6e);
                    vcvtph2ps(c, r1, px);
                    vbroadcastss(c, r1, r1);

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
                    vex(c, 1, 1, 0, 0, px, 0, RAX, 0x6e);
                    vcvtph2ps(c, r2, px);
                    vbroadcastss(c, r2, r2);

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
                    vex(c, 1, 1, 0, 0, px, 0, RAX, 0x6e);
                    vcvtph2ps(c, r3, px);
                    vbroadcastss(c, r3, r3);
                } else {
                    // Plane 0 (R): load 8 x f16 (16 bytes), convert to 8 x f32
                    vmov_load(c, 0, px, R11, XI, 2, 0);
                    vcvtph2ps(c, s0.rd, px);

                    // Plane 1 (G): advance R11 by plane_stride
                    rex_w(c, RBX, R11);
                    emit1(c, 0x01);
                    emit1(c, (uint8_t)(0xc0 | ((RBX & 7) << 3) | (R11 & 7)));
                    vmov_load(c, 0, px, R11, XI, 2, 0);
                    vcvtph2ps(c, r1, px);

                    // Plane 2 (B): advance R11 by plane_stride
                    rex_w(c, RBX, R11);
                    emit1(c, 0x01);
                    emit1(c, (uint8_t)(0xc0 | ((RBX & 7) << 3) | (R11 & 7)));
                    vmov_load(c, 0, px, R11, XI, 2, 0);
                    vcvtph2ps(c, r2, px);

                    // Plane 3 (A): advance R11 by plane_stride
                    rex_w(c, RBX, R11);
                    emit1(c, 0x01);
                    emit1(c, (uint8_t)(0xc0 | ((RBX & 7) << 3) | (R11 & 7)));
                    vmov_load(c, 0, px, R11, XI, 2, 0);
                    vcvtph2ps(c, r3, px);
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
        case op_store_fp16x4: {
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
            int8_t one   = ra_alloc(ra, sl, ns);
            if (scalar) {
                // Pack 4 fp32 channels into one xmm, then VCVTPS2PH.
                // VINSERTPS px, rr, rr, 0x00 — copy rr[0] to px[0]
                vmovaps(c, px, rr);
                // VINSERTPS px, px, rg, 0x10 — rg[0] -> px[1]
                vex(c, 1, 3, 0, 0, px, px, rg, 0x21); emit1(c, 0x10);
                // VINSERTPS px, px, rb_, 0x20 — rb_[0] -> px[2]
                vex(c, 1, 3, 0, 0, px, px, rb_, 0x21); emit1(c, 0x20);
                // VINSERTPS px, px, ra_v, 0x30 — ra_v[0] -> px[3]
                vex(c, 1, 3, 0, 0, px, px, ra_v, 0x21); emit1(c, 0x30);
                // VCVTPS2PH px, px, 4 (round to nearest)
                vcvtps2ph(c, px, px, 4);
                // VMOVQ [base + XI*8], px (store 8 bytes)
                vex_mem(c, 1, 1, 0, 0, px, 0, 0xd6, base, XI, 8, 0);
            } else {
                // Convert each channel from fp32 (8 x YMM) to fp16 (8 x XMM).
                vcvtps2ph(c, px, rr, 4);     // px  = [R0..R7] as fp16 in XMM
                vcvtps2ph(c, t, rg, 4);      // t   = [G0..G7] as fp16
                vcvtps2ph(c, z, rb_, 4);     // z   = [B0..B7] as fp16
                vcvtps2ph(c, one, ra_v, 4);  // one = [A0..A7] as fp16
                // Interleave to pixel order. Process low 4 then high 4 pixels.
                // Low 4 (VPUNPCKLWD takes low 4 words from each XMM):
                vex_rrr(c, 1, 1, 0, 0x61, scale, px, t);  // scale=[R0,G0,R1,G1,R2,G2,R3,G3]
                vex_rrr(c, 1, 1, 0, 0x61, px, z, one);    // px=[B0,A0,B1,A1,B2,A2,B3,A3]
                // Interleave at 32-bit to form pixel pairs:
                vex_rrr(c, 1, 1, 0, 0x62, t, scale, px);  // t=[R0G0,B0A0,R1G1,B1A1] (pixels 0,1)
                vex_rrr(c, 1, 1, 0, 0x6a, z, scale, px);  // z=[R2G2,B2A2,R3G3,B3A3] (pixels 2,3)
                vex(c, 1, 3, 0, 1, t, t, z, 0x38); emit1(c, 1);
                // t = [pixels 0-1 | pixels 2-3] (256 bits = 32 bytes)
                vmov_store(c, 1, t, base, XI, 8, 0);
                // High 4 (VPUNPCKHWD takes high 4 words):
                // Re-convert all channels (registers were clobbered above).
                vcvtps2ph(c, px, rr, 4);
                vcvtps2ph(c, t, rg, 4);
                vcvtps2ph(c, z, rb_, 4);
                vcvtps2ph(c, one, ra_v, 4);
                vex_rrr(c, 1, 1, 0, 0x69, scale, px, t);  // VPUNPCKHWD scale=[R4,G4,R5,G5,R6,G6,R7,G7]
                vex_rrr(c, 1, 1, 0, 0x69, px, z, one);    // VPUNPCKHWD px=[B4,A4,B5,A5,B6,A6,B7,A7]
                vex_rrr(c, 1, 1, 0, 0x62, t, scale, px);  // VPUNPCKLDQ t=[R4G4,B4A4,R5G5,B5A5]
                vex_rrr(c, 1, 1, 0, 0x6a, z, scale, px);  // VPUNPCKHDQ z=[R6G6,B6A6,R7G7,B7A7]
                vex(c, 1, 3, 0, 1, t, t, z, 0x38); emit1(c, 1);
                // t = [pixels 4-5 | pixels 6-7] (256 bits)
                vmov_store(c, 1, t, base, XI, 8, 32);
            }
            ra_return_reg(ra, one);
            ra_return_reg(ra, z);
            ra_return_reg(ra, t);
            ra_return_reg(ra, px);
            ra_return_reg(ra, scale);
            free_chan(ra, inst->x, i);
            free_chan(ra, inst->y, i);
            free_chan(ra, inst->z, i);
            free_chan(ra, inst->w, i);
        } break;
        case op_store_fp16x4_planar: {
            int8_t rr   = ra_ensure_chan(ra, sl, ns, (int)inst->x.id, (int)inst->x.chan);
            int8_t rg   = ra_ensure_chan(ra, sl, ns, (int)inst->y.id, (int)inst->y.chan);
            int8_t rb_  = ra_ensure_chan(ra, sl, ns, (int)inst->z.id, (int)inst->z.chan);
            int8_t ra_v = ra_ensure_chan(ra, sl, ns, (int)inst->w.id, (int)inst->w.chan);
            int    p = inst->ptr;
            int    base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            int8_t px = ra_alloc(ra, sl, ns);
            {
                int const sz_off = p * (int)sizeof(umbra_buf)
                                 + (int)__builtin_offsetof(umbra_buf, sz);
                mov_rr(c, R11, base);
                mov_load(c, RBX, XBUF, sz_off);
                shr_ri(c, RBX, 2);
                if (scalar) {
                    // Plane 0 (R): VCVTPS2PH, store 16-bit
                    vcvtps2ph(c, px, rr, 4);
                    // VMOVD eax, px
                    vex(c, 1, 1, 0, 0, px, 0, RAX, 0x7e);
                    // MOV word [R11 + XI*2], ax
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
                    vcvtps2ph(c, px, rg, 4);
                    vex(c, 1, 1, 0, 0, px, 0, RAX, 0x7e);
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
                    vcvtps2ph(c, px, rb_, 4);
                    vex(c, 1, 1, 0, 0, px, 0, RAX, 0x7e);
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
                    vcvtps2ph(c, px, ra_v, 4);
                    vex(c, 1, 1, 0, 0, px, 0, RAX, 0x7e);
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
                    // Plane 0 (R): VCVTPS2PH, store 128-bit (8 x f16)
                    vcvtps2ph(c, px, rr, 4);
                    vmov_store(c, 0, px, R11, XI, 2, 0);

                    // Plane 1 (G): advance R11 by plane_stride
                    rex_w(c, RBX, R11);
                    emit1(c, 0x01);
                    emit1(c, (uint8_t)(0xc0 | ((RBX & 7) << 3) | (R11 & 7)));
                    vcvtps2ph(c, px, rg, 4);
                    vmov_store(c, 0, px, R11, XI, 2, 0);

                    // Plane 2 (B): advance R11 by plane_stride
                    rex_w(c, RBX, R11);
                    emit1(c, 0x01);
                    emit1(c, (uint8_t)(0xc0 | ((RBX & 7) << 3) | (R11 & 7)));
                    vcvtps2ph(c, px, rb_, 4);
                    vmov_store(c, 0, px, R11, XI, 2, 0);

                    // Plane 3 (A): advance R11 by plane_stride
                    rex_w(c, RBX, R11);
                    emit1(c, 0x01);
                    emit1(c, (uint8_t)(0xc0 | ((RBX & 7) << 3) | (R11 & 7)));
                    vcvtps2ph(c, px, ra_v, 4);
                    vmov_store(c, 0, px, R11, XI, 2, 0);
                }
                last_ptr = -1;
            }
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
                vex(c, 1, 1, 0, 0, ry, 0, RAX, 0x7e);
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
                int     disp = inst->imm * 4;
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
            vex(c, 1, 1, 0, 0, rx, 0, RAX, 0x7e);
            free_chan(ra, inst->x, i);
            load_count_x86(c, p, 2);
            vpxor(c, scalar ? 0 : 1, s.rd, s.rd, s.rd);
            cmp_rr(c, RAX, XM);
            int skip = jcc(c, 0x03);
            vex_mem(c, 1, 1, 0, 0, s.rd, 0, 0x6e, base, RAX, 4, 0);
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
                vex(c, 1, 1, 0, 0, rx, 0, RAX, 0x7e);
                free_chan(ra, inst->x, i);
                load_count_x86(c, p, 2);
                vpxor(c, 0, s.rd, s.rd, s.rd);
                cmp_rr(c, RAX, XM);
                int skip = jcc(c, 0x03);
                vex_mem(c, 1, 1, 0, 0, s.rd, 0, 0x6e, base, RAX, 4, 0);
                patch_jcc(c, skip);
            } else {
                int8_t mask = ra_alloc(ra, sl, ns);
                int8_t cnt = ra_alloc(ra, sl, ns);
                load_count_x86(c, p, 2);
                // Build in-bounds mask: (ix >= 0) AND (ix < count)
                vpxor(c, 1, mask, mask, mask);
                vpcmpgtd(c, mask, mask, rx);          // mask = (0 > ix) = neg lanes
                vex(c, 1, 1, 0, 0, cnt, 0, XM, 0x6e);
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

        case op_gather_16: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int8_t         rx = ra_ensure(ra, sl, ns, (int)inst->x.id);
            int            p = inst->ptr;
            int            base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            load_count_x86(c, p, 1);
            if (scalar) {
                vex(c, 1, 1, 0, 0, rx, 0, RAX, 0x7e);
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
                vex(c, 1, 1, 0, 0, s.rd, 0, RAX, 0x6e);
                patch_jcc(c, skip);
            } else {
                int8_t hi_idx = ra_alloc(ra, sl, ns);
                vextracti128(c, hi_idx, rx, 1);
                vpxor(c, 0, s.rd, s.rd, s.rd);
                for (int k = 0; k < 8; k++) {
                    int src = (k < 4) ? rx : hi_idx;
                    int lane = k & 3;
                    if (lane == 0) {
                        vex(c, 1, 1, 0, 0, src, 0, RAX, 0x7e);
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
        case op_neg_f32: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            uint32_t       mask[8] = {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u,
                                      0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u};
            int            off = pool_add(&jc->pool, mask, 32);
            int            pos = vex_rip(c, 0, 1, 0, 1, s.rd, s.rx, 0x57);
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

        case op_pack: {
            struct ra_step s = ra_step_alu(ra, sl, ns, inst, i, scalar, 1);
            vpslld_i(c, s.scratch, s.ry, (uint8_t)inst->imm);
            vpor(c, 1, s.rd, s.scratch, s.rx);
            if (s.scratch >= 0) {
                ra_return_reg(ra, s.scratch);
            }
        } break;
        }
    }
#undef lu
}

#if __clang__
__attribute__((no_sanitize("function")))
#endif
static void umbra_jit_run(struct umbra_jit *j, int l, int t, int r, int b, umbra_buf buf[]) {
    assert(j);
    j->entry(l, t, r, b, buf);
}

static void umbra_jit_free(struct umbra_jit *j) {
    assert(j);
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
    assert(j);
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

static void run_jit(struct umbra_program *prog, int l, int t, int r, int b, umbra_buf buf[]) {
    umbra_jit_run((struct umbra_jit*)prog, l, t, r, b, buf);
}
static void dump_jit(struct umbra_program const *prog, FILE *f) { umbra_dump_jit_mca((struct umbra_jit const*)prog, f); }
static void free_jit(struct umbra_program *prog) { umbra_jit_free((struct umbra_jit*)prog); }
static struct umbra_program *compile_jit(struct umbra_backend           *be,
                                         struct umbra_basic_block const *bb) {
    struct umbra_jit *const j = umbra_jit(bb);
    assert(j);
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
