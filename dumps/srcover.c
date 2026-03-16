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
    u16* restrict p2 = (u16*)ptrs[2];
    u16* restrict p3 = (u16*)ptrs[3];
    u16* restrict p4 = (u16*)ptrs[4];
    long sz0 = szs[0];
    long sz1 = szs[1];
    long sz2 = szs[2];
    long sz3 = szs[3];
    long sz4 = szs[4];
    u32 v0 = 0u;
    u32 v1 = 998277249u;
    u32 v2 = 1065353216u;

    for (int i = 0; i < n; i++) {
        u32 v3 = (u32)((unsigned char*)ptrs[0])[i*4+0];
        u32 v4 = f2u((float)(s32)v3);
        u32 v5 = (u32)(s32)(s16)p1[i];
        u32 v6 = f2u(h2f((u16)v5));
        u32 v7 = (u32)((unsigned char*)ptrs[0])[i*4+3];
        u32 v8 = f2u((float)(s32)v7);
        u32 v9 = f2u(fmaf(-u2f(v1), u2f(v8), u2f(v2)));
        u32 v10 = f2u(u2f(v6) * u2f(v9));
        u32 v11 = f2u(fmaf(u2f(v1), u2f(v4), u2f(v10)));
        u32 v12 = (u32)f2h(u2f(v11));
        p1[i] = (u16)v12;

        u32 v14 = (u32)((unsigned char*)ptrs[0])[i*4+1];
        u32 v15 = f2u((float)(s32)v14);
        u32 v16 = (u32)(s32)(s16)p2[i];
        u32 v17 = f2u(h2f((u16)v16));
        u32 v18 = f2u(u2f(v17) * u2f(v9));
        u32 v19 = f2u(fmaf(u2f(v1), u2f(v15), u2f(v18)));
        u32 v20 = (u32)f2h(u2f(v19));
        p2[i] = (u16)v20;

        u32 v22 = (u32)((unsigned char*)ptrs[0])[i*4+2];
        u32 v23 = f2u((float)(s32)v22);
        u32 v24 = (u32)(s32)(s16)p3[i];
        u32 v25 = f2u(h2f((u16)v24));
        u32 v26 = f2u(u2f(v25) * u2f(v9));
        u32 v27 = f2u(fmaf(u2f(v1), u2f(v23), u2f(v26)));
        u32 v28 = (u32)f2h(u2f(v27));
        p3[i] = (u16)v28;

        u32 v30 = (u32)(s32)(s16)p4[i];
        u32 v31 = f2u(h2f((u16)v30));
        u32 v32 = f2u(u2f(v31) * u2f(v9));
        u32 v33 = f2u(fmaf(u2f(v1), u2f(v8), u2f(v32)));
        u32 v34 = (u32)f2h(u2f(v33));
        p4[i] = (u16)v34;
    }
}
