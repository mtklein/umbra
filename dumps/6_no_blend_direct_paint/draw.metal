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
    uint v22 = as_type<float>(v8) <= as_type<float>(v21) ? 0xffffffffu : 0u;
    uint v23 = as_type<float>(v21) <  as_type<float>(v10) ? 0xffffffffu : 0u;
    uint v24 = v22 & v23;
    uint v25 = v24 & v14;
    uint v26 = (v25 & v15) | (~v25 & v0);
    uint v27 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v26), as_type<float>(v0)));
    uint v28 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v16), as_type<float>(v17)));
    uint v29 = (uint)(int)as_type<float>(v28);
    uint v30 = v29 & v18;
    uint v31 = as_type<uint>(fma(as_type<float>(v5), as_type<float>(v26), as_type<float>(v0)));
    uint v32 = as_type<uint>(fma(as_type<float>(v31), as_type<float>(v16), as_type<float>(v17)));
    uint v33 = (uint)(int)as_type<float>(v32);
    uint v34 = v33 & v18;
    uint v35 = v34 << 8u;
    uint v36 = v30 | v35;
    uint v37 = as_type<uint>(fma(as_type<float>(v6), as_type<float>(v26), as_type<float>(v0)));
    uint v38 = as_type<uint>(fma(as_type<float>(v37), as_type<float>(v16), as_type<float>(v17)));
    uint v39 = (uint)(int)as_type<float>(v38);
    uint v40 = v39 & v18;
    uint v41 = v40 << 16u;
    uint v42 = v36 | v41;
    uint v43 = as_type<uint>(fma(as_type<float>(v7), as_type<float>(v26), as_type<float>(v0)));
    uint v44 = as_type<uint>(fma(as_type<float>(v43), as_type<float>(v16), as_type<float>(v17)));
    uint v45 = (uint)(int)as_type<float>(v44);
    uint v46 = v45 << 24u;
    uint v47 = v42 | v46;
    ((device uint*)p0)[i] = v47;
}
