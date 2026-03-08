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

void umbra_entry(int n, void **ptrs) {
    u16* restrict p3 = (u16*)ptrs[3];
    u32 v0 = 0u;
    float v1 = h2f(p3[0]);
    u32 v2 = 1u;
    float v3 = h2f(p3[1]);
    u32 v4 = 2u;
    float v5 = h2f(p3[2]);
    u32 v6 = 3u;
    float v7 = h2f(p3[3]);
    float v8 = h2f(7172);
    float v9 = h2f(15360);
    float v10 = v9 - v7;
    float v11 = h2f(23544);
    float v12 = h2f(14336);

    for (int i = 0; i < n; i++) {
        u16 v13 = (u16)((unsigned char*)ptrs[0])[i*4+0];
        float v14 = (float)(s16)v13;
        float v15 = v8 * v14;
        float v16 = v15 * v10 + v1;
        float v17 = v16 * v11 + v12;
        u16 v18 = (u16)(s16)v17;
        u16 v19 = (u16)((unsigned char*)ptrs[0])[i*4+1];
        float v20 = (float)(s16)v19;
        float v21 = v8 * v20;
        float v22 = v21 * v10 + v3;
        float v23 = v22 * v11 + v12;
        u16 v24 = (u16)(s16)v23;
        u16 v25 = (u16)((unsigned char*)ptrs[0])[i*4+2];
        float v26 = (float)(s16)v25;
        float v27 = v8 * v26;
        float v28 = v27 * v10 + v5;
        float v29 = v28 * v11 + v12;
        u16 v30 = (u16)(s16)v29;
        u16 v31 = (u16)((unsigned char*)ptrs[0])[i*4+3];
        float v32 = (float)(s16)v31;
        float v33 = v8 * v32;
        float v34 = v33 * v10 + v7;
        float v35 = v34 * v11 + v12;
        u16 v36 = (u16)(s16)v35;
        ((unsigned char*)ptrs[0])[i*4+0] = (unsigned char)v18;
        ((unsigned char*)ptrs[0])[i*4+1] = (unsigned char)v24;
        ((unsigned char*)ptrs[0])[i*4+2] = (unsigned char)v30;
        ((unsigned char*)ptrs[0])[i*4+3] = (unsigned char)v36;
    }
}
