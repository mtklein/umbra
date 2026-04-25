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

_Static_assert(sizeof(struct umbra_buf) == 16, "");
_Static_assert(offsetof(struct umbra_buf, ptr)    ==  0, "");
_Static_assert(offsetof(struct umbra_buf, count)  ==  8, "");
_Static_assert(offsetof(struct umbra_buf, stride) == 12, "");

struct pool_ref {
    int data_off, code_pos, extra;
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
static void pool_ref_at(struct pool *p, int data_off, int code_pos, int extra) {
    if (p->refs == p->cap_refs) {
        p->cap_refs = p->cap_refs ? 2 * p->cap_refs : 32;
        p->ref = realloc(p->ref, (size_t)p->cap_refs * sizeof *p->ref);
    }
    p->ref[p->refs++] = (struct pool_ref){data_off, code_pos, extra};
}
static void pool_free(struct pool *p) {
    free(p->data);
    free(p->ref);
}

// RDI=col_end, RSI=x0_col, RDX=buf, R10=col, R14=row, R12=end_row.
enum { XWIDTH = RSI, XBUF = RDX, XCOL_X86 = R10, XH_X86 = R12, XM = R13, XY = R14 };

static void patch_jcc(Buf *c, int fixup) {
    int32_t rel = (int32_t)(c->size - (size_t)(fixup + 4));
    __builtin_memcpy(c->byte + fixup, &rel, 4);
}

static void load_count_x86(Buf *c, ptr p) {
    mov_load32(c, XM, XBUF, p.bits * (int)sizeof(struct umbra_buf)
               + (int)__builtin_offsetof(struct umbra_buf, count));
}
static int load_ptr_x86(Buf *c, ptr p, int *last_ptr, int elem_shift) {
    if (*last_ptr != p.bits) {
        *last_ptr = p.bits;
        mov_load(c, R11, XBUF,
                 p.bits * (int)sizeof(struct umbra_buf));
        mov_load32(c, RAX, XBUF,
                   p.bits * (int)sizeof(struct umbra_buf)
                   + (int)__builtin_offsetof(struct umbra_buf, stride));
        if (elem_shift) {
            shl_ri(c, RAX, (uint8_t)elem_shift);
        }
        imul_rr(c, RAX, XY);
        add_rr (c, R11, RAX);
    }
    return R11;
}

static int8_t const ra_pool_x86[] = {0, 1, 2,  3,  4,  5,  6,  7,
                                     8, 9, 10, 11, 12, 13, 14, 15};

struct jit_ctx {
    Buf                            *c;
    struct umbra_flat_ir const *ir;
    struct pool                     pool;
    int                             vars, loop_top, loop_br_skip;
    int                             if_depth;
    int                             if_cond_val[8];
};

static void x86_spill(int reg, int slot, void *ctx) {
    struct jit_ctx *j = ctx;
    vspill(j->c, reg, slot + j->vars);
}
static void x86_fill(int reg, int slot, void *ctx) {
    struct jit_ctx *j = ctx;
    vfill(j->c, reg, slot + j->vars);
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
                  pos = vbroadcastss_rip(c, d);
        pool_ref_at(p, off, pos, 0);
    }
}

static void x86_remat(int reg, int val, void *ctx) {
    struct jit_ctx *j = ctx;
    pool_broadcast(j->c, &j->pool, reg, (uint32_t)j->ir->inst[val].imm);
}

static struct ra* ra_create_x86(struct umbra_flat_ir const *ir, struct jit_ctx *jc) {
    struct ra_config cfg = {
        .pool = ra_pool_x86,
        .nregs = 16,
        .max_reg = 16,
        .spill = x86_spill,
        .fill = x86_fill,
        .remat = x86_remat,
        .ctx = jc,
    };
    return ra_create(ir, &cfg);
}

// Extract one 16-bit channel from two 128-bit lanes of interleaved u16 pixels,
// shuffling via the VPSHUFB mask at `off` and landing 8 contiguous u16 in `dst`.
// Inputs: `t0` and `t1` hold pixels 0-3 and 4-7; `px` is scratch; both `t0` and
// `px` are clobbered.
static void deinterleave_channel(Buf *c, struct jit_ctx *jc,
                                 int8_t dst, int8_t t0, int8_t t1, int8_t px, int off) {
    int ref = vpshufb_rip(c, dst, t0);
    pool_ref_at(&jc->pool, off, ref, 0);
    vextracti128(c, px, dst, 1);
    vpunpckldq(c, dst, dst, px);
    ref = vpshufb_rip(c, px, t1);
    pool_ref_at(&jc->pool, off, ref, 0);
    vextracti128(c, t0, px, 1);
    vpunpckldq(c, px, px, t0);
    vpunpcklqdq(c, dst, dst, px);
}

