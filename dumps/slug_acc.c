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
    void *pd1 = *(void**)((char*)ptrs[1] + 56);
    long szd1 = *(long*)((char*)ptrs[1] + 64);
    if (szd1 < 0) szd1 = -szd1;
    u32 v2 = p1[0];
    u32 v3 = 1u;
    u32 v4 = p1[1];
    u32 v5 = f2u((float)(s32)v4);
    u32 v6 = 2u;
    u32 v7 = p1[2];
    u32 v8 = 3u;
    u32 v9 = p1[3];
    u32 v10 = 4u;
    u32 v11 = p1[4];
    u32 v12 = 5u;
    u32 v13 = p1[5];
    u32 v14 = 6u;
    u32 v15 = p1[6];
    u32 v16 = p1[7];
    u32 v17 = p1[8];
    u32 v18 = p1[9];
    u32 v19 = p1[10];
    u32 v20 = p1[11];
    u32 v21 = p1[12];
    u32 v22 = f2u(u2f(v5) * u2f(v18));
    u32 v23 = f2u(u2f(v5) * u2f(v9));
    u32 v24 = f2u(u2f(v5) * u2f(v15));
    u32 v25 = 1065353216u;
    u32 v26 = 1073741824u;
    u32 v27 = 931135488u;
    u32 v28 = p1[18];
    u32 v29 = (u32)(v14 * v28);
    u32 v30 = ((u32*)pd1)[clamp_ix((s32)v29,szd1,4)];
    u32 v31 = (u32)(v3 + v29);
    u32 v32 = ((u32*)pd1)[clamp_ix((s32)v31,szd1,4)];
    u32 v33 = (u32)(v6 + v29);
    u32 v34 = ((u32*)pd1)[clamp_ix((s32)v33,szd1,4)];
    u32 v35 = (u32)(v8 + v29);
    u32 v36 = ((u32*)pd1)[clamp_ix((s32)v35,szd1,4)];
    u32 v37 = (u32)(v10 + v29);
    u32 v38 = ((u32*)pd1)[clamp_ix((s32)v37,szd1,4)];
    u32 v39 = (u32)(v12 + v29);
    u32 v40 = ((u32*)pd1)[clamp_ix((s32)v39,szd1,4)];
    u32 v41 = 3212836864u;

    for (int i = 0; i < n; i++) {
        u32 v42 = (u32)i;
        u32 v43 = (u32)(v42 + v2);
        u32 v44 = f2u((float)(s32)v43);
        u32 v45 = f2u(fmaf(u2f(v44), u2f(v17), u2f(v22)));
        u32 v46 = f2u(u2f(v19) + u2f(v45));
        u32 v47 = f2u(fmaf(u2f(v44), u2f(v7), u2f(v23)));
        u32 v48 = f2u(u2f(v11) + u2f(v47));
        u32 v49 = f2u(u2f(v48) / u2f(v46));
        u32 v50 = (u32)-(s32)(u2f(v0) <= u2f(v49));
        u32 v51 = (u32)-(s32)(u2f(v49) <  u2f(v20));
        u32 v52 = (u32)(v50 & v51);
        u32 v53 = f2u(fmaf(u2f(v44), u2f(v13), u2f(v24)));
        u32 v54 = f2u(u2f(v16) + u2f(v53));
        u32 v55 = f2u(u2f(v54) / u2f(v46));
        u32 v56 = (u32)-(s32)(u2f(v0) <= u2f(v55));
        u32 v57 = (u32)-(s32)(u2f(v55) <  u2f(v21));
        u32 v58 = (u32)(v56 & v57);
        u32 v59 = (u32)(v52 & v58);
        u32 v60 = f2u(u2f(v32) - u2f(v55));
        u32 v61 = f2u(u2f(v36) - u2f(v55));
        u32 v62 = f2u(u2f(v60) - u2f(v61));
        u32 v63 = f2u(u2f(v40) - u2f(v55));
        u32 v64 = f2u(fmaf(-u2f(v26), u2f(v61), u2f(v60)));
        u32 v65 = f2u(u2f(v63) + u2f(v64));
        u32 v66 = f2u(u2f(v0) - u2f(v65));
        u32 v67 = f2u(fmaxf(u2f(v65), u2f(v66)));
        u32 v68 = (u32)-(s32)(u2f(v27) <  u2f(v67));
        u32 v69 = (v68 & v65) | (~v68 & v25);
        u32 v70 = f2u(u2f(v25) / u2f(v69));
        u32 v71 = f2u(u2f(v62) * u2f(v62));
        u32 v72 = f2u(fmaf(-u2f(v60), u2f(v65), u2f(v71)));
        u32 v73 = f2u(fmaxf(u2f(v0), u2f(v72)));
        u32 v74 = f2u(sqrtf(u2f(v73)));
        u32 v75 = f2u(u2f(v62) - u2f(v74));
        u32 v76 = f2u(u2f(v70) * u2f(v75));
        u32 v77 = f2u(u2f(v0) - u2f(v62));
        u32 v78 = f2u(fmaxf(u2f(v62), u2f(v77)));
        u32 v79 = (u32)-(s32)(u2f(v27) <  u2f(v78));
        u32 v80 = f2u(u2f(v26) * u2f(v62));
        u32 v81 = (v79 & v80) | (~v79 & v25);
        u32 v82 = f2u(u2f(v60) / u2f(v81));
        u32 v83 = (v68 & v76) | (~v68 & v82);
        u32 v84 = f2u(u2f(v65) * u2f(v83));
        u32 v85 = f2u(u2f(v84) - u2f(v62));
        u32 v86 = (u32)-(s32)(u2f(v0) <  u2f(v85));
        u32 v87 = (u32)-(s32)(u2f(v85) <  u2f(v0));
        u32 v88 = (v87 & v41) | (~v87 & v0);
        u32 v89 = (v86 & v25) | (~v86 & v88);
        u32 v90 = (u32)-(s32)(u2f(v0) <= u2f(v72));
        u32 v91 = (u32)-(s32)(u2f(v0) <= u2f(v83));
        u32 v92 = (u32)-(s32)(u2f(v83) <  u2f(v25));
        u32 v93 = (u32)(v91 & v92);
        u32 v94 = (u32)(v90 & v93);
        u32 v95 = f2u(u2f(v30) - u2f(v49));
        u32 v96 = f2u(u2f(v34) - u2f(v49));
        u32 v97 = f2u(u2f(v96) - u2f(v95));
        u32 v98 = f2u(u2f(v38) - u2f(v49));
        u32 v99 = f2u(fmaf(-u2f(v26), u2f(v96), u2f(v95)));
        u32 v100 = f2u(u2f(v98) + u2f(v99));
        u32 v101 = f2u(u2f(v83) * u2f(v100));
        u32 v102 = f2u(fmaf(u2f(v26), u2f(v97), u2f(v101)));
        u32 v103 = f2u(fmaf(u2f(v83), u2f(v102), u2f(v95)));
        u32 v104 = (u32)-(s32)(u2f(v0) <  u2f(v103));
        u32 v105 = (u32)(v94 & v104);
        u32 v106 = (v105 & v89) | (~v105 & v0);
        u32 v107 = f2u(u2f(v62) + u2f(v74));
        u32 v108 = f2u(u2f(v70) * u2f(v107));
        u32 v109 = f2u(u2f(v65) * u2f(v108));
        u32 v110 = f2u(u2f(v109) - u2f(v62));
        u32 v111 = (u32)-(s32)(u2f(v0) <  u2f(v110));
        u32 v112 = (u32)-(s32)(u2f(v110) <  u2f(v0));
        u32 v113 = (v112 & v41) | (~v112 & v0);
        u32 v114 = (v111 & v25) | (~v111 & v113);
        u32 v115 = (u32)(v90 & v68);
        u32 v116 = (u32)-(s32)(u2f(v0) <= u2f(v108));
        u32 v117 = (u32)-(s32)(u2f(v108) <  u2f(v25));
        u32 v118 = (u32)(v116 & v117);
        u32 v119 = (u32)(v115 & v118);
        u32 v120 = f2u(u2f(v108) * u2f(v100));
        u32 v121 = f2u(fmaf(u2f(v26), u2f(v97), u2f(v120)));
        u32 v122 = f2u(fmaf(u2f(v108), u2f(v121), u2f(v95)));
        u32 v123 = (u32)-(s32)(u2f(v0) <  u2f(v122));
        u32 v124 = (u32)(v119 & v123);
        u32 v125 = (v124 & v114) | (~v124 & v0);
        u32 v126 = f2u(u2f(v106) + u2f(v125));
        u32 v127 = (v59 & v126) | (~v59 & v0);
        u32 v128 = p0[i];
        u32 v129 = f2u(u2f(v127) + u2f(v128));
        p0[i] = v129;
    }
}
