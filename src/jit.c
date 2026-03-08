#include "../umbra.h"
#include "bb.h"

#if !defined(__aarch64__) && !defined(__AVX2__)

struct umbra_jit { int dummy; };
struct umbra_jit* umbra_jit(struct umbra_basic_block const *bb) { (void)bb; return 0; }
void umbra_jit_run (struct umbra_jit *j, int n, umbra_buf buf[]) {
    (void)j; (void)n; (void)buf;
}
void umbra_jit_free(struct umbra_jit *j) { (void)j; }
void umbra_jit_dump(struct umbra_jit const *j, FILE *f) { (void)j; (void)f; }
void umbra_jit_mca (struct umbra_jit const *j, FILE *f) { (void)j; (void)f; }

#elif defined(__aarch64__)

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
V3(AND_16b) V3(ORR_16b) V3(EOR_16b) V3(BSL_16b) V3(BIT_16b) V3(BIF_16b)
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
enum { XP=8, XI=9, XT=10, XH=11, XW=12, XS=15 };

static void load_imm_w(Buf *c, int rd, uint32_t v) {
    put(c, MOVZ_w(rd, (uint16_t)(v&0xffff)));
    if (v>>16) put(c, MOVK_w16(rd, (uint16_t)(v>>16)));
}

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
    case op_eq_half: put(c, W(FCMEQ_4h(d,x,y))); return 1;
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
    int     owner[32];    // owner[v] = inst whose value is in Vv, or -1
    int8_t  free_stack[32];
    int     _pad;
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
    struct ra *ra = ra_create(bb);

    put(&c, STP_pre(29,30,31,-2));
    put(&c, ADD_xi(29,31,0));
    int stack_patch = c.len;
    put(&c, 0xD503201Fu);  // NOP placeholder: patched to SUB SP if spills needed
    put(&c, 0xD503201Fu);  // NOP placeholder: patched to MOV XS,SP if spills needed

    // x0=n, x1=buf[].  Pointers loaded on demand via load_ptr().

    // Preamble: uniforms go straight into registers via RA.
    emit_ops(&c, bb, 0, bb->preamble, sl, &ns, ra, 0);

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
    j->entry = (void(*)(int,umbra_buf*))mem;
    return j;
}

