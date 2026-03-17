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
    uint v6 = 1132396544u;
    uint v7 = 1056964608u;
    uint v8 = as_type<uint>(fma(as_type<float>(v1), as_type<float>(v6), as_type<float>(v7)));
    uint v9 = (uint)(int)as_type<float>(v8);
    uint v10 = as_type<uint>(fma(as_type<float>(v2), as_type<float>(v6), as_type<float>(v7)));
    uint v11 = (uint)(int)as_type<float>(v10);
    uint v12 = as_type<uint>(fma(as_type<float>(v3), as_type<float>(v6), as_type<float>(v7)));
    uint v13 = (uint)(int)as_type<float>(v12);
    uint v14 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v6), as_type<float>(v7)));
    uint v15 = (uint)(int)as_type<float>(v14);
    uint v16 = v5 & v9;
    uint v17 = v5 & v11;
    uint v18 = v17 << 8u;
    uint v19 = v16 | v18;
    uint v20 = v5 & v13;
    uint v21 = v20 << 16u;
    uint v22 = v19 | v21;
    uint v23 = v15 << 24u;
    uint v24 = v22 | v23;
    ((device uint*)p0)[i] = v24;
}
