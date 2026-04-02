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
    uint v10 = 255u;
    uint v11 = 998277249u;
    uint v12 = 1132396544u;
    uint v13 = x0 + pos.x;
    float v14 = (float)(int)v13;
    uint v15 = v14 <  as_type<float>(v7) ? 0xffffffffu : 0u;
    uint v16 = as_type<float>(v5) <= v14 ? 0xffffffffu : 0u;
    uint v17 = v16 & v15;
    uint v18 = y0 + pos.y;
    float v19 = (float)(int)v18;
    uint v20 = v19 <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v21 = as_type<float>(v6) <= v19 ? 0xffffffffu : 0u;
    uint v22 = v21 & v20;
    uint v23 = v17 & v22;
    uint v24 = select(v0, v9, v23 != 0u);
    uint v25 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v26 = v25 >> 24u;
    float v27 = (float)(int)v26;
    float v28 = v27 * as_type<float>(998277249u);
    float v29 = as_type<float>(v9) - v28;
    float v30 = fma(as_type<float>(v4), v29, v28);
    float v31 = v30 - v28;
    float v32 = fma(as_type<float>(v24), v31, v28);
    float v33 = max(v32, as_type<float>(0u));
    float v34 = min(v33, as_type<float>(1065353216u));
    float v35 = v34 * as_type<float>(1132396544u);
    uint v36 = (uint)(int)rint(v35);
    uint v37 = v36 << 24u;
    uint v38 = v25 >> 8u;
    uint v39 = v38 & 255u;
    float v40 = (float)(int)v39;
    float v41 = v40 * as_type<float>(998277249u);
    float v42 = fma(as_type<float>(v2), v29, v41);
    float v43 = v42 - v41;
    float v44 = fma(as_type<float>(v24), v43, v41);
    float v45 = max(v44, as_type<float>(0u));
    float v46 = min(v45, as_type<float>(1065353216u));
    float v47 = v46 * as_type<float>(1132396544u);
    uint v48 = (uint)(int)rint(v47);
    uint v49 = v48 << 8u;
    uint v50 = v25 >> 16u;
    uint v51 = v50 & 255u;
    float v52 = (float)(int)v51;
    float v53 = v52 * as_type<float>(998277249u);
    float v54 = fma(as_type<float>(v3), v29, v53);
    float v55 = v54 - v53;
    float v56 = fma(as_type<float>(v24), v55, v53);
    float v57 = max(v56, as_type<float>(0u));
    float v58 = min(v57, as_type<float>(1065353216u));
    float v59 = v58 * as_type<float>(1132396544u);
    uint v60 = (uint)(int)rint(v59);
    uint v61 = v60 << 16u;
    uint v62 = v25 & 255u;
    float v63 = (float)(int)v62;
    float v64 = v63 * as_type<float>(998277249u);
    float v65 = fma(as_type<float>(v1), v29, v64);
    float v66 = v65 - v64;
    float v67 = fma(as_type<float>(v24), v66, v64);
    float v68 = max(v67, as_type<float>(0u));
    float v69 = min(v68, as_type<float>(1065353216u));
    float v70 = v69 * as_type<float>(1132396544u);
    uint v71 = (uint)(int)rint(v70);
    uint v72 = v71 | v49;
    uint v73 = v72 | v61;
    uint v74 = v73 | v37;
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v74;
}
