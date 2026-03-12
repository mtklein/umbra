#include "../umbra.h"
#include "bb.h"

#if !defined(__aarch64__) && !defined(__AVX2__)

struct umbra_jit { int dummy; };
struct umbra_jit* umbra_jit(struct umbra_basic_block const *bb) { (void)bb; return 0; }
void umbra_jit_run (struct umbra_jit *j, int n, umbra_buf buf[]) {
    (void)j; (void)n; (void)buf;
}
void umbra_jit_free(struct umbra_jit *j) { (void)j; }
void umbra_jit_dump    (struct umbra_jit const *j, FILE *f) { (void)j; (void)f; }
void umbra_jit_dump_bin(struct umbra_jit const *j, FILE *f) { (void)j; (void)f; }
void umbra_jit_mca     (struct umbra_jit const *j, FILE *f) { (void)j; (void)f; }

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

// x0=n, x1..x6=p0..p5, x9=loop i, x10=scratch, x11/x12=byte offsets, x15=stack base
enum { XP=8, XI=9, XT=10, XH=11, XW=12, XS=15 };

// Load buf[p].ptr into XP.  X1 = buf array base, umbra_buf is 16 bytes.
// Skips the load if XP already holds pointer p (tracked by *last_ptr).
static void load_ptr(Buf *c, int p, int *last_ptr) {
    if (*last_ptr == p) return;
    *last_ptr = p;
    int disp = p * 16;
    put(c, 0xF9400020u | ((uint32_t)(disp/8)<<10) | (1u<<5) | (uint32_t)XP);
}

static void vld(Buf *c, int vd, int s) { put(c, LDR_qi(vd, XS, s)); }
static void vst(Buf *c, int vd, int s) { put(c, STR_qi(vd, XS, s)); }

// Register-to-register ALU emission (d,x,y,z are NEON register numbers).
// scratch is a free NEON register for destructive ops (FMLA, variable shift-right).
// At K=8: 32-bit ops use 4S (caller emits twice for lo/hi pairs).
//         16-bit/half ops use 8H (W() promotes 4H->8H) and 16B bitwise.
static _Bool emit_alu_reg(Buf *c, enum op op, int d, int x, int y, int z, int imm, int scratch) {
    switch (op) {
    case op_imm_32: {
        uint32_t v=(uint32_t)imm;
        if      (v == 0)                          { put(c, MOVI_4s(d, 0, 0)); }
        else if (v == (v & 0x000000FFu))          { put(c, MOVI_4s(d, (uint8_t)v,  0)); }
        else if (v == (v & 0x0000FF00u))          { put(c, MOVI_4s(d, (uint8_t)(v>> 8),  8)); }
        else if (v == (v & 0x00FF0000u))          { put(c, MOVI_4s(d, (uint8_t)(v>>16), 16)); }
        else if (v == (v & 0xFF000000u))          { put(c, MOVI_4s(d, (uint8_t)(v>>24), 24)); }
        else if ((~v) == ((~v) & 0x000000FFu))    { put(c, MVNI_4s(d, (uint8_t)~v,  0)); }
        else if ((~v) == ((~v) & 0x0000FF00u))    { put(c, MVNI_4s(d, (uint8_t)(~v>> 8),  8)); }
        else if ((~v) == ((~v) & 0x00FF0000u))    { put(c, MVNI_4s(d, (uint8_t)(~v>>16), 16)); }
        else if ((~v) == ((~v) & 0xFF000000u))    { put(c, MVNI_4s(d, (uint8_t)(~v>>24), 24)); }
        else { load_imm_w(c,XT,v); put(c, DUP_4s_w(d,XT)); }
    } return 1;
    case op_imm_16: {
        uint16_t v=(uint16_t)imm;
        if      (v == 0)                     { put(c, W(MOVI_8h(d, 0, 0))); }
        else if (v == (v & 0x00FFu))         { put(c, W(MOVI_8h(d, (uint8_t)v, 0))); }
        else if (v == (v & 0xFF00u))         { put(c, W(MOVI_8h(d, (uint8_t)(v>>8), 8))); }
        else if ((uint16_t)~v == ((uint16_t)~v & 0x00FFu)) { put(c, W(MVNI_8h(d, (uint8_t)~v, 0))); }
        else if ((uint16_t)~v == ((uint16_t)~v & 0xFF00u)) { put(c, W(MVNI_8h(d, (uint8_t)(~v>>8), 8))); }
        else { put(c, MOVZ_w(XT,v)); put(c, W(DUP_4h_w(d,XT))); }
    } return 1;
    case op_imm_half:
        put(c, MOVZ_w(XT,(uint16_t)imm));
        put(c, W(DUP_4h_w(d,XT)));
        return 1;

    case op_add_f32: put(c, FADD_4s(d,x,y)); return 1;
    case op_sub_f32: put(c, FSUB_4s(d,x,y)); return 1;
    case op_mul_f32: put(c, FMUL_4s(d,x,y)); return 1;
    case op_div_f32: put(c, FDIV_4s(d,x,y)); return 1;
    case op_min_f32: put(c, FMINNM_4s(d,x,y)); return 1;
    case op_max_f32: put(c, FMAXNM_4s(d,x,y)); return 1;
    case op_sqrt_f32: put(c, FSQRT_4s(d,x)); return 1;
    case op_fma_f32:
        if (d==z) { put(c, FMLA_4s(d,x,y)); }
        else if (d!=x && d!=y) { put(c, ORR_16b(d,z,z)); put(c, FMLA_4s(d,x,y)); }
        else { put(c, ORR_16b(scratch,z,z)); put(c, FMLA_4s(scratch,x,y)); put(c, ORR_16b(d,scratch,scratch)); }
        return 1;
    case op_fms_f32:
        if (d==z) { put(c, FMLS_4s(d,x,y)); }
        else if (d!=x && d!=y) { put(c, ORR_16b(d,z,z)); put(c, FMLS_4s(d,x,y)); }
        else { put(c, ORR_16b(scratch,z,z)); put(c, FMLS_4s(scratch,x,y)); put(c, ORR_16b(d,scratch,scratch)); }
        return 1;

    case op_add_i32: put(c, ADD_4s(d,x,y)); return 1;
    case op_sub_i32: put(c, SUB_4s(d,x,y)); return 1;
    case op_mul_i32: put(c, MUL_4s(d,x,y)); return 1;
    case op_shl_i32: put(c, USHL_4s(d,x,y)); return 1;
    case op_shr_u32: put(c, NEG_4s(scratch,y)); put(c, USHL_4s(d,x,scratch)); return 1;
    case op_shr_s32: put(c, NEG_4s(scratch,y)); put(c, SSHL_4s(d,x,scratch)); return 1;
    case op_shl_i32_imm: put(c, SHL_4s_imm(d,x,imm)); return 1;
    case op_shr_u32_imm: put(c, USHR_4s_imm(d,x,imm)); return 1;
    case op_shr_s32_imm: put(c, SSHR_4s_imm(d,x,imm)); return 1;

    case op_and_32: put(c, AND_16b(d,x,y)); return 1;
    case op_or_32:  put(c, ORR_16b(d,x,y)); return 1;
    case op_xor_32: put(c, EOR_16b(d,x,y)); return 1;
    case op_sel_32:
        if (d==x) { put(c, BSL_16b(d,y,z)); }
        else if (d==y) { put(c, BIF_16b(d,z,x)); }
        else if (d==z) { put(c, BIT_16b(d,y,x)); }
        else { put(c, ORR_16b(d,z,z)); put(c, BIT_16b(d,y,x)); }
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

    // 16-bit integer ops: W() promotes 4H->8H for K=8
    case op_add_i16: put(c, W(ADD_4h(d,x,y))); return 1;
    case op_sub_i16: put(c, W(SUB_4h(d,x,y))); return 1;
    case op_mul_i16: put(c, W(MUL_4h(d,x,y))); return 1;
    case op_shl_i16: put(c, W(USHL_4h(d,x,y))); return 1;
    case op_shr_u16: put(c, W(NEG_4h(scratch,y))); put(c, W(USHL_4h(d,x,scratch))); return 1;
    case op_shr_s16: put(c, W(NEG_4h(scratch,y))); put(c, W(SSHL_4h(d,x,scratch))); return 1;
    case op_shl_i16_imm: put(c, W(SHL_4h_imm(d,x,imm))); return 1;
    case op_shr_u16_imm: put(c, W(USHR_4h_imm(d,x,imm))); return 1;
    case op_shr_s16_imm: put(c, W(SSHR_4h_imm(d,x,imm))); return 1;
    // 16-bit and half bitwise: both use 16B ops on full Q register
    case op_and_half: // same encoding as and_16
    case op_and_16: put(c, AND_16b(d,x,y)); return 1;
    case op_or_half:  // same encoding as or_16
    case op_or_16:  put(c, ORR_16b(d,x,y)); return 1;
    case op_xor_half: // same encoding as xor_16
    case op_xor_16: put(c, EOR_16b(d,x,y)); return 1;
    case op_sel_half: // same encoding as sel_16
    case op_sel_16:
        if (d==x) { put(c, BSL_16b(d,y,z)); }
        else if (d==y) { put(c, BIF_16b(d,z,x)); }
        else if (d==z) { put(c, BIT_16b(d,y,x)); }
        else { put(c, ORR_16b(d,z,z)); put(c, BIT_16b(d,y,x)); }
        return 1;
    case op_eq_i16: put(c, W(CMEQ_4h(d,x,y))); return 1;
    case op_lt_s16: put(c, W(CMGT_4h(d,y,x))); return 1;
    case op_le_s16: put(c, W(CMGE_4h(d,y,x))); return 1;
    case op_lt_u16: put(c, W(CMHI_4h(d,y,x))); return 1;
    case op_le_u16: put(c, W(CMHS_4h(d,y,x))); return 1;

    // Half float ops: W() promotes 4H->8H
    case op_add_half: put(c, W(FADD_4h(d,x,y))); return 1;
    case op_sub_half: put(c, W(FSUB_4h(d,x,y))); return 1;
    case op_mul_half: put(c, W(FMUL_4h(d,x,y))); return 1;
    case op_div_half: put(c, W(FDIV_4h(d,x,y))); return 1;
    case op_min_half: put(c, W(FMINNM_4h(d,x,y))); return 1;
    case op_max_half: put(c, W(FMAXNM_4h(d,x,y))); return 1;
    case op_sqrt_half: put(c, W(FSQRT_4h(d,x))); return 1;
    case op_fma_half:
        if (d==z) { put(c, W(FMLA_4h(d,x,y))); }
        else if (d!=x && d!=y) { put(c, ORR_16b(d,z,z)); put(c, W(FMLA_4h(d,x,y))); }
        else { put(c, ORR_16b(scratch,z,z)); put(c, W(FMLA_4h(scratch,x,y))); put(c, ORR_16b(d,scratch,scratch)); }
        return 1;
    case op_fms_half:
        if (d==z) { put(c, W(FMLS_4h(d,x,y))); }
        else if (d!=x && d!=y) { put(c, ORR_16b(d,z,z)); put(c, W(FMLS_4h(d,x,y))); }
        else { put(c, ORR_16b(scratch,z,z)); put(c, W(FMLS_4h(scratch,x,y))); put(c, ORR_16b(d,scratch,scratch)); }
        return 1;
    case op_eq_half: put(c, W(FCMEQ_4h(d,x,y))); return 1;
    case op_lt_half: put(c, W(FCMGT_4h(d,y,x))); return 1;
    case op_le_half: put(c, W(FCMGE_4h(d,y,x))); return 1;

    // Conversions within same width
    case op_half_from_i16: put(c, W(SCVTF_4h(d,x))); return 1;
    case op_i16_from_half: put(c, W(FCVTZS_4h(d,x))); return 1;

    case op_lane:
    case op_uni_32:    case op_load_32:    case op_gather_32:  case op_store_32:  case op_scatter_32:
    case op_uni_16:    case op_load_16:    case op_gather_16:  case op_store_16:  case op_scatter_16:
    case op_uni_half:  case op_load_half:  case op_gather_half: case op_store_half: case op_scatter_half:
    case op_load_8x4:  case op_store_8x4:
    case op_half_from_f32: case op_f32_from_half: case op_half_from_i32: case op_i32_from_half:
    case op_i16_from_i32: case op_i32_from_i16: case op_shr_narrow_u32:
        return 0;
    }
}

