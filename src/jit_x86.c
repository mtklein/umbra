#include "jit.h"
#if defined(__AVX2__)
#include "assume.h"
#include "ra.h"

#include "asm_x86.h"

typedef struct asm_x86 Buf;

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

_Static_assert(sizeof(struct umbra_buf) == 24, "");
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
    int32_t rel = (int32_t)(c->size - (size_t)(fixup + 4));
    __builtin_memcpy(c->byte + fixup, &rel, 4);
}

static void load_count_x86(Buf *c, ptr p, int elem_shift) {
    if (p.deref) {
        mov_ri(c, XM, 0x7fffffff);
    } else {
        mov_load(c, XM, XBUF, p.bits * (int)sizeof(struct umbra_buf)
                 + (int)__builtin_offsetof(struct umbra_buf, sz));
        if (elem_shift) {
            shr_ri(c, XM, (uint8_t)elem_shift);
        }
    }
}
static int load_ptr_x86(Buf *c, ptr p, int *last_ptr) {
    if (*last_ptr != p.bits) {
        *last_ptr = p.bits;
        mov_load(c, R11, XBUF,
                 p.bits * (int)sizeof(struct umbra_buf));
        mov_load(c, RAX, XBUF,
                 p.bits * (int)sizeof(struct umbra_buf)
                 + (int)__builtin_offsetof(struct umbra_buf, row_bytes));
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
    int const shr = v ? __builtin_clz(v) : 0,
              shl = v ? __builtin_ctz(v) : 0;
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
        int const off = pool_add(p, &v, 4),
                  pos = vex_rip(c, 1, 2, 0, 1, d, 0, 0x18);
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

static int resolve_ptr_x86(Buf *c, ptr p, int *last_ptr, int const *deref_gpr,
                           int const *deref_rb_gpr) {
    if (p.deref) {
        int const gpr = deref_gpr[p.ix],
                  rb  = deref_rb_gpr[p.ix];
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

struct jit_program *jit_program(struct jit_backend *be,
                                           struct umbra_basic_block const *bb) {
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

    int const stack_patch = (int)c.size;
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

    int const loop_top = (int)c.size;

    // remaining = row_end (RDI) - XI
    mov_rr(&c, R11, RDI);
    rex_w(&c, XI, R11);
    emit1(&c, 0x29);
    emit1(&c, (uint8_t)(0xc0 | ((XI & 7) << 3) | (R11 & 7)));

    cmp_ri(&c, R11, 8);
    int const br_tail = jcc(&c, 0x0c);

    int const loop_body_start = (int)c.size;
    emit_ops(&c, bb, bb->preamble, bb->insts, sl, &ns, ra, 0, deref_gpr, deref_rb_gpr, &jc);

    ra_end_loop(ra, sl);

    int const loop_body_end = (int)c.size;

    add_ri(&c, XI, 8);
    {
        int     const j   = jmp(&c);
        int32_t const rel = (int32_t)(loop_top - (j + 4));
        __builtin_memcpy(c.byte + j, &rel, 4);
    }

    int const tail_top = (int)c.size;
    patch_jcc(&c, br_tail);

    cmp_rr(&c, XI, RDI);                       // xi >= row_end?
    int const br_row_done = jcc(&c, 0x0d);     // JGE row_done

    ra_reset_pool(ra);
    for (int i = 0; i < bb->insts; i++) { sl[i] = -1; }

    emit_ops(&c, bb, 0, bb->preamble, sl, &ns, ra, 0, deref_gpr, deref_rb_gpr, &jc);
    emit_ops(&c, bb, bb->preamble, bb->insts, sl, &ns, ra, 1, deref_gpr, deref_rb_gpr, &jc);

    add_ri(&c, XI, 1);
    {
        int     const j   = jmp(&c);
        int32_t const rel = (int32_t)(tail_top - (j + 4));
        __builtin_memcpy(c.byte + j, &rel, 4);
    }

    // row_done:
    patch_jcc(&c, br_row_done);

    add_ri(&c, XY, 1);
    mov_rr(&c, XI, XWIDTH);
    cmp_rr(&c, XY, XH_X86);
    int const br_more_rows = jcc(&c, 0x0c);    // JL -> more rows
    {
        int32_t const rel = (int32_t)(loop_top - (br_more_rows + 4));
        __builtin_memcpy(c.byte + br_more_rows, &rel, 4);
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
        c.byte[pos++] = 0x48;
        c.byte[pos++] = 0x81;
        c.byte[pos++] = (uint8_t)(0xc0 | (5 << 3) | (RSP & 7));
        int32_t const sz = ns * 32;
        __builtin_memcpy(c.byte + pos, &sz, 4);
    }

    int const pool_start = (int)c.size;
    for (int i = 0; i < jc.pool.nbytes; i++) {
        emit1(&c, jc.pool.data[i]);
    }
    for (int i = 0; i < jc.pool.nrefs; i++) {
        struct pool_ref *r = &jc.pool.refs[i];
        int     const entry_off = pool_start + r->data_off;
        int32_t const rel       = (int32_t)(entry_off - (r->code_pos + 4 + r->extra));
        __builtin_memcpy(c.byte + r->code_pos, &rel, 4);
    }
    pool_free(&jc.pool);

    ra_destroy(ra);
    free(sl);
    free(deref_gpr);
    free(deref_rb_gpr);

    size_t const code_sz = c.size,
                 pg      = (size_t)sysconf(_SC_PAGESIZE),
                 alloc   = (code_sz + pg - 1) & ~(pg - 1);

    void  *buf_mem;
    size_t buf_size;
    acquire_code_buf(be, &buf_mem, &buf_size, alloc + pg);
    __builtin_memcpy(buf_mem, c.byte, code_sz);
    mprotect((char *)buf_mem + alloc, pg, PROT_NONE);
    int const ok = mprotect(buf_mem, alloc, PROT_READ | PROT_EXEC);
    assume(ok == 0);
    free(c.byte);

    struct jit_program *j = malloc(sizeof *j);
    j->code = buf_mem;
    j->code_size = buf_size;
    j->loop_start = loop_body_start;
    j->loop_end = loop_body_end;
    {
        union {
            void *p;
            void (*fn)(int, int, int, int, struct umbra_buf *);
        } u = {.p = buf_mem};
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
            ptr            p = inst->ptr;
            int            base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            if (scalar) {
                vmovd_load(c, s.rd, base, XI, 4, 0);
            } else {
                vmov_load(c, 1, s.rd, base, XI, 4, 0);
            }
        } break;

        case op_load_16: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            ptr            p = inst->ptr;
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
            int8_t ry = ra_ensure(ra, sl, ns, inst->y.id);
            ptr    p = inst->ptr;
            int    base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            if (scalar) {
                vmovd_store(c, ry, base, XI, 4, 0);
            } else {
                vmov_store(c, 1, ry, base, XI, 4, 0);
            }
            ra_free_chan(ra, inst->y, i);
        } break;

        case op_load_16x4: {
            struct ra_step s0 = ra_step_alloc(ra, sl, ns, i);
            int8_t r1 = ra_alloc(ra, sl, ns);
            int8_t r2 = ra_alloc(ra, sl, ns);
            int8_t r3 = ra_alloc(ra, sl, ns);
            ptr    p = inst->ptr;
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
            ptr    p = inst->ptr;
            int    base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);

            int8_t px = ra_alloc(ra, sl, ns);
            int8_t t0 = ra_alloc(ra, sl, ns);
            int8_t t1 = ra_alloc(ra, sl, ns);

            {
                int const sz_off = p.bits * (int)sizeof(struct umbra_buf)
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
            int8_t rr   = ra_ensure_chan(ra, sl, ns, inst->x.id, (int)inst->x.chan);
            int8_t rg   = ra_ensure_chan(ra, sl, ns, inst->y.id, (int)inst->y.chan);
            int8_t rb_  = ra_ensure_chan(ra, sl, ns, inst->z.id, (int)inst->z.chan);
            int8_t ra_v = ra_ensure_chan(ra, sl, ns, inst->w.id, (int)inst->w.chan);
            ptr    p = inst->ptr;
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
                vex_rrr(c, 1, 1, 0, 0x61, scale, rr, rg);
                vex_rrr(c, 1, 1, 0, 0x61, px, rb_, ra_v);
                vex_rrr(c, 1, 1, 0, 0x62, t, scale, px);
                vex_rrr(c, 1, 1, 0, 0x6a, z, scale, px);
                vex(c, 1, 3, 0, 1, t, t, z, 0x38); emit1(c, 1);
                vmov_store(c, 1, t, base, XI, 8, 0);
                // High 4:
                vex_rrr(c, 1, 1, 0, 0x69, scale, rr, rg);
                vex_rrr(c, 1, 1, 0, 0x69, px, rb_, ra_v);
                vex_rrr(c, 1, 1, 0, 0x62, t, scale, px);
                vex_rrr(c, 1, 1, 0, 0x6a, z, scale, px);
                vex(c, 1, 3, 0, 1, t, t, z, 0x38); emit1(c, 1);
                vmov_store(c, 1, t, base, XI, 8, 32);
            }
            ra_return_reg(ra, z);
            ra_return_reg(ra, t);
            ra_return_reg(ra, px);
            ra_return_reg(ra, scale);
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
            int    base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            {
                int const sz_off = p.bits * (int)sizeof(struct umbra_buf)
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
            int8_t rr   = ra_ensure_chan(ra, sl, ns, inst->x.id, (int)inst->x.chan);
            int8_t rg   = ra_ensure_chan(ra, sl, ns, inst->y.id, (int)inst->y.chan);
            int8_t rb_  = ra_ensure_chan(ra, sl, ns, inst->z.id, (int)inst->z.chan);
            int8_t ra_v = ra_ensure_chan(ra, sl, ns, inst->w.id, (int)inst->w.chan);
            ptr    p = inst->ptr;
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
            ra_free_chan(ra, inst->x, i);
            ra_free_chan(ra, inst->y, i);
            ra_free_chan(ra, inst->z, i);
            ra_free_chan(ra, inst->w, i);
        } break;

        case op_store_16: {
            int8_t ry = ra_ensure(ra, sl, ns, inst->y.id);
            ptr    p = inst->ptr;
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
            ra_free_chan(ra, inst->y, i);
        } break;

        case op_uniform_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            ptr            p = inst->ptr;
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
            int8_t         rx = ra_ensure(ra, sl, ns, inst->x.id);
            ptr            p = inst->ptr;
            int            base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            vmovd_to_gpr(c, RAX, rx);
            ra_free_chan(ra, inst->x, i);
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
            int8_t         rx = ra_ensure(ra, sl, ns, inst->x.id);
            ptr            p = inst->ptr;
            int            base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            if (scalar) {
                vmovd_to_gpr(c, RAX, rx);
                ra_free_chan(ra, inst->x, i);
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
                ra_free_chan(ra, inst->x, i);
            }
        } break;

        case op_sample_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int8_t         rx = ra_ensure(ra, sl, ns, inst->x.id);
            ptr            p = inst->ptr;
            int            base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            int8_t frac_r = ra_alloc(ra, sl, ns);
            int8_t hi_r   = ra_alloc(ra, sl, ns);
            if (scalar) {
                // floor + frac
                vroundps(c, hi_r, rx, 1);                  // hi_r = floor(ix)
                vsubps(c, frac_r, rx, hi_r);                // frac_r = ix - floor
                vcvttps2dq(c, hi_r, hi_r);                  // hi_r = int(floor)
                ra_free_chan(ra, inst->x, i);
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
                ra_free_chan(ra, inst->x, i);
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
            int8_t         rx = ra_ensure(ra, sl, ns, inst->x.id);
            ptr            p = inst->ptr;
            int            base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr, deref_rb_gpr);
            load_count_x86(c, p, 1);
            if (scalar) {
                vmovd_to_gpr(c, RAX, rx);
                ra_free_chan(ra, inst->x, i);
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
                ra_free_chan(ra, inst->x, i);
            }
        } break;
        case op_f32_from_f16: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i);
            vcvtph2ps(c, s.rd, s.rx);
        } break;

        case op_f16_from_f32: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i);
            vcvtps2ph(c, s.rd, s.rx, 4);
        } break;

        case op_i32_from_s16: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i);
            vpmovsxwd(c, s.rd, s.rx);
        } break;

        case op_i32_from_u16: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i);
            vpmovzxwd(c, s.rd, s.rx);
        } break;

        case op_i16_from_i32: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i);
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
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i);
            int8_t tmp = ra_alloc(ra, sl, ns);
            pool_broadcast(c, &jc->pool, tmp, (uint32_t)inst->imm);
            vpcmpgtd(c, s.rd, tmp, s.rx);
            ra_return_reg(ra, tmp);
        } break;
        case op_le_s32_imm: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i);
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
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i);
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
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i);
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
            struct ra_step s = ra_step_alu(ra, sl, ns, inst, i, nscratch);
            emit_alu_reg(c, inst->op, s.rd, s.rx, s.ry, s.rz, inst->imm, s.scratch,
                         s.scratch2);
            if (s.scratch >= 0) { ra_return_reg(ra, s.scratch); }
            if (s.scratch2 >= 0) { ra_return_reg(ra, s.scratch2); }
        } break;

        case op_shl_i32_imm:
        case op_shr_u32_imm:
        case op_shr_s32_imm: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i);
            emit_alu_reg(c, inst->op, s.rd, s.rx, 0, 0, inst->imm, -1, -1);
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

void jit_program_dump(struct jit_program const *j, FILE *f) {
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

#endif
