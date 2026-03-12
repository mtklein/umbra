#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// ---- byte buffer ----
typedef struct { uint8_t *buf; int len,cap; } Buf;

static void emit1(Buf *b, uint8_t v) {
    if (b->len == b->cap) {
        b->cap = b->cap ? 2*b->cap : 4096;
        b->buf = realloc(b->buf, (size_t)b->cap);
    }
    b->buf[b->len++] = v;
}
static void emit4(Buf *b, uint32_t v) {
    emit1(b,(uint8_t)v); emit1(b,(uint8_t)(v>>8)); emit1(b,(uint8_t)(v>>16)); emit1(b,(uint8_t)(v>>24));
}

// ---- VEX encoding ----
// pp: 0=none,1=66,2=F3,3=F2  mm: 1=0F,2=0F38,3=0F3A  L: 0=128,1=256
// d=ModRM.reg  v=VEX.vvvv(NDS)  s=ModRM.rm

static void vex(Buf *b, int pp, int mm, int W, int L, int d, int v, int s, uint8_t op) {
    int R = ~d >> 3, X = 1, B = ~s >> 3;
    if (mm == 1 && W == 0 && (B & 1) && (X & 1)) {
        emit1(b, 0xC5);
        emit1(b, (uint8_t)(((R&1)<<7) | ((~v&0xf)<<3) | (L<<2) | pp));
    } else {
        emit1(b, 0xC4);
        emit1(b, (uint8_t)(((R&1)<<7) | ((X&1)<<6) | ((B&1)<<5) | mm));
        emit1(b, (uint8_t)((W<<7) | ((~v&0xf)<<3) | (L<<2) | pp));
    }
    emit1(b, op);
    emit1(b, (uint8_t)(0xC0 | ((d&7)<<3) | (s&7)));
}

// 3-operand: op ymm/xmm d, v, s
static void vex_rrr(Buf *b, int pp, int mm, int L, uint8_t op, int d, int v, int s) {
    vex(b,pp,mm,0,L,d,v,s,op);
}
// 2-operand: op ymm/xmm d, s  (vvvv unused = 0 -> encoded as 1111)
static void vex_rr(Buf *b, int pp, int mm, int L, uint8_t op, int d, int s) {
    vex(b,pp,mm,0,L,d,0,s,op);
}
// shift-by-immediate: ModRM.reg=ext, VEX.vvvv=d(dest), ModRM.rm=s(src), +imm8
static void vex_shift(Buf *b, int pp, int mm, int L, uint8_t op, int ext, int d, int s, uint8_t imm) {
    vex(b,pp,mm,0,L,ext,d,s,op);
    emit1(b, imm);
}
// VEX memory operands: [base + index*scale + disp]
// ModRM + optional SIB + optional disp
static void vex_mem(Buf *b, int pp, int mm, int W, int L, int reg, int v, uint8_t op,
                    int base, int index, int scale, int disp) {
    // reg goes in ModRM.reg, base in SIB.base or ModRM.rm
    int R = ~reg >> 3, X = ~index >> 3, B = ~base >> 3;
    emit1(b, 0xC4);
    emit1(b, (uint8_t)(((R&1)<<7) | ((X&1)<<6) | ((B&1)<<5) | mm));
    emit1(b, (uint8_t)((W<<7) | ((~v&0xf)<<3) | (L<<2) | pp));
    emit1(b, op);

    int mod = 0;
    if (disp == 0 && (base&7) != 5) mod = 0;
    else if (disp >= -128 && disp <= 127) mod = 1;
    else mod = 2;

    emit1(b, (uint8_t)((mod<<6) | ((reg&7)<<3) | 4));  // SIB follows
    int ss = (scale==1)?0 : (scale==2)?1 : (scale==4)?2 : 3;
    emit1(b, (uint8_t)((ss<<6) | ((index&7)<<3) | (base&7)));

    if (mod == 1) emit1(b, (uint8_t)(int8_t)disp);
    else if (mod == 2) emit4(b, (uint32_t)disp);
}

