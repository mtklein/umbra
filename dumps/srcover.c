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
    u32 v2 = 998277249u;
    u32 v3 = 8u;
    u32 v4 = 16u;
    u32 v5 = 24u;
    float v6 = h2f(15360);
    for (int i = 0; i < n; i++) {
        u32 v7 = (u32)i;
        u32 v8 = p0[i];
        u32 v9 = (u32)(v8 & v1);
        u32 v10 = f2u((float)(s32)v9);
        u32 v11 = f2u(u2f(v2) * u2f(v10));
        float v12 = u2f(v11);
        u32 v13 = (u32)(v8 >> v3);
        u32 v14 = (u32)(v1 & v13);
        u32 v15 = f2u((float)(s32)v14);
        u32 v16 = f2u(u2f(v2) * u2f(v15));
        float v17 = u2f(v16);
        u32 v18 = (u32)(v8 >> v4);
        u32 v19 = (u32)(v1 & v18);
        u32 v20 = f2u((float)(s32)v19);
        u32 v21 = f2u(u2f(v2) * u2f(v20));
        float v22 = u2f(v21);
        u32 v23 = (u32)(v8 >> v5);
        u32 v24 = (u32)(v1 & v23);
        u32 v25 = f2u((float)(s32)v24);
        u32 v26 = f2u(u2f(v2) * u2f(v25));
        float v27 = u2f(v26);
        float v28 = h2f(p1[i]);
        float v29 = h2f(p2[i]);
        float v30 = h2f(p3[i]);
        float v31 = h2f(p4[i]);
        float v32 = v6 - v27;
        float v33 = v28 * v32 + v12;
        float v34 = v29 * v32 + v17;
        float v35 = v30 * v32 + v22;
        float v36 = v31 * v32 + v27;
        p1[i] = f2h(v33);
        p2[i] = f2h(v34);
        p3[i] = f2h(v35);
        p4[i] = f2h(v36);
    }
}
