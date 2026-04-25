#include "../src/asm_x86.h"
#include "test.h"
#include <stdlib.h>
#include <string.h>

// All encodings verified against clang -target x86_64 + llvm-objdump.

static _Bool bytes_eq(struct asm_x86 *b, int n, uint8_t const exp[]) {
    return b->size == (size_t)n && memcmp(b->byte, exp, (size_t)n) == 0;
}

static void reset(struct asm_x86 *b) { b->size = 0; }

TEST(test_ret_vzeroupper_nop) {
    struct asm_x86 b = {0};
    ret(&b);
    bytes_eq(&b, 1, (uint8_t[]){0xC3}) here;
    reset(&b);

    vzeroupper(&b);
    bytes_eq(&b, 3, (uint8_t[]){0xC5, 0xF8, 0x77}) here;
    reset(&b);

    nop(&b);
    bytes_eq(&b, 1, (uint8_t[]){0x90}) here;
    free(b.byte);
}

TEST(test_gpr) {
    struct asm_x86 b = {0};

    // addq $8, %rdi => 48 83 c7 08
    add_ri(&b, RDI, 8);
    bytes_eq(&b, 4, (uint8_t[]){0x48, 0x83, 0xC7, 0x08}) here;
    reset(&b);

    // addq $256, %rsi => 48 81 c6 00 01 00 00
    add_ri(&b, RSI, 256);
    bytes_eq(&b, 7, (uint8_t[]){0x48, 0x81, 0xC6, 0x00, 0x01, 0x00, 0x00}) here;
    reset(&b);

    // cmpq %rcx, %rdx => 48 39 ca
    cmp_rr(&b, RDX, RCX);
    bytes_eq(&b, 3, (uint8_t[]){0x48, 0x39, 0xCA}) here;
    reset(&b);

    // cmpq $127, %r11 => 49 83 fb 7f
    cmp_ri(&b, R11, 127);
    bytes_eq(&b, 4, (uint8_t[]){0x49, 0x83, 0xFB, 0x7F}) here;
    reset(&b);

    // movq %rsp, %r12 => 49 89 e4
    mov_rr(&b, R12, RSP);
    bytes_eq(&b, 3, (uint8_t[]){0x49, 0x89, 0xE4}) here;
    reset(&b);

    // cmpq $256, %rax => 48 81 f8 00 01 00 00
    cmp_ri(&b, RAX, 256);
    bytes_eq(&b, 7, (uint8_t[]){0x48, 0x81, 0xF8, 0x00, 0x01, 0x00, 0x00}) here;
    reset(&b);

    // movq 256(%rdi), %rax => 48 8b 87 00 01 00 00
    mov_load(&b, RAX, RDI, 256);
    bytes_eq(&b, 7, (uint8_t[]){0x48, 0x8B, 0x87, 0x00, 0x01, 0x00, 0x00}) here;
    reset(&b);

    // movq $42, %rax => 48 c7 c0 2a 00 00 00
    mov_ri(&b, RAX, 42);
    bytes_eq(&b, 7, (uint8_t[]){0x48, 0xC7, 0xC0, 0x2A, 0x00, 0x00, 0x00}) here;
    reset(&b);

    // push %rax => 50
    push_r(&b, RAX);
    bytes_eq(&b, 1, (uint8_t[]){0x50}) here;
    reset(&b);

    // push %r12 => 41 54
    push_r(&b, R12);
    bytes_eq(&b, 2, (uint8_t[]){0x41, 0x54}) here;
    reset(&b);

    // pop %rax => 58
    pop_r(&b, RAX);
    bytes_eq(&b, 1, (uint8_t[]){0x58}) here;
    reset(&b);

    // pop %r12 => 41 5c
    pop_r(&b, R12);
    bytes_eq(&b, 2, (uint8_t[]){0x41, 0x5C}) here;
    reset(&b);

    // shrq $4, %rax => 48 c1 e8 04
    shr_ri(&b, RAX, 4);
    bytes_eq(&b, 4, (uint8_t[]){0x48, 0xC1, 0xE8, 0x04}) here;
    reset(&b);

    // shrq $1, %rax => 48 d1 e8
    shr_ri(&b, RAX, 1);
    bytes_eq(&b, 3, (uint8_t[]){0x48, 0xD1, 0xE8}) here;
    free(b.byte);
}

