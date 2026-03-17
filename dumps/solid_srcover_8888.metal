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
    uint v1 = ((device const uint*)p1)[2];
    uint v2 = ((device const uint*)p1)[3];
    uint v3 = ((device const uint*)p1)[4];
    uint v4 = ((device const uint*)p1)[5];
    uint v5 = 255u;
    uint v6 = 8u;
    uint v7 = 16u;
    uint v8 = 24u;
    uint v9 = 998277249u;
    uint v10 = 1065353216u;
    uint v11 = as_type<uint>(as_type<float>(v10) - as_type<float>(v4));
    uint v12 = 1132396544u;
    uint v13 = 1056964608u;
    uint v14 = ((device uint*)p0)[i];
    uint v15 = v14 & v5;
    uint v16 = as_type<uint>((float)(int)v15);
    uint v17 = as_type<uint>(as_type<float>(v9) * as_type<float>(v16));
    uint v18 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v11), as_type<float>(v1)));
    uint v19 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v12), as_type<float>(v13)));
    uint v20 = (uint)(int)as_type<float>(v19);
    uint v21 = v5 & v20;
    uint v22 = v14 >> v6;
    uint v23 = v5 & v22;
    uint v24 = as_type<uint>((float)(int)v23);
    uint v25 = as_type<uint>(as_type<float>(v9) * as_type<float>(v24));
    uint v26 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v11), as_type<float>(v2)));
    uint v27 = as_type<uint>(fma(as_type<float>(v26), as_type<float>(v12), as_type<float>(v13)));
    uint v28 = (uint)(int)as_type<float>(v27);
    uint v29 = v5 & v28;
    uint v30 = v29 << v6;
    uint v31 = v21 | v30;
    uint v32 = v14 >> v7;
    uint v33 = v5 & v32;
    uint v34 = as_type<uint>((float)(int)v33);
    uint v35 = as_type<uint>(as_type<float>(v9) * as_type<float>(v34));
    uint v36 = as_type<uint>(fma(as_type<float>(v35), as_type<float>(v11), as_type<float>(v3)));
    uint v37 = as_type<uint>(fma(as_type<float>(v36), as_type<float>(v12), as_type<float>(v13)));
    uint v38 = (uint)(int)as_type<float>(v37);
    uint v39 = v5 & v38;
    uint v40 = v39 << v7;
    uint v41 = v31 | v40;
    uint v42 = v14 >> v8;
    uint v43 = as_type<uint>((float)(int)v42);
    uint v44 = as_type<uint>(as_type<float>(v9) * as_type<float>(v43));
    uint v45 = as_type<uint>(fma(as_type<float>(v44), as_type<float>(v11), as_type<float>(v4)));
    uint v46 = as_type<uint>(fma(as_type<float>(v45), as_type<float>(v12), as_type<float>(v13)));
    uint v47 = (uint)(int)as_type<float>(v46);
    uint v48 = v47 << v8;
    uint v49 = v41 | v48;
    ((device uint*)p0)[i] = v49;
}
