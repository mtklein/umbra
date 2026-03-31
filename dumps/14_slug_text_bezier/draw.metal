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
    uint v25 = v24 << 24u;
    uint v26 = v14 >> 8u;
    uint v27 = v26 & 255u;
    float v28 = (float)(int)v27;
    float v29 = v28 * as_type<float>(998277249u);
    float v30 = fma(v29, v9, as_type<float>(v2));
    float v31 = v30 - v29;
    float v32 = fma(v13, v31, v29);
    float v33 = max(v32, as_type<float>(0u));
    float v34 = min(v33, as_type<float>(1065353216u));
    float v35 = v34 * as_type<float>(1132396544u);
    uint v36 = (uint)(int)rint(v35);
    uint v37 = v36 << 8u;
    uint v38 = v14 >> 16u;
    uint v39 = v38 & 255u;
    float v40 = (float)(int)v39;
    float v41 = v40 * as_type<float>(998277249u);
    float v42 = fma(v41, v9, as_type<float>(v3));
    float v43 = v42 - v41;
    float v44 = fma(v13, v43, v41);
    float v45 = max(v44, as_type<float>(0u));
    float v46 = min(v45, as_type<float>(1065353216u));
    float v47 = v46 * as_type<float>(1132396544u);
    uint v48 = (uint)(int)rint(v47);
    uint v49 = v48 << 16u;
    uint v50 = v14 & 255u;
    float v51 = (float)(int)v50;
    float v52 = v51 * as_type<float>(998277249u);
    float v53 = fma(v52, v9, as_type<float>(v1));
    float v54 = v53 - v52;
    float v55 = fma(v13, v54, v52);
    float v56 = max(v55, as_type<float>(0u));
    float v57 = min(v56, as_type<float>(1065353216u));
    float v58 = v57 * as_type<float>(1132396544u);
    uint v59 = (uint)(int)rint(v58);
    uint v60 = v59 | v37;
    uint v61 = v60 | v49;
    uint v62 = v61 | v25;
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v62;
}
