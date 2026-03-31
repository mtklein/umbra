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
    float v12 = as_type<float>(v9) - as_type<float>(v4);
    uint v13 = 1132396544u;
    uint v14 = x0 + pos.x;
    float v15 = (float)(int)v14;
    uint v16 = v15 <  as_type<float>(v7) ? 0xffffffffu : 0u;
    uint v17 = as_type<float>(v5) <= v15 ? 0xffffffffu : 0u;
    uint v18 = v17 & v16;
    uint v19 = y0 + pos.y;
    float v20 = (float)(int)v19;
    uint v21 = v20 <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v22 = as_type<float>(v6) <= v20 ? 0xffffffffu : 0u;
    uint v23 = v22 & v21;
    uint v24 = v18 & v23;
    uint v25 = select(v0, v9, v24 != 0u);
    uint v26 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v27 = v26 >> 24u;
    float v28 = (float)(int)v27;
    float v29 = v28 * as_type<float>(998277249u);
    float v30 = v29 * v12;
    uint v31 = v26 >> 8u;
    uint v32 = v31 & 255u;
    float v33 = (float)(int)v32;
    float v34 = v33 * as_type<float>(998277249u);
    float v35 = v34 * v12;
    uint v36 = v26 >> 16u;
    uint v37 = v36 & 255u;
    float v38 = (float)(int)v37;
    float v39 = v38 * as_type<float>(998277249u);
    float v40 = v39 * v12;
    uint v41 = v26 & 255u;
    float v42 = (float)(int)v41;
    float v43 = v42 * as_type<float>(998277249u);
    float v44 = v43 * v12;
    float v45 = as_type<float>(v9) - v29;
    float v46 = fma(as_type<float>(v4), v45, v30);
    float v47 = fma(as_type<float>(v4), v29, v46);
    float v48 = v47 - v29;
    float v49 = fma(as_type<float>(v25), v48, v29);
    float v50 = max(v49, as_type<float>(0u));
    float v51 = min(v50, as_type<float>(1065353216u));
    float v52 = v51 * as_type<float>(1132396544u);
    uint v53 = (uint)(int)rint(v52);
    uint v54 = v53 << 24u;
    float v55 = fma(as_type<float>(v1), v45, v44);
    float v56 = fma(as_type<float>(v1), v43, v55);
    float v57 = v56 - v43;
    float v58 = fma(as_type<float>(v25), v57, v43);
    float v59 = max(v58, as_type<float>(0u));
    float v60 = min(v59, as_type<float>(1065353216u));
    float v61 = v60 * as_type<float>(1132396544u);
    uint v62 = (uint)(int)rint(v61);
    float v63 = fma(as_type<float>(v2), v45, v35);
    float v64 = fma(as_type<float>(v2), v34, v63);
    float v65 = v64 - v34;
    float v66 = fma(as_type<float>(v25), v65, v34);
    float v67 = max(v66, as_type<float>(0u));
    float v68 = min(v67, as_type<float>(1065353216u));
    float v69 = v68 * as_type<float>(1132396544u);
    uint v70 = (uint)(int)rint(v69);
    uint v71 = v70 << 8u;
    uint v72 = v62 | v71;
    float v73 = fma(as_type<float>(v3), v45, v40);
    float v74 = fma(as_type<float>(v3), v39, v73);
    float v75 = v74 - v39;
    float v76 = fma(as_type<float>(v25), v75, v39);
    float v77 = max(v76, as_type<float>(0u));
    float v78 = min(v77, as_type<float>(1065353216u));
    float v79 = v78 * as_type<float>(1132396544u);
    uint v80 = (uint)(int)rint(v79);
    uint v81 = v80 << 16u;
    uint v82 = v72 | v81;
    uint v83 = v82 | v54;
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v83;
}