TEST(test_avx_f32) {
    struct asm_x86 b = {0};

    // vaddps %ymm4, %ymm3, %ymm2 => c5 e4 58 d4
    vaddps(&b, 2, 3, 4);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xE4, 0x58, 0xD4}) here;
    reset(&b);

    // vsubps %ymm5, %ymm6, %ymm7 => c5 cc 5c fd
    vsubps(&b, 7, 6, 5);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xCC, 0x5C, 0xFD}) here;
    reset(&b);

    // vmulps %ymm4, %ymm3, %ymm2 => c5 e4 59 d4
    vmulps(&b, 2, 3, 4);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xE4, 0x59, 0xD4}) here;
    reset(&b);

    // vminps %ymm4, %ymm3, %ymm2 => c5 e4 5d d4
    vminps(&b, 2, 3, 4);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xE4, 0x5D, 0xD4}) here;
    reset(&b);

    // vmaxps %ymm4, %ymm3, %ymm2 => c5 e4 5f d4
    vmaxps(&b, 2, 3, 4);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xE4, 0x5F, 0xD4}) here;
    reset(&b);

    // vcmpps %ymm4, %ymm3, %ymm2, $0 => c5 e4 c2 d4 00
    vcmpps(&b, 2, 3, 4, 0);
    bytes_eq(&b, 5, (uint8_t[]){0xC5, 0xE4, 0xC2, 0xD4, 0x00}) here;
    reset(&b);

    // vcvtdq2ps %ymm3, %ymm2 => c5 fc 5b d3
    vcvtdq2ps(&b, 2, 3);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xFC, 0x5B, 0xD3}) here;
    reset(&b);

    // vcvtps2dq %ymm3, %ymm2 => c5 fd 5b d3
    vcvtps2dq(&b, 2, 3);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xFD, 0x5B, 0xD3}) here;
    reset(&b);

    // vcvttps2dq %ymm3, %ymm2 => c5 fe 5b d3
    vcvttps2dq(&b, 2, 3);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xFE, 0x5B, 0xD3}) here;
    reset(&b);

    // vroundps %ymm3, %ymm2, $8 => c4 e3 7d 08 d3 08
    vroundps(&b, 2, 3, 8);
    bytes_eq(&b, 6, (uint8_t[]){0xC4, 0xE3, 0x7D, 0x08, 0xD3, 0x08}) here;
    free(b.byte);
}

TEST(test_avx_fma) {
    struct asm_x86 b = {0};

    // vfmadd132ps %ymm4, %ymm3, %ymm2 => c4 e2 65 98 d4
    vfmadd132ps(&b, 2, 3, 4);
    bytes_eq(&b, 5, (uint8_t[]){0xC4, 0xE2, 0x65, 0x98, 0xD4}) here;
    reset(&b);

    // vfmadd213ps %ymm4, %ymm3, %ymm2 => c4 e2 65 a8 d4
    vfmadd213ps(&b, 2, 3, 4);
    bytes_eq(&b, 5, (uint8_t[]){0xC4, 0xE2, 0x65, 0xA8, 0xD4}) here;
    reset(&b);

    // vfmadd231ps %ymm4, %ymm3, %ymm2 => c4 e2 65 b8 d4
    vfmadd231ps(&b, 2, 3, 4);
    bytes_eq(&b, 5, (uint8_t[]){0xC4, 0xE2, 0x65, 0xB8, 0xD4}) here;
    reset(&b);

    // vfnmadd132ps %ymm7, %ymm6, %ymm5 => c4 e2 4d 9c fd
    vfnmadd132ps(&b, 7, 6, 5);
    bytes_eq(&b, 5, (uint8_t[]){0xC4, 0xE2, 0x4D, 0x9C, 0xFD}) here;
    reset(&b);

    // vfnmadd213ps %ymm7, %ymm6, %ymm5 => c4 e2 4d ac fd
    vfnmadd213ps(&b, 7, 6, 5);
    bytes_eq(&b, 5, (uint8_t[]){0xC4, 0xE2, 0x4D, 0xAC, 0xFD}) here;
    reset(&b);

    // vfnmadd231ps %ymm4, %ymm3, %ymm2 => c4 e2 65 bc d4
    vfnmadd231ps(&b, 2, 3, 4);
    bytes_eq(&b, 5, (uint8_t[]){0xC4, 0xE2, 0x65, 0xBC, 0xD4}) here;
    free(b.byte);
}

