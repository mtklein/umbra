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
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    device uchar *p2 [[buffer(3)]],
    constant uint *buf_szs [[buffer(4)]],
    uint2 pos [[thread_position_in_grid]]
) {
    uint i = pos.y * w + pos.x;
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
    uint v14 = 931135488u;
    uint v15 = ((device const uint*)p0)[16];
    uint v16 = v15 * 6u;
    uint v17 = ((device uint*)p2)[safe_ix((int)v16,buf_szs[2],4)] & oob_mask((int)v16,buf_szs[2],4);
    uint v18 = v16 + 1u;
    uint v19 = ((device uint*)p2)[safe_ix((int)v18,buf_szs[2],4)] & oob_mask((int)v18,buf_szs[2],4);
    uint v20 = v16 + 2u;
    uint v21 = ((device uint*)p2)[safe_ix((int)v20,buf_szs[2],4)] & oob_mask((int)v20,buf_szs[2],4);
    uint v22 = v16 + 3u;
    uint v23 = ((device uint*)p2)[safe_ix((int)v22,buf_szs[2],4)] & oob_mask((int)v22,buf_szs[2],4);
    uint v24 = v16 + 4u;
    uint v25 = ((device uint*)p2)[safe_ix((int)v24,buf_szs[2],4)] & oob_mask((int)v24,buf_szs[2],4);
    uint v26 = v16 + 5u;
    uint v27 = ((device uint*)p2)[safe_ix((int)v26,buf_szs[2],4)] & oob_mask((int)v26,buf_szs[2],4);
    uint v28 = 3212836864u;
    uint v29 = pos.x;
    uint v30 = as_type<uint>((float)(int)v29);
    uint v31 = pos.y;
    uint v32 = as_type<uint>((float)(int)v31);
    uint v33 = as_type<uint>(as_type<float>(v32) * as_type<float>(v6));
    uint v34 = as_type<uint>(fma(as_type<float>(v30), as_type<float>(v5), as_type<float>(v33)));
    uint v35 = as_type<uint>(as_type<float>(v7) + as_type<float>(v34));
    uint v36 = as_type<uint>(as_type<float>(v32) * as_type<float>(v9));
    uint v37 = as_type<uint>(fma(as_type<float>(v30), as_type<float>(v8), as_type<float>(v36)));
    uint v38 = as_type<uint>(as_type<float>(v10) + as_type<float>(v37));
    uint v39 = as_type<uint>(as_type<float>(v35) / as_type<float>(v38));
    uint v40 = as_type<uint>(as_type<float>(v27) - as_type<float>(v39));
    uint v41 = as_type<uint>(as_type<float>(v32) * as_type<float>(v3));
    uint v42 = as_type<uint>(fma(as_type<float>(v30), as_type<float>(v2), as_type<float>(v41)));
    uint v43 = as_type<uint>(as_type<float>(v4) + as_type<float>(v42));
    uint v44 = as_type<float>(v39) <  as_type<float>(v12) ? 0xffffffffu : 0u;
    uint v45 = as_type<uint>(as_type<float>(v23) - as_type<float>(v39));
    uint v46 = as_type<uint>(as_type<float>(v19) - as_type<float>(v39));
    uint v47 = as_type<uint>(as_type<float>(v43) / as_type<float>(v38));
    uint v48 = as_type<uint>(as_type<float>(v25) - as_type<float>(v47));
    uint v49 = as_type<float>(v47) <  as_type<float>(v11) ? 0xffffffffu : 0u;
    uint v50 = as_type<uint>(as_type<float>(v21) - as_type<float>(v47));
    uint v51 = as_type<uint>(as_type<float>(v17) - as_type<float>(v47));
    uint v52 = as_type<uint>(as_type<float>(v50) - as_type<float>(v51));
    uint v53 = as_type<uint>(as_type<float>(v52) * as_type<float>(1073741824u));
    uint v54 = as_type<uint>(as_type<float>(v46) - as_type<float>(v45));
    uint v55 = as_type<float>(v0) <= as_type<float>(v47) ? 0xffffffffu : 0u;
    uint v56 = v55 & v49;
    uint v57 = as_type<float>(v0) <= as_type<float>(v39) ? 0xffffffffu : 0u;
    uint v58 = v57 & v44;
    uint v59 = v56 & v58;
    uint v60 = as_type<uint>(as_type<float>(v45) * as_type<float>(1073741824u));
    uint v61 = as_type<uint>(as_type<float>(v46) - as_type<float>(v60));
    uint v62 = as_type<uint>(as_type<float>(v40) + as_type<float>(v61));
    uint v63 = as_type<uint>(as_type<float>(v54) * as_type<float>(v54));
    uint v64 = as_type<uint>(as_type<float>(v63) - as_type<float>(v46) * as_type<float>(v62));
    uint v65 = as_type<uint>(max(as_type<float>(v64), as_type<float>(0u)));
    uint v66 = as_type<uint>(sqrt(as_type<float>(v65)));
    uint v67 = as_type<uint>(as_type<float>(v54) + as_type<float>(v66));
    uint v68 = as_type<uint>(fabs(as_type<float>(v62)));
    uint v69 = as_type<float>(v14) <  as_type<float>(v68) ? 0xffffffffu : 0u;
    uint v70 = (v69 & v62) | (~v69 & v13);
    uint v71 = as_type<uint>(as_type<float>(v13) / as_type<float>(v70));
    uint v72 = as_type<uint>(as_type<float>(v71) * as_type<float>(v67));
    uint v73 = as_type<uint>(as_type<float>(v62) * as_type<float>(v72));
    uint v74 = as_type<uint>(as_type<float>(v73) - as_type<float>(v54));
    uint v75 = as_type<float>(v74) <  as_type<float>(0u) ? 0xffffffffu : 0u;
    uint v76 = (v75 & v28) | (~v75 & v0);
    uint v77 = as_type<uint>(as_type<float>(v54) - as_type<float>(v66));
    uint v78 = as_type<uint>(as_type<float>(v71) * as_type<float>(v77));
    uint v79 = as_type<uint>(fabs(as_type<float>(v54)));
    uint v80 = as_type<float>(v14) <  as_type<float>(v79) ? 0xffffffffu : 0u;
    uint v81 = as_type<uint>(as_type<float>(v54) * as_type<float>(1073741824u));
    uint v82 = (v80 & v81) | (~v80 & v13);
    uint v83 = as_type<uint>(as_type<float>(v46) / as_type<float>(v82));
    uint v84 = (v69 & v78) | (~v69 & v83);
    uint v85 = as_type<uint>(as_type<float>(v62) * as_type<float>(v84));
    uint v86 = as_type<uint>(as_type<float>(v85) - as_type<float>(v54));
    uint v87 = as_type<float>(v86) <  as_type<float>(0u) ? 0xffffffffu : 0u;
    uint v88 = (v87 & v28) | (~v87 & v0);
    uint v89 = as_type<float>(v0) <= as_type<float>(v84) ? 0xffffffffu : 0u;
    uint v90 = as_type<float>(v84) <  as_type<float>(1065353216u) ? 0xffffffffu : 0u;
    uint v91 = v89 & v90;
    uint v92 = as_type<float>(v0) <= as_type<float>(v64) ? 0xffffffffu : 0u;
    uint v93 = v92 & v69;
    uint v94 = v92 & v91;
    uint v95 = as_type<float>(v72) <  as_type<float>(1065353216u) ? 0xffffffffu : 0u;
    uint v96 = as_type<float>(v0) <= as_type<float>(v72) ? 0xffffffffu : 0u;
    uint v97 = v96 & v95;
    uint v98 = v93 & v97;
    uint v99 = as_type<uint>(as_type<float>(v50) * as_type<float>(1073741824u));
    uint v100 = as_type<uint>(as_type<float>(v51) - as_type<float>(v99));
    uint v101 = as_type<uint>(as_type<float>(v48) + as_type<float>(v100));
    uint v102 = as_type<uint>(fma(as_type<float>(v72), as_type<float>(v101), as_type<float>(v53)));
    uint v103 = as_type<uint>(fma(as_type<float>(v72), as_type<float>(v102), as_type<float>(v51)));
    uint v104 = as_type<float>(v0) <  as_type<float>(v103) ? 0xffffffffu : 0u;
    uint v105 = v98 & v104;
    uint v106 = as_type<uint>(fma(as_type<float>(v84), as_type<float>(v101), as_type<float>(v53)));
    uint v107 = as_type<uint>(fma(as_type<float>(v84), as_type<float>(v106), as_type<float>(v51)));
    uint v108 = as_type<float>(v0) <  as_type<float>(v107) ? 0xffffffffu : 0u;
    uint v109 = v94 & v108;
    uint v110 = as_type<float>(v0) <  as_type<float>(v86) ? 0xffffffffu : 0u;
    uint v111 = (v110 & v13) | (~v110 & v88);
    uint v112 = (v109 & v111) | (~v109 & v0);
    uint v113 = as_type<float>(v0) <  as_type<float>(v74) ? 0xffffffffu : 0u;
    uint v114 = (v113 & v13) | (~v113 & v76);
    uint v115 = (v105 & v114) | (~v105 & v0);
    uint v116 = as_type<uint>(as_type<float>(v112) + as_type<float>(v115));
    uint v117 = (v59 & v116) | (~v59 & v0);
    uint v118 = ((device uint*)p1)[i];
    uint v119 = as_type<uint>(as_type<float>(v117) + as_type<float>(v118));
    ((device uint*)p1)[i] = v119;
}
