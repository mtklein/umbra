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
    uint v27 = 998277249u;
    uint v28 = 255u;
    uint v29 = as_type<uint>(as_type<float>(v23) - as_type<float>(v7));
    uint v30 = 1132396544u;
    uint v31 = 1056964608u;
    uint v32 = ((device uint*)p0)[i];
    uint v33 = v32 & v28;
    uint v34 = as_type<uint>((float)(int)v33);
    uint v35 = i;
    uint v36 = v35 + v1;
    uint v37 = as_type<uint>((float)(int)v36);
    uint v38 = as_type<uint>(fma(as_type<float>(v37), as_type<float>(v15), as_type<float>(v20)));
    uint v39 = as_type<uint>(as_type<float>(v17) + as_type<float>(v38));
    uint v40 = as_type<uint>(fma(as_type<float>(v37), as_type<float>(v9), as_type<float>(v21)));
    uint v41 = as_type<uint>(as_type<float>(v11) + as_type<float>(v40));
    uint v42 = as_type<uint>(as_type<float>(v41) / as_type<float>(v39));
    uint v43 = as_type<float>(v0) <= as_type<float>(v42) ? 0xffffffffu : 0u;
    uint v44 = as_type<float>(v42) <  as_type<float>(v18) ? 0xffffffffu : 0u;
    uint v45 = v43 & v44;
    uint v46 = as_type<uint>(fma(as_type<float>(v37), as_type<float>(v12), as_type<float>(v22)));
    uint v47 = as_type<uint>(as_type<float>(v14) + as_type<float>(v46));
    uint v48 = as_type<uint>(as_type<float>(v47) / as_type<float>(v39));
    uint v49 = as_type<float>(v0) <= as_type<float>(v48) ? 0xffffffffu : 0u;
    uint v50 = as_type<float>(v48) <  as_type<float>(v19) ? 0xffffffffu : 0u;
    uint v51 = v49 & v50;
    uint v52 = v45 & v51;
    uint v53 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v42)));
    uint v54 = as_type<uint>(min(as_type<float>(v53), as_type<float>(v24)));
    uint v55 = (uint)(int)as_type<float>(v54);
    uint v56 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v48)));
    uint v57 = as_type<uint>(min(as_type<float>(v56), as_type<float>(v25)));
    uint v58 = (uint)(int)as_type<float>(v57);
    uint v59 = v58 * v26;
    uint v60 = v55 + v59;
    uint v61 = (uint)((device ushort*)p2)[clamp_ix((int)v60,buf_szs[2],2)];
    uint v62 = (uint)(int)(short)(ushort)v61;
    uint v63 = as_type<uint>((float)(int)v62);
    uint v64 = as_type<uint>(as_type<float>(v27) * as_type<float>(v63));
    uint v65 = (v52 & v64) | (~v52 & v0);
    uint v66 = as_type<uint>(as_type<float>(v27) * as_type<float>(v34));
    uint v67 = as_type<uint>(fma(as_type<float>(v66), as_type<float>(v29), as_type<float>(v4)));
    uint v68 = as_type<uint>(as_type<float>(v67) - as_type<float>(v27) * as_type<float>(v34));
    uint v69 = as_type<uint>(as_type<float>(v65) * as_type<float>(v68));
    uint v70 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v34), as_type<float>(v69)));
    uint v71 = as_type<uint>(fma(as_type<float>(v70), as_type<float>(v30), as_type<float>(v31)));
    uint v72 = (uint)(int)as_type<float>(v71);
    uint v73 = v28 & v72;
    uint v74 = v32 >> 8u;
    uint v75 = v28 & v74;
    uint v76 = as_type<uint>((float)(int)v75);
    uint v77 = as_type<uint>(as_type<float>(v27) * as_type<float>(v76));
    uint v78 = as_type<uint>(fma(as_type<float>(v77), as_type<float>(v29), as_type<float>(v5)));
    uint v79 = as_type<uint>(as_type<float>(v78) - as_type<float>(v27) * as_type<float>(v76));
    uint v80 = as_type<uint>(as_type<float>(v65) * as_type<float>(v79));
    uint v81 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v76), as_type<float>(v80)));
    uint v82 = as_type<uint>(fma(as_type<float>(v81), as_type<float>(v30), as_type<float>(v31)));
    uint v83 = (uint)(int)as_type<float>(v82);
    uint v84 = v28 & v83;
    uint v85 = v84 << 8u;
    uint v86 = v73 | v85;
    uint v87 = v32 >> 16u;
    uint v88 = v28 & v87;
    uint v89 = as_type<uint>((float)(int)v88);
    uint v90 = as_type<uint>(as_type<float>(v27) * as_type<float>(v89));
    uint v91 = as_type<uint>(fma(as_type<float>(v90), as_type<float>(v29), as_type<float>(v6)));
    uint v92 = as_type<uint>(as_type<float>(v91) - as_type<float>(v27) * as_type<float>(v89));
    uint v93 = as_type<uint>(as_type<float>(v65) * as_type<float>(v92));
    uint v94 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v89), as_type<float>(v93)));
    uint v95 = as_type<uint>(fma(as_type<float>(v94), as_type<float>(v30), as_type<float>(v31)));
    uint v96 = (uint)(int)as_type<float>(v95);
    uint v97 = v28 & v96;
    uint v98 = v97 << 16u;
    uint v99 = v86 | v98;
    uint v100 = v32 >> 24u;
    uint v101 = as_type<uint>((float)(int)v100);
    uint v102 = as_type<uint>(as_type<float>(v27) * as_type<float>(v101));
    uint v103 = as_type<uint>(fma(as_type<float>(v102), as_type<float>(v29), as_type<float>(v7)));
    uint v104 = as_type<uint>(as_type<float>(v103) - as_type<float>(v27) * as_type<float>(v101));
    uint v105 = as_type<uint>(as_type<float>(v65) * as_type<float>(v104));
    uint v106 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v101), as_type<float>(v105)));
    uint v107 = as_type<uint>(fma(as_type<float>(v106), as_type<float>(v30), as_type<float>(v31)));
    uint v108 = (uint)(int)as_type<float>(v107);
    uint v109 = v108 << 24u;
    uint v110 = v99 | v109;
    ((device uint*)p0)[i] = v110;
}
