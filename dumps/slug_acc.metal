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
    constant uint &n [[buffer(0)]],
    constant uint &w [[buffer(5)]],
    constant uint &stride [[buffer(6)]],
    constant uint &x0 [[buffer(7)]],
    constant uint &y0 [[buffer(8)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    device uchar *p2 [[buffer(3)]],
    constant uint *buf_szs [[buffer(4)]],
    uint2 pos [[thread_position_in_grid]]
) {
    uint i = (y0 + pos.y) * stride + x0 + pos.x;
    if (i >= n) return;
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
    uint v51 = as_type<float>(v46) <  as_type<float>(v12) ? 0xffffffffu : 0u;
    uint v52 = as_type<uint>(as_type<float>(v28) - as_type<float>(v46));
    uint v53 = as_type<uint>(as_type<float>(v22) - as_type<float>(v46));
    uint v54 = as_type<uint>(as_type<float>(v50) / as_type<float>(v45));
    uint v55 = as_type<uint>(as_type<float>(v31) - as_type<float>(v54));
    uint v56 = as_type<float>(v54) <  as_type<float>(v11) ? 0xffffffffu : 0u;
    uint v57 = as_type<uint>(as_type<float>(v25) - as_type<float>(v54));
    uint v58 = as_type<uint>(as_type<float>(v19) - as_type<float>(v54));
    uint v59 = as_type<uint>(as_type<float>(v57) - as_type<float>(v58));
    uint v60 = as_type<uint>(as_type<float>(v59) * as_type<float>(1073741824u));
    uint v61 = as_type<uint>(as_type<float>(v53) - as_type<float>(v52));
    uint v62 = as_type<float>(v0) <= as_type<float>(v54) ? 0xffffffffu : 0u;
    uint v63 = v62 & v56;
    uint v64 = as_type<float>(v0) <= as_type<float>(v46) ? 0xffffffffu : 0u;
    uint v65 = v64 & v51;
    uint v66 = v63 & v65;
    uint v67 = as_type<uint>(as_type<float>(v52) * as_type<float>(1073741824u));
    uint v68 = as_type<uint>(as_type<float>(v53) - as_type<float>(v67));
    uint v69 = as_type<uint>(as_type<float>(v47) + as_type<float>(v68));
    uint v70 = as_type<uint>(as_type<float>(v61) * as_type<float>(v61));
    uint v71 = as_type<uint>(fma(-as_type<float>(v53), as_type<float>(v69), as_type<float>(v70)));
    uint v72 = as_type<uint>(max(as_type<float>(v71), as_type<float>(0u)));
    uint v73 = as_type<uint>(sqrt(as_type<float>(v72)));
    uint v74 = as_type<uint>(as_type<float>(v61) + as_type<float>(v73));
    uint v75 = as_type<uint>(fabs(as_type<float>(v69)));
    uint v76 = as_type<float>(v15) <  as_type<float>(v75) ? 0xffffffffu : 0u;
    uint v77 = (v76 & v69) | (~v76 & v13);
    uint v78 = as_type<uint>(as_type<float>(v13) / as_type<float>(v77));
    uint v79 = as_type<uint>(as_type<float>(v78) * as_type<float>(v74));
    uint v80 = as_type<uint>(as_type<float>(v69) * as_type<float>(v79));
    uint v81 = as_type<uint>(as_type<float>(v80) - as_type<float>(v61));
    uint v82 = as_type<float>(v81) <  as_type<float>(0u) ? 0xffffffffu : 0u;
    uint v83 = (v82 & v35) | (~v82 & v0);
    uint v84 = as_type<uint>(as_type<float>(v61) - as_type<float>(v73));
    uint v85 = as_type<uint>(as_type<float>(v78) * as_type<float>(v84));
    uint v86 = as_type<uint>(fabs(as_type<float>(v61)));
    uint v87 = as_type<float>(v15) <  as_type<float>(v86) ? 0xffffffffu : 0u;
    uint v88 = as_type<uint>(as_type<float>(v61) * as_type<float>(1073741824u));
    uint v89 = (v87 & v88) | (~v87 & v13);
    uint v90 = as_type<uint>(as_type<float>(v53) / as_type<float>(v89));
    uint v91 = (v76 & v85) | (~v76 & v90);
    uint v92 = as_type<uint>(as_type<float>(v69) * as_type<float>(v91));
    uint v93 = as_type<uint>(as_type<float>(v92) - as_type<float>(v61));
    uint v94 = as_type<float>(v93) <  as_type<float>(0u) ? 0xffffffffu : 0u;
    uint v95 = (v94 & v35) | (~v94 & v0);
    uint v96 = as_type<float>(v0) <= as_type<float>(v91) ? 0xffffffffu : 0u;
    uint v97 = as_type<float>(v91) <  as_type<float>(1065353216u) ? 0xffffffffu : 0u;
    uint v98 = v96 & v97;
    uint v99 = as_type<float>(v0) <= as_type<float>(v71) ? 0xffffffffu : 0u;
    uint v100 = v99 & v76;
    uint v101 = v99 & v98;
    uint v102 = as_type<float>(v79) <  as_type<float>(1065353216u) ? 0xffffffffu : 0u;
    uint v103 = as_type<float>(v0) <= as_type<float>(v79) ? 0xffffffffu : 0u;
    uint v104 = v103 & v102;
    uint v105 = v100 & v104;
    uint v106 = as_type<uint>(as_type<float>(v57) * as_type<float>(1073741824u));
    uint v107 = as_type<uint>(as_type<float>(v58) - as_type<float>(v106));
    uint v108 = as_type<uint>(as_type<float>(v55) + as_type<float>(v107));
    uint v109 = as_type<uint>(fma(as_type<float>(v79), as_type<float>(v108), as_type<float>(v60)));
    uint v110 = as_type<uint>(fma(as_type<float>(v79), as_type<float>(v109), as_type<float>(v58)));
    uint v111 = as_type<float>(v0) <  as_type<float>(v110) ? 0xffffffffu : 0u;
    uint v112 = v105 & v111;
    uint v113 = as_type<uint>(fma(as_type<float>(v91), as_type<float>(v108), as_type<float>(v60)));
    uint v114 = as_type<uint>(fma(as_type<float>(v91), as_type<float>(v113), as_type<float>(v58)));
    uint v115 = as_type<float>(v0) <  as_type<float>(v114) ? 0xffffffffu : 0u;
    uint v116 = v101 & v115;
    uint v117 = as_type<float>(v0) <  as_type<float>(v93) ? 0xffffffffu : 0u;
    uint v118 = (v117 & v13) | (~v117 & v95);
    uint v119 = (v116 & v118) | (~v116 & v0);
    uint v120 = as_type<float>(v0) <  as_type<float>(v81) ? 0xffffffffu : 0u;
    uint v121 = (v120 & v13) | (~v120 & v83);
    uint v122 = (v112 & v121) | (~v112 & v0);
    uint v123 = as_type<uint>(as_type<float>(v119) + as_type<float>(v122));
    uint v124 = (v66 & v123) | (~v66 & v0);
    uint v125 = ((device uint*)p1)[i];
    uint v126 = as_type<uint>(as_type<float>(v124) + as_type<float>(v125));
    ((device uint*)p1)[i] = v126;
}
