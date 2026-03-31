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
    uint v19 = v18 << 24u;
    uint v20 = v10 >> 8u;
    uint v21 = v20 & 255u;
    float v22 = (float)(int)v21;
    float v23 = v22 * as_type<float>(998277249u);
    float v24 = fma(v23, v8, as_type<float>(v2));
    float v25 = max(v24, as_type<float>(0u));
    float v26 = min(v25, as_type<float>(1065353216u));
    float v27 = v26 * as_type<float>(1132396544u);
    uint v28 = (uint)(int)rint(v27);
    uint v29 = v28 << 8u;
    uint v30 = v10 >> 16u;
    uint v31 = v30 & 255u;
    float v32 = (float)(int)v31;
    float v33 = v32 * as_type<float>(998277249u);
    float v34 = fma(v33, v8, as_type<float>(v3));
    float v35 = max(v34, as_type<float>(0u));
    float v36 = min(v35, as_type<float>(1065353216u));
    float v37 = v36 * as_type<float>(1132396544u);
    uint v38 = (uint)(int)rint(v37);
    uint v39 = v38 << 16u;
    uint v40 = v10 & 255u;
    float v41 = (float)(int)v40;
    float v42 = v41 * as_type<float>(998277249u);
    float v43 = fma(v42, v8, as_type<float>(v1));
    float v44 = max(v43, as_type<float>(0u));
    float v45 = min(v44, as_type<float>(1065353216u));
    float v46 = v45 * as_type<float>(1132396544u);
    uint v47 = (uint)(int)rint(v46);
    uint v48 = v47 | v29;
    uint v49 = v48 | v39;
    uint v50 = v49 | v19;
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v50;
}
