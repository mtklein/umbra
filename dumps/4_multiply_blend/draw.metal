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
    constant uint *buf_szs [[buffer(3)]],
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
    uint v8 = ((device const uint*)p1)[6];
    uint v9 = ((device const uint*)p1)[7];
    uint v10 = ((device const uint*)p1)[8];
    uint v11 = ((device const uint*)p1)[9];
    uint v12 = as_type<float>(v9) <= as_type<float>(v3) ? 0xffffffffu : 0u;
    uint v13 = as_type<float>(v3) <  as_type<float>(v11) ? 0xffffffffu : 0u;
    uint v14 = v12 & v13;
    uint v15 = 1065353216u;
    uint v16 = 255u;
    uint v17 = 998277249u;
    uint v18 = as_type<uint>(as_type<float>(v15) - as_type<float>(v7));
    uint v19 = 1132396544u;
    uint v20 = 1056964608u;
    uint v21 = ((device uint*)p0)[i];
    uint v22 = v21 & v16;
    uint v23 = as_type<uint>((float)(int)v22);
    uint v24 = i;
    uint v25 = v24 + v1;
    uint v26 = as_type<uint>((float)(int)v25);
    uint v27 = as_type<float>(v8) <= as_type<float>(v26) ? 0xffffffffu : 0u;
    uint v28 = as_type<float>(v26) <  as_type<float>(v10) ? 0xffffffffu : 0u;
    uint v29 = v27 & v28;
    uint v30 = v29 & v14;
    uint v31 = (v30 & v15) | (~v30 & v0);
    uint v32 = as_type<uint>(as_type<float>(v17) * as_type<float>(v23));
    uint v33 = v21 >> 24u;
    uint v34 = as_type<uint>((float)(int)v33);
    uint v35 = as_type<uint>(as_type<float>(v15) - as_type<float>(v17) * as_type<float>(v34));
    uint v36 = as_type<uint>(as_type<float>(v32) * as_type<float>(v18));
    uint v37 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v35), as_type<float>(v36)));
    uint v38 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v32), as_type<float>(v37)));
    uint v39 = as_type<uint>(as_type<float>(v38) - as_type<float>(v17) * as_type<float>(v23));
    uint v40 = as_type<uint>(as_type<float>(v31) * as_type<float>(v39));
    uint v41 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v23), as_type<float>(v40)));
    uint v42 = as_type<uint>(fma(as_type<float>(v41), as_type<float>(v19), as_type<float>(v20)));
    uint v43 = (uint)(int)as_type<float>(v42);
    uint v44 = v16 & v43;
    uint v45 = v21 >> 8u;
    uint v46 = v16 & v45;
    uint v47 = as_type<uint>((float)(int)v46);
    uint v48 = as_type<uint>(as_type<float>(v17) * as_type<float>(v47));
    uint v49 = as_type<uint>(as_type<float>(v48) * as_type<float>(v18));
    uint v50 = as_type<uint>(fma(as_type<float>(v5), as_type<float>(v35), as_type<float>(v49)));
    uint v51 = as_type<uint>(fma(as_type<float>(v5), as_type<float>(v48), as_type<float>(v50)));
    uint v52 = as_type<uint>(as_type<float>(v51) - as_type<float>(v17) * as_type<float>(v47));
    uint v53 = as_type<uint>(as_type<float>(v31) * as_type<float>(v52));
    uint v54 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v47), as_type<float>(v53)));
    uint v55 = as_type<uint>(fma(as_type<float>(v54), as_type<float>(v19), as_type<float>(v20)));
    uint v56 = (uint)(int)as_type<float>(v55);
    uint v57 = v16 & v56;
    uint v58 = v57 << 8u;
    uint v59 = v44 | v58;
    uint v60 = v21 >> 16u;
    uint v61 = v16 & v60;
    uint v62 = as_type<uint>((float)(int)v61);
    uint v63 = as_type<uint>(as_type<float>(v17) * as_type<float>(v62));
    uint v64 = as_type<uint>(as_type<float>(v63) * as_type<float>(v18));
    uint v65 = as_type<uint>(fma(as_type<float>(v6), as_type<float>(v35), as_type<float>(v64)));
    uint v66 = as_type<uint>(fma(as_type<float>(v6), as_type<float>(v63), as_type<float>(v65)));
    uint v67 = as_type<uint>(as_type<float>(v66) - as_type<float>(v17) * as_type<float>(v62));
    uint v68 = as_type<uint>(as_type<float>(v31) * as_type<float>(v67));
    uint v69 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v62), as_type<float>(v68)));
    uint v70 = as_type<uint>(fma(as_type<float>(v69), as_type<float>(v19), as_type<float>(v20)));
    uint v71 = (uint)(int)as_type<float>(v70);
    uint v72 = v16 & v71;
    uint v73 = v72 << 16u;
    uint v74 = v59 | v73;
    uint v75 = as_type<uint>(as_type<float>(v17) * as_type<float>(v34));
    uint v76 = as_type<uint>(as_type<float>(v75) * as_type<float>(v18));
    uint v77 = as_type<uint>(fma(as_type<float>(v7), as_type<float>(v35), as_type<float>(v76)));
    uint v78 = as_type<uint>(fma(as_type<float>(v7), as_type<float>(v75), as_type<float>(v77)));
    uint v79 = as_type<uint>(as_type<float>(v78) - as_type<float>(v17) * as_type<float>(v34));
    uint v80 = as_type<uint>(as_type<float>(v31) * as_type<float>(v79));
    uint v81 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v34), as_type<float>(v80)));
    uint v82 = as_type<uint>(fma(as_type<float>(v81), as_type<float>(v19), as_type<float>(v20)));
    uint v83 = (uint)(int)as_type<float>(v82);
    uint v84 = v83 << 24u;
    uint v85 = v74 | v84;
    ((device uint*)p0)[i] = v85;
}
