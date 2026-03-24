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
    uint v2 = ((device const uint*)p1)[2];
    uint v3 = ((device const uint*)p1)[3];
    uint v4 = ((device const uint*)p1)[4];
    uint v5 = ((device const uint*)p1)[5];
    uint v6 = ((device const uint*)p1)[6];
    uint v7 = ((device const uint*)p1)[7];
    uint v8 = ((device const uint*)p1)[8];
    uint v9 = ((device const uint*)p1)[9];
    uint v10 = 1065353216u;
    uint v11 = 1132396544u;
    uint v12 = 255u;
    uint v13 = pos.x;
    uint v14 = as_type<uint>((float)(int)v13);
    uint v15 = as_type<float>(v14) <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v16 = as_type<float>(v6) <= as_type<float>(v14) ? 0xffffffffu : 0u;
    uint v17 = v16 & v15;
    uint v18 = pos.y;
    uint v19 = v18 * v1;
    uint v20 = as_type<uint>((float)(int)v18);
    uint v21 = as_type<float>(v20) <  as_type<float>(v9) ? 0xffffffffu : 0u;
    uint v22 = as_type<float>(v7) <= as_type<float>(v20) ? 0xffffffffu : 0u;
    uint v23 = v22 & v21;
    uint v24 = v17 & v23;
    uint v25 = (v24 & v10) | (~v24 & v0);
    uint v26 = as_type<uint>(as_type<float>(v5) * as_type<float>(v25));
    uint v27 = as_type<uint>(as_type<float>(v2) * as_type<float>(v25));
    uint v28 = as_type<uint>(as_type<float>(v3) * as_type<float>(v25));
    uint v29 = as_type<uint>(as_type<float>(v4) * as_type<float>(v25));
    uint v30 = as_type<uint>(as_type<float>(v26) * as_type<float>(v11));
    uint v31 = as_type<uint>((int)rint(as_type<float>(v30)));
    uint v32 = v13 + v19;
    uint v33 = as_type<uint>(as_type<float>(v27) * as_type<float>(v11));
    uint v34 = as_type<uint>((int)rint(as_type<float>(v33)));
    uint v35 = as_type<uint>(as_type<float>(v28) * as_type<float>(v11));
    uint v36 = as_type<uint>((int)rint(as_type<float>(v35)));
    uint v37 = as_type<uint>(as_type<float>(v29) * as_type<float>(v11));
    uint v38 = as_type<uint>((int)rint(as_type<float>(v37)));
    uint v39 = v38 & v12;
    uint v40 = v34 & v12;
    uint v41 = v36 & v12;
    uint v42 = v41 << 8u;
    uint v43 = v40 | v42;
    uint v44 = v39 << 16u;
    uint v45 = v43 | v44;
    uint v46 = v31 << 24u;
    uint v47 = v45 | v46;
    ((device uint*)p0)[clamp_ix((int)v32,buf_szs[0],4)] = v47;
}
