#pragma once
#include <stdint.h>

typedef struct {
    uint32_t *buf;
    int       len, cap;
} Buf;

void     put(Buf *b, uint32_t w);
uint32_t RET(void);
uint32_t ADD_xr(int d, int n, int m);
uint32_t ADD_xi(int d, int n, int imm12);
uint32_t SUB_xi(int d, int n, int imm12);
uint32_t SUBS_xi(int d, int n, int imm12);
uint32_t MOVZ_x(int d, uint16_t imm);
uint32_t MOVZ_w(int d, uint16_t imm);
uint32_t MOVK_w16(int d, uint16_t imm);
uint32_t STP_pre(int t1, int t2, int n, int imm7);
uint32_t LDP_post(int t1, int t2, int n, int imm7);
uint32_t B(int off26);
uint32_t Bcond(int cond, int off19);
uint32_t LDR_sx(int d, int n, int m);
uint32_t STR_sx(int d, int n, int m);
uint32_t STR_hx(int d, int n, int m);
uint32_t LDR_hx(int d, int n, int m);
uint32_t LDR_d(int d, int n, int m);
uint32_t STR_d(int d, int n, int m);
uint32_t LDR_q(int d, int n, int m);
uint32_t STR_q(int d, int n, int m);
uint32_t LSL_xi(int d, int n, int shift);
uint32_t LSR_xi(int d, int n, int shift);
uint32_t LDR_qi(int d, int n, int imm);
uint32_t STR_qi(int d, int n, int imm);
uint32_t LDR_si(int d, int n, int imm);
uint32_t STR_si(int d, int n, int imm);
uint32_t W(uint32_t insn);

// float 4S
uint32_t FADD_4s(int d, int n, int m);
uint32_t FSUB_4s(int d, int n, int m);
uint32_t FMUL_4s(int d, int n, int m);
uint32_t FDIV_4s(int d, int n, int m);
uint32_t FMLA_4s(int d, int n, int m);
uint32_t FMLS_4s(int d, int n, int m);
uint32_t FMINNM_4s(int d, int n, int m);
uint32_t FMAXNM_4s(int d, int n, int m);
uint32_t FSQRT_4s(int d, int n);
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

// int 4S
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

// bitwise 16B
uint32_t AND_16b(int d, int n, int m);
uint32_t ORR_16b(int d, int n, int m);
uint32_t EOR_16b(int d, int n, int m);
uint32_t BSL_16b(int d, int n, int m);
uint32_t BIT_16b(int d, int n, int m);
uint32_t BIF_16b(int d, int n, int m);

// compare against zero
uint32_t CMEQ_4s_z(int d, int n);
uint32_t CMGT_4s_z(int d, int n);
uint32_t CMGE_4s_z(int d, int n);
uint32_t CMLE_4s_z(int d, int n);
uint32_t CMLT_4s_z(int d, int n);
uint32_t FCMEQ_4s_z(int d, int n);
uint32_t FCMGT_4s_z(int d, int n);
uint32_t FCMGE_4s_z(int d, int n);
uint32_t FCMLE_4s_z(int d, int n);
uint32_t FCMLT_4s_z(int d, int n);

// conversions
uint32_t FCVTN_4h(int d, int n);
uint32_t FCVTL_4s(int d, int n);
uint32_t XTN_4h(int d, int n);
uint32_t SXTL_4s(int d, int n);

uint32_t UZP1_4s(int d, int n, int m);
uint32_t UZP2_4s(int d, int n, int m);
uint32_t ZIP1_4s(int d, int n, int m);
uint32_t ZIP2_4s(int d, int n, int m);

uint32_t LD2_4s(int t, int n);  // LD2 {Vt.4S, V(t+1).4S}, [Xn]
uint32_t ST2_4s(int t, int n);  // ST2 {Vt.4S, V(t+1).4S}, [Xn]

uint32_t SLI_4s_imm(int d, int n, int shift);

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
void     load_imm_w(Buf *c, int rd, uint32_t v);
