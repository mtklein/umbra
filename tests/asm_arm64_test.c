#include "../src/asm_arm64.h"
#include "test.h"

// All encodings verified against `as` + `llvm-objdump`.

TEST(test_buf) {
    struct Buf b = {0};
    put(&b, 0xDEADBEEF);
    put(&b, 0xCAFEBABE);
    b.words == 2 here;
    b.word[0] == 0xDEADBEEF here;
    b.word[1] == 0xCAFEBABE here;

    // Push past initial mmap to cover the grow path.
    for (int i = b.words; i < 1025; i++) { put(&b, NOP()); }
    b.words == 1025 here;
    b.word[0] == 0xDEADBEEF here;
    b.word[1] == 0xCAFEBABE here;
    Buf_free(&b);
}

TEST(test_gpr) {
    RET() == 0xD65F03C0 here;
    NOP() == 0xD503201F here;
    ADD_xr(0, 1, 2) == 0x8B020020 here;
    SUB_xr(10, 0, 9) == 0xCB09000A here;
    ADD_xi(3, 4, 16) == 0x91004083 here;
    SUB_xi(5, 6, 8) == 0xD10020C5 here;
    SUBS_xi(7, 8, 1) == 0xF1000507 here;
    CMP_xr(9, 0) == 0xEB00013F here;
    MOVZ_w(1, 0xFF) == 0x52801FE1 here;
    MOVZ_x_lsl16(13, 0x7FFF) == 0xD2AFFFED here;
    MOVK_w16(2, 0x1234) == 0x72A24682 here;
    LSR_wi(13, 13, 2) == 0x53027DAD here;
}

TEST(test_branch) {
    B(4) == 0x14000004 here;
    Bcond(0, 3) == 0x54000060 here;  // B.EQ +3
    Bcond(1, 10) == 0x54000141 here; // B.NE +10
}

TEST(test_mem) {
    LDR_sx(0, 1, 2) == 0xBC627820 here;
    STR_sx(3, 4, 5) == 0xBC257883 here;
    STR_hx(3, 4, 5) == 0x7C257883 here;
    LDR_hx(0, 1, 2) == 0x7C627820 here;
    LDR_d(0, 1, 2) == 0xFC626820 here;
    STR_d(3, 4, 5) == 0xFC256883 here;
    LDR_q(0, 1, 2) == 0x3CE26820 here;
    STR_q(3, 4, 5) == 0x3CA56883 here;
    LDR_qi(6, 7, 1) == 0x3DC004E6 here;
    STR_qi(8, 9, 2) == 0x3D800928 here;
}

TEST(test_neon_f32) {
    FADD_4s(4, 5, 6) == 0x4E26D4A4 here;
    FSUB_4s(0, 1, 2) == 0x4EA2D420 here;
    FMUL_4s(7, 8, 9) == 0x6E29DD07 here;
    FDIV_4s(0, 1, 2) == 0x6E22FC20 here;
    FMLA_4s(0, 1, 2) == 0x4E22CC20 here;
    FMLS_4s(3, 4, 5) == 0x4EA5CC83 here;
    FMINNM_4s(0, 1, 2) == 0x4EA2C420 here;
    FMAXNM_4s(3, 4, 5) == 0x4E25C483 here;
    FSQRT_4s(3, 4) == 0x6EA1F883 here;
    FABS_4s(0, 1) == 0x4EA0F820 here;
    FNEG_4s(2, 3) == 0x6EA0F862 here;
    FRINTN_4s(4, 5) == 0x4E2188A4 here;
    FRINTM_4s(6, 7) == 0x4E2198E6 here;
    FRINTP_4s(0, 1) == 0x4EA18820 here;
    SCVTF_4s(5, 6) == 0x4E21D8C5 here;
    FCVTZS_4s(7, 8) == 0x4EA1B907 here;
    FCVTNS_4s(2, 3) == 0x4E21A862 here;
    FCVTMS_4s(4, 5) == 0x4E21B8A4 here;
    FCVTPS_4s(6, 7) == 0x4EA1A8E6 here;
    FCMEQ_4s(0, 1, 2) == 0x4E22E420 here;
    FCMGT_4s(3, 4, 5) == 0x6EA5E483 here;
    FCMGE_4s(6, 7, 8) == 0x6E28E4E6 here;
}

TEST(test_neon_i32) {
    ADD_4s(0, 1, 2) == 0x4EA28420 here;
    SUB_4s(3, 4, 5) == 0x6EA58483 here;
    MUL_4s(6, 7, 8) == 0x4EA89CE6 here;
    USHL_4s(0, 1, 2) == 0x6EA24420 here;
    SSHL_4s(0, 1, 2) == 0x4EA24420 here;
    NEG_4s(3, 4) == 0x6EA0B883 here;
    CMEQ_4s(5, 6, 7) == 0x6EA78CC5 here;
    CMGT_4s(0, 1, 2) == 0x4EA23420 here;
    CMGE_4s(0, 1, 2) == 0x4EA23C20 here;
    CMHI_4s(3, 4, 5) == 0x6EA53483 here;
    CMHS_4s(3, 4, 5) == 0x6EA53C83 here;
}

