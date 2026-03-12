#pragma once
#include <stdint.h>

typedef struct { uint8_t *buf; int len,cap; } Buf;

void emit1(Buf *b, uint8_t v);
void emit4(Buf *b, uint32_t v);

void vex(Buf *b, int pp, int mm, int W, int L, int d, int v, int s, uint8_t op);
void vex_rrr(Buf *b, int pp, int mm, int L, uint8_t op, int d, int v, int s);
void vex_rr(Buf *b, int pp, int mm, int L, uint8_t op, int d, int s);
void vex_shift(Buf *b, int pp, int mm, int L, uint8_t op, int ext, int d, int s, uint8_t imm);
void vex_mem(Buf *b, int pp, int mm, int W, int L, int reg, int v, uint8_t op,
             int base, int index, int scale, int disp);

void vmov_load(Buf *b, int L, int reg, int base, int index, int scale, int disp);
void vmov_store(Buf *b, int L, int reg, int base, int index, int scale, int disp);

void rex_w(Buf *b, int r, int b_);
void xor_rr(Buf *b, int d, int s);
void add_ri(Buf *b, int d, int32_t imm);
void sub_ri(Buf *b, int d, int32_t imm);
void cmp_rr(Buf *b, int a, int c);
void cmp_ri(Buf *b, int a, int32_t imm);
void mov_rr(Buf *b, int d, int s);
void mov_load(Buf *b, int d, int base, int disp);
int  jcc(Buf *b, uint8_t cc);
int  jmp(Buf *b);
void ret(Buf *b);
void vzeroupper(Buf *b);
void nop(Buf *b);

enum { RAX=0,RCX=1,RDX=2,RBX=3,RSP=4,RBP=5,RSI=6,RDI=7,
       R8=8,R9=9,R10=10,R11=11,R12=12,R13=13,R14=14,R15=15 };

void vspill(Buf *b, int reg, int slot);
void vfill(Buf *b, int reg, int slot);

void vmovaps(Buf *b, int d, int s);
void vmovaps_x(Buf *b, int d, int s);
void vpxor(Buf *b, int L, int d, int v, int s);
void vbroadcastss(Buf *b, int d, int s);

void broadcast_imm32(Buf *b, int d, uint32_t v);
void broadcast_imm16(Buf *b, int d, uint16_t v);
void broadcast_half_imm(Buf *b, int d, uint16_t bits);

void vaddps(Buf *b, int d, int v, int s);
void vsubps(Buf *b, int d, int v, int s);
void vmulps(Buf *b, int d, int v, int s);
void vdivps(Buf *b, int d, int v, int s);
void vminps(Buf *b, int d, int v, int s);
void vmaxps(Buf *b, int d, int v, int s);
void vsqrtps(Buf *b, int d, int s);
void vfmadd132ps(Buf *b, int d, int v, int s);
void vfmadd213ps(Buf *b, int d, int v, int s);
void vfmadd231ps(Buf *b, int d, int v, int s);
void vfnmadd132ps(Buf *b, int d, int v, int s);
void vfnmadd213ps(Buf *b, int d, int v, int s);
void vfnmadd231ps(Buf *b, int d, int v, int s);
void vcvtdq2ps(Buf *b, int d, int s);
void vcvttps2dq(Buf *b, int d, int s);
void vcmpps(Buf *b, int d, int v, int s, uint8_t pred);

void vcvtph2ps(Buf *b, int d, int s);
void vcvtps2ph(Buf *b, int d, int s, uint8_t rnd);

void vpaddd(Buf *b, int d, int v, int s);
void vpsubd(Buf *b, int d, int v, int s);
void vpmulld(Buf *b, int d, int v, int s);
void vpsllvd(Buf *b, int d, int v, int s);
void vpsrlvd(Buf *b, int d, int v, int s);
void vpsravd(Buf *b, int d, int v, int s);
void vpslld_i(Buf *b, int d, int s, uint8_t imm);
void vpsrld_i(Buf *b, int d, int s, uint8_t imm);
void vpsrad_i(Buf *b, int d, int s, uint8_t imm);

void vpand(Buf *b, int L, int d, int v, int s);
void vpor(Buf *b, int L, int d, int v, int s);
void vpxor_3(Buf *b, int L, int d, int v, int s);
void vpblendvb(Buf *b, int L, int d, int z, int y, int x);

void vpcmpeqd(Buf *b, int d, int v, int s);
void vpcmpgtd(Buf *b, int d, int v, int s);

void vpaddw(Buf *b, int d, int v, int s);
void vpsubw(Buf *b, int d, int v, int s);
void vpmullw(Buf *b, int d, int v, int s);
void vpsllw_i(Buf *b, int d, int s, uint8_t imm);
void vpsrlw_i(Buf *b, int d, int s, uint8_t imm);
void vpsraw_i(Buf *b, int d, int s, uint8_t imm);

void vpcmpeqw(Buf *b, int d, int v, int s);
void vpcmpgtw(Buf *b, int d, int v, int s);

void vpmovsxwd(Buf *b, int d, int s);
void vpmovzxwd(Buf *b, int d, int s);
void vpackuswb(Buf *b, int d, int v, int s);
void vpunpcklbw(Buf *b, int d, int v, int s);

void vextracti128(Buf *b, int d, int s, uint8_t imm);
void vinserti128(Buf *b, int d, int v, int s, uint8_t imm);