static void emit_alu_reg(Buf *c, enum op op, int d, int x, int y, int z,
                         int scratch, int scratch2) {
    switch ((int)op) {
    case op_add_f32: vaddps(c, d, x, y); break;
    case op_sub_f32: vsubps(c, d, x, y); break;
    case op_mul_f32: vmulps(c, d, x, y); break;
    case op_min_f32: vminps(c, d, x, y); break;
    case op_max_f32: vmaxps(c, d, x, y); break;
    case op_square_f32: vmulps(c, d, x, x); break;
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
    case op_square_add_f32:
        // d = x*x + y.
        if (d == x) {
            vfmadd132ps(c, d, y, x);
        } else if (d == y) {
            vfmadd231ps(c, d, x, x);
        } else {
            vmovaps(c, d, y);
            vfmadd231ps(c, d, x, x);
        }
        break;
    case op_square_sub_f32:
        // d = y - x*x.
        if (d == x) {
            vfnmadd132ps(c, d, y, x);
        } else if (d == y) {
            vfnmadd231ps(c, d, x, x);
        } else {
            vmovaps(c, d, y);
            vfnmadd231ps(c, d, x, x);
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
        vpminsd(c, scratch2, x, y);
        vpcmpeqd(c, d, y, scratch2);
        vpcmpeqd(c, scratch, scratch, scratch);
        vpxor_3(c, 1, d, d, scratch);
        break;
    case op_le_u32:
        vpmaxsd(c, scratch, x, y);
        vpcmpeqd(c, d, y, scratch);
        break;
    }
}

static void emit_ops(Buf *c, struct umbra_flat_ir const *ir, int from, int to,
                     int *sl, int *ns, struct ra *ra, _Bool scalar,
                     struct jit_ctx *jc);

static int resolve_ptr_x86(Buf *c, ptr p, int *last_ptr, int elem_shift) {
    return load_ptr_x86(c, p, last_ptr, elem_shift);
}

struct jit_program* jit_program(struct jit_backend *be,
                                           struct umbra_flat_ir const *ir) {
    int *sl = malloc((size_t)ir->insts * sizeof(int));
    for (int i = 0; i < ir->insts; i++) {
        sl[i] = -1;
    }
    int  ns = 0;

    Buf            c = {0};
    struct jit_ctx jc = {.c = &c, .ir = ir, .pool = {0}, .vars = ir->vars};
    struct ra     *ra = ra_create_x86(ir, &jc);

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
    mov_rr(&c, XCOL_X86, RDX);             // XCOL_X86 = r (save before XBUF overwrite)
    mov_rr(&c, XH_X86, RCX);         // XH_X86 = b
    mov_rr(&c, XBUF, R8);            // XBUF(RDX) = buf

    int const preamble_off = (int)c.size;
    emit_ops(&c, ir, 0, ir->preamble, sl, &ns, ra, 0, &jc);

    ra_begin_loop(ra);

    // RDI=l, RSI=t survive preamble; XCOL_X86=r, XH_X86=b saved above.
    mov_rr(&c, XY, RSI);             // XY = t
    mov_rr(&c, RSI, RDI);            // XWIDTH = l
    mov_rr(&c, RDI, XCOL_X86);             // RDI = r = col_end
    mov_rr(&c, XCOL_X86, RSI);             // XCOL_X86 = l

    int const loop_top = (int)c.size;
    ra_assert_loop_invariant(ra);

    // remaining = row_end (RDI) - XCOL_X86
    mov_rr(&c, R11, RDI);
    sub_rr(&c, R11, XCOL_X86);

    cmp_ri(&c, R11, 8);
    int const br_tail = jcc(&c, 0x0c);

    int const simd_body_off = (int)c.size;
    emit_ops(&c, ir, ir->preamble, ir->insts, sl, &ns, ra, 0, &jc);

    ra_end_loop(ra, sl);
    ra_assert_loop_invariant(ra);

    add_ri(&c, XCOL_X86, 8);
    {
        int     const j   = jmp(&c);
        int32_t const rel = (int32_t)(loop_top - (j + 4));
        __builtin_memcpy(c.byte + j, &rel, 4);
    }

    int const tail_top = (int)c.size;
    patch_jcc(&c, br_tail);

    cmp_rr(&c, XCOL_X86, RDI);                       // xi >= row_end?
    int const br_row_done = jcc(&c, 0x0d);     // JGE row_done

    ra_reset_pool(ra);
    for (int i = 0; i < ir->insts; i++) { sl[i] = -1; }

    emit_ops(&c, ir, 0, ir->preamble, sl, &ns, ra, 0, &jc);
    emit_ops(&c, ir, ir->preamble, ir->insts, sl, &ns, ra, 1, &jc);

    add_ri(&c, XCOL_X86, 1);
    {
        int     const j   = jmp(&c);
        int32_t const rel = (int32_t)(tail_top - (j + 4));
        __builtin_memcpy(c.byte + j, &rel, 4);
    }

    // row_done:
    int const row_done = (int)c.size;
    patch_jcc(&c, br_row_done);

    add_ri(&c, XY, 1);
    mov_rr(&c, XCOL_X86, XWIDTH);
    cmp_rr(&c, XY, XH_X86);
    int const br_done_all = jcc(&c, 0x0d);    // JGE -> done_all (no more rows)

    // Re-emit preamble so the next row's SIMD iter starts with uniforms in
    // the registers it expects: the tail body clobbers them, and the SIMD
    // body's end-of-loop fills are skipped when we enter the tail.
    int const next_row_off = (int)c.size;
    ra_reset_pool(ra);
    for (int i = 0; i < ir->insts; i++) { sl[i] = -1; }
    emit_ops(&c, ir, 0, ir->preamble, sl, &ns, ra, 0, &jc);
    ra_assert_loop_invariant(ra);
    {
        int     const j   = jmp(&c);
        int32_t const rel = (int32_t)(loop_top - (j + 4));
        __builtin_memcpy(c.byte + j, &rel, 4);
    }

    // done_all:
    int const done_all = (int)c.size;
    patch_jcc(&c, br_done_all);

    {
        int const total = ns + ir->vars;
        if (total > 0) {
            add_ri(&c, RSP, total * 32);
        }
    }
    pop_r(&c, RBX);
    pop_r(&c, R15);
    pop_r(&c, XH_X86);
    pop_r(&c, XY);
    pop_r(&c, XM);
    vzeroupper(&c);
    ret(&c);

    {
        int const total = ns + ir->vars;
        if (total > 0) {
            int pos = stack_patch;
            c.byte[pos++] = 0x48;
            c.byte[pos++] = 0x81;
            c.byte[pos++] = (uint8_t)(0xc0 | (5 << 3) | (RSP & 7));
            int32_t const sz = total * 32;
            __builtin_memcpy(c.byte + pos, &sz, 4);
        }
    }

    int const pool_start = (int)c.size;
    emit_bytes(&c, jc.pool.data, (size_t)jc.pool.bytes);
    for (int i = 0; i < jc.pool.refs; i++) {
        struct pool_ref *r = &jc.pool.ref[i];
        int     const entry_off = pool_start + r->data_off;
        int32_t const rel       = (int32_t)(entry_off - (r->code_pos + 4 + r->extra));
        __builtin_memcpy(c.byte + r->code_pos, &rel, 4);
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

    size_t const code_sz = c.size,
                 pg      = (size_t)sysconf(_SC_PAGESIZE),
                 alloc   = (code_sz + pg - 1) & ~(pg - 1);

    void  *buf_mem;
    size_t buf_size;
    jit_acquire_code_buf(be, &buf_mem, &buf_size, alloc + pg);
    __builtin_memcpy(buf_mem, c.byte, code_sz);
    mprotect((char *)buf_mem + alloc, pg, PROT_NONE);
    int const ok = mprotect(buf_mem, alloc, PROT_READ | PROT_EXEC);
    assume(ok == 0);
    free(c.byte);

    j->code = buf_mem;
    j->code_size = buf_size;
    j->code_bytes = (int)code_sz;
    j->labels = 0;
    j->label[j->labels++] = (struct jit_label){.name = "preamble",  .byte_off = preamble_off};
    j->label[j->labels++] = (struct jit_label){.name = "loop_top",  .byte_off = loop_top};
    j->label[j->labels++] = (struct jit_label){.name = "simd_body", .byte_off = simd_body_off};
    j->label[j->labels++] = (struct jit_label){.name = "tail_top",  .byte_off = tail_top};
    j->label[j->labels++] = (struct jit_label){.name = "row_done",  .byte_off = row_done};
    j->label[j->labels++] = (struct jit_label){.name = "next_row",  .byte_off = next_row_off};
    j->label[j->labels++] = (struct jit_label){.name = "done_all",  .byte_off = done_all};
    {
        union {
            void *p;
            void (*fn)(int, int, int, int, struct umbra_buf *);
        } u = {.p = buf_mem};
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
            vmovd_from_gpr(c, s.rd, XCOL_X86);
            if (!scalar) {
                vbroadcastss(c, s.rd, s.rd);
                uint32_t iota8[8] = {0, 1, 2, 3, 4, 5, 6, 7};
                int      off = pool_add(&jc->pool, iota8, 32);
                int      pos = vpaddd_rip(c, s.rd, s.rd);
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
            int            base = resolve_ptr_x86(c, p, &last_ptr, 2);
            if (scalar) {
                vmovd_load(c, s.rd, base, XCOL_X86, 4, 0);
            } else {
                vmov_load(c, 1, s.rd, base, XCOL_X86, 4, 0);
            }
        } break;

        case op_load_16: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            ptr            p = inst->ptr;
            int            base = resolve_ptr_x86(c, p, &last_ptr, 1);
            if (scalar) {
                movzx_word_load(c, RAX, base, XCOL_X86, 2, 0);
                vmovd_from_gpr(c, s.rd, RAX);
            } else {
                vmov_load(c, 0, s.rd, base, XCOL_X86, 2, 0);
            }
        } break;

        case op_store_32: {
            int8_t ry = ra_ensure_chan(ra, sl, ns, inst->y.id, (int)inst->y.chan);
            ptr    p = inst->ptr;
            int    base = resolve_ptr_x86(c, p, &last_ptr, 2);
            if (jc->if_depth > 0) {
                int8_t rm  = ra_ensure(ra, sl, ns, jc->if_cond_val[jc->if_depth - 1]);
                int8_t tmp = ra_alloc(ra, sl, ns);
                if (scalar) {
                    vmovd_load(c, tmp, base, XCOL_X86, 4, 0);
                    vpblendvb(c, 0, tmp, tmp, ry, rm);
                    vmovd_store(c, tmp, base, XCOL_X86, 4, 0);
                } else {
                    vmov_load(c, 1, tmp, base, XCOL_X86, 4, 0);
                    vpblendvb(c, 1, tmp, tmp, ry, rm);
                    vmov_store(c, 1, tmp, base, XCOL_X86, 4, 0);
                }
                ra_return_reg(ra, tmp);
            } else {
                if (scalar) {
                    vmovd_store(c, ry, base, XCOL_X86, 4, 0);
                } else {
                    vmov_store(c, 1, ry, base, XCOL_X86, 4, 0);
                }
            }
            ra_free_chan(ra, inst->y, i);
        } break;

        case op_load_16x4: {
            struct ra_step s0 = ra_step_alloc(ra, sl, ns, i);
            int8_t r1 = ra_alloc(ra, sl, ns);
            int8_t r2 = ra_alloc(ra, sl, ns);
            int8_t r3 = ra_alloc(ra, sl, ns);
            ptr    p = inst->ptr;
            int    base = resolve_ptr_x86(c, p, &last_ptr, 3);

            int8_t px = ra_alloc(ra, sl, ns);
            int8_t t0 = ra_alloc(ra, sl, ns);
            int8_t t1 = ra_alloc(ra, sl, ns);

            if (scalar) {
                vmovq_load(c, px, base, XCOL_X86, 8, 0);
                vmovaps(c, s0.rd, px);
                vpsrldq(c, r1, px, 2);
                vpsrldq(c, r2, px, 4);
                vpsrldq(c, r3, px, 6);
            } else {
                // Re-load raw data.
                vmov_load(c, 1, t0, base, XCOL_X86, 8, 0);   // pixels 0-3 raw
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

                vmov_load(c, 1, t1, base, XCOL_X86, 8, 32);  // pixels 4-7

                vmov_load(c, 1, t0, base, XCOL_X86, 8, 0);
                deinterleave_channel(c, jc, s0.rd, t0, t1, px, off_r);
                vmov_load(c, 1, t0, base, XCOL_X86, 8, 0);
                deinterleave_channel(c, jc, r1, t0, t1, px, off_g);
                vmov_load(c, 1, t0, base, XCOL_X86, 8, 0);
                deinterleave_channel(c, jc, r2, t0, t1, px, off_b);
                vmov_load(c, 1, t0, base, XCOL_X86, 8, 0);
                deinterleave_channel(c, jc, r3, t0, t1, px, off_a);
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
            int    base = resolve_ptr_x86(c, p, &last_ptr, 1);

            int8_t px = ra_alloc(ra, sl, ns);
            int8_t t0 = ra_alloc(ra, sl, ns);
            int8_t t1 = ra_alloc(ra, sl, ns);

            {
                int const count_off = p.bits * (int)sizeof(struct umbra_buf)
                                   + (int)__builtin_offsetof(struct umbra_buf, count);
                int const stride = scalar ? (int)XM : (int)RAX;
                mov_rr(c, R11, base);
                mov_load32(c, stride, XBUF, count_off);
                shr_ri(c, stride, 1);
                int8_t const plane[4] = {s0.rd, r1, r2, r3};
                if (scalar) {
                    for (int k = 0; k < 4; k++) {
                        if (k > 0) { add_rr(c, R11, stride); }
                        movzx_word_load(c, RAX, R11, XCOL_X86, 2, 0);
                        vmovd_from_gpr(c, plane[k], RAX);
                    }
                } else {
                    for (int k = 0; k < 4; k++) {
                        if (k > 0) { add_rr(c, R11, stride); }
                        vmov_load(c, 0, plane[k], R11, XCOL_X86, 2, 0);
                    }
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
            int    base = resolve_ptr_x86(c, p, &last_ptr, 3);
            int8_t scale = ra_alloc(ra, sl, ns);
            int8_t px    = ra_alloc(ra, sl, ns);
            int8_t t     = ra_alloc(ra, sl, ns);
            int8_t z     = ra_alloc(ra, sl, ns);
            if (jc->if_depth > 0) {
                int8_t rm  = ra_ensure(ra, sl, ns, jc->if_cond_val[jc->if_depth - 1]);
                int8_t mw  = ra_alloc(ra, sl, ns);
                int8_t old = ra_alloc(ra, sl, ns);
                if (scalar) {
                    vpunpcklwd(c, px, rr, rg);
                    vpunpcklwd(c, t, rb_, ra_v);
                    vpunpckldq(c, z, px, t);
                    vpmovsxdq (c, mw, rm);
                    vmovq_load (c, old, base, XCOL_X86, 8, 0);
                    vpblendvb  (c, 0, z, old, z, mw);
                    vmovq_store(c, z, base, XCOL_X86, 8, 0);
                } else {
                    vpunpcklwd(c, scale, rr, rg);
                    vpunpcklwd(c, px, rb_, ra_v);
                    vpunpckldq(c, t, scale, px);
                    vpunpckhdq(c, z, scale, px);
                    vinserti128(c, t, t, z, 1);
                    vpmovsxdq  (c, mw, rm);
                    vmov_load  (c, 1, old, base, XCOL_X86, 8, 0);
                    vpblendvb  (c, 1, t, old, t, mw);
                    vmov_store (c, 1, t, base, XCOL_X86, 8, 0);
                    vpunpckhwd(c, scale, rr, rg);
                    vpunpckhwd(c, px, rb_, ra_v);
                    vpunpckldq(c, t, scale, px);
                    vpunpckhdq(c, z, scale, px);
                    vinserti128(c, t, t, z, 1);
                    vextracti128(c, mw, rm, 1);
                    vpmovsxdq   (c, mw, mw);
                    vmov_load   (c, 1, old, base, XCOL_X86, 8, 32);
                    vpblendvb   (c, 1, t, old, t, mw);
                    vmov_store  (c, 1, t, base, XCOL_X86, 8, 32);
                }
                ra_return_reg(ra, old);
                ra_return_reg(ra, mw);
            } else if (scalar) {
                vpunpcklwd(c, px, rr, rg);     // [R,G,?,?...]
                vpunpcklwd(c, t, rb_, ra_v);   // [B,A,?,?...]
                vpunpckldq(c, z, px, t);       // [R,G,B,A,?,?...]
                vmovq_store(c, z, base, XCOL_X86, 8, 0);
            } else {
                vpunpcklwd(c, scale, rr, rg);
                vpunpcklwd(c, px, rb_, ra_v);
                vpunpckldq(c, t, scale, px);
                vpunpckhdq(c, z, scale, px);
                vinserti128(c, t, t, z, 1);
                vmov_store(c, 1, t, base, XCOL_X86, 8, 0);
                vpunpckhwd(c, scale, rr, rg);
                vpunpckhwd(c, px, rb_, ra_v);
                vpunpckldq(c, t, scale, px);
                vpunpckhdq(c, z, scale, px);
                vinserti128(c, t, t, z, 1);
                vmov_store(c, 1, t, base, XCOL_X86, 8, 32);
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
            int    base = resolve_ptr_x86(c, p, &last_ptr, 1);
            {
                int const count_off = p.bits * (int)sizeof(struct umbra_buf)
                                   + (int)__builtin_offsetof(struct umbra_buf, count);
                int const stride = scalar ? (int)XM : (int)RAX;
                mov_rr(c, R11, base);
                mov_load32(c, stride, XBUF, count_off);
                shr_ri(c, stride, 1);
                int8_t const plane[4] = {rr, rg, rb_, ra_v};
                if (jc->if_depth > 0) {
                    int8_t rm  = ra_ensure(ra, sl, ns, jc->if_cond_val[jc->if_depth - 1]);
                    int8_t mn  = ra_alloc(ra, sl, ns);
                    int8_t old = ra_alloc(ra, sl, ns);
                    if (scalar) {
                        for (int k = 0; k < 4; k++) {
                            if (k > 0) { add_rr(c, R11, stride); }
                            movzx_word_load(c, RAX, R11, XCOL_X86, 2, 0);
                            vmovd_from_gpr(c, old, RAX);
                            vpblendvb     (c, 0, old, old, plane[k], rm);
                            vmovd_to_gpr  (c, RAX, old);
                            mov_word_store(c, RAX, R11, XCOL_X86, 2, 0);
                        }
                    } else {
                        vextracti128(c, mn, rm, 1);
                        vpackssdw   (c, mn, rm, mn);
                        for (int k = 0; k < 4; k++) {
                            if (k > 0) { add_rr(c, R11, stride); }
                            vmov_load (c, 0, old, R11, XCOL_X86, 2, 0);
                            vpblendvb (c, 0, old, old, plane[k], mn);
                            vmov_store(c, 0, old, R11, XCOL_X86, 2, 0);
                        }
                    }
                    ra_return_reg(ra, old);
                    ra_return_reg(ra, mn);
                } else if (scalar) {
                    for (int k = 0; k < 4; k++) {
                        if (k > 0) { add_rr(c, R11, stride); }
                        vmovd_to_gpr(c, RAX, plane[k]);
                        mov_word_store(c, RAX, R11, XCOL_X86, 2, 0);
                    }
                } else {
                    for (int k = 0; k < 4; k++) {
                        if (k > 0) { add_rr(c, R11, stride); }
                        vmov_store(c, 0, plane[k], R11, XCOL_X86, 2, 0);
                    }
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
            int    base = resolve_ptr_x86(c, p, &last_ptr, 2);
            int8_t px   = ra_alloc(ra, sl, ns);
            int8_t mask = ra_alloc(ra, sl, ns);
            broadcast_imm32(c, mask, 0xFF);
            if (scalar) {
                vmovd_load(c, px, base, XCOL_X86, 4, 0);
            } else {
                vmov_load(c, 1, px, base, XCOL_X86, 4, 0);
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
            int    base = resolve_ptr_x86(c, p, &last_ptr, 2);
            int8_t px = ra_alloc(ra, sl, ns);
            int8_t t  = ra_alloc(ra, sl, ns);
            int L = scalar ? 0 : 1;
            vpslld_i(c, t, rg, 8);    vpor(c, L, px, rr, t);
            vpslld_i(c, t, rb_, 16);  vpor(c, L, px, px, t);
            vpslld_i(c, t, ra_v, 24); vpor(c, L, px, px, t);
            if (jc->if_depth > 0) {
                int8_t rm  = ra_ensure(ra, sl, ns, jc->if_cond_val[jc->if_depth - 1]);
                int8_t old = ra_alloc(ra, sl, ns);
                if (scalar) {
                    vmovd_load (c, old, base, XCOL_X86, 4, 0);
                    vpblendvb  (c, 0, px, old, px, rm);
                    vmovd_store(c, px,  base, XCOL_X86, 4, 0);
                } else {
                    vmov_load (c, 1, old, base, XCOL_X86, 4, 0);
                    vpblendvb (c, 1, px, old, px, rm);
                    vmov_store(c, 1, px,  base, XCOL_X86, 4, 0);
                }
                ra_return_reg(ra, old);
            } else if (scalar) {
                vmovd_store(c, px, base, XCOL_X86, 4, 0);
            } else {
                vmov_store(c, 1, px, base, XCOL_X86, 4, 0);
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
            int    base = resolve_ptr_x86(c, p, &last_ptr, 1);
            if (jc->if_depth > 0) {
                int8_t rm  = ra_ensure(ra, sl, ns, jc->if_cond_val[jc->if_depth - 1]);
                int8_t tmp = ra_alloc(ra, sl, ns);
                if (scalar) {
                    movzx_word_load(c, RAX, base, XCOL_X86, 2, 0);
                    vmovd_from_gpr(c, tmp, RAX);
                    vpblendvb(c, 0, tmp, tmp, ry, rm);
                    vmovd_to_gpr(c, RAX, tmp);
                    mov_word_store(c, RAX, base, XCOL_X86, 2, 0);
                    ra_return_reg(ra, tmp);
                } else {
                    int8_t mn = ra_alloc(ra, sl, ns);
                    vmov_load   (c, 0, tmp, base, XCOL_X86, 2, 0);
                    vextracti128(c, mn, rm, 1);
                    vpackssdw   (c, mn, rm, mn);
                    vpblendvb   (c, 0, tmp, tmp, ry, mn);
                    vmov_store  (c, 0, tmp, base, XCOL_X86, 2, 0);
                    ra_return_reg(ra, mn);
                    ra_return_reg(ra, tmp);
                }
            } else {
                if (scalar) {
                    vmovd_to_gpr(c, RAX, ry);
                    mov_word_store(c, RAX, base, XCOL_X86, 2, 0);
                } else {
                    vmov_store(c, 0, ry, base, XCOL_X86, 2, 0);
                }
            }
            ra_free_chan(ra, inst->y, i);
        } break;

        case op_uniform_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            ptr            p = inst->ptr;
            int            base = resolve_ptr_x86(c, p, &last_ptr, 2);
            vbroadcastss_mem(c, s.rd, base, inst->imm * 4);
        } break;

        case op_gather_uniform_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int8_t         rx = ra_ensure(ra, sl, ns, inst->x.id);
            ptr            p = inst->ptr;
            int            base = resolve_ptr_x86(c, p, &last_ptr, 2);
            vmovd_to_gpr(c, RAX, rx);
            ra_free_chan(ra, inst->x, i);
            load_count_x86(c, p);
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
            int            base = resolve_ptr_x86(c, p, &last_ptr, 2);
            if (scalar) {
                vmovd_to_gpr(c, RAX, rx);
                ra_free_chan(ra, inst->x, i);
                load_count_x86(c, p);
                vpxor(c, 0, s.rd, s.rd, s.rd);
                cmp_rr(c, RAX, XM);
                int skip = jcc(c, 0x03);
                vmovd_load(c, s.rd, base, RAX, 4, 0);
                patch_jcc(c, skip);
            } else {
                int8_t mask = ra_alloc(ra, sl, ns);
                int8_t cnt = ra_alloc(ra, sl, ns);
                load_count_x86(c, p);
                // Build in-bounds mask: (ix >= 0) AND (ix < count)
                vpxor(c, 1, mask, mask, mask);
                vpcmpgtd(c, mask, mask, rx);          // mask = (0 > ix) = neg lanes
                vmovd_from_gpr(c, cnt, XM);
                vbroadcastss(c, cnt, cnt);             // cnt = broadcast(count)
                vpcmpgtd(c, cnt, cnt, rx);             // cnt = (count > ix)
                // in_bounds = NOT(neg) AND (count > ix) = VPANDN(neg, count>ix)
                vpandn(c, mask, mask, cnt);                 // mask = NOT(mask) AND cnt
                // Pre-zero dst, gather only in-bounds lanes
                vpxor(c, 1, s.rd, s.rd, s.rd);
                vpgatherdd(c, s.rd, base, rx, 4, mask);
                ra_return_reg(ra, cnt);
                ra_return_reg(ra, mask);
                ra_free_chan(ra, inst->x, i);
            }
        } break;

        case op_gather_16: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int8_t         rx = ra_ensure(ra, sl, ns, inst->x.id);
            ptr            p = inst->ptr;
            int            base = resolve_ptr_x86(c, p, &last_ptr, 1);
            load_count_x86(c, p);
            if (scalar) {
                vmovd_to_gpr(c, RAX, rx);
                ra_free_chan(ra, inst->x, i);
                vpxor(c, 0, s.rd, s.rd, s.rd);
                cmp_rr(c, RAX, XM);
                int skip = jcc(c, 0x03);
                movzx_word_load(c, RAX, base, RAX, 2, 0);
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
                    movzx_word_load(c, RAX, base, RAX, 2, 0);
                    vpinsrw(c, s.rd, s.rd, RAX, (uint8_t)k);
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
                vpackusdw(c, s.rd, s.rx, tmp);
                ra_return_reg(ra, tmp);
            }
        } break;

        case op_imm_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            pool_broadcast(c, &jc->pool, s.rd, (uint32_t)inst->imm);
        } break;

        case op_abs_f32: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i);
            uint32_t       mask[8] = {0x7fffffffu, 0x7fffffffu, 0x7fffffffu, 0x7fffffffu,
                                      0x7fffffffu, 0x7fffffffu, 0x7fffffffu, 0x7fffffffu};
            int            off = pool_add(&jc->pool, mask, 32);
            int            pos = vandps_rip(c, s.rd, s.rx);
            pool_ref_at(&jc->pool, off, pos, 0);
        } break;
        case op_add_f32:
        case op_sub_f32:
        case op_mul_f32:
        case op_min_f32:
        case op_max_f32:
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
        case op_le_u32: {
            int            nscratch = (inst->op == op_lt_u32)                 ? 2
                           : (inst->op == op_le_s32 || inst->op == op_le_u32) ? 1
                                                                              : 0;
            struct ra_step s = ra_step_alu(ra, sl, ns, inst, i, nscratch);
            emit_alu_reg(c, inst->op, s.rd, s.rx, s.ry, s.rz, s.scratch, s.scratch2);
            if (s.scratch >= 0) { ra_return_reg(ra, s.scratch); }
            if (s.scratch2 >= 0) { ra_return_reg(ra, s.scratch2); }
        } break;

        case op_load_var: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            vfill(c, s.rd, inst->imm);
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
                vfill(c, tmp, inst->imm);
                vpblendvb(c, scalar ? 0 : 1, tmp, tmp, ry, rm);
                vspill(c, tmp, inst->imm);
                ra_return_reg(ra, tmp);
            } else {
                vspill(c, ry, inst->imm);
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
                vpand(c, scalar ? 0 : 1, rx, rx, outer);
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
            vmovd_to_gpr(c, R11, rx);
            ra_free_chan(ra, inst->x, i);
            ra_evict_live_before(ra, sl, ns, i);
            cmp_ri(c, R11, 0);
            jc->loop_br_skip = jcc(c, 0x0e);
            jc->loop_top = (int)c->size;
        } break;

        case op_loop_end: {
            int8_t rx = ra_ensure(ra, sl, ns, inst->x.id);
            vmovd_to_gpr(c, R11, rx);
            ra_free_chan(ra, inst->x, i);
            int8_t tmp = ra_alloc(ra, sl, ns);
            vfill(c, tmp, inst->imm);
            vmovd_to_gpr(c, XM, tmp);
            ra_return_reg(ra, tmp);
            cmp_rr(c, XM, R11);
            int const br = jcc(c, 0x0c);
            int32_t const rel = (int32_t)(jc->loop_top - (br + 4));
            __builtin_memcpy(c->byte + br, &rel, 4);
            patch_jcc(c, jc->loop_br_skip);
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
    uint8_t const *code = (uint8_t const*)j->code;
    int      const n    = j->code_bytes;

    char spath[] = "/tmp/umbra_dump_XXXXXX.s";
    char opath[sizeof spath + 2];
    int  fd      = mkstemps(spath, 2);
    if (fd < 0) { return; }
    FILE *sfp = fdopen(fd, "w");
    if (!sfp) { close(fd); remove(spath); return; }
    for (int i = 0; i < n; i++) {
        fprintf(sfp, ".byte 0x%02x\n", code[i]);
    }
    fclose(sfp);
    snprintf(opath, sizeof opath, "%.*s.o", (int)(sizeof spath - 3), spath);

    char cmd[512];
    snprintf(cmd, sizeof cmd,
             "/opt/homebrew/opt/llvm/bin/clang -target x86_64-apple-macos13"
             " -c %s -o %s 2>/dev/null", spath, opath);
    if (system(cmd) == 0) {
        jit_dump_with_labels(f, opath, j->label, j->labels);
    }
    remove(spath);
    remove(opath);
}

#endif
