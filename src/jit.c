#include "../umbra.h"
#include "bb.h"

#if !defined(__aarch64__)

struct umbra_jit { int dummy; };
struct umbra_jit* umbra_jit(struct umbra_basic_block const *bb) { (void)bb; return 0; }
void umbra_jit_run (struct umbra_jit *j, int n, void *p0, void *p1, void *p2, void *p3, void *p4, void *p5) {
    (void)j; (void)n; (void)p0; (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
}
void umbra_jit_free(struct umbra_jit *j) { (void)j; }
void umbra_jit_dump(struct umbra_jit const *j, FILE *f) { (void)j; (void)f; }
void umbra_jit_mca (struct umbra_jit const *j, FILE *f) { (void)j; (void)f; }

#else

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <pthread.h>
#include <unistd.h>
#include <libkern/OSCacheControl.h>

typedef struct { uint32_t *buf; int len,cap; } Buf;

static void put(Buf *b, uint32_t w) {
    if (b->len == b->cap) {
        b->cap = b->cap ? 2*b->cap : 1024;
        b->buf = realloc(b->buf, (size_t)b->cap * 4);
    }
    b->buf[b->len++] = w;
}

static uint32_t RET(void) { return 0xD65F03C0u; }

static uint32_t ADD_xr(int d, int n, int m) {
    return 0x8B000000u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}
static uint32_t ADD_xi(int d, int n, int imm12) {
    return 0x91000000u | ((uint32_t)imm12<<10) | ((uint32_t)n<<5) | (uint32_t)d;
}
static uint32_t SUB_xi(int d, int n, int imm12) {
    return 0xD1000000u | ((uint32_t)imm12<<10) | ((uint32_t)n<<5) | (uint32_t)d;
}
static uint32_t SUBS_xi(int d, int n, int imm12) {
    return 0xF1000000u | ((uint32_t)imm12<<10) | ((uint32_t)n<<5) | (uint32_t)d;
}
static uint32_t MOVZ_x(int d, uint16_t imm) {
    return 0xD2800000u | ((uint32_t)imm<<5) | (uint32_t)d;
}
static uint32_t MOVZ_w(int d, uint16_t imm) {
    return 0x52800000u | ((uint32_t)imm<<5) | (uint32_t)d;
}
static uint32_t MOVK_w16(int d, uint16_t imm) {
    return 0x72A00000u | ((uint32_t)imm<<5) | (uint32_t)d;
}
static uint32_t STP_pre(int t1, int t2, int n, int imm7) {
    return 0xA9800000u | ((uint32_t)(imm7&0x7f)<<15) | ((uint32_t)t2<<10) | ((uint32_t)n<<5) | (uint32_t)t1;
}
static uint32_t LDP_post(int t1, int t2, int n, int imm7) {
    return 0xA8C00000u | ((uint32_t)(imm7&0x7f)<<15) | ((uint32_t)t2<<10) | ((uint32_t)n<<5) | (uint32_t)t1;
}
static uint32_t B(int off26) {
    return 0x14000000u | (uint32_t)(off26 & 0x3ffffff);
}
static uint32_t Bcond(int cond, int off19) {
    return 0x54000000u | ((uint32_t)(off19&0x7ffff)<<5) | (uint32_t)cond;
}

static uint32_t LDR_sx(int d, int n, int m) {  // LDR Sd,[Xn,Xm,LSL#2]
    return 0xBC607800u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}
static uint32_t STR_sx(int d, int n, int m) {
    return 0xBC207800u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}
static uint32_t LDR_hx(int d, int n, int m) {  // LDR Hd,[Xn,Xm,LSL#1]
    return 0x7C607800u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}
static uint32_t STR_hx(int d, int n, int m) {
    return 0x7C207800u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}

static uint32_t LDR_q(int d, int n, int m) {  // LDR Qd,[Xn,Xm]
    return 0x3CE06800u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}
static uint32_t STR_q(int d, int n, int m) {
    return 0x3CA06800u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}

// LSL Xd, Xn, #imm  (UBFM alias)
static uint32_t LSL_xi(int d, int n, int shift) {
    int immr = (-shift) & 63, imms = 63 - shift;
    return 0xD3400000u | ((uint32_t)immr<<16) | ((uint32_t)imms<<10)
         | ((uint32_t)n<<5) | (uint32_t)d;
}

static uint32_t LDR_qi(int d, int n, int imm) { // LDR Qd,[Xn,#imm*16]
    return 0x3DC00000u | ((uint32_t)imm<<10) | ((uint32_t)n<<5) | (uint32_t)d;
}
static uint32_t STR_qi(int d, int n, int imm) {
    return 0x3D800000u | ((uint32_t)imm<<10) | ((uint32_t)n<<5) | (uint32_t)d;
}

// LDP Qt1, Qt2, [Xn, #imm*16]
static uint32_t LDP_qi(int t1, int t2, int n, int imm) {
    return 0xAD400000u | ((uint32_t)(imm&0x7f)<<15) | ((uint32_t)t2<<10) | ((uint32_t)n<<5) | (uint32_t)t1;
}
// STP Qt1, Qt2, [Xn, #imm*16]
static uint32_t STP_qi(int t1, int t2, int n, int imm) {
    return 0xAD000000u | ((uint32_t)(imm&0x7f)<<15) | ((uint32_t)t2<<10) | ((uint32_t)n<<5) | (uint32_t)t1;
}

// LD4 {Vt.8B, Vt+1.8B, Vt+2.8B, Vt+3.8B}, [Xn]
static uint32_t LD4_8b(int t, int n) { return 0x0C400000u | ((uint32_t)n<<5) | (uint32_t)t; }
// ST4 {Vt.8B, Vt+1.8B, Vt+2.8B, Vt+3.8B}, [Xn]
static uint32_t ST4_8b(int t, int n) { return 0x0C000000u | ((uint32_t)n<<5) | (uint32_t)t; }

// Promote D-register (64-bit) NEON op to Q-register (128-bit) by setting bit 30.
// Turns 4H->8H, 8B->16B, FCVTN->FCVTN2, FCVTL->FCVTL2, XTN->XTN2, SXTL->SXTL2, etc.
static uint32_t W(uint32_t insn) { return insn | 0x40000000u; }

#define V3(enc) static uint32_t enc(int d,int n,int m){return enc##_ | ((uint32_t)m<<16)|((uint32_t)n<<5)|(uint32_t)d;}
#define V2(enc) static uint32_t enc(int d,int n){return enc##_ | ((uint32_t)n<<5)|(uint32_t)d;}

enum {
    // float 4S
    FADD_4s_  =0x4E20D400u, FSUB_4s_  =0x4EA0D400u, FMUL_4s_  =0x6E20DC00u,
    FDIV_4s_  =0x6E20FC00u, FMLA_4s_  =0x4E20CC00u,
    FMINNM_4s_=0x4EA0C400u, FMAXNM_4s_=0x4E20C400u,
    FSQRT_4s_ =0x6EA1F800u,
    SCVTF_4s_ =0x4E21D800u, FCVTZS_4s_=0x4EA1B800u,
    FCMEQ_4s_ =0x4E20E400u, FCMGT_4s_ =0x6EA0E400u, FCMGE_4s_ =0x6E20E400u,

    // int 4S
    ADD_4s_ =0x4EA08400u, SUB_4s_ =0x6EA08400u, MUL_4s_ =0x4EA09C00u,
    USHL_4s_=0x6EA04400u, SSHL_4s_=0x4EA04400u, NEG_4s_ =0x6EA0B800u,
    CMEQ_4s_=0x6EA08C00u, CMGT_4s_=0x4EA03400u, CMGE_4s_=0x4EA03C00u,
    CMHI_4s_=0x6EA03400u, CMHS_4s_=0x6EA03C00u,

    // bitwise 16B
    AND_16b_=0x4E201C00u, ORR_16b_=0x4EA01C00u, EOR_16b_=0x6E201C00u,
    BSL_16b_=0x6E601C00u, BIT_16b_=0x6EA01C00u, BIF_16b_=0x6EE01C00u,
    MVN_16b_=0x6E205800u,

    // float 4H (FEAT_FP16)
    FADD_4h_  =0x0E401400u, FSUB_4h_  =0x0EC01400u, FMUL_4h_  =0x2E401C00u,
    FDIV_4h_  =0x2E403C00u, FMLA_4h_  =0x0E400C00u,
    FMINNM_4h_=0x0EC00400u, FMAXNM_4h_=0x0E400400u,
    FSQRT_4h_ =0x2EF9F800u,
    FCMEQ_4h_ =0x0E402400u, FCMGT_4h_ =0x2EC02400u, FCMGE_4h_ =0x2E402400u,

    // int 4H
    ADD_4h_ =0x0E608400u, SUB_4h_ =0x2E608400u, MUL_4h_ =0x0E609C00u,
    USHL_4h_=0x2E604400u, SSHL_4h_=0x0E604400u, NEG_4h_ =0x2E60B800u,
    CMEQ_4h_=0x2E608C00u, CMGT_4h_=0x0E603400u, CMGE_4h_=0x0E603C00u,
    CMHI_4h_=0x2E603400u, CMHS_4h_=0x2E603C00u,

    // conversions
    FCVTN_4h_=0x0E216800u, FCVTL_4s_=0x0E217800u,
    SCVTF_4h_=0x0E79D800u, FCVTZS_4h_=0x0EF9B800u,
    XTN_4h_  =0x0E612800u, SXTL_4s_  =0x0F10A400u,
    UXTL_8h_ =0x2F08A400u,
};

V3(FADD_4s)  V3(FSUB_4s)  V3(FMUL_4s) V3(FDIV_4s)  V3(FMLA_4s)
V3(FMINNM_4s) V3(FMAXNM_4s) V2(FSQRT_4s) V2(SCVTF_4s) V2(FCVTZS_4s)
V3(FCMEQ_4s) V3(FCMGT_4s) V3(FCMGE_4s)
V3(ADD_4s) V3(SUB_4s) V3(MUL_4s)
V3(USHL_4s) V3(SSHL_4s) V2(NEG_4s)
V3(CMEQ_4s) V3(CMGT_4s) V3(CMGE_4s) V3(CMHI_4s) V3(CMHS_4s)
V3(AND_16b) V3(ORR_16b) V3(EOR_16b) V3(BSL_16b) V3(BIT_16b) V3(BIF_16b) V2(MVN_16b)
V3(FADD_4h)  V3(FSUB_4h)  V3(FMUL_4h) V3(FDIV_4h)  V3(FMLA_4h)
V3(FMINNM_4h) V3(FMAXNM_4h) V2(FSQRT_4h)
V3(FCMEQ_4h) V3(FCMGT_4h) V3(FCMGE_4h)
V3(ADD_4h) V3(SUB_4h) V3(MUL_4h)
V3(USHL_4h) V3(SSHL_4h) V2(NEG_4h)
V3(CMEQ_4h) V3(CMGT_4h) V3(CMGE_4h) V3(CMHI_4h) V3(CMHS_4h)
V2(FCVTN_4h) V2(FCVTL_4s)
V2(SCVTF_4h) V2(FCVTZS_4h) V2(XTN_4h) V2(SXTL_4s) V2(UXTL_8h)

#undef V3
#undef V2

// Shift-by-immediate: immh:immb at bits 22:16
static uint32_t SHL_4s_imm (int d, int n, int sh) { return 0x4F005400u|((uint32_t)(sh+32)<<16)|((uint32_t)n<<5)|(uint32_t)d; }
static uint32_t USHR_4s_imm(int d, int n, int sh) { return 0x6F000400u|((uint32_t)(64-sh)<<16)|((uint32_t)n<<5)|(uint32_t)d; }
static uint32_t SSHR_4s_imm(int d, int n, int sh) { return 0x4F000400u|((uint32_t)(64-sh)<<16)|((uint32_t)n<<5)|(uint32_t)d; }
static uint32_t SHRN_4h    (int d, int n, int sh) { return 0x0F008400u|((uint32_t)(32-sh)<<16)|((uint32_t)n<<5)|(uint32_t)d; }
static uint32_t SHL_4h_imm (int d, int n, int sh) { return 0x0F005400u|((uint32_t)(sh+16)<<16)|((uint32_t)n<<5)|(uint32_t)d; }
static uint32_t USHR_4h_imm(int d, int n, int sh) { return 0x2F000400u|((uint32_t)(32-sh)<<16)|((uint32_t)n<<5)|(uint32_t)d; }
static uint32_t SSHR_4h_imm(int d, int n, int sh) { return 0x0F000400u|((uint32_t)(32-sh)<<16)|((uint32_t)n<<5)|(uint32_t)d; }

static uint32_t TBL_16b(int d, int n, int m) { return 0x4E000000u|((uint32_t)m<<16)|((uint32_t)n<<5)|(uint32_t)d; }
static uint32_t FMOV_d_x(int d, int n) { return 0x9E670000u|((uint32_t)n<<5)|(uint32_t)d; }
static uint32_t INS_d(int d, int idx, int n) {
    uint32_t imm5 = (uint32_t)(idx<<4)|8;
    return 0x4E001C00u|(imm5<<16)|((uint32_t)n<<5)|(uint32_t)d;
}

// ST1 {Vt.B}[idx], [Xn]
static uint32_t ST1_b(int t, int idx, int n) {
    uint32_t Q = ((uint32_t)idx >> 3) & 1;
    uint32_t S = ((uint32_t)idx >> 2) & 1;
    uint32_t sz = (uint32_t)idx & 3;
    return 0x0D000000u | (Q<<30) | (S<<12) | (sz<<10) | ((uint32_t)n<<5) | (uint32_t)t;
}

// XTN Vd.8B, Vn.8H (narrow i16->i8)
static uint32_t XTN_8b(int d, int n) { return 0x0E212800u | ((uint32_t)n<<5) | (uint32_t)d; }

static uint32_t MOVI_4s_0(int d) { return 0x4F000400u|(uint32_t)d; }
static uint32_t DUP_4s_w(int d, int n)  { return 0x4E040C00u|((uint32_t)n<<5)|(uint32_t)d; }
static uint32_t DUP_4h_w(int d, int n)  { return 0x0E020C00u|((uint32_t)n<<5)|(uint32_t)d; }

static uint32_t INS_s(int d, int idx, int n) {
    uint32_t imm5 = (uint32_t)(idx<<3)|4;
    return 0x4E001C00u|(imm5<<16)|((uint32_t)n<<5)|(uint32_t)d;
}

// x0=n, x1..x6=p0..p5, x9=loop i, x10=scratch, x11/x12=byte offsets, x15=stack base
enum { XI=9, XT=10, XH=11, XW=12, XS=15 };

static void load_imm_w(Buf *c, int rd, uint32_t v) {
    put(c, MOVZ_w(rd, (uint16_t)(v&0xffff)));
    if (v>>16) put(c, MOVK_w16(rd, (uint16_t)(v>>16)));
}
static void load_imm_x(Buf *c, int rd, uint64_t v) {
    put(c, MOVZ_x(rd, (uint16_t)v));
    if ((v>>16)&0xffff) put(c, 0xF2A00000u|((uint32_t)((v>>16)&0xffff)<<5)|(uint32_t)rd);
    if ((v>>32)&0xffff) put(c, 0xF2C00000u|((uint32_t)((v>>32)&0xffff)<<5)|(uint32_t)rd);
    if ((v>>48)&0xffff) put(c, 0xF2E00000u|((uint32_t)((v>>48)&0xffff)<<5)|(uint32_t)rd);
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
        if (v==0) { put(c, MOVI_4s_0(d)); }
        else { load_imm_w(c,XT,v); put(c, DUP_4s_w(d,XT)); }
    } return 1;
    case op_imm_16: case op_imm_half:
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
    case op_ne_f32: put(c, FCMEQ_4s(d,x,y)); put(c, MVN_16b(d,d)); return 1;
    case op_gt_f32: put(c, FCMGT_4s(d,x,y)); return 1;
    case op_ge_f32: put(c, FCMGE_4s(d,x,y)); return 1;
    case op_lt_f32: put(c, FCMGT_4s(d,y,x)); return 1;
    case op_le_f32: put(c, FCMGE_4s(d,y,x)); return 1;

    case op_eq_i32: put(c, CMEQ_4s(d,x,y)); return 1;
    case op_ne_i32: put(c, CMEQ_4s(d,x,y)); put(c, MVN_16b(d,d)); return 1;
    case op_gt_s32: put(c, CMGT_4s(d,x,y)); return 1;
    case op_ge_s32: put(c, CMGE_4s(d,x,y)); return 1;
    case op_lt_s32: put(c, CMGT_4s(d,y,x)); return 1;
    case op_le_s32: put(c, CMGE_4s(d,y,x)); return 1;
    case op_gt_u32: put(c, CMHI_4s(d,x,y)); return 1;
    case op_ge_u32: put(c, CMHS_4s(d,x,y)); return 1;
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
    // 16-bit bitwise: use 16B (full 128-bit Q register)
    case op_and_16: put(c, AND_16b(d,x,y)); return 1;
    case op_or_16:  put(c, ORR_16b(d,x,y)); return 1;
    case op_xor_16: put(c, EOR_16b(d,x,y)); return 1;
    case op_sel_16:
        if (d==x) { put(c, BSL_16b(d,y,z)); }
        else if (d==y) { put(c, BIF_16b(d,z,x)); }
        else if (d==z) { put(c, BIT_16b(d,y,x)); }
        else { put(c, ORR_16b(d,z,z)); put(c, BIT_16b(d,y,x)); }
        return 1;
    case op_eq_i16: put(c, W(CMEQ_4h(d,x,y))); return 1;
    case op_ne_i16: put(c, W(CMEQ_4h(d,x,y))); put(c, MVN_16b(d,d)); return 1;
    case op_gt_s16: put(c, W(CMGT_4h(d,x,y))); return 1;
    case op_ge_s16: put(c, W(CMGE_4h(d,x,y))); return 1;
    case op_lt_s16: put(c, W(CMGT_4h(d,y,x))); return 1;
    case op_le_s16: put(c, W(CMGE_4h(d,y,x))); return 1;
    case op_gt_u16: put(c, W(CMHI_4h(d,x,y))); return 1;
    case op_ge_u16: put(c, W(CMHS_4h(d,x,y))); return 1;
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
    // Half bitwise: use 16B (full Q register)
    case op_and_half: put(c, AND_16b(d,x,y)); return 1;
    case op_or_half:  put(c, ORR_16b(d,x,y)); return 1;
    case op_xor_half: put(c, EOR_16b(d,x,y)); return 1;
    case op_sel_half:
        if (d==x) { put(c, BSL_16b(d,y,z)); }
        else if (d==y) { put(c, BIF_16b(d,z,x)); }
        else if (d==z) { put(c, BIT_16b(d,y,x)); }
        else { put(c, ORR_16b(d,z,z)); put(c, BIT_16b(d,y,x)); }
        return 1;
    case op_eq_half: put(c, W(FCMEQ_4h(d,x,y))); return 1;
    case op_ne_half: put(c, W(FCMEQ_4h(d,x,y))); put(c, MVN_16b(d,d)); return 1;
    case op_gt_half: put(c, W(FCMGT_4h(d,x,y))); return 1;
    case op_ge_half: put(c, W(FCMGE_4h(d,x,y))); return 1;
    case op_lt_half: put(c, W(FCMGT_4h(d,y,x))); return 1;
    case op_le_half: put(c, W(FCMGE_4h(d,y,x))); return 1;

    // Conversions within same width
    case op_half_from_i16: put(c, W(SCVTF_4h(d,x))); return 1;
    case op_i16_from_half: put(c, W(FCVTZS_4h(d,x))); return 1;

    default: return 0;
    }
}