TEST(test_neon_bitwise) {
    AND_16b(10, 11, 12) == 0x4E2C1D6A here;
    ORR_16b(0, 1, 2) == 0x4EA21C20 here;
    EOR_16b(3, 4, 5) == 0x6E251C83 here;
    BSL_16b(13, 14, 15) == 0x6E6F1DCD here;
    BIT_16b(0, 1, 2) == 0x6EA21C20 here;
    BIF_16b(3, 4, 5) == 0x6EE51C83 here;
}

TEST(test_conversions) {
    FCVTN_4h(2, 3) == 0x0E216862 here;
    FCVTL_4s(4, 5) == 0x0E2178A4 here;
    XTN_4h(6, 7) == 0x0E6128E6 here;
    SXTL_4s(8, 9) == 0x0F10A528 here;
}

TEST(test_W_promotion) {
    W(FCVTN_4h(2, 3)) == 0x4E216862 here; // fcvtn2
    W(XTN_4h(6, 7)) == 0x4E6128E6 here;   // xtn2
    W(SXTL_4s(8, 9)) == 0x4F10A528 here;  // sxtl2
}

TEST(test_shift_imm) {
    SHL_4s_imm(2, 3, 4) == 0x4F245462 here;
    USHR_4s_imm(2, 3, 4) == 0x6F3C0462 here;
    SSHR_4s_imm(2, 3, 4) == 0x4F3C0462 here;
}

TEST(test_movi) {
    MOVI_4s(5, 0xFF, 0) == 0x4F0707E5 here;
    MVNI_4s(6, 0xAB, 0) == 0x6F050566 here;
    (MOVI_4s(0, 0x42, 8)
     == (0x4F000400u
         | ((uint32_t)((0x42 >> 5) & 7) << 16)
         | (2u << 12)
         | ((uint32_t)(0x42 & 0x1F) << 5)
         | 0)) here;
}

TEST(test_dup_ins) {
    DUP_4s_w(7, 8) == 0x4E040D07 here;
    DUP_4s_lane(0, 1, 0) == 0x4E040420 here;
    DUP_4s_lane(0, 1, 2) == 0x4E140420 here;
    DUP_4s_lane(3, 4, 1) == 0x4E0C0483 here;
    DUP_4s_lane(5, 6, 3) == 0x4E1C04C5 here;
    UMOV_ws(0, 1) == 0x0E043C20 here;
    UMOV_ws_lane(0, 1, 0) == 0x0E043C20 here;
    UMOV_ws_lane(0, 1, 2) == 0x0E143C20 here;
    LD1_s(0, 0, 1) == 0x0D408020 here;
    LD1_s(2, 1, 3) == 0x0D409062 here;
    LD1_s(4, 2, 5) == 0x4D4080A4 here;
    INS_s(0, 1, 2) == 0x4E0C1C40 here;
    INS_s(3, 0, 4) == 0x4E041C83 here;
}

TEST(test_load_imm_w) {
    // Small value: just MOVZ
    struct Buf b = {0};
    load_imm_w(&b, 3, 42);
    b.words == 1 here;
    b.word[0] == MOVZ_w(3, 42) here;
    Buf_free(&b);

    // Large value: MOVZ + MOVK
    b = (struct Buf){0};
    load_imm_w(&b, 5, 0x12340056);
    b.words == 2 here;
    b.word[0] == MOVZ_w(5, 0x0056) here;
    b.word[1] == MOVK_w16(5, 0x1234) here;
    Buf_free(&b);
}

TEST(test_stp_ldp) {
    (STP_pre(19, 20, 31, -2)
     == (0xA9800000u | ((uint32_t)(-2 & 0x7f) << 15) | (20u << 10) | (31u << 5) | 19)) here;
    (LDP_post(19, 20, 31, 2) == (0xA8C00000u | (2u << 15) | (20u << 10) | (31u << 5) | 19))
        here;
}

TEST(test_lsl) {
    LSL_xi(0, 1, 4) == (0xD3400000u | (60u << 16) | (59u << 10) | (1u << 5) | 0) here;
    LSR_xi(0, 1, 2)  == 0xD342FC20 here;
    LSR_xi(3, 4, 5)  == 0xD345FC83 here;
}

TEST(test_ldr_si) {
    LDR_si(3, 4, 0) == 0xBD400083 here;
    LDR_si(3, 4, 1) == 0xBD400483 here;
}

TEST(test_zip) {
    ZIP1_4s(6, 7, 8) == (0x4E803800u | (8u << 16) | (7u << 5) | 6u) here;
    ZIP2_4s(0, 1, 2) == (0x4E807800u | (2u << 16) | (1u << 5) | 0u) here;
}

