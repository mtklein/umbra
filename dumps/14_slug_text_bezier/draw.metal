#include <metal_stdlib>
using namespace metal;

static inline int clamp_ix(int ix, uint bytes, int elem) {
    int hi = (int)(bytes / (uint)elem) - 1;
    if (hi < 0) hi = 0;
    return clamp(ix, 0, hi);
}

kernel void umbra_entry(
    constant uint &n [[buffer(0)]],
    constant uint &w [[buffer(5)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    device uchar *p2 [[buffer(3)]],
    constant uint *buf_szs [[buffer(4)]],
    uint2 pos [[thread_position_in_grid]]
) {
    uint i = pos.y * w + pos.x;
    if (i >= n) return;
    uint v0 = 0u;
    uint v1 = ((device const uint*)p0)[2];
    uint v2 = ((device const uint*)p0)[3];
    uint v3 = ((device const uint*)p0)[4];
    uint v4 = ((device const uint*)p0)[5];
    uint v6 = 1065353216u;
    uint v7 = 255u;
    uint v8 = 998277249u;
    uint v9 = as_type<uint>(as_type<float>(v6) - as_type<float>(v4));
    uint v10 = 1132396544u;
    uint v11 = ((device uint*)p2)[i];
    uint v12 = as_type<uint>(fabs(as_type<float>(v11)));
    uint v13 = as_type<uint>(min(as_type<float>(v12), as_type<float>(v6)));
    uint v14 = ((device uint*)p1)[i];
    uint v15 = v14 >> 24u;
    uint v16 = as_type<uint>((float)(int)v15);
    uint v17 = as_type<uint>(as_type<float>(v8) * as_type<float>(v16));
    uint v18 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v9), as_type<float>(v4)));
    uint v19 = as_type<uint>(as_type<float>(v18) - as_type<float>(v17));
    uint v20 = as_type<uint>(fma(as_type<float>(v13), as_type<float>(v19), as_type<float>(v17)));
    uint v21 = as_type<uint>(as_type<float>(v20) * as_type<float>(v10));
    uint v22 = as_type<uint>((int)rint(as_type<float>(v21)));
    uint v23 = v14 & v7;
    uint v24 = as_type<uint>((float)(int)v23);
    uint v25 = v14 >> 8u;
    uint v26 = v7 & v25;
    uint v27 = as_type<uint>((float)(int)v26);
    uint v28 = v14 >> 16u;
    uint v29 = v7 & v28;
    uint v30 = as_type<uint>((float)(int)v29);
    uint v31 = as_type<uint>(as_type<float>(v8) * as_type<float>(v24));
    uint v32 = as_type<uint>(fma(as_type<float>(v31), as_type<float>(v9), as_type<float>(v1)));
    uint v33 = as_type<uint>(as_type<float>(v32) - as_type<float>(v31));
    uint v34 = as_type<uint>(fma(as_type<float>(v13), as_type<float>(v33), as_type<float>(v31)));
    uint v35 = as_type<uint>(as_type<float>(v8) * as_type<float>(v27));
    uint v36 = as_type<uint>(fma(as_type<float>(v35), as_type<float>(v9), as_type<float>(v2)));
    uint v37 = as_type<uint>(as_type<float>(v36) - as_type<float>(v35));
    uint v38 = as_type<uint>(fma(as_type<float>(v13), as_type<float>(v37), as_type<float>(v35)));
    uint v39 = as_type<uint>(as_type<float>(v8) * as_type<float>(v30));
    uint v40 = as_type<uint>(fma(as_type<float>(v39), as_type<float>(v9), as_type<float>(v3)));
    uint v41 = as_type<uint>(as_type<float>(v40) - as_type<float>(v39));
    uint v42 = as_type<uint>(fma(as_type<float>(v13), as_type<float>(v41), as_type<float>(v39)));
    uint v43 = as_type<uint>(as_type<float>(v34) * as_type<float>(v10));
    uint v44 = as_type<uint>((int)rint(as_type<float>(v43)));
    uint v45 = as_type<uint>(as_type<float>(v38) * as_type<float>(v10));
    uint v46 = as_type<uint>((int)rint(as_type<float>(v45)));
    uint v47 = as_type<uint>(as_type<float>(v42) * as_type<float>(v10));
    uint v48 = as_type<uint>((int)rint(as_type<float>(v47)));
    uint v49 = v7 & v48;
    uint v50 = v7 & v44;
    uint v51 = v7 & v46;
    uint v52 = v51 << 8u;
    uint v53 = v50 | v52;
    uint v54 = v49 << 16u;
    uint v55 = v53 | v54;
    uint v56 = v22 << 24u;
    uint v57 = v55 | v56;
    ((device uint*)p1)[i] = v57;
}
