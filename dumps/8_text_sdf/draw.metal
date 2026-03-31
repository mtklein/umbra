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
    uint v7 = 1054867456u;
    uint v8 = 1090519040u;
    uint v9 = 1065353216u;
    uint v10 = 255u;
    float v11 = as_type<float>(v9) - as_type<float>(v4);
    uint v12 = (uint)((device ushort*)(p2 + y * buf_rbs[2]))[x];
    uint v13 = (uint)(int)(short)(ushort)v12;
    float v14 = (float)(int)v13;
    float v15 = v14 * as_type<float>(998277249u);
    float v16 = v15 - as_type<float>(1054867456u);
    float v17 = v16 * as_type<float>(1090519040u);
    float v18 = max(v17, as_type<float>(0u));
    float v19 = min(v18, as_type<float>(1065353216u));
    uint v20 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v21 = v20 >> 24u;
    float v22 = (float)(int)v21;
    float v23 = v22 * as_type<float>(998277249u);
    float v24 = fma(v23, v11, as_type<float>(v4));
    float v25 = v24 - v23;
    float v26 = fma(v19, v25, v23);
    uint v27 = v20 >> 8u;
    uint v28 = v27 & 255u;
    float v29 = (float)(int)v28;
    float v30 = v29 * as_type<float>(998277249u);
    float v31 = fma(v30, v11, as_type<float>(v2));
    float v32 = v31 - v30;
    float v33 = fma(v19, v32, v30);
    uint v34 = v20 >> 16u;
    uint v35 = v34 & 255u;
    float v36 = (float)(int)v35;
    float v37 = v36 * as_type<float>(998277249u);
    float v38 = fma(v37, v11, as_type<float>(v3));
    float v39 = v38 - v37;
    float v40 = fma(v19, v39, v37);
    uint v41 = v20 & 255u;
    float v42 = (float)(int)v41;
    float v43 = v42 * as_type<float>(998277249u);
    float v44 = fma(v43, v11, as_type<float>(v1));
    float v45 = v44 - v43;
    float v46 = fma(v19, v45, v43);
    {
        uint ri = uint(int(rint(clamp(v46, 0.0f, 1.0f) * 255.0f)));
        uint gi = uint(int(rint(clamp(v33, 0.0f, 1.0f) * 255.0f)));
        uint bi = uint(int(rint(clamp(v40, 0.0f, 1.0f) * 255.0f)));
        uint ai = uint(int(rint(clamp(v26, 0.0f, 1.0f) * 255.0f)));
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = ri | (gi << 8) | (bi << 16) | (ai << 24);
    }
}
