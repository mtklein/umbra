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
    float v2 = h2f(p3[1]);
    float v3 = h2f(p3[2]);
    float v4 = h2f(p3[3]);
    float v5 = h2f(7172);
    float v6 = h2f(15360);
    float v7 = v6 - v4;
    float v8 = h2f(23544);
    float v9 = h2f(14336);

    for (int i = 0; i < n; i++) {
        u16 v10 = (u16)((unsigned char*)ptrs[0])[i*4+0];
        float v11 = (float)(s16)v10;
        float v12 = v5 * v11;
        float v13 = v12 * v7 + v1;
        float v14 = v13 * v8 + v9;
        u16 v15 = (u16)(s16)v14;
        u16 v16 = (u16)((unsigned char*)ptrs[0])[i*4+1];
        float v17 = (float)(s16)v16;
        float v18 = v5 * v17;
        float v19 = v18 * v7 + v2;
        float v20 = v19 * v8 + v9;
        u16 v21 = (u16)(s16)v20;
        u16 v22 = (u16)((unsigned char*)ptrs[0])[i*4+2];
        float v23 = (float)(s16)v22;
        float v24 = v5 * v23;
        float v25 = v24 * v7 + v3;
        float v26 = v25 * v8 + v9;
        u16 v27 = (u16)(s16)v26;
        u16 v28 = (u16)((unsigned char*)ptrs[0])[i*4+3];
        float v29 = (float)(s16)v28;
        float v30 = v5 * v29;
        float v31 = v30 * v7 + v4;
        float v32 = v31 * v8 + v9;
        u16 v33 = (u16)(s16)v32;
        ((unsigned char*)ptrs[0])[i*4+0] = (unsigned char)v15;
        ((unsigned char*)ptrs[0])[i*4+1] = (unsigned char)v21;
        ((unsigned char*)ptrs[0])[i*4+2] = (unsigned char)v27;
        ((unsigned char*)ptrs[0])[i*4+3] = (unsigned char)v33;
    }
}