TEST(test_avx_i32) {
    struct asm_x86 b = {0};

    // vpaddd %ymm8, %ymm9, %ymm10 => c4 41 35 fe d0
    vpaddd(&b, 10, 9, 8);
    bytes_eq(&b, 5, (uint8_t[]){0xC4, 0x41, 0x35, 0xFE, 0xD0}) here;
    reset(&b);

    // vpsubd %ymm4, %ymm3, %ymm2 => c5 e5 fa d4
    vpsubd(&b, 2, 3, 4);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xE5, 0xFA, 0xD4}) here;
    reset(&b);

    // vpmulld %ymm4, %ymm3, %ymm2 => c4 e2 65 40 d4
    vpmulld(&b, 2, 3, 4);
    bytes_eq(&b, 5, (uint8_t[]){0xC4, 0xE2, 0x65, 0x40, 0xD4}) here;
    reset(&b);

    // vpsllvd %ymm4, %ymm3, %ymm2 => c4 e2 65 47 d4
    vpsllvd(&b, 2, 3, 4);
    bytes_eq(&b, 5, (uint8_t[]){0xC4, 0xE2, 0x65, 0x47, 0xD4}) here;
    reset(&b);

    // vpsrlvd %ymm4, %ymm3, %ymm2 => c4 e2 65 45 d4
    vpsrlvd(&b, 2, 3, 4);
    bytes_eq(&b, 5, (uint8_t[]){0xC4, 0xE2, 0x65, 0x45, 0xD4}) here;
    reset(&b);

    // vpsravd %ymm4, %ymm3, %ymm2 => c4 e2 65 46 d4
    vpsravd(&b, 2, 3, 4);
    bytes_eq(&b, 5, (uint8_t[]){0xC4, 0xE2, 0x65, 0x46, 0xD4}) here;
    reset(&b);

    // vpsrad $4, %ymm3, %ymm2 => c5 ed 72 e3 04
    vpsrad_i(&b, 2, 3, 4);
    bytes_eq(&b, 5, (uint8_t[]){0xC5, 0xED, 0x72, 0xE3, 0x04}) here;
    reset(&b);

    free(b.byte);
}

TEST(test_avx_bitwise) {
    struct asm_x86 b = {0};

    // vpxor %ymm3, %ymm3, %ymm3 => c5 e5 ef db
    vpxor(&b, 1, 3, 3, 3);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xE5, 0xEF, 0xDB}) here;
    reset(&b);

    // vpand %ymm4, %ymm3, %ymm2 => c5 e5 db d4
    vpand(&b, 1, 2, 3, 4);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xE5, 0xDB, 0xD4}) here;
    reset(&b);

    // vpor %ymm4, %ymm3, %ymm2 => c5 e5 eb d4
    vpor(&b, 1, 2, 3, 4);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xE5, 0xEB, 0xD4}) here;
    reset(&b);

    // vpxor_3 %ymm4, %ymm3, %ymm2 => c5 e5 ef d4
    vpxor_3(&b, 1, 2, 3, 4);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xE5, 0xEF, 0xD4}) here;
    reset(&b);

    // vpblendvb %ymm5, %ymm4, %ymm3, %ymm2 => c4 e3 65 4c d4 50
    vpblendvb(&b, 1, 2, 3, 4, 5);
    bytes_eq(&b, 6, (uint8_t[]){0xC4, 0xE3, 0x65, 0x4C, 0xD4, 0x50}) here;
    free(b.byte);
}

TEST(test_avx_cmp) {
    struct asm_x86 b = {0};

    // vpcmpeqd %ymm4, %ymm4, %ymm4 => c5 dd 76 e4
    vpcmpeqd(&b, 4, 4, 4);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xDD, 0x76, 0xE4}) here;
    reset(&b);

    // vpcmpgtd %ymm4, %ymm3, %ymm2 => c5 e5 66 d4
    vpcmpgtd(&b, 2, 3, 4);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xE5, 0x66, 0xD4}) here;
    free(b.byte);
}

TEST(test_avx_shift) {
    struct asm_x86 b = {0};

    // vpsrld $4, %ymm3, %ymm2 => c5 ed 72 d3 04
    vpsrld_i(&b, 2, 3, 4);
    bytes_eq(&b, 5, (uint8_t[]){0xC5, 0xED, 0x72, 0xD3, 0x04}) here;
    reset(&b);

    // vpslld $8, %ymm5, %ymm4 => c5 dd 72 f5 08
    vpslld_i(&b, 4, 5, 8);
    bytes_eq(&b, 5, (uint8_t[]){0xC5, 0xDD, 0x72, 0xF5, 0x08}) here;
    free(b.byte);
}

