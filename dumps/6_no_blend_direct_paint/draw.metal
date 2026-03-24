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
    uint v1 = ((device const uint*)p1)[0];
    uint v2 = ((device const uint*)p1)[1];
    uint v3 = as_type<uint>((float)(int)v2);
    uint v4 = ((device const uint*)p1)[2];
    uint v5 = ((device const uint*)p1)[3];
    uint v6 = ((device const uint*)p1)[4];
    uint v7 = ((device const uint*)p1)[5];
    uint v8 = ((device const uint*)p1)[6];
    uint v9 = ((device const uint*)p1)[7];
    uint v10 = ((device const uint*)p1)[8];
    uint v11 = ((device const uint*)p1)[9];
    uint v12 = as_type<float>(v9) <= as_type<float>(v3) ? 0xffffffffu : 0u;
    uint v13 = as_type<float>(v3) <  as_type<float>(v11) ? 0xffffffffu : 0u;
    uint v14 = v12 & v13;
    uint v15 = 1065353216u;
    uint v16 = 1132396544u;
    uint v17 = 255u;
    uint v18 = i;
    uint v19 = v18 + v1;
    uint v20 = as_type<uint>((float)(int)v19);
    uint v21 = as_type<float>(v20) <  as_type<float>(v10) ? 0xffffffffu : 0u;
    uint v22 = as_type<float>(v8) <= as_type<float>(v20) ? 0xffffffffu : 0u;
    uint v23 = v22 & v21;
    uint v24 = v23 & v14;
    uint v25 = (v24 & v15) | (~v24 & v0);
    uint v26 = as_type<uint>(as_type<float>(v7) * as_type<float>(v25));
    uint v27 = as_type<uint>(as_type<float>(v4) * as_type<float>(v25));
    uint v28 = as_type<uint>(as_type<float>(v5) * as_type<float>(v25));
    uint v29 = as_type<uint>(as_type<float>(v6) * as_type<float>(v25));
    uint v30 = as_type<uint>(as_type<float>(v26) * as_type<float>(v16));
    uint v31 = as_type<uint>((int)rint(as_type<float>(v30)));
    uint v32 = as_type<uint>(as_type<float>(v27) * as_type<float>(v16));
    uint v33 = as_type<uint>((int)rint(as_type<float>(v32)));
    uint v34 = as_type<uint>(as_type<float>(v28) * as_type<float>(v16));
    uint v35 = as_type<uint>((int)rint(as_type<float>(v34)));
    uint v36 = as_type<uint>(as_type<float>(v29) * as_type<float>(v16));
    uint v37 = as_type<uint>((int)rint(as_type<float>(v36)));
    uint v38 = v37 & v17;
    uint v39 = v33 & v17;
    uint v40 = v35 & v17;
    uint v41 = v40 << 8u;
    uint v42 = v39 | v41;
    uint v43 = v38 << 16u;
    uint v44 = v42 | v43;
    uint v45 = v31 << 24u;
    uint v46 = v44 | v45;
    ((device uint*)p0)[i] = v46;
}
