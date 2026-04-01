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
    float v17 = v16 - as_type<float>(v2);
    uint v18 = y0 + pos.y;
    float v19 = (float)(int)v18;
    float v20 = v19 - as_type<float>(v3);
    float v21 = v20 * v20;
    float v22 = fma(v17, v17, v21);
    float v23 = precise::sqrt(v22);
    float v24 = as_type<float>(v4) * v23;
    float v25 = max(v24, as_type<float>(0u));
    float v26 = min(v25, as_type<float>(1065353216u));
    float v27 = v26 * v7;
    float v28 = floor(v27);
    float v29 = min(v9, v28);
    uint v30 = (uint)(int)v29;
    uint v31 = v30 + 1u;
    uint v32 = v31 + v13;
    uint v33 = ((device uint*)p2)[safe_ix((int)v32,buf_szs[2],4)] & oob_mask((int)v32,buf_szs[2],4);
    uint v34 = v31 + v11;
    uint v35 = ((device uint*)p2)[safe_ix((int)v34,buf_szs[2],4)] & oob_mask((int)v34,buf_szs[2],4);
    uint v36 = v31 + v12;
    uint v37 = ((device uint*)p2)[safe_ix((int)v36,buf_szs[2],4)] & oob_mask((int)v36,buf_szs[2],4);
    uint v38 = v30 + v13;
    uint v39 = ((device uint*)p2)[safe_ix((int)v38,buf_szs[2],4)] & oob_mask((int)v38,buf_szs[2],4);
    float v40 = as_type<float>(v33) - as_type<float>(v39);
    float v41 = v27 - v29;
    float v42 = fma(v41, v40, as_type<float>(v39));
    float v43 = max(v42, as_type<float>(0u));
    float v44 = min(v43, as_type<float>(1065353216u));
    float v45 = v44 * as_type<float>(1132396544u);
    uint v46 = (uint)(int)rint(v45);
    uint v47 = ((device uint*)p2)[safe_ix((int)v31,buf_szs[2],4)] & oob_mask((int)v31,buf_szs[2],4);
    uint v48 = ((device uint*)p2)[safe_ix((int)v30,buf_szs[2],4)] & oob_mask((int)v30,buf_szs[2],4);
    float v49 = as_type<float>(v47) - as_type<float>(v48);
    float v50 = fma(v41, v49, as_type<float>(v48));
    float v51 = max(v50, as_type<float>(0u));
    float v52 = min(v51, as_type<float>(1065353216u));
    float v53 = v52 * as_type<float>(1132396544u);
    uint v54 = (uint)(int)rint(v53);
    uint v55 = v30 + v11;
    uint v56 = ((device uint*)p2)[safe_ix((int)v55,buf_szs[2],4)] & oob_mask((int)v55,buf_szs[2],4);
    float v57 = as_type<float>(v35) - as_type<float>(v56);
    float v58 = fma(v41, v57, as_type<float>(v56));
    float v59 = max(v58, as_type<float>(0u));
    float v60 = min(v59, as_type<float>(1065353216u));
    float v61 = v60 * as_type<float>(1132396544u);
    uint v62 = (uint)(int)rint(v61);
    uint v63 = v54 | (v62 << 8u);
    uint v64 = v30 + v12;
    uint v65 = ((device uint*)p2)[safe_ix((int)v64,buf_szs[2],4)] & oob_mask((int)v64,buf_szs[2],4);
    float v66 = as_type<float>(v37) - as_type<float>(v65);
    float v67 = fma(v41, v66, as_type<float>(v65));
    float v68 = max(v67, as_type<float>(0u));
    float v69 = min(v68, as_type<float>(1065353216u));
    float v70 = v69 * as_type<float>(1132396544u);
    uint v71 = (uint)(int)rint(v70);
    uint v72 = v63 | (v71 << 16u);
    uint v73 = v72 | (v46 << 24u);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v73;
}