TEST(test_avx_convert) {
    struct asm_x86 b = {0};

    // vcvtph2ps %xmm3, %ymm2 => c4 e2 7d 13 d3
    vcvtph2ps(&b, 2, 3);
    bytes_eq(&b, 5, (uint8_t[]){0xC4, 0xE2, 0x7D, 0x13, 0xD3}) here;
    reset(&b);

    // vcvtps2ph $4, %ymm5, %xmm4 => c4 e3 7d 1d ec 04
    vcvtps2ph(&b, 4, 5, 4);
    bytes_eq(&b, 6, (uint8_t[]){0xC4, 0xE3, 0x7D, 0x1D, 0xEC, 0x04}) here;
    reset(&b);

    // vpmovzxwd %xmm6, %ymm7 => c4 e2 7d 33 fe
    vpmovzxwd(&b, 7, 6);
    bytes_eq(&b, 5, (uint8_t[]){0xC4, 0xE2, 0x7D, 0x33, 0xFE}) here;
    reset(&b);

    // vpmovsxwd %xmm8, %ymm9 => c4 42 7d 23 c8
    vpmovsxwd(&b, 9, 8);
    bytes_eq(&b, 5, (uint8_t[]){0xC4, 0x42, 0x7D, 0x23, 0xC8}) here;
    reset(&b);

    // vpmovsxdq %xmm3, %ymm2 => c4 e2 7d 25 d3
    vpmovsxdq(&b, 2, 3);
    bytes_eq(&b, 5, (uint8_t[]){0xC4, 0xE2, 0x7D, 0x25, 0xD3}) here;
    free(b.byte);
}

TEST(test_avx_extract) {
    struct asm_x86 b = {0};

    // vextracti128 $1, %ymm7, %xmm6 => c4 e3 7d 39 fe 01
    vextracti128(&b, 6, 7, 1);
    bytes_eq(&b, 6, (uint8_t[]){0xC4, 0xE3, 0x7D, 0x39, 0xFE, 0x01}) here;
    reset(&b);

    // vpextrd $1, %xmm3, %eax => c4 e3 79 16 d8 01
    vpextrd(&b, RAX, 3, 1);
    bytes_eq(&b, 6, (uint8_t[]){0xC4, 0xE3, 0x79, 0x16, 0xD8, 0x01}) here;
    free(b.byte);
}

TEST(test_avx_mov) {
    struct asm_x86 b = {0};

    // vmovaps %ymm3, %ymm4 => c5 fc 28 e3
    vmovaps(&b, 4, 3);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xFC, 0x28, 0xE3}) here;
    reset(&b);

    // vmov_store ymm3, [rdi+rcx*4+0] => c4 e1 7e 7f 1c 8f
    vmov_store(&b, 1, 3, RDI, RCX, 4, 0);
    bytes_eq(&b, 6, (uint8_t[]){0xC4, 0xE1, 0x7E, 0x7F, 0x1C, 0x8F}) here;
    reset(&b);

    free(b.byte);
}

TEST(test_avx_broadcast) {
    struct asm_x86 b = {0};

    // vbroadcastss %xmm0, %ymm2 => c4 e2 7d 18 d0
    vbroadcastss(&b, 2, 0);
    bytes_eq(&b, 5, (uint8_t[]){0xC4, 0xE2, 0x7D, 0x18, 0xD0}) here;
    free(b.byte);
}

TEST(test_broadcast_imm32) {
    struct asm_x86 b = {0};

    // Zero => vpxor ymm3,ymm3,ymm3 => c5 e5 ef db
    broadcast_imm32(&b, 3, 0);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xE5, 0xEF, 0xDB}) here;
    reset(&b);

    // All ones => vpcmpeqd ymm4,ymm4,ymm4 => c5 dd 76 e4
    broadcast_imm32(&b, 4, 0xFFFFFFFF);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xDD, 0x76, 0xE4}) here;
    reset(&b);

    // Shift-right pattern: 0x7FFFFFFF => vpcmpeqd + vpsrld $1
    broadcast_imm32(&b, 5, 0x7FFFFFFF);
    b.size == 9 here;
    reset(&b);

    // Shift-left pattern: 0xFFFF0000 => vpcmpeqd + vpslld $16
    broadcast_imm32(&b, 6, 0xFFFF0000);
    b.size == 9 here;
    reset(&b);

    // General fallback: 0xDEADBEEF => mov eax + vmovd + vbroadcastss
    broadcast_imm32(&b, 7, 0xDEADBEEF);
    b.byte[0] == 0xB8 here;
    free(b.byte);
}

