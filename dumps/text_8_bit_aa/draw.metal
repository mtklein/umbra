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
    uint v6 = 998277249u;
    uint v7 = 1065353216u;
    float v8 = as_type<float>(v7) - as_type<float>(v4);
    uint v9 = 1132396544u;
    uint v10 = (uint)((device ushort*)(p2 + y * buf_rbs[2]))[x];
    uint v11 = (uint)(int)(short)(ushort)v10;
    float v12 = (float)(int)v11;
    float v13 = v12 * as_type<float>(998277249u);
    uint px14 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v14 = px14 & 0xFFu;
    uint v14_1 = (px14 >> 8u) & 0xFFu;
    uint v14_2 = (px14 >> 16u) & 0xFFu;
    uint v14_3 = px14 >> 24u;
    float v15 = (float)(int)v14;
    float v16 = v15 * as_type<float>(998277249u);
    float v17 = fma(v16, v8, as_type<float>(v1));
    float v18 = v17 - v16;
    float v19 = fma(v13, v18, v16);
    float v20 = max(v19, as_type<float>(0u));
    float v21 = min(v20, as_type<float>(1065353216u));
    float v22 = v21 * as_type<float>(1132396544u);
    uint v23 = (uint)(int)rint(v22);
    float v24 = (float)(int)v14_3;
    float v25 = v24 * as_type<float>(998277249u);
    float v26 = fma(v25, v8, as_type<float>(v4));
    float v27 = v26 - v25;
    float v28 = fma(v13, v27, v25);
    float v29 = max(v28, as_type<float>(0u));
    float v30 = min(v29, as_type<float>(1065353216u));
    float v31 = v30 * as_type<float>(1132396544u);
    uint v32 = (uint)(int)rint(v31);
    float v33 = (float)(int)v14_1;
    float v34 = v33 * as_type<float>(998277249u);
    float v35 = fma(v34, v8, as_type<float>(v2));
    float v36 = v35 - v34;
    float v37 = fma(v13, v36, v34);
    float v38 = max(v37, as_type<float>(0u));
    float v39 = min(v38, as_type<float>(1065353216u));
    float v40 = v39 * as_type<float>(1132396544u);
    uint v41 = (uint)(int)rint(v40);
    float v42 = (float)(int)v14_2;
    float v43 = v42 * as_type<float>(998277249u);
    float v44 = fma(v43, v8, as_type<float>(v3));
    float v45 = v44 - v43;
    float v46 = fma(v13, v45, v43);
    float v47 = max(v46, as_type<float>(0u));
    float v48 = min(v47, as_type<float>(1065353216u));
    float v49 = v48 * as_type<float>(1132396544u);
    uint v50 = (uint)(int)rint(v49);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = (v23 & 0xFFu) | ((v41 & 0xFFu) << 8u) | ((v50 & 0xFFu) << 16u) | (v32 << 24u);
}
