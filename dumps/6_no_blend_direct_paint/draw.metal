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
    float v29 = as_type<float>(v4) - v28;
    float v30 = fma(as_type<float>(v24), v29, v28);
    float v31 = max(v30, as_type<float>(0u));
    float v32 = min(v31, as_type<float>(1065353216u));
    float v33 = v32 * as_type<float>(1132396544u);
    uint v34 = (uint)(int)rint(v33);
    uint v35 = v34 << 24u;
    uint v36 = v25 >> 8u;
    uint v37 = v36 & 255u;
    float v38 = (float)(int)v37;
    float v39 = v38 * as_type<float>(998277249u);
    float v40 = as_type<float>(v2) - v39;
    float v41 = fma(as_type<float>(v24), v40, v39);
    float v42 = max(v41, as_type<float>(0u));
    float v43 = min(v42, as_type<float>(1065353216u));
    float v44 = v43 * as_type<float>(1132396544u);
    uint v45 = (uint)(int)rint(v44);
    uint v46 = v45 << 8u;
    uint v47 = v25 >> 16u;
    uint v48 = v47 & 255u;
    float v49 = (float)(int)v48;
    float v50 = v49 * as_type<float>(998277249u);
    float v51 = as_type<float>(v3) - v50;
    float v52 = fma(as_type<float>(v24), v51, v50);
    float v53 = max(v52, as_type<float>(0u));
    float v54 = min(v53, as_type<float>(1065353216u));
    float v55 = v54 * as_type<float>(1132396544u);
    uint v56 = (uint)(int)rint(v55);
    uint v57 = v56 << 16u;
    uint v58 = v25 & 255u;
    float v59 = (float)(int)v58;
    float v60 = v59 * as_type<float>(998277249u);
    float v61 = as_type<float>(v1) - v60;
    float v62 = fma(as_type<float>(v24), v61, v60);
    float v63 = max(v62, as_type<float>(0u));
    float v64 = min(v63, as_type<float>(1065353216u));
    float v65 = v64 * as_type<float>(1132396544u);
    uint v66 = (uint)(int)rint(v65);
    uint v67 = v66 | v46;
    uint v68 = v67 | v57;
    uint v69 = v68 | v35;
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v69;
}
