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
    uint v8 = as_type<uint>(as_type<float>(v7) - as_type<float>(v4));
    uint v9 = 1132396544u;
    uint v10 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v11 = v10 >> 24u;
    uint v12 = as_type<uint>((float)(int)v11);
    uint v13 = as_type<uint>(as_type<float>(v12) * as_type<float>(998277249u));
    uint v14 = as_type<uint>(fma(as_type<float>(v13), as_type<float>(v8), as_type<float>(v4)));
    uint v15 = as_type<uint>(max(as_type<float>(v14), as_type<float>(0u)));
    uint v16 = as_type<uint>(min(as_type<float>(v15), as_type<float>(1065353216u)));
    uint v17 = as_type<uint>(as_type<float>(v16) * as_type<float>(1132396544u));
    uint v18 = as_type<uint>((int)rint(as_type<float>(v17)));
    uint v19 = v10 >> 8u;
    uint v20 = v19 & 255u;
    uint v21 = as_type<uint>((float)(int)v20);
    uint v22 = as_type<uint>(as_type<float>(v21) * as_type<float>(998277249u));
    uint v23 = as_type<uint>(fma(as_type<float>(v22), as_type<float>(v8), as_type<float>(v2)));
    uint v24 = as_type<uint>(max(as_type<float>(v23), as_type<float>(0u)));
    uint v25 = as_type<uint>(min(as_type<float>(v24), as_type<float>(1065353216u)));
    uint v26 = as_type<uint>(as_type<float>(v25) * as_type<float>(1132396544u));
    uint v27 = as_type<uint>((int)rint(as_type<float>(v26)));
    uint v28 = v27 & 255u;
    uint v29 = v10 >> 16u;
    uint v30 = v29 & 255u;
    uint v31 = as_type<uint>((float)(int)v30);
    uint v32 = as_type<uint>(as_type<float>(v31) * as_type<float>(998277249u));
    uint v33 = as_type<uint>(fma(as_type<float>(v32), as_type<float>(v8), as_type<float>(v3)));
    uint v34 = as_type<uint>(max(as_type<float>(v33), as_type<float>(0u)));
    uint v35 = as_type<uint>(min(as_type<float>(v34), as_type<float>(1065353216u)));
    uint v36 = as_type<uint>(as_type<float>(v35) * as_type<float>(1132396544u));
    uint v37 = as_type<uint>((int)rint(as_type<float>(v36)));
    uint v38 = v37 & 255u;
    uint v39 = v10 & 255u;
    uint v40 = as_type<uint>((float)(int)v39);
    uint v41 = as_type<uint>(as_type<float>(v40) * as_type<float>(998277249u));
    uint v42 = as_type<uint>(fma(as_type<float>(v41), as_type<float>(v8), as_type<float>(v1)));
    uint v43 = as_type<uint>(max(as_type<float>(v42), as_type<float>(0u)));
    uint v44 = as_type<uint>(min(as_type<float>(v43), as_type<float>(1065353216u)));
    uint v45 = as_type<uint>(as_type<float>(v44) * as_type<float>(1132396544u));
    uint v46 = as_type<uint>((int)rint(as_type<float>(v45)));
    uint v47 = v46 & 255u;
    uint v48 = v47 | (v28 << 8u);
    uint v49 = v48 | (v38 << 16u);
    uint v50 = v49 | (v18 << 24u);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v50;
}