// Load: VMOVDQU ymm, [base+index*scale+disp] or VMOVUPS
static void vmov_load(Buf *b, int L, int reg, int base, int index, int scale, int disp) {
    // VMOVDQU: VEX.F3.0F 6F /r
    vex_mem(b, 2, 1, 0, L, reg, 0, 0x6F, base, index, scale, disp);
}
// Store: VMOVDQU [base+index*scale+disp], ymm
static void vmov_store(Buf *b, int L, int reg, int base, int index, int scale, int disp) {
    // VMOVDQU: VEX.F3.0F 7F /r  — reg is source, memory is dest
    vex_mem(b, 2, 1, 0, L, reg, 0, 0x7F, base, index, scale, disp);
}

// ---- GPR instructions ----
// REX prefix helpers
static void rex_w(Buf *b, int r, int b_) {
    emit1(b, (uint8_t)(0x48 | ((r>>3)<<2) | (b_>>3)));
}
// XOR r32,r32 (zero register)
static void xor_rr(Buf *b, int d, int s) {
    if (d >= 8 || s >= 8) emit1(b, (uint8_t)(0x40 | ((d>>3)<<2) | (s>>3)));
    emit1(b, 0x31);
    emit1(b, (uint8_t)(0xC0 | ((s&7)<<3) | (d&7)));
}
// ADD r64, imm32
static void add_ri(Buf *b, int d, int32_t imm) {
    rex_w(b, 0, d);
    if (imm >= -128 && imm <= 127) {
        emit1(b, 0x83);
        emit1(b, (uint8_t)(0xC0 | (d&7)));
        emit1(b, (uint8_t)(int8_t)imm);
    } else {
        emit1(b, 0x81);
        emit1(b, (uint8_t)(0xC0 | (d&7)));
        emit4(b, (uint32_t)imm);
    }
}
// SUB r64, imm32
static void sub_ri(Buf *b, int d, int32_t imm) {
    rex_w(b, 0, d);
    if (imm >= -128 && imm <= 127) {
        emit1(b, 0x83);
        emit1(b, (uint8_t)(0xC0 | (5<<3) | (d&7)));
        emit1(b, (uint8_t)(int8_t)imm);
    } else {
        emit1(b, 0x81);
        emit1(b, (uint8_t)(0xC0 | (5<<3) | (d&7)));
        emit4(b, (uint32_t)imm);
    }
}
// CMP r64, r64
static void cmp_rr(Buf *b, int a, int c) {
    rex_w(b, c, a);
    emit1(b, 0x39);
    emit1(b, (uint8_t)(0xC0 | ((c&7)<<3) | (a&7)));
}
// CMP r64, imm32
static void cmp_ri(Buf *b, int a, int32_t imm) {
    rex_w(b, 0, a);
    if (imm >= -128 && imm <= 127) {
        emit1(b, 0x83);
        emit1(b, (uint8_t)(0xC0 | (7<<3) | (a&7)));
        emit1(b, (uint8_t)(int8_t)imm);
    } else {
        emit1(b, 0x81);
        emit1(b, (uint8_t)(0xC0 | (7<<3) | (a&7)));
        emit4(b, (uint32_t)imm);
    }
}
// MOV r64, r64
static void mov_rr(Buf *b, int d, int s) {
    rex_w(b, s, d);
    emit1(b, 0x89);
    emit1(b, (uint8_t)(0xC0 | ((s&7)<<3) | (d&7)));
}
// MOV r64, [r64+disp]
static void mov_load(Buf *b, int d, int base, int disp) {
    rex_w(b, d, base);
    emit1(b, 0x8B);
    if (disp == 0 && (base&7) != 5) {
        emit1(b, (uint8_t)(((d&7)<<3) | (base&7)));
        if ((base&7) == 4) emit1(b, 0x24);
    } else if (disp >= -128 && disp <= 127) {
        emit1(b, (uint8_t)(0x40 | ((d&7)<<3) | (base&7)));
        if ((base&7) == 4) emit1(b, 0x24);
        emit1(b, (uint8_t)(int8_t)disp);
    } else {
        emit1(b, (uint8_t)(0x80 | ((d&7)<<3) | (base&7)));
        if ((base&7) == 4) emit1(b, 0x24);
        emit4(b, (uint32_t)disp);
    }
}
// JCC rel32 — returns patch position
static int jcc(Buf *b, uint8_t cc) {
    emit1(b, 0x0F);
    emit1(b, (uint8_t)(0x80 | cc));
    int pos = b->len;
    emit4(b, 0);
    return pos;
}
// JMP rel32
static int jmp(Buf *b) {
    emit1(b, 0xE9);
    int pos = b->len;
    emit4(b, 0);
    return pos;
}
// RET
static void ret(Buf *b) { emit1(b, 0xC3); }
// VZEROUPPER
static void vzeroupper(Buf *b) { emit1(b,0xC5); emit1(b,0xF8); emit1(b,0x77); }
// NOP (for alignment)
static void nop(Buf *b) { emit1(b, 0x90); }

