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
    uint v5 = 998277249u;
    uint v6 = 1065353216u;
    float v7 = as_type<float>(v6) - as_type<float>(v4);
    uint v8 = 1132396544u;
    uint px9 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v9 = px9 & 0xFFu;
    uint v9_1 = (px9 >> 8u) & 0xFFu;
    uint v9_2 = (px9 >> 16u) & 0xFFu;
    uint v9_3 = px9 >> 24u;
    float v10 = (float)(int)v9_3;
    float v11 = v10 * as_type<float>(998277249u);
    float v12 = fma(v11, v7, as_type<float>(v4));
    float v13 = max(v12, as_type<float>(0u));
    float v14 = min(v13, as_type<float>(1065353216u));
    float v15 = v14 * as_type<float>(1132396544u);
    uint v16 = (uint)(int)rint(v15);
    float v17 = (float)(int)v9;
    float v18 = v17 * as_type<float>(998277249u);
    float v19 = fma(v18, v7, as_type<float>(v1));
    float v20 = max(v19, as_type<float>(0u));
    float v21 = min(v20, as_type<float>(1065353216u));
    float v22 = v21 * as_type<float>(1132396544u);
    uint v23 = (uint)(int)rint(v22);
    float v24 = (float)(int)v9_1;
    float v25 = v24 * as_type<float>(998277249u);
    float v26 = fma(v25, v7, as_type<float>(v2));
    float v27 = max(v26, as_type<float>(0u));
    float v28 = min(v27, as_type<float>(1065353216u));
    float v29 = v28 * as_type<float>(1132396544u);
    uint v30 = (uint)(int)rint(v29);
    float v31 = (float)(int)v9_2;
    float v32 = v31 * as_type<float>(998277249u);
    float v33 = fma(v32, v7, as_type<float>(v3));
    float v34 = max(v33, as_type<float>(0u));
    float v35 = min(v34, as_type<float>(1065353216u));
    float v36 = v35 * as_type<float>(1132396544u);
    uint v37 = (uint)(int)rint(v36);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = (v23 & 0xFFu) | ((v30 & 0xFFu) << 8u) | ((v37 & 0xFFu) << 16u) | (v16 << 24u);
}
