#include "asm_x86.h"
#include <stdlib.h>
#include <string.h>

typedef struct Buf Buf;

// ---- byte buffer ----

void emit1(Buf *b, uint8_t v) {
    if (b->len == b->cap) {
        b->cap = b->cap ? 2 * b->cap : 4096;
        b->buf = realloc(b->buf, (size_t)b->cap);
    }
    b->buf[b->len++] = v;
}
void emit4(Buf *b, uint32_t v) {
    emit1(b, (uint8_t)v);
    emit1(b, (uint8_t)(v >> 8));
    emit1(b, (uint8_t)(v >> 16));
    emit1(b, (uint8_t)(v >> 24));
}

// ---- VEX encoding ----
// pp: 0=none,1=66,2=F3,3=F2  mm: 1=0F,2=0F38,3=0F3A  L: 0=128,1=256
// d=ModRM.reg  v=VEX.vvvv(NDS)  s=ModRM.rm

void vex(Buf *b, int pp, int mm, int W, int L, int d, int v, int s, uint8_t op) {
    int R = ~d >> 3, X = 1, B = ~s >> 3;
    if (mm == 1 && W == 0 && (B & 1) && (X & 1)) {
        emit1(b, 0xc5);
        emit1(b, (uint8_t)(((R & 1) << 7) | ((~v & 0xf) << 3) | (L << 2) | pp));
    } else {
        emit1(b, 0xc4);
        emit1(b, (uint8_t)(((R & 1) << 7) | ((X & 1) << 6) | ((B & 1) << 5) | mm));
        emit1(b, (uint8_t)((W << 7) | ((~v & 0xf) << 3) | (L << 2) | pp));
    }
    emit1(b, op);
    emit1(b, (uint8_t)(0xc0 | ((d & 7) << 3) | (s & 7)));
}

// 3-operand: op ymm/xmm d, v, s
void vex_rrr(Buf *b, int pp, int mm, int L, uint8_t op, int d, int v, int s) {
    vex(b, pp, mm, 0, L, d, v, s, op);
}
// 2-operand: op ymm/xmm d, s  (vvvv unused = 0 -> encoded as 1111)
static void vex_rr(Buf *b, int pp, int mm, int L, uint8_t op, int d, int s) {
    vex(b, pp, mm, 0, L, d, 0, s, op);
}
// shift-by-immediate: ModRM.reg=ext, VEX.vvvv=d(dest), ModRM.rm=s(src), +imm8
static void vex_shift(Buf *b, int pp, int mm, int L, uint8_t op, int ext, int d, int s,
                      uint8_t imm) {
    vex(b, pp, mm, 0, L, ext, d, s, op);
    emit1(b, imm);
}
// VEX memory operands: [base + index*scale + disp]
void vex_mem(Buf *b, int pp, int mm, int W, int L, int reg, int v, uint8_t op, int base,
             int index, int scale, int disp) {
    int R = ~reg >> 3, X = ~index >> 3, B = ~base >> 3;
    emit1(b, 0xc4);
    emit1(b, (uint8_t)(((R & 1) << 7) | ((X & 1) << 6) | ((B & 1) << 5) | mm));
    emit1(b, (uint8_t)((W << 7) | ((~v & 0xf) << 3) | (L << 2) | pp));
    emit1(b, op);

    int mod = 0;
    if (disp == 0 && (base & 7) != 5) {
        mod = 0;
    } else if (disp >= -128 && disp <= 127) {
        mod = 1;
    } else {
        mod = 2;
    }

    emit1(b, (uint8_t)((mod << 6) | ((reg & 7) << 3) | 4)); // SIB follows
    int ss = (scale == 1) ? 0 : (scale == 2) ? 1 : (scale == 4) ? 2 : 3;
    emit1(b, (uint8_t)((ss << 6) | ((index & 7) << 3) | (base & 7)));

    if (mod == 1) {
        emit1(b, (uint8_t)(int8_t)disp);
    } else if (mod == 2) {
        emit4(b, (uint32_t)disp);
    }
}

int vex_rip(Buf *b, int pp, int mm, int W, int L, int reg, int v, uint8_t op) {
    int R = ~reg >> 3;
    emit1(b, 0xc4);
    emit1(b, (uint8_t)(((R & 1) << 7) | (1 << 6) | (1 << 5) | mm));
    emit1(b, (uint8_t)((W << 7) | ((~v & 0xf) << 3) | (L << 2) | pp));
    emit1(b, op);
    emit1(b, (uint8_t)(((reg & 7) << 3) | 5));
    int pos = b->len;
    emit4(b, 0);
    return pos;
}

