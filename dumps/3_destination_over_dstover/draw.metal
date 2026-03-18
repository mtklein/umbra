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
    uint v18 = 1132396544u;
    uint v19 = 1056964608u;
    uint v20 = i;
    uint v21 = v20 + v1;
    uint v22 = as_type<uint>((float)(int)v21);
    uint v23 = as_type<float>(v8) <= as_type<float>(v22) ? 0xffffffffu : 0u;
    uint v24 = as_type<float>(v22) <  as_type<float>(v10) ? 0xffffffffu : 0u;
    uint v25 = v23 & v24;
    uint v26 = v25 & v14;
    uint v27 = (v26 & v15) | (~v26 & v0);
    uint v28 = ((device uint*)p0)[i];
    uint v29 = v28 & v16;
    uint v30 = as_type<uint>((float)(int)v29);
    uint v31 = as_type<uint>(as_type<float>(v17) * as_type<float>(v30));
    uint v32 = v28 >> 24u;
    uint v33 = as_type<uint>((float)(int)v32);
    uint v34 = as_type<uint>(as_type<float>(v17) * as_type<float>(v33));
    uint v35 = as_type<uint>(as_type<float>(v15) - as_type<float>(v34));
    uint v36 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v35), as_type<float>(v31)));
    uint v37 = as_type<uint>(as_type<float>(v36) - as_type<float>(v31));
    uint v38 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v37), as_type<float>(v31)));
    uint v39 = as_type<uint>(as_type<float>(v38) * as_type<float>(v18));
    uint v40 = as_type<uint>(as_type<float>(v19) + as_type<float>(v39));
    uint v41 = (uint)(int)as_type<float>(v40);
    uint v42 = v16 & v41;
    uint v43 = v28 >> 8u;
    uint v44 = v16 & v43;
    uint v45 = as_type<uint>((float)(int)v44);
    uint v46 = as_type<uint>(as_type<float>(v17) * as_type<float>(v45));
    uint v47 = as_type<uint>(fma(as_type<float>(v5), as_type<float>(v35), as_type<float>(v46)));
    uint v48 = as_type<uint>(as_type<float>(v47) - as_type<float>(v46));
    uint v49 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v48), as_type<float>(v46)));
    uint v50 = as_type<uint>(as_type<float>(v49) * as_type<float>(v18));
    uint v51 = as_type<uint>(as_type<float>(v19) + as_type<float>(v50));
    uint v52 = (uint)(int)as_type<float>(v51);
    uint v53 = v16 & v52;
    uint v54 = v53 << 8u;
    uint v55 = v42 | v54;
    uint v56 = v28 >> 16u;
    uint v57 = v16 & v56;
    uint v58 = as_type<uint>((float)(int)v57);
    uint v59 = as_type<uint>(as_type<float>(v17) * as_type<float>(v58));
    uint v60 = as_type<uint>(fma(as_type<float>(v6), as_type<float>(v35), as_type<float>(v59)));
    uint v61 = as_type<uint>(as_type<float>(v60) - as_type<float>(v59));
    uint v62 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v61), as_type<float>(v59)));
    uint v63 = as_type<uint>(as_type<float>(v62) * as_type<float>(v18));
    uint v64 = as_type<uint>(as_type<float>(v19) + as_type<float>(v63));
    uint v65 = (uint)(int)as_type<float>(v64);
    uint v66 = v16 & v65;
    uint v67 = v66 << 16u;
    uint v68 = v55 | v67;
    uint v69 = as_type<uint>(fma(as_type<float>(v7), as_type<float>(v35), as_type<float>(v34)));
    uint v70 = as_type<uint>(as_type<float>(v69) - as_type<float>(v34));
    uint v71 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v70), as_type<float>(v34)));
    uint v72 = as_type<uint>(as_type<float>(v71) * as_type<float>(v18));
    uint v73 = as_type<uint>(as_type<float>(v19) + as_type<float>(v72));
    uint v74 = (uint)(int)as_type<float>(v73);
    uint v75 = v74 << 24u;
    uint v76 = v68 | v75;
    ((device uint*)p0)[i] = v76;
}
