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
    u32 v1 = p1[2];
    u32 v2 = p1[3];
    u32 v3 = p1[4];
    u32 v4 = p1[5];
    u32 v5 = 255u;
    u32 v6 = 8u;
    u32 v7 = 16u;
    u32 v8 = 24u;
    u32 v9 = 998277249u;
    u32 v10 = 1065353216u;
    u32 v11 = f2u(u2f(v10) - u2f(v4));
    u32 v12 = 1132396544u;
    u32 v13 = 1056964608u;

    for (int i = 0; i < n; i++) {
        u32 v14 = p0[i];
        u32 v15 = (u32)(v14 & v5);
        u32 v16 = f2u((float)(s32)v15);
        u32 v17 = f2u(u2f(v9) * u2f(v16));
        u32 v18 = f2u(fmaf(u2f(v17), u2f(v11), u2f(v1)));
        u32 v19 = f2u(fmaf(u2f(v18), u2f(v12), u2f(v13)));
        u32 v20 = (u32)(s32)u2f(v19);
        u32 v21 = (u32)(v5 & v20);
        u32 v22 = (u32)(v14 >> v6);
        u32 v23 = (u32)(v14 >> 8);
        u32 v24 = v22;
        u32 v25 = (u32)(v5 & v24);
        u32 v26 = f2u((float)(s32)v25);
        u32 v27 = f2u(u2f(v9) * u2f(v26));
        u32 v28 = f2u(fmaf(u2f(v27), u2f(v11), u2f(v2)));
        u32 v29 = f2u(fmaf(u2f(v28), u2f(v12), u2f(v13)));
        u32 v30 = (u32)(s32)u2f(v29);
        u32 v31 = (u32)(v5 & v30);
        u32 v32 = (u32)(v31 << v6);
        u32 v33 = (u32)(v31 << 8);
        u32 v34 = v32;
        u32 v35 = (u32)(v21 | v34);
        u32 v36 = (u32)(v14 >> v7);
        u32 v37 = (u32)(v14 >> 16);
        u32 v38 = v36;
        u32 v39 = (u32)(v5 & v38);
        u32 v40 = f2u((float)(s32)v39);
        u32 v41 = f2u(u2f(v9) * u2f(v40));
        u32 v42 = f2u(fmaf(u2f(v41), u2f(v11), u2f(v3)));
        u32 v43 = f2u(fmaf(u2f(v42), u2f(v12), u2f(v13)));
        u32 v44 = (u32)(s32)u2f(v43);
        u32 v45 = (u32)(v5 & v44);
        u32 v46 = (u32)(v45 << v7);
        u32 v47 = (u32)(v45 << 16);
        u32 v48 = v46;
        u32 v49 = (u32)(v35 | v48);
        u32 v50 = (u32)(v14 >> v8);
        u32 v51 = (u32)(v14 >> 24);
        u32 v52 = v50;
        u32 v53 = f2u((float)(s32)v52);
        u32 v54 = f2u(u2f(v9) * u2f(v53));
        u32 v55 = f2u(fmaf(u2f(v54), u2f(v11), u2f(v4)));
        u32 v56 = f2u(fmaf(u2f(v55), u2f(v12), u2f(v13)));
        u32 v57 = (u32)(s32)u2f(v56);
        u32 v58 = (u32)(v57 << v8);
        u32 v59 = (u32)(v57 << 24);
        u32 v60 = v58;
        u32 v61 = (u32)(v49 | v60);
        p0[i] = v61;
    }
}
