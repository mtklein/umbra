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
    uint v35 = v25 >> 8u;
    uint v36 = v35 & 255u;
    float v37 = (float)(int)v36;
    float v38 = v37 * as_type<float>(998277249u);
    float v39 = as_type<float>(v2) - v38;
    float v40 = fma(as_type<float>(v24), v39, v38);
    float v41 = max(v40, as_type<float>(0u));
    float v42 = min(v41, as_type<float>(1065353216u));
    float v43 = v42 * as_type<float>(1132396544u);
    uint v44 = (uint)(int)rint(v43);
    uint v45 = v25 >> 16u;
    uint v46 = v45 & 255u;
    float v47 = (float)(int)v46;
    float v48 = v47 * as_type<float>(998277249u);
    float v49 = as_type<float>(v3) - v48;
    float v50 = fma(as_type<float>(v24), v49, v48);
    float v51 = max(v50, as_type<float>(0u));
    float v52 = min(v51, as_type<float>(1065353216u));
    float v53 = v52 * as_type<float>(1132396544u);
    uint v54 = (uint)(int)rint(v53);
    uint v55 = v25 & 255u;
    float v56 = (float)(int)v55;
    float v57 = v56 * as_type<float>(998277249u);
    float v58 = as_type<float>(v1) - v57;
    float v59 = fma(as_type<float>(v24), v58, v57);
    float v60 = max(v59, as_type<float>(0u));
    float v61 = min(v60, as_type<float>(1065353216u));
    float v62 = v61 * as_type<float>(1132396544u);
    uint v63 = (uint)(int)rint(v62);
    uint v64 = v63 | (v44 << 8u);
    uint v65 = v64 | (v54 << 16u);
    uint v66 = v65 | (v34 << 24u);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v66;
}