TEST(test_spill_fill) {
    struct asm_x86 b = {0};

    // vspill ymm3 to slot 0: vmovdqu [rsp], ymm3 => c5 fe 7f 1c 24
    vspill(&b, 3, 0);
    bytes_eq(&b, 5, (uint8_t[]){0xC5, 0xFE, 0x7F, 0x1C, 0x24}) here;
    reset(&b);

    // vspill ymm3 to slot 1 (disp=32): c5 fe 7f 5c 24 20
    vspill(&b, 3, 1);
    bytes_eq(&b, 6, (uint8_t[]){0xC5, 0xFE, 0x7F, 0x5C, 0x24, 0x20}) here;
    reset(&b);

    // vfill ymm3 from slot 0: vmovdqu ymm3, [rsp] => c5 fe 6f 1c 24
    vfill(&b, 3, 0);
    bytes_eq(&b, 5, (uint8_t[]){0xC5, 0xFE, 0x6F, 0x1C, 0x24}) here;
    reset(&b);

    // vfill ymm3 from slot 1 (disp=32): c5 fe 6f 5c 24 20
    vfill(&b, 3, 1);
    bytes_eq(&b, 6, (uint8_t[]){0xC5, 0xFE, 0x6F, 0x5C, 0x24, 0x20}) here;
    free(b.byte);
}

TEST(test_gather) {
    struct asm_x86 b = {0};

    // vpgatherdd ymm2, [rdi+rcx*4], ymm5 => c4 e2 55 90 14 8f
    vpgatherdd(&b, 2, RDI, RCX, 4, 5);
    bytes_eq(&b, 6, (uint8_t[]){0xC4, 0xE2, 0x55, 0x90, 0x14, 0x8F}) here;
    free(b.byte);
}

TEST(test_large_disp) {
    struct asm_x86 b = {0};

    // vmovdqu 256(%rdi,%r10,4), %ymm0
    vmov_load(&b, 1, 0, RDI, R10, 4, 256);
    b.byte[0] == 0xC4 here;
    b.size == 10 here;
    free(b.byte);
}

TEST(test_vex_2byte_vs_3byte) {
    struct asm_x86 b = {0};

    // Low regs => 2-byte VEX (C5)
    vaddps(&b, 4, 3, 2);
    b.byte[0] == 0xC5 here;
    reset(&b);

    // High reg in rm => 3-byte VEX (C4)
    vpaddd(&b, 10, 9, 8);
    b.byte[0] == 0xC4 here;
    reset(&b);

    // mm==2 (0F38) always needs 3-byte VEX
    vcvtph2ps(&b, 2, 3);
    b.byte[0] == 0xC4 here;
    free(b.byte);
}

TEST(test_avx_unpack_pack) {
    struct asm_x86 b = {0};

    // vpunpckldq xmm2, xmm3, xmm4 => c5 e1 62 d4
    vpunpckldq(&b, 2, 3, 4);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xE1, 0x62, 0xD4}) here;
    reset(&b);

    // vpunpcklqdq xmm2, xmm3, xmm4 => c5 e1 6c d4
    vpunpcklqdq(&b, 2, 3, 4);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xE1, 0x6C, 0xD4}) here;
    reset(&b);

    // vpunpcklwd xmm2, xmm3, xmm4 => c5 e1 61 d4
    vpunpcklwd(&b, 2, 3, 4);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xE1, 0x61, 0xD4}) here;
    reset(&b);

    // vpunpckhwd xmm2, xmm3, xmm4 => c5 e1 69 d4
    vpunpckhwd(&b, 2, 3, 4);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xE1, 0x69, 0xD4}) here;
    reset(&b);

    // vpunpckhdq xmm2, xmm3, xmm4 => c5 e1 6a d4
    vpunpckhdq(&b, 2, 3, 4);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xE1, 0x6A, 0xD4}) here;
    reset(&b);

    // vpackusdw xmm2, xmm3, xmm4 => c4 e2 61 2b d4 (mm=2 forces 3-byte VEX)
    vpackusdw(&b, 2, 3, 4);
    bytes_eq(&b, 5, (uint8_t[]){0xC4, 0xE2, 0x61, 0x2B, 0xD4}) here;
    reset(&b);

    // vpackssdw xmm2, xmm3, xmm4 => c5 e1 6b d4
    vpackssdw(&b, 2, 3, 4);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xE1, 0x6B, 0xD4}) here;
    reset(&b);

    // vpunpckldq xmm10, xmm9, xmm8 => c4 41 31 62 d0 (high regs force 3-byte VEX)
    vpunpckldq(&b, 10, 9, 8);
    bytes_eq(&b, 5, (uint8_t[]){0xC4, 0x41, 0x31, 0x62, 0xD0}) here;
    free(b.byte);
}