// ---- x86 GPR numbering ----
enum { RAX=0,RCX=1,RDX=2,RBX=3,RSP=4,RBP=5,RSI=6,RDI=7,
       R8=8,R9=9,R10=10,R11=11,R12=12,R13=13,R14=14,R15=15 };

// ---- Spill: VMOVDQU [RSP+slot*32], ymm or VMOVDQU ymm, [RSP+slot*32] ----
static void vspill(Buf *b, int reg, int slot) {
    // VMOVDQU [RSP + slot*32], ymm
    int disp = slot * 32;
    int R = ~reg >> 3;
    emit1(b, 0xC5);
    emit1(b, (uint8_t)(((R&1)<<7) | 0x7E));  // VEX.256.F3.0F, vvvv=1111
    emit1(b, 0x7F);  // VMOVDQU store
    if (disp == 0) {
        emit1(b, (uint8_t)(((reg&7)<<3) | 4));  // ModRM: mod=0, rm=4 (SIB follows)
        emit1(b, 0x24);  // SIB: scale=1, index=RSP(none), base=RSP
    } else if (disp >= -128 && disp <= 127) {
        emit1(b, (uint8_t)(0x40 | ((reg&7)<<3) | 4));
        emit1(b, 0x24);
        emit1(b, (uint8_t)(int8_t)disp);
    } else {
        emit1(b, (uint8_t)(0x80 | ((reg&7)<<3) | 4));
        emit1(b, 0x24);
        emit4(b, (uint32_t)disp);
    }
}
static void vfill(Buf *b, int reg, int slot) {
    // VMOVDQU ymm, [RSP + slot*32]
    int disp = slot * 32;
    int R = ~reg >> 3;
    emit1(b, 0xC5);
    emit1(b, (uint8_t)(((R&1)<<7) | 0x7E));
    emit1(b, 0x6F);  // VMOVDQU load
    if (disp == 0) {
        emit1(b, (uint8_t)(((reg&7)<<3) | 4));
        emit1(b, 0x24);
    } else if (disp >= -128 && disp <= 127) {
        emit1(b, (uint8_t)(0x40 | ((reg&7)<<3) | 4));
        emit1(b, 0x24);
        emit1(b, (uint8_t)(int8_t)disp);
    } else {
        emit1(b, (uint8_t)(0x80 | ((reg&7)<<3) | 4));
        emit1(b, 0x24);
        emit4(b, (uint32_t)disp);
    }
}

// ---- AVX2 instruction helpers ----

// VMOVAPS ymm,ymm (for reg moves)
static void vmovaps(Buf *b, int d, int s) { vex_rr(b,0,1,1,0x28,d,s); }
// VMOVAPS xmm,xmm
static void vmovaps_x(Buf *b, int d, int s) { vex_rr(b,0,1,0,0x28,d,s); }

// VPXOR ymm,ymm,ymm (zero or xor)
static void vpxor(Buf *b, int L, int d, int v, int s) { vex_rrr(b,1,1,L,0xEF,d,v,s); }

// VBROADCASTSS ymm, xmm  — VEX.256.66.0F38 18 /r (from reg: VEX.W=0)
static void vbroadcastss(Buf *b, int d, int s) { vex_rrr(b,1,2,1,0x18,d,0,s); }