// Register allocator: v4-v7,v16-v31 in initial pool; v0-v3 injected after LD4.
static const int8_t ra_pool[] = {4,5,6,7,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31};
#define RA_NREGS 20

struct ra {
    int    *last_use;     // [insts] last varying op that reads each value
    int8_t *reg;          // [insts] NEON register for value i, or -1
    int8_t *reg_hi;       // [insts] hi register for OP_32 pairs, or -1
    _Bool  *is_pair;      // [insts] true if value needs register pair (varying OP_32)
    int     nfree;
    int     bytes_ctrl_n;
    int     owner[32];    // owner[v] = inst whose value is in Vv, or -1
    int     bytes_ctrl[16];
    int8_t  free_stack[32];
    int8_t  bytes_reg[16];
};

static struct ra* ra_create(struct umbra_basic_block const *bb) {
    int n = bb->insts;
    struct ra *ra = malloc(sizeof *ra);
    ra->reg      = malloc((size_t)n * sizeof *ra->reg);
    ra->reg_hi   = malloc((size_t)n * sizeof *ra->reg_hi);
    ra->last_use = malloc((size_t)n * sizeof *ra->last_use);
    ra->is_pair  = malloc((size_t)n * sizeof *ra->is_pair);

    for (int i = 0; i < n; i++) {
        ra->reg[i] = -1;
        ra->reg_hi[i] = -1;
        ra->is_pair[i] = (op_type(bb->inst[i].op) == OP_32) && (i >= bb->preamble);
    }
    for (int i = 0; i < 32; i++) ra->owner[i] = -1;
    ra->bytes_ctrl_n = 0;

