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
    uint v24 = as_type<float>(v23) <  as_type<float>(v10) ? 0xffffffffu : 0u;
    uint v25 = as_type<float>(v8) <= as_type<float>(v23) ? 0xffffffffu : 0u;
    uint v26 = v25 & v24;
    uint v27 = v26 & v14;
    uint v28 = (v27 & v15) | (~v27 & v0);
    uint v29 = ((device uint*)p0)[i];
    uint v30 = v29 >> 24u;
    uint v31 = as_type<uint>((float)(int)v30);
    uint v32 = as_type<uint>(as_type<float>(v17) * as_type<float>(v31));
    uint v33 = as_type<uint>(as_type<float>(v15) - as_type<float>(v32));
    uint v34 = as_type<uint>(as_type<float>(v32) * as_type<float>(v18));
    uint v35 = as_type<uint>(fma(as_type<float>(v7), as_type<float>(v33), as_type<float>(v34)));
    uint v36 = as_type<uint>(fma(as_type<float>(v7), as_type<float>(v32), as_type<float>(v35)));
    uint v37 = as_type<uint>(as_type<float>(v36) - as_type<float>(v32));
    uint v38 = as_type<uint>(fma(as_type<float>(v28), as_type<float>(v37), as_type<float>(v32)));
    uint v39 = as_type<uint>(as_type<float>(v38) * as_type<float>(v19));
    uint v40 = as_type<uint>(as_type<float>(v20) + as_type<float>(v39));
    uint v41 = (uint)(int)as_type<float>(v40);
    uint v42 = v29 & v16;
    uint v43 = as_type<uint>((float)(int)v42);
    uint v44 = v29 >> 8u;
    uint v45 = v16 & v44;
    uint v46 = as_type<uint>((float)(int)v45);
    uint v47 = v29 >> 16u;
    uint v48 = v16 & v47;
    uint v49 = as_type<uint>((float)(int)v48);
    uint v50 = as_type<uint>(as_type<float>(v17) * as_type<float>(v43));
    uint v51 = as_type<uint>(as_type<float>(v17) * as_type<float>(v46));
    uint v52 = as_type<uint>(as_type<float>(v17) * as_type<float>(v49));
    uint v53 = as_type<uint>(as_type<float>(v50) * as_type<float>(v18));
    uint v54 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v33), as_type<float>(v53)));
    uint v55 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v50), as_type<float>(v54)));
    uint v56 = as_type<uint>(as_type<float>(v55) - as_type<float>(v50));
    uint v57 = as_type<uint>(fma(as_type<float>(v28), as_type<float>(v56), as_type<float>(v50)));
    uint v58 = as_type<uint>(as_type<float>(v51) * as_type<float>(v18));
    uint v59 = as_type<uint>(fma(as_type<float>(v5), as_type<float>(v33), as_type<float>(v58)));
    uint v60 = as_type<uint>(fma(as_type<float>(v5), as_type<float>(v51), as_type<float>(v59)));
    uint v61 = as_type<uint>(as_type<float>(v60) - as_type<float>(v51));
    uint v62 = as_type<uint>(fma(as_type<float>(v28), as_type<float>(v61), as_type<float>(v51)));
    uint v63 = as_type<uint>(as_type<float>(v52) * as_type<float>(v18));
    uint v64 = as_type<uint>(fma(as_type<float>(v6), as_type<float>(v33), as_type<float>(v63)));
    uint v65 = as_type<uint>(fma(as_type<float>(v6), as_type<float>(v52), as_type<float>(v64)));
    uint v66 = as_type<uint>(as_type<float>(v65) - as_type<float>(v52));
    uint v67 = as_type<uint>(fma(as_type<float>(v28), as_type<float>(v66), as_type<float>(v52)));
    uint v68 = as_type<uint>(as_type<float>(v57) * as_type<float>(v19));
    uint v69 = as_type<uint>(as_type<float>(v20) + as_type<float>(v68));
    uint v70 = (uint)(int)as_type<float>(v69);
    uint v71 = as_type<uint>(as_type<float>(v62) * as_type<float>(v19));
    uint v72 = as_type<uint>(as_type<float>(v20) + as_type<float>(v71));
    uint v73 = (uint)(int)as_type<float>(v72);
    uint v74 = as_type<uint>(as_type<float>(v67) * as_type<float>(v19));
    uint v75 = as_type<uint>(as_type<float>(v20) + as_type<float>(v74));
    uint v76 = (uint)(int)as_type<float>(v75);
    uint v77 = v16 & v76;
    uint v78 = v16 & v70;
    uint v79 = v16 & v73;
    uint v80 = v79 << 8u;
    uint v81 = v78 | v80;
    uint v82 = v77 << 16u;
    uint v83 = v81 | v82;
    uint v84 = v41 << 24u;
    uint v85 = v83 | v84;
    ((device uint*)p0)[i] = v85;
}