TEST(test_ldr_xi) {
    // LDR x0, [x1, #16] (imm=2, scaled by 8)
    LDR_xi(0, 1, 2) == 0xF9400820 here;
    // LDR x3, [x4, #0]
    LDR_xi(3, 4, 0) == 0xF9400083 here;
}

TEST(test_ldrh_wi) {
    // LDRH w0, [x1, #4] (imm=2, scaled by 2)
    LDRH_wi(0, 1, 2) == 0x79400820 here;
    // LDRH w3, [x4, #0]
    LDRH_wi(3, 4, 0) == 0x79400083 here;
}

TEST(test_ldrh_wr) {
    // LDRH w3, [x4, x5, LSL #1]
    LDRH_wr(3, 4, 5) == 0x78657883 here;
}

TEST(test_ldr_wr) {
    // LDR w3, [x4, x5, LSL #2]
    LDR_wr(3, 4, 5) == 0xB8657883 here;
}

TEST(test_ldr_str_di) {
    // LDR d3, [x4, #16] (imm=2, scaled by 8)
    LDR_di(3, 4, 2) == 0xFD400883 here;
    // LDR d0, [x1, #0]
    LDR_di(0, 1, 0) == 0xFD400020 here;
    // STR d5, [x6, #24] (imm=3, scaled by 8)
    STR_di(5, 6, 3) == 0xFD000CC5 here;
    // STR d0, [x1, #0]
    STR_di(0, 1, 0) == 0xFD000020 here;
}

TEST(test_madd) {
    // MADD x0, x1, x2, x3
    MADD_x(0, 1, 2, 3) == 0x9B020C20 here;
}

TEST(test_cmp) {
    CMP_wr(5, 6) == 0x6B0600BF here;
    CMP_wr(0, 0) == 0x6B00001F here;
}

TEST(test_uzp_zip_8h) {
    // UZP1.8H v0, v1, v2
    UZP1_8h(0, 1, 2) == 0x4E421820 here;
    // UZP2.8H v3, v4, v5
    UZP2_8h(3, 4, 5) == 0x4E455883 here;
    // ZIP1.8H v6, v7, v8
    ZIP1_8h(6, 7, 8) == 0x4E4838E6 here;
}

TEST(test_ext_16b) {
    // EXT.16B v0, v1, v2, #8
    EXT_16b(0, 1, 2, 8) == 0x6E024020 here;
}

TEST(test_uxtl_4s) {
    // USHLL.4S v3, v4, #0 (= UXTL.4S)
    UXTL_4s(3, 4) == 0x2F10A483 here;
}

TEST(test_ins_elem_s) {
    // INS v0.s[1], v1.s[0]
    INS_elem_s(0, 1, 1, 0) == 0x6E0C0420 here;
    // INS v2.s[2], v3.s[0]
    INS_elem_s(2, 2, 3, 0) == 0x6E140462 here;
    // INS v4.s[3], v5.s[0]
    INS_elem_s(4, 3, 5, 0) == 0x6E1C04A4 here;
    // INS v6.s[0], v7.s[2]
    INS_elem_s(6, 0, 7, 2) == 0x6E0444E6 here;
}

TEST(test_stp_ldp_qi) {
    STP_qi_pre(8, 9, 31, -8) == 0xADBC27E8 here;
    STP_qi(10, 11, 31, 2) == 0xAD012FEA here;
    STP_qi(12, 13, 31, 4) == 0xAD0237EC here;
    STP_qi(14, 15, 31, 6) == 0xAD033FEE here;
    LDP_qi(14, 15, 31, -2) == 0xAD7F3FEE here;
    LDP_qi(12, 13, 31, -4) == 0xAD7E37EC here;
    LDP_qi(8, 9, 31, -8) == 0xAD7C27E8 here;
}

TEST(test_ldr_q_literal) {
    LDR_q_literal(5) == 0x9C000005 here;
    LDR_q_literal(0) == 0x9C000000 here;
}

TEST(test_uxtl_8h_xtn_8b) {
    UXTL_8h(0, 1) == 0x2F08A420 here;
    XTN_8b(0, 1) == 0x0E212820 here;
    W(UXTL_8h(0, 1)) == 0x6F08A420 here;
    W(XTN_8b(0, 1)) == 0x4E212820 here;
}

TEST(test_ld4_st4) {
    LD4_4h(0, 5) == 0x0C4004A0 here;
    ST4_4h(0, 5) == 0x0C0004A0 here;
    LD4_8h(0, 5) == 0x4C4004A0 here;
    ST4_8h(0, 5) == 0x4C0004A0 here;
    LD4_8b(0, 5) == 0x0C4000A0 here;
    ST4_8b(0, 5) == 0x0C0000A0 here;
    LD4_4h(2, 10) == 0x0C400542 here;
    ST4_8h(8, 3) == 0x4C000468 here;
}
