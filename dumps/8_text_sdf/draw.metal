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
    uint v32 = v31 << 24u;
    uint v33 = v21 >> 8u;
    uint v34 = v33 & 255u;
    float v35 = (float)(int)v34;
    float v36 = v35 * as_type<float>(998277249u);
    float v37 = fma(v36, v11, as_type<float>(v2));
    float v38 = v37 - v36;
    float v39 = fma(v20, v38, v36);
    float v40 = max(v39, as_type<float>(0u));
    float v41 = min(v40, as_type<float>(1065353216u));
    float v42 = v41 * as_type<float>(1132396544u);
    uint v43 = (uint)(int)rint(v42);
    uint v44 = v43 << 8u;
    uint v45 = v21 >> 16u;
    uint v46 = v45 & 255u;
    float v47 = (float)(int)v46;
    float v48 = v47 * as_type<float>(998277249u);
    float v49 = fma(v48, v11, as_type<float>(v3));
    float v50 = v49 - v48;
    float v51 = fma(v20, v50, v48);
    float v52 = max(v51, as_type<float>(0u));
    float v53 = min(v52, as_type<float>(1065353216u));
    float v54 = v53 * as_type<float>(1132396544u);
    uint v55 = (uint)(int)rint(v54);
    uint v56 = v55 << 16u;
    uint v57 = v21 & 255u;
    float v58 = (float)(int)v57;
    float v59 = v58 * as_type<float>(998277249u);
    float v60 = fma(v59, v11, as_type<float>(v1));
    float v61 = v60 - v59;
    float v62 = fma(v20, v61, v59);
    float v63 = max(v62, as_type<float>(0u));
    float v64 = min(v63, as_type<float>(1065353216u));
    float v65 = v64 * as_type<float>(1132396544u);
    uint v66 = (uint)(int)rint(v65);
    uint v67 = v66 | v44;
    uint v68 = v67 | v56;
    uint v69 = v68 | v32;
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v69;
}
