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
    uint v6 = 1065353216u;
    uint v7 = 998277249u;
    float v8 = as_type<float>(v6) - as_type<float>(v4);
    uint v9 = 1132396544u;
    uint v10 = ((device uint*)(p2 + y * buf_rbs[2]))[x];
    float v11 = fabs(as_type<float>(v10));
    float v12 = min(v11, as_type<float>(1065353216u));
    uint px13 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v13 = px13 & 0xFFu;
    uint v13_1 = (px13 >> 8u) & 0xFFu;
    uint v13_2 = (px13 >> 16u) & 0xFFu;
    uint v13_3 = px13 >> 24u;
    float v14 = (float)(int)v13;
    float v15 = v14 * as_type<float>(998277249u);
    float v16 = fma(v15, v8, as_type<float>(v1));
    float v17 = v16 - v15;
    float v18 = fma(v12, v17, v15);
    float v19 = max(v18, as_type<float>(0u));
    float v20 = min(v19, as_type<float>(1065353216u));
    float v21 = v20 * as_type<float>(1132396544u);
    uint v22 = (uint)(int)rint(v21);
    float v23 = (float)(int)v13_3;
    float v24 = v23 * as_type<float>(998277249u);
    float v25 = fma(v24, v8, as_type<float>(v4));
    float v26 = v25 - v24;
    float v27 = fma(v12, v26, v24);
    float v28 = max(v27, as_type<float>(0u));
    float v29 = min(v28, as_type<float>(1065353216u));
    float v30 = v29 * as_type<float>(1132396544u);
    uint v31 = (uint)(int)rint(v30);
    float v32 = (float)(int)v13_1;
    float v33 = v32 * as_type<float>(998277249u);
    float v34 = fma(v33, v8, as_type<float>(v2));
    float v35 = v34 - v33;
    float v36 = fma(v12, v35, v33);
    float v37 = max(v36, as_type<float>(0u));
    float v38 = min(v37, as_type<float>(1065353216u));
    float v39 = v38 * as_type<float>(1132396544u);
    uint v40 = (uint)(int)rint(v39);
    float v41 = (float)(int)v13_2;
    float v42 = v41 * as_type<float>(998277249u);
    float v43 = fma(v42, v8, as_type<float>(v3));
    float v44 = v43 - v42;
    float v45 = fma(v12, v44, v42);
    float v46 = max(v45, as_type<float>(0u));
    float v47 = min(v46, as_type<float>(1065353216u));
    float v48 = v47 * as_type<float>(1132396544u);
    uint v49 = (uint)(int)rint(v48);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = (v22 & 0xFFu) | ((v40 & 0xFFu) << 8u) | ((v49 & 0xFFu) << 16u) | (v31 << 24u);
}
