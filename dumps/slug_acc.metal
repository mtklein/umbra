#include <metal_stdlib>
using namespace metal;

static inline int safe_ix(int ix, uint bytes, int elem) {
    int count = (int)(bytes / (uint)elem);
    return clamp(ix, 0, max(count-1, 0));
}
static inline uint oob_mask(int ix, uint bytes, int elem) {
    int count = (int)(bytes / (uint)elem);
    return (ix >= 0 && ix < count) ? ~0u : 0u;
}

kernel void umbra_entry(
    constant uint &w [[buffer(3)]],
    constant uint *buf_szs [[buffer(4)]],
    constant uint *buf_rbs [[buffer(5)]],
    constant uint &x0 [[buffer(6)]],
    constant uint &y0 [[buffer(7)]],
    constant uint *buf_fmts [[buffer(8)]],
    constant float *buf_transfers [[buffer(9)]],
    device uchar *p0 [[buffer(0)]],
    device uchar *p1 [[buffer(1)]],
    device uchar *p2 [[buffer(2)]],
    device uchar *p0_g [[buffer(10)]],
    device uchar *p0_b [[buffer(11)]],
    device uchar *p0_a [[buffer(12)]],
    device uchar *p1_g [[buffer(13)]],
    device uchar *p1_b [[buffer(14)]],
    device uchar *p1_a [[buffer(15)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= w) return;
    uint x = x0 + pos.x;
    uint y = y0 + pos.y;
    uint v0 = 0u;
    uint v2 = ((device const uint*)p0)[0];
    uint v3 = ((device const uint*)p0)[1];
    uint v4 = ((device const uint*)p0)[2];
    uint v5 = ((device const uint*)p0)[3];
    uint v6 = ((device const uint*)p0)[4];
    uint v7 = ((device const uint*)p0)[5];
    uint v8 = ((device const uint*)p0)[6];
    uint v9 = ((device const uint*)p0)[7];
    uint v10 = ((device const uint*)p0)[8];
    uint v11 = ((device const uint*)p0)[9];
    uint v12 = ((device const uint*)p0)[10];
    uint v13 = 1065353216u;
    uint v14 = 1073741824u;
    uint v15 = 931135488u;
    uint v16 = ((device const uint*)p0)[18];
    uint v17 = 6u;
    uint v18 = v16 * 6u;
    uint v19 = ((device uint*)p2)[safe_ix((int)v18,buf_szs[2],4)] & oob_mask((int)v18,buf_szs[2],4);
    uint v20 = 1u;
    uint v21 = v18 + 1u;
    uint v22 = ((device uint*)p2)[safe_ix((int)v21,buf_szs[2],4)] & oob_mask((int)v21,buf_szs[2],4);
    uint v23 = 2u;
    uint v24 = v18 + 2u;
    uint v25 = ((device uint*)p2)[safe_ix((int)v24,buf_szs[2],4)] & oob_mask((int)v24,buf_szs[2],4);
    uint v26 = 3u;
    uint v27 = v18 + 3u;
    uint v28 = ((device uint*)p2)[safe_ix((int)v27,buf_szs[2],4)] & oob_mask((int)v27,buf_szs[2],4);
    uint v29 = 4u;
    uint v30 = v18 + 4u;
    uint v31 = ((device uint*)p2)[safe_ix((int)v30,buf_szs[2],4)] & oob_mask((int)v30,buf_szs[2],4);
    uint v32 = 5u;
    uint v33 = v18 + 5u;
    uint v34 = ((device uint*)p2)[safe_ix((int)v33,buf_szs[2],4)] & oob_mask((int)v33,buf_szs[2],4);
    uint v35 = 3212836864u;
    uint v36 = x0 + pos.x;
    uint v37 = as_type<uint>((float)(int)v36);
    uint v38 = y0 + pos.y;
    uint v39 = as_type<uint>((float)(int)v38);
    uint v40 = as_type<uint>(as_type<float>(v39) * as_type<float>(v6));
    uint v41 = as_type<uint>(fma(as_type<float>(v37), as_type<float>(v5), as_type<float>(v40)));
    uint v42 = as_type<uint>(as_type<float>(v7) + as_type<float>(v41));
    uint v43 = as_type<uint>(as_type<float>(v39) * as_type<float>(v9));
    uint v44 = as_type<uint>(fma(as_type<float>(v37), as_type<float>(v8), as_type<float>(v43)));
    uint v45 = as_type<uint>(as_type<float>(v10) + as_type<float>(v44));
    uint v46 = as_type<uint>(as_type<float>(v42) / as_type<float>(v45));
    uint v47 = as_type<uint>(as_type<float>(v34) - as_type<float>(v46));
    uint v48 = as_type<uint>(as_type<float>(v39) * as_type<float>(v3));
    uint v49 = as_type<uint>(fma(as_type<float>(v37), as_type<float>(v2), as_type<float>(v48)));
    uint v50 = as_type<uint>(as_type<float>(v4) + as_type<float>(v49));
    uint v51 = as_type<uint>(as_type<float>(v50) / as_type<float>(v45));
    uint v52 = as_type<uint>(as_type<float>(v31) - as_type<float>(v51));
    uint v53 = as_type<float>(v51) <  as_type<float>(v11) ? 0xffffffffu : 0u;
    uint v54 = as_type<float>(v46) <  as_type<float>(v12) ? 0xffffffffu : 0u;
    uint v55 = as_type<uint>(as_type<float>(v28) - as_type<float>(v46));
    uint v56 = as_type<uint>(as_type<float>(v55) * as_type<float>(1073741824u));
    uint v57 = as_type<uint>(as_type<float>(v22) - as_type<float>(v46));
    uint v58 = as_type<uint>(as_type<float>(v57) - as_type<float>(v56));
    uint v59 = as_type<uint>(as_type<float>(v47) + as_type<float>(v58));
    uint v60 = as_type<uint>(fabs(as_type<float>(v59)));
    uint v61 = as_type<float>(v15) <  as_type<float>(v60) ? 0xffffffffu : 0u;
    uint v62 = (v61 & v59) | (~v61 & v13);
    uint v63 = as_type<uint>(as_type<float>(v13) / as_type<float>(v62));
    uint v64 = as_type<uint>(as_type<float>(v25) - as_type<float>(v51));
    uint v65 = as_type<uint>(as_type<float>(v64) * as_type<float>(1073741824u));
    uint v66 = as_type<uint>(as_type<float>(v19) - as_type<float>(v51));
    uint v67 = as_type<uint>(as_type<float>(v66) - as_type<float>(v65));
    uint v68 = as_type<uint>(as_type<float>(v52) + as_type<float>(v67));
    uint v69 = as_type<uint>(as_type<float>(v64) - as_type<float>(v66));
    uint v70 = as_type<uint>(as_type<float>(v69) * as_type<float>(1073741824u));
    uint v71 = as_type<uint>(as_type<float>(v57) - as_type<float>(v55));
    uint v72 = as_type<uint>(as_type<float>(v71) * as_type<float>(v71));
    uint v73 = as_type<uint>(fma(-as_type<float>(v57), as_type<float>(v59), as_type<float>(v72)));
    uint v74 = as_type<uint>(max(as_type<float>(v73), as_type<float>(0u)));
    uint v75 = as_type<uint>(sqrt(as_type<float>(v74)));
    uint v76 = as_type<uint>(as_type<float>(v71) + as_type<float>(v75));
    uint v77 = as_type<uint>(as_type<float>(v63) * as_type<float>(v76));
    uint v78 = as_type<uint>(fma(as_type<float>(v77), as_type<float>(v68), as_type<float>(v70)));
    uint v79 = as_type<uint>(fma(as_type<float>(v77), as_type<float>(v78), as_type<float>(v66)));
    uint v80 = as_type<float>(v0) <  as_type<float>(v79) ? 0xffffffffu : 0u;
    uint v81 = as_type<uint>(as_type<float>(v59) * as_type<float>(v77));
    uint v82 = as_type<uint>(as_type<float>(v81) - as_type<float>(v71));
    uint v83 = as_type<float>(v82) <  as_type<float>(0u) ? 0xffffffffu : 0u;
    uint v84 = (v83 & v35) | (~v83 & v0);
    uint v85 = as_type<float>(v0) <= as_type<float>(v51) ? 0xffffffffu : 0u;
    uint v86 = v85 & v53;
    uint v87 = as_type<float>(v0) <= as_type<float>(v46) ? 0xffffffffu : 0u;
    uint v88 = v87 & v54;
    uint v89 = v86 & v88;
    uint v90 = as_type<uint>(as_type<float>(v71) - as_type<float>(v75));
    uint v91 = as_type<uint>(as_type<float>(v63) * as_type<float>(v90));
    uint v92 = as_type<uint>(fabs(as_type<float>(v71)));
    uint v93 = as_type<float>(v15) <  as_type<float>(v92) ? 0xffffffffu : 0u;
    uint v94 = as_type<uint>(as_type<float>(v71) * as_type<float>(1073741824u));
    uint v95 = (v93 & v94) | (~v93 & v13);
    uint v96 = as_type<uint>(as_type<float>(v57) / as_type<float>(v95));
    uint v97 = (v61 & v91) | (~v61 & v96);
    uint v98 = as_type<uint>(as_type<float>(v59) * as_type<float>(v97));
    uint v99 = as_type<uint>(as_type<float>(v98) - as_type<float>(v71));
    uint v100 = as_type<float>(v99) <  as_type<float>(0u) ? 0xffffffffu : 0u;
    uint v101 = (v100 & v35) | (~v100 & v0);
    uint v102 = as_type<float>(v0) <= as_type<float>(v97) ? 0xffffffffu : 0u;
    uint v103 = as_type<float>(v97) <  as_type<float>(1065353216u) ? 0xffffffffu : 0u;
    uint v104 = v102 & v103;
    uint v105 = as_type<float>(v0) <= as_type<float>(v73) ? 0xffffffffu : 0u;
    uint v106 = v105 & v61;
    uint v107 = v105 & v104;
    uint v108 = as_type<float>(v77) <  as_type<float>(1065353216u) ? 0xffffffffu : 0u;
    uint v109 = as_type<float>(v0) <= as_type<float>(v77) ? 0xffffffffu : 0u;
    uint v110 = v109 & v108;
    uint v111 = v106 & v110;
    uint v112 = v111 & v80;
    uint v113 = as_type<uint>(fma(as_type<float>(v97), as_type<float>(v68), as_type<float>(v70)));
    uint v114 = as_type<uint>(fma(as_type<float>(v97), as_type<float>(v113), as_type<float>(v66)));
    uint v115 = as_type<float>(v0) <  as_type<float>(v114) ? 0xffffffffu : 0u;
    uint v116 = v107 & v115;
    uint v117 = as_type<float>(v0) <  as_type<float>(v99) ? 0xffffffffu : 0u;
    uint v118 = (v117 & v13) | (~v117 & v101);
    uint v119 = (v116 & v118) | (~v116 & v0);
    uint v120 = as_type<float>(v0) <  as_type<float>(v82) ? 0xffffffffu : 0u;
    uint v121 = (v120 & v13) | (~v120 & v84);
    uint v122 = (v112 & v121) | (~v112 & v0);
    uint v123 = as_type<uint>(as_type<float>(v119) + as_type<float>(v122));
    uint v124 = (v89 & v123) | (~v89 & v0);
    uint v125 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v126 = as_type<uint>(as_type<float>(v124) + as_type<float>(v125));
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v126;
}
