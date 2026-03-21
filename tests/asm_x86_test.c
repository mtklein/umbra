#include "../src/asm_x86.h"
#include "test.h"
#include <stdlib.h>
#include <string.h>

// All encodings verified against clang -target x86_64 + llvm-objdump.

static _Bool bytes_eq(Buf *b, int n, const uint8_t exp[]) {
    return b->len == n && memcmp(b->buf, exp, (size_t)n) == 0;
}

static void reset(Buf *b) { b->len = 0; }

static void test_emit(void) {
    Buf b = {0};
    emit1(&b, 0xAB);
    emit4(&b, 0xDEADBEEF);
    (b.len == 5)         here;
    (b.buf[0] == 0xAB)  here;
    (b.buf[1] == 0xEF)  here;
    (b.buf[2] == 0xBE)  here;
    (b.buf[3] == 0xAD)  here;
    (b.buf[4] == 0xDE)  here;
    free(b.buf);
}

static void test_ret_vzeroupper_nop(void) {
    Buf b = {0};
    ret(&b);
    (bytes_eq(&b, 1, (uint8_t[]){0xC3})) here;
    reset(&b);

    vzeroupper(&b);
    (bytes_eq(&b, 3, (uint8_t[]){0xC5, 0xF8, 0x77})) here;
    reset(&b);

    nop(&b);
    (bytes_eq(&b, 1, (uint8_t[]){0x90})) here;
    free(b.buf);
}

static void test_gpr(void) {
    Buf b = {0};

    // xorl %eax, %eax => 31 c0
    xor_rr(&b, RAX, RAX);
    (bytes_eq(&b, 2, (uint8_t[]){0x31, 0xC0})) here;
    reset(&b);

    // xorl %r10d, %r10d => 45 31 d2
    xor_rr(&b, R10, R10);
    (bytes_eq(&b, 3, (uint8_t[]){0x45, 0x31, 0xD2})) here;
    reset(&b);

    // addq $8, %rdi => 48 83 c7 08
    add_ri(&b, RDI, 8);
    (bytes_eq(&b, 4, (uint8_t[]){0x48, 0x83, 0xC7, 0x08})) here;
    reset(&b);

    // addq $256, %rsi => 48 81 c6 00 01 00 00
    add_ri(&b, RSI, 256);
    (bytes_eq(&b, 7, (uint8_t[]){0x48, 0x81, 0xC6, 0x00, 0x01, 0x00, 0x00})) here;
    reset(&b);

    // subq $32, %r10 => 49 83 ea 20
    sub_ri(&b, R10, 32);
    (bytes_eq(&b, 4, (uint8_t[]){0x49, 0x83, 0xEA, 0x20})) here;
    reset(&b);

    // cmpq %rcx, %rdx => 48 39 ca
    cmp_rr(&b, RDX, RCX);
    (bytes_eq(&b, 3, (uint8_t[]){0x48, 0x39, 0xCA})) here;
    reset(&b);

    // cmpq $127, %r11 => 49 83 fb 7f
    cmp_ri(&b, R11, 127);
    (bytes_eq(&b, 4, (uint8_t[]){0x49, 0x83, 0xFB, 0x7F})) here;
    reset(&b);

    // movq %rsp, %r12 => 49 89 e4
    mov_rr(&b, R12, RSP);
    (bytes_eq(&b, 3, (uint8_t[]){0x49, 0x89, 0xE4})) here;
    free(b.buf);
}

static void test_avx_f32(void) {
    Buf b = {0};

    // vaddps %ymm4, %ymm3, %ymm2 => c5 e4 58 d4
    vaddps(&b, 2, 3, 4);
    (bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xE4, 0x58, 0xD4})) here;
    reset(&b);

    // vsubps %ymm5, %ymm6, %ymm7 => c5 cc 5c fd
    vsubps(&b, 7, 6, 5);
    (bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xCC, 0x5C, 0xFD})) here;
    free(b.buf);
}

static void test_avx_i32(void) {
    Buf b = {0};

    // vpaddd %ymm8, %ymm9, %ymm10 => c4 41 35 fe d0
    vpaddd(&b, 10, 9, 8);
    (bytes_eq(&b, 5, (uint8_t[]){0xC4, 0x41, 0x35, 0xFE, 0xD0})) here;
    free(b.buf);
}

static void test_avx_bitwise(void) {
    Buf b = {0};

    // vpxor %ymm3, %ymm3, %ymm3 => c5 e5 ef db
    vpxor(&b, 1, 3, 3, 3);
    (bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xE5, 0xEF, 0xDB})) here;
    free(b.buf);
}

static void test_avx_cmp(void) {
    Buf b = {0};

    // vpcmpeqd %ymm4, %ymm4, %ymm4 => c5 dd 76 e4
    vpcmpeqd(&b, 4, 4, 4);
    (bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xDD, 0x76, 0xE4})) here;
    free(b.buf);
}