// Load: VMOVDQU ymm, [base+index*scale+disp]
void vmov_load(Buf *b, int L, int reg, int base, int index, int scale, int disp) {
    vex_mem(b, 2, 1, 0, L, reg, 0, 0x6f, base, index, scale, disp);
}
// Store: VMOVDQU [base+index*scale+disp], ymm
void vmov_store(Buf *b, int L, int reg, int base, int index, int scale, int disp) {
    vex_mem(b, 2, 1, 0, L, reg, 0, 0x7f, base, index, scale, disp);
}

// ---- GPR instructions ----
void rex_w(Buf *b, int r, int b_) {
    emit1(b, (uint8_t)(0x48 | ((r >> 3) << 2) | (b_ >> 3)));
}
void push_r(Buf *b, int r) {
    if (r >= 8) { emit1(b, 0x41); }
    emit1(b, (uint8_t)(0x50 | (r & 7)));
}
void pop_r(Buf *b, int r) {
    if (r >= 8) { emit1(b, 0x41); }
    emit1(b, (uint8_t)(0x58 | (r & 7)));
}
void add_ri(Buf *b, int d, int32_t imm) {
    rex_w(b, 0, d);
    if (imm >= -128 && imm <= 127) {
        emit1(b, 0x83);
        emit1(b, (uint8_t)(0xc0 | (d & 7)));
        emit1(b, (uint8_t)(int8_t)imm);
    } else {
        emit1(b, 0x81);
        emit1(b, (uint8_t)(0xc0 | (d & 7)));
        emit4(b, (uint32_t)imm);
    }
}
void cmp_rr(Buf *b, int a, int c) {
    rex_w(b, c, a);
    emit1(b, 0x39);
    emit1(b, (uint8_t)(0xc0 | ((c & 7) << 3) | (a & 7)));
}
void cmp_ri(Buf *b, int a, int32_t imm) {
    rex_w(b, 0, a);
    if (imm >= -128 && imm <= 127) {
        emit1(b, 0x83);
        emit1(b, (uint8_t)(0xc0 | (7 << 3) | (a & 7)));
        emit1(b, (uint8_t)(int8_t)imm);
    } else {
        emit1(b, 0x81);
        emit1(b, (uint8_t)(0xc0 | (7 << 3) | (a & 7)));
        emit4(b, (uint32_t)imm);
    }
}
void shr_ri(Buf *b, int r, uint8_t imm) {
    rex_w(b, 0, r);
    if (imm == 1) {
        emit1(b, 0xd1);
        emit1(b, (uint8_t)(0xc0 | (5 << 3) | (r & 7)));
    } else {
        emit1(b, 0xc1);
        emit1(b, (uint8_t)(0xc0 | (5 << 3) | (r & 7)));
        emit1(b, imm);
    }
}
void mov_ri(Buf *b, int d, int32_t imm) {
    rex_w(b, 0, d);
    emit1(b, 0xc7);
    emit1(b, (uint8_t)(0xc0 | (d & 7)));
    emit4(b, (uint32_t)imm);
}
void mov_rr(Buf *b, int d, int s) {
    rex_w(b, s, d);
    emit1(b, 0x89);
    emit1(b, (uint8_t)(0xc0 | ((s & 7) << 3) | (d & 7)));
}
void mov_load(Buf *b, int d, int base, int disp) {
    rex_w(b, d, base);
    emit1(b, 0x8b);
    if (disp == 0 && (base & 7) != 5) {
        emit1(b, (uint8_t)(((d & 7) << 3) | (base & 7)));
        if ((base & 7) == 4) { emit1(b, 0x24); }
    } else if (disp >= -128 && disp <= 127) {
        emit1(b, (uint8_t)(0x40 | ((d & 7) << 3) | (base & 7)));
        if ((base & 7) == 4) { emit1(b, 0x24); }
        emit1(b, (uint8_t)(int8_t)disp);
    } else {
        emit1(b, (uint8_t)(0x80 | ((d & 7) << 3) | (base & 7)));
        if ((base & 7) == 4) { emit1(b, 0x24); }
        emit4(b, (uint32_t)disp);
    }
}
int jcc(Buf *b, uint8_t cc) {
    emit1(b, 0x0f);
    emit1(b, (uint8_t)(0x80 | cc));
    int pos = b->len;
    emit4(b, 0);
    return pos;
}
int jmp(Buf *b) {
    emit1(b, 0xe9);
    int pos = b->len;
    emit4(b, 0);
    return pos;
}
void ret(Buf *b) { emit1(b, 0xc3); }
void vzeroupper(Buf *b) {
    emit1(b, 0xc5);
    emit1(b, 0xf8);
    emit1(b, 0x77);
}
void nop(Buf *b) { emit1(b, 0x90); }

