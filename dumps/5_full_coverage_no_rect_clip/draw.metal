#include <metal_stdlib>
using namespace metal;

static inline int safe_ix(int ix, uint bytes, int elem) {
    int count = (int)(bytes / (uint)elem);
    return clamp(ix, 0, max(count-1, 0));
}
static inline uint oob_mask(int ix, uint bytes, int elem) {
    int count = (int)(bytes / (uint)elem);
    return (ix >= 0 && ix < count) ? ~0u : 0u;
}

kernel void umbra_entry(
    constant uint &n [[buffer(0)]],
    constant uint &w [[buffer(4)]],
    constant uint &stride [[buffer(5)]],
    constant uint &x0 [[buffer(6)]],
    constant uint &y0 [[buffer(7)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    constant uint *buf_szs [[buffer(3)]],
    uint2 pos [[thread_position_in_grid]]
) {
    uint i = (y0 + pos.y) * stride + x0 + pos.x;
    if (i >= n) return;
    uint v0 = 0u;
    uint v1 = ((device const uint*)p0)[0];
    uint v2 = ((device const uint*)p0)[1];
    uint v3 = ((device const uint*)p0)[2];
    uint v4 = ((device const uint*)p0)[3];
    uint v5 = 255u;
    uint v6 = 998277249u;
    uint v7 = 1065353216u;
    uint v8 = as_type<uint>(as_type<float>(v7) - as_type<float>(v4));
    uint v9 = 1132396544u;
    uint v10 = ((device uint*)p1)[i];
    uint v11 = v10 >> 24u;
    uint v12 = as_type<uint>((float)(int)v11);
    uint v13 = as_type<uint>(as_type<float>(v12) * as_type<float>(998277249u));
    uint v14 = as_type<uint>(fma(as_type<float>(v13), as_type<float>(v8), as_type<float>(v4)));
    uint v15 = as_type<uint>(max(as_type<float>(v14), as_type<float>(0u)));
    uint v16 = as_type<uint>(min(as_type<float>(v15), as_type<float>(1065353216u)));
    uint v17 = as_type<uint>(as_type<float>(v16) * as_type<float>(1132396544u));
    uint v18 = v10 >> 8u;
    uint v19 = v18 & 255u;
    uint v20 = as_type<uint>((float)(int)v19);
    uint v21 = as_type<uint>(as_type<float>(v20) * as_type<float>(998277249u));
    uint v22 = as_type<uint>(fma(as_type<float>(v21), as_type<float>(v8), as_type<float>(v2)));
    uint v23 = as_type<uint>(max(as_type<float>(v22), as_type<float>(0u)));
    uint v24 = as_type<uint>(min(as_type<float>(v23), as_type<float>(1065353216u)));
    uint v25 = as_type<uint>(as_type<float>(v24) * as_type<float>(1132396544u));
    uint v26 = v10 >> 16u;
    uint v27 = v26 & 255u;
    uint v28 = as_type<uint>((float)(int)v27);
    uint v29 = as_type<uint>(as_type<float>(v28) * as_type<float>(998277249u));
    uint v30 = as_type<uint>(fma(as_type<float>(v29), as_type<float>(v8), as_type<float>(v3)));
    uint v31 = as_type<uint>(max(as_type<float>(v30), as_type<float>(0u)));
    uint v32 = as_type<uint>(min(as_type<float>(v31), as_type<float>(1065353216u)));
    uint v33 = as_type<uint>(as_type<float>(v32) * as_type<float>(1132396544u));
    uint v34 = v10 & 255u;
    uint v35 = as_type<uint>((float)(int)v34);
    uint v36 = as_type<uint>(as_type<float>(v35) * as_type<float>(998277249u));
    uint v37 = as_type<uint>(fma(as_type<float>(v36), as_type<float>(v8), as_type<float>(v1)));
    uint v38 = as_type<uint>(max(as_type<float>(v37), as_type<float>(0u)));
    uint v39 = as_type<uint>(min(as_type<float>(v38), as_type<float>(1065353216u)));
    uint v40 = as_type<uint>(as_type<float>(v39) * as_type<float>(1132396544u));
    uint v41 = as_type<uint>((int)rint(as_type<float>(v40)));
    uint v42 = as_type<uint>((int)rint(as_type<float>(v25)));
    uint v43 = v41 & 255u;
    uint v44 = v42 & 255u;
    uint v45 = v43 | (v44 << 8u);
    uint v46 = as_type<uint>((int)rint(as_type<float>(v33)));
    uint v47 = v46 & 255u;
    uint v48 = v45 | (v47 << 16u);
    uint v49 = as_type<uint>((int)rint(as_type<float>(v17)));
    uint v50 = v48 | (v49 << 24u);
    ((device uint*)p1)[i] = v50;
}
