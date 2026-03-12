#pragma clang diagnostic ignored "-Wunused-function"
#include "src/asm_x86.h"
#include <stdio.h>

static int failures;

#define expect_bytes(name, b_, ...) do { \
    uint8_t exp[] = { __VA_ARGS__ }; \
    int n = (int)sizeof(exp); \
    if ((b_).len != n || memcmp((b_).buf, exp, (size_t)n) != 0) { \
        printf("FAIL %s:%d: %s len=%d expected=%d\n  got:", \
               __FILE__, __LINE__, name, (b_).len, n); \
        for (int i_ = 0; i_ < (b_).len; i_++) printf(" %02x", (b_).buf[i_]); \
        printf("\n  exp:"); \
        for (int i_ = 0; i_ < n; i_++) printf(" %02x", exp[i_]); \
        printf("\n"); \
        failures++; \
    } \
} while(0)

static void reset(Buf *b) { b->len = 0; }

static void test_emit(void) {
    Buf b = {0};
    emit1(&b, 0xAB);
    emit4(&b, 0xDEADBEEF);
    expect_bytes("emit1+emit4", b, 0xAB, 0xEF, 0xBE, 0xAD, 0xDE);
    free(b.buf);
}

// All encodings verified against clang -target x86_64 + llvm-objdump.
static void test_ret_vzeroupper_nop(void) {
    Buf b = {0};
    ret(&b);
    expect_bytes("ret", b, 0xC3);
    reset(&b);

    vzeroupper(&b);
    expect_bytes("vzeroupper", b, 0xC5, 0xF8, 0x77);
    reset(&b);

    nop(&b);
    expect_bytes("nop", b, 0x90);
    free(b.buf);
}

static void test_gpr(void) {
    Buf b = {0};

    // xorl %eax, %eax => 31 c0
    xor_rr(&b, RAX, RAX);
    expect_bytes("xor eax,eax", b, 0x31, 0xC0);
    reset(&b);

    // xorl %r10d, %r10d => 45 31 d2
    xor_rr(&b, R10, R10);
    expect_bytes("xor r10,r10", b, 0x45, 0x31, 0xD2);
    reset(&b);

    // addq $8, %rdi => 48 83 c7 08
    add_ri(&b, RDI, 8);
    expect_bytes("add rdi,8", b, 0x48, 0x83, 0xC7, 0x08);
    reset(&b);

    // addq $256, %rsi => 48 81 c6 00 01 00 00
    add_ri(&b, RSI, 256);
    expect_bytes("add rsi,256", b, 0x48, 0x81, 0xC6, 0x00, 0x01, 0x00, 0x00);
    reset(&b);

    // subq $32, %r10 => 49 83 ea 20
    sub_ri(&b, R10, 32);
    expect_bytes("sub r10,32", b, 0x49, 0x83, 0xEA, 0x20);
    reset(&b);

    // cmpq %rcx, %rdx => 48 39 ca
    cmp_rr(&b, RDX, RCX);
    expect_bytes("cmp rdx,rcx", b, 0x48, 0x39, 0xCA);
    reset(&b);

    // cmpq $127, %r11 => 49 83 fb 7f
    cmp_ri(&b, R11, 127);
    expect_bytes("cmp r11,127", b, 0x49, 0x83, 0xFB, 0x7F);
    reset(&b);

    // movq %rsp, %r12 => 49 89 e4
    mov_rr(&b, R12, RSP);
    expect_bytes("mov r12,rsp", b, 0x49, 0x89, 0xE4);
    free(b.buf);
}

static void test_avx_f32(void) {
    Buf b = {0};

    // vaddps %ymm4, %ymm3, %ymm2 => c5 e4 58 d4
    vaddps(&b, 2, 3, 4);
    expect_bytes("vaddps", b, 0xC5, 0xE4, 0x58, 0xD4);
    reset(&b);

    // vsubps %ymm5, %ymm6, %ymm7 => c5 cc 5c fd
    vsubps(&b, 7, 6, 5);
    expect_bytes("vsubps", b, 0xC5, 0xCC, 0x5C, 0xFD);
    reset(&b);

    // vmulps
    vmulps(&b, 2, 3, 4);
    reset(&b);

    // vdivps
    vdivps(&b, 2, 3, 4);
    reset(&b);

    // vsqrtps
    vsqrtps(&b, 2, 3);
    reset(&b);

    // vcvtdq2ps
    vcvtdq2ps(&b, 2, 3);
    reset(&b);

    // vcvttps2dq
    vcvttps2dq(&b, 2, 3);
    free(b.buf);
}

