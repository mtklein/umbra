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
    uint v26 = as_type<uint>((int)floor(as_type<float>(v18)));
    uint v27 = 998277249u;
    uint v28 = 255u;
    uint v29 = as_type<uint>(as_type<float>(v23) - as_type<float>(v7));
    uint v30 = 1132396544u;
    uint v31 = i;
    uint v32 = v31 + v1;
    uint v33 = as_type<uint>((float)(int)v32);
    uint v34 = as_type<uint>(fma(as_type<float>(v33), as_type<float>(v12), as_type<float>(v22)));
    uint v35 = as_type<uint>(fma(as_type<float>(v33), as_type<float>(v15), as_type<float>(v20)));
    uint v36 = as_type<uint>(fma(as_type<float>(v33), as_type<float>(v9), as_type<float>(v21)));
    uint v37 = as_type<uint>(as_type<float>(v11) + as_type<float>(v36));
    uint v38 = as_type<uint>(as_type<float>(v14) + as_type<float>(v34));
    uint v39 = as_type<uint>(as_type<float>(v17) + as_type<float>(v35));
    uint v40 = as_type<uint>(as_type<float>(v38) / as_type<float>(v39));
    uint v41 = as_type<float>(v40) <  as_type<float>(v19) ? 0xffffffffu : 0u;
    uint v42 = as_type<uint>(as_type<float>(v37) / as_type<float>(v39));
    uint v43 = as_type<float>(v42) <  as_type<float>(v18) ? 0xffffffffu : 0u;
    uint v44 = as_type<float>(v0) <= as_type<float>(v42) ? 0xffffffffu : 0u;
    uint v45 = v44 & v43;
    uint v46 = as_type<float>(v0) <= as_type<float>(v40) ? 0xffffffffu : 0u;
    uint v47 = v46 & v41;
    uint v48 = v45 & v47;
    uint v49 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v42)));
    uint v50 = as_type<uint>(min(as_type<float>(v49), as_type<float>(v24)));
    uint v51 = as_type<uint>((int)floor(as_type<float>(v50)));
    uint v52 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v40)));
    uint v53 = as_type<uint>(min(as_type<float>(v52), as_type<float>(v25)));
    uint v54 = as_type<uint>((int)floor(as_type<float>(v53)));
    uint v55 = v54 * v26;
    uint v56 = v51 + v55;
    uint v57 = (uint)((device ushort*)p2)[clamp_ix((int)v56,buf_szs[2],2)];
    uint v58 = (uint)(int)(short)(ushort)v57;
    uint v59 = as_type<uint>((float)(int)v58);
    uint v60 = as_type<uint>(as_type<float>(v27) * as_type<float>(v59));
    uint v61 = (v48 & v60) | (~v48 & v0);
    uint v62 = ((device uint*)p0)[i];
    uint v63 = v62 >> 24u;
    uint v64 = as_type<uint>((float)(int)v63);
    uint v65 = as_type<uint>(as_type<float>(v27) * as_type<float>(v64));
    uint v66 = as_type<uint>(fma(as_type<float>(v65), as_type<float>(v29), as_type<float>(v7)));
    uint v67 = as_type<uint>(as_type<float>(v66) - as_type<float>(v65));
    uint v68 = as_type<uint>(fma(as_type<float>(v61), as_type<float>(v67), as_type<float>(v65)));
    uint v69 = as_type<uint>(as_type<float>(v68) * as_type<float>(v30));
    uint v70 = as_type<uint>((int)rint(as_type<float>(v69)));
    uint v71 = v62 & v28;
    uint v72 = as_type<uint>((float)(int)v71);
    uint v73 = v62 >> 8u;
    uint v74 = v28 & v73;
    uint v75 = as_type<uint>((float)(int)v74);
    uint v76 = v62 >> 16u;
    uint v77 = v28 & v76;
    uint v78 = as_type<uint>((float)(int)v77);
    uint v79 = as_type<uint>(as_type<float>(v27) * as_type<float>(v72));
    uint v80 = as_type<uint>(fma(as_type<float>(v79), as_type<float>(v29), as_type<float>(v4)));
    uint v81 = as_type<uint>(as_type<float>(v80) - as_type<float>(v79));
    uint v82 = as_type<uint>(fma(as_type<float>(v61), as_type<float>(v81), as_type<float>(v79)));
    uint v83 = as_type<uint>(as_type<float>(v27) * as_type<float>(v75));
    uint v84 = as_type<uint>(fma(as_type<float>(v83), as_type<float>(v29), as_type<float>(v5)));
    uint v85 = as_type<uint>(as_type<float>(v84) - as_type<float>(v83));
    uint v86 = as_type<uint>(fma(as_type<float>(v61), as_type<float>(v85), as_type<float>(v83)));
    uint v87 = as_type<uint>(as_type<float>(v27) * as_type<float>(v78));
    uint v88 = as_type<uint>(fma(as_type<float>(v87), as_type<float>(v29), as_type<float>(v6)));
    uint v89 = as_type<uint>(as_type<float>(v88) - as_type<float>(v87));
    uint v90 = as_type<uint>(fma(as_type<float>(v61), as_type<float>(v89), as_type<float>(v87)));
    uint v91 = as_type<uint>(as_type<float>(v82) * as_type<float>(v30));
    uint v92 = as_type<uint>((int)rint(as_type<float>(v91)));
    uint v93 = as_type<uint>(as_type<float>(v86) * as_type<float>(v30));
    uint v94 = as_type<uint>((int)rint(as_type<float>(v93)));
    uint v95 = as_type<uint>(as_type<float>(v90) * as_type<float>(v30));
    uint v96 = as_type<uint>((int)rint(as_type<float>(v95)));
    uint v97 = v28 & v96;
    uint v98 = v28 & v92;
    uint v99 = v28 & v94;
    uint v100 = v99 << 8u;
    uint v101 = v98 | v100;
    uint v102 = v97 << 16u;
    uint v103 = v101 | v102;
    uint v104 = v70 << 24u;
    uint v105 = v103 | v104;
    ((device uint*)p0)[i] = v105;
}
