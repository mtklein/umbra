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
    uint v7 = as_type<uint>(as_type<float>(v3) - as_type<float>(v5));
    uint v8 = as_type<uint>(as_type<float>(v7) * as_type<float>(v7));
    uint v9 = 1065353216u;
    uint v10 = ((device const uint*)p1)[5];
    uint v11 = ((device const uint*)p1)[6];
    uint v12 = ((device const uint*)p1)[7];
    uint v13 = ((device const uint*)p1)[8];
    uint v14 = ((device const uint*)p1)[9];
    uint v15 = ((device const uint*)p1)[10];
    uint v16 = ((device const uint*)p1)[11];
    uint v17 = ((device const uint*)p1)[12];
    uint v18 = as_type<uint>(as_type<float>(v14) - as_type<float>(v10));
    uint v19 = as_type<uint>(as_type<float>(v15) - as_type<float>(v11));
    uint v20 = as_type<uint>(as_type<float>(v16) - as_type<float>(v12));
    uint v21 = as_type<uint>(as_type<float>(v17) - as_type<float>(v13));
    uint v22 = 1132396544u;
    uint v23 = 255u;
    uint v24 = i;
    uint v25 = v24 + v1;
    uint v26 = as_type<uint>((float)(int)v25);
    uint v27 = as_type<uint>(as_type<float>(v26) - as_type<float>(v4));
    uint v28 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v27), as_type<float>(v8)));
    uint v29 = as_type<uint>(sqrt(as_type<float>(v28)));
    uint v30 = as_type<uint>(as_type<float>(v6) * as_type<float>(v29));
    uint v31 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v30)));
    uint v32 = as_type<uint>(min(as_type<float>(v31), as_type<float>(v9)));
    uint v33 = as_type<uint>(fma(as_type<float>(v32), as_type<float>(v21), as_type<float>(v13)));
    uint v34 = as_type<uint>(fma(as_type<float>(v32), as_type<float>(v18), as_type<float>(v10)));
    uint v35 = as_type<uint>(fma(as_type<float>(v32), as_type<float>(v19), as_type<float>(v11)));
    uint v36 = as_type<uint>(fma(as_type<float>(v32), as_type<float>(v20), as_type<float>(v12)));
    uint v37 = as_type<uint>(as_type<float>(v33) * as_type<float>(v22));
    uint v38 = as_type<uint>((int)rint(as_type<float>(v37)));
    uint v39 = as_type<uint>(as_type<float>(v34) * as_type<float>(v22));
    uint v40 = as_type<uint>((int)rint(as_type<float>(v39)));
    uint v41 = as_type<uint>(as_type<float>(v35) * as_type<float>(v22));
    uint v42 = as_type<uint>((int)rint(as_type<float>(v41)));
    uint v43 = as_type<uint>(as_type<float>(v36) * as_type<float>(v22));
    uint v44 = as_type<uint>((int)rint(as_type<float>(v43)));
    uint v45 = v44 & v23;
    uint v46 = v40 & v23;
    uint v47 = v42 & v23;
    uint v48 = v47 << 8u;
    uint v49 = v46 | v48;
    uint v50 = v45 << 16u;
    uint v51 = v49 | v50;
    uint v52 = v38 << 24u;
    uint v53 = v51 | v52;
    ((device uint*)p0)[i] = v53;
}
