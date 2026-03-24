#include <metal_stdlib>
using namespace metal;

static inline int clamp_ix(int ix, uint bytes, int elem) {
    int hi = (int)(bytes / (uint)elem) - 1;
    if (hi < 0) hi = 0;
    return clamp(ix, 0, hi);
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
    uint v45 = as_type<uint>(fma(as_type<float>(v44), as_type<float>(v13), as_type<float>(v24)));
    uint v46 = as_type<uint>(fma(as_type<float>(v44), as_type<float>(v17), as_type<float>(v22)));
    uint v47 = as_type<uint>(fma(as_type<float>(v44), as_type<float>(v7), as_type<float>(v23)));
    uint v48 = as_type<uint>(as_type<float>(v11) + as_type<float>(v47));
    uint v49 = as_type<uint>(as_type<float>(v16) + as_type<float>(v45));
    uint v50 = as_type<uint>(as_type<float>(v19) + as_type<float>(v46));
    uint v51 = as_type<uint>(as_type<float>(v49) / as_type<float>(v50));
    uint v52 = as_type<uint>(as_type<float>(v40) - as_type<float>(v51));
    uint v53 = as_type<float>(v51) <  as_type<float>(v21) ? 0xffffffffu : 0u;
    uint v54 = as_type<uint>(as_type<float>(v36) - as_type<float>(v51));
    uint v55 = as_type<uint>(as_type<float>(v32) - as_type<float>(v51));
    uint v56 = as_type<uint>(as_type<float>(v48) / as_type<float>(v50));
    uint v57 = as_type<uint>(as_type<float>(v38) - as_type<float>(v56));
    uint v58 = as_type<float>(v56) <  as_type<float>(v20) ? 0xffffffffu : 0u;
    uint v59 = as_type<uint>(as_type<float>(v34) - as_type<float>(v56));
    uint v60 = as_type<uint>(as_type<float>(v30) - as_type<float>(v56));
    uint v61 = as_type<uint>(as_type<float>(v59) - as_type<float>(v60));
    uint v62 = as_type<uint>(as_type<float>(v26) * as_type<float>(v61));
    uint v63 = as_type<uint>(as_type<float>(v55) - as_type<float>(v54));
    uint v64 = as_type<float>(v0) <= as_type<float>(v56) ? 0xffffffffu : 0u;
    uint v65 = v64 & v58;
    uint v66 = as_type<float>(v0) <= as_type<float>(v51) ? 0xffffffffu : 0u;
    uint v67 = v66 & v53;
    uint v68 = v65 & v67;
    uint v69 = as_type<uint>(as_type<float>(v26) * as_type<float>(v54));
    uint v70 = as_type<uint>(as_type<float>(v55) - as_type<float>(v69));
    uint v71 = as_type<uint>(as_type<float>(v52) + as_type<float>(v70));
    uint v72 = as_type<uint>(as_type<float>(v63) * as_type<float>(v63));
    uint v73 = as_type<uint>(as_type<float>(v72) - as_type<float>(v55) * as_type<float>(v71));
    uint v74 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v73)));
    uint v75 = as_type<uint>(sqrt(as_type<float>(v74)));
    uint v76 = as_type<uint>(as_type<float>(v63) + as_type<float>(v75));
    uint v77 = as_type<uint>(fabs(as_type<float>(v71)));
    uint v78 = as_type<float>(v27) <  as_type<float>(v77) ? 0xffffffffu : 0u;
    uint v79 = (v78 & v71) | (~v78 & v25);
    uint v80 = as_type<uint>(as_type<float>(v25) / as_type<float>(v79));
    uint v81 = as_type<uint>(as_type<float>(v80) * as_type<float>(v76));
    uint v82 = as_type<uint>(as_type<float>(v71) * as_type<float>(v81));
    uint v83 = as_type<uint>(as_type<float>(v82) - as_type<float>(v63));
    uint v84 = as_type<uint>(as_type<float>(v63) - as_type<float>(v75));
    uint v85 = as_type<uint>(as_type<float>(v80) * as_type<float>(v84));
    uint v86 = as_type<uint>(fabs(as_type<float>(v63)));
    uint v87 = as_type<float>(v27) <  as_type<float>(v86) ? 0xffffffffu : 0u;
    uint v88 = as_type<uint>(as_type<float>(v26) * as_type<float>(v63));
    uint v89 = (v87 & v88) | (~v87 & v25);
    uint v90 = as_type<uint>(as_type<float>(v55) / as_type<float>(v89));
    uint v91 = (v78 & v85) | (~v78 & v90);
    uint v92 = as_type<uint>(as_type<float>(v71) * as_type<float>(v91));
    uint v93 = as_type<uint>(as_type<float>(v92) - as_type<float>(v63));
    uint v94 = as_type<float>(v91) <  as_type<float>(v25) ? 0xffffffffu : 0u;
    uint v95 = as_type<float>(v0) <= as_type<float>(v91) ? 0xffffffffu : 0u;
    uint v96 = v95 & v94;
    uint v97 = as_type<float>(v0) <= as_type<float>(v73) ? 0xffffffffu : 0u;
    uint v98 = v97 & v78;
    uint v99 = v97 & v96;
    uint v100 = as_type<float>(v81) <  as_type<float>(v25) ? 0xffffffffu : 0u;
    uint v101 = as_type<float>(v0) <= as_type<float>(v81) ? 0xffffffffu : 0u;
    uint v102 = v101 & v100;
    uint v103 = v98 & v102;
    uint v104 = as_type<uint>(as_type<float>(v26) * as_type<float>(v59));
    uint v105 = as_type<uint>(as_type<float>(v60) - as_type<float>(v104));
    uint v106 = as_type<uint>(as_type<float>(v57) + as_type<float>(v105));
    uint v107 = as_type<uint>(fma(as_type<float>(v81), as_type<float>(v106), as_type<float>(v62)));
    uint v108 = as_type<uint>(fma(as_type<float>(v81), as_type<float>(v107), as_type<float>(v60)));
    uint v109 = as_type<float>(v0) <  as_type<float>(v108) ? 0xffffffffu : 0u;
    uint v110 = v103 & v109;
    uint v111 = as_type<uint>(fma(as_type<float>(v91), as_type<float>(v106), as_type<float>(v62)));
    uint v112 = as_type<uint>(fma(as_type<float>(v91), as_type<float>(v111), as_type<float>(v60)));
    uint v113 = as_type<float>(v0) <  as_type<float>(v112) ? 0xffffffffu : 0u;
    uint v114 = v99 & v113;
    uint v115 = as_type<float>(v93) <  as_type<float>(v0) ? 0xffffffffu : 0u;
    uint v116 = (v115 & v41) | (~v115 & v0);
    uint v117 = as_type<float>(v0) <  as_type<float>(v93) ? 0xffffffffu : 0u;
    uint v118 = (v117 & v25) | (~v117 & v116);
    uint v119 = (v114 & v118) | (~v114 & v0);
    uint v120 = as_type<float>(v83) <  as_type<float>(v0) ? 0xffffffffu : 0u;
    uint v121 = (v120 & v41) | (~v120 & v0);
    uint v122 = as_type<float>(v0) <  as_type<float>(v83) ? 0xffffffffu : 0u;
    uint v123 = (v122 & v25) | (~v122 & v121);
    uint v124 = (v110 & v123) | (~v110 & v0);
    uint v125 = as_type<uint>(as_type<float>(v119) + as_type<float>(v124));
    uint v126 = (v68 & v125) | (~v68 & v0);
    uint v127 = ((device uint*)p0)[i];
    uint v128 = as_type<uint>(as_type<float>(v126) + as_type<float>(v127));
    ((device uint*)p0)[i] = v128;
}
