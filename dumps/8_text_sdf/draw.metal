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
    uint v10 = 255u;
    float v11 = as_type<float>(v9) - as_type<float>(v4);
    uint v12 = 1132396544u;
    uint v13 = (uint)((device ushort*)(p2 + y * buf_rbs[2]))[x];
    uint v14 = (uint)(int)(short)(ushort)v13;
    float v15 = (float)(int)v14;
    float v16 = v15 * as_type<float>(998277249u);
    float v17 = v16 - as_type<float>(1054867456u);
    float v18 = v17 * as_type<float>(1090519040u);
    float v19 = max(v18, as_type<float>(0u));
    float v20 = min(v19, as_type<float>(1065353216u));
    uint v21 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v22 = v21 >> 24u;
    float v23 = (float)(int)v22;
    float v24 = v23 * as_type<float>(998277249u);
    float v25 = fma(v24, v11, as_type<float>(v4));
    float v26 = v25 - v24;
    float v27 = fma(v20, v26, v24);
    float v28 = max(v27, as_type<float>(0u));
    float v29 = min(v28, as_type<float>(1065353216u));
    float v30 = v29 * as_type<float>(1132396544u);
    uint v31 = (uint)(int)rint(v30);
    uint v32 = v21 >> 8u;
    uint v33 = v32 & 255u;
    float v34 = (float)(int)v33;
    float v35 = v34 * as_type<float>(998277249u);
    float v36 = fma(v35, v11, as_type<float>(v2));
    float v37 = v36 - v35;
    float v38 = fma(v20, v37, v35);
    float v39 = max(v38, as_type<float>(0u));
    float v40 = min(v39, as_type<float>(1065353216u));
    float v41 = v40 * as_type<float>(1132396544u);
    uint v42 = (uint)(int)rint(v41);
    uint v43 = v21 >> 16u;
    uint v44 = v43 & 255u;
    float v45 = (float)(int)v44;
    float v46 = v45 * as_type<float>(998277249u);
    float v47 = fma(v46, v11, as_type<float>(v3));
    float v48 = v47 - v46;
    float v49 = fma(v20, v48, v46);
    float v50 = max(v49, as_type<float>(0u));
    float v51 = min(v50, as_type<float>(1065353216u));
    float v52 = v51 * as_type<float>(1132396544u);
    uint v53 = (uint)(int)rint(v52);
    uint v54 = v21 & 255u;
    float v55 = (float)(int)v54;
    float v56 = v55 * as_type<float>(998277249u);
    float v57 = fma(v56, v11, as_type<float>(v1));
    float v58 = v57 - v56;
    float v59 = fma(v20, v58, v56);
    float v60 = max(v59, as_type<float>(0u));
    float v61 = min(v60, as_type<float>(1065353216u));
    float v62 = v61 * as_type<float>(1132396544u);
    uint v63 = (uint)(int)rint(v62);
    uint v64 = v63 | (v42 << 8u);
    uint v65 = v64 | (v53 << 16u);
    uint v66 = v65 | (v31 << 24u);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v66;
}
