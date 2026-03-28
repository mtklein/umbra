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
    uint v5 = 998277249u;
    uint v6 = 1065353216u;
    uint v7 = as_type<uint>(as_type<float>(v6) - as_type<float>(v4));
    uint v8 = 1132396544u;
    uint v9_px = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v9 = v9_px & 0xFFu;
    uint v9_1 = (v9_px >> 8) & 0xFFu;
    uint v9_2 = (v9_px >> 16) & 0xFFu;
    uint v9_3 = v9_px >> 24;
    uint v10 = as_type<uint>((float)(int)v9_3);
    uint v11 = as_type<uint>(as_type<float>(v10) * as_type<float>(998277249u));
    uint v12 = as_type<uint>(fma(as_type<float>(v11), as_type<float>(v7), as_type<float>(v4)));
    uint v13 = as_type<uint>(max(as_type<float>(v12), as_type<float>(0u)));
    uint v14 = as_type<uint>(min(as_type<float>(v13), as_type<float>(1065353216u)));
    uint v15 = as_type<uint>(as_type<float>(v14) * as_type<float>(1132396544u));
    uint v16 = as_type<uint>((int)rint(as_type<float>(v15)));
    uint v17 = as_type<uint>((float)(int)v9);
    uint v18 = as_type<uint>(as_type<float>(v17) * as_type<float>(998277249u));
    uint v19 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v7), as_type<float>(v1)));
    uint v20 = as_type<uint>(max(as_type<float>(v19), as_type<float>(0u)));
    uint v21 = as_type<uint>(min(as_type<float>(v20), as_type<float>(1065353216u)));
    uint v22 = as_type<uint>(as_type<float>(v21) * as_type<float>(1132396544u));
    uint v23 = as_type<uint>((int)rint(as_type<float>(v22)));
    uint v24 = as_type<uint>((float)(int)v9_1);
    uint v25 = as_type<uint>(as_type<float>(v24) * as_type<float>(998277249u));
    uint v26 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v7), as_type<float>(v2)));
    uint v27 = as_type<uint>(max(as_type<float>(v26), as_type<float>(0u)));
    uint v28 = as_type<uint>(min(as_type<float>(v27), as_type<float>(1065353216u)));
    uint v29 = as_type<uint>(as_type<float>(v28) * as_type<float>(1132396544u));
    uint v30 = as_type<uint>((int)rint(as_type<float>(v29)));
    uint v31 = as_type<uint>((float)(int)v9_2);
    uint v32 = as_type<uint>(as_type<float>(v31) * as_type<float>(998277249u));
    uint v33 = as_type<uint>(fma(as_type<float>(v32), as_type<float>(v7), as_type<float>(v3)));
    uint v34 = as_type<uint>(max(as_type<float>(v33), as_type<float>(0u)));
    uint v35 = as_type<uint>(min(as_type<float>(v34), as_type<float>(1065353216u)));
    uint v36 = as_type<uint>(as_type<float>(v35) * as_type<float>(1132396544u));
    uint v37 = as_type<uint>((int)rint(as_type<float>(v36)));
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = (v23 & 0xFFu) | ((v30 & 0xFFu) << 8) | ((v37 & 0xFFu) << 16) | (v16 << 24);
}
