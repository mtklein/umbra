#include "../src/asm_arm64.h"
#include "test.h"
#include <stdlib.h>

// All encodings verified against `as` + `llvm-objdump`.

static void test_buf(void) {
    Buf b = {0};
    put(&b, 0xDEADBEEF);
    put(&b, 0xCAFEBABE);
    (b.len == 2)               here;
    (b.buf[0] == 0xDEADBEEF)  here;
    (b.buf[1] == 0xCAFEBABE)  here;
    free(b.buf);
}

static void test_gpr(void) {
    (RET()                == 0xD65F03C0) here;
    (ADD_xr(0, 1, 2)     == 0x8B020020) here;
    (ADD_xi(3, 4, 16)    == 0x91004083) here;
    (SUB_xi(5, 6, 8)     == 0xD10020C5) here;
    (SUBS_xi(7, 8, 1)    == 0xF1000507) here;
    (MOVZ_x(0, 42)       == 0xD2800540) here;
    (MOVZ_w(1, 0xFF)     == 0x52801FE1) here;
    (MOVK_w16(2, 0x1234) == 0x72A24682) here;
}

static void test_branch(void) {
    (B(4)            == 0x14000004) here;
    (Bcond(0, 3)     == 0x54000060) here;  // B.EQ +3
    (Bcond(1, 10)    == 0x54000141) here;  // B.NE +10
}

static void test_mem(void) {
    (LDR_sx(0, 1, 2) == 0xBC627820) here;
    (STR_sx(3, 4, 5) == 0xBC257883) here;
    (LDR_hx(0, 1, 2) == 0x7C627820) here;
    (STR_hx(3, 4, 5) == 0x7C257883) here;
    (LDR_q(0, 1, 2)  == 0x3CE26820) here;
    (STR_q(3, 4, 5)  == 0x3CA56883) here;
    (LDR_qi(6, 7, 1) == 0x3DC004E6) here;
    (STR_qi(8, 9, 2) == 0x3D800928) here;
    (LD4_8b(0, 4)    == 0x0C400080) here;
    (ST4_8b(0, 4)    == 0x0C000080) here;
}

static void test_neon_f32(void) {
    (FADD_4s(4, 5, 6)   == 0x4E26D4A4) here;
    (FSUB_4s(0, 1, 2)   == 0x4EA2D420) here;
    (FMUL_4s(7, 8, 9)   == 0x6E29DD07) here;
    (FDIV_4s(0, 1, 2)   == 0x6E22FC20) here;
    (FSQRT_4s(3, 4)     == 0x6EA1F883) here;
    (SCVTF_4s(5, 6)     == 0x4E21D8C5) here;
    (FCVTZS_4s(7, 8)    == 0x4EA1B907) here;
    (FCMEQ_4s(0, 1, 2)  == 0x4E22E420) here;
    (FCMGT_4s(3, 4, 5)  == 0x6EA5E483) here;
    (FCMGE_4s(6, 7, 8)  == 0x6E28E4E6) here;
}

static void test_neon_i32(void) {
    (ADD_4s(0, 1, 2)  == 0x4EA28420) here;
    (SUB_4s(3, 4, 5)  == 0x6EA58483) here;
    (MUL_4s(6, 7, 8)  == 0x4EA89CE6) here;
    (USHL_4s(0, 1, 2) == 0x6EA24420) here;
    (NEG_4s(3, 4)     == 0x6EA0B883) here;
    (CMEQ_4s(5, 6, 7) == 0x6EA78CC5) here;
    (CMGT_4s(0, 1, 2) == 0x4EA23420) here;
    (CMHI_4s(3, 4, 5) == 0x6EA53483) here;
}

static void test_neon_bitwise(void) {
    (AND_16b(10, 11, 12) == 0x4E2C1D6A) here;
    (ORR_16b(0, 1, 2)    == 0x4EA21C20) here;
    (EOR_16b(3, 4, 5)    == 0x6E251C83) here;
    (BSL_16b(13, 14, 15) == 0x6E6F1DCD) here;
    (BIT_16b(0, 1, 2)    == 0x6EA21C20) here;
    (BIF_16b(3, 4, 5)    == 0x6EE51C83) here;
}