TEST(test_avx_minmax_andn) {
    struct asm_x86 b = {0};

    // vpminsd ymm2, ymm3, ymm4 => c4 e2 65 3b d4
    vpminsd(&b, 2, 3, 4);
    bytes_eq(&b, 5, (uint8_t[]){0xC4, 0xE2, 0x65, 0x3B, 0xD4}) here;
    reset(&b);

    // vpmaxsd ymm2, ymm3, ymm4 => c4 e2 65 3f d4
    vpmaxsd(&b, 2, 3, 4);
    bytes_eq(&b, 5, (uint8_t[]){0xC4, 0xE2, 0x65, 0x3F, 0xD4}) here;
    reset(&b);

    // vpandn ymm2, ymm3, ymm4 => c5 e5 df d4
    vpandn(&b, 2, 3, 4);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xE5, 0xDF, 0xD4}) here;
    reset(&b);

    // vpandn ymm10, ymm9, ymm8 => c4 41 35 df d0 (high regs force 3-byte VEX)
    vpandn(&b, 10, 9, 8);
    bytes_eq(&b, 5, (uint8_t[]){0xC4, 0x41, 0x35, 0xDF, 0xD0}) here;
    free(b.byte);
}

TEST(test_avx_rip) {
    struct asm_x86 b = {0};

    // vbroadcastss ymm2, [rip+0] => c4 e2 7d 18 15 00 00 00 00
    int pos = vbroadcastss_rip(&b, 2);
    pos == 5 here;
    bytes_eq(&b, 9, (uint8_t[]){0xC4, 0xE2, 0x7D, 0x18, 0x15, 0x00, 0x00, 0x00, 0x00}) here;
    reset(&b);

    // vpshufb ymm2, ymm3, [rip+0] => c4 e2 65 00 15 00 00 00 00
    pos = vpshufb_rip(&b, 2, 3);
    pos == 5 here;
    bytes_eq(&b, 9, (uint8_t[]){0xC4, 0xE2, 0x65, 0x00, 0x15, 0x00, 0x00, 0x00, 0x00}) here;
    reset(&b);

    // vpaddd ymm2, ymm2, [rip+0] => c4 e1 6d fe 15 00 00 00 00
    pos = vpaddd_rip(&b, 2, 2);
    pos == 5 here;
    bytes_eq(&b, 9, (uint8_t[]){0xC4, 0xE1, 0x6D, 0xFE, 0x15, 0x00, 0x00, 0x00, 0x00}) here;
    reset(&b);

    // vandps ymm2, ymm3, [rip+0] => c4 e1 64 54 15 00 00 00 00
    pos = vandps_rip(&b, 2, 3);
    pos == 5 here;
    bytes_eq(&b, 9, (uint8_t[]){0xC4, 0xE1, 0x64, 0x54, 0x15, 0x00, 0x00, 0x00, 0x00}) here;
    reset(&b);

    // High reg in dst: vbroadcastss ymm10, [rip+0]
    // R = ~10>>3 & 1 = 0. byte1 = (0<<7)|(1<<6)|(1<<5)|2 = 0x62.
    // byte2 unchanged: 0x7D. ModRM = ((10&7)<<3)|5 = 0x15.
    pos = vbroadcastss_rip(&b, 10);
    pos == 5 here;
    bytes_eq(&b, 9, (uint8_t[]){0xC4, 0x62, 0x7D, 0x18, 0x15, 0x00, 0x00, 0x00, 0x00}) here;
    free(b.byte);
}

