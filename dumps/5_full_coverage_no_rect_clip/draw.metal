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
    constant uint &w [[buffer(2)]],
    constant uint *buf_szs [[buffer(3)]],
    constant uint *buf_rbs [[buffer(4)]],
    constant uint &x0 [[buffer(5)]],
    constant uint &y0 [[buffer(6)]],
    device uchar *p0 [[buffer(0)]],
    device uchar *p1 [[buffer(1)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= w) return;
    uint x = x0 + pos.x;
    uint y = y0 + pos.y;
    uint v0 = 0u;
    uint v1 = ((device const uint*)p0)[0];
    uint v2 = ((device const uint*)p0)[1];
    uint v3 = ((device const uint*)p0)[2];
    uint v4 = ((device const uint*)p0)[3];
    uint v5 = 255u;
    uint v6 = 998277249u;
    uint v7 = 1065353216u;
    float v8 = as_type<float>(v7) - as_type<float>(v4);
    uint v9 = 1132396544u;
    uint v10 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v11 = v10 >> 24u;
    float v12 = (float)(int)v11;
    float v13 = v12 * as_type<float>(998277249u);
    float v14 = fma(v13, v8, as_type<float>(v4));
    float v15 = max(v14, as_type<float>(0u));
    float v16 = min(v15, as_type<float>(1065353216u));
    float v17 = v16 * as_type<float>(1132396544u);
    uint v18 = (uint)(int)rint(v17);
    uint v19 = v10 >> 8u;
    uint v20 = v19 & 255u;
    float v21 = (float)(int)v20;
    float v22 = v21 * as_type<float>(998277249u);
    float v23 = fma(v22, v8, as_type<float>(v2));
    float v24 = max(v23, as_type<float>(0u));
    float v25 = min(v24, as_type<float>(1065353216u));
    float v26 = v25 * as_type<float>(1132396544u);
    uint v27 = (uint)(int)rint(v26);
    uint v28 = v10 >> 16u;
    uint v29 = v28 & 255u;
    float v30 = (float)(int)v29;
    float v31 = v30 * as_type<float>(998277249u);
    float v32 = fma(v31, v8, as_type<float>(v3));
    float v33 = max(v32, as_type<float>(0u));
    float v34 = min(v33, as_type<float>(1065353216u));
    float v35 = v34 * as_type<float>(1132396544u);
    uint v36 = (uint)(int)rint(v35);
    uint v37 = v10 & 255u;
    float v38 = (float)(int)v37;
    float v39 = v38 * as_type<float>(998277249u);
    float v40 = fma(v39, v8, as_type<float>(v1));
    float v41 = max(v40, as_type<float>(0u));
    float v42 = min(v41, as_type<float>(1065353216u));
    float v43 = v42 * as_type<float>(1132396544u);
    uint v44 = (uint)(int)rint(v43);
    uint v45 = v44 | (v27 << 8u);
    uint v46 = v45 | (v36 << 16u);
    uint v47 = v46 | (v18 << 24u);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v47;
}