    for (int i = 0; i < n; i++) ra->last_use[i] = -1;
    for (int i = 0; i < n; i++) {
        struct bb_inst const *inst = &bb->inst[i];
        // load_8x4 continuations use .x as metadata (base index), not a data dependency.
        if (!(inst->op == op_load_8x4 && inst->x)) ra->last_use[inst->x] = i;
        ra->last_use[inst->y] = i;
        ra->last_use[inst->z] = i;
        ra->last_use[inst->w] = i;
    }
    // Preamble values used in the varying loop persist across iterations.
    for (int i = 0; i < bb->preamble; i++) {
        if (ra->last_use[i] >= bb->preamble) {
            ra->last_use[i] = n;
        }
    }

    ra->nfree = RA_NREGS;
    for (int i = 0; i < RA_NREGS; i++) ra->free_stack[i] = ra_pool[RA_NREGS - 1 - i];

    return ra;
}

static void ra_destroy(struct ra *ra) {
    free(ra->reg);
    free(ra->reg_hi);
    free(ra->last_use);
    free(ra->is_pair);
    free(ra);
}

static void ra_free_reg(struct ra *ra, int val) {
    int8_t r = ra->reg[val];
    if (r < 0) return;
    ra->free_stack[ra->nfree++] = r;
    ra->owner[(int)r] = -1;
    ra->reg[val] = -1;
    int8_t rh = ra->reg_hi[val];
    if (rh >= 0) {
        ra->free_stack[ra->nfree++] = rh;
        ra->owner[(int)rh] = -1;
        ra->reg_hi[val] = -1;
    }
}

