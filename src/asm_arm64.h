#pragma once
#include <stddef.h>
#include <stdint.h>

struct asm_arm64 {
    uint32_t *word;
    int       words, cap;
    size_t    mmap_size;
};

void     put     (struct asm_arm64 *b, uint32_t w);
void     asm_arm64_free(struct asm_arm64 *b);
uint32_t RET(void);
uint32_t NOP(void);
uint32_t ADD_xr(int d, int n, int m);
uint32_t SUB_xr(int d, int n, int m);
uint32_t ADD_xi(int d, int n, int imm12);
uint32_t SUB_xi(int d, int n, int imm12);
uint32_t SUBS_xi(int d, int n, int imm12);
uint32_t CMP_xr(int n, int m);
uint32_t MOVZ_w(int d, uint16_t imm);
uint32_t MOVZ_x_lsl16(int d, uint16_t imm);
uint32_t MOVK_w16(int d, uint16_t imm);
uint32_t LSR_wi(int d, int n, int shift);
uint32_t STP_pre(int t1, int t2, int n, int imm7);
uint32_t LDP_post(int t1, int t2, int n, int imm7);
uint32_t STP_qi_pre(int t1, int t2, int n, int imm7);
uint32_t STP_qi(int t1, int t2, int n, int imm7);
uint32_t LDP_qi(int t1, int t2, int n, int imm7);
uint32_t B(int off26);
uint32_t Bcond(int cond, int off19);
uint32_t LDR_sx(int d, int n, int m);
uint32_t STR_sx(int d, int n, int m);
uint32_t STR_hx(int d, int n, int m);
uint32_t LDR_hx(int d, int n, int m);
uint32_t LDR_hi(int d, int n, int imm);
uint32_t LDR_d(int d, int n, int m);
uint32_t STR_d(int d, int n, int m);
uint32_t LDR_q_literal(int d);
uint32_t LDR_q(int d, int n, int m);
uint32_t STR_q(int d, int n, int m);
uint32_t LSL_xi(int d, int n, int shift);
uint32_t LSR_xi(int d, int n, int shift);
uint32_t LDR_qi(int d, int n, int imm);
uint32_t STR_qi(int d, int n, int imm);
uint32_t LDR_si(int d, int n, int imm);
uint32_t LDR_xi(int d, int n, int imm);
uint32_t LDR_wi(int d, int n, int imm);
uint32_t LDRH_wi(int d, int n, int imm);
uint32_t LDRH_wr(int d, int n, int m);
uint32_t LDRB_wi(int d, int n, int imm);
uint32_t LDRB_wr(int d, int n, int m);
uint32_t STR_bx (int d, int n, int m);
uint32_t LDR_wr(int d, int n, int m);
uint32_t LDR_di(int d, int n, int imm);
uint32_t STR_di(int d, int n, int imm);
uint32_t MADD_x(int d, int n, int m, int a);
uint32_t CMP_wr(int n, int m);

uint32_t W(uint32_t insn);

uint32_t FADD_4s(int d, int n, int m);
uint32_t FSUB_4s(int d, int n, int m);
uint32_t FMUL_4s(int d, int n, int m);
uint32_t FMLA_4s(int d, int n, int m);
uint32_t FMLS_4s(int d, int n, int m);
uint32_t FMINNM_4s(int d, int n, int m);
uint32_t FMAXNM_4s(int d, int n, int m);
uint32_t FABS_4s(int d, int n);
uint32_t FNEG_4s(int d, int n);
uint32_t FRINTN_4s(int d, int n);
uint32_t FRINTM_4s(int d, int n);
uint32_t FRINTP_4s(int d, int n);
uint32_t SCVTF_4s(int d, int n);
uint32_t FCVTZS_4s(int d, int n);
uint32_t FCVTNS_4s(int d, int n);
uint32_t FCVTMS_4s(int d, int n);
uint32_t FCVTPS_4s(int d, int n);
uint32_t FCMEQ_4s(int d, int n, int m);
uint32_t FCMGT_4s(int d, int n, int m);
uint32_t FCMGE_4s(int d, int n, int m);

uint32_t ADD_4s(int d, int n, int m);
uint32_t SUB_4s(int d, int n, int m);
uint32_t MUL_4s(int d, int n, int m);
uint32_t USHL_4s(int d, int n, int m);
uint32_t SSHL_4s(int d, int n, int m);
uint32_t NEG_4s(int d, int n);
uint32_t CMEQ_4s(int d, int n, int m);
uint32_t CMGT_4s(int d, int n, int m);
uint32_t CMGE_4s(int d, int n, int m);
uint32_t CMHI_4s(int d, int n, int m);
uint32_t CMHS_4s(int d, int n, int m);

uint32_t AND_16b(int d, int n, int m);
uint32_t ORR_16b(int d, int n, int m);
uint32_t EOR_16b(int d, int n, int m);
uint32_t BSL_16b(int d, int n, int m);
uint32_t BIT_16b(int d, int n, int m);
uint32_t BIF_16b(int d, int n, int m);

uint32_t FCVTN_4h(int d, int n);
uint32_t FCVTL_4s(int d, int n);
uint32_t XTN_4h(int d, int n);
uint32_t SXTL_4s(int d, int n);

uint32_t ZIP1_4s(int d, int n, int m);
uint32_t ZIP2_4s(int d, int n, int m);

uint32_t UZP1_8h(int d, int n, int m);
uint32_t UZP2_8h(int d, int n, int m);
uint32_t ZIP1_8h(int d, int n, int m);

uint32_t EXT_16b(int d, int n, int m, int imm);
uint32_t UXTL_4s(int d, int n);
uint32_t UXTL_8h(int d, int n);
uint32_t XTN_8b(int d, int n);
uint32_t INS_elem_s(int d, int dst_lane, int n, int src_lane);

uint32_t SHL_4s_imm(int d, int n, int sh);
uint32_t USHR_4s_imm(int d, int n, int sh);
uint32_t SSHR_4s_imm(int d, int n, int sh);
uint32_t UMOV_ws(int d, int n);
uint32_t UMOV_ws_lane(int d, int n, int lane);
uint32_t LD1_s(int t, int idx, int n);
uint32_t MOVI_4s(int d, uint8_t imm8, int shift);
uint32_t MVNI_4s(int d, uint8_t imm8, int shift);
uint32_t DUP_4s_w(int d, int n);
uint32_t DUP_4s_lane(int d, int n, int lane);
uint32_t INS_s(int d, int idx, int n);

uint32_t LD4_4h(int t, int n);
uint32_t ST4_4h(int t, int n);
uint32_t LD4_8h(int t, int n);
uint32_t ST4_8h(int t, int n);
uint32_t LD4_8b(int t, int n);
uint32_t ST4_8b(int t, int n);

uint32_t FMOV_4s_imm(int d, uint8_t imm8);
_Bool    movi_4s(struct asm_arm64 *c, int d, uint32_t v);
void     load_imm_w(struct asm_arm64 *c, int rd, uint32_t v);