// ---- Spill: VMOVDQU [RSP+slot*32], ymm or VMOVDQU ymm, [RSP+slot*32] ----
void vspill(Buf *b, int reg, int slot) {
    int disp = slot * 32;
    int R = ~reg >> 3;
    emit1(b, 0xc5);
    emit1(b, (uint8_t)(((R & 1) << 7) | 0x7e));
    emit1(b, 0x7f);
    if (disp == 0) {
        emit1(b, (uint8_t)(((reg & 7) << 3) | 4));
        emit1(b, 0x24);
    } else if (disp >= -128 && disp <= 127) {
        emit1(b, (uint8_t)(0x40 | ((reg & 7) << 3) | 4));
        emit1(b, 0x24);
        emit1(b, (uint8_t)(int8_t)disp);
    } else {
        emit1(b, (uint8_t)(0x80 | ((reg & 7) << 3) | 4));
        emit1(b, 0x24);
        emit4(b, (uint32_t)disp);
    }
}
void vfill(Buf *b, int reg, int slot) {
    int disp = slot * 32;
    int R = ~reg >> 3;
    emit1(b, 0xc5);
    emit1(b, (uint8_t)(((R & 1) << 7) | 0x7e));
    emit1(b, 0x6f);
    if (disp == 0) {
        emit1(b, (uint8_t)(((reg & 7) << 3) | 4));
        emit1(b, 0x24);
    } else if (disp >= -128 && disp <= 127) {
        emit1(b, (uint8_t)(0x40 | ((reg & 7) << 3) | 4));
        emit1(b, 0x24);
        emit1(b, (uint8_t)(int8_t)disp);
    } else {
        emit1(b, (uint8_t)(0x80 | ((reg & 7) << 3) | 4));
        emit1(b, 0x24);
        emit4(b, (uint32_t)disp);
    }
}

// ---- AVX2 instruction helpers ----

void vmovaps(Buf *b, int d, int s) { vex_rr(b, 0, 1, 1, 0x28, d, s); }
void vpxor(Buf *b, int L, int d, int v, int s) { vex_rrr(b, 1, 1, L, 0xef, d, v, s); }
void vbroadcastss(Buf *b, int d, int s) { vex_rrr(b, 1, 2, 1, 0x18, d, 0, s); }

void broadcast_imm32(Buf *b, int d, uint32_t v) {
    int shr = v ? __builtin_clz(v) : 0;
    int shl = v ? __builtin_ctz(v) : 0;

    if (v == 0) {
        vpxor(b, 1, d, d, d);
    } else if (v == 0xffffffffu) {
        vpcmpeqd(b, d, d, d);
    } else if (v == 0xffffffffu >> shr) {
        vpcmpeqd(b, d, d, d);
        vpsrld_i(b, d, d, (uint8_t)shr);
    } else if (v == (uint32_t)(UINT64_C(0xffffffff) << shl)) {
        vpcmpeqd(b, d, d, d);
        vpslld_i(b, d, d, (uint8_t)shl);
    } else {
        emit1(b, 0xb8);
        emit4(b, v);                         // MOV eax, v
        vex(b, 1, 1, 0, 0, d, 0, RAX, 0x6e); // VMOVD xmm_d, eax
        vbroadcastss(b, d, d);               // VBROADCASTSS ymm_d, xmm_d
    }
}

// ---- F32 arithmetic (YMM, L=1) ----
void vaddps(Buf *b, int d, int v, int s) { vex_rrr(b, 0, 1, 1, 0x58, d, v, s); }
void vsubps(Buf *b, int d, int v, int s) { vex_rrr(b, 0, 1, 1, 0x5c, d, v, s); }
void vmulps(Buf *b, int d, int v, int s) { vex_rrr(b, 0, 1, 1, 0x59, d, v, s); }
void vdivps(Buf *b, int d, int v, int s) { vex_rrr(b, 0, 1, 1, 0x5e, d, v, s); }
void vminps(Buf *b, int d, int v, int s) { vex_rrr(b, 0, 1, 1, 0x5d, d, v, s); }
void vmaxps(Buf *b, int d, int v, int s) { vex_rrr(b, 0, 1, 1, 0x5f, d, v, s); }
void vsqrtps(Buf *b, int d, int s)  { vex_rr(b, 0, 1, 1, 0x51, d, s); }
void vfmadd132ps(Buf *b, int d, int v, int s) { vex(b, 1, 2, 0, 1, d, v, s, 0x98); }
void vfmadd213ps(Buf *b, int d, int v, int s) { vex(b, 1, 2, 0, 1, d, v, s, 0xa8); }
void vfmadd231ps(Buf *b, int d, int v, int s) { vex(b, 1, 2, 0, 1, d, v, s, 0xb8); }
void vfnmadd132ps(Buf *b, int d, int v, int s) { vex(b, 1, 2, 0, 1, d, v, s, 0x9c); }
void vfnmadd213ps(Buf *b, int d, int v, int s) { vex(b, 1, 2, 0, 1, d, v, s, 0xac); }
void vfnmadd231ps(Buf *b, int d, int v, int s) { vex(b, 1, 2, 0, 1, d, v, s, 0xbc); }
void vcvtdq2ps(Buf *b, int d, int s) { vex_rr(b, 0, 1, 1, 0x5b, d, s); }
void vcvttps2dq(Buf *b, int d, int s) { vex_rr(b, 2, 1, 1, 0x5b, d, s); }
void vcvtps2dq(Buf *b, int d, int s) { vex_rr(b, 1, 1, 1, 0x5b, d, s); }
void vroundps(Buf *b, int d, int s, uint8_t imm) {
    vex_rrr(b, 1, 3, 1, 0x08, d, 0, s);
    emit1(b, imm);
}
void vcmpps(Buf *b, int d, int v, int s, uint8_t pred) {
    vex_rrr(b, 0, 1, 1, 0xc2, d, v, s);
    emit1(b, pred);
}

