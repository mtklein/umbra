#include "backend.h"
#include "bb.h"

#if !defined(__aarch64__) && !defined(__AVX2__)

struct umbra_jit { int dummy; };
struct umbra_jit* umbra_jit(
    struct umbra_basic_block const *bb
) { (void)bb; return 0; }
void umbra_jit_run (struct umbra_jit *j, int n, umbra_buf buf[]) {
    (void)j; (void)n; (void)buf;
}
void umbra_jit_free(struct umbra_jit *j) { (void)j; }
void umbra_dump_jit(
    struct umbra_jit const *j, FILE *f
) { (void)j; (void)f; }
void umbra_dump_jit_bin(
    struct umbra_jit const *j, FILE *f
) { (void)j; (void)f; }
void umbra_dump_jit_mca(
    struct umbra_jit const *j, FILE *f
) { (void)j; (void)f; }

#elif defined(__aarch64__)

#include "ra.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <pthread.h>
#include <unistd.h>
#include <libkern/OSCacheControl.h>

#include "asm_arm64.h"

enum { XP=8, XI=9, XT=10, XH=11, XW=12, XM=13, XS=15 };

static void load_ptr(Buf *c, int p, int *last_ptr) {
    if (*last_ptr == p) { return; }
    *last_ptr = p;
    int disp = p * 16;
    put(c, 0xf9400020u | ((uint32_t)(disp/8)<<10) | (1u<<5) | (uint32_t)XP);
}

static void resolve_ptr(Buf *c, int p, int *last_ptr, int const *deref_gpr) {
    if (p < 0) {
        if (*last_ptr != p) {
            *last_ptr = p;
            put(c, ADD_xi(XP, deref_gpr[~p], 0));
        }
    } else {
        load_ptr(c, p, last_ptr);
    }
}

static void load_max_ix(Buf *c, int p, int elem_shift, int const *deref_gpr) {
    if (p < 0) {
        (void)deref_gpr;
        put(c, 0xd2a00000u
            | (0x7fffu << 5)
            | (uint32_t)XM);
        return;
    }
    int disp = p * 16 + 8;
    put(c, 0xf9400020u | ((uint32_t)(disp / 8) << 10)
                       | (1u << 5) | (uint32_t)XM);
    put(c, 0xf10001bfu);
    put(c, 0xda8da1adu);
    put(c, 0x53000000u | ((uint32_t)elem_shift << 16) | (31u << 10)
                       | ((uint32_t)XM << 5) | (uint32_t)XM);
    put(c, 0x71000000u | (1u << 10) | ((uint32_t)XM << 5) | (uint32_t)XM);
    put(c, 0x1a800000u | ((uint32_t)XM << 16)
                       | (0x4u << 12)
                       | (31u << 5)
                       | (uint32_t)XM);
}

static void clamp_wt(Buf *c) {
    put(c, 0x7100001fu | ((uint32_t)XT << 5));
    put(c, 0x1a800000u | ((uint32_t)XT << 16)
                       | (0xbu << 12)
                       | (31u << 5)
                       | (uint32_t)XT);
    put(c, 0x6b00001fu | ((uint32_t)XM << 16) | ((uint32_t)XT << 5));
    put(c, 0x1a800000u | ((uint32_t)XT << 16)
                       | (0xau << 12)
                       | ((uint32_t)XM << 5)
                       | (uint32_t)XT);
}

static void vld(Buf *c, int vd, int s) { put(c, LDR_qi(vd, XS, s)); }
static void vst(Buf *c, int vd, int s) { put(c, STR_qi(vd, XS, s)); }

static _Bool emit_alu_reg(Buf *c, enum op op,
                          int d, int x, int y,
                          int z, int imm,
                          int scratch) {
    switch (op) {
    case op_imm_32: {
        uint32_t v=(uint32_t)imm;
        if (v == 0) {
            put(c, MOVI_4s(d, 0, 0));
        } else if (v == (v & 0x000000ffu)) {
            put(c, MOVI_4s(d, (uint8_t)v, 0));
        } else if (v == (v & 0x0000ff00u)) {
            put(c, MOVI_4s(d, (uint8_t)(v>>8), 8));
        } else if (v == (v & 0x00ff0000u)) {
            put(c, MOVI_4s(d,(uint8_t)(v>>16),16));
        } else if (v == (v & 0xff000000u)) {
            put(c, MOVI_4s(d,(uint8_t)(v>>24),24));
        } else if ((~v) == ((~v) & 0x000000ffu)) {
            put(c, MVNI_4s(d, (uint8_t)~v, 0));
        } else if ((~v) == ((~v) & 0x0000ff00u)) {
            put(c, MVNI_4s(d,(uint8_t)(~v>>8), 8));
        } else if ((~v) == ((~v) & 0x00ff0000u)) {
            put(c, MVNI_4s(d,(uint8_t)(~v>>16),16));
        } else if ((~v) == ((~v) & 0xff000000u)) {
            put(c, MVNI_4s(d,(uint8_t)(~v>>24),24));
        } else {
            load_imm_w(c,XT,v);
            put(c, DUP_4s_w(d,XT));
        }
    } return 1;

    case op_add_f32: put(c, FADD_4s(d,x,y)); return 1;
    case op_sub_f32: put(c, FSUB_4s(d,x,y)); return 1;
    case op_mul_f32: put(c, FMUL_4s(d,x,y)); return 1;
    case op_div_f32: put(c, FDIV_4s(d,x,y)); return 1;
    case op_min_f32: put(c, FMINNM_4s(d,x,y)); return 1;
    case op_max_f32: put(c, FMAXNM_4s(d,x,y)); return 1;
    case op_sqrt_f32: put(c, FSQRT_4s(d,x)); return 1;
    case op_fma_f32:
        if (d==z) {
            put(c, FMLA_4s(d,x,y));
        } else if (d!=x && d!=y) {
            put(c, ORR_16b(d,z,z));
            put(c, FMLA_4s(d,x,y));
        } else {
            put(c, ORR_16b(scratch,z,z));
            put(c, FMLA_4s(scratch,x,y));
            put(c, ORR_16b(d,scratch,scratch));
        }
        return 1;
    case op_fms_f32:
        if (d==z) {
            put(c, FMLS_4s(d,x,y));
        } else if (d!=x && d!=y) {
            put(c, ORR_16b(d,z,z));
            put(c, FMLS_4s(d,x,y));
        } else {
            put(c, ORR_16b(scratch,z,z));
            put(c, FMLS_4s(scratch,x,y));
            put(c, ORR_16b(d,scratch,scratch));
        }
        return 1;

    case op_add_i32: put(c, ADD_4s(d,x,y)); return 1;
    case op_sub_i32: put(c, SUB_4s(d,x,y)); return 1;
    case op_mul_i32: put(c, MUL_4s(d,x,y)); return 1;
    case op_shl_i32: put(c, USHL_4s(d,x,y)); return 1;
    case op_shr_u32:
        put(c, NEG_4s(scratch,y));
        put(c, USHL_4s(d,x,scratch));
        return 1;
    case op_shr_s32:
        put(c, NEG_4s(scratch,y));
        put(c, SSHL_4s(d,x,scratch));
        return 1;

    case op_f32_from_i32: put(c, SCVTF_4s(d,x)); return 1;
    case op_i32_from_f32: put(c, FCVTZS_4s(d,x)); return 1;

    case op_eq_f32: put(c, FCMEQ_4s(d,x,y)); return 1;
    case op_lt_f32: put(c, FCMGT_4s(d,y,x)); return 1;
    case op_le_f32: put(c, FCMGE_4s(d,y,x)); return 1;

    case op_eq_i32: put(c, CMEQ_4s(d,x,y)); return 1;
    case op_lt_s32: put(c, CMGT_4s(d,y,x)); return 1;
    case op_le_s32: put(c, CMGE_4s(d,y,x)); return 1;
    case op_lt_u32: put(c, CMHI_4s(d,y,x)); return 1;
    case op_le_u32: put(c, CMHS_4s(d,y,x)); return 1;

    case op_and_32: put(c, AND_16b(d,x,y)); return 1;
    case op_or_32:  put(c, ORR_16b(d,x,y)); return 1;
    case op_xor_32: put(c, EOR_16b(d,x,y)); return 1;
    case op_sel_32:
        if (d==x) { put(c, BSL_16b(d,y,z)); }
        else if (d==y) { put(c, BIF_16b(d,z,x)); }
        else if (d==z) { put(c, BIT_16b(d,y,x)); }
        else { put(c, ORR_16b(d,z,z)); put(c, BIT_16b(d,y,x)); }
        return 1;
    case op_join:
        if (d != x) { put(c, ORR_16b(d,x,x)); }
        return 1;
    case op_shl_imm:
        put(c, SHL_4s_imm(d,x,imm));
        return 1;
    case op_shr_u32_imm:
        put(c, USHR_4s_imm(d,x,imm));
        return 1;
    case op_shr_s32_imm:
        put(c, SSHR_4s_imm(d,x,imm));
        return 1;
    case op_pack:
        if (d == x) {
            put(c, SLI_4s_imm(d,y,imm));
        } else if (d != y) {
            put(c, ORR_16b(d,x,x));
            put(c, SLI_4s_imm(d,y,imm));
        } else {
            put(c, ORR_16b(scratch,x,x));
            put(c, SLI_4s_imm(scratch,y,imm));
            put(c, ORR_16b(d,scratch,scratch));
        }
        return 1;

    case op_and_imm:
    case op_iota:
    case op_deref_ptr:
    case op_buf_n: case op_lane_mask:
    case op_uni_32:   case op_load_32:
    case op_gather_32: case op_store_32:
    case op_scatter_32:
    case op_uni_16:   case op_load_16:
    case op_gather_16: case op_store_16:
    case op_scatter_16:
    case op_widen_f16:
    case op_narrow_f32:
    case op_widen_s16: case op_widen_u16:
    case op_narrow_16:
        return 0;
    }
}

