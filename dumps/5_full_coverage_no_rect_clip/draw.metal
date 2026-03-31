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
    uint v5 = 255u;
    uint v6 = 998277249u;
    uint v7 = 1065353216u;
    float v8 = as_type<float>(v7) - as_type<float>(v4);
    uint v9 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v10 = v9 >> 24u;
    float v11 = (float)(int)v10;
    float v12 = v11 * as_type<float>(998277249u);
    float v13 = fma(v12, v8, as_type<float>(v4));
    uint v14 = v9 >> 8u;
    uint v15 = v14 & 255u;
    float v16 = (float)(int)v15;
    float v17 = v16 * as_type<float>(998277249u);
    float v18 = fma(v17, v8, as_type<float>(v2));
    uint v19 = v9 >> 16u;
    uint v20 = v19 & 255u;
    float v21 = (float)(int)v20;
    float v22 = v21 * as_type<float>(998277249u);
    float v23 = fma(v22, v8, as_type<float>(v3));
    uint v24 = v9 & 255u;
    float v25 = (float)(int)v24;
    float v26 = v25 * as_type<float>(998277249u);
    float v27 = fma(v26, v8, as_type<float>(v1));
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = pack_float_to_unorm4x8(clamp(float4(v27,v18,v23,v13), 0.0, 1.0));
}
