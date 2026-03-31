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
    float v20 = v19 - as_type<float>(v1);
    uint v21 = y0 + pos.y;
    float v22 = (float)(int)v21;
    float v23 = v22 - as_type<float>(v2);
    float v24 = v23 * v23;
    float v25 = fma(v20, v20, v24);
    float v26 = precise::sqrt(v25);
    float v27 = as_type<float>(v3) * v26;
    float v28 = max(v27, as_type<float>(0u));
    float v29 = min(v28, as_type<float>(1065353216u));
    float v30 = fma(v29, v16, as_type<float>(v8));
    float v31 = max(v30, as_type<float>(0u));
    float v32 = min(v31, as_type<float>(1065353216u));
    float v33 = v32 * as_type<float>(1132396544u);
    uint v34 = (uint)(int)rint(v33);
    float v35 = fma(v29, v13, as_type<float>(v5));
    float v36 = max(v35, as_type<float>(0u));
    float v37 = min(v36, as_type<float>(1065353216u));
    float v38 = v37 * as_type<float>(1132396544u);
    uint v39 = (uint)(int)rint(v38);
    float v40 = fma(v29, v14, as_type<float>(v6));
    float v41 = max(v40, as_type<float>(0u));
    float v42 = min(v41, as_type<float>(1065353216u));
    float v43 = v42 * as_type<float>(1132396544u);
    uint v44 = (uint)(int)rint(v43);
    uint v45 = v39 | (v44 << 8u);
    float v46 = fma(v29, v15, as_type<float>(v7));
    float v47 = max(v46, as_type<float>(0u));
    float v48 = min(v47, as_type<float>(1065353216u));
    float v49 = v48 * as_type<float>(1132396544u);
    uint v50 = (uint)(int)rint(v49);
    uint v51 = v45 | (v50 << 16u);
    uint v52 = v51 | (v34 << 24u);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v52;
}
