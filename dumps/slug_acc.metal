#include <metal_stdlib>
using namespace metal;

static inline int clamp_ix(int ix, uint bytes, int elem) {
    int hi = (int)(bytes / (uint)elem) - 1;
    if (hi < 0) hi = 0;
    return clamp(ix, 0, hi);
}

kernel void umbra_entry(
    constant uint &n [[buffer(0)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    device uchar *p2 [[buffer(3)]],
    constant uint *buf_szs [[buffer(4)]],
    uint i [[thread_position_in_grid]]
) {
    if (i >= n) return;
    uint v0 = 0u;
    uint v2 = ((device const uint*)p1)[0];
    uint v3 = 1u;
    uint v4 = ((device const uint*)p1)[1];
    uint v5 = as_type<uint>((float)(int)v4);
    uint v6 = 2u;
    uint v7 = ((device const uint*)p1)[2];
    uint v8 = 3u;
    uint v9 = ((device const uint*)p1)[3];
    uint v10 = 4u;
    uint v11 = ((device const uint*)p1)[4];
    uint v12 = 5u;
    uint v13 = ((device const uint*)p1)[5];
    uint v14 = 6u;
    uint v15 = ((device const uint*)p1)[6];
    uint v16 = ((device const uint*)p1)[7];
    uint v17 = ((device const uint*)p1)[8];
    uint v18 = ((device const uint*)p1)[9];
    uint v19 = ((device const uint*)p1)[10];
    uint v20 = ((device const uint*)p1)[11];
    uint v21 = ((device const uint*)p1)[12];
    uint v22 = as_type<uint>(as_type<float>(v5) * as_type<float>(v18));
    uint v23 = as_type<uint>(as_type<float>(v5) * as_type<float>(v9));
    uint v24 = as_type<uint>(as_type<float>(v5) * as_type<float>(v15));
    uint v25 = 1065353216u;
    uint v26 = 1073741824u;
    uint v27 = 931135488u;
    uint v28 = ((device const uint*)p1)[18];
    uint v29 = v14 * v28;
    uint v30 = ((device const uint*)p2)[v29];
    uint v31 = v3 + v29;
    uint v32 = ((device const uint*)p2)[v31];
    uint v33 = v6 + v29;
    uint v34 = ((device const uint*)p2)[v33];
    uint v35 = v8 + v29;
    uint v36 = ((device const uint*)p2)[v35];
    uint v37 = v10 + v29;
    uint v38 = ((device const uint*)p2)[v37];
    uint v39 = v12 + v29;
    uint v40 = ((device const uint*)p2)[v39];
    uint v41 = 3212836864u;
    uint v42 = i;
    uint v43 = v42 + v2;
    uint v44 = as_type<uint>((float)(int)v43);
    uint v45 = as_type<uint>(fma(as_type<float>(v44), as_type<float>(v17), as_type<float>(v22)));
    uint v46 = as_type<uint>(as_type<float>(v19) + as_type<float>(v45));
    uint v47 = as_type<uint>(fma(as_type<float>(v44), as_type<float>(v7), as_type<float>(v23)));
    uint v48 = as_type<uint>(as_type<float>(v11) + as_type<float>(v47));
    uint v49 = as_type<uint>(as_type<float>(v48) / as_type<float>(v46));
    uint v50 = as_type<float>(v0) <= as_type<float>(v49) ? 0xffffffffu : 0u;
    uint v51 = as_type<float>(v49) <  as_type<float>(v20) ? 0xffffffffu : 0u;
    uint v52 = v50 & v51;
    uint v53 = as_type<uint>(fma(as_type<float>(v44), as_type<float>(v13), as_type<float>(v24)));
    uint v54 = as_type<uint>(as_type<float>(v16) + as_type<float>(v53));
    uint v55 = as_type<uint>(as_type<float>(v54) / as_type<float>(v46));
    uint v56 = as_type<float>(v0) <= as_type<float>(v55) ? 0xffffffffu : 0u;
    uint v57 = as_type<float>(v55) <  as_type<float>(v21) ? 0xffffffffu : 0u;
    uint v58 = v56 & v57;
    uint v59 = v52 & v58;
    uint v60 = as_type<uint>(as_type<float>(v32) - as_type<float>(v55));
    uint v61 = as_type<uint>(as_type<float>(v36) - as_type<float>(v55));
    uint v62 = as_type<uint>(as_type<float>(v60) - as_type<float>(v61));
    uint v63 = as_type<uint>(as_type<float>(v40) - as_type<float>(v55));
    uint v64 = as_type<uint>(as_type<float>(v60) - as_type<float>(v26) * as_type<float>(v61));
    uint v65 = as_type<uint>(as_type<float>(v63) + as_type<float>(v64));
    uint v66 = as_type<uint>(as_type<float>(v0) - as_type<float>(v65));
    uint v67 = as_type<uint>(max(as_type<float>(v65), as_type<float>(v66)));
    uint v68 = as_type<float>(v27) <  as_type<float>(v67) ? 0xffffffffu : 0u;
    uint v69 = (v68 & v65) | (~v68 & v25);
    uint v70 = as_type<uint>(as_type<float>(v25) / as_type<float>(v69));
    uint v71 = as_type<uint>(as_type<float>(v62) * as_type<float>(v62));
    uint v72 = as_type<uint>(as_type<float>(v71) - as_type<float>(v60) * as_type<float>(v65));
    uint v73 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v72)));
    uint v74 = as_type<uint>(sqrt(as_type<float>(v73)));
    uint v75 = as_type<uint>(as_type<float>(v62) - as_type<float>(v74));
    uint v76 = as_type<uint>(as_type<float>(v70) * as_type<float>(v75));
    uint v77 = as_type<uint>(as_type<float>(v0) - as_type<float>(v62));
    uint v78 = as_type<uint>(max(as_type<float>(v62), as_type<float>(v77)));
    uint v79 = as_type<float>(v27) <  as_type<float>(v78) ? 0xffffffffu : 0u;
    uint v80 = as_type<uint>(as_type<float>(v26) * as_type<float>(v62));
    uint v81 = (v79 & v80) | (~v79 & v25);
    uint v82 = as_type<uint>(as_type<float>(v60) / as_type<float>(v81));
    uint v83 = (v68 & v76) | (~v68 & v82);
    uint v84 = as_type<uint>(as_type<float>(v65) * as_type<float>(v83));
    uint v85 = as_type<uint>(as_type<float>(v84) - as_type<float>(v62));
    uint v86 = as_type<float>(v0) <  as_type<float>(v85) ? 0xffffffffu : 0u;
    uint v87 = as_type<float>(v85) <  as_type<float>(v0) ? 0xffffffffu : 0u;
    uint v88 = (v87 & v41) | (~v87 & v0);
    uint v89 = (v86 & v25) | (~v86 & v88);
    uint v90 = as_type<float>(v0) <= as_type<float>(v72) ? 0xffffffffu : 0u;
    uint v91 = as_type<float>(v0) <= as_type<float>(v83) ? 0xffffffffu : 0u;
    uint v92 = as_type<float>(v83) <  as_type<float>(v25) ? 0xffffffffu : 0u;
    uint v93 = v91 & v92;
    uint v94 = v90 & v93;
    uint v95 = as_type<uint>(as_type<float>(v30) - as_type<float>(v49));
    uint v96 = as_type<uint>(as_type<float>(v34) - as_type<float>(v49));
    uint v97 = as_type<uint>(as_type<float>(v96) - as_type<float>(v95));
    uint v98 = as_type<uint>(as_type<float>(v38) - as_type<float>(v49));
    uint v99 = as_type<uint>(as_type<float>(v95) - as_type<float>(v26) * as_type<float>(v96));
    uint v100 = as_type<uint>(as_type<float>(v98) + as_type<float>(v99));
    uint v101 = as_type<uint>(as_type<float>(v83) * as_type<float>(v100));
    uint v102 = as_type<uint>(fma(as_type<float>(v26), as_type<float>(v97), as_type<float>(v101)));
    uint v103 = as_type<uint>(fma(as_type<float>(v83), as_type<float>(v102), as_type<float>(v95)));
    uint v104 = as_type<float>(v0) <  as_type<float>(v103) ? 0xffffffffu : 0u;
    uint v105 = v94 & v104;
    uint v106 = (v105 & v89) | (~v105 & v0);
    uint v107 = as_type<uint>(as_type<float>(v62) + as_type<float>(v74));
    uint v108 = as_type<uint>(as_type<float>(v70) * as_type<float>(v107));
    uint v109 = as_type<uint>(as_type<float>(v65) * as_type<float>(v108));
    uint v110 = as_type<uint>(as_type<float>(v109) - as_type<float>(v62));
    uint v111 = as_type<float>(v0) <  as_type<float>(v110) ? 0xffffffffu : 0u;
    uint v112 = as_type<float>(v110) <  as_type<float>(v0) ? 0xffffffffu : 0u;
    uint v113 = (v112 & v41) | (~v112 & v0);
    uint v114 = (v111 & v25) | (~v111 & v113);
    uint v115 = v90 & v68;
    uint v116 = as_type<float>(v0) <= as_type<float>(v108) ? 0xffffffffu : 0u;
    uint v117 = as_type<float>(v108) <  as_type<float>(v25) ? 0xffffffffu : 0u;
    uint v118 = v116 & v117;
    uint v119 = v115 & v118;
    uint v120 = as_type<uint>(as_type<float>(v108) * as_type<float>(v100));
    uint v121 = as_type<uint>(fma(as_type<float>(v26), as_type<float>(v97), as_type<float>(v120)));
    uint v122 = as_type<uint>(fma(as_type<float>(v108), as_type<float>(v121), as_type<float>(v95)));
    uint v123 = as_type<float>(v0) <  as_type<float>(v122) ? 0xffffffffu : 0u;
    uint v124 = v119 & v123;
    uint v125 = (v124 & v114) | (~v124 & v0);
    uint v126 = as_type<uint>(as_type<float>(v106) + as_type<float>(v125));
    uint v127 = (v59 & v126) | (~v59 & v0);
    uint v128 = ((device uint*)p0)[i];
    uint v129 = as_type<uint>(as_type<float>(v127) + as_type<float>(v128));
    ((device uint*)p0)[i] = v129;
}
