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
    uint v2 = ((device const uint*)p0)[0];
    uint v3 = ((device const uint*)p0)[1];
    uint v4 = ((device const uint*)p0)[2];
    uint v5 = ((device const uint*)p0)[3];
    uint v6 = as_type<uint>(as_type<float>(v5) - as_type<float>(1065353216u));
    uint v7 = as_type<uint>(as_type<float>(v5) - as_type<float>(1073741824u));
    uint v8 = pos.x;
    uint v9 = as_type<uint>((float)(int)v8);
    uint v10 = pos.y;
    uint v11 = as_type<uint>((float)(int)v10);
    uint v12 = as_type<uint>(as_type<float>(v11) * as_type<float>(v3));
    uint v13 = as_type<uint>(fma(as_type<float>(v9), as_type<float>(v2), as_type<float>(v12)));
    uint v14 = as_type<uint>(as_type<float>(v4) + as_type<float>(v13));
    uint v15 = as_type<uint>(max(as_type<float>(v14), as_type<float>(0u)));
    uint v16 = as_type<uint>(min(as_type<float>(v15), as_type<float>(1065353216u)));
    uint v17 = as_type<uint>(as_type<float>(v16) * as_type<float>(v6));
    uint v18 = as_type<uint>(floor(as_type<float>(v17)));
    uint v19 = as_type<uint>(min(as_type<float>(v7), as_type<float>(v18)));
    uint v20 = (uint)(int)as_type<float>(v19);
    uint v21 = v20 << 2u;
    uint v22 = v21 + 3u;
    uint v23 = as_type<uint>(as_type<float>(v17) - as_type<float>(v19));
    uint v24 = ((device uint*)p2)[safe_ix((int)v22,buf_szs[2],4)] & oob_mask((int)v22,buf_szs[2],4);
    uint v25 = v21 + 1u;
    uint v26 = ((device uint*)p2)[safe_ix((int)v25,buf_szs[2],4)] & oob_mask((int)v25,buf_szs[2],4);
    uint v27 = v21 + 2u;
    uint v28 = ((device uint*)p2)[safe_ix((int)v27,buf_szs[2],4)] & oob_mask((int)v27,buf_szs[2],4);
    uint v29 = v21 + 4u;
    uint v30 = v29 + 3u;
    uint v31 = ((device uint*)p2)[safe_ix((int)v30,buf_szs[2],4)] & oob_mask((int)v30,buf_szs[2],4);
    uint v32 = as_type<uint>(as_type<float>(v31) - as_type<float>(v24));
    uint v33 = as_type<uint>(fma(as_type<float>(v23), as_type<float>(v32), as_type<float>(v24)));
    uint v34 = as_type<uint>(as_type<float>(v33) * as_type<float>(1132396544u));
    uint v35 = v29 + 1u;
    uint v36 = ((device uint*)p2)[safe_ix((int)v35,buf_szs[2],4)] & oob_mask((int)v35,buf_szs[2],4);
    uint v37 = as_type<uint>(as_type<float>(v36) - as_type<float>(v26));
    uint v38 = as_type<uint>(fma(as_type<float>(v23), as_type<float>(v37), as_type<float>(v26)));
    uint v39 = as_type<uint>(as_type<float>(v38) * as_type<float>(1132396544u));
    uint v40 = as_type<uint>((int)rint(as_type<float>(v39)));
    uint v41 = v40 & 255u;
    uint v42 = v29 + 2u;
    uint v43 = ((device uint*)p2)[safe_ix((int)v42,buf_szs[2],4)] & oob_mask((int)v42,buf_szs[2],4);
    uint v44 = as_type<uint>(as_type<float>(v43) - as_type<float>(v28));
    uint v45 = as_type<uint>(fma(as_type<float>(v23), as_type<float>(v44), as_type<float>(v28)));
    uint v46 = as_type<uint>(as_type<float>(v45) * as_type<float>(1132396544u));
    uint v47 = as_type<uint>((int)rint(as_type<float>(v46)));
    uint v48 = v47 & 255u;
    uint v49 = as_type<uint>((int)rint(as_type<float>(v34)));
    uint v50 = ((device uint*)p2)[safe_ix((int)v29,buf_szs[2],4)] & oob_mask((int)v29,buf_szs[2],4);
    uint v51 = ((device uint*)p2)[safe_ix((int)v21,buf_szs[2],4)] & oob_mask((int)v21,buf_szs[2],4);
    uint v52 = as_type<uint>(as_type<float>(v50) - as_type<float>(v51));
    uint v53 = as_type<uint>(fma(as_type<float>(v23), as_type<float>(v52), as_type<float>(v51)));
    uint v54 = as_type<uint>(as_type<float>(v53) * as_type<float>(1132396544u));
    uint v55 = as_type<uint>((int)rint(as_type<float>(v54)));
    uint v56 = v55 & 255u;
    uint v57 = v56 | (v41 << 8u);
    uint v58 = v57 | (v48 << 16u);
    uint v59 = v58 | (v49 << 24u);
    ((device uint*)p1)[i] = v59;
}
