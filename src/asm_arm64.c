#include "asm_arm64.h"
#include <stdlib.h>

void put(Buf *b, uint32_t w) {
    if (b->len == b->cap) {
        b->cap = b->cap ? 2*b->cap : 1024;
        b->buf = realloc(b->buf, (size_t)b->cap * 4);
    }
    b->buf[b->len++] = w;
}

uint32_t RET(void) { return 0xd65f03c0u; }

uint32_t ADD_xr(int d, int n, int m) {
    return 0x8b000000u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}
uint32_t ADD_xi(int d, int n, int imm12) {
    return 0x91000000u | ((uint32_t)imm12<<10) | ((uint32_t)n<<5) | (uint32_t)d;
}
uint32_t SUB_xi(int d, int n, int imm12) {
    return 0xd1000000u | ((uint32_t)imm12<<10) | ((uint32_t)n<<5) | (uint32_t)d;
}
uint32_t SUBS_xi(int d, int n, int imm12) {
    return 0xf1000000u | ((uint32_t)imm12<<10) | ((uint32_t)n<<5) | (uint32_t)d;
}
uint32_t MOVZ_x(int d, uint16_t imm) {
    return 0xd2800000u | ((uint32_t)imm<<5) | (uint32_t)d;
}
uint32_t MOVZ_w(int d, uint16_t imm) {
    return 0x52800000u | ((uint32_t)imm<<5) | (uint32_t)d;
}
uint32_t MOVK_w16(int d, uint16_t imm) {
    return 0x72a00000u | ((uint32_t)imm<<5) | (uint32_t)d;
}
uint32_t STP_pre(int t1, int t2, int n, int imm7) {
    return 0xa9800000u | ((uint32_t)(imm7&0x7f)<<15)
         | ((uint32_t)t2<<10) | ((uint32_t)n<<5)
         | (uint32_t)t1;
}
uint32_t LDP_post(int t1, int t2, int n, int imm7) {
    return 0xa8c00000u | ((uint32_t)(imm7&0x7f)<<15)
         | ((uint32_t)t2<<10) | ((uint32_t)n<<5)
         | (uint32_t)t1;
}
uint32_t B(int off26) {
    return 0x14000000u | (uint32_t)(off26 & 0x3ffffff);
}
uint32_t Bcond(int cond, int off19) {
    return 0x54000000u | ((uint32_t)(off19&0x7ffff)<<5) | (uint32_t)cond;
}

uint32_t LDR_sx(int d, int n, int m) {
    return 0xbc607800u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}
uint32_t STR_sx(int d, int n, int m) {
    return 0xbc207800u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}
uint32_t LDR_hx(int d, int n, int m) {
    return 0x7c607800u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}
uint32_t STR_hx(int d, int n, int m) {
    return 0x7c207800u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}

uint32_t LDR_d(int d, int n, int m) {
    return 0xfc606800u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}
uint32_t STR_d(int d, int n, int m) {
    return 0xfc206800u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}
uint32_t LDR_q(int d, int n, int m) {
    return 0x3ce06800u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}
