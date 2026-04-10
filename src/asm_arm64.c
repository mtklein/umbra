#include "asm_arm64.h"
#include <string.h>
#include <sys/mman.h>

typedef struct asm_arm64 buf;

void put(buf *b, uint32_t w) {
    if (b->words == b->cap) {
        size_t const pg       = 16384,
                     new_size = b->mmap_size ? 2 * b->mmap_size : 4 * pg;
        void *m = mmap(NULL, new_size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANON, -1, 0);
        memcpy(m, b->word, (size_t)b->words * 4);
        if (b->mmap_size) { munmap(b->word, b->mmap_size); }
        b->word      = m;
        b->cap       = (int)((new_size - pg) / 4);
        b->mmap_size = new_size;
    }
    b->word[b->words++] = w;
}

void asm_arm64_free(buf *b) {
    munmap(b->word, b->mmap_size);
}

uint32_t RET(void) { return 0xd65f03c0u; }
uint32_t NOP(void) { return 0xd503201fu; }

uint32_t ADD_xr(int d, int n, int m) {
    return 0x8b000000u | ((uint32_t)m << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t SUB_xr(int d, int n, int m) {
    return 0xcb000000u | ((uint32_t)m << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t ADD_xi(int d, int n, int imm12) {
    return 0x91000000u | ((uint32_t)imm12 << 10) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t SUB_xi(int d, int n, int imm12) {
    return 0xd1000000u | ((uint32_t)imm12 << 10) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t SUBS_xi(int d, int n, int imm12) {
    return 0xf1000000u | ((uint32_t)imm12 << 10) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t CMP_xr(int n, int m) {
    return 0xeb00001fu | ((uint32_t)m << 16) | ((uint32_t)n << 5);
}
uint32_t MOVZ_w(int d, uint16_t imm) {
    return 0x52800000u | ((uint32_t)imm << 5) | (uint32_t)d;
}
uint32_t MOVZ_x_lsl16(int d, uint16_t imm) {
    return 0xd2a00000u | ((uint32_t)imm << 5) | (uint32_t)d;
}
uint32_t MOVK_w16(int d, uint16_t imm) {
    return 0x72a00000u | ((uint32_t)imm << 5) | (uint32_t)d;
}
uint32_t LSR_wi(int d, int n, int shift) {
    return 0x53000000u | ((uint32_t)shift << 16) | (31u << 10)
                       | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t STP_pre(int t1, int t2, int n, int imm7) {
    return 0xa9800000u
        | ((uint32_t)(imm7 & 0x7f) << 15)
        | ((uint32_t)t2 << 10)
        | ((uint32_t)n << 5)
        | (uint32_t)t1;
}
uint32_t LDP_post(int t1, int t2, int n, int imm7) {
    return 0xa8c00000u
        | ((uint32_t)(imm7 & 0x7f) << 15)
        | ((uint32_t)t2 << 10)
        | ((uint32_t)n << 5)
        | (uint32_t)t1;
}
uint32_t B(int off26) { return 0x14000000u | (uint32_t)(off26 & 0x3ffffff); }
uint32_t Bcond(int cond, int off19) {
    return 0x54000000u | ((uint32_t)(off19 & 0x7ffff) << 5) | (uint32_t)cond;
}

uint32_t LDR_sx(int d, int n, int m) {
    return 0xbc607800u | ((uint32_t)m << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t STR_sx(int d, int n, int m) {
    return 0xbc207800u | ((uint32_t)m << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t STR_hx(int d, int n, int m) {
    return 0x7c207800u | ((uint32_t)m << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t LDR_hx(int d, int n, int m) {
    return 0x7c607800u | ((uint32_t)m << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t LDR_hi(int d, int n, int imm) {
    return 0x7d400000u | ((uint32_t)imm << 10) | ((uint32_t)n << 5) | (uint32_t)d;
}

static uint32_t stp_ldp_qi(uint32_t base, int t1, int t2, int n, int imm7) {
    return base | ((uint32_t)(imm7 & 0x7f) << 15) | ((uint32_t)t2 << 10)
               | ((uint32_t)n << 5) | (uint32_t)t1;
}
uint32_t STP_qi_pre(int t1, int t2, int n, int imm7) { return stp_ldp_qi(0xad800000u, t1, t2, n, imm7); }
uint32_t STP_qi(int t1, int t2, int n, int imm7)     { return stp_ldp_qi(0xad000000u, t1, t2, n, imm7); }
uint32_t LDP_qi(int t1, int t2, int n, int imm7)     { return stp_ldp_qi(0xad400000u, t1, t2, n, imm7); }

uint32_t LDR_q_literal(int d) { return 0x9c000000u | (uint32_t)d; }

uint32_t LDR_d(int d, int n, int m) {
    return 0xfc606800u | ((uint32_t)m << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t STR_d(int d, int n, int m) {
    return 0xfc206800u | ((uint32_t)m << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t LDR_q(int d, int n, int m) {
    return 0x3ce06800u | ((uint32_t)m << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t STR_q(int d, int n, int m) {
    return 0x3ca06800u | ((uint32_t)m << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}

uint32_t LSL_xi(int d, int n, int shift) {
    int const immr = (-shift) & 63,
              imms = 63 - shift;
    return 0xd3400000u
        | ((uint32_t)immr << 16)
        | ((uint32_t)imms << 10)
        | ((uint32_t)n << 5)
        | (uint32_t)d;
}
uint32_t LSR_xi(int d, int n, int shift) {
    return 0xd340fc00u
        | ((uint32_t)shift << 16)
        | ((uint32_t)n << 5)
        | (uint32_t)d;
}

uint32_t LDR_xi(int d, int n, int imm) {
    return 0xf9400000u | ((uint32_t)imm << 10) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t LDRH_wi(int d, int n, int imm) {
    return 0x79400000u | ((uint32_t)imm << 10) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t LDRH_wr(int d, int n, int m) {
    return 0x78607800u | ((uint32_t)m << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t LDR_wr(int d, int n, int m) {
    return 0xb8607800u | ((uint32_t)m << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t LDR_di(int d, int n, int imm) {
    return 0xfd400000u | ((uint32_t)imm << 10) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t STR_di(int d, int n, int imm) {
    return 0xfd000000u | ((uint32_t)imm << 10) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t MADD_x(int d, int n, int m, int a) {
    return 0x9b000000u | ((uint32_t)m << 16) | ((uint32_t)a << 10)
                       | ((uint32_t)n << 5)  | (uint32_t)d;
}
uint32_t CMP_wr(int n, int m) {
    return 0x6b00001fu | ((uint32_t)m << 16) | ((uint32_t)n << 5);
}
uint32_t LDR_si(int d, int n, int imm) {
    return 0xbd400000u | ((uint32_t)imm << 10) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t LDR_qi(int d, int n, int imm) {
    return 0x3dc00000u | ((uint32_t)imm << 10) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t STR_qi(int d, int n, int imm) {
    return 0x3d800000u | ((uint32_t)imm << 10) | ((uint32_t)n << 5) | (uint32_t)d;
}

uint32_t W(uint32_t insn) { return insn | 0x40000000u; }

// Encoding constants for macro-generated NEON functions.
enum {
    // float 4S
    FADD_4s_ = 0x4e20d400u,
    FSUB_4s_ = 0x4ea0d400u,
    FMUL_4s_ = 0x6e20dc00u,
    FDIV_4s_ = 0x6e20fc00u,
    FMLA_4s_ = 0x4e20cc00u,
    FMLS_4s_ = 0x4ea0cc00u,
    FMINNM_4s_ = 0x4ea0c400u,
    FMAXNM_4s_ = 0x4e20c400u,
    FSQRT_4s_ = 0x6ea1f800u,
    FABS_4s_ = 0x4ea0f800u,
    FNEG_4s_ = 0x6ea0f800u,
    FRINTN_4s_ = 0x4e218800u,
    FRINTM_4s_ = 0x4e219800u,
    FRINTP_4s_ = 0x4ea18800u,
    SCVTF_4s_ = 0x4e21d800u,
    FCVTZS_4s_ = 0x4ea1b800u,
    FCVTNS_4s_ = 0x4e21a800u,
    FCVTMS_4s_ = 0x4e21b800u,
    FCVTPS_4s_ = 0x4ea1a800u,
    FCMEQ_4s_ = 0x4e20e400u,
    FCMGT_4s_ = 0x6ea0e400u,
    FCMGE_4s_ = 0x6e20e400u,

    // int 4S
    ADD_4s_ = 0x4ea08400u,
    SUB_4s_ = 0x6ea08400u,
    MUL_4s_ = 0x4ea09c00u,
    USHL_4s_ = 0x6ea04400u,
    SSHL_4s_ = 0x4ea04400u,
    NEG_4s_ = 0x6ea0b800u,
    CMEQ_4s_ = 0x6ea08c00u,
    CMGT_4s_ = 0x4ea03400u,
    CMGE_4s_ = 0x4ea03c00u,
    CMHI_4s_ = 0x6ea03400u,
    CMHS_4s_ = 0x6ea03c00u,

    // bitwise 16B
    AND_16b_ = 0x4e201c00u,
    ORR_16b_ = 0x4ea01c00u,
    EOR_16b_ = 0x6e201c00u,
    BSL_16b_ = 0x6e601c00u,
    BIT_16b_ = 0x6ea01c00u,
    BIF_16b_ = 0x6ee01c00u,

    // conversions
    FCVTN_4h_ = 0x0e216800u,
    FCVTL_4s_ = 0x0e217800u,
    XTN_4h_ = 0x0e612800u,
    SXTL_4s_ = 0x0f10a400u,
};

static uint32_t v3(uint32_t enc, int d, int n, int m) {
    return enc | ((uint32_t)m << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}
static uint32_t v2(uint32_t enc, int d, int n) {
    return enc | ((uint32_t)n << 5) | (uint32_t)d;
}

uint32_t FADD_4s(int d, int n, int m) { return v3(FADD_4s_, d, n, m); }
uint32_t FSUB_4s(int d, int n, int m) { return v3(FSUB_4s_, d, n, m); }
uint32_t FMUL_4s(int d, int n, int m) { return v3(FMUL_4s_, d, n, m); }
uint32_t FDIV_4s(int d, int n, int m) { return v3(FDIV_4s_, d, n, m); }
uint32_t FMLA_4s(int d, int n, int m) { return v3(FMLA_4s_, d, n, m); }
uint32_t FMLS_4s(int d, int n, int m) { return v3(FMLS_4s_, d, n, m); }
uint32_t FMINNM_4s(int d, int n, int m) { return v3(FMINNM_4s_, d, n, m); }
uint32_t FMAXNM_4s(int d, int n, int m) { return v3(FMAXNM_4s_, d, n, m); }
uint32_t FSQRT_4s(int d, int n) { return v2(FSQRT_4s_, d, n); }
uint32_t FABS_4s(int d, int n) { return v2(FABS_4s_, d, n); }
uint32_t FNEG_4s(int d, int n) { return v2(FNEG_4s_, d, n); }
uint32_t FRINTN_4s(int d, int n) { return v2(FRINTN_4s_, d, n); }
uint32_t FRINTM_4s(int d, int n) { return v2(FRINTM_4s_, d, n); }
uint32_t FRINTP_4s(int d, int n) { return v2(FRINTP_4s_, d, n); }
uint32_t SCVTF_4s(int d, int n) { return v2(SCVTF_4s_, d, n); }
uint32_t FCVTZS_4s(int d, int n) { return v2(FCVTZS_4s_, d, n); }
uint32_t FCVTNS_4s(int d, int n) { return v2(FCVTNS_4s_, d, n); }
uint32_t FCVTMS_4s(int d, int n) { return v2(FCVTMS_4s_, d, n); }
uint32_t FCVTPS_4s(int d, int n) { return v2(FCVTPS_4s_, d, n); }
uint32_t FCMEQ_4s(int d, int n, int m) { return v3(FCMEQ_4s_, d, n, m); }
uint32_t FCMGT_4s(int d, int n, int m) { return v3(FCMGT_4s_, d, n, m); }
uint32_t FCMGE_4s(int d, int n, int m) { return v3(FCMGE_4s_, d, n, m); }

uint32_t ADD_4s(int d, int n, int m) { return v3(ADD_4s_, d, n, m); }
uint32_t SUB_4s(int d, int n, int m) { return v3(SUB_4s_, d, n, m); }
uint32_t MUL_4s(int d, int n, int m) { return v3(MUL_4s_, d, n, m); }
uint32_t USHL_4s(int d, int n, int m) { return v3(USHL_4s_, d, n, m); }
uint32_t SSHL_4s(int d, int n, int m) { return v3(SSHL_4s_, d, n, m); }
uint32_t NEG_4s(int d, int n) { return v2(NEG_4s_, d, n); }
uint32_t CMEQ_4s(int d, int n, int m) { return v3(CMEQ_4s_, d, n, m); }
uint32_t CMGT_4s(int d, int n, int m) { return v3(CMGT_4s_, d, n, m); }
uint32_t CMGE_4s(int d, int n, int m) { return v3(CMGE_4s_, d, n, m); }
uint32_t CMHI_4s(int d, int n, int m) { return v3(CMHI_4s_, d, n, m); }
uint32_t CMHS_4s(int d, int n, int m) { return v3(CMHS_4s_, d, n, m); }

uint32_t AND_16b(int d, int n, int m) { return v3(AND_16b_, d, n, m); }
uint32_t ORR_16b(int d, int n, int m) { return v3(ORR_16b_, d, n, m); }
uint32_t EOR_16b(int d, int n, int m) { return v3(EOR_16b_, d, n, m); }
uint32_t BSL_16b(int d, int n, int m) { return v3(BSL_16b_, d, n, m); }
uint32_t BIT_16b(int d, int n, int m) { return v3(BIT_16b_, d, n, m); }
uint32_t BIF_16b(int d, int n, int m) { return v3(BIF_16b_, d, n, m); }

uint32_t FCVTN_4h(int d, int n) { return v2(FCVTN_4h_, d, n); }
uint32_t FCVTL_4s(int d, int n) { return v2(FCVTL_4s_, d, n); }
uint32_t XTN_4h(int d, int n) { return v2(XTN_4h_, d, n); }
uint32_t SXTL_4s(int d, int n) { return v2(SXTL_4s_, d, n); }

// Shift-by-immediate: immh:immb at bits 22:16
uint32_t SHL_4s_imm(int d, int n, int sh) {
    return 0x4f005400u | ((uint32_t)(sh + 32) << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t USHR_4s_imm(int d, int n, int sh) {
    return 0x6f000400u | ((uint32_t)(64 - sh) << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t SSHR_4s_imm(int d, int n, int sh) {
    return 0x4f000400u | ((uint32_t)(64 - sh) << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t UMOV_ws(int d, int n) { return 0x0e043c00u | ((uint32_t)n << 5) | (uint32_t)d; }
uint32_t UMOV_ws_lane(int d, int n, int lane) {
    uint32_t const imm5 = (uint32_t)((lane << 3) | 4);
    return 0x0e003c00u | (imm5 << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t ZIP1_4s(int d, int n, int m) {
    return 0x4e803800u | ((uint32_t)m << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t ZIP2_4s(int d, int n, int m) {
    return 0x4e807800u | ((uint32_t)m << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t UZP1_8h(int d, int n, int m) {
    return 0x4e401800u | ((uint32_t)m << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t UZP2_8h(int d, int n, int m) {
    return 0x4e405800u | ((uint32_t)m << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t ZIP1_8h(int d, int n, int m) {
    return 0x4e403800u | ((uint32_t)m << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t EXT_16b(int d, int n, int m, int imm) {
    return 0x6e004000u | ((uint32_t)imm << 11) | ((uint32_t)m << 16)
                       | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t UXTL_4s(int d, int n) {
    return 0x2f10a400u | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t UXTL_8h(int d, int n) {
    return 0x2f08a400u | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t XTN_8b(int d, int n) {
    return 0x0e212800u | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t INS_elem_s(int d, int dst_lane, int n, int src_lane) {
    uint32_t const imm5 = (uint32_t)(dst_lane << 3) | 4,
                   imm4 = (uint32_t)(src_lane << 2);
    return 0x6e000400u | (imm5 << 16) | (imm4 << 11) | ((uint32_t)n << 5) | (uint32_t)d;
}
uint32_t LD1_s(int t, int idx, int n) {
    uint32_t const Q = ((uint32_t)idx >> 1) & 1,
                   S = (uint32_t)idx & 1;
    return 0x0d408000u | (Q << 30) | (S << 12) | ((uint32_t)n << 5) | (uint32_t)t;
}

uint32_t MOVI_4s(int d, uint8_t imm8, int shift) {
    int      const cmode = (shift / 8) * 2;
    uint32_t const abc   = (imm8 >> 5) & 7,
                   defgh = imm8 & 0x1f;
    return 0x4f000400u | (abc << 16) | ((uint32_t)cmode << 12) | (defgh << 5) | (uint32_t)d;
}
uint32_t MVNI_4s(int d, uint8_t imm8, int shift) {
    return MOVI_4s(d, imm8, shift) | (1u << 29);
}
uint32_t DUP_4s_w(int d, int n) { return 0x4e040c00u | ((uint32_t)n << 5) | (uint32_t)d; }
uint32_t DUP_4s_lane(int d, int n, int lane) {
    uint32_t const imm5 = (uint32_t)(lane << 3) | 4;
    return 0x4e000400u | (imm5 << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}

uint32_t INS_s(int d, int idx, int n) {
    uint32_t const imm5 = (uint32_t)(idx << 3) | 4;
    return 0x4e001c00u | (imm5 << 16) | ((uint32_t)n << 5) | (uint32_t)d;
}

static uint32_t ld4st4(uint32_t base, int t, int n) {
    return base | ((uint32_t)n << 5) | (uint32_t)t;
}
uint32_t LD4_4h(int t, int n) { return ld4st4(0x0c400400u, t, n); }
uint32_t ST4_4h(int t, int n) { return ld4st4(0x0c000400u, t, n); }
uint32_t LD4_8h(int t, int n) { return ld4st4(0x4c400400u, t, n); }
uint32_t ST4_8h(int t, int n) { return ld4st4(0x4c000400u, t, n); }
uint32_t LD4_8b(int t, int n) { return ld4st4(0x0c400000u, t, n); }
uint32_t ST4_8b(int t, int n) { return ld4st4(0x0c000000u, t, n); }

uint32_t FMOV_4s_imm(int d, uint8_t imm8) {
    return 0x4f00f400u | ((uint32_t)(imm8 >> 5) << 16) | ((uint32_t)(imm8 & 0x1fu) << 5)
                       | (uint32_t)d;
}

_Bool movi_4s(buf *c, int d, uint32_t v) {
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
        uint32_t const s = v >> 31,
                       e = (v >> 23) & 0xffu,
                       f = v & 0x7fffffu;
        if ((f & 0x7ffffu) == 0 && e >= 124 && e <= 131) {
            uint32_t const E    = e - 124;
            uint32_t const imm8 =
                (s << 7) | (((~E >> 2) & 1) << 6) | ((E & 3) << 4) | ((f >> 19) & 0xf);
            put(c, FMOV_4s_imm(d, (uint8_t)imm8));
        } else {
            return 0;
        }
    }
    return 1;
}

void load_imm_w(buf *c, int rd, uint32_t v) {
    put(c, MOVZ_w(rd, (uint16_t)(v & 0xffff)));
    if (v >> 16) { put(c, MOVK_w16(rd, (uint16_t)(v >> 16))); }
}
