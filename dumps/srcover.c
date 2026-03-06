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

void umbra_entry(int n, void *a0, void *a1, void *a2, void *a3, void *a4, void *a5) {
    u32* restrict p0 = (u32*)a0;
    u16* restrict p1 = (u16*)a1;
    u16* restrict p2 = (u16*)a2;
    u16* restrict p3 = (u16*)a3;
    u16* restrict p4 = (u16*)a4;
    u32 v0 = 0u;
    u32 v1 = 255u;
    float v2 = h2f(7172);
    float v3 = h2f(15360);

    for (int i = 0; i < n; i++) {
        u32 v4 = p0[i];
        u32 v5 = (u32)(v4 & v1);
        u16 v6 = (u16)(s16)v5;
        float v7 = (float)(s16)v6;
        float v8 = h2f(p1[i]);
        u32 v9 = (u32)(v4 >> 24);
        u32 v10 = (u32)(v1 & v9);
        u16 v11 = (u16)(s16)v10;
        float v12 = (float)(s16)v11;
        float v13 = v2 * v12;
        float v14 = v3 - v13;
        float v15 = v8 * v14;
        float v16 = v2 * v7 + v15;
        p1[i] = f2h(v16);

        u32 v18 = (u32)(v4 >> 8);
        u32 v19 = (u32)(v1 & v18);
        u16 v20 = (u16)(s16)v19;
        float v21 = (float)(s16)v20;
        float v22 = h2f(p2[i]);
        float v23 = v22 * v14;
        float v24 = v2 * v21 + v23;
        p2[i] = f2h(v24);

        u32 v26 = (u32)(v4 >> 16);
        u32 v27 = (u32)(v1 & v26);
        u16 v28 = (u16)(s16)v27;
        float v29 = (float)(s16)v28;
        float v30 = h2f(p3[i]);
        float v31 = v30 * v14;
        float v32 = v2 * v29 + v31;
        p3[i] = f2h(v32);

        float v34 = h2f(p4[i]);
        float v35 = v34 * v14;
        float v36 = v2 * v12 + v35;
        p4[i] = f2h(v36);
    }
}