// Register allocator: v4-v7,v16-v31 in pool; v0-v3 reserved for LD4/ST4.
static const int8_t ra_pool[] = {4,5,6,7,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31};
#define RA_NREGS 20

static void arm64_spill(int reg, int slot, void *ctx) { vst((Buf*)ctx, reg, slot); }
static void arm64_fill (int reg, int slot, void *ctx) { vld((Buf*)ctx, reg, slot); }

static struct ra* ra_create_arm64(struct umbra_basic_block const *bb, Buf *c) {
    struct ra_config cfg = {
        .pool     = ra_pool,
        .nregs    = RA_NREGS,
        .max_reg  = 32,
        .has_pairs = 1,
        .spill    = arm64_spill,
        .fill     = arm64_fill,
        .ctx      = c,
    };
    return ra_create(bb, &cfg);
}

// Alias for brevity.
#define hi ra_hi

static void emit_ops(Buf *c, struct umbra_basic_block const *bb,
                     int from, int to,
                     int *sl, int *ns, struct ra *ra, _Bool scalar);

struct umbra_jit {
    void  *code;
    size_t code_size;
    void (*entry)(int, umbra_buf*);
    int    loop_start, loop_end;
};

struct umbra_jit* umbra_jit(struct umbra_basic_block const *bb) {
    int *sl = malloc((size_t)bb->insts * sizeof(int));
    for (int i = 0; i < bb->insts; i++) sl[i] = -1;
    int ns = 0;

    Buf c={0};
    struct ra *ra = ra_create_arm64(bb, &c);

    put(&c, STP_pre(29,30,31,-2));
    put(&c, ADD_xi(29,31,0));
    int stack_patch = c.len;
    put(&c, 0xD503201Fu);  // NOP placeholder: patched to SUB SP if spills needed
    put(&c, 0xD503201Fu);  // NOP placeholder: patched to MOV XS,SP if spills needed

    // x0=n, x1=buf[].  Pointers loaded on demand via load_ptr().

    // Preamble: uniforms go straight into registers via RA.
    emit_ops(&c, bb, 0, bb->preamble, sl, &ns, ra, 0);

    ra_begin_loop(ra);

    put(&c, MOVZ_x(XI,0));

    int loop_top = c.len;

    put(&c, 0xCB090000u|(uint32_t)XT);  // SUB X10, X0, X9
    put(&c, SUBS_xi(31,XT,8));           // K=8: need 8 remaining
    int br_tail = c.len;
    put(&c, Bcond(0xB,0));  // B.LT tail (patch later)
    put(&c, LSL_xi(XH, XI, 1));  // x11 = i*2  (half byte offset)
    put(&c, LSL_xi(XW, XI, 2));  // x12 = i*4  (32-bit byte offset)

    // Vector loop: uniforms already in registers from preamble.
    int loop_body_start = c.len;
    emit_ops(&c, bb, bb->preamble, bb->insts, sl, &ns, ra, 0);

    ra_end_loop(ra, sl);

    int loop_body_end = c.len;

    put(&c, ADD_xi(XI,XI,8));   // K=8: advance by 8
    put(&c, B(loop_top - c.len));

    int tail_top = c.len;
    c.buf[br_tail] = Bcond(0xB, tail_top - br_tail);

    put(&c, 0xEB09001Fu);  // SUBS XZR, X0, X9
    int br_epi = c.len;
    put(&c, Bcond(0xD,0));  // B.LE (patch later)

    // Scalar tail: free ALL values (including preamble) and re-emit preamble.
    // The vector loop may have spilled preamble values to stack slots, but when
    // n < K the vector loop never runs, leaving those slots uninitialized.
    // Re-emitting the preamble reloads everything from original sources.
    for (int i = 0; i < bb->insts; i++) ra_free_reg(ra, i);
    for (int i = 0; i < bb->insts; i++) sl[i] = -1;

    // In scalar tail, disable pairs (single-element processing).
    for (int i = bb->preamble; i < bb->insts; i++) ra_set_pair(ra, i, 0);

    emit_ops(&c, bb, 0, bb->preamble, sl, &ns, ra, 0);
    emit_ops(&c, bb, bb->preamble, bb->insts, sl, &ns, ra, 1);

    put(&c, ADD_xi(XI,XI,1));
    put(&c, B(tail_top - c.len));

    int epi = c.len;
    c.buf[br_epi] = Bcond(0xD, epi - br_epi);

    put(&c, ADD_xi(31,29,0));
    put(&c, LDP_post(29,30,31,2));
    put(&c, RET());

    if (ns > 0) {
        c.buf[stack_patch  ] = SUB_xi(31,31,ns*16);
        c.buf[stack_patch+1] = ADD_xi(XS,31,0);
    }
    ra_destroy(ra); free(sl);

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
                     struct ra *ra, _Bool scalar)
{
    #define lu(v) ra_last_use(ra, (v))
    int last_ptr = -1;  // cached pointer index in XP

    for (int i=from; i<to; i++) {
        struct bb_inst const *inst = &bb->inst[i];

        switch (inst->op) {
        case op_lane: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            if (s.rdh >= 0) {
                int8_t tmp = ra_alloc(ra, sl, ns);
                put(c, DUP_4s_w(s.rd, XI));
                put(c, MOVI_4s(tmp, 0, 0));
                put(c, MOVZ_w(XT,1)); put(c, INS_s(tmp,1,XT));
                put(c, MOVZ_w(XT,2)); put(c, INS_s(tmp,2,XT));
                put(c, MOVZ_w(XT,3)); put(c, INS_s(tmp,3,XT));
                put(c, ADD_4s(s.rd, s.rd, tmp));
                put(c, MOVZ_w(XT,4));
                put(c, DUP_4s_w(tmp, XT));
                put(c, ADD_4s(s.rdh, s.rd, tmp));
                ra_return_reg(ra, tmp);
            } else {
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
            }
        } break;

        case op_load_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int p = inst->ptr;
            load_ptr(c, p, &last_ptr);
            if (s.rdh >= 0) {
                put(c, ADD_xr(XT, XP, XW));
                put(c, LDP_qi(s.rd, s.rdh, XT, 0));
            } else {
                if (scalar) put(c, LDR_sx(s.rd, XP, XI));
                else        put(c, LDR_q(s.rd, XP, XW));
            }
        } break;