uint32_t STR_q(int d, int n, int m) {
    return 0x3ca06800u | ((uint32_t)m<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}

uint32_t LSL_xi(int d, int n, int shift) {
    int immr = (-shift) & 63, imms = 63 - shift;
    return 0xd3400000u | ((uint32_t)immr<<16) | ((uint32_t)imms<<10)
         | ((uint32_t)n<<5) | (uint32_t)d;
}

uint32_t LDR_qi(int d, int n, int imm) {
    return 0x3dc00000u | ((uint32_t)imm<<10) | ((uint32_t)n<<5) | (uint32_t)d;
}
uint32_t STR_qi(int d, int n, int imm) {
    return 0x3d800000u | ((uint32_t)imm<<10) | ((uint32_t)n<<5) | (uint32_t)d;
}

uint32_t LDP_qi(int t1, int t2, int n, int imm) {
    return 0xad400000u | ((uint32_t)(imm&0x7f)<<15)
         | ((uint32_t)t2<<10) | ((uint32_t)n<<5)
         | (uint32_t)t1;
}
uint32_t STP_qi(int t1, int t2, int n, int imm) {
    return 0xad000000u | ((uint32_t)(imm&0x7f)<<15)
         | ((uint32_t)t2<<10) | ((uint32_t)n<<5)
         | (uint32_t)t1;
}

uint32_t LD4_8b(int t, int n) { return 0x0c400000u | ((uint32_t)n<<5) | (uint32_t)t; }
uint32_t ST4_8b(int t, int n) { return 0x0c000000u | ((uint32_t)n<<5) | (uint32_t)t; }

uint32_t SLI_4s_imm(int d, int n, int shift) {
    return 0x6f005400u
        | ((uint32_t)(shift + 32) << 16)
        | ((uint32_t)n << 5)
        | (uint32_t)d;
}

uint32_t W(uint32_t insn) { return insn | 0x40000000u; }

// Encoding constants for macro-generated NEON functions.
enum {
    // float 4S
    FADD_4s_  =0x4e20d400u, FSUB_4s_  =0x4ea0d400u, FMUL_4s_  =0x6e20dc00u,
    FDIV_4s_  =0x6e20fc00u, FMLA_4s_  =0x4e20cc00u, FMLS_4s_  =0x4ea0cc00u,
    FMINNM_4s_=0x4ea0c400u, FMAXNM_4s_=0x4e20c400u,
    FSQRT_4s_ =0x6ea1f800u,
    SCVTF_4s_ =0x4e21d800u, FCVTZS_4s_=0x4ea1b800u,
    FCMEQ_4s_ =0x4e20e400u, FCMGT_4s_ =0x6ea0e400u, FCMGE_4s_ =0x6e20e400u,

    // int 4S
    ADD_4s_ =0x4ea08400u, SUB_4s_ =0x6ea08400u, MUL_4s_ =0x4ea09c00u,
    USHL_4s_=0x6ea04400u, SSHL_4s_=0x4ea04400u, NEG_4s_ =0x6ea0b800u,
    CMEQ_4s_=0x6ea08c00u, CMGT_4s_=0x4ea03400u, CMGE_4s_=0x4ea03c00u,
    CMHI_4s_=0x6ea03400u, CMHS_4s_=0x6ea03c00u,

    // bitwise 16B
    AND_16b_=0x4e201c00u, ORR_16b_=0x4ea01c00u, EOR_16b_=0x6e201c00u,
    BSL_16b_=0x6e601c00u, BIT_16b_=0x6ea01c00u, BIF_16b_=0x6ee01c00u,

    // compare against zero (2-operand)
    CMEQ_4s_z_=0x4ea09800u, CMGT_4s_z_=0x4ea08800u, CMGE_4s_z_=0x6ea08800u,
    CMLE_4s_z_=0x6ea09800u, CMLT_4s_z_=0x4ea0a800u,
    FCMEQ_4s_z_=0x4ea0d800u, FCMGT_4s_z_=0x4ea0c800u, FCMGE_4s_z_=0x6ea0c800u,
    FCMLE_4s_z_=0x6ea0d800u, FCMLT_4s_z_=0x4ea0e800u,

    // conversions
    FCVTN_4h_=0x0e216800u, FCVTL_4s_=0x0e217800u,
    XTN_4h_  =0x0e612800u, SXTL_4s_  =0x0f10a400u,
};

#define V3(enc) uint32_t enc(int d,int n,int m){\
    return enc##_ | ((uint32_t)m<<16)\
                  | ((uint32_t)n<<5)\
                  | (uint32_t)d;}
#define V2(enc) uint32_t enc(int d,int n){return enc##_ | ((uint32_t)n<<5)|(uint32_t)d;}