void vcvtph2ps(Buf *b, int d, int s) { vex_rr(b, 1, 2, 1, 0x13, d, s); }
void vcvtps2ph(Buf *b, int d, int s, uint8_t rnd) {
    vex_rrr(b, 1, 3, 1, 0x1d, s, 0, d);
    emit1(b, rnd);
}

// ---- I32 arithmetic (YMM, L=1) ----
void vpaddd(Buf *b, int d, int v, int s) { vex_rrr(b, 1, 1, 1, 0xfe, d, v, s); }
void vpsubd(Buf *b, int d, int v, int s) { vex_rrr(b, 1, 1, 1, 0xfa, d, v, s); }
void vpmulld(Buf *b, int d, int v, int s) { vex_rrr(b, 1, 2, 1, 0x40, d, v, s); }
void vpsllvd(Buf *b, int d, int v, int s) { vex_rrr(b, 1, 2, 1, 0x47, d, v, s); }
void vpsrlvd(Buf *b, int d, int v, int s) { vex_rrr(b, 1, 2, 1, 0x45, d, v, s); }
void vpsravd(Buf *b, int d, int v, int s) { vex_rrr(b, 1, 2, 1, 0x46, d, v, s); }
void vpslld_i(Buf *b, int d, int s, uint8_t imm) {
    vex_shift(b, 1, 1, 1, 0x72, 6, d, s, imm);
}
void vpsrld_i(Buf *b, int d, int s, uint8_t imm) {
    vex_shift(b, 1, 1, 1, 0x72, 2, d, s, imm);
}
void vpsrad_i(Buf *b, int d, int s, uint8_t imm) {
    vex_shift(b, 1, 1, 1, 0x72, 4, d, s, imm);
}

// ---- Bitwise (YMM) ----
void vpand(Buf *b, int L, int d, int v, int s) { vex_rrr(b, 1, 1, L, 0xdb, d, v, s); }
void vpor(Buf *b, int L, int d, int v, int s) { vex_rrr(b, 1, 1, L, 0xeb, d, v, s); }
void vpxor_3(Buf *b, int L, int d, int v, int s) { vex_rrr(b, 1, 1, L, 0xef, d, v, s); }
void vpblendvb(Buf *b, int L, int d, int z, int y, int x) {
    vex_rrr(b, 1, 3, L, 0x4c, d, z, y);
    emit1(b, (uint8_t)(x << 4));
}

// ---- I32 compare ----
void vpcmpeqd(Buf *b, int d, int v, int s) { vex_rrr(b, 1, 1, 1, 0x76, d, v, s); }
void vpcmpgtd(Buf *b, int d, int v, int s) { vex_rrr(b, 1, 1, 1, 0x66, d, v, s); }

// ---- I16 arithmetic (XMM, L=0) ----
// ---- Widening/narrowing ----
void vpmovsxwd(Buf *b, int d, int s) { vex_rr(b, 1, 2, 1, 0x23, d, s); }
void vpmovzxwd(Buf *b, int d, int s) { vex_rr(b, 1, 2, 1, 0x33, d, s); }

void vpgatherdd(Buf *b, int dst, int base, int idx, int scale, int mask) {
    vex_mem(b, 1, 2, 0, 1, dst, mask, 0x90, base, idx, scale, 0);
}
void vpextrd(Buf *b, int gpr, int xmm, uint8_t imm) {
    vex(b, 1, 3, 0, 0, xmm, 0, gpr, 0x16);
    emit1(b, imm);
}
void vextracti128(Buf *b, int d, int s, uint8_t imm) {
    vex_rrr(b, 1, 3, 1, 0x39, s, 0, d);
    emit1(b, imm);
}