        case op_load_16: case op_load_half: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int p = inst->ptr;
            load_ptr(c, p, &last_ptr);
            if (scalar) put(c, LDR_hx(s.rd, XP, XI));
            else        put(c, LDR_q(s.rd, XP, XH));
        } break;

        case op_uni_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int p = inst->ptr;
            load_ptr(c, p, &last_ptr);
            load_imm_w(c, XT, (uint32_t)inst->imm);
            put(c, LDR_sx(s.rd, XP, XT));
            put(c, 0x4E040400u | ((uint32_t)s.rd<<5) | (uint32_t)s.rd);
            if (s.rdh >= 0) {
                put(c, 0x4EA01C00u | ((uint32_t)s.rd<<5) | (uint32_t)s.rdh);
            }
        } break;

        case op_uni_16: case op_uni_half: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int p = inst->ptr;
            load_ptr(c, p, &last_ptr);
            load_imm_w(c, XT, (uint32_t)inst->imm);
            put(c, LDR_hx(s.rd, XP, XT));
            put(c, W(0x0E020400u | ((uint32_t)s.rd<<5) | (uint32_t)s.rd));
        } break;

        case op_gather_32: case op_gather_16: case op_gather_half: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            if (scalar) {
                // Extract index from NEON to GPR, then load.
                int8_t rx = ra_ensure(ra, sl, ns, inst->x);
                put(c, UMOV_ws(XT, rx));
                if (lu(inst->x) <= i) ra_free_reg(ra, inst->x);
                int p = inst->ptr;
                load_ptr(c, p, &last_ptr);
                if (inst->op == op_gather_32) {
                    put(c, LDR_sx(s.rd, XP, XT));
                } else {
                    put(c, LDR_hx(s.rd, XP, XT));
                }
            } else {
                if (lu(inst->x) <= i) ra_free_reg(ra, inst->x);
                put(c, MOVI_4s(s.rd, 0, 0));
                if (s.rdh >= 0) {
                    put(c, MOVI_4s(s.rdh, 0, 0));
                }
            }
        } break;

        case op_store_32: {
            int8_t ry = ra_ensure(ra, sl, ns, inst->y);
            int8_t ryh = hi(ra, inst->y);
            int p = inst->ptr;
            load_ptr(c, p, &last_ptr);
            if (!scalar && ryh != ry) {
                put(c, ADD_xr(XT, XP, XW));
                put(c, STP_qi(ry, ryh, XT, 0));
            } else {
                if (scalar) put(c, STR_sx(ry, XP, XI));
                else        put(c, STR_q(ry, XP, XW));
            }
            if (lu(inst->y) <= i) ra_free_reg(ra, inst->y);
        } break;

        case op_store_16: case op_store_half: {
            int8_t ry = ra_ensure(ra, sl, ns, inst->y);
            int p = inst->ptr;
            load_ptr(c, p, &last_ptr);
            if (scalar) put(c, STR_hx(ry, XP, XI));
            else        put(c, STR_q(ry, XP, XH));
            if (lu(inst->y) <= i) ra_free_reg(ra, inst->y);
        } break;

        case op_scatter_32: case op_scatter_16: case op_scatter_half: {
            if (scalar) {
                int8_t rx = ra_ensure(ra, sl, ns, inst->x);
                int8_t ry = ra_ensure(ra, sl, ns, inst->y);
                put(c, UMOV_ws(XT, rx));
                if (lu(inst->x) <= i) ra_free_reg(ra, inst->x);
                int p = inst->ptr;
                load_ptr(c, p, &last_ptr);
                if (inst->op == op_scatter_32) {
                    put(c, STR_sx(ry, XP, XT));
                } else {
                    put(c, STR_hx(ry, XP, XT));
                }
            } else {
                if (lu(inst->x) <= i) ra_free_reg(ra, inst->x);
            }
            if (lu(inst->y) <= i) ra_free_reg(ra, inst->y);
        } break;

        case op_load_8x4: {
            _Bool is_base = inst->x == 0;
            int ch = is_base ? 0 : inst->imm;
            int p  = is_base ? inst->ptr : bb->inst[inst->x].ptr;

            if (scalar) {
                load_ptr(c, p, &last_ptr);
                struct ra_step s = ra_step_alloc(ra, sl, ns, i);
                put(c, LSL_xi(XT, XI, 2));
                if (ch) put(c, ADD_xi(XT, XT, ch));
                // LDRB Wt, [Xn, Xm]
                put(c, 0x38606800u | ((uint32_t)XT << 16) | ((uint32_t)XP << 5) | (uint32_t)XT);
                // DUP Vd.8H, Wt (broadcast byte as u16)
                put(c, W(0x0E020C00u) | ((uint32_t)XT << 5) | (uint32_t)s.rd);
            } else if (is_base) {
                load_ptr(c, p, &last_ptr);
                // V0-V3 are scratch for LD4/ST4, not in the RA pool.
                put(c, ADD_xr(XT, XP, XW));
                put(c, LD4_8b(0, XT));
                // Widen u8->u16 in-place in V0-V3.
                for (int c2 = 0; c2 < 4; c2++) {
                    put(c, UXTL_8h(c2, c2));
                }
                // Determine which channels are needed.
                _Bool ch_needed[] = {1, 0, 0, 0};
                for (int j = i+1; j < to; j++) {
                    if (bb->inst[j].op == op_load_8x4 && bb->inst[j].x == i) {
                        ch_needed[bb->inst[j].imm] = 1;
                    }
                }
                // Copy needed channels from V0-V3 into RA-allocated registers.
                int8_t ch_regs[] = {-1, -1, -1, -1};
                for (int c2 = 0; c2 < 4; c2++) {
                    if (!ch_needed[c2]) continue;
                    ch_regs[c2] = ra_alloc(ra, sl, ns);
                    put(c, ORR_16b(ch_regs[c2], c2, c2));
                }
                // Assign RA ownership.
                ra_assign(ra, i, ch_regs[0]);
                for (int j = i+1; j < to; j++) {
                    if (bb->inst[j].op == op_load_8x4 && bb->inst[j].x == i) {
                        int c2 = bb->inst[j].imm;
                        ra_assign(ra, j, ch_regs[c2]);
                    }
                }
            }
            // Continuation slots: already claimed by base, nothing to emit.
        } break;

        case op_store_8x4: {
            int p = inst->ptr;
            load_ptr(c, p, &last_ptr);
            int const inputs[] = {inst->x, inst->y, inst->z, inst->w};
            if (scalar) {
                for (int ch = 0; ch < 4; ch++) {
                    int8_t ry = ra_ensure(ra, sl, ns, inputs[ch]);
                    put(c, LSL_xi(XT, XI, 2));
                    if (ch) put(c, ADD_xi(XT, XT, ch));
                    put(c, ADD_xr(XT, XP, XT));
                    put(c, ST1_b(ry, 0, XT));
                }
            } else {
                // V0-V3 are scratch for LD4/ST4, not in the RA pool.
                // Copy inputs from RA registers into V0-V3.
                for (int ch = 0; ch < 4; ch++) {
                    int8_t ry = ra_ensure(ra, sl, ns, inputs[ch]);
                    put(c, ORR_16b(ch, ry, ry));
                }
                // Narrow u16->u8: XTN Vd.8B, Vn.8H (in-place)
                for (int ch = 0; ch < 4; ch++) {
                    put(c, XTN_8b(ch, ch));
                }
                put(c, ADD_xr(XT, XP, XW));
                put(c, ST4_8b(0, XT));
            }
            for (int ch = 0; ch < 4; ch++) {
                if (lu(inputs[ch]) <= i) ra_free_reg(ra, inputs[ch]);
            }
        } break;

        // Cross-width conversions between 32-bit (pairs) and 16-bit (single Q)
        case op_half_from_f32: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            if (!scalar && s.rxh != s.rx) {
                put(c, FCVTN_4h(s.rd, s.rx));
                put(c, W(FCVTN_4h(s.rd, s.rxh)));
            } else {
                put(c, FCVTN_4h(s.rd, s.rx));
            }
        } break;

        case op_f32_from_half: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            if (s.rdh >= 0) {
                put(c, FCVTL_4s(s.rd, s.rx));
                put(c, W(FCVTL_4s(s.rdh, s.rx)));
            } else {
                put(c, FCVTL_4s(s.rd, s.rx));
            }
        } break;

        case op_half_from_i32: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            if (!scalar && s.rxh != s.rx) {
                int8_t tmp = ra_alloc(ra, sl, ns);
                put(c, SCVTF_4s(tmp, s.rx));
                put(c, FCVTN_4h(s.rd, tmp));
                put(c, SCVTF_4s(tmp, s.rxh));
                put(c, W(FCVTN_4h(s.rd, tmp)));
                ra_return_reg(ra, tmp);
            } else {
                // Non-pair: use rd as intermediate (NEON reads all before writing).
                put(c, SCVTF_4s(s.rd, s.rx));
                put(c, FCVTN_4h(s.rd, s.rd));
            }
        } break;

        case op_i32_from_half: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            if (s.rdh >= 0) {
                put(c, FCVTL_4s(s.rd, s.rx));
                put(c, FCVTZS_4s(s.rd, s.rd));
                put(c, W(FCVTL_4s(s.rdh, s.rx)));
                put(c, FCVTZS_4s(s.rdh, s.rdh));
            } else {
                // Non-pair: use rd as intermediate (NEON reads all before writing).
                put(c, FCVTL_4s(s.rd, s.rx));
                put(c, FCVTZS_4s(s.rd, s.rd));
            }
        } break;

        case op_i16_from_i32: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            if (!scalar && s.rxh != s.rx) {
                put(c, XTN_4h(s.rd, s.rx));
                put(c, W(XTN_4h(s.rd, s.rxh)));
            } else {
                put(c, XTN_4h(s.rd, s.rx));
            }
        } break;

        case op_i32_from_i16: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            if (s.rdh >= 0) {
                put(c, SXTL_4s(s.rd, s.rx));
                put(c, W(SXTL_4s(s.rdh, s.rx)));
            } else {
                put(c, SXTL_4s(s.rd, s.rx));
            }
        } break;

        case op_shr_narrow_u32: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            int sh = inst->imm;
            int shrn_sh = sh <= 16 ? sh : 16;
            int extra   = sh - shrn_sh;
            if (!scalar && s.rxh != s.rx) {
                put(c, SHRN_4h(s.rd, s.rx, shrn_sh));
                put(c, W(SHRN_4h(s.rd, s.rxh, shrn_sh)));
            } else {
                put(c, SHRN_4h(s.rd, s.rx, shrn_sh));
            }
            if (extra) {
                put(c, W(USHR_4h_imm(s.rd, s.rd, extra)));
            }
        } break;

        // Compare-against-zero: use 2-operand forms when one input is imm=0.
        case op_eq_f32: case op_lt_f32: case op_le_f32:
        case op_eq_i32: case op_lt_s32: case op_le_s32:
        case op_eq_i16: case op_lt_s16: case op_le_s16:
        case op_eq_half: case op_lt_half: case op_le_half: {
            // Check if x or y is an immediate zero.
            _Bool x_is_0 = (bb->inst[inst->x].op == op_imm_32   && bb->inst[inst->x].imm == 0)
                        || (bb->inst[inst->x].op == op_imm_16   && bb->inst[inst->x].imm == 0)
                        || (bb->inst[inst->x].op == op_imm_half && bb->inst[inst->x].imm == 0);
            _Bool y_is_0 = (bb->inst[inst->y].op == op_imm_32   && bb->inst[inst->y].imm == 0)
                        || (bb->inst[inst->y].op == op_imm_16   && bb->inst[inst->y].imm == 0)
                        || (bb->inst[inst->y].op == op_imm_half && bb->inst[inst->y].imm == 0);

            if (x_is_0 || y_is_0) {
                // Unary compare against zero: only need one input register.
                int val = x_is_0 ? inst->y : inst->x;
                int8_t rv = ra_ensure(ra, sl, ns, val);
                int8_t rvh = hi(ra, val);
                _Bool v_dead = lu(val) <= i;

                int8_t rd;
                _Bool pair = ra_is_pair(ra, i) && !scalar;
                if (v_dead) {
                    rd = ra_claim(ra, val, i);
                } else {
                    rd = ra_alloc(ra, sl, ns);
                    ra_assign(ra, i, rd);
                }
                int8_t rdh = -1;
                if (pair) {
                    rdh = ra_reg_hi(ra, i);
                    if (rdh < 0) {
                        rdh = ra_alloc(ra, sl, ns);
                        ra_assign_hi(ra, i, rdh);
                    }
                }

                // Emit the compare-against-zero instruction.
                // x_is_0: cmp(0,y) — y is non-zero, "zero is on the left"
                // y_is_0: cmp(x,0) — x is non-zero, "zero is on the right"
                #define CZ(op_name, z_right, z_left) \
                    case op_name: put(c, y_is_0 ? z_right(rd, rv) : z_left(rd, rv)); \
                                  if (rdh>=0) put(c, y_is_0 ? z_right(rdh,rvh) : z_left(rdh,rvh)); \
                                  break;
                #define CZh(op_name, z_right, z_left) \
                    case op_name: put(c, y_is_0 ? W(z_right(rd, rv)) : W(z_left(rd, rv))); break;

                switch (inst->op) {
                // eq is symmetric, both sides are the same
                CZ(op_eq_f32,  FCMEQ_4s_z, FCMEQ_4s_z)
                CZ(op_eq_i32,  CMEQ_4s_z,  CMEQ_4s_z)
                // lt(x,0) = x<0 → CMLT; lt(0,y) = 0<y = y>0 → CMGT
                CZ(op_lt_f32,  FCMLT_4s_z, FCMGT_4s_z)
                CZ(op_lt_s32,  CMLT_4s_z,  CMGT_4s_z)
                // le(x,0) = x<=0 → CMLE; le(0,y) = 0<=y = y>=0 → CMGE
                CZ(op_le_f32,  FCMLE_4s_z, FCMGE_4s_z)
                CZ(op_le_s32,  CMLE_4s_z,  CMGE_4s_z)
                // 16-bit
                CZh(op_eq_i16, CMEQ_4h_z,  CMEQ_4h_z)
                CZh(op_lt_s16, CMLT_4h_z,  CMGT_4h_z)
                CZh(op_le_s16, CMLE_4h_z,  CMGE_4h_z)
                // half
                CZh(op_eq_half, FCMEQ_4h_z, FCMEQ_4h_z)
                CZh(op_lt_half, FCMLT_4h_z, FCMGT_4h_z)
                CZh(op_le_half, FCMLE_4h_z, FCMGE_4h_z)
                case op_lane:
                case op_imm_32: case op_uni_32: case op_load_32: case op_gather_32: case op_store_32: case op_scatter_32:
                case op_add_f32: case op_sub_f32: case op_mul_f32: case op_div_f32:
                case op_min_f32: case op_max_f32: case op_sqrt_f32: case op_fma_f32: case op_fms_f32:
                case op_add_i32: case op_sub_i32: case op_mul_i32:
                case op_shl_i32: case op_shr_u32: case op_shr_s32:
                case op_shl_i32_imm: case op_shr_u32_imm: case op_shr_s32_imm:
                case op_and_32: case op_or_32: case op_xor_32: case op_sel_32:
                case op_f32_from_i32: case op_i32_from_f32: case op_f32_from_half:
                case op_i32_from_half: case op_i32_from_i16:
                case op_lt_u32: case op_le_u32:
                case op_imm_16: case op_uni_16: case op_load_16: case op_gather_16: case op_store_16: case op_scatter_16:
                case op_load_8x4: case op_store_8x4:
                case op_add_i16: case op_sub_i16: case op_mul_i16:
                case op_shl_i16: case op_shr_u16: case op_shr_s16:
                case op_shl_i16_imm: case op_shr_u16_imm: case op_shr_s16_imm:
                case op_and_16: case op_or_16: case op_xor_16: case op_sel_16:
                case op_i16_from_i32: case op_shr_narrow_u32:
                case op_lt_u16: case op_le_u16:
                case op_imm_half: case op_uni_half: case op_load_half: case op_gather_half: case op_store_half: case op_scatter_half:
                case op_add_half: case op_sub_half: case op_mul_half: case op_div_half:
                case op_min_half: case op_max_half: case op_sqrt_half: case op_fma_half: case op_fms_half:
                case op_and_half: case op_or_half: case op_xor_half: case op_sel_half:
                case op_half_from_f32: case op_half_from_i32: case op_half_from_i16: case op_i16_from_half:
                    break;
                }
                #undef CZ
                #undef CZh

                // Free the zero input if it's dead.
                int zero_val = x_is_0 ? inst->x : inst->y;
                if (lu(zero_val) <= i) ra_free_reg(ra, zero_val);
                break;
            }
            goto default_alu;
        }

        case op_imm_32: case op_imm_16: case op_imm_half:
        case op_add_f32: case op_sub_f32: case op_mul_f32: case op_div_f32:
        case op_min_f32: case op_max_f32: case op_sqrt_f32: case op_fma_f32: case op_fms_f32:
        case op_add_i32: case op_sub_i32: case op_mul_i32:
        case op_shl_i32: case op_shr_u32: case op_shr_s32:
        case op_shl_i32_imm: case op_shr_u32_imm: case op_shr_s32_imm:
        case op_and_32: case op_or_32: case op_xor_32: case op_sel_32:
        case op_f32_from_i32: case op_i32_from_f32:
        case op_lt_u32: case op_le_u32:
        case op_add_i16: case op_sub_i16: case op_mul_i16:
        case op_shl_i16: case op_shr_u16: case op_shr_s16:
        case op_shl_i16_imm: case op_shr_u16_imm: case op_shr_s16_imm:
        case op_and_16: case op_or_16: case op_xor_16: case op_sel_16:
        case op_lt_u16: case op_le_u16:
        case op_add_half: case op_sub_half: case op_mul_half: case op_div_half:
        case op_min_half: case op_max_half: case op_sqrt_half: case op_fma_half: case op_fms_half:
        case op_and_half: case op_or_half: case op_xor_half: case op_sel_half:
        case op_half_from_i16: case op_i16_from_half:
        default_alu: {
            enum op op2 = inst->op;
            int nscratch = (op2==op_shr_u32 || op2==op_shr_s32
                         || op2==op_shr_u16 || op2==op_shr_s16) ? 1 : 0;
            struct ra_step s = ra_step_alu(ra, sl, ns, inst, i, scalar, nscratch);

            emit_alu_reg(c, inst->op, s.rd, s.rx, s.ry, s.rz, inst->imm, s.scratch);
            if (s.rdh >= 0) {
                emit_alu_reg(c, inst->op, s.rdh, s.rxh, s.ryh, s.rzh, inst->imm, s.scratch);
            }
            if (s.scratch >= 0) {
                ra_return_reg(ra, s.scratch);
            }
        } break;
        }
    }
    #undef lu
}