static void test_avx_shift(void) {
    Buf b = {0};

    // vpsrld $4, %ymm3, %ymm2 => c5 ed 72 d3 04
    vpsrld_i(&b, 2, 3, 4);
    (bytes_eq(&b, 5, (uint8_t[]){0xC5, 0xED, 0x72, 0xD3, 0x04})) here;
    reset(&b);

    // vpslld $8, %ymm5, %ymm4 => c5 dd 72 f5 08
    vpslld_i(&b, 4, 5, 8);
    (bytes_eq(&b, 5, (uint8_t[]){0xC5, 0xDD, 0x72, 0xF5, 0x08})) here;
    free(b.buf);
}

static void test_avx_convert(void) {
    Buf b = {0};

    // vcvtph2ps %xmm3, %ymm2 => c4 e2 7d 13 d3
    vcvtph2ps(&b, 2, 3);
    (bytes_eq(&b, 5, (uint8_t[]){0xC4, 0xE2, 0x7D, 0x13, 0xD3})) here;
    reset(&b);

    // vcvtps2ph $4, %ymm5, %xmm4 => c4 e3 7d 1d ec 04
    vcvtps2ph(&b, 4, 5, 4);
    (bytes_eq(&b, 6, (uint8_t[]){0xC4, 0xE3, 0x7D, 0x1D, 0xEC, 0x04})) here;
    reset(&b);

    // vpmovzxwd %xmm6, %ymm7 => c4 e2 7d 33 fe
    vpmovzxwd(&b, 7, 6);
    (bytes_eq(&b, 5, (uint8_t[]){0xC4, 0xE2, 0x7D, 0x33, 0xFE})) here;
    reset(&b);

    // vpmovsxwd %xmm8, %ymm9 => c4 42 7d 23 c8
    vpmovsxwd(&b, 9, 8);
    (bytes_eq(&b, 5, (uint8_t[]){0xC4, 0x42, 0x7D, 0x23, 0xC8})) here;
    free(b.buf);
}

static void test_avx_extract_insert(void) {
    Buf b = {0};

    // vextracti128 $1, %ymm7, %xmm6 => c4 e3 7d 39 fe 01
    vextracti128(&b, 6, 7, 1);
    (bytes_eq(&b, 6, (uint8_t[]){0xC4, 0xE3, 0x7D, 0x39, 0xFE, 0x01})) here;
    reset(&b);

    // vinserti128 $1, %xmm8, %ymm9, %ymm10 => c4 43 35 38 d0 01
    vinserti128(&b, 10, 9, 8, 1);
    (bytes_eq(&b, 6, (uint8_t[]){0xC4, 0x43, 0x35, 0x38, 0xD0, 0x01})) here;
    free(b.buf);
}

static void test_avx_mov(void) {
    Buf b = {0};

    // vmovaps %ymm3, %ymm4 => c5 fc 28 e3
    vmovaps(&b, 4, 3);
    (bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xFC, 0x28, 0xE3})) here;
    reset(&b);

    free(b.buf);
}

static void test_avx_broadcast(void) {
    Buf b = {0};

    // vbroadcastss %xmm0, %ymm2 => c4 e2 7d 18 d0
    vbroadcastss(&b, 2, 0);
    (bytes_eq(&b, 5, (uint8_t[]){0xC4, 0xE2, 0x7D, 0x18, 0xD0})) here;
    free(b.buf);
}

static void test_broadcast_imm32(void) {
    Buf b = {0};

    // Zero => vpxor ymm3,ymm3,ymm3 => c5 e5 ef db
    broadcast_imm32(&b, 3, 0);
    (bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xE5, 0xEF, 0xDB})) here;
    reset(&b);

    // All ones => vpcmpeqd ymm4,ymm4,ymm4 => c5 dd 76 e4
    broadcast_imm32(&b, 4, 0xFFFFFFFF);
    (bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xDD, 0x76, 0xE4})) here;
    free(b.buf);
}

static void test_vex_2byte_vs_3byte(void) {
    Buf b = {0};

    // Low regs => 2-byte VEX (C5)
    vaddps(&b, 4, 3, 2);
    (b.buf[0] == 0xC5) here;
    reset(&b);

    // High reg in rm => 3-byte VEX (C4)
    vpaddd(&b, 10, 9, 8);
    (b.buf[0] == 0xC4) here;
    reset(&b);

    // mm==2 (0F38) always needs 3-byte VEX
    vcvtph2ps(&b, 2, 3);
    (b.buf[0] == 0xC4) here;
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
    test_avx_shift();
    test_avx_convert();
    test_avx_extract_insert();
    test_avx_mov();
    test_avx_broadcast();
    test_broadcast_imm32();
    test_vex_2byte_vs_3byte();
    return 0;
}