static void test_avx_i32(void) {
    Buf b = {0};

    // vpaddd %ymm8, %ymm9, %ymm10 => c4 41 35 fe d0
    vpaddd(&b, 10, 9, 8);
    expect_bytes("vpaddd", b, 0xC4, 0x41, 0x35, 0xFE, 0xD0);
    reset(&b);

    // vpsubd
    vpsubd(&b, 2, 3, 4);
    reset(&b);

    // vpmulld
    vpmulld(&b, 2, 3, 4);
    free(b.buf);
}

static void test_avx_bitwise(void) {
    Buf b = {0};

    // vpxor %ymm3, %ymm3, %ymm3 => c5 e5 ef db
    vpxor(&b, 1, 3, 3, 3);
    expect_bytes("vpxor ymm", b, 0xC5, 0xE5, 0xEF, 0xDB);
    reset(&b);

    // vpand
    vpand(&b, 1, 2, 3, 4);
    reset(&b);

    // vpor
    vpor(&b, 1, 2, 3, 4);
    free(b.buf);
}

static void test_avx_cmp(void) {
    Buf b = {0};

    // vpcmpeqd %ymm4, %ymm4, %ymm4 => c5 dd 76 e4
    vpcmpeqd(&b, 4, 4, 4);
    expect_bytes("vpcmpeqd", b, 0xC5, 0xDD, 0x76, 0xE4);
    reset(&b);

    // vpcmpgtd
    vpcmpgtd(&b, 2, 3, 4);
    free(b.buf);
}

static void test_avx_i16(void) {
    Buf b = {0};

    // vpaddw %xmm5, %xmm6, %xmm7 => c5 c9 fd fd
    vpaddw(&b, 7, 6, 5);
    expect_bytes("vpaddw", b, 0xC5, 0xC9, 0xFD, 0xFD);
    reset(&b);

    // vpsubw %xmm8, %xmm9, %xmm10 => c4 41 31 f9 d0
    vpsubw(&b, 10, 9, 8);
    expect_bytes("vpsubw", b, 0xC4, 0x41, 0x31, 0xF9, 0xD0);
    reset(&b);

    // vpmullw
    vpmullw(&b, 2, 3, 4);
    reset(&b);

    // vpcmpeqw
    vpcmpeqw(&b, 2, 3, 4);
    reset(&b);

    // vpcmpgtw
    vpcmpgtw(&b, 2, 3, 4);
    free(b.buf);
}

static void test_avx_shift(void) {
    Buf b = {0};

    // vpsrld $4, %ymm3, %ymm2 => c5 ed 72 d3 04
    vpsrld_i(&b, 2, 3, 4);
    expect_bytes("vpsrld", b, 0xC5, 0xED, 0x72, 0xD3, 0x04);
    reset(&b);

    // vpslld $8, %ymm5, %ymm4 => c5 dd 72 f5 08
    vpslld_i(&b, 4, 5, 8);
    expect_bytes("vpslld", b, 0xC5, 0xDD, 0x72, 0xF5, 0x08);
    reset(&b);

    // vpsrad
    vpsrad_i(&b, 2, 3, 4);
    reset(&b);

    // i16 shifts
    vpsllw_i(&b, 2, 3, 4);
    reset(&b);

    vpsrlw_i(&b, 2, 3, 4);
    reset(&b);

    vpsraw_i(&b, 2, 3, 4);
    free(b.buf);
}