void umbra_jit_run(struct umbra_jit *j, int n, umbra_buf buf[]) {
    if (!j) return;
    j->entry(n, buf);
}

void umbra_jit_free(struct umbra_jit *j) {
    if (!j) return;
    munmap(j->code, j->code_size);
    free(j);
}

void umbra_jit_dump(struct umbra_jit const *j, FILE *f) {
    if (!j) return;
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
                     "/opt/homebrew/opt/llvm/bin/llvm-objdump -d --no-show-raw-insn --no-leading-addr %s 2>/dev/null",
                     opath, tmp, opath);
            FILE *p = popen(cmd, "r");
            if (p) {
                char line[256];
                _Bool ok = 0;
                while (fgets(line, (int)sizeof line, p)) {
                    if (!ok && __builtin_strstr(line, "file format")) { ok = 1; continue; }
                    fputs(line, f);
                }
                int rc = pclose(p);
                remove(tmp);
                remove(opath);
                if (ok && rc == 0) return;
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

void umbra_jit_dump_bin(struct umbra_jit const *j, FILE *f) {
    if (!j) return;
    fwrite(j->code, 1, j->code_size, f);
}

void umbra_jit_mca(struct umbra_jit const *j, FILE *f) {
    if (!j || j->loop_start >= j->loop_end) return;
    uint32_t const *words = (uint32_t const *)j->code;

    char tmp[] = "/tmp/umbra_mca_XXXXXX.s";
    int fd = mkstemps(tmp, 2);
    if (fd < 0) return;
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
             "/opt/homebrew/opt/llvm/bin/llvm-objdump -d --no-show-raw-insn --no-leading-addr %s 2>/dev/null",
             opath, tmp, opath);
    FILE *p = popen(cmd, "r");
    if (!p) { close(afd); remove(tmp); remove(opath); remove(asm_path); return; }

    FILE *afp = fdopen(afd, "w");
    char line[256];
    _Bool past_header = 0;
    while (fgets(line, (int)sizeof line, p)) {
        if (!past_header) {
            if (__builtin_strstr(line, "<")) past_header = 1;
            continue;
        }
        if (line[0] == '\n' || line[0] == '<') continue;
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

// x0=RDI(n), x1=RSI(buf[])
// R10=loop counter, R11/RAX=scratch
enum { XI=R10 };  // loop counter

// Load buf[p].ptr into R11.  RSI = buf array base, umbra_buf is 16 bytes.
// Skips the load if R11 already holds pointer p (tracked by *last_ptr).
static int load_ptr_x86(Buf *c, int p, int *last_ptr) {
    if (*last_ptr != p) {
        *last_ptr = p;
        mov_load(c, R11, RSI, p * 16);
    }
    return R11;
}

// ---- Register allocator ----
static const int8_t ra_pool[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
#define RA_NREGS 16

static void x86_spill(int reg, int slot, void *ctx) { vspill((Buf*)ctx, reg, slot); }
static void x86_fill (int reg, int slot, void *ctx) { vfill ((Buf*)ctx, reg, slot); }

static struct ra* ra_create_x86(struct umbra_basic_block const *bb, Buf *c) {
    struct ra_config cfg = {
        .pool     = ra_pool,
        .nregs    = RA_NREGS,
        .max_reg  = 16,
        .has_pairs = 0,
        .spill    = x86_spill,
        .fill     = x86_fill,
        .ctx      = c,
    };
    return ra_create(bb, &cfg);
}

// ---- ALU emission ----
// d,x,y,z are YMM/XMM register numbers.  scratch/scratch2 are free registers (-1 if unused).
static _Bool emit_alu_reg(Buf *c, enum op op, int d, int x, int y, int z, int imm,
                          int scratch, int scratch2) {
    switch (op) {
    case op_imm_32: broadcast_imm32(c, d, (uint32_t)imm); return 1;
    case op_imm_16: broadcast_imm16(c, d, (uint16_t)imm); return 1;
    case op_imm_half: broadcast_half_imm(c, d, (uint16_t)imm); return 1;

    // f32 arithmetic (YMM)
    case op_add_f32: vaddps(c,d,x,y); return 1;
    case op_sub_f32: vsubps(c,d,x,y); return 1;
    case op_mul_f32: vmulps(c,d,x,y); return 1;
    case op_div_f32: vdivps(c,d,x,y); return 1;
    case op_min_f32: vminps(c,d,x,y); return 1;
    case op_max_f32: vmaxps(c,d,x,y); return 1;
    case op_sqrt_f32: vsqrtps(c,d,x); return 1;
    // fma(x,y,z) = x*y + z
    case op_fma_f32:
        if      (d == x) { vfmadd132ps(c,d,z,y); }  // d = d*y + z
        else if (d == y) { vfmadd213ps(c,d,x,z); }  // d = x*d + z
        else if (d == z) { vfmadd231ps(c,d,x,y); }  // d = x*y + d
        else             { vmovaps(c,d,z); vfmadd231ps(c,d,x,y); }
        return 1;
    // fms(x,y,z) = z - x*y
    case op_fms_f32:
        if      (d == x) { vfnmadd132ps(c,d,z,y); }  // d = -(d*y) + z
        else if (d == y) { vfnmadd213ps(c,d,x,z); }  // d = -(x*d) + z
        else if (d == z) { vfnmadd231ps(c,d,x,y); }  // d = -(x*y) + d
        else             { vmovaps(c,d,z); vfnmadd231ps(c,d,x,y); }
        return 1;

    // i32 arithmetic (YMM)
    case op_add_i32: vpaddd(c,d,x,y); return 1;
    case op_sub_i32: vpsubd(c,d,x,y); return 1;
    case op_mul_i32: vpmulld(c,d,x,y); return 1;
    case op_shl_i32: vpsllvd(c,d,x,y); return 1;
    case op_shr_u32: vpsrlvd(c,d,x,y); return 1;
    case op_shr_s32: vpsravd(c,d,x,y); return 1;
    case op_shl_i32_imm: vpslld_i(c,d,x,(uint8_t)imm); return 1;
    case op_shr_u32_imm: vpsrld_i(c,d,x,(uint8_t)imm); return 1;
    case op_shr_s32_imm: vpsrad_i(c,d,x,(uint8_t)imm); return 1;

    // bitwise 32 and half (both YMM, same encoding)
    case op_and_half: // promoted to f32, same as and_32
    case op_and_32: vpand(c,1,d,x,y); return 1;
    case op_or_half:  // promoted to f32, same as or_32
    case op_or_32:  vpor(c,1,d,x,y);  return 1;
    case op_xor_half: // promoted to f32, same as xor_32
    case op_xor_32: vpxor_3(c,1,d,x,y); return 1;
    case op_sel_half: // promoted to f32, same as sel_32
    case op_sel_32: vpblendvb(c,1,d,z,y,x); return 1;

    // conversions
    case op_f32_from_i32: vcvtdq2ps(c,d,x); return 1;
    case op_i32_from_f32: vcvttps2dq(c,d,x); return 1;

    // f32 compare (YMM) — produces all-1/all-0 mask
    case op_eq_f32: vcmpps(c,d,x,y,0);  return 1;  // EQ_OQ
    case op_lt_f32: vcmpps(c,d,x,y,1);  return 1;  // LT_OS
    case op_le_f32: vcmpps(c,d,x,y,2);  return 1;  // LE_OS

    // i32 compare
    case op_eq_i32: vpcmpeqd(c,d,x,y); return 1;
    case op_lt_s32: vpcmpgtd(c,d,y,x); return 1;
    case op_le_s32:
        vpcmpgtd(c,d,x,y);
        vpcmpeqd(c,scratch,scratch,scratch);
        vpxor_3(c,1,d,d,scratch);
        return 1;
    case op_lt_u32:  // x <u y  ≡  ¬(y == min_u(x,y))
        vex_rrr(c,1,2,1,0x3B,scratch2,x,y);  // VPMINUD scratch2, x, y
        vpcmpeqd(c,d,y,scratch2);
        vpcmpeqd(c,scratch,scratch,scratch);
        vpxor_3(c,1,d,d,scratch);
        return 1;
    case op_le_u32:  // x <=u y  ≡  y == max_u(x,y)
        vex_rrr(c,1,2,1,0x3F,scratch,x,y);   // VPMAXUD scratch, x, y
        vpcmpeqd(c,d,y,scratch);
        return 1;

    // ---- i16 arithmetic (XMM, L=0) ----
    case op_add_i16: vpaddw(c,d,x,y); return 1;
    case op_sub_i16: vpsubw(c,d,x,y); return 1;
    case op_mul_i16: vpmullw(c,d,x,y); return 1;
    case op_shl_i16_imm: vpsllw_i(c,d,x,(uint8_t)imm); return 1;
    case op_shr_u16_imm: vpsrlw_i(c,d,x,(uint8_t)imm); return 1;
    case op_shr_s16_imm: vpsraw_i(c,d,x,(uint8_t)imm); return 1;

    // Variable i16 shifts: widen to i32, shift, narrow back via 128-bit pack
    case op_shl_i16:
        vpmovzxwd(c,scratch,x);               // scratch = 8×i32 data (zero-extended)
        vpmovzxwd(c,scratch2,y);              // scratch2 = 8×i32 shifts
        vpsllvd(c,scratch,scratch,scratch2);  // scratch = shifted results
        vextracti128(c,scratch2,scratch,1);   // scratch2 = hi 4 dwords
        // VPACKUSDW xmm_d, xmm_scratch, xmm_scratch2
        vex_rrr(c,1,2,0,0x2B,d,scratch,scratch2);
        return 1;
    case op_shr_u16:
        vpmovzxwd(c,scratch,x);
        vpmovzxwd(c,scratch2,y);
        vpsrlvd(c,scratch,scratch,scratch2);
        vextracti128(c,scratch2,scratch,1);
        vex_rrr(c,1,2,0,0x2B,d,scratch,scratch2);
        return 1;
    case op_shr_s16:
        vpmovsxwd(c,scratch,x);              // sign-extend for signed shift
        vpmovzxwd(c,scratch2,y);
        vpsravd(c,scratch,scratch,scratch2);
        vextracti128(c,scratch2,scratch,1);
        vex_rrr(c,1,1,0,0x6B,d,scratch,scratch2);  // VPACKSSDW xmm
        return 1;

    // i16 bitwise (XMM)
    case op_and_16: vpand(c,0,d,x,y); return 1;
    case op_or_16:  vpor(c,0,d,x,y);  return 1;
    case op_xor_16: vpxor_3(c,0,d,x,y); return 1;
    case op_sel_16: vpblendvb(c,0,d,z,y,x); return 1;

    // i16 compare (XMM)
    case op_eq_i16: vpcmpeqw(c,d,x,y); return 1;
    case op_lt_s16: vpcmpgtw(c,d,y,x); return 1;
    case op_le_s16:
        vpcmpgtw(c,d,x,y);
        vpcmpeqw(c,scratch,scratch,scratch);
        vpxor_3(c,0,d,d,scratch);
        return 1;
    case op_lt_u16:  // x <u y  ≡  ¬(y == min_u(x,y))
        vex_rrr(c,1,2,0,0x3A,scratch2,x,y);  // VPMINUW scratch2, x, y
        vpcmpeqw(c,d,y,scratch2);
        vpcmpeqw(c,scratch,scratch,scratch);
        vpxor_3(c,0,d,d,scratch);
        return 1;
    case op_le_u16:  // x <=u y  ≡  y == max_u(x,y)
        vex_rrr(c,1,2,0,0x3E,scratch,x,y);   // VPMAXUW scratch, x, y
        vpcmpeqw(c,d,y,scratch);
        return 1;

    // ---- Half ops: carried as f32 in YMM, arithmetic uses f32 instructions ----
    case op_add_half: vaddps(c,d,x,y); return 1;
    case op_sub_half: vsubps(c,d,x,y); return 1;
    case op_mul_half: vmulps(c,d,x,y); return 1;
    case op_div_half: vdivps(c,d,x,y); return 1;
    case op_min_half: vminps(c,d,x,y); return 1;
    case op_max_half: vmaxps(c,d,x,y); return 1;
    case op_sqrt_half: vsqrtps(c,d,x); return 1;
    case op_fma_half:
        if      (d == x) { vfmadd132ps(c,d,z,y); }
        else if (d == y) { vfmadd213ps(c,d,x,z); }
        else if (d == z) { vfmadd231ps(c,d,x,y); }
        else             { vmovaps(c,d,z); vfmadd231ps(c,d,x,y); }
        return 1;
    case op_fms_half:
        if      (d == x) { vfnmadd132ps(c,d,z,y); }
        else if (d == y) { vfnmadd213ps(c,d,x,z); }
        else if (d == z) { vfnmadd231ps(c,d,x,y); }
        else             { vmovaps(c,d,z); vfnmadd231ps(c,d,x,y); }
        return 1;

    // Half bitwise: handled above with 32-bit bitwise (both YMM)

    // Half compare: use f32 compare
    case op_eq_half: vcmpps(c,d,x,y,0); return 1;
    case op_lt_half: vcmpps(c,d,x,y,1); return 1;
    case op_le_half: vcmpps(c,d,x,y,2); return 1;

    // ---- Cross-width conversions ----
    // half_from_i16: i16 in XMM → f32 in YMM (sign-extend to i32, then cvt)
    case op_half_from_i16:
        vpmovsxwd(c,d,x);      // sign-extend 8×i16 → 8×i32 in YMM
        vcvtdq2ps(c,d,d);      // i32 → f32
        return 1;
    // i16_from_half: f32 in YMM → i16 in XMM
    case op_i16_from_half:
        vcvttps2dq(c,d,x);     // f32 → i32
        // Pack i32 to i16: extract hi128, pack both halves
        vextracti128(c,scratch,d,1);
        vex_rrr(c,1,1,0,0x6B,d,d,scratch);  // VPACKSSDW xmm
        return 1;

    case op_lane:
    case op_uni_32:    case op_load_32:    case op_gather_32:  case op_store_32:  case op_scatter_32:
    case op_uni_16:    case op_load_16:    case op_gather_16:  case op_store_16:  case op_scatter_16:
    case op_uni_half:  case op_load_half:  case op_gather_half: case op_store_half: case op_scatter_half:
    case op_load_8x4:  case op_store_8x4:
    case op_half_from_f32: case op_f32_from_half: case op_half_from_i32: case op_i32_from_half:
    case op_i16_from_i32: case op_i32_from_i16: case op_shr_narrow_u32:
        return 0;
    }
}

// ---- emit_ops ----
static void emit_ops(Buf *c, struct umbra_basic_block const *bb,
                     int from, int to,
                     int *sl, int *ns, struct ra *ra, _Bool scalar);

struct umbra_jit {
    void  *code;
    size_t code_size, code_len;
    void (*entry)(int, umbra_buf*);
    int    loop_start, loop_end;
};

struct umbra_jit* umbra_jit(struct umbra_basic_block const *bb) {
    int *sl = malloc((size_t)bb->insts * sizeof(int));
    for (int i = 0; i < bb->insts; i++) sl[i] = -1;
    int ns = 0;

    Buf c = {0};
    struct ra *ra = ra_create_x86(bb, &c);

    // SysV ABI: RDI=n, RSI=buf[].  Pointers loaded on demand via load_ptr_x86().

    // Reserve stack space for spills (patched later)
    int stack_patch = c.len;
    // Placeholder: SUB RSP, imm32 (7 bytes: REX.W + 81 /5 + imm32)
    for (int i = 0; i < 7; i++) nop(&c);

    // Preamble
    emit_ops(&c, bb, 0, bb->preamble, sl, &ns, ra, 0);

    ra_begin_loop(ra);

    // Loop counter = 0
    xor_rr(&c, XI, XI);

    int loop_top = c.len;

    // Remaining = n - i; need K=8
    mov_rr(&c, R11, RDI);
    // SUB R11, R10
    rex_w(&c, XI, R11);
    emit1(&c, 0x29);
    emit1(&c, (uint8_t)(0xC0 | ((XI&7)<<3) | (R11&7)));

    cmp_ri(&c, R11, 8);
    int br_tail = jcc(&c, 0x0C);  // JL (SF!=OF)

    int loop_body_start = c.len;
    emit_ops(&c, bb, bb->preamble, bb->insts, sl, &ns, ra, 0);

    ra_end_loop(ra, sl);

    int loop_body_end = c.len;

    add_ri(&c, XI, 8);
    int jmp_top = jmp(&c);
    {   // Patch JMP to loop_top (backward jump)
        int32_t rel = (int32_t)(loop_top - (jmp_top + 4));
        __builtin_memcpy(c.buf + jmp_top, &rel, 4);
    }

    // Scalar tail — patch the branch-to-tail forward jump
    int tail_top = c.len;
    {
        int32_t rel = (int32_t)(tail_top - (br_tail + 4));
        __builtin_memcpy(c.buf + br_tail, &rel, 4);
    }

    // Check if any elements remain
    cmp_rr(&c, XI, RDI);
    int br_epi = jcc(&c, 0x0D);  // JGE

    // Scalar tail: free ALL values (including preamble) and re-emit preamble.
    // The vector loop may have spilled preamble values to stack slots, but when
    // n < K the vector loop never runs, leaving those slots uninitialized.
    // Re-emitting the preamble reloads everything from original sources.
    for (int i = 0; i < bb->insts; i++) ra_free_reg(ra, i);
    for (int i = 0; i < bb->insts; i++) sl[i] = -1;

    emit_ops(&c, bb, 0, bb->preamble, sl, &ns, ra, 0);
    emit_ops(&c, bb, bb->preamble, bb->insts, sl, &ns, ra, 1);

    add_ri(&c, XI, 1);
    {
        int j = jmp(&c);
        int32_t rel = (int32_t)(tail_top - (j + 4));
        __builtin_memcpy(c.buf + j, &rel, 4);
    }

    // Epilogue
    int epi = c.len;
    {
        int32_t rel = (int32_t)(epi - (br_epi + 4));
        __builtin_memcpy(c.buf + br_epi, &rel, 4);
    }

    // Restore stack and callee-saved regs
    if (ns > 0) {
        add_ri(&c, RSP, ns * 32);
    }
    vzeroupper(&c);
    ret(&c);

    // Patch stack allocation
    if (ns > 0) {
        int pos = stack_patch;
        c.buf[pos++] = 0x48;  // REX.W
        c.buf[pos++] = 0x81;
        c.buf[pos++] = (uint8_t)(0xC0 | (5<<3) | (RSP&7));  // SUB RSP, imm32
        int32_t sz = ns * 32;
        __builtin_memcpy(c.buf + pos, &sz, 4);
    }

    ra_destroy(ra); free(sl);

    size_t code_sz = (size_t)c.len;
    size_t pg = (size_t)sysconf(_SC_PAGESIZE);
    size_t alloc = (code_sz + pg-1) & ~(pg-1);

    void *mem = mmap(NULL, alloc, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
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
                     struct ra *ra, _Bool scalar)
{
    #define lu(v) ra_last_use(ra, (v))
    int last_ptr = -1;  // cached pointer index in R11

    for (int i = from; i < to; i++) {
        struct bb_inst const *inst = &bb->inst[i];
        switch (inst->op) {
        case op_lane: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            if (scalar) {
                vex(c, 1, 1, 0, 0, s.rd, 0, XI, 0x6E);
            } else {
                vex(c, 1, 1, 0, 0, s.rd, 0, XI, 0x6E);
                vbroadcastss(c, s.rd, s.rd);
                int8_t tmp = ra_alloc(ra, sl, ns);
                sub_ri(c, RSP, 32);
                for (int k = 0; k < 8; k++) {
                    emit1(c, 0xC7);
                    if (k == 0) {
                        emit1(c, 0x04); emit1(c, 0x24);
                    } else {
                        emit1(c, 0x44); emit1(c, 0x24); emit1(c, (uint8_t)(k*4));
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
            if (scalar) {
                vex_mem(c, 1, 1, 0, 0, s.rd, 0, 0x6E, load_ptr_x86(c, p, &last_ptr), XI, 4, 0);
            } else {
                vmov_load(c, 1, s.rd, load_ptr_x86(c, p, &last_ptr), XI, 4, 0);
            }
        } break;

        case op_load_16: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int p = inst->ptr;
            if (scalar) {
                // MOVZX eax, word [base + R10*2]; VMOVD xmm, eax
                {
                    int base = load_ptr_x86(c, p, &last_ptr);
                    uint8_t rex = 0x40;
                    if (XI >= 8) rex |= 0x02;
                    if (base >= 8) rex |= 0x01;
                    if (rex != 0x40) emit1(c, rex);
                    emit1(c, 0x0F); emit1(c, 0xB7);
                    emit1(c, (uint8_t)(((RAX&7)<<3) | 4));
                    emit1(c, (uint8_t)((1<<6) | ((XI&7)<<3) | (base&7)));
                }
                vex(c, 1, 1, 0, 0, s.rd, 0, RAX, 0x6E);
            } else {
                // VMOVDQU xmm, [base + R10*2] — 8 × 16-bit = 16 bytes
                vmov_load(c, 0, s.rd, load_ptr_x86(c, p, &last_ptr), XI, 2, 0);
            }
        } break;

        case op_load_half: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int p = inst->ptr;
            if (scalar) {
                // Load single fp16, convert to f32
                // MOVZX eax, word [base + R10*2]
                {
                    int base = load_ptr_x86(c, p, &last_ptr);
                    uint8_t rex = 0x40;
                    if (XI >= 8) rex |= 0x02;
                    if (base >= 8) rex |= 0x01;
                    if (rex != 0x40) emit1(c, rex);
                    emit1(c, 0x0F); emit1(c, 0xB7);
                    emit1(c, (uint8_t)(((RAX&7)<<3) | 4));
                    emit1(c, (uint8_t)((1<<6) | ((XI&7)<<3) | (base&7)));
                }
                int8_t tmp = ra_alloc(ra, sl, ns);
                vex(c, 1, 1, 0, 0, tmp, 0, RAX, 0x6E);  // VMOVD xmm_tmp, eax
                vcvtph2ps(c, s.rd, tmp);                  // VCVTPH2PS xmm_rd, xmm_tmp
                ra_return_reg(ra, tmp);
            } else {
                // Load 8 × fp16 = 16 bytes into xmm, then VCVTPH2PS to ymm
                int8_t tmp = ra_alloc(ra, sl, ns);
                vmov_load(c, 0, tmp, load_ptr_x86(c, p, &last_ptr), XI, 2, 0);
                vcvtph2ps(c, s.rd, tmp);
                ra_return_reg(ra, tmp);
            }
        } break;

        case op_store_32: {
            int8_t ry = ra_ensure(ra, sl, ns, inst->y);
            int p = inst->ptr;
            if (scalar) {
                // VMOVD [base + R10*4], xmm
                vex_mem(c, 1, 1, 0, 0, ry, 0, 0x7E, load_ptr_x86(c, p, &last_ptr), XI, 4, 0);
            } else {
                vmov_store(c, 1, ry, load_ptr_x86(c, p, &last_ptr), XI, 4, 0);
            }
            if (lu(inst->y) <= i) ra_free_reg(ra, inst->y);
        } break;

        case op_store_16: {
            int8_t ry = ra_ensure(ra, sl, ns, inst->y);
            int p = inst->ptr;
            if (scalar) {
                // VMOVD eax, xmm; MOV word [base + R10*2], ax
                vex(c, 1, 1, 0, 0, ry, 0, RAX, 0x7E);  // VMOVD eax, xmm
                // MOV [base + R10*2], ax (16-bit store)
                {
                    int base = load_ptr_x86(c, p, &last_ptr);
                    emit1(c, 0x66);  // operand-size prefix for 16-bit
                    uint8_t rex = 0x40;
                    if (XI >= 8) rex |= 0x02;
                    if (base >= 8) rex |= 0x01;
                    if (rex != 0x40) emit1(c, rex);
                    emit1(c, 0x89);
                    emit1(c, (uint8_t)(((RAX&7)<<3) | 4));
                    emit1(c, (uint8_t)((1<<6) | ((XI&7)<<3) | (base&7)));
                }
            } else {
                vmov_store(c, 0, ry, load_ptr_x86(c, p, &last_ptr), XI, 2, 0);
            }
            if (lu(inst->y) <= i) ra_free_reg(ra, inst->y);
        } break;

        case op_store_half: {
            int8_t ry = ra_ensure(ra, sl, ns, inst->y);
            int p = inst->ptr;
            int8_t tmp = ra_alloc(ra, sl, ns);
            if (scalar) {
                vcvtps2ph(c, tmp, ry, 4);
                vex(c, 1, 1, 0, 0, tmp, 0, RAX, 0x7E);  // VMOVD eax, xmm_tmp
                {
                    int base = load_ptr_x86(c, p, &last_ptr);
                    emit1(c, 0x66);
                    uint8_t rex = 0x40;
                    if (XI >= 8) rex |= 0x02;
                    if (base >= 8) rex |= 0x01;
                    if (rex != 0x40) emit1(c, rex);
                    emit1(c, 0x89);
                    emit1(c, (uint8_t)(((RAX&7)<<3) | 4));
                    emit1(c, (uint8_t)((1<<6) | ((XI&7)<<3) | (base&7)));
                }
            } else {
                vcvtps2ph(c, tmp, ry, 4);
                vmov_store(c, 0, tmp, load_ptr_x86(c, p, &last_ptr), XI, 2, 0);
            }
            ra_return_reg(ra, tmp);
            if (lu(inst->y) <= i) ra_free_reg(ra, inst->y);
        } break;

        case op_uni_32: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int p = inst->ptr;
            int base = load_ptr_x86(c, p, &last_ptr);
            int disp = inst->imm * 4;
            // VBROADCASTSS ymm_rd, dword [base + disp]
            // VEX.256.66.0F38 18 /r with memory operand
            {
                uint8_t R = (uint8_t)(~s.rd >> 3) & 1;
                uint8_t B = (uint8_t)(~base >> 3) & 1;
                // 3-byte VEX: C4 RXBmmmmm WvvvvLpp
                emit1(c, 0xC4);
                emit1(c, (uint8_t)((R<<7) | (1<<6) | (B<<5) | 0x02));  // R.1.B.00010 (0F38)
                emit1(c, (uint8_t)(0x7D));  // W=0, vvvv=1111, L=1(256), pp=01 → 0.1111.1.01=0x7D
                emit1(c, 0x18);
                if (disp == 0 && (base & 7) != RBP) {
                    emit1(c, (uint8_t)(((s.rd & 7) << 3) | (base & 7)));
                    if ((base & 7) == RSP) emit1(c, 0x24);
                } else if (disp >= -128 && disp <= 127) {
                    emit1(c, (uint8_t)(0x40 | ((s.rd & 7) << 3) | (base & 7)));
                    if ((base & 7) == RSP) emit1(c, 0x24);
                    emit1(c, (uint8_t)disp);
                } else {
                    emit1(c, (uint8_t)(0x80 | ((s.rd & 7) << 3) | (base & 7)));
                    if ((base & 7) == RSP) emit1(c, 0x24);
                    emit4(c, (uint32_t)disp);
                }
            }
        } break;

        case op_uni_16: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int p = inst->ptr;
            int base = load_ptr_x86(c, p, &last_ptr);
            // MOVZX eax, word [base + imm*2]
            {
                uint8_t rex = 0x40;
                if (base >= 8) rex |= 0x01;
                emit1(c, rex);
                emit1(c, 0x0F); emit1(c, 0xB7);
                int disp = inst->imm * 2;
                if (disp == 0 && (base & 7) != RBP) {
                    emit1(c, (uint8_t)(((RAX & 7) << 3) | (base & 7)));
                    if ((base & 7) == RSP) emit1(c, 0x24);
                } else {
                    emit1(c, (uint8_t)(0x40 | ((RAX & 7) << 3) | (base & 7)));
                    if ((base & 7) == RSP) emit1(c, 0x24);
                    emit1(c, (uint8_t)disp);
                }
            }
            {
                int8_t tmp = ra_alloc(ra, sl, ns);
                vex(c, 1, 1, 0, 0, tmp, 0, RAX, 0x6E);   // VMOVD xmm_tmp, eax
                vex_rr(c, 1, 2, 0, 0x79, s.rd, tmp);      // VPBROADCASTW xmm_rd, xmm_tmp
                ra_return_reg(ra, tmp);
            }
        } break;

        case op_uni_half: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            int p = inst->ptr;
            int base = load_ptr_x86(c, p, &last_ptr);
            // MOVZX eax, word [base + imm*2]
            {
                uint8_t rex = 0x40;
                if (base >= 8) rex |= 0x01;
                emit1(c, rex);
                emit1(c, 0x0F); emit1(c, 0xB7);
                int disp = inst->imm * 2;
                if (disp == 0 && (base & 7) != RBP) {
                    emit1(c, (uint8_t)(((RAX & 7) << 3) | (base & 7)));
                    if ((base & 7) == RSP) emit1(c, 0x24);
                } else {
                    emit1(c, (uint8_t)(0x40 | ((RAX & 7) << 3) | (base & 7)));
                    if ((base & 7) == RSP) emit1(c, 0x24);
                    emit1(c, (uint8_t)disp);
                }
            }
            {
                int8_t tmp = ra_alloc(ra, sl, ns);
                vex(c, 1, 1, 0, 0, tmp, 0, RAX, 0x6E);   // VMOVD xmm_tmp, eax
                vex_rr(c, 1, 2, 0, 0x79, tmp, tmp);       // VPBROADCASTW xmm_tmp, xmm_tmp
                vcvtph2ps(c, s.rd, tmp);                   // VCVTPH2PS ymm_rd, xmm_tmp
                ra_return_reg(ra, tmp);
            }
        } break;

        case op_gather_32: case op_gather_16: case op_gather_half: {
            struct ra_step s = ra_step_alloc(ra, sl, ns, i);
            if (scalar) {
                // Extract index to RAX, then load from ptr + rax * elem_size
                int8_t rx = ra_ensure(ra, sl, ns, inst->x);
                vex(c, 1, 1, 0, 0, rx, 0, RAX, 0x7E);  // VMOVD eax, xmm_rx
                if (lu(inst->x) <= i) ra_free_reg(ra, inst->x);
                int p = inst->ptr;
                int base = load_ptr_x86(c, p, &last_ptr);
                if (inst->op == op_gather_32) {
                    // VMOVD xmm_rd, [base + rax*4]
                    vex_mem(c, 1, 1, 0, 0, s.rd, 0, 0x6E, base, RAX, 4, 0);
                } else if (inst->op == op_gather_16) {
                    // MOVZX eax, word [base + rax*2]
                    {
                        uint8_t rex = 0x40;
                        if (base >= 8) rex |= 0x01;
                        if (rex != 0x40) emit1(c, rex);
                        emit1(c, 0x0F); emit1(c, 0xB7);
                        emit1(c, (uint8_t)(((RAX&7)<<3) | 4));
                        emit1(c, (uint8_t)((1<<6) | ((RAX&7)<<3) | (base&7)));
                    }
                    vex(c, 1, 1, 0, 0, s.rd, 0, RAX, 0x6E);  // VMOVD xmm_rd, eax
                } else {
                    // gather_half: MOVZX eax, word [base + rax*2]; VMOVD xmm0, eax; VCVTPH2PS
                    {
                        uint8_t rex = 0x40;
                        if (base >= 8) rex |= 0x01;
                        if (rex != 0x40) emit1(c, rex);
                        emit1(c, 0x0F); emit1(c, 0xB7);
                        emit1(c, (uint8_t)(((RAX&7)<<3) | 4));
                        emit1(c, (uint8_t)((1<<6) | ((RAX&7)<<3) | (base&7)));
                    }
                    {
                        int8_t tmp = ra_alloc(ra, sl, ns);
                        vex(c, 1, 1, 0, 0, tmp, 0, RAX, 0x6E);  // VMOVD xmm_tmp, eax
                        vcvtph2ps(c, s.rd, tmp);
                        ra_return_reg(ra, tmp);
                    }
                }
            } else {
                if (lu(inst->x) <= i) ra_free_reg(ra, inst->x);
                vpxor(c, 1, s.rd, s.rd, s.rd);
            }
        } break;

        case op_scatter_32: case op_scatter_16: case op_scatter_half: {
            if (scalar) {
                int8_t rx = ra_ensure(ra, sl, ns, inst->x);
                int8_t ry = ra_ensure(ra, sl, ns, inst->y);
                // Extract index to RAX
                vex(c, 1, 1, 0, 0, rx, 0, RAX, 0x7E);  // VMOVD eax, xmm_rx
                if (lu(inst->x) <= i) ra_free_reg(ra, inst->x);
                int p = inst->ptr;
                int base = load_ptr_x86(c, p, &last_ptr);
                if (inst->op == op_scatter_32) {
                    // VMOVD [base + rax*4], xmm_ry
                    vex_mem(c, 1, 1, 0, 0, ry, 0, 0x7E, base, RAX, 4, 0);
                } else if (inst->op == op_scatter_16) {
                    // LEA R11, [base + RAX*2] then VMOVD eax, xmm_ry then MOV [R11], ax
                    // Must compute address before extracting value since base is R11.
                    emit1(c, 0x4D); emit1(c, 0x8D); emit1(c, 0x1C); emit1(c, 0x43);
                    vex(c, 1, 1, 0, 0, ry, 0, RAX, 0x7E);
                    emit1(c, 0x66); emit1(c, 0x41); emit1(c, 0x89); emit1(c, 0x03);
                    last_ptr = -1;
                } else {
                    // scatter_half: VCVTPS2PH then LEA+extract+store as above.
                    int8_t tmp = ra_alloc(ra, sl, ns);
                    vcvtps2ph(c, tmp, ry, 4);
                    emit1(c, 0x4D); emit1(c, 0x8D); emit1(c, 0x1C); emit1(c, 0x43);
                    vex(c, 1, 1, 0, 0, tmp, 0, RAX, 0x7E);
                    emit1(c, 0x66); emit1(c, 0x41); emit1(c, 0x89); emit1(c, 0x03);
                    ra_return_reg(ra, tmp);
                    last_ptr = -1;
                }
            } else {
                if (lu(inst->x) <= i) ra_free_reg(ra, inst->x);
            }
            if (lu(inst->y) <= i) ra_free_reg(ra, inst->y);
        } break;

        case op_load_8x4: {
            _Bool is_base = inst->x == 0;
            int ch = is_base ? 0 : inst->imm;
            int p = is_base ? inst->ptr : bb->inst[inst->x].ptr;

            if (scalar) {
                struct ra_step rs = ra_step_alloc(ra, sl, ns, i);
                // Load single byte: MOVZX eax, byte [base + R10*4 + ch]
                {
                    int base = load_ptr_x86(c, p, &last_ptr);
                    uint8_t rex = 0x40;
                    if (XI >= 8) rex |= 0x02;
                    if (base >= 8) rex |= 0x01;
                    if (rex != 0x40) emit1(c, rex);
                    emit1(c, 0x0F); emit1(c, 0xB6);
                    int disp = ch;
                    int mod = disp ? ((disp < 128) ? 1 : 2) : 0;
                    if (mod == 0 && (base&7) == 5) mod = 1;
                    emit1(c, (uint8_t)((mod<<6) | ((RAX&7)<<3) | 4));
                    emit1(c, (uint8_t)((2<<6) | ((XI&7)<<3) | (base&7)));  // scale=4
                    if (mod == 1) emit1(c, (uint8_t)disp);
                    else if (mod == 2) emit4(c, (uint32_t)disp);
                }
                vex(c, 1, 1, 0, 0, rs.rd, 0, RAX, 0x6E);  // VMOVD xmm, eax (as u16, zero-extended)
            } else if (is_base) {
                // Load 8 pixels × 4 channels = 32 bytes of interleaved RGBA u8 data.
                // Deinterleave into 4 × 8-element u16 XMM registers.
                // Strategy: each pixel is a dword. Shift right by ch*8 and mask 0xFF
                // to isolate one channel in each of 8 dwords (YMM). Then narrow
                // 8×u32(YMM) → 8×u16(XMM) via VEXTRACTI128 + VPACKUSDW.

                int8_t mask = ra_alloc(ra, sl, ns);  // broadcast 0xFF mask
                emit1(c, 0xB8); emit4(c, 0x000000FFu);           // MOV eax, 0xFF
                vex(c, 1, 1, 1, 0, mask, 0, RAX, 0x6E);         // VMOVD xmm_mask, eax
                vex_rr(c, 1, 2, 1, 0x58, mask, mask);            // VPBROADCASTD ymm_mask

                int8_t s0 = ra_alloc(ra, sl, ns);  // scratch for shifted/masked channel
                int8_t ch_regs[4];
                int8_t loaded = ra_alloc(ra, sl, ns);
                vmov_load(c, 1, loaded, load_ptr_x86(c, p, &last_ptr), XI, 4, 0);
                for (int ch2 = 0; ch2 < 4; ch2++) {
                    if (ch2 == 0) vpand(c, 1, s0, loaded, mask);
                    else {
                        vpsrld_i(c, s0, loaded, (uint8_t)(ch2*8));
                        vpand(c, 1, s0, s0, mask);
                    }

                    int8_t rd2 = ra_alloc(ra, sl, ns);
                    ch_regs[ch2] = rd2;
                    vextracti128(c, rd2, s0, 1);                    // xmm_rd = hi128 of s0
                    vex_rrr(c, 1, 2, 0, 0x2B, rd2, s0, rd2);      // VPACKUSDW: 4+4 u32 → 8 u16
                }

                ra_return_reg(ra, loaded);
                ra_return_reg(ra, s0);
                ra_return_reg(ra, mask);

                // Assign RA ownership.
                ra_assign(ra, i, ch_regs[0]);
                for (int j = i+1; j < to; j++) {
                    if (bb->inst[j].op == op_load_8x4 && bb->inst[j].x == i) {
                        int c2 = bb->inst[j].imm;
                        ra_assign(ra, j, ch_regs[c2]);
                    }
                }
            }
        } break;

        case op_store_8x4: {
            int p = inst->ptr;
            int const inputs[] = {inst->x, inst->y, inst->z, inst->w};
            if (scalar) {
                for (int ch = 0; ch < 4; ch++) {
                    int8_t ry = ra_ensure(ra, sl, ns, inputs[ch]);
                    // VMOVD eax, xmm
                    vex(c, 1, 1, 0, 0, ry, 0, RAX, 0x7E);
                    // MOV byte [base + R10*4 + ch], al
                    {
                        int base = load_ptr_x86(c, p, &last_ptr);
                        uint8_t rex = 0x40;
                        if (XI >= 8) rex |= 0x02;
                        if (base >= 8) rex |= 0x01;
                        if (rex != 0x40) emit1(c, rex);
                        emit1(c, 0x88);  // MOV r/m8, r8 (al)
                        int disp = ch;
                        int mod = disp ? 1 : 0;
                        if (mod == 0 && (base&7) == 5) mod = 1;
                        emit1(c, (uint8_t)((mod<<6) | ((RAX&7)<<3) | 4));
                        emit1(c, (uint8_t)((2<<6) | ((XI&7)<<3) | (base&7)));
                        if (mod == 1) emit1(c, (uint8_t)disp);
                    }
                }
            } else {
                // Narrow 4 × 8×u16(XMM) channels to u8, interleave to RGBA, store 32 bytes.
                for (int ch2 = 0; ch2 < 4; ch2++) ra_ensure(ra, sl, ns, inputs[ch2]);
                int8_t r0 = ra_reg(ra, inputs[0]), r1 = ra_reg(ra, inputs[1]);
                int8_t r2 = ra_reg(ra, inputs[2]), r3 = ra_reg(ra, inputs[3]);

                int8_t s0 = ra_alloc(ra, sl, ns);
                int8_t s1 = ra_alloc(ra, sl, ns);

                // Pack each channel u16→u8 individually (with zero high half):
                int8_t zero = ra_alloc(ra, sl, ns);
                vpxor(c, 0, zero, zero, zero);
                vpackuswb(c, s0, r0, zero);            // s0 = [R0..R7, 0..0]
                vpackuswb(c, s1, r1, zero);            // s1 = [G0..G7, 0..0]
                vpunpcklbw(c, s0, s0, s1);             // s0 = [R0,G0,R1,G1,...,R7,G7]

                vpackuswb(c, s1, r2, zero);            // s1 = [B0..B7, 0..0]
                {
                    int8_t atmp = ra_alloc(ra, sl, ns);
                    vpackuswb(c, atmp, r3, zero);       // atmp = [A0..A7, 0..0]
                    vpunpcklbw(c, s1, s1, atmp);        // s1 = [B0,A0,...,B7,A7]
                    ra_return_reg(ra, atmp);
                }
                ra_return_reg(ra, zero);

                // s0=[RG pairs], s1=[BA pairs] → interleave words for RGBA pixels
                {
                    int8_t stmp = ra_alloc(ra, sl, ns);
                    vmovaps_x(c, stmp, s0);
                    vex_rrr(c, 1, 1, 0, 0x61, s0, stmp, s1);  // VPUNPCKLWD → first 4 pixels
                    vex_rrr(c, 1, 1, 0, 0x69, s1, stmp, s1);  // VPUNPCKHWD → last 4 pixels
                    ra_return_reg(ra, stmp);
                }
                vinserti128(c, s0, s0, s1, 1);         // ymm_s0 = [first4 | last4]
                vmov_store(c, 1, s0, load_ptr_x86(c, p, &last_ptr), XI, 4, 0);
                ra_return_reg(ra, s0);
                ra_return_reg(ra, s1);
            }
            for (int ch = 0; ch < 4; ch++) {
                if (lu(inputs[ch]) <= i) ra_free_reg(ra, inputs[ch]);
            }
        } break;

        // Halfs are carried as f32 in YMM. Round to fp16 precision via VCVTPS2PH+VCVTPH2PS.
        case op_half_from_f32: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            int8_t tmp = ra_alloc(ra, sl, ns);
            vcvtps2ph(c, tmp, s.rx, 4);
            vcvtph2ps(c, s.rd, tmp);
            ra_return_reg(ra, tmp);
        } break;

        case op_f32_from_half: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            if (s.rd != s.rx) vmovaps(c, s.rd, s.rx);
        } break;

        case op_half_from_i32: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            int8_t tmp = ra_alloc(ra, sl, ns);
            vcvtdq2ps(c, s.rd, s.rx);
            vcvtps2ph(c, tmp, s.rd, 4);
            vcvtph2ps(c, s.rd, tmp);
            ra_return_reg(ra, tmp);
        } break;

        case op_i32_from_half: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            vcvttps2dq(c, s.rd, s.rx);
        } break;

        case op_i16_from_i32: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            int8_t tmp = ra_alloc(ra, sl, ns);
            vextracti128(c, tmp, s.rx, 1);
            vex_rrr(c, 1, 1, 0, 0x6B, s.rd, s.rx, tmp);
            ra_return_reg(ra, tmp);
        } break;

        case op_shr_narrow_u32: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            int8_t t0 = ra_alloc(ra, sl, ns);
            int8_t t1 = ra_alloc(ra, sl, ns);
            vpsrld_i(c, t0, s.rx, (uint8_t)inst->imm);
            vextracti128(c, t1, t0, 1);
            vex_rrr(c, 1, 1, 0, 0x6B, s.rd, t0, t1);
            ra_return_reg(ra, t0);
            ra_return_reg(ra, t1);
        } break;

        case op_i32_from_i16: {
            struct ra_step s = ra_step_unary(ra, sl, ns, inst, i, scalar);
            vpmovsxwd(c, s.rd, s.rx);
        } break;

        case op_imm_32: case op_imm_16: case op_imm_half:
        case op_add_f32: case op_sub_f32: case op_mul_f32: case op_div_f32:
        case op_min_f32: case op_max_f32: case op_sqrt_f32: case op_fma_f32: case op_fms_f32:
        case op_add_i32: case op_sub_i32: case op_mul_i32:
        case op_shl_i32: case op_shr_u32: case op_shr_s32:
        case op_shl_i32_imm: case op_shr_u32_imm: case op_shr_s32_imm:
        case op_and_32: case op_or_32: case op_xor_32: case op_sel_32:
        case op_f32_from_i32: case op_i32_from_f32:
        case op_eq_f32: case op_lt_f32: case op_le_f32:
        case op_eq_i32: case op_lt_s32: case op_le_s32: case op_lt_u32: case op_le_u32:
        case op_add_i16: case op_sub_i16: case op_mul_i16:
        case op_shl_i16: case op_shr_u16: case op_shr_s16:
        case op_shl_i16_imm: case op_shr_u16_imm: case op_shr_s16_imm:
        case op_and_16: case op_or_16: case op_xor_16: case op_sel_16:
        case op_eq_i16: case op_lt_s16: case op_le_s16: case op_lt_u16: case op_le_u16:
        case op_add_half: case op_sub_half: case op_mul_half: case op_div_half:
        case op_min_half: case op_max_half: case op_sqrt_half: case op_fma_half: case op_fms_half:
        case op_and_half: case op_or_half: case op_xor_half: case op_sel_half:
        case op_eq_half: case op_lt_half: case op_le_half:
        case op_half_from_i16: case op_i16_from_half: {
            enum op op2 = inst->op;
            int nscratch = (op2==op_lt_u32 || op2==op_lt_u16
                         || op2==op_shl_i16 || op2==op_shr_u16 || op2==op_shr_s16) ? 2
                         : (op2==op_le_s32 || op2==op_le_u32
                         || op2==op_le_s16 || op2==op_le_u16
                         || op2==op_i16_from_half) ? 1 : 0;
            struct ra_step s = ra_step_alu(ra, sl, ns, inst, i, scalar, nscratch);

            emit_alu_reg(c, inst->op, s.rd, s.rx, s.ry, s.rz, inst->imm,
                         s.scratch, s.scratch2);
            if (s.scratch >= 0) ra_return_reg(ra, s.scratch);
            if (s.scratch2 >= 0) ra_return_reg(ra, s.scratch2);
        } break;
        }
    }
    #undef lu
}

