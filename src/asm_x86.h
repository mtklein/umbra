#pragma once
#include <stdint.h>

struct Buf {
    uint8_t *buf;
    int      len, cap;
};

void emit1(struct Buf *b, uint8_t v);
void emit4(struct Buf *b, uint32_t v);

void vex(struct Buf *b, int pp, int mm, int W, int L, int d, int v, int s, uint8_t op);
void vex_rrr(struct Buf *b, int pp, int mm, int L, uint8_t op, int d, int v, int s);
void vex_mem(struct Buf *b, int pp, int mm, int W, int L, int reg, int v, uint8_t op, int base,
             int index, int scale, int disp);

int  vex_rip(struct Buf *b, int pp, int mm, int W, int L, int reg, int v, uint8_t op);
void vmov_load(struct Buf *b, int L, int reg, int base, int index, int scale, int disp);
void vmov_store(struct Buf *b, int L, int reg, int base, int index, int scale, int disp);

void rex_w(struct Buf *b, int r, int b_);
void push_r(struct Buf *b, int r);
void pop_r(struct Buf *b, int r);
void add_ri(struct Buf *b, int d, int32_t imm);
void cmp_rr(struct Buf *b, int a, int c);
void cmp_ri(struct Buf *b, int a, int32_t imm);
void shr_ri(struct Buf *b, int r, uint8_t imm);
void mov_ri(struct Buf *b, int d, int32_t imm);
void mov_rr(struct Buf *b, int d, int s);
void mov_load(struct Buf *b, int d, int base, int disp);
int  jcc(struct Buf *b, uint8_t cc);
int  jmp(struct Buf *b);
void ret(struct Buf *b);
void vzeroupper(struct Buf *b);
void nop(struct Buf *b);

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

void vspill(struct Buf *b, int reg, int slot);
void vfill(struct Buf *b, int reg, int slot);

void vmovaps(struct Buf *b, int d, int s);
void vpxor(struct Buf *b, int L, int d, int v, int s);
void vbroadcastss(struct Buf *b, int d, int s);

void broadcast_imm32(struct Buf *b, int d, uint32_t v);

void vaddps(struct Buf *b, int d, int v, int s);
void vsubps(struct Buf *b, int d, int v, int s);
void vmulps(struct Buf *b, int d, int v, int s);
void vdivps(struct Buf *b, int d, int v, int s);
void vminps(struct Buf *b, int d, int v, int s);
void vmaxps(struct Buf *b, int d, int v, int s);
void vsqrtps(struct Buf *b, int d, int s);
void vfmadd132ps(struct Buf *b, int d, int v, int s);
void vfmadd213ps(struct Buf *b, int d, int v, int s);
void vfmadd231ps(struct Buf *b, int d, int v, int s);
void vfnmadd132ps(struct Buf *b, int d, int v, int s);
void vfnmadd213ps(struct Buf *b, int d, int v, int s);
void vfnmadd231ps(struct Buf *b, int d, int v, int s);
void vcvtdq2ps(struct Buf *b, int d, int s);
void vcvttps2dq(struct Buf *b, int d, int s);
void vcvtps2dq(struct Buf *b, int d, int s);
void vroundps(struct Buf *b, int d, int s, uint8_t imm);
void vcmpps(struct Buf *b, int d, int v, int s, uint8_t pred);

void vcvtph2ps(struct Buf *b, int d, int s);
void vcvtps2ph(struct Buf *b, int d, int s, uint8_t rnd);

void vpaddd(struct Buf *b, int d, int v, int s);
void vpsubd(struct Buf *b, int d, int v, int s);
void vpmulld(struct Buf *b, int d, int v, int s);
void vpsllvd(struct Buf *b, int d, int v, int s);
void vpsrlvd(struct Buf *b, int d, int v, int s);
void vpsravd(struct Buf *b, int d, int v, int s);
void vpslld_i(struct Buf *b, int d, int s, uint8_t imm);
void vpsrld_i(struct Buf *b, int d, int s, uint8_t imm);
void vpsrad_i(struct Buf *b, int d, int s, uint8_t imm);

void vpand(struct Buf *b, int L, int d, int v, int s);
void vpor(struct Buf *b, int L, int d, int v, int s);
void vpxor_3(struct Buf *b, int L, int d, int v, int s);
void vpblendvb(struct Buf *b, int L, int d, int z, int y, int x);

void vpcmpeqd(struct Buf *b, int d, int v, int s);
void vpcmpgtd(struct Buf *b, int d, int v, int s);
void vpmovsxwd(struct Buf *b, int d, int s);
void vpmovzxwd(struct Buf *b, int d, int s);

void vpgatherdd(struct Buf *b, int dst, int base, int idx, int scale, int mask);
void vpextrd(struct Buf *b, int gpr, int xmm, uint8_t imm);
void vextracti128(struct Buf *b, int d, int s, uint8_t imm);
