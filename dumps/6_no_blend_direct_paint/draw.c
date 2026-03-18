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
    u32* restrict p0 = (u32*)ptrs[0];
    u32* restrict p1 = (u32*)ptrs[1];
    long sz0 = szs[0];
    long sz1 = szs[1];
    u32 v0 = 0u;
    u32 v1 = p1[0];
    u32 v2 = p1[1];
    u32 v3 = f2u((float)(s32)v2);
    u32 v4 = p1[2];
    u32 v5 = p1[3];
    u32 v6 = p1[4];
    u32 v7 = p1[5];
    u32 v8 = p1[6];
    u32 v9 = p1[7];
    u32 v10 = p1[8];
    u32 v11 = p1[9];
    u32 v12 = (u32)-(s32)(u2f(v9) <= u2f(v3));
    u32 v13 = (u32)-(s32)(u2f(v3) <  u2f(v11));
    u32 v14 = (u32)(v12 & v13);
    u32 v15 = 1065353216u;
    u32 v16 = 1132396544u;
    u32 v17 = 1056964608u;
    u32 v18 = 255u;

    for (int i = 0; i < n; i++) {
        u32 v19 = (u32)i;
        u32 v20 = (u32)(v19 + v1);
        u32 v21 = f2u((float)(s32)v20);
        u32 v22 = (u32)-(s32)(u2f(v8) <= u2f(v21));
        u32 v23 = (u32)-(s32)(u2f(v21) <  u2f(v10));
        u32 v24 = (u32)(v22 & v23);
        u32 v25 = (u32)(v24 & v14);
        u32 v26 = (v25 & v15) | (~v25 & v0);
        u32 v27 = f2u(fmaf(u2f(v4), u2f(v26), u2f(v0)));
        u32 v28 = f2u(fmaf(u2f(v27), u2f(v16), u2f(v17)));
        u32 v29 = (u32)(s32)u2f(v28);
        u32 v30 = (u32)(v29 & v18);
        u32 v31 = f2u(fmaf(u2f(v5), u2f(v26), u2f(v0)));
        u32 v32 = f2u(fmaf(u2f(v31), u2f(v16), u2f(v17)));
        u32 v33 = (u32)(s32)u2f(v32);
        u32 v34 = (u32)(v33 & v18);
        u32 v35 = (u32)(v34 << 8);
        u32 v36 = (u32)(v30 | v35);
        u32 v37 = f2u(fmaf(u2f(v6), u2f(v26), u2f(v0)));
        u32 v38 = f2u(fmaf(u2f(v37), u2f(v16), u2f(v17)));
        u32 v39 = (u32)(s32)u2f(v38);
        u32 v40 = (u32)(v39 & v18);
        u32 v41 = (u32)(v40 << 16);
        u32 v42 = (u32)(v36 | v41);
        u32 v43 = f2u(fmaf(u2f(v7), u2f(v26), u2f(v0)));
        u32 v44 = f2u(fmaf(u2f(v43), u2f(v16), u2f(v17)));
        u32 v45 = (u32)(s32)u2f(v44);
        u32 v46 = (u32)(v45 << 24);
        u32 v47 = (u32)(v42 | v46);
        p0[i] = v47;
    }
}