void umbra_jit_run(struct umbra_jit *j, int n, umbra_buf buf[]) {
    if (!j) return;
    j->entry(n, buf);
}

void umbra_jit_free(struct umbra_jit *j) {
    if (!j) return;
    munmap(j->code, j->code_size);
    free(j);
}

// Write .byte directives for code bytes, assemble, disassemble.
// Returns 1 on success with disassembly written to out_path .o, 0 on failure.
static _Bool x86_disasm(uint8_t const *code, size_t n,
                        char const *spath, char const *opath, FILE *f) {
    FILE *fp = fopen(spath, "w");
    if (!fp) return 0;
    for (size_t i = 0; i < n; i++) fprintf(fp, ".byte 0x%02x\n", code[i]);
    fclose(fp);

    char cmd[1024];
    snprintf(cmd, sizeof cmd,
             "/opt/homebrew/opt/llvm/bin/clang"
             " -target x86_64-apple-macos13 -c %s -o %s 2>/dev/null &&"
             " /opt/homebrew/opt/llvm/bin/llvm-objdump"
             " -d --no-show-raw-insn --no-leading-addr %s 2>/dev/null",
             spath, opath, opath);
    FILE *p = popen(cmd, "r");
    if (!p) return 0;
    char line[256];
    _Bool ok = 0;
    while (fgets(line, (int)sizeof line, p)) {
        if (!ok && __builtin_strstr(line, "file format")) { ok = 1; continue; }
        if (f) fputs(line, f);
    }
    return pclose(p) == 0 && ok;
}

