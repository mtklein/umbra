#pragma clang diagnostic ignored "-Wunused-function"
#include "src/asm_arm64.h"
#include <stdio.h>

static int failures;

#define expect(expr, expected) do { \
    uint32_t got = (expr); \
    uint32_t exp = (expected); \
    if (got != exp) { \
        printf("FAIL %s:%d: %s = 0x%08X, expected 0x%08X\n", \
               __FILE__, __LINE__, #expr, got, exp); \
        failures++; \
    } \
} while(0)

// Verify Buf+put basics
static void test_buf(void) {
    Buf b = {0};
    put(&b, 0xDEADBEEF);
    put(&b, 0xCAFEBABE);
    if (b.len != 2 || b.buf[0] != 0xDEADBEEF || b.buf[1] != 0xCAFEBABE) {
        printf("FAIL %s:%d: Buf/put\n", __FILE__, __LINE__);
        failures++;
    }
    free(b.buf);
}

// All encodings verified against `as` + `llvm-objdump`.
static void test_gpr(void) {
    expect(RET(),               0xD65F03C0);
    expect(ADD_xr(0, 1, 2),    0x8B020020);
    expect(ADD_xi(3, 4, 16),   0x91004083);
    expect(SUB_xi(5, 6, 8),    0xD10020C5);
    expect(SUBS_xi(7, 8, 1),   0xF1000507);
    expect(MOVZ_x(0, 42),      0xD2800540);
    expect(MOVZ_w(1, 0xFF),    0x52801FE1);
    expect(MOVK_w16(2, 0x1234),0x72A24682);
}

static void test_branch(void) {
    expect(B(4),            0x14000004);
    expect(Bcond(0, 3),     0x54000060);  // B.EQ +3
    expect(Bcond(1, 10),    0x54000141);  // B.NE +10
}

static void test_mem(void) {
    expect(LDR_sx(0, 1, 2), 0xBC627820);  // LDR S0,[X1,X2,LSL#2]
    expect(STR_sx(3, 4, 5), 0xBC257883);  // STR S3,[X4,X5,LSL#2]
    expect(LDR_hx(0, 1, 2), 0x7C627820);  // LDR H0,[X1,X2,LSL#1]
    expect(STR_hx(3, 4, 5), 0x7C257883);  // STR H3,[X4,X5,LSL#1]
    expect(LDR_q(0, 1, 2),  0x3CE26820);  // LDR Q0,[X1,X2]
    expect(STR_q(3, 4, 5),  0x3CA56883);  // STR Q3,[X4,X5]
    expect(LDR_qi(6, 7, 1), 0x3DC004E6);  // LDR Q6,[X7,#16]
    expect(STR_qi(8, 9, 2), 0x3D800928);  // STR Q8,[X9,#32]
    expect(LD4_8b(0, 4),    0x0C400080);
    expect(ST4_8b(0, 4),    0x0C000080);
}

static void test_neon_f32(void) {
    expect(FADD_4s(4, 5, 6),   0x4E26D4A4);
    expect(FSUB_4s(0, 1, 2),   0x4EA2D420);
    expect(FMUL_4s(7, 8, 9),   0x6E29DD07);
    expect(FDIV_4s(0, 1, 2),   0x6E22FC20);
    expect(FSQRT_4s(3, 4),     0x6EA1F883);
    expect(SCVTF_4s(5, 6),     0x4E21D8C5);
    expect(FCVTZS_4s(7, 8),    0x4EA1B907);
    expect(FCMEQ_4s(0, 1, 2),  0x4E22E420);
    expect(FCMGT_4s(3, 4, 5),  0x6EA5E483);
    expect(FCMGE_4s(6, 7, 8),  0x6E28E4E6);
}

static void test_neon_i32(void) {
    expect(ADD_4s(0, 1, 2),  0x4EA28420);
    expect(SUB_4s(3, 4, 5),  0x6EA58483);
    expect(MUL_4s(6, 7, 8),  0x4EA89CE6);
    expect(USHL_4s(0, 1, 2), 0x6EA24420);
    expect(NEG_4s(3, 4),     0x6EA0B883);
    expect(CMEQ_4s(5, 6, 7), 0x6EA78CC5);
    expect(CMGT_4s(0, 1, 2), 0x4EA23420);
    expect(CMHI_4s(3, 4, 5), 0x6EA53483);
}

static void test_neon_bitwise(void) {
    // and.16b v10, v11, v12: 0x4E2C1D6A
    expect(AND_16b(10, 11, 12), 0x4E2C1D6A);
    expect(ORR_16b(0, 1, 2),   0x4EA21C20);
    expect(EOR_16b(3, 4, 5),   0x6E251C83);
    expect(BSL_16b(13, 14, 15),0x6E6F1DCD);
    expect(BIT_16b(0, 1, 2),   0x6EA21C20);
    expect(BIF_16b(3, 4, 5),   0x6EE51C83);
}

static void test_neon_i16(void) {
    expect(ADD_4h(7, 8, 9),  0x0E698507);
    expect(SUB_4h(0, 1, 2),  0x2E628420);
    expect(MUL_4h(3, 4, 5),  0x0E659C83);
    expect(USHL_4h(6, 7, 8), 0x2E6844E6);
    expect(NEG_4h(0, 1),     0x2E60B820);
    expect(CMEQ_4h(2, 3, 4), 0x2E648C62);
}

static void test_neon_f16(void) {
    expect(FADD_4h(0, 1, 2), 0x0E421420);
    expect(FSUB_4h(3, 4, 5), 0x0EC51483);
    expect(FMUL_4h(6, 7, 8), 0x2E481CE6);
    expect(FDIV_4h(0, 1, 2), 0x2E423C20);
}

