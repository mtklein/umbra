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
    uint v4 = 1065353216u;
    uint v5 = ((device const uint*)p0)[3];
    uint v6 = ((device const uint*)p0)[4];
    uint v7 = ((device const uint*)p0)[5];
    uint v8 = ((device const uint*)p0)[6];
    uint v9 = ((device const uint*)p0)[7];
    uint v10 = ((device const uint*)p0)[8];
    uint v11 = ((device const uint*)p0)[9];
    uint v12 = ((device const uint*)p0)[10];
    float v13 = as_type<float>(v9) - as_type<float>(v5);
    float v14 = as_type<float>(v10) - as_type<float>(v6);
    float v15 = as_type<float>(v11) - as_type<float>(v7);
    float v16 = as_type<float>(v12) - as_type<float>(v8);
    uint v17 = 1132396544u;
    uint v18 = x0 + pos.x;
    float v19 = (float)(int)v18;
    uint v20 = y0 + pos.y;
    float v21 = (float)(int)v20;
    float v22 = v21 * as_type<float>(v2);
    float v23 = fma(v19, as_type<float>(v1), v22);
    float v24 = as_type<float>(v3) + v23;
    float v25 = max(v24, as_type<float>(0u));
    float v26 = min(v25, as_type<float>(1065353216u));
    float v27 = fma(v26, v16, as_type<float>(v8));
    float v28 = max(v27, as_type<float>(0u));
    float v29 = min(v28, as_type<float>(1065353216u));
    float v30 = v29 * as_type<float>(1132396544u);
    uint v31 = (uint)(int)rint(v30);
    float v32 = fma(v26, v13, as_type<float>(v5));
    float v33 = max(v32, as_type<float>(0u));
    float v34 = min(v33, as_type<float>(1065353216u));
    float v35 = v34 * as_type<float>(1132396544u);
    uint v36 = (uint)(int)rint(v35);
    float v37 = fma(v26, v14, as_type<float>(v6));
    float v38 = max(v37, as_type<float>(0u));
    float v39 = min(v38, as_type<float>(1065353216u));
    float v40 = v39 * as_type<float>(1132396544u);
    uint v41 = (uint)(int)rint(v40);
    uint v42 = v36 | (v41 << 8u);
    float v43 = fma(v26, v15, as_type<float>(v7));
    float v44 = max(v43, as_type<float>(0u));
    float v45 = min(v44, as_type<float>(1065353216u));
    float v46 = v45 * as_type<float>(1132396544u);
    uint v47 = (uint)(int)rint(v46);
    uint v48 = v42 | (v47 << 16u);
    uint v49 = v48 | (v31 << 24u);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v49;
}