static int8_t const ra_pool[] = {
    4,5,6,7,16,17,18,19,20,21,
    22,23,24,25,26,27,28,29,30,31,
};

static void arm64_spill(int reg, int slot,
                        void *ctx) {
    vst((Buf*)ctx, reg, slot);
}
static void arm64_fill(int reg, int slot,
                       void *ctx) {
    vld((Buf*)ctx, reg, slot);
}

static struct ra* ra_create_arm64(struct umbra_basic_block const *bb, Buf *c) {
    struct ra_config cfg = {
        .pool     = ra_pool,
        .nregs    = 20,
        .max_reg  = 32,
        .spill    = arm64_spill,
        .fill     = arm64_fill,
        .ctx      = c,
    };
    return ra_create(bb, &cfg);
}


static void emit_ops(Buf *c, struct umbra_basic_block const *bb,
                     int from, int to,
                     int *sl, int *ns, struct ra *ra, _Bool scalar,
                     int *deref_gpr);

struct umbra_jit {
    void  *code;
    size_t code_size;
    void (*entry)(int, umbra_buf*);
    int    loop_start, loop_end;
};

static _Bool arm64_chooser(struct bb_inst const *insts,
                           int join_id) {
    return insts[insts[join_id].y].op == op_pack;
}

struct umbra_jit* umbra_jit(struct umbra_basic_block const *bb) {
    struct umbra_basic_block *resolved =
        umbra_resolve_joins(bb, arm64_chooser);
    bb = resolved;

    int *sl = malloc((size_t)bb->insts * sizeof(int));
    for (int i = 0; i < bb->insts; i++) { sl[i] = -1; }
    int ns = 0;
    int *deref_gpr = calloc((size_t)bb->insts, sizeof(int));

    Buf c={0};
    struct ra *ra = ra_create_arm64(bb, &c);

    put(&c, STP_pre(29,30,31,-2));
    put(&c, ADD_xi(29,31,0));
    int stack_patch = c.len;
    put(&c, 0xd503201fu);
    put(&c, 0xd503201fu);

    emit_ops(&c, bb, 0, bb->preamble, sl, &ns, ra, 0, deref_gpr);

    ra_begin_loop(ra);

    put(&c, MOVZ_x(XI,0));

    int loop_top = c.len;

    put(&c, 0xcb090000u|(uint32_t)XT);
    put(&c, SUBS_xi(31,XT,4));
    int br_tail = c.len;
    put(&c, Bcond(0xb,0));
    put(&c, LSL_xi(XH, XI, 1));
    put(&c, LSL_xi(XW, XI, 2));

    int loop_body_start = c.len;
    emit_ops(&c, bb, bb->preamble, bb->insts, sl, &ns, ra, 0, deref_gpr);

    ra_end_loop(ra, sl);

    int loop_body_end = c.len;

    put(&c, ADD_xi(XI,XI,4));
    put(&c, B(loop_top - c.len));

    int tail_top = c.len;
    c.buf[br_tail] = Bcond(0xb, tail_top - br_tail);

    put(&c, 0xeb09001fu);
    int br_epi = c.len;
    put(&c, Bcond(0xd,0));

    for (int i = 0; i < bb->insts; i++) { ra_free_reg(ra, i); }
    for (int i = 0; i < bb->insts; i++) { sl[i] = -1; }

    emit_ops(&c, bb, 0, bb->preamble, sl, &ns, ra, 0, deref_gpr);
    emit_ops(&c, bb, bb->preamble, bb->insts, sl, &ns, ra, 1, deref_gpr);

    put(&c, ADD_xi(XI,XI,1));
    put(&c, B(tail_top - c.len));

    int epi = c.len;
    c.buf[br_epi] = Bcond(0xd, epi - br_epi);

    put(&c, ADD_xi(31,29,0));
    put(&c, LDP_post(29,30,31,2));
    put(&c, RET());

    if (ns > 0) {
        c.buf[stack_patch  ] = SUB_xi(31,31,ns*16);
        c.buf[stack_patch+1] = ADD_xi(XS,31,0);
    }
    ra_destroy(ra); free(sl); free(deref_gpr);
    umbra_basic_block_free(resolved);

    size_t code_sz = (size_t)c.len * 4;
    size_t pg = 16384;
    size_t alloc = (code_sz + pg-1) & ~(pg-1);

    void *mem = mmap(NULL, alloc, PROT_READ|PROT_WRITE|PROT_EXEC,
                     MAP_PRIVATE|MAP_ANON|MAP_JIT, -1, 0);
    if (mem==MAP_FAILED) { free(c.buf); return 0; }

    pthread_jit_write_protect_np(0);
    __builtin_memcpy(mem, c.buf, code_sz);
    pthread_jit_write_protect_np(1);
    sys_icache_invalidate(mem, code_sz);
    free(c.buf);

    struct umbra_jit *j = malloc(sizeof *j);
    j->code = mem;
    j->code_size = alloc;
    j->loop_start = loop_body_start;
    j->loop_end   = loop_body_end;
    j->entry = (void(*)(int,umbra_buf*))mem;
    return j;
}