static void test_conversions(void) {
    expect(FCVTN_4h(2, 3),  0x0E216862);
    expect(FCVTL_4s(4, 5),  0x0E2178A4);
    expect(XTN_4h(6, 7),    0x0E6128E6);
    expect(SXTL_4s(8, 9),   0x0F10A528);
    expect(UXTL_8h(0, 1),   0x2F08A420);
}

static void test_W_promotion(void) {
    // W() sets bit 30 to promote D-reg ops to Q-reg ops.
    expect(W(FADD_4h(0, 1, 2)), 0x4E421420);  // fadd.8h
    expect(W(ADD_4h(7, 8, 9)),  0x4E698507);   // add.8h
    expect(W(FCVTN_4h(2, 3)),   0x4E216862);   // fcvtn2
    expect(W(XTN_4h(6, 7)),     0x4E6128E6);   // xtn2
    expect(W(SXTL_4s(8, 9)),    0x4F10A528);   // sxtl2
}

static void test_shift_imm(void) {
    expect(SHL_4s_imm(2, 3, 4),  0x4F245462);
    expect(USHR_4s_imm(2, 3, 4), 0x6F3C0462);
    expect(SSHR_4s_imm(2, 3, 4), 0x4F3C0462);
    expect(SHRN_4h(2, 3, 4),     0x0F1C8462);
    // 4H shifts
    expect(SHL_4h_imm(0, 1, 3),  0x0F135420);  // SHL v0.4h, v1.4h, #3: immh:immb=10011
    expect(USHR_4h_imm(0, 1, 3), 0x2F1D0420);  // USHR v0.4h, v1.4h, #3: 32-3=29=0x1D
    expect(SSHR_4h_imm(0, 1, 3), 0x0F1D0420);
}

static void test_movi(void) {
    expect(MOVI_4s(5, 0xFF, 0),  0x4F0707E5);
    expect(MVNI_4s(6, 0xAB, 0),  0x6F050566);
    // MOVI with LSL #8: cmode=2
    expect(MOVI_4s(0, 0x42, 8),
           0x4F000400u | ((uint32_t)((0x42>>5)&7)<<16) | (2u<<12) | ((uint32_t)(0x42&0x1F)<<5) | 0);
}

static void test_dup_ins(void) {
    expect(DUP_4s_w(7, 8),  0x4E040D07);  // DUP V7.4S, W8
    expect(DUP_4h_w(0, 1),  0x0E020C20);  // DUP V0.4H, W1
    expect(UMOV_ws(0, 1),   0x0E043C20);  // UMOV W0, V1.S[0]
}

static void test_compare_zero(void) {
    expect(CMEQ_4s_z(0, 1), 0x4EA09820);
    expect(CMGT_4s_z(2, 3), 0x4EA08862);
    expect(FCMEQ_4s_z(4, 5),0x4EA0D8A4);
    expect(CMEQ_4h_z(6, 7), 0x0E6098E6);
    expect(FCMGE_4h_z(0,1), 0x2EF8C820);
}

static void test_load_imm_w(void) {
    // Small value: just MOVZ
    Buf b = {0};
    load_imm_w(&b, 3, 42);
    if (b.len != 1 || b.buf[0] != MOVZ_w(3, 42)) {
        printf("FAIL %s:%d: load_imm_w small\n", __FILE__, __LINE__);
        failures++;
    }
    free(b.buf);

    // Large value: MOVZ + MOVK
    b = (Buf){0};
    load_imm_w(&b, 5, 0x12340056);
    if (b.len != 2 || b.buf[0] != MOVZ_w(5, 0x0056) || b.buf[1] != MOVK_w16(5, 0x1234)) {
        printf("FAIL %s:%d: load_imm_w large\n", __FILE__, __LINE__);
        failures++;
    }
    free(b.buf);
}

static void test_stp_ldp(void) {
    // STP_pre X19, X20, [SP, #-16]!  => imm7 = -16/8 = -2
    expect(STP_pre(19, 20, 31, -2),
           0xA9800000u | ((uint32_t)(-2 & 0x7f) << 15) | (20u << 10) | (31u << 5) | 19);
    // LDP_post X19, X20, [SP], #16  => imm7 = 16/8 = 2
    expect(LDP_post(19, 20, 31, 2),
           0xA8C00000u | (2u << 15) | (20u << 10) | (31u << 5) | 19);
    // STP_qi, LDP_qi for Q regs
    expect(LDP_qi(0, 1, 2, 3),
           0xAD400000u | ((3u & 0x7f) << 15) | (1u << 10) | (2u << 5) | 0);
    expect(STP_qi(4, 5, 6, 2),
           0xAD000000u | ((2u & 0x7f) << 15) | (5u << 10) | (6u << 5) | 4);
}

static void test_lsl(void) {
    // LSL X0, X1, #4 => UBFM X0, X1, #60, #59
    expect(LSL_xi(0, 1, 4),
           0xD3400000u | (60u << 16) | (59u << 10) | (1u << 5) | 0);
}

int main(void) {
    test_buf();
    test_gpr();
    test_branch();
    test_mem();
    test_neon_f32();
    test_neon_i32();
    test_neon_bitwise();
    test_neon_i16();
    test_neon_f16();
    test_conversions();
    test_W_promotion();
    test_shift_imm();
    test_movi();
    test_dup_ins();
    test_compare_zero();
    test_load_imm_w();
    test_stp_ldp();
    test_lsl();

    if (failures) {
        printf("%d failures\n", failures);
        return 1;
    }
    printf("asm_arm64: all tests passed\n");
    return 0;
}