static void test_avx_convert(void) {
    Buf b = {0};

    // vcvtph2ps %xmm3, %ymm2 => c4 e2 7d 13 d3
    vcvtph2ps(&b, 2, 3);
    expect_bytes("vcvtph2ps", b, 0xC4, 0xE2, 0x7D, 0x13, 0xD3);
    reset(&b);

    // vcvtps2ph $4, %ymm5, %xmm4 => c4 e3 7d 1d ec 04
    vcvtps2ph(&b, 4, 5, 4);
    expect_bytes("vcvtps2ph", b, 0xC4, 0xE3, 0x7D, 0x1D, 0xEC, 0x04);
    reset(&b);

    // vpmovzxwd %xmm6, %ymm7 => c4 e2 7d 33 fe
    vpmovzxwd(&b, 7, 6);
    expect_bytes("vpmovzxwd", b, 0xC4, 0xE2, 0x7D, 0x33, 0xFE);
    reset(&b);

    // vpmovsxwd %xmm8, %ymm9 => c4 42 7d 23 c8
    vpmovsxwd(&b, 9, 8);
    expect_bytes("vpmovsxwd", b, 0xC4, 0x42, 0x7D, 0x23, 0xC8);
    free(b.buf);
}

static void test_avx_extract_insert(void) {
    Buf b = {0};

    // vextracti128 $1, %ymm7, %xmm6 => c4 e3 7d 39 fe 01
    vextracti128(&b, 6, 7, 1);
    expect_bytes("vextracti128", b, 0xC4, 0xE3, 0x7D, 0x39, 0xFE, 0x01);
    reset(&b);

    // vinserti128 $1, %xmm8, %ymm9, %ymm10 => c4 43 35 38 d0 01
    vinserti128(&b, 10, 9, 8, 1);
    expect_bytes("vinserti128", b, 0xC4, 0x43, 0x35, 0x38, 0xD0, 0x01);
    free(b.buf);
}

static void test_avx_mov(void) {
    Buf b = {0};

    // vmovaps %ymm3, %ymm4 => c5 fc 28 e3
    vmovaps(&b, 4, 3);
    expect_bytes("vmovaps ymm", b, 0xC5, 0xFC, 0x28, 0xE3);
    reset(&b);

    // vmovaps %xmm5, %xmm6 => c5 f8 28 f5
    vmovaps_x(&b, 6, 5);
    expect_bytes("vmovaps xmm", b, 0xC5, 0xF8, 0x28, 0xF5);
    free(b.buf);
}

static void test_avx_broadcast(void) {
    Buf b = {0};

    // vbroadcastss %xmm0, %ymm2 => c4 e2 7d 18 d0
    vbroadcastss(&b, 2, 0);
    expect_bytes("vbroadcastss", b, 0xC4, 0xE2, 0x7D, 0x18, 0xD0);
    free(b.buf);
}

static void test_broadcast_imm32(void) {
    Buf b = {0};

    // Zero => vpxor ymm,ymm,ymm
    broadcast_imm32(&b, 3, 0);
    // Should be vpxor %ymm3, %ymm3, %ymm3 => c5 e5 ef db
    expect_bytes("bcast32 zero", b, 0xC5, 0xE5, 0xEF, 0xDB);
    reset(&b);

    // All ones => vpcmpeqd ymm,ymm,ymm
    broadcast_imm32(&b, 4, 0xFFFFFFFF);
    expect_bytes("bcast32 ones", b, 0xC5, 0xDD, 0x76, 0xE4);
    free(b.buf);
}

static void test_broadcast_imm16(void) {
    Buf b = {0};

    // Zero => vpxor xmm,xmm,xmm
    broadcast_imm16(&b, 3, 0);
    // vpxor %xmm3, %xmm3, %xmm3 => c5 e1 ef db
    expect_bytes("bcast16 zero", b, 0xC5, 0xE1, 0xEF, 0xDB);
    reset(&b);

    // All ones => vpcmpeqw xmm,xmm,xmm
    broadcast_imm16(&b, 4, 0xFFFF);
    expect_bytes("bcast16 ones", b, 0xC5, 0xD9, 0x75, 0xE4);
    free(b.buf);
}