static void emit_ops(Buf *c, struct umbra_basic_block const *bb,
                     int from, int to,
                     int *sl, int *ns,
                     struct ra *ra, _Bool scalar,
                     int *deref_gpr)
{
    #define lu(v) ra_last_use(ra, (v))
    int last_ptr = -1;
    int dc = 0;

    for (int i=from; i<to; i++) {
        struct bb_inst const *inst = &bb->inst[i];

        switch (inst->op) {
        case op_deref_ptr: {
            load_ptr(c, inst->ptr, &last_ptr);
            int gpr = 2 + dc++;
            deref_gpr[i] = gpr;
            if (inst->imm == 0) {
                put(c, 0xf9400000u | ((uint32_t)XP << 5) | (uint32_t)gpr);
            } else {
                load_imm_w(c, XT, (uint32_t)inst->imm);
                put(c, ADD_xr(XT, XP, XT));
                put(c, 0xf9400000u | ((uint32_t)XT << 5) | (uint32_t)gpr);
            }
        } break;

        case op_buf_n: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int p = inst->ptr;
            if (p < 0) {
                load_imm_w(c, XT, 0x7fffffffu);
            } else {
                int disp = p * 16 + 8;
                put(c, 0xf9400020u
                    | ((uint32_t)(disp / 8) << 10)
                    | (1u << 5) | (uint32_t)XT);
                put(c, 0xf100001fu
                    | ((uint32_t)XT << 5));
                put(c, 0xda800400u
                    | ((uint32_t)XT << 16)
                    | (0xau << 12)
                    | ((uint32_t)XT << 5)
                    | (uint32_t)XT);
                if (inst->imm > 0) {
                    put(c, 0xd340fc00u
                        | ((uint32_t)inst->imm << 16)
                        | ((uint32_t)XT << 5)
                        | (uint32_t)XT);
                }
            }
            put(c, DUP_4s_w(s.rd, XT));
        } break;
        case op_lane_mask: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            put(c, MVNI_4s(s.rd, 0, 0));
        } break;

        case op_iota: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            put(c, DUP_4s_w(s.rd, XI));
            if (!scalar) {
                int8_t tmp = ra_alloc(ra, sl, ns);
                put(c, MOVI_4s(tmp, 0, 0));
                put(c, MOVZ_w(XT,1)); put(c, INS_s(tmp,1,XT));
                put(c, MOVZ_w(XT,2)); put(c, INS_s(tmp,2,XT));
                put(c, MOVZ_w(XT,3)); put(c, INS_s(tmp,3,XT));
                put(c, ADD_4s(s.rd, s.rd, tmp));
                ra_return_reg(ra, tmp);
            }
        } break;

        case op_load_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr);
            if (inst->x) {
                int8_t ro = ra_ensure(ra, sl, ns, inst->x);
                put(c, UMOV_ws(XT, ro));
                if (lu(inst->x) <= i) { ra_free_reg(ra, inst->x); }
                put(c, LSL_xi(XT, XT, 2));
                put(c, ADD_xr(XT, XP, XT));
                if (scalar) { put(c, LDR_sx(s.rd, XT, XI)); }
                else         { put(c, LDR_q(s.rd, XT, XW)); }
                last_ptr = -999;
            } else {
                if (scalar) { put(c, LDR_sx(s.rd, XP, XI)); }
                else         { put(c, LDR_q(s.rd, XP, XW)); }
            }
        } break;

        case op_load_16: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr);
            int base16 = XP;
            if (inst->x) {
                int8_t ro = ra_ensure(ra, sl, ns, inst->x);
                put(c, UMOV_ws(XT, ro));
                if (lu(inst->x) <= i) { ra_free_reg(ra, inst->x); }
                put(c, LSL_xi(XT, XT, 1));
                put(c, ADD_xr(XT, XP, XT));
                last_ptr = -999;
                base16 = XT;
            }
            if (scalar) {
                put(c, 0x78607800u
                    | ((uint32_t)XI << 16)
                    | ((uint32_t)base16 << 5)
                    | (uint32_t)XT);
                put(c, DUP_4s_w(s.rd, XT));
            } else {
                put(c, LDR_d(s.rd, base16, XH));
            }
        } break;

        case op_uni_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr);
            if (inst->x) {
                int8_t rx = ra_ensure(ra, sl, ns, inst->x);
                if (lu(inst->x) <= i) { ra_free_reg(ra, inst->x); }
                put(c, UMOV_ws(XT, rx));
            } else {
                load_imm_w(c, XT, (uint32_t)inst->imm);
            }
            put(c, LDR_sx(s.rd, XP, XT));
            put(c, 0x4e040400u | ((uint32_t)s.rd<<5) | (uint32_t)s.rd);
        } break;

        case op_uni_16: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr);
            if (inst->x) {
                int8_t rx = ra_ensure(ra, sl, ns, inst->x);
                if (lu(inst->x) <= i) { ra_free_reg(ra, inst->x); }
                put(c, UMOV_ws(XT, rx));
                put(c, LSL_xi(XT, XT, 1));
            } else {
                load_imm_w(c, XT, (uint32_t)(inst->imm * 2));
            }
            put(c, 0x78606800u
                | ((uint32_t)XT << 16)
                | ((uint32_t)XP << 5)
                | (uint32_t)XT);
            put(c, DUP_4s_w(s.rd, XT));
        } break;

        case op_gather_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int8_t rx = ra_ensure(ra, sl, ns, inst->x);
            int8_t rz = ra_ensure(ra, sl, ns, inst->z);
            if (lu(inst->x) <= i) { ra_free_reg(ra, inst->x); }
            int p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr);
            load_max_ix(c, p, 2, deref_gpr);
            if (scalar) {
                put(c, UMOV_ws(XT, rx));
                clamp_wt(c);
                put(c, LDR_sx(s.rd, XP, XT));
            } else {
                for (int k = 0; k < 4; k++) {
                    put(c, UMOV_ws_lane(XT, rx, k));
                    clamp_wt(c);
                    put(c, LSL_xi(XT, XT, 2));
                    put(c, ADD_xr(XT, XP, XT));
                    put(c, LD1_s(s.rd, k, XT));
                }
            }
            put(c, AND_16b(s.rd, s.rd, rz));
            if (lu(inst->z) <= i) { ra_free_reg(ra, inst->z); }
        } break;

        case op_gather_16: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int8_t rx = ra_ensure(ra, sl, ns, inst->x);
            int8_t rz = ra_ensure(ra, sl, ns, inst->z);
            if (lu(inst->x) <= i) { ra_free_reg(ra, inst->x); }
            int p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr);
            load_max_ix(c, p, 1, deref_gpr);
            if (scalar) {
                put(c, UMOV_ws(XT, rx));
                clamp_wt(c);
                put(c, LSL_xi(XT, XT, 1));
                put(c, ADD_xr(XT, XP, XT));
                put(c, 0x79400000u | ((uint32_t)XT << 5) | (uint32_t)XT);
                put(c, DUP_4s_w(s.rd, XT));
            } else {
                for (int k = 0; k < 4; k++) {
                    put(c, UMOV_ws_lane(XT, rx, k));
                    clamp_wt(c);
                    put(c, LSL_xi(XT, XT, 1));
                    put(c, ADD_xr(XT, XP, XT));
                    put(c, 0x79400000u | ((uint32_t)XT << 5) | (uint32_t)XT);
                    put(c, INS_s(s.rd, k, XT));
                }
            }
            put(c, AND_16b(s.rd, s.rd, rz));
            if (lu(inst->z) <= i) { ra_free_reg(ra, inst->z); }
            if (!scalar) {
                put(c, XTN_4h(s.rd, s.rd));
            }
        } break;

        case op_store_32: {
            int8_t ry = ra_ensure(ra, sl, ns, inst->y);
            int p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr);
            if (inst->x) {
                int8_t ro = ra_ensure(ra, sl, ns, inst->x);
                put(c, UMOV_ws(XT, ro));
                if (lu(inst->x) <= i) { ra_free_reg(ra, inst->x); }
                put(c, LSL_xi(XT, XT, 2));
                put(c, ADD_xr(XT, XP, XT));
                if (scalar) { put(c, STR_sx(ry, XT, XI)); }
                else         { put(c, STR_q(ry, XT, XW)); }
                last_ptr = -999;
            } else {
                if (scalar) { put(c, STR_sx(ry, XP, XI)); }
                else         { put(c, STR_q(ry, XP, XW)); }
            }
            if (lu(inst->y) <= i) { ra_free_reg(ra, inst->y); }
        } break;

        case op_store_16: {
            int8_t ry = ra_ensure(ra, sl, ns, inst->y);
            int p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr);
            int sbase = XP;
            if (inst->x) {
                int8_t ro = ra_ensure(ra, sl, ns, inst->x);
                put(c, UMOV_ws(XT, ro));
                if (lu(inst->x) <= i) { ra_free_reg(ra, inst->x); }
                put(c, LSL_xi(XT, XT, 1));
                put(c, ADD_xr(XT, XP, XT));
                last_ptr = -999;
                sbase = XT;
            }
            if (scalar) {
                put(c, STR_hx(ry, sbase, XI));
            } else {
                put(c, STR_d(ry, sbase, XH));
            }
            if (lu(inst->y) <= i) { ra_free_reg(ra, inst->y); }
        } break;

        case op_scatter_32: {
            int8_t rx = ra_ensure(ra, sl, ns, inst->x);
            int8_t ry = ra_ensure(ra, sl, ns, inst->y);
            int8_t rz = ra_ensure(ra, sl, ns, inst->z);
            int p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr);
            if (scalar) {
                put(c, UMOV_ws(XH, rz));
                int skip = c->len;
                put(c, 0x34000000u | (uint32_t)XH);
                put(c, UMOV_ws(XT, rx));
                put(c, STR_sx(ry, XP, XT));
                {
                    int off = c->len - skip;
                    c->buf[skip] = 0x34000000u
                        | ((uint32_t)(off & 0x7ffff) << 5)
                        | (uint32_t)XH;
                }
            } else {
                for (int k = 0; k < 4; k++) {
                    put(c, UMOV_ws_lane(XH, rz, k));
                    int skip = c->len;
                    put(c, 0x34000000u | (uint32_t)XH);
                    uint32_t imm5 =
                        (uint32_t)(k << 3) | 4;
                    put(c, 0x0e003c00u | (imm5 << 16)
                        | ((uint32_t)rx << 5)
                        | (uint32_t)XT);
                    put(c, 0x0e003c00u | (imm5 << 16)
                        | ((uint32_t)ry << 5)
                        | (uint32_t)XH);
                    put(c, 0xb8207800u
                        | ((uint32_t)XT << 16)
                        | ((uint32_t)XP << 5)
                        | (uint32_t)XH);
                    {
                        int off = c->len - skip;
                        c->buf[skip] = 0x34000000u
                            | ((uint32_t)(off & 0x7ffff) << 5)
                            | (uint32_t)XH;
                    }
                }
            }
            if (lu(inst->z) <= i) { ra_free_reg(ra, inst->z); }
            if (lu(inst->x) <= i) { ra_free_reg(ra, inst->x); }
            if (lu(inst->y) <= i) { ra_free_reg(ra, inst->y); }
        } break;

        case op_scatter_16: {
            int8_t rx = ra_ensure(ra, sl, ns, inst->x);
            int8_t ry = ra_ensure(ra, sl, ns, inst->y);
            int8_t rz = ra_ensure(ra, sl, ns, inst->z);
            int p = inst->ptr;
            resolve_ptr(c, p, &last_ptr, deref_gpr);
            if (scalar) {
                put(c, UMOV_ws(XH, rz));
                int skip = c->len;
                put(c, 0x34000000u | (uint32_t)XH);
                put(c, UMOV_ws(XT, rx));
                put(c, 0x0e003c00u | (4u << 16)
                    | ((uint32_t)ry << 5)
                    | (uint32_t)XH);
                put(c, 0x78207800u
                    | ((uint32_t)XT << 16)
                    | ((uint32_t)XP << 5)
                    | (uint32_t)XH);
                {
                    int off = c->len - skip;
                    c->buf[skip] = 0x34000000u
                        | ((uint32_t)(off & 0x7ffff) << 5)
                        | (uint32_t)XH;
                }
            } else {
                int8_t wide =
                    ra_alloc(ra, sl, ns);
                put(c, 0x2f10a400u
                    | ((uint32_t)ry << 5)
                    | (uint32_t)wide);
                for (int k = 0; k < 4; k++) {
                    put(c, UMOV_ws_lane(XH, rz, k));
                    int skip = c->len;
                    put(c, 0x34000000u | (uint32_t)XH);
                    uint32_t imm5 =
                        (uint32_t)(k << 3) | 4;
                    put(c, 0x0e003c00u | (imm5 << 16)
                        | ((uint32_t)rx << 5)
                        | (uint32_t)XT);
                    put(c, 0x0e003c00u | (imm5 << 16)
                        | ((uint32_t)wide << 5)
                        | (uint32_t)XH);
                    put(c, 0x78207800u
                        | ((uint32_t)XT << 16)
                        | ((uint32_t)XP << 5)
                        | (uint32_t)XH);
                    {
                        int off = c->len - skip;
                        c->buf[skip] = 0x34000000u
                            | ((uint32_t)(off & 0x7ffff) << 5)
                            | (uint32_t)XH;
                    }
                }
                ra_return_reg(ra, wide);
            }
            if (lu(inst->z) <= i) { ra_free_reg(ra, inst->z); }
            if (lu(inst->x) <= i) { ra_free_reg(ra, inst->x); }
            if (lu(inst->y) <= i) { ra_free_reg(ra, inst->y); }
        } break;

        case op_widen_f16: {
            struct ra_step s = ra_step_unary(
                ra, sl, ns, inst, i, scalar);
            put(c, FCVTL_4s(s.rd, s.rx));
        } break;

        case op_narrow_f32: {
            struct ra_step s = ra_step_unary(
                ra, sl, ns, inst, i, scalar);
            put(c, FCVTN_4h(s.rd, s.rx));
        } break;

        case op_widen_s16: {
            struct ra_step s = ra_step_unary(
                ra, sl, ns, inst, i, scalar);
            put(c, SXTL_4s(s.rd, s.rx));
        } break;

        case op_widen_u16: {
            struct ra_step s = ra_step_unary(
                ra, sl, ns, inst, i, scalar);
            put(c, 0x2f10a400u
                | ((uint32_t)s.rx << 5)
                | (uint32_t)s.rd);
        } break;

        case op_narrow_16: {
            struct ra_step s = ra_step_unary(
                ra, sl, ns, inst, i, scalar);
            put(c, XTN_4h(s.rd, s.rx));
        } break;

        case op_eq_f32: case op_lt_f32: case op_le_f32:
        case op_eq_i32: case op_lt_s32: case op_le_s32: {
            _Bool x_is_0 = bb->inst[inst->x].op == op_imm_32
                        && bb->inst[inst->x].imm == 0;
            _Bool y_is_0 = bb->inst[inst->y].op == op_imm_32
                        && bb->inst[inst->y].imm == 0;

            if (x_is_0 || y_is_0) {
                int val = x_is_0 ? inst->y : inst->x;
                int8_t rv = ra_ensure(ra, sl, ns, val);
                _Bool v_dead = lu(val) <= i;

                int8_t rd;
                if (v_dead) {
                    rd = ra_claim(ra, val, i);
                } else {
                    rd = ra_alloc(ra, sl, ns);
                    ra_assign(ra, i, rd);
                }

                #define CZ(op_name, zr, zl) \
                    case op_name:       \
                        put(c, y_is_0   \
                            ? zr(rd,rv) \
                            : zl(rd,rv)); \
                        break;

                switch (inst->op) {
                CZ(op_eq_f32,  FCMEQ_4s_z, FCMEQ_4s_z)
                CZ(op_eq_i32,  CMEQ_4s_z,  CMEQ_4s_z)
                CZ(op_lt_f32,  FCMLT_4s_z, FCMGT_4s_z)
                CZ(op_lt_s32,  CMLT_4s_z,  CMGT_4s_z)
                CZ(op_le_f32,  FCMLE_4s_z, FCMGE_4s_z)
                CZ(op_le_s32,  CMLE_4s_z,  CMGE_4s_z)
                case op_iota: case op_deref_ptr:
                case op_buf_n: case op_lane_mask:
                case op_imm_32:
                case op_uni_32: case op_load_32:
                case op_gather_32: case op_store_32:
                case op_scatter_32:
                case op_add_f32: case op_sub_f32:
                case op_mul_f32: case op_div_f32:
                case op_min_f32: case op_max_f32:
                case op_sqrt_f32:
                case op_fma_f32: case op_fms_f32:
                case op_add_i32: case op_sub_i32:
                case op_mul_i32:
                case op_shl_i32: case op_shr_u32:
                case op_shr_s32:
                case op_and_32: case op_or_32:
                case op_xor_32: case op_sel_32:
                case op_f32_from_i32:
                case op_i32_from_f32:
                case op_lt_u32: case op_le_u32:
                case op_uni_16: case op_load_16:
                case op_gather_16: case op_store_16:
                case op_scatter_16:
                case op_widen_f16: case op_narrow_f32:
                case op_widen_s16: case op_widen_u16:
                case op_narrow_16:
                case op_join:
                case op_shl_imm: case op_shr_u32_imm:
                case op_shr_s32_imm: case op_pack:
                case op_and_imm:
                    break;
                }
                #undef CZ

                int zero_val = x_is_0 ? inst->x : inst->y;
                if (lu(zero_val) <= i) { ra_free_reg(ra, zero_val); }
                break;
            }
            goto default_alu;
        }

        case op_imm_32:
        case op_add_f32: case op_sub_f32:
        case op_mul_f32: case op_div_f32:
        case op_min_f32: case op_max_f32:
        case op_sqrt_f32:
        case op_fma_f32: case op_fms_f32:
        case op_add_i32: case op_sub_i32:
        case op_mul_i32:
        case op_shl_i32: case op_shr_u32:
        case op_shr_s32:
        case op_and_32: case op_or_32:
        case op_xor_32: case op_sel_32:
        case op_f32_from_i32:
        case op_i32_from_f32:
        case op_lt_u32: case op_le_u32:
        case op_join:
        case op_and_imm:
        default_alu: {
            int nscratch =
                (inst->op==op_shr_u32
              || inst->op==op_shr_s32)
                ? 1 : 0;
            struct ra_step s = ra_step_alu(
                ra, sl, ns, inst, i,
                scalar, nscratch);
            emit_alu_reg(c, inst->op,
                s.rd, s.rx, s.ry, s.rz,
                inst->imm, s.scratch);
            if (s.scratch >= 0) {
                ra_return_reg(ra, s.scratch);
            }
        } break;

        case op_shl_imm:
        case op_shr_u32_imm:
        case op_shr_s32_imm: {
            struct ra_step s = ra_step_unary(
                ra, sl, ns, inst, i, scalar);
            emit_alu_reg(c, inst->op,
                s.rd, s.rx, 0, 0,
                inst->imm, -1);
        } break;

        case op_pack: {
            struct ra_step s = ra_step_alu(
                ra, sl, ns, inst, i,
                scalar, 1);
            emit_alu_reg(c, inst->op,
                s.rd, s.rx, s.ry, 0,
                inst->imm, s.scratch);
            if (s.scratch >= 0) {
                ra_return_reg(ra, s.scratch);
            }
        } break;
        }
    }
    #undef lu
}

