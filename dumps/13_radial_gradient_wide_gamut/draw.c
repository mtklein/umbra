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
    u32 v2 = 1u;
    u32 v3 = p1[1];
    u32 v4 = f2u((float)(s32)v3);
    void *pd5 = *(void**)((char*)ptrs[1] + 24);
    long szd5 = *(long*)((char*)ptrs[1] + 32);
    if (szd5 < 0) szd5 = -szd5;
    u32 v6 = 2u;
    u32 v7 = p1[2];
    u32 v8 = 3u;
    u32 v9 = p1[3];
    u32 v10 = 4u;
    u32 v11 = p1[4];
    u32 v12 = f2u(u2f(v4) - u2f(v9));
    u32 v13 = f2u(u2f(v12) * u2f(v12));
    u32 v14 = 1065353216u;
    u32 v15 = p1[5];
    u32 v16 = 1073741824u;
    u32 v17 = f2u(u2f(v15) - u2f(v14));
    u32 v18 = f2u(u2f(v15) - u2f(v16));
    u32 v19 = 1132396544u;
    u32 v20 = 1056964608u;
    u32 v21 = 255u;

    for (int i = 0; i < n; i++) {
        u32 v22 = (u32)i;
        u32 v23 = (u32)(v22 + v1);
        u32 v24 = f2u((float)(s32)v23);
        u32 v25 = f2u(u2f(v24) - u2f(v7));
        u32 v26 = f2u(fmaf(u2f(v25), u2f(v25), u2f(v13)));
        u32 v27 = f2u(sqrtf(u2f(v26)));
        u32 v28 = f2u(u2f(v11) * u2f(v27));
        u32 v29 = f2u(fmaxf(u2f(v0), u2f(v28)));
        u32 v30 = f2u(fminf(u2f(v29), u2f(v14)));
        u32 v31 = f2u(u2f(v30) * u2f(v17));
        u32 v32 = (u32)(s32)u2f(v31);
        u32 v33 = f2u((float)(s32)v32);
        u32 v34 = f2u(fminf(u2f(v18), u2f(v33)));
        u32 v35 = f2u(u2f(v31) - u2f(v34));
        u32 v36 = (u32)(s32)u2f(v34);
        u32 v37 = (u32)(v36 << 2);
        u32 v38 = ((u32*)pd5)[clamp_ix((s32)v37,szd5,4)];
        u32 v39 = (u32)(v10 + v37);
        u32 v40 = ((u32*)pd5)[clamp_ix((s32)v39,szd5,4)];
        u32 v41 = f2u(u2f(v40) - u2f(v38));
        u32 v42 = f2u(fmaf(u2f(v35), u2f(v41), u2f(v38)));
        u32 v43 = f2u(fmaf(u2f(v42), u2f(v19), u2f(v20)));
        u32 v44 = (u32)(s32)u2f(v43);
        u32 v45 = (u32)(v44 & v21);
        u32 v46 = (u32)(v2 + v37);
        u32 v47 = ((u32*)pd5)[clamp_ix((s32)v46,szd5,4)];
        u32 v48 = (u32)(v2 + v39);
        u32 v49 = ((u32*)pd5)[clamp_ix((s32)v48,szd5,4)];
        u32 v50 = f2u(u2f(v49) - u2f(v47));
        u32 v51 = f2u(fmaf(u2f(v35), u2f(v50), u2f(v47)));
        u32 v52 = f2u(fmaf(u2f(v51), u2f(v19), u2f(v20)));
        u32 v53 = (u32)(s32)u2f(v52);
        u32 v54 = (u32)(v53 & v21);
        u32 v55 = (u32)(v54 << 8);
        u32 v56 = (u32)(v45 | v55);
        u32 v57 = (u32)(v6 + v37);
        u32 v58 = ((u32*)pd5)[clamp_ix((s32)v57,szd5,4)];
        u32 v59 = (u32)(v6 + v39);
        u32 v60 = ((u32*)pd5)[clamp_ix((s32)v59,szd5,4)];
        u32 v61 = f2u(u2f(v60) - u2f(v58));
        u32 v62 = f2u(fmaf(u2f(v35), u2f(v61), u2f(v58)));
        u32 v63 = f2u(fmaf(u2f(v62), u2f(v19), u2f(v20)));
        u32 v64 = (u32)(s32)u2f(v63);
        u32 v65 = (u32)(v64 & v21);
        u32 v66 = (u32)(v65 << 16);
        u32 v67 = (u32)(v56 | v66);
        u32 v68 = (u32)(v8 + v37);
        u32 v69 = ((u32*)pd5)[clamp_ix((s32)v68,szd5,4)];
        u32 v70 = (u32)(v8 + v39);
        u32 v71 = ((u32*)pd5)[clamp_ix((s32)v70,szd5,4)];
        u32 v72 = f2u(u2f(v71) - u2f(v69));
        u32 v73 = f2u(fmaf(u2f(v35), u2f(v72), u2f(v69)));
        u32 v74 = f2u(fmaf(u2f(v73), u2f(v19), u2f(v20)));
        u32 v75 = (u32)(s32)u2f(v74);
        u32 v76 = (u32)(v75 << 24);
        u32 v77 = (u32)(v67 | v76);
        p0[i] = v77;
    }
}
