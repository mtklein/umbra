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
    uint v19 = buf_szs[0] >> 2u;
    uint v20 = i;
    uint v21 = v20 + v1;
    uint v22 = as_type<uint>((float)(int)v21);
    uint v23 = as_type<float>(v8) <= as_type<float>(v22) ? 0xffffffffu : 0u;
    uint v24 = as_type<float>(v22) <  as_type<float>(v10) ? 0xffffffffu : 0u;
    uint v25 = v23 & v24;
    uint v26 = v25 & v14;
    uint v27 = (v26 & v15) | (~v26 & v0);
    uint v28 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v27), as_type<float>(v0)));
    uint v29 = as_type<uint>(fma(as_type<float>(v28), as_type<float>(v16), as_type<float>(v17)));
    uint v30 = (uint)(int)as_type<float>(v29);
    uint v31 = v30 & v18;
    uint v32 = as_type<uint>(fma(as_type<float>(v5), as_type<float>(v27), as_type<float>(v0)));
    uint v33 = as_type<uint>(fma(as_type<float>(v32), as_type<float>(v16), as_type<float>(v17)));
    uint v34 = (uint)(int)as_type<float>(v33);
    uint v35 = v34 & v18;
    uint v36 = v35 << 8u;
    uint v37 = v31 | v36;
    uint v38 = as_type<uint>(fma(as_type<float>(v6), as_type<float>(v27), as_type<float>(v0)));
    uint v39 = as_type<uint>(fma(as_type<float>(v38), as_type<float>(v16), as_type<float>(v17)));
    uint v40 = (uint)(int)as_type<float>(v39);
    uint v41 = v40 & v18;
    uint v42 = v41 << 16u;
    uint v43 = v37 | v42;
    uint v44 = as_type<uint>(fma(as_type<float>(v7), as_type<float>(v27), as_type<float>(v0)));
    uint v45 = as_type<uint>(fma(as_type<float>(v44), as_type<float>(v16), as_type<float>(v17)));
    uint v46 = (uint)(int)as_type<float>(v45);
    uint v47 = v46 << 24u;
    uint v48 = v43 | v47;
    uint v49 = 0xffffffffu;
    uint v50 = v20 <  v19 ? 0xffffffffu : 0u;
    uint v51 = v49 & v50;
    ((device uint*)p0)[i] = v48;
}
