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
    constant uint &w [[buffer(5)]],
    constant uint &stride [[buffer(6)]],
    constant uint &x0 [[buffer(7)]],
    constant uint &y0 [[buffer(8)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    device uchar *p2 [[buffer(3)]],
    constant uint *buf_szs [[buffer(4)]],
    uint2 pos [[thread_position_in_grid]]
) {
    uint i = (y0 + pos.y) * stride + x0 + pos.x;
    if (i >= n) return;
    uint v0 = 0u;
    uint v1 = ((device const uint*)p0)[0];
    uint v2 = ((device const uint*)p0)[1];
    uint v3 = ((device const uint*)p0)[2];
    uint v4 = ((device const uint*)p0)[3];
    uint v6 = 1065353216u;
    uint v7 = 255u;
    uint v8 = 998277249u;
    uint v9 = as_type<uint>(as_type<float>(v6) - as_type<float>(v4));
    uint v10 = 1132396544u;
    uint v11 = ((device uint*)p2)[i];
    uint v12 = as_type<uint>(fabs(as_type<float>(v11)));
    uint v13 = as_type<uint>(min(as_type<float>(v12), as_type<float>(1065353216u)));
    uint v14 = ((device uint*)p1)[i];
    uint v15 = v14 >> 24u;
    uint v16 = as_type<uint>((float)(int)v15);
    uint v17 = as_type<uint>(as_type<float>(v16) * as_type<float>(998277249u));
    uint v18 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v9), as_type<float>(v4)));
    uint v19 = as_type<uint>(as_type<float>(v18) - as_type<float>(v17));
    uint v20 = as_type<uint>(fma(as_type<float>(v13), as_type<float>(v19), as_type<float>(v17)));
    uint v21 = as_type<uint>(max(as_type<float>(v20), as_type<float>(0u)));
    uint v22 = as_type<uint>(min(as_type<float>(v21), as_type<float>(1065353216u)));
    uint v23 = as_type<uint>(as_type<float>(v22) * as_type<float>(1132396544u));
    uint v24 = v14 >> 8u;
    uint v25 = v24 & 255u;
    uint v26 = as_type<uint>((float)(int)v25);
    uint v27 = as_type<uint>(as_type<float>(v26) * as_type<float>(998277249u));
    uint v28 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v9), as_type<float>(v2)));
    uint v29 = as_type<uint>(as_type<float>(v28) - as_type<float>(v27));
    uint v30 = as_type<uint>(fma(as_type<float>(v13), as_type<float>(v29), as_type<float>(v27)));
    uint v31 = as_type<uint>(max(as_type<float>(v30), as_type<float>(0u)));
    uint v32 = as_type<uint>(min(as_type<float>(v31), as_type<float>(1065353216u)));
    uint v33 = as_type<uint>(as_type<float>(v32) * as_type<float>(1132396544u));
    uint v34 = v14 >> 16u;
    uint v35 = v34 & 255u;
    uint v36 = as_type<uint>((float)(int)v35);
    uint v37 = as_type<uint>(as_type<float>(v36) * as_type<float>(998277249u));
    uint v38 = as_type<uint>(fma(as_type<float>(v37), as_type<float>(v9), as_type<float>(v3)));
    uint v39 = as_type<uint>(as_type<float>(v38) - as_type<float>(v37));
    uint v40 = as_type<uint>(fma(as_type<float>(v13), as_type<float>(v39), as_type<float>(v37)));
    uint v41 = as_type<uint>(max(as_type<float>(v40), as_type<float>(0u)));
    uint v42 = as_type<uint>(min(as_type<float>(v41), as_type<float>(1065353216u)));
    uint v43 = as_type<uint>(as_type<float>(v42) * as_type<float>(1132396544u));
    uint v44 = v14 & 255u;
    uint v45 = as_type<uint>((float)(int)v44);
    uint v46 = as_type<uint>(as_type<float>(v45) * as_type<float>(998277249u));
    uint v47 = as_type<uint>(fma(as_type<float>(v46), as_type<float>(v9), as_type<float>(v1)));
    uint v48 = as_type<uint>(as_type<float>(v47) - as_type<float>(v46));
    uint v49 = as_type<uint>(fma(as_type<float>(v13), as_type<float>(v48), as_type<float>(v46)));
    uint v50 = as_type<uint>(max(as_type<float>(v49), as_type<float>(0u)));
    uint v51 = as_type<uint>(min(as_type<float>(v50), as_type<float>(1065353216u)));
    uint v52 = as_type<uint>(as_type<float>(v51) * as_type<float>(1132396544u));
    uint v53 = as_type<uint>((int)rint(as_type<float>(v52)));
    uint v54 = as_type<uint>((int)rint(as_type<float>(v33)));
    uint v55 = v53 & 255u;
    uint v56 = v54 & 255u;
    uint v57 = v55 | (v56 << 8u);
    uint v58 = as_type<uint>((int)rint(as_type<float>(v43)));
    uint v59 = v58 & 255u;
    uint v60 = v57 | (v59 << 16u);
    uint v61 = as_type<uint>((int)rint(as_type<float>(v23)));
    uint v62 = v60 | (v61 << 24u);
    ((device uint*)p1)[i] = v62;
}
