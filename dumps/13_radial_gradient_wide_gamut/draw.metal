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
    uint v2 = ((device const uint*)p0)[0];
    uint v3 = ((device const uint*)p0)[1];
    uint v4 = ((device const uint*)p0)[2];
    uint v5 = 1065353216u;
    uint v6 = ((device const uint*)p0)[3];
    uint v7 = 1073741824u;
    float v8 = as_type<float>(v6) - as_type<float>(1065353216u);
    float v9 = as_type<float>(v6) - as_type<float>(1073741824u);
    uint v10 = 2u;
    uint v11 = 4u;
    uint v12 = 1u;
    uint v13 = 3u;
    uint v14 = 1132396544u;
    uint v15 = x0 + pos.x;
    float v16 = (float)(int)v15;
    float v17 = v16 - as_type<float>(v2);
    uint v18 = y0 + pos.y;
    float v19 = (float)(int)v18;
    float v20 = v19 - as_type<float>(v3);
    float v21 = v20 * v20;
    float v22 = fma(v17, v17, v21);
    float v23 = sqrt(v22);
    float v24 = as_type<float>(v4) * v23;
    float v25 = max(v24, as_type<float>(0u));
    float v26 = min(v25, as_type<float>(1065353216u));
    float v27 = v26 * v8;
    float v28 = floor(v27);
    float v29 = min(v9, v28);
    uint v30 = (uint)(int)v29;
    uint v31 = v30 << 2u;
    uint v32 = v31 + 3u;
    uint v33 = ((device uint*)p2)[safe_ix((int)v32,buf_szs[2],4)] & oob_mask((int)v32,buf_szs[2],4);
    uint v34 = v31 + 4u;
    uint v35 = v34 + 3u;
    uint v36 = ((device uint*)p2)[safe_ix((int)v35,buf_szs[2],4)] & oob_mask((int)v35,buf_szs[2],4);
    float v37 = as_type<float>(v36) - as_type<float>(v33);
    uint v38 = v34 + 1u;
    uint v39 = ((device uint*)p2)[safe_ix((int)v38,buf_szs[2],4)] & oob_mask((int)v38,buf_szs[2],4);
    uint v40 = v34 + 2u;
    uint v41 = ((device uint*)p2)[safe_ix((int)v40,buf_szs[2],4)] & oob_mask((int)v40,buf_szs[2],4);
    float v42 = v27 - v29;
    float v43 = fma(v42, v37, as_type<float>(v33));
    float v44 = max(v43, as_type<float>(0u));
    float v45 = min(v44, as_type<float>(1065353216u));
    float v46 = v45 * as_type<float>(1132396544u);
    uint v47 = (uint)(int)rint(v46);
    uint v48 = v31 + 1u;
    uint v49 = ((device uint*)p2)[safe_ix((int)v48,buf_szs[2],4)] & oob_mask((int)v48,buf_szs[2],4);
    float v50 = as_type<float>(v39) - as_type<float>(v49);
    float v51 = fma(v42, v50, as_type<float>(v49));
    float v52 = max(v51, as_type<float>(0u));
    float v53 = min(v52, as_type<float>(1065353216u));
    float v54 = v53 * as_type<float>(1132396544u);
    uint v55 = (uint)(int)rint(v54);
    uint v56 = v31 + 2u;
    uint v57 = ((device uint*)p2)[safe_ix((int)v56,buf_szs[2],4)] & oob_mask((int)v56,buf_szs[2],4);
    float v58 = as_type<float>(v41) - as_type<float>(v57);
    float v59 = fma(v42, v58, as_type<float>(v57));
    float v60 = max(v59, as_type<float>(0u));
    float v61 = min(v60, as_type<float>(1065353216u));
    float v62 = v61 * as_type<float>(1132396544u);
    uint v63 = (uint)(int)rint(v62);
    uint v64 = ((device uint*)p2)[safe_ix((int)v34,buf_szs[2],4)] & oob_mask((int)v34,buf_szs[2],4);
    uint v65 = ((device uint*)p2)[safe_ix((int)v31,buf_szs[2],4)] & oob_mask((int)v31,buf_szs[2],4);
    float v66 = as_type<float>(v64) - as_type<float>(v65);
    float v67 = fma(v42, v66, as_type<float>(v65));
    float v68 = max(v67, as_type<float>(0u));
    float v69 = min(v68, as_type<float>(1065353216u));
    float v70 = v69 * as_type<float>(1132396544u);
    uint v71 = (uint)(int)rint(v70);
    uint v72 = v71 | (v55 << 8u);
    uint v73 = v72 | (v63 << 16u);
    uint v74 = v73 | (v47 << 24u);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v74;
}
