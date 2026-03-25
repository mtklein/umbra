#include <metal_stdlib>
using namespace metal;

static inline int clamp_ix(int ix, uint bytes, int elem) {
    int hi = (int)(bytes / (uint)elem) - 1;
    if (hi < 0) hi = 0;
    return clamp(ix, 0, hi);
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
    uint v1 = ((device const uint*)p0)[2];
    uint v2 = ((device const uint*)p0)[3];
    uint v3 = ((device const uint*)p0)[4];
    uint v4 = ((device const uint*)p0)[5];
    uint v5 = ((device const uint*)p0)[6];
    uint v6 = ((device const uint*)p0)[7];
    uint v7 = ((device const uint*)p0)[8];
    uint v8 = ((device const uint*)p0)[9];
    uint v9 = 1065353216u;
    uint v10 = 1132396544u;
    uint v11 = 255u;
    uint v12 = pos.x;
    uint v13 = as_type<uint>((float)(int)v12);
    uint v14 = as_type<float>(v13) <  as_type<float>(v7) ? 0xffffffffu : 0u;
    uint v15 = as_type<float>(v5) <= as_type<float>(v13) ? 0xffffffffu : 0u;
    uint v16 = v15 & v14;
    uint v17 = pos.y;
    uint v18 = as_type<uint>((float)(int)v17);
    uint v19 = as_type<float>(v18) <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v20 = as_type<float>(v6) <= as_type<float>(v18) ? 0xffffffffu : 0u;
    uint v21 = v20 & v19;
    uint v22 = v16 & v21;
    uint v23 = (v22 & v9) | (~v22 & v0);
    uint v24 = as_type<uint>(as_type<float>(v4) * as_type<float>(v23));
    uint v25 = as_type<uint>(as_type<float>(v1) * as_type<float>(v23));
    uint v26 = as_type<uint>(as_type<float>(v2) * as_type<float>(v23));
    uint v27 = as_type<uint>(as_type<float>(v3) * as_type<float>(v23));
    uint v28 = as_type<uint>(as_type<float>(v24) * as_type<float>(v10));
    uint v29 = as_type<uint>((int)rint(as_type<float>(v28)));
    uint v30 = as_type<uint>(as_type<float>(v25) * as_type<float>(v10));
    uint v31 = as_type<uint>((int)rint(as_type<float>(v30)));
    uint v32 = as_type<uint>(as_type<float>(v26) * as_type<float>(v10));
    uint v33 = as_type<uint>((int)rint(as_type<float>(v32)));
    uint v34 = as_type<uint>(as_type<float>(v27) * as_type<float>(v10));
    uint v35 = as_type<uint>((int)rint(as_type<float>(v34)));
    uint v36 = v35 & v11;
    uint v37 = v31 & v11;
    uint v38 = v33 & v11;
    uint v39 = v38 << 8u;
    uint v40 = v37 | v39;
    uint v41 = v36 << 16u;
    uint v42 = v40 | v41;
    uint v43 = v29 << 24u;
    uint v44 = v42 | v43;
    ((device uint*)p1)[i] = v44;
}