static void test_vex_2byte_vs_3byte(void) {
    // 2-byte VEX is used when: mm==1, W==0, B==1 (i.e. rm reg 0-7), X==1
    // 3-byte VEX is needed for: mm!=1, W!=0, or high regs (8-15) in rm position
    Buf b = {0};

    // Low regs only => 2-byte VEX prefix
    // vaddps %ymm2, %ymm3, %ymm4 => 2-byte VEX (C5)
    vaddps(&b, 4, 3, 2);
    if (b.len < 1 || b.buf[0] != 0xC5) {
        printf("FAIL %s:%d: expected 2-byte VEX (0xC5) for low regs, got 0x%02X\n",
               __FILE__, __LINE__, b.buf[0]);
        failures++;
    }
    reset(&b);

    // High reg in rm => 3-byte VEX prefix
    // vpaddd %ymm8, %ymm9, %ymm10 => 3-byte VEX (C4)
    vpaddd(&b, 10, 9, 8);
    if (b.len < 1 || b.buf[0] != 0xC4) {
        printf("FAIL %s:%d: expected 3-byte VEX (0xC4) for high regs, got 0x%02X\n",
               __FILE__, __LINE__, b.buf[0]);
        failures++;
    }
    reset(&b);

    // mm==2 (0F38) always needs 3-byte VEX
    // vcvtph2ps => VEX.66.0F38 13 => 3-byte
    vcvtph2ps(&b, 2, 3);
    if (b.len < 1 || b.buf[0] != 0xC4) {
        printf("FAIL %s:%d: expected 3-byte VEX (0xC4) for mm=2, got 0x%02X\n",
               __FILE__, __LINE__, b.buf[0]);
        failures++;
    }
    free(b.buf);
}

static void test_pack_unpack(void) {
    Buf b = {0};
    vpackuswb(&b, 2, 3, 4);
    reset(&b);
    vpunpcklbw(&b, 2, 3, 4);
    free(b.buf);
}

static void test_blend(void) {
    Buf b = {0};
    vpblendvb(&b, 1, 2, 3, 4, 5);
    reset(&b);
    vpblendvb(&b, 0, 2, 3, 4, 5);
    free(b.buf);
}

static void test_fma(void) {
    Buf b = {0};
    vfmadd132ps(&b, 2, 3, 4);
    reset(&b);
    vfmadd213ps(&b, 2, 3, 4);
    reset(&b);
    vfmadd231ps(&b, 2, 3, 4);
    reset(&b);
    vfnmadd132ps(&b, 2, 3, 4);
    reset(&b);
    vfnmadd213ps(&b, 2, 3, 4);
    reset(&b);
    vfnmadd231ps(&b, 2, 3, 4);
    free(b.buf);
}

static void test_variable_shift(void) {
    Buf b = {0};
    vpsllvd(&b, 2, 3, 4);
    reset(&b);
    vpsrlvd(&b, 2, 3, 4);
    reset(&b);
    vpsravd(&b, 2, 3, 4);
    free(b.buf);
}

static void test_vcmpps(void) {
    Buf b = {0};
    // vcmpps with EQ predicate (0)
    vcmpps(&b, 2, 3, 4, 0);
    reset(&b);
    // vcmpps with LT predicate (1)
    vcmpps(&b, 2, 3, 4, 1);
    free(b.buf);
}

int main(void) {
    test_emit();
    test_ret_vzeroupper_nop();
    test_gpr();
    test_avx_f32();
    test_avx_i32();
    test_avx_bitwise();
    test_avx_cmp();
    test_avx_i16();
    test_avx_shift();
    test_avx_convert();
    test_avx_extract_insert();
    test_avx_mov();
    test_avx_broadcast();
    test_broadcast_imm32();
    test_broadcast_imm16();
    test_vex_2byte_vs_3byte();
    test_pack_unpack();
    test_blend();
    test_fma();
    test_variable_shift();
    test_vcmpps();

    if (failures) {
        printf("%d failures\n", failures);
        return 1;
    }
    printf("asm_x86: all tests passed\n");
    return 0;
}
