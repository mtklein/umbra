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
    float v7 = as_type<float>(v6) - as_type<float>(1065353216u);
    uint v8 = 1073741824u;
    float v9 = as_type<float>(v6) - as_type<float>(1073741824u);
    uint v10 = 1u;
    uint v11 = (uint)(int)as_type<float>(v6);
    uint v12 = v11 + v11;
    uint v13 = v11 + v12;
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
    float v24 = v23 * v7;
    float v25 = floor(v24);
    float v26 = min(v9, v25);
    uint v27 = (uint)(int)v26;
    uint v28 = v27 + 1u;
    uint v29 = v28 + v13;
    uint v30 = ((device uint*)p2)[safe_ix((int)v29,buf_szs[2],4)] & oob_mask((int)v29,buf_szs[2],4);
    uint v31 = v28 + v11;
    uint v32 = ((device uint*)p2)[safe_ix((int)v31,buf_szs[2],4)] & oob_mask((int)v31,buf_szs[2],4);
    uint v33 = v28 + v12;
    uint v34 = ((device uint*)p2)[safe_ix((int)v33,buf_szs[2],4)] & oob_mask((int)v33,buf_szs[2],4);
    uint v35 = v27 + v13;
    uint v36 = ((device uint*)p2)[safe_ix((int)v35,buf_szs[2],4)] & oob_mask((int)v35,buf_szs[2],4);
    float v37 = as_type<float>(v30) - as_type<float>(v36);
    float v38 = v24 - v26;
    float v39 = fma(v38, v37, as_type<float>(v36));
    float v40 = max(v39, as_type<float>(0u));
    float v41 = min(v40, as_type<float>(1065353216u));
    float v42 = v41 * as_type<float>(1132396544u);
    uint v43 = (uint)(int)rint(v42);
    uint v44 = ((device uint*)p2)[safe_ix((int)v28,buf_szs[2],4)] & oob_mask((int)v28,buf_szs[2],4);
    uint v45 = ((device uint*)p2)[safe_ix((int)v27,buf_szs[2],4)] & oob_mask((int)v27,buf_szs[2],4);
    float v46 = as_type<float>(v44) - as_type<float>(v45);
    float v47 = fma(v38, v46, as_type<float>(v45));
    float v48 = max(v47, as_type<float>(0u));
    float v49 = min(v48, as_type<float>(1065353216u));
    float v50 = v49 * as_type<float>(1132396544u);
    uint v51 = (uint)(int)rint(v50);
    uint v52 = v27 + v11;
    uint v53 = ((device uint*)p2)[safe_ix((int)v52,buf_szs[2],4)] & oob_mask((int)v52,buf_szs[2],4);
    float v54 = as_type<float>(v32) - as_type<float>(v53);
    float v55 = fma(v38, v54, as_type<float>(v53));
    float v56 = max(v55, as_type<float>(0u));
    float v57 = min(v56, as_type<float>(1065353216u));
    float v58 = v57 * as_type<float>(1132396544u);
    uint v59 = (uint)(int)rint(v58);
    uint v60 = v51 | (v59 << 8u);
    uint v61 = v27 + v12;
    uint v62 = ((device uint*)p2)[safe_ix((int)v61,buf_szs[2],4)] & oob_mask((int)v61,buf_szs[2],4);
    float v63 = as_type<float>(v34) - as_type<float>(v62);
    float v64 = fma(v38, v63, as_type<float>(v62));
    float v65 = max(v64, as_type<float>(0u));
    float v66 = min(v65, as_type<float>(1065353216u));
    float v67 = v66 * as_type<float>(1132396544u);
    uint v68 = (uint)(int)rint(v67);
    uint v69 = v60 | (v68 << 16u);
    uint v70 = v69 | (v43 << 24u);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v70;
}