void umbra_jit_dump(struct umbra_jit const *j, FILE *f) {
    if (!j) return;
    uint8_t const *code = (uint8_t const *)j->code;
    size_t n = j->code_len;

    char spath[] = "/tmp/umbra_jit_XXXXXX.s";
    int fd = mkstemps(spath, 2);
    if (fd < 0) goto fallback;
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
        if ((i & 15) == 15 || i == n-1) fputc('\n', f);
    }
}

void umbra_jit_dump_bin(struct umbra_jit const *j, FILE *f) {
    if (!j) return;
    fwrite(j->code, 1, j->code_len, f);
}

void umbra_jit_mca(struct umbra_jit const *j, FILE *f) {
    if (!j || j->loop_start >= j->loop_end) return;
    uint8_t const *code = (uint8_t const *)j->code;
    size_t n = (size_t)(j->loop_end - j->loop_start);

    // Disassemble loop body to .s file
    char spath[] = "/tmp/umbra_mca_XXXXXX.s";
    int fd = mkstemps(spath, 2);
    if (fd < 0) return;
    close(fd);

    char opath[sizeof spath + 2];
    snprintf(opath, sizeof opath, "%.*s.o", (int)(sizeof spath - 3), spath);

    // First pass: disassemble to get AT&T asm
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

    // Strip the disassembly down to just instructions (remove blank lines, labels)
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
                if (__builtin_strstr(line, "<")) past_header = 1;
                continue;
            }
            if (line[0] == '\n' || line[0] == '<') continue;
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
        char line[256];
        while (fgets(line, (int)sizeof line, p)) fputs(line, f);
        pclose(p);
    }
    remove(cleanpath);
}

#endif
