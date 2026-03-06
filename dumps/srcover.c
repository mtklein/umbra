#include <stdint.h>
#include <math.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef  int32_t s32;
typedef  int16_t s16;

static inline u32 f2u(float f) { union{float f;u32 u;} x; x.f=f; return x.u; }
static inline float u2f(u32 u) { union{float f;u32 u;} x; x.u=u; return x.f; }
static inline u32 bytes(u32 x, int c) {
    unsigned char b[] = {0, x&0xff, (x>>8)&0xff, (x>>16)&0xff, (x>>24)&0xff};
    return (u32)b[c&0xf] | (u32)b[(c>>4)&0xf]<<8 | (u32)b[(c>>8)&0xf]<<16 | (u32)b[(c>>12)&0xf]<<24;
}

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

void umbra_entry(int n, void *a0, void *a1, void *a2, void *a3, void *a4, void *a5) {
    u16* restrict p1 = (u16*)a1;
    u16* restrict p2 = (u16*)a2;
    u16* restrict p3 = (u16*)a3;
    u16* restrict p4 = (u16*)a4;
    u32 v0 = 0u;
    float v1 = h2f(7172);
    float v2 = h2f(15360);

    for (int i = 0; i < n; i++) {
        u16 v3 = ((unsigned char*)a0)[i*4+0];
        u16 v4 = (u16)v3;
        float v5 = (float)(s16)v4;
        float v6 = h2f(p1[i]);
        u16 v7 = ((unsigned char*)a0)[i*4+3];
        u16 v8 = (u16)v7;
        float v9 = (float)(s16)v8;
        float v10 = v1 * v9;
        float v11 = v2 - v10;
        float v12 = v6 * v11;
        float v13 = v1 * v5 + v12;
        p1[i] = f2h(v13);

        u16 v15 = ((unsigned char*)a0)[i*4+1];
        u16 v16 = (u16)v15;
        float v17 = (float)(s16)v16;
        float v18 = h2f(p2[i]);
        float v19 = v18 * v11;
        float v20 = v1 * v17 + v19;
        p2[i] = f2h(v20);

        u16 v22 = ((unsigned char*)a0)[i*4+2];
        u16 v23 = (u16)v22;
        float v24 = (float)(s16)v23;
        float v25 = h2f(p3[i]);
        float v26 = v25 * v11;
        float v27 = v1 * v24 + v26;
        p3[i] = f2h(v27);

        float v29 = h2f(p4[i]);
        float v30 = v29 * v11;
        float v31 = v1 * v9 + v30;
        p4[i] = f2h(v31);
    }
}