TEST(test_vmovd_vmovq_vpsrldq) {
    struct asm_x86 b = {0};

    // VMOVD xmm0, eax: C5 F9 6E C0
    vmovd_from_gpr(&b, 0, RAX);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xF9, 0x6E, 0xC0}) here;
    reset(&b);

    // VMOVD eax, xmm0: C5 F9 7E C0
    vmovd_to_gpr(&b, RAX, 0);
    bytes_eq(&b, 4, (uint8_t[]){0xC5, 0xF9, 0x7E, 0xC0}) here;
    reset(&b);

    // VMOVD xmm1, [rax+rcx*4]: C4 E1 79 6E 0C 88
    vmovd_load(&b, 1, RAX, RCX, 4, 0);
    bytes_eq(&b, 6, (uint8_t[]){0xC4, 0xE1, 0x79, 0x6E, 0x0C, 0x88}) here;
    reset(&b);

    // VMOVD [rax+rcx*4], xmm1: C4 E1 79 7E 0C 88
    vmovd_store(&b, 1, RAX, RCX, 4, 0);
    bytes_eq(&b, 6, (uint8_t[]){0xC4, 0xE1, 0x79, 0x7E, 0x0C, 0x88}) here;
    reset(&b);

    // VMOVQ xmm2, [rax+rcx*8]: C4 E1 7A 7E 14 C8
    vmovq_load(&b, 2, RAX, RCX, 8, 0);
    bytes_eq(&b, 6, (uint8_t[]){0xC4, 0xE1, 0x7A, 0x7E, 0x14, 0xC8}) here;
    reset(&b);

    // VMOVQ [rax+rcx*8], xmm2: C4 E1 79 D6 14 C8
    vmovq_store(&b, 2, RAX, RCX, 8, 0);
    bytes_eq(&b, 6, (uint8_t[]){0xC4, 0xE1, 0x79, 0xD6, 0x14, 0xC8}) here;
    reset(&b);

    // VPSRLDQ xmm3, xmm4, 2: C5 E1 73 DC 02
    vpsrldq(&b, 3, 4, 2);
    bytes_eq(&b, 5, (uint8_t[]){0xC5, 0xE1, 0x73, 0xDC, 0x02}) here;
    reset(&b);

    free(b.byte);
}

TEST(test_vpinsrw) {
    struct asm_x86 b = {0};

    // vpinsrw $3, %eax, %xmm0, %xmm0  =>  C5 F9 C4 C0 03
    vpinsrw(&b, 0, 0, RAX, 3);
    bytes_eq(&b, 5, (uint8_t[]){0xC5, 0xF9, 0xC4, 0xC0, 0x03}) here;
    reset(&b);

    // vpinsrw $7, %r10d, %xmm5, %xmm5  =>  C4 C1 51 C4 EA 07
    vpinsrw(&b, 5, 5, R10, 7);
    bytes_eq(&b, 6, (uint8_t[]){0xC4, 0xC1, 0x51, 0xC4, 0xEA, 0x07}) here;
    reset(&b);

    free(b.byte);
}

TEST(test_vinserti128_vbroadcastss_mem_vcmpps_rip) {
    struct asm_x86 b = {0};

    // vinserti128 $1, %xmm3, %ymm2, %ymm1   =>  C4 E3 6D 38 CB 01
    vinserti128(&b, 1, 2, 3, 1);
    bytes_eq(&b, 6, (uint8_t[]){0xC4, 0xE3, 0x6D, 0x38, 0xCB, 0x01}) here;
    reset(&b);

    // vbroadcastss (%rdi), %ymm3   =>  C4 E2 7D 18 1F
    vbroadcastss_mem(&b, 3, RDI, 0);
    bytes_eq(&b, 5, (uint8_t[]){0xC4, 0xE2, 0x7D, 0x18, 0x1F}) here;
    reset(&b);

    // vbroadcastss 256(%r12), %ymm3   =>  C4 C2 7D 18 9C 24 00 01 00 00
    vbroadcastss_mem(&b, 3, R12, 256);
    bytes_eq(&b, 10, (uint8_t[]){0xC4, 0xC2, 0x7D, 0x18, 0x9C, 0x24,
                                 0x00, 0x01, 0x00, 0x00}) here;
    reset(&b);

    // vcmpltps (%rip), %ymm1, %ymm2 : vex_rip always uses the 3-byte VEX form
    // so we emit C4 E1 74 C2 15 <rip32> 01, one byte longer than llvm-mc's
    // 2-byte-VEX picking.  Same instruction; vex_rip returns the offset of the
    // rip-relative disp32 so the caller can patch it.
    int pos = vcmpps_rip(&b, 2, 1, /*pred=*/1);
    pos == 5 here;
    bytes_eq(&b, 10, (uint8_t[]){0xC4, 0xE1, 0x74, 0xC2, 0x15,
                                 0x00, 0x00, 0x00, 0x00, 0x01}) here;
    reset(&b);

    free(b.byte);
}