void umbra_jit_run(struct umbra_jit *j, int n, umbra_buf buf[]) {
    if (!j) { return; }
    j->entry(n, buf);
}
void umbra_jit_free(struct umbra_jit *j) {
    if (!j) { return; }
    munmap(j->code, j->code_size);
    free(j);
}

void umbra_dump_jit(struct umbra_jit const *j, FILE *f) {
    if (!j) { return; }
    size_t code_bytes = j->code_size;
    uint32_t const *words = (uint32_t const *)j->code;
    size_t nwords = code_bytes / 4;

    char tmp[] = "/tmp/umbra_jit_XXXXXX.s";
    int fd = mkstemps(tmp, 2);
    if (fd >= 0) {
        FILE *fp = fdopen(fd, "w");
        if (fp) {
            for (size_t i = 0; i < nwords; i++) {
                fprintf(fp, ".inst 0x%08x\n", words[i]);
            }
            fclose(fp);

            char opath[sizeof tmp + 2];
            snprintf(opath, sizeof opath, "%.*s.o", (int)(sizeof tmp - 3), tmp);

            char cmd[1024];
            snprintf(cmd, sizeof cmd,
                     "as -o %s %s 2>/dev/null && "
                     "/opt/homebrew/opt/llvm/bin/"
                     "llvm-objdump -d"
                     " --no-show-raw-insn"
                     " --no-leading-addr"
                     " %s 2>/dev/null",
                     opath, tmp, opath);
            FILE *p = popen(cmd, "r");
            if (p) {
                char line[256];
                _Bool ok = 0;
                while (fgets(line, (int)sizeof line, p)) {
                    if (!ok && __builtin_strstr(
                            line, "file format")) {
                        ok = 1; continue;
                    }
                    fputs(line, f);
                }
                int rc = pclose(p);
                remove(tmp);
                remove(opath);
                if (ok && rc == 0) { return; }
            }
            remove(tmp);
            remove(opath);
        } else {
            close(fd);
            remove(tmp);
        }
    }

    for (size_t i = 0; i < nwords; i++) {
        fprintf(f, "  %04zx: %08x\n", i * 4, words[i]);
    }
}

void umbra_dump_jit_bin(struct umbra_jit const *j, FILE *f) {
    if (!j) { return; }
    fwrite(j->code, 1, j->code_size, f);
}

void umbra_dump_jit_mca(struct umbra_jit const *j, FILE *f) {
    if (!j || j->loop_start >= j->loop_end) { return; }
    uint32_t const *words = (uint32_t const *)j->code;

    char tmp[] = "/tmp/umbra_mca_XXXXXX.s";
    int fd = mkstemps(tmp, 2);
    if (fd < 0) { return; }
    FILE *fp = fdopen(fd, "w");
    if (!fp) { close(fd); remove(tmp); return; }
    for (int i = j->loop_start; i < j->loop_end; i++) {
        fprintf(fp, ".inst 0x%08x\n", words[i]);
    }
    fclose(fp);

    char opath[sizeof tmp + 2];
    snprintf(opath, sizeof opath, "%.*s.o", (int)(sizeof tmp - 3), tmp);

    char asm_path[] = "/tmp/umbra_mca_loop_XXXXXX.s";
    int afd = mkstemps(asm_path, 2);
    if (afd < 0) { remove(tmp); return; }

    char cmd[1024];
    snprintf(cmd, sizeof cmd,
             "as -o %s %s 2>/dev/null && "
             "/opt/homebrew/opt/llvm/bin/"
             "llvm-objdump -d"
             " --no-show-raw-insn"
             " --no-leading-addr"
             " %s 2>/dev/null",
             opath, tmp, opath);
    FILE *p = popen(cmd, "r");
    if (!p) {
        close(afd);
        remove(tmp); remove(opath);
        remove(asm_path);
        return;
    }

    FILE *afp = fdopen(afd, "w");
    char line[256];
    _Bool past_header = 0;
    while (fgets(line, (int)sizeof line, p)) {
        if (!past_header) {
            if (__builtin_strstr(line, "<")) { past_header = 1; }
            continue;
        }
        if (line[0] == '\n' || line[0] == '<') { continue; }
        fputs(line, afp);
    }
    pclose(p);
    fclose(afp);
    remove(tmp);
    remove(opath);

    snprintf(cmd, sizeof cmd,
             "/opt/homebrew/opt/llvm/bin/llvm-mca"
             " -mcpu=apple-m4"
             " -iterations=100"
             " -bottleneck-analysis"
             " %s 2>&1",
             asm_path);
    p = popen(cmd, "r");
    if (p) {
        while (fgets(line, (int)sizeof line, p)) {
            fputs(line, f);
        }
        pclose(p);
    }
    remove(asm_path);
}

