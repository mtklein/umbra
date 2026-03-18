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
    uint v21 = i;
    uint v22 = v21 + v1;
    uint v23 = as_type<uint>((float)(int)v22);
    uint v24 = as_type<float>(v8) <= as_type<float>(v23) ? 0xffffffffu : 0u;
    uint v25 = as_type<float>(v23) <  as_type<float>(v10) ? 0xffffffffu : 0u;
    uint v26 = v24 & v25;
    uint v27 = v26 & v14;
    uint v28 = (v27 & v15) | (~v27 & v0);
    uint v29 = ((device uint*)p0)[i];
    uint v30 = v29 & v16;
    uint v31 = as_type<uint>((float)(int)v30);
    uint v32 = as_type<uint>(as_type<float>(v17) * as_type<float>(v31));
    uint v33 = v29 >> 24u;
    uint v34 = as_type<uint>((float)(int)v33);
    uint v35 = as_type<uint>(as_type<float>(v17) * as_type<float>(v34));
    uint v36 = as_type<uint>(as_type<float>(v15) - as_type<float>(v35));
    uint v37 = as_type<uint>(as_type<float>(v32) * as_type<float>(v18));
    uint v38 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v36), as_type<float>(v37)));
    uint v39 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v32), as_type<float>(v38)));
    uint v40 = as_type<uint>(as_type<float>(v39) - as_type<float>(v32));
    uint v41 = as_type<uint>(fma(as_type<float>(v28), as_type<float>(v40), as_type<float>(v32)));
    uint v42 = as_type<uint>(as_type<float>(v41) * as_type<float>(v19));
    uint v43 = as_type<uint>(as_type<float>(v20) + as_type<float>(v42));
    uint v44 = (uint)(int)as_type<float>(v43);
    uint v45 = v16 & v44;
    uint v46 = v29 >> 8u;
    uint v47 = v16 & v46;
    uint v48 = as_type<uint>((float)(int)v47);
    uint v49 = as_type<uint>(as_type<float>(v17) * as_type<float>(v48));
    uint v50 = as_type<uint>(as_type<float>(v49) * as_type<float>(v18));
    uint v51 = as_type<uint>(fma(as_type<float>(v5), as_type<float>(v36), as_type<float>(v50)));
    uint v52 = as_type<uint>(fma(as_type<float>(v5), as_type<float>(v49), as_type<float>(v51)));
    uint v53 = as_type<uint>(as_type<float>(v52) - as_type<float>(v49));
    uint v54 = as_type<uint>(fma(as_type<float>(v28), as_type<float>(v53), as_type<float>(v49)));
    uint v55 = as_type<uint>(as_type<float>(v54) * as_type<float>(v19));
    uint v56 = as_type<uint>(as_type<float>(v20) + as_type<float>(v55));
    uint v57 = (uint)(int)as_type<float>(v56);
    uint v58 = v16 & v57;
    uint v59 = v58 << 8u;
    uint v60 = v45 | v59;
    uint v61 = v29 >> 16u;
    uint v62 = v16 & v61;
    uint v63 = as_type<uint>((float)(int)v62);
    uint v64 = as_type<uint>(as_type<float>(v17) * as_type<float>(v63));
    uint v65 = as_type<uint>(as_type<float>(v64) * as_type<float>(v18));
    uint v66 = as_type<uint>(fma(as_type<float>(v6), as_type<float>(v36), as_type<float>(v65)));
    uint v67 = as_type<uint>(fma(as_type<float>(v6), as_type<float>(v64), as_type<float>(v66)));
    uint v68 = as_type<uint>(as_type<float>(v67) - as_type<float>(v64));
    uint v69 = as_type<uint>(fma(as_type<float>(v28), as_type<float>(v68), as_type<float>(v64)));
    uint v70 = as_type<uint>(as_type<float>(v69) * as_type<float>(v19));
    uint v71 = as_type<uint>(as_type<float>(v20) + as_type<float>(v70));
    uint v72 = (uint)(int)as_type<float>(v71);
    uint v73 = v16 & v72;
    uint v74 = v73 << 16u;
    uint v75 = v60 | v74;
    uint v76 = as_type<uint>(as_type<float>(v35) * as_type<float>(v18));
    uint v77 = as_type<uint>(fma(as_type<float>(v7), as_type<float>(v36), as_type<float>(v76)));
    uint v78 = as_type<uint>(fma(as_type<float>(v7), as_type<float>(v35), as_type<float>(v77)));
    uint v79 = as_type<uint>(as_type<float>(v78) - as_type<float>(v35));
    uint v80 = as_type<uint>(fma(as_type<float>(v28), as_type<float>(v79), as_type<float>(v35)));
    uint v81 = as_type<uint>(as_type<float>(v80) * as_type<float>(v19));
    uint v82 = as_type<uint>(as_type<float>(v20) + as_type<float>(v81));
    uint v83 = (uint)(int)as_type<float>(v82);
    uint v84 = v83 << 24u;
    uint v85 = v75 | v84;
    ((device uint*)p0)[i] = v85;
}
