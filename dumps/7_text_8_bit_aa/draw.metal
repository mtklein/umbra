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
    uint v26 = v25 << 24u;
    uint v27 = v15 >> 8u;
    uint v28 = v27 & 255u;
    float v29 = (float)(int)v28;
    float v30 = v29 * as_type<float>(998277249u);
    float v31 = fma(v30, v9, as_type<float>(v2));
    float v32 = v31 - v30;
    float v33 = fma(v14, v32, v30);
    float v34 = max(v33, as_type<float>(0u));
    float v35 = min(v34, as_type<float>(1065353216u));
    float v36 = v35 * as_type<float>(1132396544u);
    uint v37 = (uint)(int)rint(v36);
    uint v38 = v37 << 8u;
    uint v39 = v15 >> 16u;
    uint v40 = v39 & 255u;
    float v41 = (float)(int)v40;
    float v42 = v41 * as_type<float>(998277249u);
    float v43 = fma(v42, v9, as_type<float>(v3));
    float v44 = v43 - v42;
    float v45 = fma(v14, v44, v42);
    float v46 = max(v45, as_type<float>(0u));
    float v47 = min(v46, as_type<float>(1065353216u));
    float v48 = v47 * as_type<float>(1132396544u);
    uint v49 = (uint)(int)rint(v48);
    uint v50 = v49 << 16u;
    uint v51 = v15 & 255u;
    float v52 = (float)(int)v51;
    float v53 = v52 * as_type<float>(998277249u);
    float v54 = fma(v53, v9, as_type<float>(v1));
    float v55 = v54 - v53;
    float v56 = fma(v14, v55, v53);
    float v57 = max(v56, as_type<float>(0u));
    float v58 = min(v57, as_type<float>(1065353216u));
    float v59 = v58 * as_type<float>(1132396544u);
    uint v60 = (uint)(int)rint(v59);
    uint v61 = v60 | v38;
    uint v62 = v61 | v50;
    uint v63 = v62 | v26;
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v63;
}
