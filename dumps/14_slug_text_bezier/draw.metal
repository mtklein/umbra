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
    uint v7 = as_type<uint>(as_type<float>(v6) - as_type<float>(v4));
    uint v8 = ((device uint*)p2)[i];
    uint v9 = as_type<uint>(fabs(as_type<float>(v8)));
    uint v10 = as_type<uint>(min(as_type<float>(v9), as_type<float>(1065353216u)));
    uint v11 = ((device uint*)p1)[i];
    uint v12 = v11 >> 24u;
    uint v13 = as_type<uint>((float)(int)v12);
    uint v14 = as_type<uint>(as_type<float>(v13) * as_type<float>(998277249u));
    uint v15 = as_type<uint>(fma(as_type<float>(v14), as_type<float>(v7), as_type<float>(v4)));
    uint v16 = as_type<uint>(as_type<float>(v15) - as_type<float>(v14));
    uint v17 = as_type<uint>(fma(as_type<float>(v10), as_type<float>(v16), as_type<float>(v14)));
    uint v18 = v11 >> 8u;
    uint v19 = v18 & 255u;
    uint v20 = as_type<uint>((float)(int)v19);
    uint v21 = as_type<uint>(as_type<float>(v20) * as_type<float>(998277249u));
    uint v22 = as_type<uint>(fma(as_type<float>(v21), as_type<float>(v7), as_type<float>(v2)));
    uint v23 = as_type<uint>(as_type<float>(v22) - as_type<float>(v21));
    uint v24 = as_type<uint>(fma(as_type<float>(v10), as_type<float>(v23), as_type<float>(v21)));
    uint v25 = as_type<uint>(as_type<float>(v24) * as_type<float>(1132396544u));
    uint v26 = as_type<uint>(as_type<float>(v17) * as_type<float>(1132396544u));
    uint v27 = as_type<uint>((int)rint(as_type<float>(v25)));
    uint v28 = v11 >> 16u;
    uint v29 = v28 & 255u;
    uint v30 = as_type<uint>((float)(int)v29);
    uint v31 = as_type<uint>(as_type<float>(v30) * as_type<float>(998277249u));
    uint v32 = as_type<uint>(fma(as_type<float>(v31), as_type<float>(v7), as_type<float>(v3)));
    uint v33 = as_type<uint>(as_type<float>(v32) - as_type<float>(v31));
    uint v34 = as_type<uint>(fma(as_type<float>(v10), as_type<float>(v33), as_type<float>(v31)));
    uint v35 = as_type<uint>(as_type<float>(v34) * as_type<float>(1132396544u));
    uint v36 = v27 & 255u;
    uint v37 = as_type<uint>((int)rint(as_type<float>(v35)));
    uint v38 = v11 & 255u;
    uint v39 = as_type<uint>((float)(int)v38);
    uint v40 = as_type<uint>(as_type<float>(v39) * as_type<float>(998277249u));
    uint v41 = as_type<uint>(fma(as_type<float>(v40), as_type<float>(v7), as_type<float>(v1)));
    uint v42 = as_type<uint>(as_type<float>(v41) - as_type<float>(v40));
    uint v43 = as_type<uint>(fma(as_type<float>(v10), as_type<float>(v42), as_type<float>(v40)));
    uint v44 = as_type<uint>(as_type<float>(v43) * as_type<float>(1132396544u));
    uint v45 = as_type<uint>((int)rint(as_type<float>(v44)));
    uint v46 = v45 & 255u;
    uint v47 = v46 | (v36 << 8u);
    uint v48 = v37 & 255u;
    uint v49 = v47 | (v48 << 16u);
    uint v50 = as_type<uint>((int)rint(as_type<float>(v26)));
    uint v51 = v49 | (v50 << 24u);
    ((device uint*)p1)[i] = v51;
}
