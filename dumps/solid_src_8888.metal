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
    uint v9 = 1132396544u;
    uint v10 = 1056964608u;
    uint v11 = as_type<uint>(fma(as_type<float>(v1), as_type<float>(v9), as_type<float>(v10)));
    uint v12 = (uint)(int)as_type<float>(v11);
    uint v13 = as_type<uint>(fma(as_type<float>(v2), as_type<float>(v9), as_type<float>(v10)));
    uint v14 = (uint)(int)as_type<float>(v13);
    uint v15 = as_type<uint>(fma(as_type<float>(v3), as_type<float>(v9), as_type<float>(v10)));
    uint v16 = (uint)(int)as_type<float>(v15);
    uint v17 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v9), as_type<float>(v10)));
    uint v18 = (uint)(int)as_type<float>(v17);
    uint v19 = v5 & v12;
    uint v20 = v5 & v14;
    uint v21 = v20 << v6;
    uint v22 = v20 << 8u;
    uint v23 = v21;
    uint v24 = v19 | v23;
    uint v25 = v5 & v16;
    uint v26 = v25 << v7;
    uint v27 = v25 << 16u;
    uint v28 = v26;
    uint v29 = v24 | v28;
    uint v30 = v18 << v8;
    uint v31 = v18 << 24u;
    uint v32 = v30;
    uint v33 = v29 | v32;
    ((device uint*)p0)[i] = v33;
}
