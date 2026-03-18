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
    uint v6 = 998277249u;
    uint v7 = 1065353216u;
    uint v8 = as_type<uint>(as_type<float>(v7) - as_type<float>(v4));
    uint v9 = 1132396544u;
    uint v10 = 1056964608u;
    uint v11 = ((device uint*)p0)[i];
    uint v12 = v11 & v5;
    uint v13 = as_type<uint>((float)(int)v12);
    uint v14 = as_type<uint>(as_type<float>(v6) * as_type<float>(v13));
    uint v15 = as_type<uint>(fma(as_type<float>(v14), as_type<float>(v8), as_type<float>(v1)));
    uint v16 = as_type<uint>(as_type<float>(v15) * as_type<float>(v9));
    uint v17 = as_type<uint>(as_type<float>(v10) + as_type<float>(v16));
    uint v18 = (uint)(int)as_type<float>(v17);
    uint v19 = v5 & v18;
    uint v20 = v11 >> 8u;
    uint v21 = v5 & v20;
    uint v22 = as_type<uint>((float)(int)v21);
    uint v23 = as_type<uint>(as_type<float>(v6) * as_type<float>(v22));
    uint v24 = as_type<uint>(fma(as_type<float>(v23), as_type<float>(v8), as_type<float>(v2)));
    uint v25 = as_type<uint>(as_type<float>(v24) * as_type<float>(v9));
    uint v26 = as_type<uint>(as_type<float>(v10) + as_type<float>(v25));
    uint v27 = (uint)(int)as_type<float>(v26);
    uint v28 = v5 & v27;
    uint v29 = v28 << 8u;
    uint v30 = v19 | v29;
    uint v31 = v11 >> 16u;
    uint v32 = v5 & v31;
    uint v33 = as_type<uint>((float)(int)v32);
    uint v34 = as_type<uint>(as_type<float>(v6) * as_type<float>(v33));
    uint v35 = as_type<uint>(fma(as_type<float>(v34), as_type<float>(v8), as_type<float>(v3)));
    uint v36 = as_type<uint>(as_type<float>(v35) * as_type<float>(v9));
    uint v37 = as_type<uint>(as_type<float>(v10) + as_type<float>(v36));
    uint v38 = (uint)(int)as_type<float>(v37);
    uint v39 = v5 & v38;
    uint v40 = v39 << 16u;
    uint v41 = v30 | v40;
    uint v42 = v11 >> 24u;
    uint v43 = as_type<uint>((float)(int)v42);
    uint v44 = as_type<uint>(as_type<float>(v6) * as_type<float>(v43));
    uint v45 = as_type<uint>(fma(as_type<float>(v44), as_type<float>(v8), as_type<float>(v4)));
    uint v46 = as_type<uint>(as_type<float>(v45) * as_type<float>(v9));
    uint v47 = as_type<uint>(as_type<float>(v10) + as_type<float>(v46));
    uint v48 = (uint)(int)as_type<float>(v47);
    uint v49 = v48 << 24u;
    uint v50 = v41 | v49;
    ((device uint*)p0)[i] = v50;
}
