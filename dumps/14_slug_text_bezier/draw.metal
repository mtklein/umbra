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
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    device uchar *p2 [[buffer(3)]],
    constant uint *buf_szs [[buffer(4)]],
    uint2 pos [[thread_position_in_grid]]
) {
    uint i = pos.y * w + pos.x;
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
    uint v21 = as_type<uint>(as_type<float>(v20) * as_type<float>(1132396544u));
    uint v22 = v14 >> 8u;
    uint v23 = v22 & 255u;
    uint v24 = as_type<uint>((float)(int)v23);
    uint v25 = as_type<uint>(as_type<float>(v24) * as_type<float>(998277249u));
    uint v26 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v9), as_type<float>(v2)));
    uint v27 = as_type<uint>(as_type<float>(v26) - as_type<float>(v25));
    uint v28 = as_type<uint>(fma(as_type<float>(v13), as_type<float>(v27), as_type<float>(v25)));
    uint v29 = as_type<uint>(as_type<float>(v28) * as_type<float>(1132396544u));
    uint v30 = as_type<uint>((int)rint(as_type<float>(v29)));
    uint v31 = v30 & 255u;
    uint v32 = v14 >> 16u;
    uint v33 = v32 & 255u;
    uint v34 = as_type<uint>((float)(int)v33);
    uint v35 = as_type<uint>(as_type<float>(v34) * as_type<float>(998277249u));
    uint v36 = as_type<uint>(fma(as_type<float>(v35), as_type<float>(v9), as_type<float>(v3)));
    uint v37 = as_type<uint>(as_type<float>(v36) - as_type<float>(v35));
    uint v38 = as_type<uint>(fma(as_type<float>(v13), as_type<float>(v37), as_type<float>(v35)));
    uint v39 = as_type<uint>(as_type<float>(v38) * as_type<float>(1132396544u));
    uint v40 = as_type<uint>((int)rint(as_type<float>(v39)));
    uint v41 = v40 & 255u;
    uint v42 = as_type<uint>((int)rint(as_type<float>(v21)));
    uint v43 = v14 & 255u;
    uint v44 = as_type<uint>((float)(int)v43);
    uint v45 = as_type<uint>(as_type<float>(v44) * as_type<float>(998277249u));
    uint v46 = as_type<uint>(fma(as_type<float>(v45), as_type<float>(v9), as_type<float>(v1)));
    uint v47 = as_type<uint>(as_type<float>(v46) - as_type<float>(v45));
    uint v48 = as_type<uint>(fma(as_type<float>(v13), as_type<float>(v47), as_type<float>(v45)));
    uint v49 = as_type<uint>(as_type<float>(v48) * as_type<float>(1132396544u));
    uint v50 = as_type<uint>((int)rint(as_type<float>(v49)));
    uint v51 = v50 & 255u;
    uint v52 = v51 | (v31 << 8u);
    uint v53 = v52 | (v41 << 16u);
    uint v54 = v53 | (v42 << 24u);
    ((device uint*)p1)[i] = v54;
}
