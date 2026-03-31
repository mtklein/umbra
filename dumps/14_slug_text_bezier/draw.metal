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
    constant uint &w [[buffer(3)]],
    constant uint *buf_szs [[buffer(4)]],
    constant uint *buf_rbs [[buffer(5)]],
    constant uint &x0 [[buffer(6)]],
    constant uint &y0 [[buffer(7)]],
    device uchar *p0 [[buffer(0)]],
    device uchar *p1 [[buffer(1)]],
    device uchar *p2 [[buffer(2)]],
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
    uint v6 = 1065353216u;
    uint v7 = 255u;
    uint v8 = 998277249u;
    float v9 = as_type<float>(v6) - as_type<float>(v4);
    uint v10 = 1132396544u;
    uint v11 = ((device uint*)(p2 + y * buf_rbs[2]))[x];
    float v12 = fabs(as_type<float>(v11));
    float v13 = min(v12, as_type<float>(1065353216u));
    uint v14 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v15 = v14 >> 24u;
    float v16 = (float)(int)v15;
    float v17 = v16 * as_type<float>(998277249u);
    float v18 = fma(v17, v9, as_type<float>(v4));
    float v19 = v18 - v17;
    float v20 = fma(v13, v19, v17);
    float v21 = max(v20, as_type<float>(0u));
    float v22 = min(v21, as_type<float>(1065353216u));
    float v23 = v22 * as_type<float>(1132396544u);
    uint v24 = (uint)(int)rint(v23);
    uint v25 = v14 >> 8u;
    uint v26 = v25 & 255u;
    float v27 = (float)(int)v26;
    float v28 = v27 * as_type<float>(998277249u);
    float v29 = fma(v28, v9, as_type<float>(v2));
    float v30 = v29 - v28;
    float v31 = fma(v13, v30, v28);
    float v32 = max(v31, as_type<float>(0u));
    float v33 = min(v32, as_type<float>(1065353216u));
    float v34 = v33 * as_type<float>(1132396544u);
    uint v35 = (uint)(int)rint(v34);
    uint v36 = v14 >> 16u;
    uint v37 = v36 & 255u;
    float v38 = (float)(int)v37;
    float v39 = v38 * as_type<float>(998277249u);
    float v40 = fma(v39, v9, as_type<float>(v3));
    float v41 = v40 - v39;
    float v42 = fma(v13, v41, v39);
    float v43 = max(v42, as_type<float>(0u));
    float v44 = min(v43, as_type<float>(1065353216u));
    float v45 = v44 * as_type<float>(1132396544u);
    uint v46 = (uint)(int)rint(v45);
    uint v47 = v14 & 255u;
    float v48 = (float)(int)v47;
    float v49 = v48 * as_type<float>(998277249u);
    float v50 = fma(v49, v9, as_type<float>(v1));
    float v51 = v50 - v49;
    float v52 = fma(v13, v51, v49);
    float v53 = max(v52, as_type<float>(0u));
    float v54 = min(v53, as_type<float>(1065353216u));
    float v55 = v54 * as_type<float>(1132396544u);
    uint v56 = (uint)(int)rint(v55);
    uint v57 = v56 | (v35 << 8u);
    uint v58 = v57 | (v46 << 16u);
    uint v59 = v58 | (v24 << 24u);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v59;
}
