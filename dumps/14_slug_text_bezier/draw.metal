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
    uint v6 = 1065353216u;
    uint v7 = 255u;
    uint v8 = 998277249u;
    float v9 = as_type<float>(v6) - as_type<float>(v4);
    uint v10 = ((device uint*)(p2 + y * buf_rbs[2]))[x];
    float v11 = fabs(as_type<float>(v10));
    float v12 = min(v11, as_type<float>(1065353216u));
    uint v13 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v14 = v13 >> 24u;
    float v15 = (float)(int)v14;
    float v16 = v15 * as_type<float>(998277249u);
    float v17 = fma(v16, v9, as_type<float>(v4));
    float v18 = v17 - v16;
    float v19 = fma(v12, v18, v16);
    uint v20 = v13 >> 8u;
    uint v21 = v20 & 255u;
    float v22 = (float)(int)v21;
    float v23 = v22 * as_type<float>(998277249u);
    float v24 = fma(v23, v9, as_type<float>(v2));
    float v25 = v24 - v23;
    float v26 = fma(v12, v25, v23);
    uint v27 = v13 >> 16u;
    uint v28 = v27 & 255u;
    float v29 = (float)(int)v28;
    float v30 = v29 * as_type<float>(998277249u);
    float v31 = fma(v30, v9, as_type<float>(v3));
    float v32 = v31 - v30;
    float v33 = fma(v12, v32, v30);
    uint v34 = v13 & 255u;
    float v35 = (float)(int)v34;
    float v36 = v35 * as_type<float>(998277249u);
    float v37 = fma(v36, v9, as_type<float>(v1));
    float v38 = v37 - v36;
    float v39 = fma(v12, v38, v36);
    {
        uint ri = uint(int(rint(clamp(v39, 0.0f, 1.0f) * 255.0f)));
        uint gi = uint(int(rint(clamp(v26, 0.0f, 1.0f) * 255.0f)));
        uint bi = uint(int(rint(clamp(v33, 0.0f, 1.0f) * 255.0f)));
        uint ai = uint(int(rint(clamp(v19, 0.0f, 1.0f) * 255.0f)));
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = ri | (gi << 8) | (bi << 16) | (ai << 24);
    }
}