static int8_t ra_alloc(Buf *c, struct ra *ra, int *sl, int *ns) {
    if (ra->nfree > 0) return ra->free_stack[--ra->nfree];

    // Evict: find register whose owner has farthest last_use (Belady-like)
    int best_r = -1, best_lu = -1;
    for (int r = 0; r < 32; r++) {
        if (ra->owner[r] < 0) continue;
        int val = ra->owner[r];
        if (ra->last_use[val] > best_lu) { best_lu = ra->last_use[val]; best_r = r; }
    }
    int evicted = ra->owner[best_r];
    if (sl[evicted] < 0) {
        if (ra->is_pair[evicted]) { sl[evicted] = *ns; *ns += 2; }
        else { sl[evicted] = (*ns)++; }
    }
    // Spill lo
    vst(c, ra->reg[evicted], sl[evicted]);
    ra->owner[(int)ra->reg[evicted]] = -1;
    // Spill hi if pair
    if (ra->reg_hi[evicted] >= 0) {
        vst(c, ra->reg_hi[evicted], sl[evicted]+1);
        int8_t rh = ra->reg_hi[evicted];
        ra->owner[(int)rh] = -1;
        ra->reg_hi[evicted] = -1;
        ra->free_stack[ra->nfree++] = rh;
    }
    int8_t r = ra->reg[evicted];
    ra->reg[evicted] = -1;
    return r;
}

static int8_t ra_ensure(Buf *c, struct ra *ra, int *sl, int *ns, int val) {
    if (ra->reg[val] >= 0) return ra->reg[val];
    int8_t r = ra_alloc(c, ra, sl, ns);
    if (sl[val] >= 0) vld(c, r, sl[val]);
    ra->reg[val] = r;
    ra->owner[(int)r] = val;
    // Also restore hi register if this is a pair
    if (ra->is_pair[val]) {
        int8_t rh = ra_alloc(c, ra, sl, ns);
        if (sl[val] >= 0) vld(c, rh, sl[val]+1);
        ra->reg_hi[val] = rh;
        ra->owner[(int)rh] = val;
    }
    return r;
}

// Claim a dying input's register for the output, transferring ownership without free/alloc.
static int8_t ra_claim(struct ra *ra, int old_val, int new_val) {
    int8_t r = ra->reg[old_val];
    ra->reg[old_val] = -1;
    ra->reg[new_val] = r;
    ra->owner[(int)r] = new_val;
    // Transfer hi register too if old was a pair
    if (ra->reg_hi[old_val] >= 0) {
        int8_t rh = ra->reg_hi[old_val];
        ra->reg_hi[old_val] = -1;
        ra->reg_hi[new_val] = rh;
        ra->owner[(int)rh] = new_val;
    }
    return r;
}

// Evict any values living in V0-V3 (needed before LD4/ST4 which use these as fixed destinations).
static void evict_scratch(Buf *c, struct ra *ra, int *sl, int *ns) {
    for (int r = 0; r < 4; r++) {
        int val = ra->owner[r];
        if (val < 0) continue;
        int8_t new_r = ra_alloc(c, ra, sl, ns);
        put(c, ORR_16b(new_r, r, r));
        ra->reg[val] = new_r;
        ra->owner[(int)new_r] = val;
        ra->owner[r] = -1;
    }
}

// Get the hi register for a value, falling back to lo if not a pair.
static int8_t hi(struct ra *ra, int val) {
    return ra->reg_hi[val] >= 0 ? ra->reg_hi[val] : ra->reg[val];
}

