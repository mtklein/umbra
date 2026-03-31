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
    uint v17 = y0 + pos.y;
    float v18 = (float)(int)v17;
    float v19 = v18 * as_type<float>(v3);
    float v20 = fma(v16, as_type<float>(v2), v19);
    float v21 = as_type<float>(v4) + v20;
    float v22 = max(v21, as_type<float>(0u));
    float v23 = min(v22, as_type<float>(1065353216u));
    float v24 = v23 * v8;
    float v25 = floor(v24);
    float v26 = min(v9, v25);
    uint v27 = (uint)(int)v26;
    uint v28 = v27 << 2u;
    uint v29 = v28 + 3u;
    uint v30 = ((device uint*)p2)[safe_ix((int)v29,buf_szs[2],4)] & oob_mask((int)v29,buf_szs[2],4);
    uint v31 = v28 + 4u;
    uint v32 = v31 + 3u;
    uint v33 = ((device uint*)p2)[safe_ix((int)v32,buf_szs[2],4)] & oob_mask((int)v32,buf_szs[2],4);
    float v34 = as_type<float>(v33) - as_type<float>(v30);
    uint v35 = v31 + 1u;
    uint v36 = ((device uint*)p2)[safe_ix((int)v35,buf_szs[2],4)] & oob_mask((int)v35,buf_szs[2],4);
    uint v37 = v31 + 2u;
    uint v38 = ((device uint*)p2)[safe_ix((int)v37,buf_szs[2],4)] & oob_mask((int)v37,buf_szs[2],4);
    float v39 = v24 - v26;
    float v40 = fma(v39, v34, as_type<float>(v30));
    float v41 = max(v40, as_type<float>(0u));
    float v42 = min(v41, as_type<float>(1065353216u));
    float v43 = v42 * as_type<float>(1132396544u);
    uint v44 = (uint)(int)rint(v43);
    uint v45 = v44 << 24u;
    uint v46 = v28 + 1u;
    uint v47 = ((device uint*)p2)[safe_ix((int)v46,buf_szs[2],4)] & oob_mask((int)v46,buf_szs[2],4);
    float v48 = as_type<float>(v36) - as_type<float>(v47);
    float v49 = fma(v39, v48, as_type<float>(v47));
    float v50 = max(v49, as_type<float>(0u));
    float v51 = min(v50, as_type<float>(1065353216u));
    float v52 = v51 * as_type<float>(1132396544u);
    uint v53 = (uint)(int)rint(v52);
    uint v54 = v53 << 8u;
    uint v55 = v28 + 2u;
    uint v56 = ((device uint*)p2)[safe_ix((int)v55,buf_szs[2],4)] & oob_mask((int)v55,buf_szs[2],4);
    float v57 = as_type<float>(v38) - as_type<float>(v56);
    float v58 = fma(v39, v57, as_type<float>(v56));
    float v59 = max(v58, as_type<float>(0u));
    float v60 = min(v59, as_type<float>(1065353216u));
    float v61 = v60 * as_type<float>(1132396544u);
    uint v62 = (uint)(int)rint(v61);
    uint v63 = v62 << 16u;
    uint v64 = ((device uint*)p2)[safe_ix((int)v31,buf_szs[2],4)] & oob_mask((int)v31,buf_szs[2],4);
    uint v65 = ((device uint*)p2)[safe_ix((int)v28,buf_szs[2],4)] & oob_mask((int)v28,buf_szs[2],4);
    float v66 = as_type<float>(v64) - as_type<float>(v65);
    float v67 = fma(v39, v66, as_type<float>(v65));
    float v68 = max(v67, as_type<float>(0u));
    float v69 = min(v68, as_type<float>(1065353216u));
    float v70 = v69 * as_type<float>(1132396544u);
    uint v71 = (uint)(int)rint(v70);
    uint v72 = v71 | v54;
    uint v73 = v72 | v63;
    uint v74 = v73 | v45;
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v74;
}
