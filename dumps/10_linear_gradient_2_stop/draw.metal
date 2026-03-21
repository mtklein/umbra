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
    uint v7 = as_type<uint>(as_type<float>(v3) * as_type<float>(v5));
    uint v8 = 1065353216u;
    uint v9 = ((device const uint*)p1)[5];
    uint v10 = ((device const uint*)p1)[6];
    uint v11 = ((device const uint*)p1)[7];
    uint v12 = ((device const uint*)p1)[8];
    uint v13 = ((device const uint*)p1)[9];
    uint v14 = ((device const uint*)p1)[10];
    uint v15 = ((device const uint*)p1)[11];
    uint v16 = ((device const uint*)p1)[12];
    uint v17 = as_type<uint>(as_type<float>(v13) - as_type<float>(v9));
    uint v18 = as_type<uint>(as_type<float>(v14) - as_type<float>(v10));
    uint v19 = as_type<uint>(as_type<float>(v15) - as_type<float>(v11));
    uint v20 = as_type<uint>(as_type<float>(v16) - as_type<float>(v12));
    uint v21 = 1132396544u;
    uint v22 = 255u;
    uint v23 = i;
    uint v24 = v23 + v1;
    uint v25 = as_type<uint>((float)(int)v24);
    uint v26 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v4), as_type<float>(v7)));
    uint v27 = as_type<uint>(as_type<float>(v6) + as_type<float>(v26));
    uint v28 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v27)));
    uint v29 = as_type<uint>(min(as_type<float>(v28), as_type<float>(v8)));
    uint v30 = as_type<uint>(fma(as_type<float>(v29), as_type<float>(v20), as_type<float>(v12)));
    uint v31 = as_type<uint>(fma(as_type<float>(v29), as_type<float>(v17), as_type<float>(v9)));
    uint v32 = as_type<uint>(fma(as_type<float>(v29), as_type<float>(v18), as_type<float>(v10)));
    uint v33 = as_type<uint>(fma(as_type<float>(v29), as_type<float>(v19), as_type<float>(v11)));
    uint v34 = as_type<uint>(as_type<float>(v30) * as_type<float>(v21));
    uint v35 = as_type<uint>((int)rint(as_type<float>(v34)));
    uint v36 = as_type<uint>(as_type<float>(v31) * as_type<float>(v21));
    uint v37 = as_type<uint>((int)rint(as_type<float>(v36)));
    uint v38 = as_type<uint>(as_type<float>(v32) * as_type<float>(v21));
    uint v39 = as_type<uint>((int)rint(as_type<float>(v38)));
    uint v40 = as_type<uint>(as_type<float>(v33) * as_type<float>(v21));
    uint v41 = as_type<uint>((int)rint(as_type<float>(v40)));
    uint v42 = v41 & v22;
    uint v43 = v37 & v22;
    uint v44 = v39 & v22;
    uint v45 = v44 << 8u;
    uint v46 = v43 | v45;
    uint v47 = v42 << 16u;
    uint v48 = v46 | v47;
    uint v49 = v35 << 24u;
    uint v50 = v48 | v49;
    ((device uint*)p0)[i] = v50;
}