static void emit_tbl_ctrl(Buf *c, int vd, int ctrl) {
    uint8_t tbl[16];
    for (int lane = 0; lane < 4; lane++) {
        for (int b = 0; b < 4; b++) {
            int nibble = (ctrl >> (b*4)) & 0xf;
            tbl[lane*4+b] = nibble ? (uint8_t)(lane*4 + nibble - 1) : 16;
        }
    }
    uint64_t lo, h;
    __builtin_memcpy(&lo, tbl+0, 8);
    __builtin_memcpy(&h, tbl+8, 8);
    load_imm_x(c, XT, lo);
    put(c, FMOV_d_x(vd, XT));
    load_imm_x(c, XT, h);
    put(c, INS_d(vd, 1, XT));
}

static void emit_ops(Buf *c, struct umbra_basic_block const *bb,
                     int from, int to,
                     int *sl, int *ns, struct ra *ra, _Bool scalar);

struct umbra_jit {
    void  *code;
    size_t code_size;
    int    loop_start, loop_end;
    void (*entry)(int, void*, void*, void*, void*, void*, void*);
};

struct umbra_jit* umbra_jit(struct umbra_basic_block const *bb) {
    int *sl = malloc((size_t)bb->insts * sizeof(int));
    for (int i = 0; i < bb->insts; i++) sl[i] = -1;
    int ns = 0;

    int max_ptr=-1;
    for (int i=0; i<bb->insts; i++) {
        if (has_ptr(bb->inst[i].op))
            if (bb->inst[i].ptr>max_ptr) max_ptr=bb->inst[i].ptr;
    }
    if (max_ptr>5) { free(sl); return 0; }

    Buf c={0};
    struct ra *ra = ra_create(bb);

    put(&c, STP_pre(29,30,31,-2));
    put(&c, ADD_xi(29,31,0));
    int stack_patch = c.len;
    put(&c, 0xD503201Fu);  // NOP placeholder: patched to SUB SP if spills needed
    put(&c, 0xD503201Fu);  // NOP placeholder: patched to MOV XS,SP if spills needed

    // Preamble: uniforms go straight into registers via RA.
    emit_ops(&c, bb, 0, bb->preamble, sl, &ns, ra, 0);

    // Pre-load TBL control vectors for op_bytes in the loop body.
    for (int i = bb->preamble; i < bb->insts; i++) {
        if (bb->inst[i].op != op_bytes) continue;
        int ctrl = bb->inst[i].imm;
        _Bool found = 0;
        for (int j = 0; j < ra->bytes_ctrl_n; j++) {
            if (ra->bytes_ctrl[j] == ctrl) { found = 1; break; }
        }
        if (!found) {
            int8_t r = ra_alloc(&c, ra, sl, &ns);
            ra->owner[(int)r] = -2;
            emit_tbl_ctrl(&c, r, ctrl);
            ra->bytes_ctrl[ra->bytes_ctrl_n] = ctrl;
            ra->bytes_reg[ra->bytes_ctrl_n] = r;
            ra->bytes_ctrl_n++;
        }
    }

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
    int loop_body_end = c.len;

    put(&c, ADD_xi(XI,XI,8));   // K=8: advance by 8
    put(&c, B(loop_top - c.len));

    int tail_top = c.len;
    c.buf[br_tail] = Bcond(0xB, tail_top - br_tail);

    put(&c, 0xEB09001Fu);  // SUBS XZR, X0, X9
    int br_epi = c.len;
    put(&c, Bcond(0xD,0));  // B.LE (patch later)

    // Scalar tail: free all varying values' registers from the vector loop.
    for (int i = bb->preamble; i < bb->insts; i++) ra_free_reg(ra, i);

    // Clear varying spill slots so they don't alias vector loop spills.
    for (int i = bb->preamble; i < bb->insts; i++) sl[i] = -1;

    // In scalar tail, disable pairs (single-element processing).
    for (int i = bb->preamble; i < bb->insts; i++) ra->is_pair[i] = 0;

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
    j->entry = (void(*)(int,void*,void*,void*,void*,void*,void*))mem;
    return j;
}

static void emit_ops(Buf *c, struct umbra_basic_block const *bb,
                     int from, int to,
                     int *sl, int *ns,
                     struct ra *ra, _Bool scalar)
{
    int *lu = ra->last_use;

