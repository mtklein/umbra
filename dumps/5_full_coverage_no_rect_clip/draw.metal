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
    uint v5 = 1065353216u;
    uint v6 = as_type<uint>(as_type<float>(v5) - as_type<float>(v4));
    uint v7 = ((device uint*)p1)[i];
    uint v8 = v7 >> 24u;
    uint v9 = as_type<uint>((float)(int)v8);
    uint v10 = as_type<uint>(as_type<float>(v9) * as_type<float>(998277249u));
    uint v11 = as_type<uint>(fma(as_type<float>(v10), as_type<float>(v6), as_type<float>(v4)));
    uint v12 = v7 >> 8u;
    uint v13 = v12 & 255u;
    uint v14 = as_type<uint>((float)(int)v13);
    uint v15 = as_type<uint>(as_type<float>(v14) * as_type<float>(998277249u));
    uint v16 = as_type<uint>(fma(as_type<float>(v15), as_type<float>(v6), as_type<float>(v2)));
    uint v17 = as_type<uint>(as_type<float>(v16) * as_type<float>(1132396544u));
    uint v18 = as_type<uint>(as_type<float>(v11) * as_type<float>(1132396544u));
    uint v19 = v7 >> 16u;
    uint v20 = v19 & 255u;
    uint v21 = as_type<uint>((float)(int)v20);
    uint v22 = as_type<uint>(as_type<float>(v21) * as_type<float>(998277249u));
    uint v23 = as_type<uint>(fma(as_type<float>(v22), as_type<float>(v6), as_type<float>(v3)));
    uint v24 = as_type<uint>(as_type<float>(v23) * as_type<float>(1132396544u));
    uint v25 = as_type<uint>((int)rint(as_type<float>(v17)));
    uint v26 = v25 & 255u;
    uint v27 = v7 & 255u;
    uint v28 = as_type<uint>((float)(int)v27);
    uint v29 = as_type<uint>(as_type<float>(v28) * as_type<float>(998277249u));
    uint v30 = as_type<uint>(fma(as_type<float>(v29), as_type<float>(v6), as_type<float>(v1)));
    uint v31 = as_type<uint>(as_type<float>(v30) * as_type<float>(1132396544u));
    uint v32 = as_type<uint>((int)rint(as_type<float>(v31)));
    uint v33 = v32 & 255u;
    uint v34 = v33 | (v26 << 8u);
    uint v35 = as_type<uint>((int)rint(as_type<float>(v24)));
    uint v36 = v35 & 255u;
    uint v37 = v34 | (v36 << 16u);
    uint v38 = as_type<uint>((int)rint(as_type<float>(v18)));
    uint v39 = v37 | (v38 << 24u);
    ((device uint*)p1)[i] = v39;
}
