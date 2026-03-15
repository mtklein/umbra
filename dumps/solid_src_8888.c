#include <stdint.h>
#include <math.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef  int32_t s32;
typedef  int16_t s16;

static inline u32 f2u(float f) { union{float f;u32 u;} x; x.f=f; return x.u; }
static inline float u2f(u32 u) { union{float f;u32 u;} x; x.u=u; return x.f; }
static inline float h2f(u16 h) {
    u32 sign=((u32)h>>15)<<31, exp=((u32)h>>10)&0x1f, mant=(u32)h&0x3ff;
    u32 normal = sign | ((exp+112)<<23) | (mant<<13);
    u32 zero   = sign;
    u32 infnan = sign | (0xffu<<23) | (mant<<13);
    u32 is_zero   = -(u32)(exp==0);
    u32 is_infnan = -(u32)(exp==31);
    u32 bits = (is_zero&zero) | (is_infnan&infnan) | (~is_zero&~is_infnan&normal);
    return u2f(bits);
}

static inline u16 f2h(float f) {
    u32 bits=f2u(f), sign=(bits>>31)<<15;
    s32 exp=(s32)((bits>>23)&0xff)-127+15;
    u32 mant=(bits>>13)&0x3ff, rb=(bits>>12)&1, st=-(u32)((bits&0xfff)!=0);
    mant+=rb&(st|(mant&1));
    u32 mo=mant>>10; exp+=(s32)mo; mant&=0x3ff;
    u32 normal = sign|((u32)exp<<10)|mant;
    u32 inf    = sign|0x7c00;
    u32 infnan = sign|0x7c00|mant;
    u32 is_of = -(u32)(exp>=31);
    u32 is_uf = -(u32)(exp<=0);
    u32 se=(bits>>23)&0xff;
    u32 is_in = -(u32)(se==0xff);
    u32 r = (is_uf&sign) | (is_of&~is_in&inf) | (is_in&infnan) | (~is_uf&~is_of&~is_in&normal);
    return (u16)r;
}

static inline s32 clamp_ix(s32 ix, long bytes, int elem) {
    s32 hi = (s32)(bytes / elem) - 1;
    if (hi < 0) hi = 0;
    if (ix < 0) ix = 0;
    if (ix > hi) ix = hi;
    return ix;
}

void umbra_entry(int n, void **ptrs, long *szs) {
    u16* restrict p1 = (u16*)ptrs[1];
    long sz0 = szs[0];
    long sz1 = szs[1];
    u32 v0 = 0u;
    float v1 = h2f(p1[4]);
    float v2 = h2f(p1[5]);
    float v3 = h2f(p1[6]);
    float v4 = h2f(p1[7]);
    float v5 = h2f(23544);
    float v6 = h2f(14336);
    float v7 = v1 * v5 + v6;
    u16 v8 = (u16)(s16)v7;
    float v9 = v2 * v5 + v6;
    u16 v10 = (u16)(s16)v9;
    float v11 = v3 * v5 + v6;
    u16 v12 = (u16)(s16)v11;
    float v13 = v4 * v5 + v6;
    u16 v14 = (u16)(s16)v13;

    for (int i = 0; i < n; i++) {
        ((unsigned char*)ptrs[0])[i*4+0] = (unsigned char)v8;
        ((unsigned char*)ptrs[0])[i*4+1] = (unsigned char)v10;
        ((unsigned char*)ptrs[0])[i*4+2] = (unsigned char)v12;
        ((unsigned char*)ptrs[0])[i*4+3] = (unsigned char)v14;
    }
}