#elif defined(__AVX2__)

#include "ra.h"
#include "asm_x86.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

enum { XI=R10, XM=R13 };

static void patch_jcc(Buf *c, int fixup) {
    int32_t rel = (int32_t)(c->len - (fixup + 4));
    __builtin_memcpy(c->buf + fixup, &rel, 4);
}

static void load_max_ix_x86(Buf *c, int p, int elem_shift) {
    if (p < 0) {
        mov_ri(c, XM, 0x7fffffff);
        return;
    }
    mov_load(c, XM, RSI, p * 16 + 8);
    test_rr(c, XM, XM);
    int skip_neg = jcc(c, 0x09);
    neg_r(c, XM);
    patch_jcc(c, skip_neg);
    if (elem_shift) { shr_ri(c, XM, (uint8_t)elem_shift); }
    sub_ri(c, XM, 1);
    test_rr(c, XM, XM);
    int skip_zero = jcc(c, 0x09);
    xor_rr(c, XM, XM);
    patch_jcc(c, skip_zero);
}

static void clamp_eax_x86(Buf *c) {
    emit1(c, 0x48); emit1(c, 0x63); emit1(c, 0xc0);
    test_rr(c, RAX, RAX);
    int skip_lo = jcc(c, 0x09);
    xor_rr(c, RAX, RAX);
    patch_jcc(c, skip_lo);
    cmp_rr(c, RAX, XM);
    int skip_hi = jcc(c, 0x0e);
    mov_rr(c, RAX, XM);
    patch_jcc(c, skip_hi);
}

static void apply_offset_x86(Buf *c, int8_t off_reg, int elem_shift) {
    vex(c, 1, 1, 0, 0, off_reg, 0, RAX, 0x7e);
    emit1(c, 0x48); emit1(c, 0x63); emit1(c, 0xc0);
    if (elem_shift) {
        emit1(c, 0x48); emit1(c, 0xc1);
        emit1(c, 0xe0);
        emit1(c, (uint8_t)elem_shift);
    }
    emit1(c, 0x49); emit1(c, 0x01); emit1(c, 0xc3);
}

static int load_ptr_x86(Buf *c, int p, int *last_ptr) {
    if (*last_ptr != p) {
        *last_ptr = p;
        mov_load(c, R11, RSI, p * 16);
    }
    return R11;
}