// Forward declarations for idioms used in broadcast helpers below.
static void vpcmpeqd(Buf*, int, int, int);
static void vpslld_i(Buf*, int, int, uint8_t);
static void vpsrld_i(Buf*, int, int, uint8_t);
static void vpcmpeqw(Buf*, int, int, int);
static void vpsllw_i(Buf*, int, int, uint8_t);
static void vpsrlw_i(Buf*, int, int, uint8_t);

// Load immediate into xmm0 low dword then broadcast to ymm.
// Uses instruction idioms for common patterns to avoid the mov+vmovd+broadcast sequence.
static void broadcast_imm32(Buf *b, int d, uint32_t v) {
    int shr = v ? __builtin_clz(v) : 0;  // ~0u >> shr == v when low bits contiguous
    int shl = v ? __builtin_ctz(v) : 0;  // ~0u << shl == v when high bits contiguous

    if      (v == 0)           { vpxor(b,1,d,d,d); }
    else if (v == 0xFFFFFFFFu) { vpcmpeqd(b,d,d,d); }
    else if (v == 0xFFFFFFFFu >> shr) { vpcmpeqd(b,d,d,d); vpsrld_i(b,d,d,(uint8_t)shr); }
    else if (v == 0xFFFFFFFFu << shl) { vpcmpeqd(b,d,d,d); vpslld_i(b,d,d,(uint8_t)shl); }
    else {
        emit1(b, 0xB8); emit4(b, v);
        vex(b, 1, 1, 0, 0, 0, 0, RAX, 0x6E);
        vbroadcastss(b, d, 0);
    }
}

// Broadcast 16-bit immediate to all 8 words of xmm.
static void broadcast_imm16(Buf *b, int d, uint16_t v) {
    int shr = v ? __builtin_clz(v) - 16 : 0;  // (uint16_t)~0 >> shr == v
    int shl = v ? __builtin_ctz(v) : 0;        // (uint16_t)~0 << shl == v

    if      (v == 0)      { vpxor(b,0,d,d,d); }
    else if (v == 0xFFFFu) { vpcmpeqw(b,d,d,d); }
    else if (v == (uint16_t)(0xFFFFu >> shr)) { vpcmpeqw(b,d,d,d); vpsrlw_i(b,d,d,(uint8_t)shr); }
    else if (v == (uint16_t)(0xFFFFu << shl)) { vpcmpeqw(b,d,d,d); vpsllw_i(b,d,d,(uint8_t)shl); }
    else {
        emit1(b, 0xB8); emit4(b, (uint32_t)v);
        vex(b, 1, 1, 0, 0, 0, 0, RAX, 0x6E);
        vex_rr(b, 1, 2, 0, 0x79, d, 0);
    }
}

// Convert fp16 bits to f32 constant at JIT time, broadcast as f32
static void broadcast_half_imm(Buf *b, int d, uint16_t bits) {
    // Load fp16 bits into xmm0, convert to f32, broadcast
    if (bits == 0) {
        vpxor(b, 1, d, d, d);
    } else {
        emit1(b, 0xB8); emit4(b, (uint32_t)bits);
        // VMOVD xmm0, eax
        vex(b, 1, 1, 0, 0, 0, 0, RAX, 0x6E);
        // VCVTPH2PS xmm0, xmm0 — VEX.128.66.0F38 13
        vex_rr(b, 1, 2, 0, 0x13, 0, 0);
        // VBROADCASTSS ymm_d, xmm0
        vbroadcastss(b, d, 0);
    }
}