static void emit_ops(Buf *c, struct umbra_basic_block const *bb,
                     int from, int to,
                     int *sl, int *ns,
                     struct ra *ra, _Bool scalar)
{
    int *lu = ra->last_use;
    int last_ptr = -1;  // cached pointer index in XP

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
            load_ptr(c, p, &last_ptr);
            if (!scalar && ra->is_pair[i]) {
                int8_t rd = ra_alloc(c, ra, sl, ns);
                int8_t rdh = ra_alloc(c, ra, sl, ns);
                ra->reg[i] = rd; ra->reg_hi[i] = rdh;
                ra->owner[(int)rd] = i; ra->owner[(int)rdh] = i;
                // LDP Qlo, Qhi, [base + XW]
                put(c, ADD_xr(XT, XP, XW));
                put(c, LDP_qi(rd, rdh, XT, 0));
            } else {
                int8_t rd = ra_alloc(c, ra, sl, ns);
                ra->reg[i] = rd; ra->owner[(int)rd] = i;
                if (scalar) put(c, LDR_sx(rd, XP, XI));
                else        put(c, LDR_q(rd, XP, XW));
            }
        } break;

        case op_load_16: case op_load_half: {
            int8_t rd = ra_alloc(c, ra, sl, ns);
            ra->reg[i] = rd; ra->owner[(int)rd] = i;
            int p = inst->ptr;
            load_ptr(c, p, &last_ptr);
            if (scalar) {
                put(c, LDR_hx(rd, XP, XI));
            } else {
                // K=8: 8 x 16-bit = 16 bytes = 1 Q register
                put(c, LDR_q(rd, XP, XH));
            }
        } break;

        case op_uni_32: {
            int8_t rd = ra_alloc(c, ra, sl, ns);
            ra->reg[i] = rd; ra->owner[(int)rd] = i;
            int p = inst->ptr;
            load_ptr(c, p, &last_ptr);
            int idx = bb->inst[inst->x].imm;
            load_imm_w(c, XT, (uint32_t)idx);
            put(c, LDR_sx(rd, XP, XT));  // Sd = ptr[idx] (LSL#2)
            // DUP Vd.4S, Vn.S[0] — broadcast scalar to all 4 lanes
            put(c, 0x4E040400u | ((uint32_t)rd<<5) | (uint32_t)rd);
            if (ra->is_pair[i]) {
                int8_t rdh = ra_alloc(c, ra, sl, ns);
                ra->reg_hi[i] = rdh; ra->owner[(int)rdh] = i;
                put(c, 0x4EA01C00u | ((uint32_t)rd<<5) | (uint32_t)rdh); // MOV rdh=rd
            }
        } break;

        case op_uni_16: case op_uni_half: {
            int8_t rd = ra_alloc(c, ra, sl, ns);
            ra->reg[i] = rd; ra->owner[(int)rd] = i;
            int p = inst->ptr;
            load_ptr(c, p, &last_ptr);
            int idx = bb->inst[inst->x].imm;
            load_imm_w(c, XT, (uint32_t)idx);
            put(c, LDR_hx(rd, XP, XT));  // Hd = ptr[idx] (LSL#1)
            // DUP Vd.8H, Vn.H[0]
            put(c, W(0x0E020400u | ((uint32_t)rd<<5) | (uint32_t)rd));
        } break;

        case op_gather_32: case op_gather_16: case op_gather_half: {
            int8_t rd = ra_alloc(c, ra, sl, ns);
            ra->reg[i] = rd; ra->owner[(int)rd] = i;
            put(c, MOVI_4s_0(rd));
            if (ra->is_pair[i]) {
                int8_t rdh = ra_alloc(c, ra, sl, ns);
                ra->reg_hi[i] = rdh; ra->owner[(int)rdh] = i;
                put(c, MOVI_4s_0(rdh));
            }
        } break;

        case op_store_32: {
            int8_t ry = ra_ensure(c, ra, sl, ns, inst->y);
            int p = inst->ptr;
            load_ptr(c, p, &last_ptr);
            if (!scalar && ra->is_pair[inst->y]) {
                int8_t ryh = ra->reg_hi[inst->y];
                put(c, ADD_xr(XT, XP, XW));
                put(c, STP_qi(ry, ryh, XT, 0));
            } else {
                if (scalar) put(c, STR_sx(ry, XP, XI));
                else        put(c, STR_q(ry, XP, XW));
            }
            if (lu[inst->y] <= i) ra_free_reg(ra, inst->y);
        } break;

        case op_store_16: case op_store_half: {
            int8_t ry = ra_ensure(c, ra, sl, ns, inst->y);
            int p = inst->ptr;
            load_ptr(c, p, &last_ptr);
            if (scalar) {
                put(c, STR_hx(ry, XP, XI));
            } else {
                // K=8: Q register store
                put(c, STR_q(ry, XP, XH));
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
                load_ptr(c, p, &last_ptr);
                int8_t rd = ra_alloc(c, ra, sl, ns);
                ra->reg[i] = rd; ra->owner[(int)rd] = i;
                put(c, LSL_xi(XT, XI, 2));
                if (ch) put(c, ADD_xi(XT, XT, ch));
                // LDRB Wt, [Xn, Xm]
                put(c, 0x38606800u | ((uint32_t)XT << 16) | ((uint32_t)XP << 5) | (uint32_t)XT);
                // DUP Vd.8H, Wt (broadcast byte as u16)
                put(c, W(0x0E020C00u) | ((uint32_t)XT << 5) | (uint32_t)rd);
            } else if (is_base) {
                load_ptr(c, p, &last_ptr);
                evict_scratch(c, ra, sl, ns);
                put(c, ADD_xr(XT, XP, XW));
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
            load_ptr(c, p, &last_ptr);
            int const inputs[] = {inst->x, inst->y, inst->z, inst->w};
            if (scalar) {
                for (int ch = 0; ch < 4; ch++) {
                    int8_t ry = ra_ensure(c, ra, sl, ns, inputs[ch]);
                    put(c, LSL_xi(XT, XI, 2));
                    if (ch) put(c, ADD_xi(XT, XT, ch));
                    put(c, ADD_xr(XT, XP, XT));
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
                put(c, ADD_xr(XT, XP, XW));
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

// ---- byte buffer ----
typedef struct { uint8_t *buf; int len,cap; } Buf;

static void emit1(Buf *b, uint8_t v) {
    if (b->len == b->cap) {
        b->cap = b->cap ? 2*b->cap : 4096;
        b->buf = realloc(b->buf, (size_t)b->cap);
    }
    b->buf[b->len++] = v;
}
static void emit4(Buf *b, uint32_t v) {
    emit1(b,(uint8_t)v); emit1(b,(uint8_t)(v>>8)); emit1(b,(uint8_t)(v>>16)); emit1(b,(uint8_t)(v>>24));
}

// ---- VEX encoding ----
// pp: 0=none,1=66,2=F3,3=F2  mm: 1=0F,2=0F38,3=0F3A  L: 0=128,1=256
// d=ModRM.reg  v=VEX.vvvv(NDS)  s=ModRM.rm

static void vex(Buf *b, int pp, int mm, int W, int L, int d, int v, int s, uint8_t op) {
    int R = ~d >> 3, X = 1, B = ~s >> 3;
    if (mm == 1 && W == 0 && (B & 1) && (X & 1)) {
        emit1(b, 0xC5);
        emit1(b, (uint8_t)(((R&1)<<7) | ((~v&0xf)<<3) | (L<<2) | pp));
    } else {
        emit1(b, 0xC4);
        emit1(b, (uint8_t)(((R&1)<<7) | ((X&1)<<6) | ((B&1)<<5) | mm));
        emit1(b, (uint8_t)((W<<7) | ((~v&0xf)<<3) | (L<<2) | pp));
    }
    emit1(b, op);
    emit1(b, (uint8_t)(0xC0 | ((d&7)<<3) | (s&7)));
}

// 3-operand: op ymm/xmm d, v, s
static void vex_rrr(Buf *b, int pp, int mm, int L, uint8_t op, int d, int v, int s) {
    vex(b,pp,mm,0,L,d,v,s,op);
}
// 2-operand: op ymm/xmm d, s  (vvvv unused = 0 -> encoded as 1111)
static void vex_rr(Buf *b, int pp, int mm, int L, uint8_t op, int d, int s) {
    vex(b,pp,mm,0,L,d,0,s,op);
}
// shift-by-immediate: ModRM.reg=ext, VEX.vvvv=d(dest), ModRM.rm=s(src), +imm8
static void vex_shift(Buf *b, int pp, int mm, int L, uint8_t op, int ext, int d, int s, uint8_t imm) {
    vex(b,pp,mm,0,L,ext,d,s,op);
    emit1(b, imm);
}
// VEX memory operands: [base + index*scale + disp]
// ModRM + optional SIB + optional disp
static void vex_mem(Buf *b, int pp, int mm, int W, int L, int reg, int v, uint8_t op,
                    int base, int index, int scale, int disp) {
    // reg goes in ModRM.reg, base in SIB.base or ModRM.rm
    int R = ~reg >> 3, X = ~index >> 3, B = ~base >> 3;
    emit1(b, 0xC4);
    emit1(b, (uint8_t)(((R&1)<<7) | ((X&1)<<6) | ((B&1)<<5) | mm));
    emit1(b, (uint8_t)((W<<7) | ((~v&0xf)<<3) | (L<<2) | pp));
    emit1(b, op);

    int mod = 0;
    if (disp == 0 && (base&7) != 5) mod = 0;
    else if (disp >= -128 && disp <= 127) mod = 1;
    else mod = 2;

    emit1(b, (uint8_t)((mod<<6) | ((reg&7)<<3) | 4));  // SIB follows
    int ss = (scale==1)?0 : (scale==2)?1 : (scale==4)?2 : 3;
    emit1(b, (uint8_t)((ss<<6) | ((index&7)<<3) | (base&7)));

    if (mod == 1) emit1(b, (uint8_t)(int8_t)disp);
    else if (mod == 2) emit4(b, (uint32_t)disp);
}

// Load: VMOVDQU ymm, [base+index*scale+disp] or VMOVUPS
static void vmov_load(Buf *b, int L, int reg, int base, int index, int scale, int disp) {
    // VMOVDQU: VEX.F3.0F 6F /r
    vex_mem(b, 2, 1, 0, L, reg, 0, 0x6F, base, index, scale, disp);
}
// Store: VMOVDQU [base+index*scale+disp], ymm
static void vmov_store(Buf *b, int L, int reg, int base, int index, int scale, int disp) {
    // VMOVDQU: VEX.F3.0F 7F /r  — reg is source, memory is dest
    vex_mem(b, 2, 1, 0, L, reg, 0, 0x7F, base, index, scale, disp);
}

// ---- GPR instructions ----
// REX prefix helpers
static void rex_w(Buf *b, int r, int b_) {
    emit1(b, (uint8_t)(0x48 | ((r>>3)<<2) | (b_>>3)));
}
// XOR r32,r32 (zero register)
static void xor_rr(Buf *b, int d, int s) {
    if (d >= 8 || s >= 8) emit1(b, (uint8_t)(0x40 | ((d>>3)<<2) | (s>>3)));
    emit1(b, 0x31);
    emit1(b, (uint8_t)(0xC0 | ((s&7)<<3) | (d&7)));
}
// ADD r64, imm32
static void add_ri(Buf *b, int d, int32_t imm) {
    rex_w(b, 0, d);
    if (imm >= -128 && imm <= 127) {
        emit1(b, 0x83);
        emit1(b, (uint8_t)(0xC0 | (d&7)));
        emit1(b, (uint8_t)(int8_t)imm);
    } else {
        emit1(b, 0x81);
        emit1(b, (uint8_t)(0xC0 | (d&7)));
        emit4(b, (uint32_t)imm);
    }
}
// SUB r64, imm32
static void sub_ri(Buf *b, int d, int32_t imm) {
    rex_w(b, 0, d);
    if (imm >= -128 && imm <= 127) {
        emit1(b, 0x83);
        emit1(b, (uint8_t)(0xC0 | (5<<3) | (d&7)));
        emit1(b, (uint8_t)(int8_t)imm);
    } else {
        emit1(b, 0x81);
        emit1(b, (uint8_t)(0xC0 | (5<<3) | (d&7)));
        emit4(b, (uint32_t)imm);
    }
}
// CMP r64, r64
static void cmp_rr(Buf *b, int a, int c) {
    rex_w(b, c, a);
    emit1(b, 0x39);
    emit1(b, (uint8_t)(0xC0 | ((c&7)<<3) | (a&7)));
}
// CMP r64, imm32
static void cmp_ri(Buf *b, int a, int32_t imm) {
    rex_w(b, 0, a);
    if (imm >= -128 && imm <= 127) {
        emit1(b, 0x83);
        emit1(b, (uint8_t)(0xC0 | (7<<3) | (a&7)));
        emit1(b, (uint8_t)(int8_t)imm);
    } else {
        emit1(b, 0x81);
        emit1(b, (uint8_t)(0xC0 | (7<<3) | (a&7)));
        emit4(b, (uint32_t)imm);
    }
}
// MOV r64, r64
static void mov_rr(Buf *b, int d, int s) {
    rex_w(b, s, d);
    emit1(b, 0x89);
    emit1(b, (uint8_t)(0xC0 | ((s&7)<<3) | (d&7)));
}
// MOV r64, [r64+disp]
static void mov_load(Buf *b, int d, int base, int disp) {
    rex_w(b, d, base);
    emit1(b, 0x8B);
    if (disp == 0 && (base&7) != 5) {
        emit1(b, (uint8_t)(((d&7)<<3) | (base&7)));
        if ((base&7) == 4) emit1(b, 0x24);
    } else if (disp >= -128 && disp <= 127) {
        emit1(b, (uint8_t)(0x40 | ((d&7)<<3) | (base&7)));
        if ((base&7) == 4) emit1(b, 0x24);
        emit1(b, (uint8_t)(int8_t)disp);
    } else {
        emit1(b, (uint8_t)(0x80 | ((d&7)<<3) | (base&7)));
        if ((base&7) == 4) emit1(b, 0x24);
        emit4(b, (uint32_t)disp);
    }
}
// JCC rel32 — returns patch position
static int jcc(Buf *b, uint8_t cc) {
    emit1(b, 0x0F);
    emit1(b, (uint8_t)(0x80 | cc));
    int pos = b->len;
    emit4(b, 0);
    return pos;
}
// JMP rel32
static int jmp(Buf *b) {
    emit1(b, 0xE9);
    int pos = b->len;
    emit4(b, 0);
    return pos;
}
// RET
static void ret(Buf *b) { emit1(b, 0xC3); }
// VZEROUPPER
static void vzeroupper(Buf *b) { emit1(b,0xC5); emit1(b,0xF8); emit1(b,0x77); }
// NOP (for alignment)
static void nop(Buf *b) { emit1(b, 0x90); }

// ---- x86 GPR numbering ----
enum { RAX=0,RCX=1,RDX=2,RBX=3,RSP=4,RBP=5,RSI=6,RDI=7,
       R8=8,R9=9,R10=10,R11=11,R12=12,R13=13,R14=14,R15=15 };

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

// ---- Spill: VMOVDQU [RSP+slot*32], ymm or VMOVDQU ymm, [RSP+slot*32] ----
static void vspill(Buf *b, int reg, int slot) {
    // VMOVDQU [RSP + slot*32], ymm
    int disp = slot * 32;
    int R = ~reg >> 3;
    emit1(b, 0xC5);
    emit1(b, (uint8_t)(((R&1)<<7) | 0x7E));  // VEX.256.F3.0F, vvvv=1111
    emit1(b, 0x7F);  // VMOVDQU store
    if (disp == 0) {
        emit1(b, (uint8_t)(((reg&7)<<3) | 4));  // ModRM: mod=0, rm=4 (SIB follows)
        emit1(b, 0x24);  // SIB: scale=1, index=RSP(none), base=RSP
    } else if (disp >= -128 && disp <= 127) {
        emit1(b, (uint8_t)(0x40 | ((reg&7)<<3) | 4));
        emit1(b, 0x24);
        emit1(b, (uint8_t)(int8_t)disp);
    } else {
        emit1(b, (uint8_t)(0x80 | ((reg&7)<<3) | 4));
        emit1(b, 0x24);
        emit4(b, (uint32_t)disp);
    }
}
static void vfill(Buf *b, int reg, int slot) {
    // VMOVDQU ymm, [RSP + slot*32]
    int disp = slot * 32;
    int R = ~reg >> 3;
    emit1(b, 0xC5);
    emit1(b, (uint8_t)(((R&1)<<7) | 0x7E));
    emit1(b, 0x6F);  // VMOVDQU load
    if (disp == 0) {
        emit1(b, (uint8_t)(((reg&7)<<3) | 4));
        emit1(b, 0x24);
    } else if (disp >= -128 && disp <= 127) {
        emit1(b, (uint8_t)(0x40 | ((reg&7)<<3) | 4));
        emit1(b, 0x24);
        emit1(b, (uint8_t)(int8_t)disp);
    } else {
        emit1(b, (uint8_t)(0x80 | ((reg&7)<<3) | 4));
        emit1(b, 0x24);
        emit4(b, (uint32_t)disp);
    }
}

// ---- AVX2 instruction helpers ----

// VMOVAPS ymm,ymm (for reg moves)
static void vmovaps(Buf *b, int d, int s) { vex_rr(b,0,1,1,0x28,d,s); }
// VMOVAPS xmm,xmm
static void vmovaps_x(Buf *b, int d, int s) { vex_rr(b,0,1,0,0x28,d,s); }

// VPXOR ymm,ymm,ymm (zero or xor)
static void vpxor(Buf *b, int L, int d, int v, int s) { vex_rrr(b,1,1,L,0xEF,d,v,s); }

// VBROADCASTSS ymm, xmm  — VEX.256.66.0F38 18 /r (from reg: VEX.W=0)
static void vbroadcastss(Buf *b, int d, int s) { vex_rrr(b,1,2,1,0x18,d,0,s); }

// Load immediate into xmm0 low dword then broadcast to ymm
static void broadcast_imm32(Buf *b, int d, uint32_t v) {
    if (v == 0) {
        vpxor(b, 1, d, d, d);
    } else {
        // MOV eax, imm32
        emit1(b, 0xB8); emit4(b, v);
        // VMOVD xmm0, eax — VEX.128.66.0F 6E /r
        vex(b, 1, 1, 0, 0, 0, 0, RAX, 0x6E);
        // VBROADCASTSS ymm_d, xmm0
        vbroadcastss(b, d, 0);
    }
}

// Broadcast 16-bit immediate to all 8 words of xmm
static void broadcast_imm16(Buf *b, int d, uint16_t v) {
    if (v == 0) {
        vpxor(b, 0, d, d, d);
    } else {
        // MOV eax, v
        emit1(b, 0xB8); emit4(b, (uint32_t)v);
        // VMOVD xmm0, eax
        vex(b, 1, 1, 0, 0, 0, 0, RAX, 0x6E);
        // VPBROADCASTW xmm_d, xmm0 — VEX.128.66.0F38 79
        vex_rr(b, 1, 2, 0, 0x79, d, 0);
    }
}

// Convert fp16 bits to f32 constant at JIT time, broadcast as f32
static void broadcast_half_imm(Buf *b, int d, uint16_t bits) {
    // Load fp16 bits into xmm0, convert to f32, broadcast
    if (bits == 0) {
        vpxor(b, 1, d, d, d);
    } else {
        emit1(b, 0xB8); emit4(b, (uint32_t)bits);
        // VMOVD xmm0, eax
        vex(b, 1, 1, 0, 0, 0, 0, RAX, 0x6E);
        // VCVTPH2PS xmm0, xmm0 — VEX.128.66.0F38 13
        vex_rr(b, 1, 2, 0, 0x13, 0, 0);
        // VBROADCASTSS ymm_d, xmm0
        vbroadcastss(b, d, 0);
    }
}

// ---- F32 arithmetic (YMM, L=1) ----
static void vaddps(Buf *b, int d, int v, int s) { vex_rrr(b,0,1,1,0x58,d,v,s); }
static void vsubps(Buf *b, int d, int v, int s) { vex_rrr(b,0,1,1,0x5C,d,v,s); }
static void vmulps(Buf *b, int d, int v, int s) { vex_rrr(b,0,1,1,0x59,d,v,s); }
static void vdivps(Buf *b, int d, int v, int s) { vex_rrr(b,0,1,1,0x5E,d,v,s); }
static void vminps(Buf *b, int d, int v, int s) { vex_rrr(b,0,1,1,0x5D,d,v,s); }
static void vmaxps(Buf *b, int d, int v, int s) { vex_rrr(b,0,1,1,0x5F,d,v,s); }
static void vsqrtps(Buf *b, int d, int s)       { vex_rr (b,0,1,1,0x51,d,s); }
// VFMADD231PS: d = v*s + d — VEX.256.66.0F38.W0 B8
static void vfmadd231ps(Buf *b, int d, int v, int s) {
    vex(b,1,2,0,1,d,v,s,0xB8);
}
// VCVTDQ2PS ymm,ymm — VEX.256.0F 5B
static void vcvtdq2ps(Buf *b, int d, int s) { vex_rr(b,0,1,1,0x5B,d,s); }
// VCVTTPS2DQ ymm,ymm — VEX.256.F3.0F 5B
static void vcvttps2dq(Buf *b, int d, int s) { vex_rr(b,2,1,1,0x5B,d,s); }
// VCMPPS ymm,ymm,ymm,imm8 — VEX.256.0F C2
static void vcmpps(Buf *b, int d, int v, int s, uint8_t pred) {
    vex_rrr(b,0,1,1,0xC2,d,v,s); emit1(b, pred);
}

// VCVTPH2PS ymm,xmm — VEX.256.66.0F38 13
static void vcvtph2ps(Buf *b, int d, int s) { vex_rr(b,1,2,1,0x13,d,s); }
// VCVTPS2PH xmm,ymm,imm8 — VEX.256.66.0F3A 1D (note: d=xmm dest is in ModRM.rm, s=ymm src in ModRM.reg)
static void vcvtps2ph(Buf *b, int d, int s, uint8_t rnd) {
    // This instruction is unusual: the xmm dest is rm, ymm src is reg
    vex_rrr(b,1,3,1,0x1D,s,0,d); emit1(b, rnd);
}

// ---- I32 arithmetic (YMM, L=1) ----
static void vpaddd(Buf *b, int d, int v, int s)  { vex_rrr(b,1,1,1,0xFE,d,v,s); }
static void vpsubd(Buf *b, int d, int v, int s)  { vex_rrr(b,1,1,1,0xFA,d,v,s); }
// VPMULLD: VEX.256.66.0F38 40
static void vpmulld(Buf *b, int d, int v, int s) { vex_rrr(b,1,2,1,0x40,d,v,s); }
// Variable shifts: VEX.256.66.0F38 47/45/46
static void vpsllvd(Buf *b, int d, int v, int s) { vex_rrr(b,1,2,1,0x47,d,v,s); }
static void vpsrlvd(Buf *b, int d, int v, int s) { vex_rrr(b,1,2,1,0x45,d,v,s); }
static void vpsravd(Buf *b, int d, int v, int s) { vex_rrr(b,1,2,1,0x46,d,v,s); }
// Shift by immediate: VEX.256.66.0F 72 /6,/2,/4
static void vpslld_i(Buf *b, int d, int s, uint8_t imm) { vex_shift(b,1,1,1,0x72,6,d,s,imm); }
static void vpsrld_i(Buf *b, int d, int s, uint8_t imm) { vex_shift(b,1,1,1,0x72,2,d,s,imm); }
static void vpsrad_i(Buf *b, int d, int s, uint8_t imm) { vex_shift(b,1,1,1,0x72,4,d,s,imm); }

// ---- Bitwise (YMM) ----
static void vpand(Buf *b, int L, int d, int v, int s)  { vex_rrr(b,1,1,L,0xDB,d,v,s); }
static void vpor(Buf *b, int L, int d, int v, int s)   { vex_rrr(b,1,1,L,0xEB,d,v,s); }
static void vpxor_3(Buf *b, int L, int d, int v, int s) { vex_rrr(b,1,1,L,0xEF,d,v,s); }
static void vpandn(Buf *b, int L, int d, int v, int s) { vex_rrr(b,1,1,L,0xDF,d,v,s); } // ~v & s

// ---- I32 compare ----
// VPCMPEQD: VEX.256.66.0F 76
static void vpcmpeqd(Buf *b, int d, int v, int s) { vex_rrr(b,1,1,1,0x76,d,v,s); }
// VPCMPGTD: VEX.256.66.0F 66
static void vpcmpgtd(Buf *b, int d, int v, int s) { vex_rrr(b,1,1,1,0x66,d,v,s); }

// ---- I16 arithmetic (XMM, L=0) ----
static void vpaddw(Buf *b, int d, int v, int s)  { vex_rrr(b,1,1,0,0xFD,d,v,s); }
static void vpsubw(Buf *b, int d, int v, int s)  { vex_rrr(b,1,1,0,0xF9,d,v,s); }
static void vpmullw(Buf *b, int d, int v, int s) { vex_rrr(b,1,1,0,0xD5,d,v,s); }
// Shift by immediate: VEX.128.66.0F 71 /6,/2,/4
static void vpsllw_i(Buf *b, int d, int s, uint8_t imm) { vex_shift(b,1,1,0,0x71,6,d,s,imm); }
static void vpsrlw_i(Buf *b, int d, int s, uint8_t imm) { vex_shift(b,1,1,0,0x71,2,d,s,imm); }
static void vpsraw_i(Buf *b, int d, int s, uint8_t imm) { vex_shift(b,1,1,0,0x71,4,d,s,imm); }

// I16 compare (XMM)
static void vpcmpeqw(Buf *b, int d, int v, int s) { vex_rrr(b,1,1,0,0x75,d,v,s); }
static void vpcmpgtw(Buf *b, int d, int v, int s) { vex_rrr(b,1,1,0,0x65,d,v,s); }

// Widening/narrowing
// VPMOVSXWD ymm,xmm — VEX.256.66.0F38 23 (sign-extend i16->i32)
static void vpmovsxwd(Buf *b, int d, int s) { vex_rr(b,1,2,1,0x23,d,s); }
// VPMOVZXWD ymm,xmm — VEX.256.66.0F38 33 (zero-extend i16->i32)
static void vpmovzxwd(Buf *b, int d, int s) { vex_rr(b,1,2,1,0x33,d,s); }
static void vpackuswb(Buf *b, int d, int v, int s) { vex_rrr(b,1,1,0,0x67,d,v,s); }
static void vpunpcklbw(Buf *b, int d, int v, int s) { vex_rrr(b,1,1,0,0x60,d,v,s); }

// VEXTRACTI128 xmm,ymm,imm8 — VEX.256.66.0F3A 39
static void vextracti128(Buf *b, int d, int s, uint8_t imm) {
    vex_rrr(b,1,3,1,0x39,s,0,d); emit1(b, imm);
}
// VINSERTI128 ymm,ymm,xmm,imm8 — VEX.256.66.0F3A 38
static void vinserti128(Buf *b, int d, int v, int s, uint8_t imm) {
    vex_rrr(b,1,3,1,0x38,d,v,s); emit1(b, imm);
}

// ---- Register allocator (simplified from ARM64: no pairs) ----
static const int8_t ra_pool[] = {2,3,4,5,6,7,8,9,10,11,12,13,14,15};
#define RA_NREGS 14

struct ra {
    int    *last_use;
    int8_t *reg;
    int     nfree;
    int     owner[16];       // owner[ymm] = inst whose value is in that reg, or -1
    int8_t  free_stack[16];
    int     _pad;
};

static struct ra* ra_create(struct umbra_basic_block const *bb) {
    int n = bb->insts;
    struct ra *ra = malloc(sizeof *ra);
    ra->reg      = malloc((size_t)n * sizeof *ra->reg);
    ra->last_use = malloc((size_t)n * sizeof *ra->last_use);

    for (int i = 0; i < n; i++) ra->reg[i] = -1;
    for (int i = 0; i < 16; i++) ra->owner[i] = -1;

    for (int i = 0; i < n; i++) ra->last_use[i] = -1;
    for (int i = 0; i < n; i++) {
        struct bb_inst const *inst = &bb->inst[i];
        if (!(inst->op == op_load_8x4 && inst->x)) ra->last_use[inst->x] = i;
        ra->last_use[inst->y] = i;
        ra->last_use[inst->z] = i;
        ra->last_use[inst->w] = i;
    }
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
    free(ra->last_use);
    free(ra);
}

static void ra_free_reg(struct ra *ra, int val) {
    int8_t r = ra->reg[val];
    if (r < 0) return;
    ra->free_stack[ra->nfree++] = r;
    ra->owner[(int)r] = -1;
    ra->reg[val] = -1;
}

static int8_t ra_alloc(Buf *c, struct ra *ra, int *sl, int *ns) {
    if (ra->nfree > 0) return ra->free_stack[--ra->nfree];
    // Evict farthest-used (Belady)
    int best_r = -1, best_lu = -1;
    for (int r = 0; r < 16; r++) {
        if (ra->owner[r] < 0) continue;
        int val = ra->owner[r];
        if (ra->last_use[val] > best_lu) { best_lu = ra->last_use[val]; best_r = r; }
    }
    int evicted = ra->owner[best_r];
    if (sl[evicted] < 0) sl[evicted] = (*ns)++;
    vspill(c, ra->reg[evicted], sl[evicted]);
    int8_t r = ra->reg[evicted];
    ra->owner[(int)r] = -1;
    ra->reg[evicted] = -1;
    return r;
}

static int8_t ra_ensure(Buf *c, struct ra *ra, int *sl, int *ns, int val) {
    if (ra->reg[val] >= 0) return ra->reg[val];
    int8_t r = ra_alloc(c, ra, sl, ns);
    if (sl[val] >= 0) vfill(c, r, sl[val]);
    ra->reg[val] = r;
    ra->owner[(int)r] = val;
    return r;
}

static int8_t ra_claim(struct ra *ra, int old_val, int new_val) {
    int8_t r = ra->reg[old_val];
    ra->reg[old_val] = -1;
    ra->reg[new_val] = r;
    ra->owner[(int)r] = new_val;
    return r;
}

// ---- ALU emission ----
// d,x,y,z are YMM/XMM register numbers.  scratch is a free register.
static _Bool emit_alu_reg(Buf *c, enum op op, int d, int x, int y, int z, int imm, int scratch) {
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
    case op_fma_f32:
        // VFMADD231PS d,x,y : d = x*y + d, so z must be in d
        if (d == z) { vfmadd231ps(c,d,x,y); }
        else if (d != x && d != y) { vmovaps(c,d,z); vfmadd231ps(c,d,x,y); }
        else { vmovaps(c,scratch,z); vfmadd231ps(c,scratch,x,y); vmovaps(c,d,scratch); }
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
    case op_sel_32:
        // sel(mask=x, true=y, false=z) = (x & y) | (~x & z)
        vpand(c,1,scratch,x,y);
        vpandn(c,1,d,x,z);
        vpor(c,1,d,d,scratch);
        return 1;

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
        vex_rrr(c,1,2,1,0x3B,0,x,y);   // VPMINUD ymm0, x, y
        vpcmpeqd(c,d,y,0);
        vpcmpeqd(c,scratch,scratch,scratch);
        vpxor_3(c,1,d,d,scratch);
        return 1;
    case op_le_u32:  // x <=u y  ≡  y == max_u(x,y)
        vex_rrr(c,1,2,1,0x3F,0,x,y);   // VPMAXUD ymm0, x, y
        vpcmpeqd(c,d,y,0);
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
        vpmovzxwd(c,0,x);              // ymm0 = 8×i32 data (zero-extended)
        vpmovzxwd(c,1,y);              // ymm1 = 8×i32 shifts
        vpsllvd(c,0,0,1);              // ymm0 = shifted results
        vextracti128(c,1,0,1);         // xmm1 = hi 4 dwords
        // VPACKUSDW xmm_d, xmm0, xmm1: VEX.128.66.0F38 2B
        vex_rrr(c,1,2,0,0x2B,d,0,1);
        return 1;
    case op_shr_u16:
        vpmovzxwd(c,0,x);
        vpmovzxwd(c,1,y);
        vpsrlvd(c,0,0,1);
        vextracti128(c,1,0,1);
        vex_rrr(c,1,2,0,0x2B,d,0,1);  // VPACKUSDW xmm
        return 1;
    case op_shr_s16:
        vpmovsxwd(c,0,x);              // sign-extend for signed shift
        vpmovzxwd(c,1,y);
        vpsravd(c,0,0,1);
        vextracti128(c,1,0,1);
        vex_rrr(c,1,1,0,0x6B,d,0,1);  // VPACKSSDW xmm (signed saturation ok for shift right)
        return 1;

    // i16 bitwise (XMM)
    case op_and_16: vpand(c,0,d,x,y); return 1;
    case op_or_16:  vpor(c,0,d,x,y);  return 1;
    case op_xor_16: vpxor_3(c,0,d,x,y); return 1;
    case op_sel_16:
        vpand(c,0,scratch,x,y);
        vpandn(c,0,d,x,z);
        vpor(c,0,d,d,scratch);
        return 1;

    // i16 compare (XMM)
    case op_eq_i16: vpcmpeqw(c,d,x,y); return 1;
    case op_lt_s16: vpcmpgtw(c,d,y,x); return 1;
    case op_le_s16:
        vpcmpgtw(c,d,x,y);
        vpcmpeqw(c,scratch,scratch,scratch);
        vpxor_3(c,0,d,d,scratch);
        return 1;
    case op_lt_u16:  // x <u y  ≡  ¬(y == min_u(x,y))
        vex_rrr(c,1,2,0,0x3A,0,x,y);   // VPMINUW xmm0, x, y
        vpcmpeqw(c,d,y,0);
        vpcmpeqw(c,scratch,scratch,scratch);
        vpxor_3(c,0,d,d,scratch);
        return 1;
    case op_le_u16:  // x <=u y  ≡  y == max_u(x,y)
        vex_rrr(c,1,2,0,0x3E,0,x,y);   // VPMAXUW xmm0, x, y
        vpcmpeqw(c,d,y,0);
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
        if (d == z) { vfmadd231ps(c,d,x,y); }
        else if (d != x && d != y) { vmovaps(c,d,z); vfmadd231ps(c,d,x,y); }
        else { vmovaps(c,scratch,z); vfmadd231ps(c,scratch,x,y); vmovaps(c,d,scratch); }
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

    default: return 0;
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
    struct ra *ra = ra_create(bb);

    // SysV ABI: RDI=n, RSI=buf[].  Pointers loaded on demand via load_ptr_x86().

    // Reserve stack space for spills (patched later)
    int stack_patch = c.len;
    // Placeholder: SUB RSP, imm32 (7 bytes: REX.W + 81 /5 + imm32)
    for (int i = 0; i < 7; i++) nop(&c);

    // Preamble
    emit_ops(&c, bb, 0, bb->preamble, sl, &ns, ra, 0);

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

    // Scalar tail: free all varying registers
    for (int i = bb->preamble; i < bb->insts; i++) ra_free_reg(ra, i);
    for (int i = bb->preamble; i < bb->insts; i++) sl[i] = -1;

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
    int *lu = ra->last_use;
    int last_ptr = -1;  // cached pointer index in R11

    for (int i = from; i < to; i++) {
        struct bb_inst const *inst = &bb->inst[i];
        switch (inst->op) {
        case op_lane: {
            int8_t rd = ra_alloc(c, ra, sl, ns);
            ra->reg[i] = rd; ra->owner[(int)rd] = i;
            if (scalar) {
                vex(c, 1, 1, 0, 0, rd, 0, XI, 0x6E);   // VMOVD xmm_rd, R10d
            } else {
                // ymm_rd = broadcast(R10d) + [0,1,2,3,4,5,6,7]
                vex(c, 1, 1, 0, 0, rd, 0, XI, 0x6E);   // VMOVD xmm_rd, r10d
                vbroadcastss(c, rd, rd);

                // Build [0..7] on stack, load into ymm0
                sub_ri(c, RSP, 32);
                for (int k = 0; k < 8; k++) {
                    emit1(c, 0xC7);                      // MOV dword [RSP+k*4], k
                    if (k == 0) {
                        emit1(c, 0x04); emit1(c, 0x24);
                    } else {
                        emit1(c, 0x44); emit1(c, 0x24); emit1(c, (uint8_t)(k*4));
                    }
                    emit4(c, (uint32_t)k);
                }
                vfill(c, 0, 0);                          // VMOVDQU ymm0, [RSP]
                vpaddd(c, rd, rd, 0);
                add_ri(c, RSP, 32);
            }
        } break;

        case op_load_32: {
            int8_t rd = ra_alloc(c, ra, sl, ns);
            ra->reg[i] = rd; ra->owner[(int)rd] = i;
            int p = inst->ptr;
            if (scalar) {
                // VMOVD xmm, [base + R10*4]
                vex_mem(c, 1, 1, 0, 0, rd, 0, 0x6E, load_ptr_x86(c, p, &last_ptr), XI, 4, 0);
            } else {
                // VMOVDQU ymm, [base + R10*4]
                vmov_load(c, 1, rd, load_ptr_x86(c, p, &last_ptr), XI, 4, 0);
            }
        } break;

        case op_load_16: {
            int8_t rd = ra_alloc(c, ra, sl, ns);
            ra->reg[i] = rd; ra->owner[(int)rd] = i;
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
                vex(c, 1, 1, 0, 0, rd, 0, RAX, 0x6E);
            } else {
                // VMOVDQU xmm, [base + R10*2] — 8 × 16-bit = 16 bytes
                vmov_load(c, 0, rd, load_ptr_x86(c, p, &last_ptr), XI, 2, 0);
            }
        } break;

        case op_load_half: {
            int8_t rd = ra_alloc(c, ra, sl, ns);
            ra->reg[i] = rd; ra->owner[(int)rd] = i;
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
                vex(c, 1, 1, 0, 0, 0, 0, RAX, 0x6E);   // VMOVD xmm0, eax
                vcvtph2ps(c, rd, 0);                      // VCVTPH2PS xmm_rd, xmm0
            } else {
                // Load 8 × fp16 = 16 bytes into xmm, then VCVTPH2PS to ymm
                vmov_load(c, 0, 0, load_ptr_x86(c, p, &last_ptr), XI, 2, 0);  // xmm0
                vcvtph2ps(c, rd, 0);                         // ymm_rd
            }
        } break;

        case op_store_32: {
            int8_t ry = ra_ensure(c, ra, sl, ns, inst->y);
            int p = inst->ptr;
            if (scalar) {
                // VMOVD [base + R10*4], xmm
                vex_mem(c, 1, 1, 0, 0, ry, 0, 0x7E, load_ptr_x86(c, p, &last_ptr), XI, 4, 0);
            } else {
                vmov_store(c, 1, ry, load_ptr_x86(c, p, &last_ptr), XI, 4, 0);
            }
            if (lu[inst->y] <= i) ra_free_reg(ra, inst->y);
        } break;

        case op_store_16: {
            int8_t ry = ra_ensure(c, ra, sl, ns, inst->y);
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
            if (lu[inst->y] <= i) ra_free_reg(ra, inst->y);
        } break;

        case op_store_half: {
            int8_t ry = ra_ensure(c, ra, sl, ns, inst->y);
            int p = inst->ptr;
            if (scalar) {
                // VCVTPS2PH xmm0, xmm_ry, 0 (round to nearest)
                vcvtps2ph(c, 0, ry, 4);  // imm8=4: _MM_FROUND_CUR_DIRECTION
                // VMOVD eax, xmm0; MOV word [base + R10*2], ax
                vex(c, 1, 1, 0, 0, 0, 0, RAX, 0x7E);
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
                // VCVTPS2PH xmm0, ymm_ry, 4
                vcvtps2ph(c, 0, ry, 4);
                // Store 16 bytes (8 × fp16) to [base + R10*2]
                vmov_store(c, 0, 0, load_ptr_x86(c, p, &last_ptr), XI, 2, 0);
            }
            if (lu[inst->y] <= i) ra_free_reg(ra, inst->y);
        } break;

        case op_uni_32: {
            int8_t rd = ra_alloc(c, ra, sl, ns);
            ra->reg[i] = rd; ra->owner[(int)rd] = i;
            int p = inst->ptr;
            int idx = bb->inst[inst->x].imm;
            int base = load_ptr_x86(c, p, &last_ptr);
            int disp = idx * 4;
            // VBROADCASTSS ymm_rd, dword [base + disp]
            // VEX.256.66.0F38 18 /r with memory operand
            {
                uint8_t R = (uint8_t)(~rd >> 3) & 1;
                uint8_t B = (uint8_t)(~base >> 3) & 1;
                // 3-byte VEX: C4 RXBmmmmm WvvvvLpp
                emit1(c, 0xC4);
                emit1(c, (uint8_t)((R<<7) | (1<<6) | (B<<5) | 0x02));  // R.1.B.00010 (0F38)
                emit1(c, (uint8_t)(0x7D));  // W=0, vvvv=1111, L=1(256), pp=01 → 0.1111.1.01=0x7D
                emit1(c, 0x18);
                if (disp == 0 && (base & 7) != RBP) {
                    emit1(c, (uint8_t)(((rd & 7) << 3) | (base & 7)));
                    if ((base & 7) == RSP) emit1(c, 0x24);
                } else if (disp >= -128 && disp <= 127) {
                    emit1(c, (uint8_t)(0x40 | ((rd & 7) << 3) | (base & 7)));
                    if ((base & 7) == RSP) emit1(c, 0x24);
                    emit1(c, (uint8_t)disp);
                } else {
                    emit1(c, (uint8_t)(0x80 | ((rd & 7) << 3) | (base & 7)));
                    if ((base & 7) == RSP) emit1(c, 0x24);
                    emit4(c, (uint32_t)disp);
                }
            }
            if (lu[inst->x] <= i) ra_free_reg(ra, inst->x);
        } break;

        case op_uni_16: {
            int8_t rd = ra_alloc(c, ra, sl, ns);
            ra->reg[i] = rd; ra->owner[(int)rd] = i;
            int p = inst->ptr;
            int idx = bb->inst[inst->x].imm;
            int base = load_ptr_x86(c, p, &last_ptr);
            // MOVZX eax, word [base + idx*2]
            {
                uint8_t rex = 0x40;
                if (base >= 8) rex |= 0x01;
                emit1(c, rex);
                emit1(c, 0x0F); emit1(c, 0xB7);
                int disp = idx * 2;
                if (disp == 0 && (base & 7) != RBP) {
                    emit1(c, (uint8_t)(((RAX & 7) << 3) | (base & 7)));
                    if ((base & 7) == RSP) emit1(c, 0x24);
                } else {
                    emit1(c, (uint8_t)(0x40 | ((RAX & 7) << 3) | (base & 7)));
                    if ((base & 7) == RSP) emit1(c, 0x24);
                    emit1(c, (uint8_t)disp);
                }
            }
            // VMOVD xmm0, eax
            vex(c, 1, 1, 0, 0, 0, 0, RAX, 0x6E);
            // VPBROADCASTW xmm_rd, xmm0
            vex_rr(c, 1, 2, 0, 0x79, rd, 0);
            if (lu[inst->x] <= i) ra_free_reg(ra, inst->x);
        } break;

        case op_uni_half: {
            int8_t rd = ra_alloc(c, ra, sl, ns);
            ra->reg[i] = rd; ra->owner[(int)rd] = i;
            int p = inst->ptr;
            int idx = bb->inst[inst->x].imm;
            int base = load_ptr_x86(c, p, &last_ptr);
            // MOVZX eax, word [base + idx*2]
            {
                uint8_t rex = 0x40;
                if (base >= 8) rex |= 0x01;
                emit1(c, rex);
                emit1(c, 0x0F); emit1(c, 0xB7);
                int disp = idx * 2;
                if (disp == 0 && (base & 7) != RBP) {
                    emit1(c, (uint8_t)(((RAX & 7) << 3) | (base & 7)));
                    if ((base & 7) == RSP) emit1(c, 0x24);
                } else {
                    emit1(c, (uint8_t)(0x40 | ((RAX & 7) << 3) | (base & 7)));
                    if ((base & 7) == RSP) emit1(c, 0x24);
                    emit1(c, (uint8_t)disp);
                }
            }
            // VMOVD xmm0, eax; VPBROADCASTW xmm0, xmm0; VCVTPH2PS ymm_rd, xmm0
            vex(c, 1, 1, 0, 0, 0, 0, RAX, 0x6E);
            vex_rr(c, 1, 2, 0, 0x79, 0, 0);
            vcvtph2ps(c, rd, 0);
            if (lu[inst->x] <= i) ra_free_reg(ra, inst->x);
        } break;

        case op_gather_32: case op_gather_16: case op_gather_half: {
            int8_t rd = ra_alloc(c, ra, sl, ns);
            ra->reg[i] = rd; ra->owner[(int)rd] = i;
            vpxor(c, 1, rd, rd, rd);
        } break;

        case op_scatter_32: case op_scatter_16: case op_scatter_half: {
            if (lu[inst->y] <= i) ra_free_reg(ra, inst->y);
        } break;

        case op_load_8x4: {
            _Bool is_base = inst->x == 0;
            int ch = is_base ? 0 : inst->imm;
            int p = is_base ? inst->ptr : bb->inst[inst->x].ptr;

            if (scalar) {
                int8_t rd = ra_alloc(c, ra, sl, ns);
                ra->reg[i] = rd; ra->owner[(int)rd] = i;
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
                vex(c, 1, 1, 0, 0, rd, 0, RAX, 0x6E);  // VMOVD xmm, eax (as u16, zero-extended)
            } else if (is_base) {
                // Load 8 pixels × 4 channels = 32 bytes of interleaved RGBA u8 data.
                // Deinterleave into 4 × 8-element u16 XMM registers.
                // Strategy: each pixel is a dword. Shift right by ch*8 and mask 0xFF
                // to isolate one channel in each of 8 dwords (YMM). Then narrow
                // 8×u32(YMM) → 8×u16(XMM) via VEXTRACTI128 + VPACKUSDW.

                // ymm1 = broadcast 0xFF mask
                emit1(c, 0xB8); emit4(c, 0x000000FFu);           // MOV eax, 0xFF
                vex(c, 1, 1, 1, 0, 1, 0, RAX, 0x6E);            // VMOVD xmm1, eax
                vex_rr(c, 1, 2, 1, 0x58, 1, 1);                  // VPBROADCASTD ymm1, xmm1

                int8_t ch_regs[4];
                int8_t loaded = ra_alloc(c, ra, sl, ns);
                vmov_load(c, 1, loaded, load_ptr_x86(c, p, &last_ptr), XI, 4, 0);   // ymm_loaded = 8 RGBA pixels
                for (int ch2 = 0; ch2 < 4; ch2++) {
                    if (ch2 == 0) vpand(c, 1, 0, loaded, 1);     // ymm0 = loaded & 0xFF
                    else {
                        vpsrld_i(c, 0, loaded, (uint8_t)(ch2*8));
                        vpand(c, 1, 0, 0, 1);                    // ymm0 &= 0xFF
                    }

                    int8_t rd = ra_alloc(c, ra, sl, ns);
                    ch_regs[ch2] = rd;
                    vextracti128(c, rd, 0, 1);                    // xmm_rd = hi128 of ymm0
                    vex_rrr(c, 1, 2, 0, 0x2B, rd, 0, rd);        // VPACKUSDW: 4+4 u32 → 8 u16
                }

                // Free the temp register holding the loaded data.
                // loaded is a register number, not a value index, so free directly.
                ra->free_stack[ra->nfree++] = loaded;

                // Fix up: base instruction owns ch0
                ra->reg[i] = ch_regs[0]; ra->owner[(int)ch_regs[0]] = i;

                // Assign continuation channels
                for (int j = i+1; j < to; j++) {
                    if (bb->inst[j].op == op_load_8x4 && bb->inst[j].x == i) {
                        int c2 = bb->inst[j].imm;
                        ra->reg[j] = ch_regs[c2];
                        ra->owner[(int)ch_regs[c2]] = j;
                    }
                }
            }
        } break;

        case op_store_8x4: {
            int p = inst->ptr;
            int const inputs[] = {inst->x, inst->y, inst->z, inst->w};
            if (scalar) {
                for (int ch = 0; ch < 4; ch++) {
                    int8_t ry = ra_ensure(c, ra, sl, ns, inputs[ch]);
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
                for (int ch = 0; ch < 4; ch++) ra_ensure(c, ra, sl, ns, inputs[ch]);
                int8_t r0 = ra->reg[inputs[0]], r1 = ra->reg[inputs[1]];
                int8_t r2 = ra->reg[inputs[2]], r3 = ra->reg[inputs[3]];

                // Pack each channel u16→u8 individually (with zero high half):
                int8_t zero = ra_alloc(c, ra, sl, ns);
                vpxor(c, 0, zero, zero, zero);
                vpackuswb(c, 0, r0, zero);            // xmm0 = [R0..R7, 0..0]
                vpackuswb(c, 1, r1, zero);            // xmm1 = [G0..G7, 0..0]
                vpunpcklbw(c, 0, 0, 1);               // xmm0 = [R0,G0,R1,G1,...,R7,G7]

                vpackuswb(c, 1, r2, zero);            // xmm1 = [B0..B7, 0..0]
                {
                    int8_t tmp = ra_alloc(c, ra, sl, ns);
                    vpackuswb(c, tmp, r3, zero);       // tmp  = [A0..A7, 0..0]
                    vpunpcklbw(c, 1, 1, tmp);          // xmm1 = [B0,A0,B1,A1,...,B7,A7]
                    ra->free_stack[ra->nfree++] = tmp;
                }
                ra->free_stack[ra->nfree++] = zero;

                // xmm0=[RG pairs], xmm1=[BA pairs] → interleave words for RGBA pixels
                {
                    int8_t stmp = ra_alloc(c, ra, sl, ns);
                    vmovaps_x(c, stmp, 0);
                    vex_rrr(c, 1, 1, 0, 0x61, 0, stmp, 1);  // VPUNPCKLWD → first 4 pixels
                    vex_rrr(c, 1, 1, 0, 0x69, 1, stmp, 1);  // VPUNPCKHWD → last 4 pixels
                    ra->free_stack[ra->nfree++] = stmp;
                }
                vinserti128(c, 0, 0, 1, 1);            // ymm0 = [first4 | last4]
                vmov_store(c, 1, 0, load_ptr_x86(c, p, &last_ptr), XI, 4, 0);
            }
            for (int ch = 0; ch < 4; ch++) {
                if (lu[inputs[ch]] <= i) ra_free_reg(ra, inputs[ch]);
            }
        } break;

        // Halfs are carried as f32 in YMM. Round to fp16 precision via VCVTPS2PH+VCVTPH2PS.
        case op_half_from_f32: {
            int8_t rx = ra_ensure(c, ra, sl, ns, inst->x);
            _Bool x_dead = lu[inst->x] <= i;
            int8_t rd;
            if (x_dead) { rd = ra_claim(ra, inst->x, i); }
            else { rd = ra_alloc(c, ra, sl, ns); ra->reg[i] = rd; ra->owner[(int)rd] = i; }
            // Round to fp16 precision: f32 → fp16 → f32
            vcvtps2ph(c, 0, rx, 4);   // xmm0 = fp16
            vcvtph2ps(c, rd, 0);      // ymm_rd = f32
        } break;

        case op_f32_from_half: {
            // Half is already f32 in YMM. This is a no-op (or just a move).
            int8_t rx = ra_ensure(c, ra, sl, ns, inst->x);
            _Bool x_dead = lu[inst->x] <= i;
            int8_t rd;
            if (x_dead) { rd = ra_claim(ra, inst->x, i); }
            else { rd = ra_alloc(c, ra, sl, ns); ra->reg[i] = rd; ra->owner[(int)rd] = i; }
            if (rd != rx) vmovaps(c, rd, rx);
        } break;

        case op_half_from_i32: {
            int8_t rx = ra_ensure(c, ra, sl, ns, inst->x);
            _Bool x_dead = lu[inst->x] <= i;
            int8_t rd;
            if (x_dead) { rd = ra_claim(ra, inst->x, i); }
            else { rd = ra_alloc(c, ra, sl, ns); ra->reg[i] = rd; ra->owner[(int)rd] = i; }
            vcvtdq2ps(c, rd, rx);     // i32 → f32
            // Round through fp16 for precision
            vcvtps2ph(c, 0, rd, 4);
            vcvtph2ps(c, rd, 0);
        } break;

        case op_i32_from_half: {
            int8_t rx = ra_ensure(c, ra, sl, ns, inst->x);
            _Bool x_dead = lu[inst->x] <= i;
            int8_t rd;
            if (x_dead) { rd = ra_claim(ra, inst->x, i); }
            else { rd = ra_alloc(c, ra, sl, ns); ra->reg[i] = rd; ra->owner[(int)rd] = i; }
            vcvttps2dq(c, rd, rx);    // f32 → i32
        } break;

        case op_i16_from_i32: {
            int8_t rx = ra_ensure(c, ra, sl, ns, inst->x);
            _Bool x_dead = lu[inst->x] <= i;
            int8_t rd;
            if (x_dead) { rd = ra_claim(ra, inst->x, i); }
            else { rd = ra_alloc(c, ra, sl, ns); ra->reg[i] = rd; ra->owner[(int)rd] = i; }
            // Narrow 8×i32 in YMM → 8×i16 in XMM
            // VEXTRACTI128 + VPACKSSDW
            vextracti128(c, 0, rx, 1);         // xmm0 = hi 4 dwords
            vex_rrr(c, 1, 1, 0, 0x6B, rd, rx, 0);  // VPACKSSDW xmm_rd, xmm_rx, xmm0
        } break;

        case op_i32_from_i16: {
            int8_t rx = ra_ensure(c, ra, sl, ns, inst->x);
            _Bool x_dead = lu[inst->x] <= i;
            int8_t rd;
            if (x_dead) { rd = ra_claim(ra, inst->x, i); }
            else { rd = ra_alloc(c, ra, sl, ns); ra->reg[i] = rd; ra->owner[(int)rd] = i; }
            // Widen 8×i16 in XMM → 8×i32 in YMM
            vpmovsxwd(c, rd, rx);
        } break;

        default: {
            // Generic ALU ops
            int8_t rx = inst->x < i ? ra_ensure(c, ra, sl, ns, inst->x) : 0;
            int8_t ry = inst->y < i ? ra_ensure(c, ra, sl, ns, inst->y) : 0;
            int8_t rz = inst->z < i ? ra_ensure(c, ra, sl, ns, inst->z) : 0;

            _Bool x_dead = inst->x < i && lu[inst->x] <= i;
            _Bool y_dead = inst->y < i && lu[inst->y] <= i;
            _Bool z_dead = inst->z < i && lu[inst->z] <= i;
            if (inst->y == inst->x) y_dead = 0;
            if (inst->z == inst->x) z_dead = 0;
            if (inst->z == inst->y) z_dead = 0;

            int8_t rd = -1;
            enum op op2 = inst->op;
            _Bool destructive = op2==op_fma_f32 || op2==op_fma_half
                             || op2==op_sel_32  || op2==op_sel_16 || op2==op_sel_half;

            if ((op2==op_fma_f32 || op2==op_fma_half) && z_dead)
                { rd = ra_claim(ra, inst->z, i); z_dead = 0; }
            else if ((op2==op_sel_32 || op2==op_sel_16 || op2==op_sel_half) && x_dead)
                { rd = ra_claim(ra, inst->x, i); x_dead = 0; }

            if (!destructive) {
                if (rd < 0 && x_dead) { rd = ra_claim(ra, inst->x, i); x_dead = 0; }
                if (rd < 0 && y_dead) { rd = ra_claim(ra, inst->y, i); y_dead = 0; }
                if (rd < 0 && z_dead) { rd = ra_claim(ra, inst->z, i); z_dead = 0; }
            }

            if (rd < 0) {
                rd = ra_alloc(c, ra, sl, ns);
                ra->reg[i] = rd; ra->owner[(int)rd] = i;
            }

            _Bool needs_scratch = op2==op_sel_32 || op2==op_sel_16 || op2==op_sel_half
                               || op2==op_le_s32
                               || op2==op_lt_u32 || op2==op_le_u32
                               || op2==op_le_s16
                               || op2==op_lt_u16 || op2==op_le_u16
                               || ((op2==op_fma_f32 || op2==op_fma_half) && rd!=rz && (rd==rx || rd==ry));
            int8_t scratch_reg = -1;
            if (needs_scratch) {
                scratch_reg = ra_alloc(c, ra, sl, ns);
            }

            // Free dead inputs AFTER allocating scratch to prevent alias
            if (x_dead) ra_free_reg(ra, inst->x);
            if (y_dead) ra_free_reg(ra, inst->y);
            if (z_dead) ra_free_reg(ra, inst->z);

            emit_alu_reg(c, inst->op, rd, rx, ry, rz, inst->imm, scratch_reg >= 0 ? scratch_reg : 0);

            if (scratch_reg >= 0) {
                ra->free_stack[ra->nfree++] = scratch_reg;
            }
        } break;
        }
    }
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