static int8_t const ra_pool_x86[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

static void x86_spill(int reg, int slot,
                      void *ctx) {
    vspill((Buf*)ctx, reg, slot);
}
static void x86_fill(int reg, int slot,
                     void *ctx) {
    vfill((Buf*)ctx, reg, slot);
}

static struct ra* ra_create_x86(struct umbra_basic_block const *bb, Buf *c) {
    struct ra_config cfg = {
        .pool     = ra_pool_x86,
        .nregs    = 16,
        .max_reg  = 16,
        .spill    = x86_spill,
        .fill     = x86_fill,
        .ctx      = c,
    };
    return ra_create(bb, &cfg);
}

static _Bool emit_alu_reg(Buf *c, enum op op,
                          int d, int x, int y,
                          int z, int imm,
                          int scratch, int scratch2) {
    switch (op) {
    case op_imm_32: broadcast_imm32(c, d, (uint32_t)imm); return 1;

    case op_add_f32:  vaddps(c,d,x,y); return 1;
    case op_sub_f32:  vsubps(c,d,x,y); return 1;
    case op_mul_f32:  vmulps(c,d,x,y); return 1;
    case op_div_f32:  vdivps(c,d,x,y); return 1;
    case op_min_f32:  vminps(c,d,x,y); return 1;
    case op_max_f32:  vmaxps(c,d,x,y); return 1;
    case op_sqrt_f32: vsqrtps(c,d,x);  return 1;
    case op_fma_f32:
        if      (d == x) { vfmadd132ps(c,d,z,y); }
        else if (d == y) { vfmadd213ps(c,d,x,z); }
        else if (d == z) { vfmadd231ps(c,d,x,y); }
        else             { vmovaps(c,d,z); vfmadd231ps(c,d,x,y); }
        return 1;
    case op_fms_f32:
        if      (d == x) { vfnmadd132ps(c,d,z,y); }
        else if (d == y) { vfnmadd213ps(c,d,x,z); }
        else if (d == z) { vfnmadd231ps(c,d,x,y); }
        else             { vmovaps(c,d,z); vfnmadd231ps(c,d,x,y); }
        return 1;

    case op_add_i32: vpaddd(c,d,x,y); return 1;
    case op_sub_i32: vpsubd(c,d,x,y); return 1;
    case op_mul_i32: vpmulld(c,d,x,y); return 1;
    case op_shl_i32: vpsllvd(c,d,x,y); return 1;
    case op_shr_u32: vpsrlvd(c,d,x,y); return 1;
    case op_shr_s32: vpsravd(c,d,x,y);
        return 1;

    case op_and_32: vpand(c,1,d,x,y); return 1;
    case op_or_32:  vpor(c,1,d,x,y);  return 1;
    case op_xor_32: vpxor_3(c,1,d,x,y); return 1;
    case op_sel_32: vpblendvb(c,1,d,z,y,x); return 1;

    case op_f32_from_i32: vcvtdq2ps(c,d,x); return 1;
    case op_i32_from_f32: vcvttps2dq(c,d,x); return 1;

    case op_eq_f32: vcmpps(c,d,x,y,0);  return 1;
    case op_lt_f32: vcmpps(c,d,x,y,1);  return 1;
    case op_le_f32: vcmpps(c,d,x,y,2);  return 1;

    case op_eq_i32: vpcmpeqd(c,d,x,y); return 1;
    case op_lt_s32: vpcmpgtd(c,d,y,x); return 1;
    case op_le_s32:
        vpcmpgtd(c,d,x,y);
        vpcmpeqd(c,scratch,scratch,scratch);
        vpxor_3(c,1,d,d,scratch);
        return 1;
    case op_lt_u32:
        vex_rrr(c,1,2,1,0x3b,scratch2,x,y);
        vpcmpeqd(c,d,y,scratch2);
        vpcmpeqd(c,scratch,scratch,scratch);
        vpxor_3(c,1,d,d,scratch);
        return 1;
    case op_le_u32:
        vex_rrr(c,1,2,1,0x3f,scratch,x,y);
        vpcmpeqd(c,d,y,scratch);
        return 1;
    case op_join:
        if (d != x) { vmovaps(c,d,x); }
        return 1;
    case op_shl_imm:
        vpslld_i(c,d,x,(uint8_t)imm);
        return 1;
    case op_shr_u32_imm:
        vpsrld_i(c,d,x,(uint8_t)imm);
        return 1;
    case op_shr_s32_imm:
        vpsrad_i(c,d,x,(uint8_t)imm);
        return 1;
    case op_pack:
    case op_and_imm:
        return 0;

    case op_iota:
    case op_deref_ptr:
    case op_buf_n: case op_lane_mask:
    case op_uni_32:   case op_load_32:
    case op_gather_32: case op_store_32:
    case op_scatter_32:
    case op_uni_16:   case op_load_16:
    case op_gather_16: case op_store_16:
    case op_scatter_16:
    case op_widen_f16: case op_narrow_f32:
    case op_widen_s16: case op_widen_u16:
    case op_narrow_16:
        return 0;
    }
}

static void emit_ops(
    Buf *c,
    struct umbra_basic_block const *bb,
    int from, int to,
    int *sl, int *ns,
    struct ra *ra, _Bool scalar,
    int *deref_gpr);

static int resolve_ptr_x86(Buf *c, int p, int *last_ptr, int const *deref_gpr) {
    if (p < 0) { return deref_gpr[~p]; }
    return load_ptr_x86(c, p, last_ptr);
}

struct umbra_jit {
    void  *code;
    size_t code_size, code_len;
    void (*entry)(int, umbra_buf*);
    int    loop_start, loop_end;
};

struct umbra_jit* umbra_jit(struct umbra_basic_block const *bb) {
    struct umbra_basic_block *resolved =
        umbra_resolve_joins(bb, NULL);
    bb = resolved;

    int *sl = malloc((size_t)bb->insts * sizeof(int));
    for (int i = 0; i < bb->insts; i++) { sl[i] = -1; }
    int ns = 0;
    int *deref_gpr = calloc((size_t)bb->insts, sizeof(int));

    Buf c = {0};
    struct ra *ra = ra_create_x86(bb, &c);

    push_r(&c, XM);

    int stack_patch = c.len;
    for (int i = 0; i < 7; i++) { nop(&c); }

    emit_ops(&c, bb, 0, bb->preamble, sl, &ns, ra, 0, deref_gpr);

    ra_begin_loop(ra);

    xor_rr(&c, XI, XI);

    int loop_top = c.len;

    mov_rr(&c, R11, RDI);
    rex_w(&c, XI, R11);
    emit1(&c, 0x29);
    emit1(&c, (uint8_t)(0xc0 | ((XI&7)<<3) | (R11&7)));

    cmp_ri(&c, R11, 8);
    int br_tail = jcc(&c, 0x0c);

    int loop_body_start = c.len;
    emit_ops(&c, bb, bb->preamble, bb->insts, sl, &ns, ra, 0, deref_gpr);

    ra_end_loop(ra, sl);

    int loop_body_end = c.len;

    add_ri(&c, XI, 8);
    int jmp_top = jmp(&c);
    {
        int32_t rel = (int32_t)(loop_top - (jmp_top + 4));
        __builtin_memcpy(c.buf + jmp_top, &rel, 4);
    }

    int tail_top = c.len;
    {
        int32_t rel = (int32_t)(tail_top - (br_tail + 4));
        __builtin_memcpy(c.buf + br_tail, &rel, 4);
    }

    cmp_rr(&c, XI, RDI);
    int br_epi = jcc(&c, 0x0d);

    for (int i = 0; i < bb->insts; i++) { ra_free_reg(ra, i); }
    for (int i = 0; i < bb->insts; i++) { sl[i] = -1; }

    emit_ops(&c, bb, 0, bb->preamble, sl, &ns, ra, 0, deref_gpr);
    emit_ops(&c, bb, bb->preamble, bb->insts, sl, &ns, ra, 1, deref_gpr);

    add_ri(&c, XI, 1);
    {
        int j = jmp(&c);
        int32_t rel = (int32_t)(tail_top - (j + 4));
        __builtin_memcpy(c.buf + j, &rel, 4);
    }

    int epi = c.len;
    {
        int32_t rel = (int32_t)(epi - (br_epi + 4));
        __builtin_memcpy(c.buf + br_epi, &rel, 4);
    }

    if (ns > 0) {
        add_ri(&c, RSP, ns * 32);
    }
    pop_r(&c, XM);
    vzeroupper(&c);
    ret(&c);

    if (ns > 0) {
        int pos = stack_patch;
        c.buf[pos++] = 0x48;
        c.buf[pos++] = 0x81;
        c.buf[pos++] = (uint8_t)(0xc0 | (5<<3) | (RSP&7));
        int32_t sz = ns * 32;
        __builtin_memcpy(c.buf + pos, &sz, 4);
    }

    ra_destroy(ra); free(sl); free(deref_gpr);
    umbra_basic_block_free(resolved);

    size_t code_sz = (size_t)c.len;
    size_t pg = (size_t)sysconf(_SC_PAGESIZE);
    size_t alloc = (code_sz + pg-1) & ~(pg-1);

    void *mem = mmap(NULL, alloc,
                     PROT_READ|PROT_WRITE,
                     MAP_PRIVATE|MAP_ANON,
                     -1, 0);
    if (mem == MAP_FAILED) { free(c.buf); return 0; }
    __builtin_memcpy(mem, c.buf, code_sz);
    if (mprotect(mem, alloc, PROT_READ|PROT_EXEC) != 0) {
        munmap(mem, alloc);
        free(c.buf);
        return 0;
    }
    free(c.buf);

    struct umbra_jit *j = malloc(sizeof *j);
    j->code = mem;
    j->code_size = alloc; j->code_len = code_sz;
    j->loop_start = loop_body_start;
    j->loop_end   = loop_body_end;
    j->entry = (void(*)(int,umbra_buf*))mem;
    return j;
}

static void emit_ops(Buf *c, struct umbra_basic_block const *bb,
                     int from, int to,
                     int *sl, int *ns,
                     struct ra *ra, _Bool scalar,
                     int *deref_gpr)
{
    #define lu(v) ra_last_use(ra, (v))
    int last_ptr = -1;
    int dc = 0;
    int const deref_gprs[] = {RDX, RCX, R8, R9};

    for (int i = from; i < to; i++) {
        struct bb_inst const *inst = &bb->inst[i];
        switch (inst->op) {
        case op_deref_ptr: {
            int base = load_ptr_x86(c, inst->ptr, &last_ptr);
            int gpr = deref_gprs[dc++];
            mov_load(c, gpr, base, inst->imm);
            deref_gpr[i] = gpr;
        } break;

        case op_buf_n: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int p = inst->ptr;
            if (p < 0) {
                broadcast_imm32(c, s.rd, 0x7fffffffu);
            } else {
                mov_load(c, RAX, RSI, p * 16 + 8);
                test_rr(c, RAX, RAX);
                int skip_neg = jcc(c, 0x09);
                neg_r(c, RAX);
                patch_jcc(c, skip_neg);
                if (inst->imm) {
                    shr_ri(c, RAX,
                           (uint8_t)inst->imm);
                }
                vex(c, 1, 1, 0, 0,
                    s.rd, 0, RAX, 0x6e);
                vbroadcastss(c, s.rd, s.rd);
            }
        } break;
        case op_lane_mask: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            vpcmpeqd(c, s.rd, s.rd, s.rd);
        } break;

        case op_iota: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            if (scalar) {
                vex(c, 1, 1, 0, 0, s.rd, 0, XI, 0x6e);
            } else {
                vex(c, 1, 1, 0, 0, s.rd, 0, XI, 0x6e);
                vbroadcastss(c, s.rd, s.rd);
                int8_t tmp = ra_alloc(ra, sl, ns);
                sub_ri(c, RSP, 32);
                for (int k = 0; k < 8; k++) {
                    emit1(c, 0xc7);
                    if (k == 0) {
                        emit1(c, 0x04); emit1(c, 0x24);
                    } else {
                        emit1(c, 0x44);
                        emit1(c, 0x24);
                        emit1(c, (uint8_t)(k*4));
                    }
                    emit4(c, (uint32_t)k);
                }
                vfill(c, tmp, 0);
                vpaddd(c, s.rd, s.rd, tmp);
                add_ri(c, RSP, 32);
                ra_return_reg(ra, tmp);
            }
        } break;

        case op_load_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int p = inst->ptr;
            int base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr);
            if (inst->x) {
                if (base != R11) { mov_rr(c, R11, base); last_ptr = -1; }
                int8_t ro = ra_ensure(ra, sl, ns, inst->x);
                apply_offset_x86(c, ro, 2);
                if (lu(inst->x) <= i) { ra_free_reg(ra, inst->x); }
                last_ptr = -1;
                base = R11;
            }
            if (scalar) {
                vex_mem(c, 1, 1, 0, 0, s.rd, 0, 0x6e, base, XI, 4, 0);
            } else {
                vmov_load(c, 1, s.rd, base, XI, 4, 0);
            }
        } break;

        case op_load_16: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int p = inst->ptr;
            int base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr);
            if (inst->x) {
                if (base != R11) { mov_rr(c, R11, base); last_ptr = -1; }
                int8_t ro = ra_ensure(ra, sl, ns, inst->x);
                apply_offset_x86(c, ro, 1);
                if (lu(inst->x) <= i) { ra_free_reg(ra, inst->x); }
                last_ptr = -1;
                base = R11;
            }
            if (scalar) {
                // MOVZX eax, word [base + R10*2]
                {
                    uint8_t rex = 0x40;
                    if (XI >= 8) { rex |= 0x02; }
                    if (base >= 8) { rex |= 0x01; }
                    if (rex != 0x40) { emit1(c, rex); }
                    emit1(c, 0x0f); emit1(c, 0xb7);
                    emit1(c, (uint8_t)(((RAX&7)<<3) | 4));
                    emit1(c, (uint8_t)((1<<6) | ((XI&7)<<3) | (base&7)));
                }
                vex(c, 1, 1, 0, 0, s.rd, 0, RAX, 0x6e);
            } else {
                // Load 128-bit (8 x u16)
                vmov_load(c, 0, s.rd, base, XI, 2, 0);
            }
        } break;

        case op_store_32: {
            int8_t ry = ra_ensure(ra, sl, ns, inst->y);
            int p = inst->ptr;
            int base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr);
            if (inst->x) {
                if (base != R11) { mov_rr(c, R11, base); last_ptr = -1; }
                int8_t ro = ra_ensure(ra, sl, ns, inst->x);
                apply_offset_x86(c, ro, 2);
                if (lu(inst->x) <= i) { ra_free_reg(ra, inst->x); }
                last_ptr = -1;
                base = R11;
            }
            if (scalar) {
                vex_mem(c, 1, 1, 0, 0, ry, 0, 0x7e, base, XI, 4, 0);
            } else {
                vmov_store(c, 1, ry, base, XI, 4, 0);
            }
            if (lu(inst->y) <= i) { ra_free_reg(ra, inst->y); }
        } break;

        case op_store_16: {
            int8_t ry = ra_ensure(ra, sl, ns, inst->y);
            int p = inst->ptr;
            int base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr);
            if (inst->x) {
                if (base != R11) { mov_rr(c, R11, base); last_ptr = -1; }
                int8_t ro = ra_ensure(ra, sl, ns, inst->x);
                apply_offset_x86(c, ro, 1);
                if (lu(inst->x) <= i) { ra_free_reg(ra, inst->x); }
                last_ptr = -1;
                base = R11;
            }
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
                    emit1(c, (uint8_t)(((RAX&7)<<3) | 4));
                    emit1(c, (uint8_t)((1<<6) | ((XI&7)<<3) | (base&7)));
                }
            } else {
                vmov_store(c, 0, ry, base, XI, 2, 0);
            }
            if (lu(inst->y) <= i) { ra_free_reg(ra, inst->y); }
        } break;

        case op_uni_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int p = inst->ptr;
            int base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr);
            if (inst->x) {
                int8_t rx = ra_ensure(ra, sl, ns, inst->x);
                if (lu(inst->x) <= i) { ra_free_reg(ra, inst->x); }
                vex(c, 1, 1, 0, 0, rx, 0, RAX, 0x7e);
                vex_mem(c, 1, 1, 0, 0, s.rd, 0,
                        0x6e, base, RAX, 4, 0);
                vbroadcastss(c, s.rd, s.rd);
            } else {
                int disp = inst->imm * 4;
                uint8_t R = (uint8_t)(~s.rd >> 3) & 1;
                uint8_t B = (uint8_t)(~base >> 3) & 1;
                emit1(c, 0xc4);
                emit1(c, (uint8_t)(
                    (R<<7) | (1<<6)
                    | (B<<5) | 0x02));
                emit1(c, 0x7d);
                emit1(c, 0x18);
                if (disp == 0
                        && (base & 7) != RBP) {
                    emit1(c, (uint8_t)(
                        ((s.rd & 7) << 3)
                        | (base & 7)));
                    if ((base & 7) == RSP) {
                        emit1(c, 0x24);
                    }
                } else if (disp >= -128
                        && disp <= 127) {
                    emit1(c, (uint8_t)(
                        0x40
                        | ((s.rd & 7) << 3)
                        | (base & 7)));
                    if ((base & 7) == RSP) {
                        emit1(c, 0x24);
                    }
                    emit1(c, (uint8_t)disp);
                } else {
                    emit1(c, (uint8_t)(
                        0x80
                        | ((s.rd & 7) << 3)
                        | (base & 7)));
                    if ((base & 7) == RSP) {
                        emit1(c, 0x24);
                    }
                    emit4(c, (uint32_t)disp);
                }
            }
        } break;

        case op_uni_16: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int p = inst->ptr;
            int base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr);
            if (inst->x) {
                int8_t rx = ra_ensure(ra, sl, ns, inst->x);
                if (lu(inst->x) <= i) { ra_free_reg(ra, inst->x); }
                vex(c, 1, 1, 0, 0, rx, 0, RAX, 0x7e);
                {
                    uint8_t rex = 0x48;
                    if (base >= 8) { rex |= 0x01; }
                    emit1(c, rex);
                    emit1(c, 0x0f); emit1(c, 0xb7);
                    emit1(c, (uint8_t)(
                        ((RAX & 7) << 3) | 4));
                    emit1(c, (uint8_t)(
                        0x40
                        | ((RAX & 7) << 3)
                        | (base & 7)));
                    emit1(c, 0);
                }
            } else {
                uint8_t rex = 0x40;
                if (base >= 8) { rex |= 0x01; }
                if (rex != 0x40) { emit1(c, rex); }
                emit1(c, 0x0f); emit1(c, 0xb7);
                int disp = inst->imm * 2;
                if (disp == 0
                        && (base & 7) != RBP) {
                    emit1(c, (uint8_t)(
                        ((RAX & 7) << 3)
                        | (base & 7)));
                    if ((base & 7) == RSP) {
                        emit1(c, 0x24);
                    }
                } else {
                    emit1(c, (uint8_t)(
                        0x40
                        | ((RAX & 7) << 3)
                        | (base & 7)));
                    if ((base & 7) == RSP) {
                        emit1(c, 0x24);
                    }
                    emit1(c, (uint8_t)disp);
                }
            }
            vex(c, 1, 1, 0, 0, s.rd, 0, RAX, 0x6e);
            vbroadcastss(c, s.rd, s.rd);
        } break;

        case op_gather_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int8_t rx = ra_ensure(ra, sl, ns, inst->x);
            int8_t rz = ra_ensure(ra, sl, ns, inst->z);
            int p = inst->ptr;
            int base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr);
            if (scalar) {
                vex(c, 1, 1, 0, 0, rx, 0, RAX, 0x7e);
                if (lu(inst->x) <= i) { ra_free_reg(ra, inst->x); }
                load_max_ix_x86(c, p, 2);
                clamp_eax_x86(c);
                vex_mem(c, 1, 1, 0, 0, s.rd, 0, 0x6e, base, RAX, 4, 0);
            } else {
                int8_t gmask = ra_alloc(ra, sl, ns);
                int8_t clamped = ra_alloc(ra, sl, ns);
                load_max_ix_x86(c, p, 2);
                vpxor(c, 1, gmask, gmask, gmask);
                vpmaxsd(c, clamped, rx, gmask);
                vex(c, 1, 1, 0, 0, gmask, 0, XM, 0x6e);
                vbroadcastss(c, gmask, gmask);
                vpminsd(c, clamped, clamped, gmask);
                vpcmpeqd(c, gmask, gmask, gmask);
                vpgatherdd(c, s.rd, base, clamped, 4, gmask);
                ra_return_reg(ra, clamped);
                ra_return_reg(ra, gmask);
                if (lu(inst->x) <= i) { ra_free_reg(ra, inst->x); }
            }
            vpand(c, 1, s.rd, s.rd, rz);
            if (lu(inst->z) <= i) { ra_free_reg(ra, inst->z); }
        } break;

        case op_gather_16: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int8_t rx = ra_ensure(ra, sl, ns, inst->x);
            int8_t rz = ra_ensure(ra, sl, ns, inst->z);
            int p = inst->ptr;
            int base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr);
            load_max_ix_x86(c, p, 1);
            if (scalar) {
                vex(c, 1, 1, 0, 0, rx, 0, RAX, 0x7e);
                if (lu(inst->x) <= i) { ra_free_reg(ra, inst->x); }
                clamp_eax_x86(c);
                {
                    uint8_t rex = 0x40;
                    if (base >= 8) { rex |= 0x01; }
                    if (rex != 0x40) { emit1(c, rex); }
                    emit1(c, 0x0f);
                    emit1(c, 0xb7u);
                    emit1(c, (uint8_t)(((RAX&7)<<3) | 4));
                    emit1(c, (uint8_t)((1<<6) | ((RAX&7)<<3) | (base&7)));
                }
                vex(c, 1, 1, 0, 0, s.rd, 0, RAX, 0x6e);
            } else {
                int8_t hi_idx = ra_alloc(ra, sl, ns);
                vextracti128(c, hi_idx, rx, 1);
                {
                    sub_ri(c, RSP, 32);
                    for (int k = 0; k < 8; k++) {
                        int src = (k < 4) ? rx : hi_idx;
                        int lane = k & 3;
                        if (lane == 0) {
                            vex(c, 1, 1, 0, 0, src, 0, RAX, 0x7e);
                        } else {
                            vpextrd(c, RAX, src, (uint8_t)lane);
                        }
                        clamp_eax_x86(c);
                        {
                            uint8_t rex2 = 0x40;
                            if (base >= 8) { rex2 |= 0x01; }
                            if (rex2 != 0x40) { emit1(c, rex2); }
                            emit1(c, 0x0f);
                            emit1(c, 0xb7);
                            emit1(c, (uint8_t)(
                                ((RAX&7)<<3) | 4));
                            emit1(c, (uint8_t)(
                                (1<<6)
                                | ((RAX&7)<<3)
                                | (base&7)));
                        }
                        emit1(c, 0x89);
                        if (k == 0) {
                            emit1(c, 0x04); emit1(c, 0x24);
                        } else {
                            emit1(c, 0x44);
                        emit1(c, 0x24);
                        emit1(c, (uint8_t)(k*4));
                        }
                    }
                    vfill(c, s.rd, 0);
                    add_ri(c, RSP, 32);
                    vpand(c, 1, s.rd, s.rd, rz);
                    vextracti128(c, hi_idx, s.rd, 1);
                    vex_rrr(c, 1, 2, 0, 0x2b,
                            s.rd, s.rd, hi_idx);
                }
                ra_return_reg(ra, hi_idx);
                if (lu(inst->x) <= i) { ra_free_reg(ra, inst->x); }
            }
            if (scalar) {
                vpand(c, 0, s.rd, s.rd, rz);
            }
            if (lu(inst->z) <= i) { ra_free_reg(ra, inst->z); }
        } break;

        case op_scatter_32: {
            int8_t rx = ra_ensure(ra, sl, ns, inst->x);
            int8_t ry = ra_ensure(ra, sl, ns, inst->y);
            int8_t rz = ra_ensure(ra, sl, ns, inst->z);
            int p = inst->ptr;
            int base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr);
            if (scalar) {
                vex(c, 1, 1, 0, 0, rz, 0, RAX, 0x7e);
                test_rr(c, RAX, RAX);
                int skip = jcc(c, 0x04);
                vex(c, 1, 1, 0, 0, rx, 0, RAX, 0x7e);
                emit1(c, 0x48); emit1(c, 0x63); emit1(c, 0xc0);
                if (base != R11) { mov_rr(c, R11, base); last_ptr = -1; }
                vex_mem(c, 1, 1, 0, 0, ry, 0, 0x7e, R11, RAX, 4, 0);
                patch_jcc(c, skip);
                if (lu(inst->x) <= i) {
                    ra_free_reg(ra, inst->x);
                }
            } else {
                sub_ri(c, RSP, 96);
                vmov_store(c,1,ry,RSP,RSP,0,0);
                vmov_store(c,1,rx,RSP,RSP,0,32);
                vmov_store(c,1,rz,RSP,RSP,0,64);
                if (lu(inst->x) <= i) {
                    ra_free_reg(ra, inst->x);
                }
                if (base != R11) {
                    mov_rr(c, R11, base);
                    last_ptr = -1;
                }
                for (int k = 0; k < 8; k++) {
                    emit1(c, 0x8b);
                    emit1(c, 0x44);
                    emit1(c, 0x24);
                    emit1(c,(uint8_t)(64+k*4));
                    test_rr(c, RAX, RAX);
                    int skip = jcc(c, 0x04);
                    emit1(c, 0x8b);
                    emit1(c, 0x44);
                    emit1(c, 0x24);
                    emit1(c,(uint8_t)(32+k*4));
                    emit1(c, 0x48);
                    emit1(c, 0x63);
                    emit1(c, 0xc0);
                    emit1(c, 0x44);
                    emit1(c, 0x8b);
                    emit1(c, 0x54);
                    emit1(c, 0x24);
                    emit1(c, (uint8_t)(k*4));
                    emit1(c, 0x45);
                    emit1(c, 0x89);
                    emit1(c, 0x14);
                    emit1(c, 0x83);
                    patch_jcc(c, skip);
                }
                add_ri(c, RSP, 96);
            }
            if (lu(inst->z) <= i) { ra_free_reg(ra, inst->z); }
            if (lu(inst->y) <= i) { ra_free_reg(ra, inst->y); }
        } break;

        case op_scatter_16: {
            int8_t rx = ra_ensure(ra, sl, ns, inst->x);
            int8_t ry = ra_ensure(ra, sl, ns, inst->y);
            int8_t rz = ra_ensure(ra, sl, ns, inst->z);
            if (scalar) {
                int p = inst->ptr;
                int base = resolve_ptr_x86(c, p, &last_ptr, deref_gpr);
                vex(c, 1, 1, 0, 0, rz, 0, RAX, 0x7e);
                test_rr(c, RAX, RAX);
                int skip = jcc(c, 0x04);
                vex(c, 1, 1, 0, 0, rx, 0, RAX, 0x7e);
                emit1(c, 0x48); emit1(c, 0x63); emit1(c, 0xc0);
                if (base != R11) { mov_rr(c, R11, base); last_ptr = -1; }
                emit1(c, 0x4d);
                emit1(c, 0x8d);
                emit1(c, 0x1c);
                emit1(c, 0x43);
                vex(c, 1, 1, 0, 0,
                    ry, 0, RAX, 0x7e);
                emit1(c, 0x66);
                emit1(c, 0x41);
                emit1(c, 0x89);
                emit1(c, 0x03);
                last_ptr = -1;
                patch_jcc(c, skip);
            } else {
            }
            if (lu(inst->z) <= i) { ra_free_reg(ra, inst->z); }
            if (lu(inst->x) <= i) { ra_free_reg(ra, inst->x); }
            if (lu(inst->y) <= i) { ra_free_reg(ra, inst->y); }
        } break;

        case op_widen_f16: {
            struct ra_step s = ra_step_unary(
                ra, sl, ns, inst, i, scalar);
            vcvtph2ps(c, s.rd, s.rx);
        } break;

        case op_narrow_f32: {
            struct ra_step s = ra_step_unary(
                ra, sl, ns, inst, i, scalar);
            vcvtps2ph(c, s.rd, s.rx, 4);
        } break;

        case op_widen_s16: {
            struct ra_step s = ra_step_unary(
                ra, sl, ns, inst, i, scalar);
            vpmovsxwd(c, s.rd, s.rx);
        } break;

        case op_widen_u16: {
            struct ra_step s = ra_step_unary(
                ra, sl, ns, inst, i, scalar);
            vpmovzxwd(c, s.rd, s.rx);
        } break;

        case op_narrow_16: {
            struct ra_step s = ra_step_unary(
                ra, sl, ns, inst, i, scalar);
            if (scalar) {
                vmovaps(c, s.rd, s.rx);
            } else {
                int8_t tmp =
                    ra_alloc(ra, sl, ns);
                vextracti128(c, tmp, s.rx, 1);
                vex_rrr(c, 1, 2, 0, 0x2b,
                    s.rd, s.rx, tmp);
                ra_return_reg(ra, tmp);
            }
        } break;

        case op_imm_32:
        case op_add_f32: case op_sub_f32:
        case op_mul_f32: case op_div_f32:
        case op_min_f32: case op_max_f32:
        case op_sqrt_f32:
        case op_fma_f32: case op_fms_f32:
        case op_add_i32: case op_sub_i32:
        case op_mul_i32:
        case op_shl_i32: case op_shr_u32:
        case op_shr_s32:
        case op_and_32: case op_or_32:
        case op_xor_32: case op_sel_32:
        case op_f32_from_i32:
        case op_i32_from_f32:
        case op_eq_f32: case op_lt_f32:
        case op_le_f32:
        case op_eq_i32: case op_lt_s32:
        case op_le_s32:
        case op_lt_u32: case op_le_u32:
        case op_join:
        case op_and_imm: {
            int nscratch = (inst->op==op_lt_u32) ? 2
                : (inst->op==op_le_s32
                    || inst->op==op_le_u32)
                ? 1 : 0;
            struct ra_step s = ra_step_alu(
                ra, sl, ns, inst,
                i, scalar, nscratch);
            emit_alu_reg(c, inst->op, s.rd, s.rx,
                s.ry, s.rz, inst->imm,
                s.scratch, s.scratch2);
            if (s.scratch >= 0) { ra_return_reg(ra, s.scratch); }
            if (s.scratch2 >= 0) { ra_return_reg(ra, s.scratch2); }
        } break;

        case op_shl_imm:
        case op_shr_u32_imm:
        case op_shr_s32_imm: {
            struct ra_step s = ra_step_unary(
                ra, sl, ns, inst, i, scalar);
            emit_alu_reg(c, inst->op,
                s.rd, s.rx, 0, 0,
                inst->imm, -1, -1);
        } break;

        case op_pack: break;
        }
    }
    #undef lu
}

