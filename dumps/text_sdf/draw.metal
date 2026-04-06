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
    uint v6 = 998277249u;
    uint v7 = 1054867456u;
    uint v8 = 1090519040u;
    uint v9 = 1065353216u;
    float v10 = as_type<float>(v9) - as_type<float>(v4);
    uint v11 = 1132396544u;
    uint v12 = (uint)((device ushort*)(p2 + y * buf_rbs[2]))[x];
    uint v13 = (uint)(int)(short)(ushort)v12;
    float v14 = (float)(int)v13;
    float v15 = v14 * as_type<float>(998277249u);
    float v16 = v15 - as_type<float>(1054867456u);
    float v17 = v16 * as_type<float>(1090519040u);
    float v18 = max(v17, as_type<float>(0u));
    float v19 = min(v18, as_type<float>(1065353216u));
    uint px20 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v20 = px20 & 0xFFu;
    uint v20_1 = (px20 >> 8u) & 0xFFu;
    uint v20_2 = (px20 >> 16u) & 0xFFu;
    uint v20_3 = px20 >> 24u;
    float v21 = (float)(int)v20_3;
    float v22 = v21 * as_type<float>(998277249u);
    float v23 = fma(v22, v10, as_type<float>(v4));
    float v24 = v23 - v22;
    float v25 = fma(v19, v24, v22);
    float v26 = max(v25, as_type<float>(0u));
    float v27 = min(v26, as_type<float>(1065353216u));
    float v28 = v27 * as_type<float>(1132396544u);
    uint v29 = (uint)(int)rint(v28);
    float v30 = (float)(int)v20;
    float v31 = v30 * as_type<float>(998277249u);
    float v32 = fma(v31, v10, as_type<float>(v1));
    float v33 = v32 - v31;
    float v34 = fma(v19, v33, v31);
    float v35 = max(v34, as_type<float>(0u));
    float v36 = min(v35, as_type<float>(1065353216u));
    float v37 = v36 * as_type<float>(1132396544u);
    uint v38 = (uint)(int)rint(v37);
    float v39 = (float)(int)v20_1;
    float v40 = v39 * as_type<float>(998277249u);
    float v41 = fma(v40, v10, as_type<float>(v2));
    float v42 = v41 - v40;
    float v43 = fma(v19, v42, v40);
    float v44 = max(v43, as_type<float>(0u));
    float v45 = min(v44, as_type<float>(1065353216u));
    float v46 = v45 * as_type<float>(1132396544u);
    uint v47 = (uint)(int)rint(v46);
    float v48 = (float)(int)v20_2;
    float v49 = v48 * as_type<float>(998277249u);
    float v50 = fma(v49, v10, as_type<float>(v3));
    float v51 = v50 - v49;
    float v52 = fma(v19, v51, v49);
    float v53 = max(v52, as_type<float>(0u));
    float v54 = min(v53, as_type<float>(1065353216u));
    float v55 = v54 * as_type<float>(1132396544u);
    uint v56 = (uint)(int)rint(v55);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = (v38 & 0xFFu) | ((v47 & 0xFFu) << 8u) | ((v56 & 0xFFu) << 16u) | (v29 << 24u);
}
