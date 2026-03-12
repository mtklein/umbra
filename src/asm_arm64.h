#pragma once
#include <stdint.h>
#include <stdlib.h>

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
    FDIV_4s_  =0x6E20FC00u, FMLA_4s_  =0x4E20CC00u, FMLS_4s_  =0x4EA0CC00u,
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
    FDIV_4h_  =0x2E403C00u, FMLA_4h_  =0x0E400C00u, FMLS_4h_  =0x0EC00C00u,
    FMINNM_4h_=0x0EC00400u, FMAXNM_4h_=0x0E400400u,
    FSQRT_4h_ =0x2EF9F800u,
    FCMEQ_4h_ =0x0E402400u, FCMGT_4h_ =0x2EC02400u, FCMGE_4h_ =0x2E402400u,

    // int 4H
    ADD_4h_ =0x0E608400u, SUB_4h_ =0x2E608400u, MUL_4h_ =0x0E609C00u,
    USHL_4h_=0x2E604400u, SSHL_4h_=0x0E604400u, NEG_4h_ =0x2E60B800u,
    CMEQ_4h_=0x2E608C00u, CMGT_4h_=0x0E603400u, CMGE_4h_=0x0E603C00u,
    CMHI_4h_=0x2E603400u, CMHS_4h_=0x2E603C00u,

    // compare against zero (2-operand)
    CMEQ_4s_z_=0x4EA09800u, CMGT_4s_z_=0x4EA08800u, CMGE_4s_z_=0x6EA08800u,
    CMLE_4s_z_=0x6EA09800u, CMLT_4s_z_=0x4EA0A800u,
    FCMEQ_4s_z_=0x4EA0D800u, FCMGT_4s_z_=0x4EA0C800u, FCMGE_4s_z_=0x6EA0C800u,
    FCMLE_4s_z_=0x6EA0D800u, FCMLT_4s_z_=0x4EA0E800u,

    CMEQ_4h_z_=0x0E609800u, CMGT_4h_z_=0x0E608800u, CMGE_4h_z_=0x2E608800u,
    CMLE_4h_z_=0x2E609800u, CMLT_4h_z_=0x0E60A800u,
    FCMEQ_4h_z_=0x0EF8D800u, FCMGT_4h_z_=0x0EF8C800u, FCMGE_4h_z_=0x2EF8C800u,
    FCMLE_4h_z_=0x2EF8D800u, FCMLT_4h_z_=0x0EF8E800u,

    // conversions
    FCVTN_4h_=0x0E216800u, FCVTL_4s_=0x0E217800u,
    SCVTF_4h_=0x0E79D800u, FCVTZS_4h_=0x0EF9B800u,
    XTN_4h_  =0x0E612800u, SXTL_4s_  =0x0F10A400u,
    UXTL_8h_ =0x2F08A400u,
};

V3(FADD_4s)  V3(FSUB_4s)  V3(FMUL_4s) V3(FDIV_4s)  V3(FMLA_4s) V3(FMLS_4s)
V3(FMINNM_4s) V3(FMAXNM_4s) V2(FSQRT_4s) V2(SCVTF_4s) V2(FCVTZS_4s)
V3(FCMEQ_4s) V3(FCMGT_4s) V3(FCMGE_4s)
V3(ADD_4s) V3(SUB_4s) V3(MUL_4s)
V3(USHL_4s) V3(SSHL_4s) V2(NEG_4s)
V3(CMEQ_4s) V3(CMGT_4s) V3(CMGE_4s) V3(CMHI_4s) V3(CMHS_4s)
V3(AND_16b) V3(ORR_16b) V3(EOR_16b) V3(BSL_16b) V3(BIT_16b) V3(BIF_16b)
V3(FADD_4h)  V3(FSUB_4h)  V3(FMUL_4h) V3(FDIV_4h)  V3(FMLA_4h) V3(FMLS_4h)
V3(FMINNM_4h) V3(FMAXNM_4h) V2(FSQRT_4h)
V3(FCMEQ_4h) V3(FCMGT_4h) V3(FCMGE_4h)
V3(ADD_4h) V3(SUB_4h) V3(MUL_4h)
V3(USHL_4h) V3(SSHL_4h) V2(NEG_4h)
V3(CMEQ_4h) V3(CMGT_4h) V3(CMGE_4h) V3(CMHI_4h) V3(CMHS_4h)
V2(CMEQ_4s_z) V2(CMGT_4s_z) V2(CMGE_4s_z) V2(CMLE_4s_z) V2(CMLT_4s_z)
V2(FCMEQ_4s_z) V2(FCMGT_4s_z) V2(FCMGE_4s_z) V2(FCMLE_4s_z) V2(FCMLT_4s_z)
V2(CMEQ_4h_z) V2(CMGT_4h_z) V2(CMGE_4h_z) V2(CMLE_4h_z) V2(CMLT_4h_z)
V2(FCMEQ_4h_z) V2(FCMGT_4h_z) V2(FCMGE_4h_z) V2(FCMLE_4h_z) V2(FCMLT_4h_z)
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


// UMOV Wd, Vn.S[0]  — extract lane 0 of 32-bit element to GPR
static uint32_t UMOV_ws(int d, int n) {
    return 0x0E043C00u | ((uint32_t)n<<5) | (uint32_t)d;
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

// MOVI Vd.4S, #imm8{, LSL #shift}  — shift=0,8,16,24 → cmode=0,2,4,6
static uint32_t MOVI_4s(int d, uint8_t imm8, int shift) {
    int cmode = (shift/8) * 2;
    uint32_t abc = (imm8 >> 5) & 7, defgh = imm8 & 0x1F;
    return 0x4F000400u | (abc<<16) | ((uint32_t)cmode<<12) | (defgh<<5) | (uint32_t)d;
}
// MVNI Vd.4S, #imm8{, LSL #shift}  — NOT of MOVI
static uint32_t MVNI_4s(int d, uint8_t imm8, int shift) {
    return MOVI_4s(d, imm8, shift) | (1u<<29);  // op=1
}
// MOVI Vd.8H, #imm8{, LSL #shift}  — shift=0,8 → cmode=8,10
static uint32_t MOVI_8h(int d, uint8_t imm8, int shift) {
    int cmode = 8 + (shift/8) * 2;
    uint32_t abc = (imm8 >> 5) & 7, defgh = imm8 & 0x1F;
    return 0x4F000400u | (abc<<16) | ((uint32_t)cmode<<12) | (defgh<<5) | (uint32_t)d;
}
// MVNI Vd.8H, #imm8{, LSL #shift}
static uint32_t MVNI_8h(int d, uint8_t imm8, int shift) {
    return MOVI_8h(d, imm8, shift) | (1u<<29);
}
static uint32_t DUP_4s_w(int d, int n)  { return 0x4E040C00u|((uint32_t)n<<5)|(uint32_t)d; }
static uint32_t DUP_4h_w(int d, int n)  { return 0x0E020C00u|((uint32_t)n<<5)|(uint32_t)d; }

static uint32_t INS_s(int d, int idx, int n) {
    uint32_t imm5 = (uint32_t)(idx<<3)|4;
    return 0x4E001C00u|(imm5<<16)|((uint32_t)n<<5)|(uint32_t)d;
}

static void load_imm_w(Buf *c, int rd, uint32_t v) {
    put(c, MOVZ_w(rd, (uint16_t)(v&0xffff)));
    if (v>>16) put(c, MOVK_w16(rd, (uint16_t)(v>>16)));
}
