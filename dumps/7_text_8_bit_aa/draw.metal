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
    uint v7 = 255u;
    uint v8 = 1065353216u;
    float v9 = as_type<float>(v8) - as_type<float>(v4);
    uint v10 = 1132396544u;
    uint v11 = (uint)((device ushort*)(p2 + y * buf_rbs[2]))[x];
    uint v12 = (uint)(int)(short)(ushort)v11;
    float v13 = (float)(int)v12;
    float v14 = v13 * as_type<float>(998277249u);
    uint v15 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v16 = v15 >> 24u;
    float v17 = (float)(int)v16;
    float v18 = v17 * as_type<float>(998277249u);
    float v19 = fma(v18, v9, as_type<float>(v4));
    float v20 = v19 - v18;
    float v21 = fma(v14, v20, v18);
    float v22 = max(v21, as_type<float>(0u));
    float v23 = min(v22, as_type<float>(1065353216u));
    float v24 = v23 * as_type<float>(1132396544u);
    uint v25 = (uint)(int)rint(v24);
    uint v26 = v15 >> 8u;
    uint v27 = v26 & 255u;
    float v28 = (float)(int)v27;
    float v29 = v28 * as_type<float>(998277249u);
    float v30 = fma(v29, v9, as_type<float>(v2));
    float v31 = v30 - v29;
    float v32 = fma(v14, v31, v29);
    float v33 = max(v32, as_type<float>(0u));
    float v34 = min(v33, as_type<float>(1065353216u));
    float v35 = v34 * as_type<float>(1132396544u);
    uint v36 = (uint)(int)rint(v35);
    uint v37 = v15 >> 16u;
    uint v38 = v37 & 255u;
    float v39 = (float)(int)v38;
    float v40 = v39 * as_type<float>(998277249u);
    float v41 = fma(v40, v9, as_type<float>(v3));
    float v42 = v41 - v40;
    float v43 = fma(v14, v42, v40);
    float v44 = max(v43, as_type<float>(0u));
    float v45 = min(v44, as_type<float>(1065353216u));
    float v46 = v45 * as_type<float>(1132396544u);
    uint v47 = (uint)(int)rint(v46);
    uint v48 = v15 & 255u;
    float v49 = (float)(int)v48;
    float v50 = v49 * as_type<float>(998277249u);
    float v51 = fma(v50, v9, as_type<float>(v1));
    float v52 = v51 - v50;
    float v53 = fma(v14, v52, v50);
    float v54 = max(v53, as_type<float>(0u));
    float v55 = min(v54, as_type<float>(1065353216u));
    float v56 = v55 * as_type<float>(1132396544u);
    uint v57 = (uint)(int)rint(v56);
    uint v58 = v57 | (v36 << 8u);
    uint v59 = v58 | (v47 << 16u);
    uint v60 = v59 | (v25 << 24u);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v60;
}
