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
    uint v1 = ((device const uint*)p0)[0];
    uint v2 = ((device const uint*)p0)[1];
    uint v3 = ((device const uint*)p0)[2];
    uint v4 = ((device const uint*)p0)[3];
    uint v6 = 998277249u;
    uint v7 = 255u;
    uint v8 = 1065353216u;
    float v9 = as_type<float>(v8) - as_type<float>(v4);
    uint v10 = (uint)((device ushort*)(p2 + y * buf_rbs[2]))[x];
    uint v11 = (uint)(int)(short)(ushort)v10;
    float v12 = (float)(int)v11;
    float v13 = v12 * as_type<float>(998277249u);
    uint v14 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v15 = v14 >> 24u;
    float v16 = (float)(int)v15;
    float v17 = v16 * as_type<float>(998277249u);
    float v18 = fma(v17, v9, as_type<float>(v4));
    float v19 = v18 - v17;
    float v20 = fma(v13, v19, v17);
    uint v21 = v14 >> 8u;
    uint v22 = v21 & 255u;
    float v23 = (float)(int)v22;
    float v24 = v23 * as_type<float>(998277249u);
    float v25 = fma(v24, v9, as_type<float>(v2));
    float v26 = v25 - v24;
    float v27 = fma(v13, v26, v24);
    uint v28 = v14 >> 16u;
    uint v29 = v28 & 255u;
    float v30 = (float)(int)v29;
    float v31 = v30 * as_type<float>(998277249u);
    float v32 = fma(v31, v9, as_type<float>(v3));
    float v33 = v32 - v31;
    float v34 = fma(v13, v33, v31);
    uint v35 = v14 & 255u;
    float v36 = (float)(int)v35;
    float v37 = v36 * as_type<float>(998277249u);
    float v38 = fma(v37, v9, as_type<float>(v1));
    float v39 = v38 - v37;
    float v40 = fma(v13, v39, v37);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = pack_float_to_unorm4x8(clamp(float4(v40,v27,v34,v20), 0.0, 1.0));
}
