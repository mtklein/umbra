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
    uint v5 = ((device const uint*)p0)[4];
    uint v6 = ((device const uint*)p0)[5];
    uint v7 = ((device const uint*)p0)[6];
    uint v8 = ((device const uint*)p0)[7];
    uint v9 = 1065353216u;
    uint v10 = 998277249u;
    uint v11 = 1132396544u;
    uint v12 = x0 + pos.x;
    float v13 = (float)(int)v12;
    uint v14 = v13 <  as_type<float>(v7) ? 0xffffffffu : 0u;
    uint v15 = as_type<float>(v5) <= v13 ? 0xffffffffu : 0u;
    uint v16 = v15 & v14;
    uint v17 = y0 + pos.y;
    float v18 = (float)(int)v17;
    uint v19 = v18 <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v20 = as_type<float>(v6) <= v18 ? 0xffffffffu : 0u;
    uint v21 = v20 & v19;
    uint v22 = v16 & v21;
    uint v23 = select(v0, v9, v22 != 0u);
    uint px24 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v24 = px24 & 0xFFu;
    uint v24_1 = (px24 >> 8u) & 0xFFu;
    uint v24_2 = (px24 >> 16u) & 0xFFu;
    uint v24_3 = px24 >> 24u;
    float v25 = (float)(int)v24_3;
    float v26 = v25 * as_type<float>(998277249u);
    float v27 = as_type<float>(v4) - v26;
    float v28 = fma(as_type<float>(v23), v27, v26);
    float v29 = max(v28, as_type<float>(0u));
    float v30 = min(v29, as_type<float>(1065353216u));
    float v31 = v30 * as_type<float>(1132396544u);
    uint v32 = (uint)(int)rint(v31);
    float v33 = (float)(int)v24;
    float v34 = v33 * as_type<float>(998277249u);
    float v35 = as_type<float>(v1) - v34;
    float v36 = fma(as_type<float>(v23), v35, v34);
    float v37 = max(v36, as_type<float>(0u));
    float v38 = min(v37, as_type<float>(1065353216u));
    float v39 = v38 * as_type<float>(1132396544u);
    uint v40 = (uint)(int)rint(v39);
    float v41 = (float)(int)v24_1;
    float v42 = v41 * as_type<float>(998277249u);
    float v43 = as_type<float>(v2) - v42;
    float v44 = fma(as_type<float>(v23), v43, v42);
    float v45 = max(v44, as_type<float>(0u));
    float v46 = min(v45, as_type<float>(1065353216u));
    float v47 = v46 * as_type<float>(1132396544u);
    uint v48 = (uint)(int)rint(v47);
    float v49 = (float)(int)v24_2;
    float v50 = v49 * as_type<float>(998277249u);
    float v51 = as_type<float>(v3) - v50;
    float v52 = fma(as_type<float>(v23), v51, v50);
    float v53 = max(v52, as_type<float>(0u));
    float v54 = min(v53, as_type<float>(1065353216u));
    float v55 = v54 * as_type<float>(1132396544u);
    uint v56 = (uint)(int)rint(v55);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = (v40 & 0xFFu) | ((v48 & 0xFFu) << 8u) | ((v56 & 0xFFu) << 16u) | (v32 << 24u);
}
