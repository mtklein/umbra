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
uint32_t LDR_hx(int d, int n, int m);
uint32_t STR_hx(int d, int n, int m);
uint32_t LDR_d(int d, int n, int m);
uint32_t STR_d(int d, int n, int m);
uint32_t LDR_q(int d, int n, int m);
uint32_t STR_q(int d, int n, int m);
uint32_t LSL_xi(int d, int n, int shift);
uint32_t LDR_qi(int d, int n, int imm);
uint32_t STR_qi(int d, int n, int imm);
uint32_t LDP_qi(int t1, int t2, int n, int imm);
uint32_t STP_qi(int t1, int t2, int n, int imm);
uint32_t LD4_8b(int t, int n);
uint32_t ST4_8b(int t, int n);
uint32_t W(uint32_t insn);

#define V3(name) uint32_t name(int d, int n, int m);
#define V2(name) uint32_t name(int d, int n);
V3(FADD_4s)
V3(FSUB_4s)
V3(FMUL_4s) V3(FDIV_4s) V3(FMLA_4s) V3(FMLS_4s) V3(FMINNM_4s) V3(FMAXNM_4s) V2(FSQRT_4s)
    V2(FABS_4s) V2(FNEG_4s) V2(FRINTN_4s) V2(FRINTM_4s) V2(FRINTP_4s) V2(SCVTF_4s)
        V2(FCVTZS_4s) V2(FCVTNS_4s) V2(FCVTMS_4s) V2(FCVTPS_4s) V3(FCMEQ_4s) V3(FCMGT_4s)
            V3(FCMGE_4s) V3(ADD_4s) V3(SUB_4s) V3(MUL_4s) V3(USHL_4s) V3(SSHL_4s)
                V2(NEG_4s) V3(CMEQ_4s) V3(CMGT_4s) V3(CMGE_4s) V3(CMHI_4s) V3(CMHS_4s)
                    V3(AND_16b) V3(ORR_16b) V3(EOR_16b) V3(BSL_16b) V3(BIT_16b)
                        V3(BIF_16b) V2(CMEQ_4s_z) V2(CMGT_4s_z) V2(CMGE_4s_z)
                            V2(CMLE_4s_z) V2(CMLT_4s_z) V2(FCMEQ_4s_z) V2(FCMGT_4s_z)
                                V2(FCMGE_4s_z) V2(FCMLE_4s_z) V2(FCMLT_4s_z) V2(FCVTN_4h)
                                    V2(FCVTL_4s) V2(XTN_4h) V2(SXTL_4s) uint32_t
    SLI_4s_imm(int d, int n, int shift);
#undef V3
#undef V2

uint32_t SHL_4s_imm(int d, int n, int sh);
uint32_t USHR_4s_imm(int d, int n, int sh);
uint32_t SSHR_4s_imm(int d, int n, int sh);
uint32_t UMOV_ws(int d, int n);
uint32_t UMOV_ws_lane(int d, int n, int lane);
uint32_t LD1_s(int t, int idx, int n);
uint32_t MOVI_4s(int d, uint8_t imm8, int shift);
uint32_t MVNI_4s(int d, uint8_t imm8, int shift);
uint32_t DUP_4s_w(int d, int n);
uint32_t INS_s(int d, int idx, int n);
void     load_imm_w(Buf *c, int rd, uint32_t v);