void umbra_jit_run(struct umbra_jit *j, int n, umbra_buf buf[]) {
    if (!j) { return; }
    j->entry(n, buf);
}

void umbra_jit_free(struct umbra_jit *j) {
    if (!j) { return; }
    munmap(j->code, j->code_size);
    free(j);
}

static _Bool x86_disasm(uint8_t const *code, size_t n,
                        char const *spath, char const *opath, FILE *f) {
    FILE *fp = fopen(spath, "w");
    if (!fp) { return 0; }
    for (size_t i = 0; i < n; i++) { fprintf(fp, ".byte 0x%02x\n", code[i]); }
    fclose(fp);

    char cmd[1024];
    snprintf(cmd, sizeof cmd,
             "/opt/homebrew/opt/llvm/bin/clang"
             " -target x86_64-apple-macos13 -c %s -o %s 2>/dev/null &&"
             " /opt/homebrew/opt/llvm/bin/llvm-objdump"
             " -d --no-show-raw-insn --no-leading-addr %s 2>/dev/null",
             spath, opath, opath);
    FILE *p = popen(cmd, "r");
    if (!p) { return 0; }
    char line[256];
    _Bool ok = 0;
    while (fgets(line, (int)sizeof line, p)) {
        if (!ok && __builtin_strstr(line, "file format")) { ok = 1; continue; }
        if (f) { fputs(line, f); }
    }
    return pclose(p) == 0 && ok;
}