TEST(test_legacy_reg_reg) {
    struct asm_x86 b = {0};

    // addq %rcx, %rdx        =>  48 01 CA
    add_rr(&b, RDX, RCX);
    bytes_eq(&b, 3, (uint8_t[]){0x48, 0x01, 0xCA}) here;
    reset(&b);

    // addq %r10, %r11        =>  4D 01 D3
    add_rr(&b, R11, R10);
    bytes_eq(&b, 3, (uint8_t[]){0x4D, 0x01, 0xD3}) here;
    reset(&b);

    // imulq %r14, %rax       =>  49 0F AF C6
    imul_rr(&b, RAX, R14);
    bytes_eq(&b, 4, (uint8_t[]){0x49, 0x0F, 0xAF, 0xC6}) here;
    reset(&b);

    // imulq %rbx, %r9        =>  4C 0F AF CB
    imul_rr(&b, R9, RBX);
    bytes_eq(&b, 4, (uint8_t[]){0x4C, 0x0F, 0xAF, 0xCB}) here;
    reset(&b);

    // subq %r10, %r11        =>  4D 29 D3
    sub_rr(&b, R11, R10);
    bytes_eq(&b, 3, (uint8_t[]){0x4D, 0x29, 0xD3}) here;
    reset(&b);

    free(b.byte);
}

TEST(test_legacy_sib_mem) {
    struct asm_x86 b = {0};

    // movzbl (%rdi,%rsi), %eax        =>  0F B6 04 37
    movzx_byte_load(&b, RAX, RDI, RSI, 1, 0);
    bytes_eq(&b, 4, (uint8_t[]){0x0F, 0xB6, 0x04, 0x37}) here;
    reset(&b);

    // movzbl (%r11,%r10), %eax        =>  43 0F B6 04 13
    movzx_byte_load(&b, RAX, R11, R10, 1, 0);
    bytes_eq(&b, 5, (uint8_t[]){0x43, 0x0F, 0xB6, 0x04, 0x13}) here;
    reset(&b);

    // movzbl 4(%rdi,%rsi), %eax       =>  0F B6 44 37 04
    movzx_byte_load(&b, RAX, RDI, RSI, 1, 4);
    bytes_eq(&b, 5, (uint8_t[]){0x0F, 0xB6, 0x44, 0x37, 0x04}) here;
    reset(&b);

    // movzbl 0x1000(%r11,%r10), %r9d  =>  47 0F B6 8C 13 00 10 00 00
    movzx_byte_load(&b, R9, R11, R10, 1, 0x1000);
    bytes_eq(&b, 9, (uint8_t[]){0x47, 0x0F, 0xB6, 0x8C, 0x13,
                                0x00, 0x10, 0x00, 0x00}) here;
    reset(&b);

    // movzwl (%r11,%r10,2), %eax      =>  43 0F B7 04 53
    movzx_word_load(&b, RAX, R11, R10, 2, 0);
    bytes_eq(&b, 5, (uint8_t[]){0x43, 0x0F, 0xB7, 0x04, 0x53}) here;
    reset(&b);

    // movzwl (%rdi,%rsi,2), %eax      =>  0F B7 04 77
    movzx_word_load(&b, RAX, RDI, RSI, 2, 0);
    bytes_eq(&b, 4, (uint8_t[]){0x0F, 0xB7, 0x04, 0x77}) here;
    reset(&b);

    // movb %al, (%r11,%r10)           =>  43 88 04 13
    mov_byte_store(&b, RAX, R11, R10, 1, 0);
    bytes_eq(&b, 4, (uint8_t[]){0x43, 0x88, 0x04, 0x13}) here;
    reset(&b);

    // movb %al, (%rdi,%rsi)           =>  88 04 37
    mov_byte_store(&b, RAX, RDI, RSI, 1, 0);
    bytes_eq(&b, 3, (uint8_t[]){0x88, 0x04, 0x37}) here;
    reset(&b);

    // movb %sil, (%rdi,%rdx)          =>  40 88 34 17
    mov_byte_store(&b, RSI, RDI, RDX, 1, 0);
    bytes_eq(&b, 4, (uint8_t[]){0x40, 0x88, 0x34, 0x17}) here;
    reset(&b);

    // movw %ax, (%r11,%r10,2)         =>  66 43 89 04 53
    mov_word_store(&b, RAX, R11, R10, 2, 0);
    bytes_eq(&b, 5, (uint8_t[]){0x66, 0x43, 0x89, 0x04, 0x53}) here;
    reset(&b);

    // movw %ax, (%rdi,%rsi,2)         =>  66 89 04 77
    mov_word_store(&b, RAX, RDI, RSI, 2, 0);
    bytes_eq(&b, 4, (uint8_t[]){0x66, 0x89, 0x04, 0x77}) here;
    reset(&b);

    free(b.byte);
}