V3(FADD_4s)  V3(FSUB_4s)  V3(FMUL_4s) V3(FDIV_4s)  V3(FMLA_4s) V3(FMLS_4s)
V3(FMINNM_4s) V3(FMAXNM_4s) V2(FSQRT_4s) V2(SCVTF_4s) V2(FCVTZS_4s)
V3(FCMEQ_4s) V3(FCMGT_4s) V3(FCMGE_4s)
V3(ADD_4s) V3(SUB_4s) V3(MUL_4s)
V3(USHL_4s) V3(SSHL_4s) V2(NEG_4s)
V3(CMEQ_4s) V3(CMGT_4s) V3(CMGE_4s) V3(CMHI_4s) V3(CMHS_4s)
V3(AND_16b) V3(ORR_16b) V3(EOR_16b) V3(BSL_16b) V3(BIT_16b) V3(BIF_16b)
V2(CMEQ_4s_z) V2(CMGT_4s_z) V2(CMGE_4s_z) V2(CMLE_4s_z) V2(CMLT_4s_z)
V2(FCMEQ_4s_z) V2(FCMGT_4s_z) V2(FCMGE_4s_z) V2(FCMLE_4s_z) V2(FCMLT_4s_z)
V2(FCVTN_4h) V2(FCVTL_4s) V2(XTN_4h) V2(SXTL_4s)

#undef V3
#undef V2

// Shift-by-immediate: immh:immb at bits 22:16
uint32_t SHL_4s_imm (int d, int n, int sh) {
    return 0x4f005400u | ((uint32_t)(sh+32)<<16)
         | ((uint32_t)n<<5) | (uint32_t)d;
}
uint32_t USHR_4s_imm(int d, int n, int sh) {
    return 0x6f000400u | ((uint32_t)(64-sh)<<16)
         | ((uint32_t)n<<5) | (uint32_t)d;
}
uint32_t SSHR_4s_imm(int d, int n, int sh) {
    return 0x4f000400u | ((uint32_t)(64-sh)<<16)
         | ((uint32_t)n<<5) | (uint32_t)d;
}
uint32_t UMOV_ws(int d, int n) {
    return 0x0e043c00u | ((uint32_t)n<<5) | (uint32_t)d;
}
uint32_t UMOV_ws_lane(int d, int n, int lane) {
    uint32_t imm5 = (uint32_t)((lane << 3) | 4);
    return 0x0e003c00u | (imm5<<16) | ((uint32_t)n<<5) | (uint32_t)d;
}
uint32_t LD1_s(int t, int idx, int n) {
    uint32_t Q = ((uint32_t)idx >> 1) & 1;
    uint32_t S = (uint32_t)idx & 1;
    return 0x0d408000u | (Q<<30) | (S<<12) | ((uint32_t)n<<5) | (uint32_t)t;
}

uint32_t MOVI_4s(int d, uint8_t imm8, int shift) {
    int cmode = (shift/8) * 2;
    uint32_t abc = (imm8 >> 5) & 7, defgh = imm8 & 0x1f;
    return 0x4f000400u | (abc<<16) | ((uint32_t)cmode<<12) | (defgh<<5) | (uint32_t)d;
}
uint32_t MVNI_4s(int d, uint8_t imm8, int shift) {
    return MOVI_4s(d, imm8, shift) | (1u<<29);
}
uint32_t DUP_4s_w(int d, int n) { return 0x4e040c00u|((uint32_t)n<<5)|(uint32_t)d; }

uint32_t INS_s(int d, int idx, int n) {
    uint32_t imm5 = (uint32_t)(idx<<3)|4;
    return 0x4e001c00u|(imm5<<16)|((uint32_t)n<<5)|(uint32_t)d;
}

void load_imm_w(Buf *c, int rd, uint32_t v) {
    put(c, MOVZ_w(rd, (uint16_t)(v&0xffff)));
    if (v>>16) { put(c, MOVK_w16(rd, (uint16_t)(v>>16))); }
}

