#pragma once
#include <stddef.h>
#include <stdint.h>

struct asm_x86 {
    uint8_t *byte;
    size_t   size, cap;
};

void emit1(struct asm_x86 *b, uint8_t v);
void emit4(struct asm_x86 *b, uint32_t v);

void vex(struct asm_x86 *b, int pp, int mm, int W, int L, int d, int v, int s, uint8_t op);
void vex_rrr(struct asm_x86 *b, int pp, int mm, int L, uint8_t op, int d, int v, int s);
void vex_mem(struct asm_x86 *b, int pp, int mm, int W, int L, int reg, int v, uint8_t op, int base,
             int index, int scale, int disp);

int  vex_rip(struct asm_x86 *b, int pp, int mm, int W, int L, int reg, int v, uint8_t op);
void vmov_load(struct asm_x86 *b, int L, int reg, int base, int index, int scale, int disp);
void vmov_store(struct asm_x86 *b, int L, int reg, int base, int index, int scale, int disp);

void rex_w(struct asm_x86 *b, int r, int breg);
void push_r(struct asm_x86 *b, int r);
void pop_r(struct asm_x86 *b, int r);
void add_ri(struct asm_x86 *b, int d, int32_t imm);
void cmp_rr(struct asm_x86 *b, int a, int c);
void cmp_ri(struct asm_x86 *b, int a, int32_t imm);
void shr_ri(struct asm_x86 *b, int r, uint8_t imm);
void shl_ri(struct asm_x86 *b, int r, uint8_t imm);
void mov_ri(struct asm_x86 *b, int d, int32_t imm);
void mov_rr(struct asm_x86 *b, int d, int s);
void mov_load(struct asm_x86 *b, int d, int base, int disp);
void mov_load32(struct asm_x86 *b, int d, int base, int disp);
int  jcc(struct asm_x86 *b, uint8_t cc);
int  jmp(struct asm_x86 *b);
void ret(struct asm_x86 *b);
void vzeroupper(struct asm_x86 *b);
void nop(struct asm_x86 *b);

enum {
    RAX = 0,
    RCX = 1,
    RDX = 2,
    RBX = 3,
    RSP = 4,
    RBP = 5,
    RSI = 6,
    RDI = 7,
    R8 = 8,
    R9 = 9,
    R10 = 10,
    R11 = 11,
    R12 = 12,
    R13 = 13,
    R14 = 14,
    R15 = 15
};

void vspill(struct asm_x86 *b, int reg, int slot);
void vfill(struct asm_x86 *b, int reg, int slot);

void vmovaps(struct asm_x86 *b, int d, int s);
void vpxor(struct asm_x86 *b, int L, int d, int v, int s);
void vbroadcastss(struct asm_x86 *b, int d, int s);

void broadcast_imm32(struct asm_x86 *b, int d, uint32_t v);

void vaddps(struct asm_x86 *b, int d, int v, int s);
void vsubps(struct asm_x86 *b, int d, int v, int s);
void vmulps(struct asm_x86 *b, int d, int v, int s);
void vdivps(struct asm_x86 *b, int d, int v, int s);
void vminps(struct asm_x86 *b, int d, int v, int s);
void vmaxps(struct asm_x86 *b, int d, int v, int s);
void vsqrtps(struct asm_x86 *b, int d, int s);
void vfmadd132ps(struct asm_x86 *b, int d, int v, int s);
void vfmadd213ps(struct asm_x86 *b, int d, int v, int s);
void vfmadd231ps(struct asm_x86 *b, int d, int v, int s);
void vfnmadd132ps(struct asm_x86 *b, int d, int v, int s);
void vfnmadd213ps(struct asm_x86 *b, int d, int v, int s);
void vfnmadd231ps(struct asm_x86 *b, int d, int v, int s);
void vcvtdq2ps(struct asm_x86 *b, int d, int s);
void vcvttps2dq(struct asm_x86 *b, int d, int s);
void vcvtps2dq(struct asm_x86 *b, int d, int s);
void vroundps(struct asm_x86 *b, int d, int s, uint8_t imm);
void vcmpps(struct asm_x86 *b, int d, int v, int s, uint8_t pred);

void vcvtph2ps(struct asm_x86 *b, int d, int s);
void vcvtps2ph(struct asm_x86 *b, int d, int s, uint8_t rnd);

void vpaddd(struct asm_x86 *b, int d, int v, int s);
void vpsubd(struct asm_x86 *b, int d, int v, int s);
void vpmulld(struct asm_x86 *b, int d, int v, int s);
void vpsllvd(struct asm_x86 *b, int d, int v, int s);
void vpsrlvd(struct asm_x86 *b, int d, int v, int s);
void vpsravd(struct asm_x86 *b, int d, int v, int s);
void vpslld_i(struct asm_x86 *b, int d, int s, uint8_t imm);
void vpsrld_i(struct asm_x86 *b, int d, int s, uint8_t imm);
void vpsrad_i(struct asm_x86 *b, int d, int s, uint8_t imm);

void vpand(struct asm_x86 *b, int L, int d, int v, int s);
void vpor(struct asm_x86 *b, int L, int d, int v, int s);
void vpxor_3(struct asm_x86 *b, int L, int d, int v, int s);
void vpblendvb(struct asm_x86 *b, int L, int d, int z, int y, int x);

void vpcmpeqd(struct asm_x86 *b, int d, int v, int s);
void vpcmpgtd(struct asm_x86 *b, int d, int v, int s);
void vpmovsxwd(struct asm_x86 *b, int d, int s);
void vpmovzxwd(struct asm_x86 *b, int d, int s);

void vpgatherdd(struct asm_x86 *b, int dst, int base, int idx, int scale, int mask);
void vpextrd(struct asm_x86 *b, int gpr, int xmm, uint8_t imm);
void vextracti128(struct asm_x86 *b, int d, int s, uint8_t imm);

void vmovd_from_gpr(struct asm_x86 *b, int xmm, int gpr);
void vmovd_to_gpr(struct asm_x86 *b, int gpr, int xmm);
void vmovd_load (struct asm_x86 *b, int reg, int base, int index, int scale, int disp);
void vmovd_store(struct asm_x86 *b, int reg, int base, int index, int scale, int disp);
void vmovq_load (struct asm_x86 *b, int reg, int base, int index, int scale, int disp);
void vmovq_store(struct asm_x86 *b, int reg, int base, int index, int scale, int disp);
void vpsrldq(struct asm_x86 *b, int d, int s, uint8_t imm);