// ---- F32 arithmetic (YMM, L=1) ----
static void vaddps(Buf *b, int d, int v, int s) { vex_rrr(b,0,1,1,0x58,d,v,s); }
static void vsubps(Buf *b, int d, int v, int s) { vex_rrr(b,0,1,1,0x5C,d,v,s); }
static void vmulps(Buf *b, int d, int v, int s) { vex_rrr(b,0,1,1,0x59,d,v,s); }
static void vdivps(Buf *b, int d, int v, int s) { vex_rrr(b,0,1,1,0x5E,d,v,s); }
static void vminps(Buf *b, int d, int v, int s) { vex_rrr(b,0,1,1,0x5D,d,v,s); }
static void vmaxps(Buf *b, int d, int v, int s) { vex_rrr(b,0,1,1,0x5F,d,v,s); }
static void vsqrtps(Buf *b, int d, int s)       { vex_rr (b,0,1,1,0x51,d,s); }
// VFMADD{132,213,231}PS: fused multiply-add variants
// 132: d = d*s + v   213: d = v*d + s   231: d = v*s + d
static void vfmadd132ps(Buf *b, int d, int v, int s) { vex(b,1,2,0,1,d,v,s,0x98); }
static void vfmadd213ps(Buf *b, int d, int v, int s) { vex(b,1,2,0,1,d,v,s,0xA8); }
static void vfmadd231ps(Buf *b, int d, int v, int s) { vex(b,1,2,0,1,d,v,s,0xB8); }
// VFNMADD{132,213,231}PS: fused negate-multiply-add variants
// 132: d = -(d*s) + v   213: d = -(v*d) + s   231: d = -(v*s) + d
static void vfnmadd132ps(Buf *b, int d, int v, int s) { vex(b,1,2,0,1,d,v,s,0x9C); }
static void vfnmadd213ps(Buf *b, int d, int v, int s) { vex(b,1,2,0,1,d,v,s,0xAC); }
static void vfnmadd231ps(Buf *b, int d, int v, int s) { vex(b,1,2,0,1,d,v,s,0xBC); }
// VCVTDQ2PS ymm,ymm — VEX.256.0F 5B
static void vcvtdq2ps(Buf *b, int d, int s) { vex_rr(b,0,1,1,0x5B,d,s); }
// VCVTTPS2DQ ymm,ymm — VEX.256.F3.0F 5B
static void vcvttps2dq(Buf *b, int d, int s) { vex_rr(b,2,1,1,0x5B,d,s); }
// VCMPPS ymm,ymm,ymm,imm8 — VEX.256.0F C2
static void vcmpps(Buf *b, int d, int v, int s, uint8_t pred) {
    vex_rrr(b,0,1,1,0xC2,d,v,s); emit1(b, pred);
}

// VCVTPH2PS ymm,xmm — VEX.256.66.0F38 13
static void vcvtph2ps(Buf *b, int d, int s) { vex_rr(b,1,2,1,0x13,d,s); }
// VCVTPS2PH xmm,ymm,imm8 — VEX.256.66.0F3A 1D (note: d=xmm dest is in ModRM.rm, s=ymm src in ModRM.reg)
static void vcvtps2ph(Buf *b, int d, int s, uint8_t rnd) {
    // This instruction is unusual: the xmm dest is rm, ymm src is reg
    vex_rrr(b,1,3,1,0x1D,s,0,d); emit1(b, rnd);
}

// ---- I32 arithmetic (YMM, L=1) ----
static void vpaddd(Buf *b, int d, int v, int s)  { vex_rrr(b,1,1,1,0xFE,d,v,s); }
static void vpsubd(Buf *b, int d, int v, int s)  { vex_rrr(b,1,1,1,0xFA,d,v,s); }
// VPMULLD: VEX.256.66.0F38 40
static void vpmulld(Buf *b, int d, int v, int s) { vex_rrr(b,1,2,1,0x40,d,v,s); }
// Variable shifts: VEX.256.66.0F38 47/45/46
static void vpsllvd(Buf *b, int d, int v, int s) { vex_rrr(b,1,2,1,0x47,d,v,s); }
static void vpsrlvd(Buf *b, int d, int v, int s) { vex_rrr(b,1,2,1,0x45,d,v,s); }
static void vpsravd(Buf *b, int d, int v, int s) { vex_rrr(b,1,2,1,0x46,d,v,s); }
// Shift by immediate: VEX.256.66.0F 72 /6,/2,/4
static void vpslld_i(Buf *b, int d, int s, uint8_t imm) { vex_shift(b,1,1,1,0x72,6,d,s,imm); }
static void vpsrld_i(Buf *b, int d, int s, uint8_t imm) { vex_shift(b,1,1,1,0x72,2,d,s,imm); }
static void vpsrad_i(Buf *b, int d, int s, uint8_t imm) { vex_shift(b,1,1,1,0x72,4,d,s,imm); }

