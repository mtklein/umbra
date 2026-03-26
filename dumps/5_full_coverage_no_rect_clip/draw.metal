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
    constant uint &n [[buffer(0)]],
    constant uint &w [[buffer(4)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    constant uint *buf_szs [[buffer(3)]],
    uint2 pos [[thread_position_in_grid]]
) {
    uint i = pos.y * w + pos.x;
    if (i >= n) return;
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
    uint v10 = ((device uint*)p1)[i];
    uint v11 = v10 >> 24u;
    uint v12 = as_type<uint>((float)(int)v11);
    uint v13 = as_type<uint>(as_type<float>(v12) * as_type<float>(998277249u));
    uint v14 = as_type<uint>(fma(as_type<float>(v13), as_type<float>(v8), as_type<float>(v4)));
    uint v15 = as_type<uint>(as_type<float>(v14) * as_type<float>(1132396544u));
    uint v16 = v10 >> 8u;
    uint v17 = v16 & 255u;
    uint v18 = as_type<uint>((float)(int)v17);
    uint v19 = as_type<uint>(as_type<float>(v18) * as_type<float>(998277249u));
    uint v20 = as_type<uint>(fma(as_type<float>(v19), as_type<float>(v8), as_type<float>(v2)));
    uint v21 = as_type<uint>(as_type<float>(v20) * as_type<float>(1132396544u));
    uint v22 = as_type<uint>((int)rint(as_type<float>(v21)));
    uint v23 = v22 & 255u;
    uint v24 = v10 >> 16u;
    uint v25 = v24 & 255u;
    uint v26 = as_type<uint>((float)(int)v25);
    uint v27 = as_type<uint>(as_type<float>(v26) * as_type<float>(998277249u));
    uint v28 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v8), as_type<float>(v3)));
    uint v29 = as_type<uint>(as_type<float>(v28) * as_type<float>(1132396544u));
    uint v30 = as_type<uint>((int)rint(as_type<float>(v29)));
    uint v31 = v30 & 255u;
    uint v32 = as_type<uint>((int)rint(as_type<float>(v15)));
    uint v33 = v10 & 255u;
    uint v34 = as_type<uint>((float)(int)v33);
    uint v35 = as_type<uint>(as_type<float>(v34) * as_type<float>(998277249u));
    uint v36 = as_type<uint>(fma(as_type<float>(v35), as_type<float>(v8), as_type<float>(v1)));
    uint v37 = as_type<uint>(as_type<float>(v36) * as_type<float>(1132396544u));
    uint v38 = as_type<uint>((int)rint(as_type<float>(v37)));
    uint v39 = v38 & 255u;
    uint v40 = v39 | (v23 << 8u);
    uint v41 = v40 | (v31 << 16u);
    uint v42 = v41 | (v32 << 24u);
    ((device uint*)p1)[i] = v42;
}