    for (int i=from; i<to; i++) {
        struct bb_inst const *inst = &bb->inst[i];

        switch (inst->op) {
        case op_lane: {
            if (!scalar && ra->is_pair[i]) {
                // K=8 vector: lo=[i,i+1,i+2,i+3], hi=[i+4,i+5,i+6,i+7]
                int8_t rd = ra_alloc(c, ra, sl, ns);
                int8_t rdh = ra_alloc(c, ra, sl, ns);
                ra->reg[i] = rd; ra->reg_hi[i] = rdh;
                ra->owner[(int)rd] = i; ra->owner[(int)rdh] = i;
                put(c, DUP_4s_w(rd, XI));
                put(c, MOVI_4s_0(0));
                put(c, MOVZ_w(XT,1)); put(c, INS_s(0,1,XT));
                put(c, MOVZ_w(XT,2)); put(c, INS_s(0,2,XT));
                put(c, MOVZ_w(XT,3)); put(c, INS_s(0,3,XT));
                put(c, ADD_4s(rd, rd, 0));
                // hi = lo + [4,4,4,4]
                put(c, MOVZ_w(XT,4));
                put(c, DUP_4s_w(0, XT));
                put(c, ADD_4s(rdh, rd, 0));
            } else {
                // scalar or preamble: single register
                int8_t rd = ra_alloc(c, ra, sl, ns);
                ra->reg[i] = rd; ra->owner[(int)rd] = i;
                put(c, DUP_4s_w(rd, XI));
                if (!scalar) {
                    put(c, MOVI_4s_0(0));
                    put(c, MOVZ_w(XT,1)); put(c, INS_s(0,1,XT));
                    put(c, MOVZ_w(XT,2)); put(c, INS_s(0,2,XT));
                    put(c, MOVZ_w(XT,3)); put(c, INS_s(0,3,XT));
                    put(c, ADD_4s(rd, rd, 0));
                }
            }
        } break;

        case op_load_32: {
            int p = inst->ptr;
            if (!scalar && ra->is_pair[i]) {
                int8_t rd = ra_alloc(c, ra, sl, ns);
                int8_t rdh = ra_alloc(c, ra, sl, ns);
                ra->reg[i] = rd; ra->reg_hi[i] = rdh;
                ra->owner[(int)rd] = i; ra->owner[(int)rdh] = i;
                // LDP Qlo, Qhi, [base + XW]
                put(c, ADD_xr(XT, 1+p, XW));
                put(c, LDP_qi(rd, rdh, XT, 0));
            } else {
                int8_t rd = ra_alloc(c, ra, sl, ns);
                ra->reg[i] = rd; ra->owner[(int)rd] = i;
                if (scalar) put(c, LDR_sx(rd, 1+p, XI));
                else        put(c, LDR_q(rd, 1+p, XW));
            }
        } break;

        case op_load_16: case op_load_half: {
            int8_t rd = ra_alloc(c, ra, sl, ns);
            ra->reg[i] = rd; ra->owner[(int)rd] = i;
            int p = inst->ptr;
            if (scalar) {
                put(c, LDR_hx(rd, 1+p, XI));
            } else {
                // K=8: 8 x 16-bit = 16 bytes = 1 Q register
                put(c, LDR_q(rd, 1+p, XH));
            }
        } break;

        case op_uni_32: case op_uni_16: case op_uni_half:
        case op_gather_32: case op_gather_16: case op_gather_half: {
            int8_t rd = ra_alloc(c, ra, sl, ns);
            ra->reg[i] = rd; ra->owner[(int)rd] = i;
            put(c, MOVI_4s_0(rd));
            // Pairs for uni/gather 32-bit: broadcast same zero to both halves
            if (ra->is_pair[i]) {
                int8_t rdh = ra_alloc(c, ra, sl, ns);
                ra->reg_hi[i] = rdh; ra->owner[(int)rdh] = i;
                put(c, MOVI_4s_0(rdh));
            }
        } break;

        case op_store_32: {
            int8_t ry = ra_ensure(c, ra, sl, ns, inst->y);
            int p = inst->ptr;
            if (!scalar && ra->is_pair[inst->y]) {
                int8_t ryh = ra->reg_hi[inst->y];
                put(c, ADD_xr(XT, 1+p, XW));
                put(c, STP_qi(ry, ryh, XT, 0));
            } else {
                if (scalar) put(c, STR_sx(ry, 1+p, XI));
                else        put(c, STR_q(ry, 1+p, XW));
            }
            if (lu[inst->y] <= i) ra_free_reg(ra, inst->y);
        } break;

        case op_store_16: case op_store_half: {
            int8_t ry = ra_ensure(c, ra, sl, ns, inst->y);
            int p = inst->ptr;
            if (scalar) {
                put(c, STR_hx(ry, 1+p, XI));
            } else {
                // K=8: Q register store
                put(c, STR_q(ry, 1+p, XH));
            }
            if (lu[inst->y] <= i) ra_free_reg(ra, inst->y);
        } break;

        case op_scatter_32: case op_scatter_16: case op_scatter_half: {
            if (lu[inst->y] <= i) ra_free_reg(ra, inst->y);
        } break;

        case op_load_8x4: {
            _Bool is_base = inst->x == 0;
            int ch = is_base ? 0 : inst->imm;
            int p  = is_base ? inst->ptr : bb->inst[inst->x].ptr;

            if (scalar) {
                int8_t rd = ra_alloc(c, ra, sl, ns);
                ra->reg[i] = rd; ra->owner[(int)rd] = i;
                put(c, LSL_xi(XT, XI, 2));
                if (ch) put(c, ADD_xi(XT, XT, ch));
                // LDRB Wt, [Xn, Xm]
                put(c, 0x38606800u | ((uint32_t)XT << 16) | ((uint32_t)(1+p) << 5) | (uint32_t)XT);
                // DUP Vd.8H, Wt (broadcast byte as u16)
                put(c, W(0x0E020C00u) | ((uint32_t)XT << 5) | (uint32_t)rd);
            } else if (is_base) {
                evict_scratch(c, ra, sl, ns);
                put(c, ADD_xr(XT, 1+p, XW));
                put(c, LD4_8b(0, XT));
                // Widen u8->u16: UXTL Vd.8H, Vn.8B (in-place)
                for (int c2 = 0; c2 < 4; c2++) {
                    put(c, UXTL_8h(c2, c2));
                }
                // Claim V0 for the base (ch0).
                ra->reg[i] = 0;
                ra->owner[0] = i;
                // Find continuations and claim V1-V3 for them.
                // Unclaimed channels go to the free stack.
                _Bool ch_claimed[] = {1, 0, 0, 0};
                for (int j = i+1; j < to; j++) {
                    if (bb->inst[j].op == op_load_8x4 && bb->inst[j].x == i) {
                        int c2 = bb->inst[j].imm;
                        ra->reg[j] = (int8_t)c2;
                        ra->owner[c2] = j;
                        ch_claimed[c2] = 1;
                    }
                }
                for (int c2 = 0; c2 < 4; c2++) {
                    if (!ch_claimed[c2]) ra->free_stack[ra->nfree++] = (int8_t)c2;
                }
            }
            // Continuation slots: already claimed by base, nothing to emit.
        } break;

        case op_store_8x4: {
            int p = inst->ptr;
            int const inputs[] = {inst->x, inst->y, inst->z, inst->w};
            if (scalar) {
                for (int ch = 0; ch < 4; ch++) {
                    int8_t ry = ra_ensure(c, ra, sl, ns, inputs[ch]);
                    put(c, LSL_xi(XT, XI, 2));
                    if (ch) put(c, ADD_xi(XT, XT, ch));
                    put(c, ADD_xr(XT, 1+p, XT));
                    put(c, ST1_b(ry, 0, XT));
                }
            } else {
                evict_scratch(c, ra, sl, ns);
                for (int ch = 0; ch < 4; ch++) {
                    int8_t ry = ra_ensure(c, ra, sl, ns, inputs[ch]);
                    if (ry != (int8_t)ch) put(c, ORR_16b(ch, ry, ry));
                }
                // Narrow u16->u8: XTN Vd.8B, Vn.8H (in-place)
                for (int ch = 0; ch < 4; ch++) {
                    put(c, XTN_8b(ch, ch));
                }
                put(c, ADD_xr(XT, 1+p, XW));
                put(c, ST4_8b(0, XT));
                // V0-V3 now hold interleaved data; clear ownership.
                // Don't push to free stack — they may already be there from
                // ra_free_reg of earlier LD4 outputs.  They'll re-enter the
                // pool naturally when future values in them die.
                for (int r = 0; r < 4; r++) {
                    ra->owner[r] = -1;
                }
            }
            for (int ch = 0; ch < 4; ch++) {
                if (lu[inputs[ch]] <= i) ra_free_reg(ra, inputs[ch]);
            }
        } break;

        // Cross-width conversions between 32-bit (pairs) and 16-bit (single Q)
        case op_half_from_f32: {
            int8_t rx = ra_ensure(c, ra, sl, ns, inst->x);
            int8_t rxh = hi(ra, inst->x);
            _Bool x_dead = lu[inst->x] <= i;
            int8_t rd;
            if (x_dead) {
                rd = ra_claim(ra, inst->x, i);
                // Pair freed, we keep lo; hi was freed by claim
            } else {
                rd = ra_alloc(c, ra, sl, ns);
                ra->reg[i] = rd; ra->owner[(int)rd] = i;
            }
            if (!scalar && rxh != rx) {
                // Pair: FCVTN lo, FCVTN2 hi
                put(c, FCVTN_4h(rd, rx));
                put(c, W(FCVTN_4h(rd, rxh)));  // FCVTN2
            } else {
                put(c, FCVTN_4h(rd, rx));
            }
        } break;

        case op_f32_from_half: {
            int8_t rx = ra_ensure(c, ra, sl, ns, inst->x);
            _Bool x_dead = lu[inst->x] <= i;
            if (!scalar && ra->is_pair[i]) {
                int8_t rd = ra_alloc(c, ra, sl, ns);
                int8_t rdh = ra_alloc(c, ra, sl, ns);
                ra->reg[i] = rd; ra->reg_hi[i] = rdh;
                ra->owner[(int)rd] = i; ra->owner[(int)rdh] = i;
                put(c, FCVTL_4s(rd, rx));       // lo from lower 4H
                put(c, W(FCVTL_4s(rdh, rx)));   // FCVTL2: hi from upper 4H
                if (x_dead) ra_free_reg(ra, inst->x);
            } else {
                int8_t rd;
                if (x_dead) { rd = ra_claim(ra, inst->x, i); }
                else { rd = ra_alloc(c, ra, sl, ns); ra->reg[i] = rd; ra->owner[(int)rd] = i; }
                put(c, FCVTL_4s(rd, rx));
            }
        } break;

        case op_half_from_i32: {
            int8_t rx = ra_ensure(c, ra, sl, ns, inst->x);
            int8_t rxh = hi(ra, inst->x);
            _Bool x_dead = lu[inst->x] <= i;
            int8_t rd;
            if (x_dead) { rd = ra_claim(ra, inst->x, i); }
            else { rd = ra_alloc(c, ra, sl, ns); ra->reg[i] = rd; ra->owner[(int)rd] = i; }
            if (!scalar && rxh != rx) {
                // SCVTF + FCVTN for lo, SCVTF + FCVTN2 for hi
                put(c, SCVTF_4s(0, rx));
                put(c, FCVTN_4h(rd, 0));
                put(c, SCVTF_4s(0, rxh));
                put(c, W(FCVTN_4h(rd, 0)));   // FCVTN2
            } else {
                put(c, SCVTF_4s(0, rx));
                put(c, FCVTN_4h(rd, 0));
            }
        } break;

        case op_i32_from_half: {
            int8_t rx = ra_ensure(c, ra, sl, ns, inst->x);
            _Bool x_dead = lu[inst->x] <= i;
            if (!scalar && ra->is_pair[i]) {
                int8_t rd = ra_alloc(c, ra, sl, ns);
                int8_t rdh = ra_alloc(c, ra, sl, ns);
                ra->reg[i] = rd; ra->reg_hi[i] = rdh;
                ra->owner[(int)rd] = i; ra->owner[(int)rdh] = i;
                put(c, FCVTL_4s(rd, rx));       // widen lower 4H to 4S
                put(c, FCVTZS_4s(rd, rd));      // convert to int
                put(c, W(FCVTL_4s(rdh, rx)));   // FCVTL2: widen upper 4H
                put(c, FCVTZS_4s(rdh, rdh));
                if (x_dead) ra_free_reg(ra, inst->x);
            } else {
                int8_t rd;
                if (x_dead) { rd = ra_claim(ra, inst->x, i); }
                else { rd = ra_alloc(c, ra, sl, ns); ra->reg[i] = rd; ra->owner[(int)rd] = i; }
                put(c, FCVTL_4s(0, rx));
                put(c, FCVTZS_4s(rd, 0));
            }
        } break;

        case op_i16_from_i32: {
            int8_t rx = ra_ensure(c, ra, sl, ns, inst->x);
            int8_t rxh = hi(ra, inst->x);
            _Bool x_dead = lu[inst->x] <= i;
            int8_t rd;
            if (x_dead) { rd = ra_claim(ra, inst->x, i); }
            else { rd = ra_alloc(c, ra, sl, ns); ra->reg[i] = rd; ra->owner[(int)rd] = i; }
            if (!scalar && rxh != rx) {
                put(c, XTN_4h(rd, rx));
                put(c, W(XTN_4h(rd, rxh)));   // XTN2
            } else {
                put(c, XTN_4h(rd, rx));
            }
        } break;

        case op_i32_from_i16: {
            int8_t rx = ra_ensure(c, ra, sl, ns, inst->x);
            _Bool x_dead = lu[inst->x] <= i;
            if (!scalar && ra->is_pair[i]) {
                int8_t rd = ra_alloc(c, ra, sl, ns);
                int8_t rdh = ra_alloc(c, ra, sl, ns);
                ra->reg[i] = rd; ra->reg_hi[i] = rdh;
                ra->owner[(int)rd] = i; ra->owner[(int)rdh] = i;
                put(c, SXTL_4s(rd, rx));        // lo from lower 4H
                put(c, W(SXTL_4s(rdh, rx)));    // SXTL2: hi from upper 4H
                if (x_dead) ra_free_reg(ra, inst->x);
            } else {
                int8_t rd;
                if (x_dead) { rd = ra_claim(ra, inst->x, i); }
                else { rd = ra_alloc(c, ra, sl, ns); ra->reg[i] = rd; ra->owner[(int)rd] = i; }
                put(c, SXTL_4s(rd, rx));
            }
        } break;

        case op_shr_u32_imm: {
            int sh = inst->imm;
            int8_t rx = ra_ensure(c, ra, sl, ns, inst->x);
            _Bool x_dead = lu[inst->x] <= i;
            _Bool pair = ra->is_pair[i] && !scalar;
            // Fuse shr_u32_imm + i16_from_i32 into SHRN when shift fits
            if (sh >= 1 && sh <= 16 && i+1 < to &&
                bb->inst[i+1].op == op_i16_from_i32 &&
                bb->inst[i+1].x == i && lu[i] == i+1)
            {
                int8_t rxh = hi(ra, inst->x);
                int8_t rd;
                if (x_dead) { rd = ra_claim(ra, inst->x, i+1); }
                else {
                    rd = ra_alloc(c, ra, sl, ns);
                    ra->reg[i+1] = rd; ra->owner[(int)rd] = i+1;
                }
                if (pair && rxh != rx) {
                    put(c, SHRN_4h(rd, rx, sh));
                    put(c, W(SHRN_4h(rd, rxh, sh)));  // SHRN2
                } else {
                    put(c, SHRN_4h(rd, rx, sh));
                }
                i++;
            } else if (pair) {
                int8_t rxh = hi(ra, inst->x);
                int8_t rd, rdh;
                if (x_dead) {
                    rd = ra_claim(ra, inst->x, i);
                    rdh = ra->reg_hi[i];
                } else {
                    rd = ra_alloc(c, ra, sl, ns);
                    rdh = ra_alloc(c, ra, sl, ns);
                    ra->reg[i] = rd; ra->reg_hi[i] = rdh;
                    ra->owner[(int)rd] = i; ra->owner[(int)rdh] = i;
                }
                put(c, USHR_4s_imm(rd, rx, sh));
                put(c, USHR_4s_imm(rdh, rxh, sh));
            } else {
                int8_t rd;
                if (x_dead) { rd = ra_claim(ra, inst->x, i); }
                else {
                    rd = ra_alloc(c, ra, sl, ns);
                    ra->reg[i] = rd; ra->owner[(int)rd] = i;
                }
                put(c, USHR_4s_imm(rd, rx, sh));
            }
        } break;

        case op_bytes: {
            int ctrl = inst->imm;
            int8_t ctrl_r = -1;
            for (int j = 0; j < ra->bytes_ctrl_n; j++) {
                if (ra->bytes_ctrl[j] == ctrl) { ctrl_r = ra->bytes_reg[j]; break; }
            }
            int8_t rx = ra_ensure(c, ra, sl, ns, inst->x);
            _Bool x_dead = lu[inst->x] <= i;
            _Bool pair = ra->is_pair[i] && !scalar;
            if (pair) {
                int8_t rxh = hi(ra, inst->x);
                int8_t rd, rdh;
                if (x_dead) {
                    rd = ra_claim(ra, inst->x, i);
                    rdh = ra->reg_hi[i];
                } else {
                    rd = ra_alloc(c, ra, sl, ns);
                    rdh = ra_alloc(c, ra, sl, ns);
                    ra->reg[i] = rd; ra->reg_hi[i] = rdh;
                    ra->owner[(int)rd] = i; ra->owner[(int)rdh] = i;
                }
                put(c, TBL_16b(rd, rx, ctrl_r));
                put(c, TBL_16b(rdh, rxh, ctrl_r));
            } else {
                int8_t rd;
                if (x_dead) { rd = ra_claim(ra, inst->x, i); }
                else {
                    rd = ra_alloc(c, ra, sl, ns);
                    ra->reg[i] = rd; ra->owner[(int)rd] = i;
                }
                put(c, TBL_16b(rd, rx, ctrl_r));
            }
        } break;

        default: {
            enum op_type ot = op_type(inst->op);
            _Bool pair = (ot == OP_32) && ra->is_pair[i] && !scalar;

            int8_t rx = inst->x < i ? ra_ensure(c, ra, sl, ns, inst->x) : 0;
            int8_t ry = inst->y < i ? ra_ensure(c, ra, sl, ns, inst->y) : 0;
            int8_t rz = inst->z < i ? ra_ensure(c, ra, sl, ns, inst->z) : 0;
            // Capture hi registers now, before claims/frees invalidate them.
            int8_t rxh = inst->x < i ? hi(ra, inst->x) : 0;
            int8_t ryh = inst->y < i ? hi(ra, inst->y) : 0;
            int8_t rzh = inst->z < i ? hi(ra, inst->z) : 0;

            _Bool x_dead = inst->x < i && lu[inst->x] <= i;
            _Bool y_dead = inst->y < i && lu[inst->y] <= i;
            _Bool z_dead = inst->z < i && lu[inst->z] <= i;
            if (inst->y == inst->x) y_dead = 0;
            if (inst->z == inst->x) z_dead = 0;
            if (inst->z == inst->y) z_dead = 0;

            int8_t rd = -1;
            enum op op = inst->op;
            _Bool destructive = op==op_fma_f32 || op==op_fma_half
                             || op==op_sel_32  || op==op_sel_16 || op==op_sel_half;
            if ((op==op_fma_f32 || op==op_fma_half) && z_dead)
                { rd = ra_claim(ra, inst->z, i); z_dead = 0; }
            else if ((op==op_sel_32 || op==op_sel_16 || op==op_sel_half) && x_dead)
                { rd = ra_claim(ra, inst->x, i); x_dead = 0; }

            if (!destructive) {
                if (rd < 0 && x_dead) { rd = ra_claim(ra, inst->x, i); x_dead = 0; }
                if (rd < 0 && y_dead) { rd = ra_claim(ra, inst->y, i); y_dead = 0; }
                if (rd < 0 && z_dead) { rd = ra_claim(ra, inst->z, i); z_dead = 0; }
            }

            if (x_dead) ra_free_reg(ra, inst->x);
            if (y_dead) ra_free_reg(ra, inst->y);
            if (z_dead) ra_free_reg(ra, inst->z);

            if (rd < 0) {
                rd = ra_alloc(c, ra, sl, ns);
                ra->reg[i] = rd; ra->owner[(int)rd] = i;
            }
            if (pair && ra->reg_hi[i] < 0) {
                int8_t rdh = ra_alloc(c, ra, sl, ns);
                ra->reg_hi[i] = rdh; ra->owner[(int)rdh] = i;
            }

            _Bool needs_scratch = op==op_shr_u32 || op==op_shr_s32
                               || op==op_shr_u16 || op==op_shr_s16
                               || ((op==op_fma_f32 || op==op_fma_half)
                                   && rd!=rz && (rd==rx || rd==ry));
            int8_t scratch_reg = -1;
            if (needs_scratch) {
                scratch_reg = ra_alloc(c, ra, sl, ns);
            }

            emit_alu_reg(c, inst->op, rd, rx, ry, rz, inst->imm, scratch_reg);

            if (pair) {
                int8_t rdh = ra->reg_hi[i];
                emit_alu_reg(c, inst->op, rdh, rxh, ryh, rzh, inst->imm, scratch_reg);
            }

            if (scratch_reg >= 0) {
                ra->free_stack[ra->nfree++] = scratch_reg;
            }
        } break;
        }
    }
}

void umbra_jit_run(struct umbra_jit *j, int n, void *p0, void *p1, void *p2, void *p3, void *p4, void *p5) {
    if (j) j->entry(n, p0, p1, p2, p3, p4, p5);
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

#endif