// ---- Bitwise (YMM) ----
static void vpand(Buf *b, int L, int d, int v, int s)  { vex_rrr(b,1,1,L,0xDB,d,v,s); }
static void vpor(Buf *b, int L, int d, int v, int s)   { vex_rrr(b,1,1,L,0xEB,d,v,s); }
static void vpxor_3(Buf *b, int L, int d, int v, int s) { vex_rrr(b,1,1,L,0xEF,d,v,s); }
// VPBLENDVB d,z,y,x: d = x[i]&0x80 ? y[i] : z[i] — VEX.NDS.{L}.66.0F3A 4C /r /is4
static void vpblendvb(Buf *b, int L, int d, int z, int y, int x) {
    vex_rrr(b,1,3,L,0x4C,d,z,y); emit1(b, (uint8_t)(x<<4));
}

// ---- I32 compare ----
// VPCMPEQD: VEX.256.66.0F 76
static void vpcmpeqd(Buf *b, int d, int v, int s) { vex_rrr(b,1,1,1,0x76,d,v,s); }
// VPCMPGTD: VEX.256.66.0F 66
static void vpcmpgtd(Buf *b, int d, int v, int s) { vex_rrr(b,1,1,1,0x66,d,v,s); }

// ---- I16 arithmetic (XMM, L=0) ----
static void vpaddw(Buf *b, int d, int v, int s)  { vex_rrr(b,1,1,0,0xFD,d,v,s); }
static void vpsubw(Buf *b, int d, int v, int s)  { vex_rrr(b,1,1,0,0xF9,d,v,s); }
static void vpmullw(Buf *b, int d, int v, int s) { vex_rrr(b,1,1,0,0xD5,d,v,s); }
// Shift by immediate: VEX.128.66.0F 71 /6,/2,/4
static void vpsllw_i(Buf *b, int d, int s, uint8_t imm) { vex_shift(b,1,1,0,0x71,6,d,s,imm); }
static void vpsrlw_i(Buf *b, int d, int s, uint8_t imm) { vex_shift(b,1,1,0,0x71,2,d,s,imm); }
static void vpsraw_i(Buf *b, int d, int s, uint8_t imm) { vex_shift(b,1,1,0,0x71,4,d,s,imm); }

// I16 compare (XMM)
static void vpcmpeqw(Buf *b, int d, int v, int s) { vex_rrr(b,1,1,0,0x75,d,v,s); }
static void vpcmpgtw(Buf *b, int d, int v, int s) { vex_rrr(b,1,1,0,0x65,d,v,s); }

// Widening/narrowing
// VPMOVSXWD ymm,xmm — VEX.256.66.0F38 23 (sign-extend i16->i32)
static void vpmovsxwd(Buf *b, int d, int s) { vex_rr(b,1,2,1,0x23,d,s); }
// VPMOVZXWD ymm,xmm — VEX.256.66.0F38 33 (zero-extend i16->i32)
static void vpmovzxwd(Buf *b, int d, int s) { vex_rr(b,1,2,1,0x33,d,s); }
static void vpackuswb(Buf *b, int d, int v, int s) { vex_rrr(b,1,1,0,0x67,d,v,s); }
static void vpunpcklbw(Buf *b, int d, int v, int s) { vex_rrr(b,1,1,0,0x60,d,v,s); }

// VEXTRACTI128 xmm,ymm,imm8 — VEX.256.66.0F3A 39
static void vextracti128(Buf *b, int d, int s, uint8_t imm) {
    vex_rrr(b,1,3,1,0x39,s,0,d); emit1(b, imm);
}
// VINSERTI128 ymm,ymm,xmm,imm8 — VEX.256.66.0F3A 38
static void vinserti128(Buf *b, int d, int v, int s, uint8_t imm) {
    vex_rrr(b,1,3,1,0x38,d,v,s); emit1(b, imm);
}
