#include <metal_stdlib>
using namespace metal;

static inline int clamp_ix(int ix, uint bytes, int elem) {
    int hi = (int)(bytes / (uint)elem) - 1;
    if (hi < 0) hi = 0;
    return clamp(ix, 0, hi);
}

kernel void umbra_entry(
    constant uint &n [[buffer(0)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    constant uint *buf_szs [[buffer(3)]],
    uint i [[thread_position_in_grid]]
) {
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
    uint v17 = 1056964608u;
    uint v18 = 255u;
    uint v19 = i;
    uint v20 = v19 + v1;
    uint v21 = as_type<uint>((float)(int)v20);
    uint v22 = as_type<float>(v21) <  as_type<float>(v10) ? 0xffffffffu : 0u;
    uint v23 = as_type<float>(v8) <= as_type<float>(v21) ? 0xffffffffu : 0u;
    uint v24 = v23 & v22;
    uint v25 = v24 & v14;
    uint v26 = (v25 & v15) | (~v25 & v0);
    uint v27 = as_type<uint>(as_type<float>(v7) * as_type<float>(v26));
    uint v28 = as_type<uint>(as_type<float>(v4) * as_type<float>(v26));
    uint v29 = as_type<uint>(as_type<float>(v5) * as_type<float>(v26));
    uint v30 = as_type<uint>(as_type<float>(v6) * as_type<float>(v26));
    uint v31 = as_type<uint>(as_type<float>(v27) * as_type<float>(v16));
    uint v32 = as_type<uint>(as_type<float>(v17) + as_type<float>(v31));
    uint v33 = (uint)(int)as_type<float>(v32);
    uint v34 = as_type<uint>(as_type<float>(v28) * as_type<float>(v16));
    uint v35 = as_type<uint>(as_type<float>(v17) + as_type<float>(v34));
    uint v36 = (uint)(int)as_type<float>(v35);
    uint v37 = as_type<uint>(as_type<float>(v29) * as_type<float>(v16));
    uint v38 = as_type<uint>(as_type<float>(v17) + as_type<float>(v37));
    uint v39 = (uint)(int)as_type<float>(v38);
    uint v40 = as_type<uint>(as_type<float>(v30) * as_type<float>(v16));
    uint v41 = as_type<uint>(as_type<float>(v17) + as_type<float>(v40));
    uint v42 = (uint)(int)as_type<float>(v41);
    uint v43 = v42 & v18;
    uint v44 = v36 & v18;
    uint v45 = v39 & v18;
    uint v46 = v45 << 8u;
    uint v47 = v44 | v46;
    uint v48 = v43 << 16u;
    uint v49 = v47 | v48;
    uint v50 = v33 << 24u;
    uint v51 = v49 | v50;
    ((device uint*)p0)[i] = v51;
}