void umbra_dump_jit(struct umbra_jit const *j, FILE *f) {
    if (!j) { return; }
    uint8_t const *code = (uint8_t const *)j->code;
    size_t n = j->code_len;

    char spath[] = "/tmp/umbra_jit_XXXXXX.s";
    int fd = mkstemps(spath, 2);
    if (fd < 0) { goto fallback; }
    close(fd);

    char opath[sizeof spath + 2];
    snprintf(opath, sizeof opath, "%.*s.o", (int)(sizeof spath - 3), spath);

    if (x86_disasm(code, n, spath, opath, f)) {
        remove(spath); remove(opath);
        return;
    }
    remove(spath); remove(opath);

fallback:
    for (size_t i = 0; i < n; i++) {
        fprintf(f, "%02x ", code[i]);
        if ((i & 15) == 15 || i == n-1) { fputc('\n', f); }
    }
}

void umbra_dump_jit_bin(struct umbra_jit const *j, FILE *f) {
    if (!j) { return; }
    fwrite(j->code, 1, j->code_len, f);
}

void umbra_dump_jit_mca(struct umbra_jit const *j, FILE *f) {
    if (!j || j->loop_start >= j->loop_end) { return; }
    uint8_t const *code = (uint8_t const *)j->code;
    size_t n = (size_t)(j->loop_end - j->loop_start);

    char spath[] = "/tmp/umbra_mca_XXXXXX.s";
    int fd = mkstemps(spath, 2);
    if (fd < 0) { return; }
    close(fd);

    char opath[sizeof spath + 2];
    snprintf(opath, sizeof opath, "%.*s.o", (int)(sizeof spath - 3), spath);

    char asmpath[] = "/tmp/umbra_mca_asm_XXXXXX.s";
    int afd = mkstemps(asmpath, 2);
    if (afd < 0) { remove(spath); return; }
    close(afd);
    FILE *afp = fopen(asmpath, "w");
    if (!afp) { remove(spath); remove(asmpath); return; }

    if (!x86_disasm(code + j->loop_start, n, spath, opath, afp)) {
        fclose(afp);
        remove(spath); remove(opath); remove(asmpath);
        return;
    }
    fclose(afp);
    remove(spath); remove(opath);

    char cleanpath[] = "/tmp/umbra_mca_clean_XXXXXX.s";
    int cfd = mkstemps(cleanpath, 2);
    if (cfd < 0) { remove(asmpath); return; }
    FILE *cfp = fdopen(cfd, "w");
    if (!cfp) { close(cfd); remove(asmpath); remove(cleanpath); return; }

    afp = fopen(asmpath, "r");
    if (afp) {
        char line[256];
        _Bool past_header = 0;
        while (fgets(line, (int)sizeof line, afp)) {
            if (!past_header) {
                if (__builtin_strstr(line, "<")) { past_header = 1; }
                continue;
            }
            if (line[0] == '\n' || line[0] == '<') { continue; }
            fputs(line, cfp);
        }
        fclose(afp);
    }
    fclose(cfp);
    remove(asmpath);

    char cmd[1024];
    snprintf(cmd, sizeof cmd,
             "/opt/homebrew/opt/llvm/bin/llvm-mca"
             " -mcpu=znver4"
             " -iterations=100"
             " -bottleneck-analysis"
             " -mtriple=x86_64"
             " %s 2>&1",
             cleanpath);
    FILE *p = popen(cmd, "r");
    if (p) {
        int cplen = (int)__builtin_strlen(cleanpath);
        char line[256];
        while (fgets(line, (int)sizeof line, p)) {
            char *s = line;
            if (__builtin_strncmp(s, cleanpath,
                                  (size_t)cplen) == 0) {
                s += cplen;
            }
            fputs(s, f);
        }
        pclose(p);
    }
    remove(cleanpath);
}

#endif