static void test_conversions(void) {
    (FCVTN_4h(2, 3) == 0x0E216862) here;
    (FCVTL_4s(4, 5) == 0x0E2178A4) here;
    (XTN_4h(6, 7)   == 0x0E6128E6) here;
    (SXTL_4s(8, 9)  == 0x0F10A528) here;
}

static void test_W_promotion(void) {
    (W(FCVTN_4h(2, 3))   == 0x4E216862) here;   // fcvtn2
    (W(XTN_4h(6, 7))     == 0x4E6128E6) here;   // xtn2
    (W(SXTL_4s(8, 9))    == 0x4F10A528) here;   // sxtl2
}

static void test_shift_imm(void) {
    (SHL_4s_imm(2, 3, 4)  == 0x4F245462) here;
    (USHR_4s_imm(2, 3, 4) == 0x6F3C0462) here;
    (SSHR_4s_imm(2, 3, 4) == 0x4F3C0462) here;
}

static void test_movi(void) {
    (MOVI_4s(5, 0xFF, 0)  == 0x4F0707E5) here;
    (MVNI_4s(6, 0xAB, 0)  == 0x6F050566) here;
    (MOVI_4s(0, 0x42, 8)  == (0x4F000400u | ((uint32_t)((0x42>>5)&7)<<16) | (2u<<12) | ((uint32_t)(0x42&0x1F)<<5) | 0)) here;
}

static void test_dup_ins(void) {
    (DUP_4s_w(7, 8)  == 0x4E040D07) here;
    (UMOV_ws(0, 1)    == 0x0E043C20) here;
}

static void test_compare_zero(void) {
    (CMEQ_4s_z(0, 1)  == 0x4EA09820) here;
    (CMGT_4s_z(2, 3)  == 0x4EA08862) here;
    (FCMEQ_4s_z(4, 5) == 0x4EA0D8A4) here;
}

static void test_load_imm_w(void) {
    // Small value: just MOVZ
    Buf b = {0};
    load_imm_w(&b, 3, 42);
    (b.len == 1)                   here;
    (b.buf[0] == MOVZ_w(3, 42))   here;
    free(b.buf);

    // Large value: MOVZ + MOVK
    b = (Buf){0};
    load_imm_w(&b, 5, 0x12340056);
    (b.len == 2)                         here;
    (b.buf[0] == MOVZ_w(5, 0x0056))     here;
    (b.buf[1] == MOVK_w16(5, 0x1234))   here;
    free(b.buf);
}

static void test_stp_ldp(void) {
    (STP_pre(19, 20, 31, -2) == (0xA9800000u | ((uint32_t)(-2 & 0x7f) << 15) | (20u << 10) | (31u << 5) | 19)) here;
    (LDP_post(19, 20, 31, 2) == (0xA8C00000u | (2u << 15) | (20u << 10) | (31u << 5) | 19)) here;
    (LDP_qi(0, 1, 2, 3) == (0xAD400000u | ((3u & 0x7f) << 15) | (1u << 10) | (2u << 5) | 0)) here;
    (STP_qi(4, 5, 6, 2) == (0xAD000000u | ((2u & 0x7f) << 15) | (5u << 10) | (6u << 5) | 4)) here;
}

static void test_lsl(void) {
    (LSL_xi(0, 1, 4) == (0xD3400000u | (60u << 16) | (59u << 10) | (1u << 5) | 0)) here;
}

int main(void) {
    test_buf();
    test_gpr();
    test_branch();
    test_mem();
    test_neon_f32();
    test_neon_i32();
    test_neon_bitwise();
    test_conversions();
    test_W_promotion();
    test_shift_imm();
    test_movi();
    test_dup_ins();
    test_compare_zero();
    test_load_imm_w();
    test_stp_ldp();
    test_lsl();
    return 0;
}
