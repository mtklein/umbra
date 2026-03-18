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
    uint v1 = ((device const uint*)p1)[0];
    uint v2 = ((device const uint*)p1)[1];
    uint v3 = as_type<uint>((float)(int)v2);
    uint v4 = ((device const uint*)p1)[2];
    uint v5 = ((device const uint*)p1)[3];
    uint v6 = ((device const uint*)p1)[4];
    uint v7 = ((device const uint*)p1)[5];
    uint v9 = ((device const uint*)p1)[6];
    uint v10 = ((device const uint*)p1)[7];
    uint v11 = ((device const uint*)p1)[8];
    uint v12 = ((device const uint*)p1)[9];
    uint v13 = ((device const uint*)p1)[10];
    uint v14 = ((device const uint*)p1)[11];
    uint v15 = ((device const uint*)p1)[12];
    uint v16 = ((device const uint*)p1)[13];
    uint v17 = ((device const uint*)p1)[14];
    uint v18 = ((device const uint*)p1)[15];
    uint v19 = ((device const uint*)p1)[16];
    uint v20 = as_type<uint>(as_type<float>(v3) * as_type<float>(v16));
    uint v21 = as_type<uint>(as_type<float>(v3) * as_type<float>(v10));
    uint v22 = as_type<uint>(as_type<float>(v3) * as_type<float>(v13));
    uint v23 = 1065353216u;
    uint v24 = as_type<uint>(as_type<float>(v18) - as_type<float>(v23));
    uint v25 = as_type<uint>(as_type<float>(v19) - as_type<float>(v23));
    uint v26 = (uint)(int)as_type<float>(v18);
    uint v27 = buf_szs[2] >> 1u;
    uint v28 = 998277249u;
    uint v29 = buf_szs[0] >> 2u;
    uint v30 = 255u;
    uint v31 = as_type<uint>(as_type<float>(v23) - as_type<float>(v7));
    uint v32 = 1132396544u;
    uint v33 = 1056964608u;
    uint v34 = 0xffffffffu;
    uint v35 = i;
    uint v36 = v35 <  v29 ? 0xffffffffu : 0u;
    uint v37 = v34 & v36;
    uint v38 = ((device uint*)p0)[i];
    uint v39 = v38 & v30;
    uint v40 = as_type<uint>((float)(int)v39);
    uint v41 = v35 + v1;
    uint v42 = as_type<uint>((float)(int)v41);
    uint v43 = as_type<uint>(fma(as_type<float>(v42), as_type<float>(v15), as_type<float>(v20)));
    uint v44 = as_type<uint>(as_type<float>(v17) + as_type<float>(v43));
    uint v45 = as_type<uint>(fma(as_type<float>(v42), as_type<float>(v9), as_type<float>(v21)));
    uint v46 = as_type<uint>(as_type<float>(v11) + as_type<float>(v45));
    uint v47 = as_type<uint>(as_type<float>(v46) / as_type<float>(v44));
    uint v48 = as_type<float>(v0) <= as_type<float>(v47) ? 0xffffffffu : 0u;
    uint v49 = as_type<float>(v47) <  as_type<float>(v18) ? 0xffffffffu : 0u;
    uint v50 = v48 & v49;
    uint v51 = as_type<uint>(fma(as_type<float>(v42), as_type<float>(v12), as_type<float>(v22)));
    uint v52 = as_type<uint>(as_type<float>(v14) + as_type<float>(v51));
    uint v53 = as_type<uint>(as_type<float>(v52) / as_type<float>(v44));
    uint v54 = as_type<float>(v0) <= as_type<float>(v53) ? 0xffffffffu : 0u;
    uint v55 = as_type<float>(v53) <  as_type<float>(v19) ? 0xffffffffu : 0u;
    uint v56 = v54 & v55;
    uint v57 = v50 & v56;
    uint v58 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v47)));
    uint v59 = as_type<uint>(min(as_type<float>(v58), as_type<float>(v24)));
    uint v60 = (uint)(int)as_type<float>(v59);
    uint v61 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v53)));
    uint v62 = as_type<uint>(min(as_type<float>(v61), as_type<float>(v25)));
    uint v63 = (uint)(int)as_type<float>(v62);
    uint v64 = v63 * v26;
    uint v65 = v60 + v64;
    uint v66 = v65 <  v27 ? 0xffffffffu : 0u;
    uint v67 = v34 & v66;
    uint v68 = v67 ? (uint)((device ushort*)p2)[(int)v65] : 0u;
    uint v69 = (uint)(int)(short)(ushort)v68;
    uint v70 = as_type<uint>((float)(int)v69);
    uint v71 = as_type<uint>(as_type<float>(v28) * as_type<float>(v70));
    uint v72 = (v57 & v71) | (~v57 & v0);
    uint v73 = as_type<uint>(as_type<float>(v28) * as_type<float>(v40));
    uint v74 = as_type<uint>(fma(as_type<float>(v73), as_type<float>(v31), as_type<float>(v4)));
    uint v75 = as_type<uint>(as_type<float>(v74) - as_type<float>(v28) * as_type<float>(v40));
    uint v76 = as_type<uint>(as_type<float>(v72) * as_type<float>(v75));
    uint v77 = as_type<uint>(fma(as_type<float>(v28), as_type<float>(v40), as_type<float>(v76)));
    uint v78 = as_type<uint>(fma(as_type<float>(v77), as_type<float>(v32), as_type<float>(v33)));
    uint v79 = (uint)(int)as_type<float>(v78);
    uint v80 = v30 & v79;
    uint v81 = v38 >> 8u;
    uint v82 = v30 & v81;
    uint v83 = as_type<uint>((float)(int)v82);
    uint v84 = as_type<uint>(as_type<float>(v28) * as_type<float>(v83));
    uint v85 = as_type<uint>(fma(as_type<float>(v84), as_type<float>(v31), as_type<float>(v5)));
    uint v86 = as_type<uint>(as_type<float>(v85) - as_type<float>(v28) * as_type<float>(v83));
    uint v87 = as_type<uint>(as_type<float>(v72) * as_type<float>(v86));
    uint v88 = as_type<uint>(fma(as_type<float>(v28), as_type<float>(v83), as_type<float>(v87)));
    uint v89 = as_type<uint>(fma(as_type<float>(v88), as_type<float>(v32), as_type<float>(v33)));
    uint v90 = (uint)(int)as_type<float>(v89);
    uint v91 = v30 & v90;
    uint v92 = v91 << 8u;
    uint v93 = v80 | v92;
    uint v94 = v38 >> 16u;
    uint v95 = v30 & v94;
    uint v96 = as_type<uint>((float)(int)v95);
    uint v97 = as_type<uint>(as_type<float>(v28) * as_type<float>(v96));
    uint v98 = as_type<uint>(fma(as_type<float>(v97), as_type<float>(v31), as_type<float>(v6)));
    uint v99 = as_type<uint>(as_type<float>(v98) - as_type<float>(v28) * as_type<float>(v96));
    uint v100 = as_type<uint>(as_type<float>(v72) * as_type<float>(v99));
    uint v101 = as_type<uint>(fma(as_type<float>(v28), as_type<float>(v96), as_type<float>(v100)));
    uint v102 = as_type<uint>(fma(as_type<float>(v101), as_type<float>(v32), as_type<float>(v33)));
    uint v103 = (uint)(int)as_type<float>(v102);
    uint v104 = v30 & v103;
    uint v105 = v104 << 16u;
    uint v106 = v93 | v105;
    uint v107 = v38 >> 24u;
    uint v108 = as_type<uint>((float)(int)v107);
    uint v109 = as_type<uint>(as_type<float>(v28) * as_type<float>(v108));
    uint v110 = as_type<uint>(fma(as_type<float>(v109), as_type<float>(v31), as_type<float>(v7)));
    uint v111 = as_type<uint>(as_type<float>(v110) - as_type<float>(v28) * as_type<float>(v108));
    uint v112 = as_type<uint>(as_type<float>(v72) * as_type<float>(v111));
    uint v113 = as_type<uint>(fma(as_type<float>(v28), as_type<float>(v108), as_type<float>(v112)));
    uint v114 = as_type<uint>(fma(as_type<float>(v113), as_type<float>(v32), as_type<float>(v33)));
    uint v115 = (uint)(int)as_type<float>(v114);
    uint v116 = v115 << 24u;
    uint v117 = v106 | v116;
    ((device uint*)p0)[i] = v117;
}
