#include <metal_stdlib>
using namespace metal;

int safe_ix(int ix, uint bytes, int elem) {
    int count = (int)(bytes / (uint)elem);
    return clamp(ix, 0, max(count-1, 0));
}
uint oob_mask(int ix, uint bytes, int elem) {
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
    uint v10 = 998277249u;
    float v11 = as_type<float>(v9) - as_type<float>(v4);
    uint v12 = 1132396544u;
    uint v13 = x0 + pos.x;
    float v14 = (float)(int)v13;
    uint v15 = as_type<float>(v5) <= v14 ? 0xffffffffu : 0u;
    uint v16 = v14 <  as_type<float>(v7) ? 0xffffffffu : 0u;
    uint v17 = v15 & v16;
    uint v18 = y0 + pos.y;
    float v19 = (float)(int)v18;
    uint v20 = as_type<float>(v6) <= v19 ? 0xffffffffu : 0u;
    uint v21 = v19 <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v22 = v20 & v21;
    uint v23 = v17 & v22;
    uint v24 = select(v0, v9, v23 != 0u);
    uint px25 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v25 = px25 & 0xFFu;
    uint v25_1 = (px25 >> 8u) & 0xFFu;
    uint v25_2 = (px25 >> 16u) & 0xFFu;
    uint v25_3 = px25 >> 24u;
    float v26 = (float)(int)v25;
    float v27 = v26 * as_type<float>(998277249u);
    float v28 = v27 * v11;
    float v29 = (float)(int)v25_3;
    float v30 = v29 * as_type<float>(998277249u);
    float v31 = as_type<float>(v9) - v30;
    float v32 = fma(as_type<float>(v1), v31, v28);
    float v33 = fma(as_type<float>(v1), v27, v32);
    float v34 = v33 - v27;
    float v35 = fma(as_type<float>(v24), v34, v27);
    float v36 = max(v35, as_type<float>(0u));
    float v37 = min(v36, as_type<float>(1065353216u));
    float v38 = v37 * as_type<float>(1132396544u);
    uint v39 = (uint)(int)rint(v38);
    float v40 = v30 * v11;
    float v41 = fma(as_type<float>(v4), v31, v40);
    float v42 = fma(as_type<float>(v4), v30, v41);
    float v43 = v42 - v30;
    float v44 = fma(as_type<float>(v24), v43, v30);
    float v45 = max(v44, as_type<float>(0u));
    float v46 = min(v45, as_type<float>(1065353216u));
    float v47 = v46 * as_type<float>(1132396544u);
    uint v48 = (uint)(int)rint(v47);
    float v49 = (float)(int)v25_1;
    float v50 = v49 * as_type<float>(998277249u);
    float v51 = v50 * v11;
    float v52 = fma(as_type<float>(v2), v31, v51);
    float v53 = fma(as_type<float>(v2), v50, v52);
    float v54 = v53 - v50;
    float v55 = fma(as_type<float>(v24), v54, v50);
    float v56 = max(v55, as_type<float>(0u));
    float v57 = min(v56, as_type<float>(1065353216u));
    float v58 = v57 * as_type<float>(1132396544u);
    uint v59 = (uint)(int)rint(v58);
    float v60 = (float)(int)v25_2;
    float v61 = v60 * as_type<float>(998277249u);
    float v62 = v61 * v11;
    float v63 = fma(as_type<float>(v3), v31, v62);
    float v64 = fma(as_type<float>(v3), v61, v63);
    float v65 = v64 - v61;
    float v66 = fma(as_type<float>(v24), v65, v61);
    float v67 = max(v66, as_type<float>(0u));
    float v68 = min(v67, as_type<float>(1065353216u));
    float v69 = v68 * as_type<float>(1132396544u);
    uint v70 = (uint)(int)rint(v69);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = (v39 & 0xFFu) | ((v59 & 0xFFu) << 8u) | ((v70 & 0xFFu) << 16u) | (v48 << 24u);
}
