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
    uint v13 = as_type<uint>(as_type<float>(v6) * as_type<float>(v12));
    uint v14 = as_type<uint>(fma(as_type<float>(v13), as_type<float>(v8), as_type<float>(v4)));
    uint v15 = as_type<uint>(as_type<float>(v14) * as_type<float>(v9));
    uint v16 = as_type<uint>((int)rint(as_type<float>(v15)));
    uint v17 = v10 & v5;
    uint v18 = as_type<uint>((float)(int)v17);
    uint v19 = v10 >> 8u;
    uint v20 = v5 & v19;
    uint v21 = as_type<uint>((float)(int)v20);
    uint v22 = v10 >> 16u;
    uint v23 = v5 & v22;
    uint v24 = as_type<uint>((float)(int)v23);
    uint v25 = as_type<uint>(as_type<float>(v6) * as_type<float>(v18));
    uint v26 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v8), as_type<float>(v1)));
    uint v27 = as_type<uint>(as_type<float>(v6) * as_type<float>(v21));
    uint v28 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v8), as_type<float>(v2)));
    uint v29 = as_type<uint>(as_type<float>(v6) * as_type<float>(v24));
    uint v30 = as_type<uint>(fma(as_type<float>(v29), as_type<float>(v8), as_type<float>(v3)));
    uint v31 = as_type<uint>(as_type<float>(v26) * as_type<float>(v9));
    uint v32 = as_type<uint>((int)rint(as_type<float>(v31)));
    uint v33 = as_type<uint>(as_type<float>(v28) * as_type<float>(v9));
    uint v34 = as_type<uint>((int)rint(as_type<float>(v33)));
    uint v35 = as_type<uint>(as_type<float>(v30) * as_type<float>(v9));
    uint v36 = as_type<uint>((int)rint(as_type<float>(v35)));
    uint v37 = v5 & v36;
    uint v38 = v5 & v32;
    uint v39 = v5 & v34;
    uint v40 = v39 << 8u;
    uint v41 = v38 | v40;
    uint v42 = v37 << 16u;
    uint v43 = v41 | v42;
    uint v44 = v16 << 24u;
    uint v45 = v43 | v44;
    ((device uint*)p1)[i] = v45;
}
