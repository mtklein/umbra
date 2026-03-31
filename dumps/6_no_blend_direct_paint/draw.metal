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
    uint v12 = x0 + pos.x;
    float v13 = (float)(int)v12;
    uint v14 = v13 <  as_type<float>(v7) ? 0xffffffffu : 0u;
    uint v15 = as_type<float>(v5) <= v13 ? 0xffffffffu : 0u;
    uint v16 = v15 & v14;
    uint v17 = y0 + pos.y;
    float v18 = (float)(int)v17;
    uint v19 = v18 <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v20 = as_type<float>(v6) <= v18 ? 0xffffffffu : 0u;
    uint v21 = v20 & v19;
    uint v22 = v16 & v21;
    uint v23 = select(v0, v9, v22 != 0u);
    uint v24 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v25 = v24 >> 24u;
    float v26 = (float)(int)v25;
    float v27 = v26 * as_type<float>(998277249u);
    float v28 = as_type<float>(v4) - v27;
    float v29 = fma(as_type<float>(v23), v28, v27);
    uint v30 = v24 >> 8u;
    uint v31 = v30 & 255u;
    float v32 = (float)(int)v31;
    float v33 = v32 * as_type<float>(998277249u);
    float v34 = as_type<float>(v2) - v33;
    float v35 = fma(as_type<float>(v23), v34, v33);
    uint v36 = v24 >> 16u;
    uint v37 = v36 & 255u;
    float v38 = (float)(int)v37;
    float v39 = v38 * as_type<float>(998277249u);
    float v40 = as_type<float>(v3) - v39;
    float v41 = fma(as_type<float>(v23), v40, v39);
    uint v42 = v24 & 255u;
    float v43 = (float)(int)v42;
    float v44 = v43 * as_type<float>(998277249u);
    float v45 = as_type<float>(v1) - v44;
    float v46 = fma(as_type<float>(v23), v45, v44);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = pack_float_to_unorm4x8(clamp(float4(v46,v35,v41,v29), 0.0, 1.0));
}
