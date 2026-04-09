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
    uint v5 = 1132396544u;
    uint v6 = 1065353216u;
    float v7 = max(as_type<float>(v1), as_type<float>(0u));
    float v8 = min(v7, as_type<float>(1065353216u));
    float v9 = v8 * as_type<float>(1132396544u);
    uint v10 = (uint)(int)rint(v9);
    float v11 = max(as_type<float>(v2), as_type<float>(0u));
    float v12 = min(v11, as_type<float>(1065353216u));
    float v13 = v12 * as_type<float>(1132396544u);
    uint v14 = (uint)(int)rint(v13);
    float v15 = max(as_type<float>(v3), as_type<float>(0u));
    float v16 = min(v15, as_type<float>(1065353216u));
    float v17 = v16 * as_type<float>(1132396544u);
    uint v18 = (uint)(int)rint(v17);
    float v19 = max(as_type<float>(v4), as_type<float>(0u));
    float v20 = min(v19, as_type<float>(1065353216u));
    float v21 = v20 * as_type<float>(1132396544u);
    uint v22 = (uint)(int)rint(v21);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = (v10 & 0xFFu) | ((v14 & 0xFFu) << 8u) | ((v18 & 0xFFu) << 16u) | (v22 << 24u);
}
