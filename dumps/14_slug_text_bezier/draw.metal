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
    uint v1 = ((device const uint*)p1)[2];
    uint v2 = ((device const uint*)p1)[3];
    uint v3 = ((device const uint*)p1)[4];
    uint v4 = ((device const uint*)p1)[5];
    uint v6 = 1065353216u;
    uint v7 = 255u;
    uint v8 = 998277249u;
    uint v9 = as_type<uint>(as_type<float>(v6) - as_type<float>(v4));
    uint v10 = 1132396544u;
    uint v11 = 1056964608u;
    uint v12 = ((device uint*)p2)[i];
    uint v13 = as_type<uint>(fabs(as_type<float>(v12)));
    uint v14 = as_type<uint>(min(as_type<float>(v13), as_type<float>(v6)));
    uint v15 = ((device uint*)p0)[i];
    uint v16 = v15 >> 24u;
    uint v17 = as_type<uint>((float)(int)v16);
    uint v18 = as_type<uint>(as_type<float>(v8) * as_type<float>(v17));
    uint v19 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v9), as_type<float>(v4)));
    uint v20 = as_type<uint>(as_type<float>(v19) - as_type<float>(v18));
    uint v21 = as_type<uint>(fma(as_type<float>(v14), as_type<float>(v20), as_type<float>(v18)));
    uint v22 = as_type<uint>(as_type<float>(v21) * as_type<float>(v10));
    uint v23 = as_type<uint>(as_type<float>(v11) + as_type<float>(v22));
    uint v24 = (uint)(int)as_type<float>(v23);
    uint v25 = v15 & v7;
    uint v26 = as_type<uint>((float)(int)v25);
    uint v27 = v15 >> 8u;
    uint v28 = v7 & v27;
    uint v29 = as_type<uint>((float)(int)v28);
    uint v30 = v15 >> 16u;
    uint v31 = v7 & v30;
    uint v32 = as_type<uint>((float)(int)v31);
    uint v33 = as_type<uint>(as_type<float>(v8) * as_type<float>(v26));
    uint v34 = as_type<uint>(fma(as_type<float>(v33), as_type<float>(v9), as_type<float>(v1)));
    uint v35 = as_type<uint>(as_type<float>(v34) - as_type<float>(v33));
    uint v36 = as_type<uint>(fma(as_type<float>(v14), as_type<float>(v35), as_type<float>(v33)));
    uint v37 = as_type<uint>(as_type<float>(v8) * as_type<float>(v29));
    uint v38 = as_type<uint>(fma(as_type<float>(v37), as_type<float>(v9), as_type<float>(v2)));
    uint v39 = as_type<uint>(as_type<float>(v38) - as_type<float>(v37));
    uint v40 = as_type<uint>(fma(as_type<float>(v14), as_type<float>(v39), as_type<float>(v37)));
    uint v41 = as_type<uint>(as_type<float>(v8) * as_type<float>(v32));
    uint v42 = as_type<uint>(fma(as_type<float>(v41), as_type<float>(v9), as_type<float>(v3)));
    uint v43 = as_type<uint>(as_type<float>(v42) - as_type<float>(v41));
    uint v44 = as_type<uint>(fma(as_type<float>(v14), as_type<float>(v43), as_type<float>(v41)));
    uint v45 = as_type<uint>(as_type<float>(v36) * as_type<float>(v10));
    uint v46 = as_type<uint>(as_type<float>(v11) + as_type<float>(v45));
    uint v47 = (uint)(int)as_type<float>(v46);
    uint v48 = as_type<uint>(as_type<float>(v40) * as_type<float>(v10));
    uint v49 = as_type<uint>(as_type<float>(v11) + as_type<float>(v48));
    uint v50 = (uint)(int)as_type<float>(v49);
    uint v51 = as_type<uint>(as_type<float>(v44) * as_type<float>(v10));
    uint v52 = as_type<uint>(as_type<float>(v11) + as_type<float>(v51));
    uint v53 = (uint)(int)as_type<float>(v52);
    uint v54 = v7 & v53;
    uint v55 = v7 & v47;
    uint v56 = v7 & v50;
    uint v57 = v56 << 8u;
    uint v58 = v55 | v57;
    uint v59 = v54 << 16u;
    uint v60 = v58 | v59;
    uint v61 = v24 << 24u;
    uint v62 = v60 | v61;
    ((device uint*)p0)[i] = v62;
}
