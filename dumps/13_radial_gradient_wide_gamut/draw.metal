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
    uint v10 = as_type<uint>(as_type<float>(v9) - as_type<float>(v2));
    uint v11 = pos.y;
    uint v12 = as_type<uint>((float)(int)v11);
    uint v13 = as_type<uint>(as_type<float>(v12) - as_type<float>(v3));
    uint v14 = as_type<uint>(as_type<float>(v13) * as_type<float>(v13));
    uint v15 = as_type<uint>(fma(as_type<float>(v10), as_type<float>(v10), as_type<float>(v14)));
    uint v16 = as_type<uint>(sqrt(as_type<float>(v15)));
    uint v17 = as_type<uint>(as_type<float>(v4) * as_type<float>(v16));
    uint v18 = as_type<uint>(max(as_type<float>(v17), as_type<float>(0u)));
    uint v19 = as_type<uint>(min(as_type<float>(v18), as_type<float>(1065353216u)));
    uint v20 = as_type<uint>(as_type<float>(v19) * as_type<float>(v6));
    uint v21 = as_type<uint>(floor(as_type<float>(v20)));
    uint v22 = as_type<uint>(min(as_type<float>(v7), as_type<float>(v21)));
    uint v23 = (uint)(int)as_type<float>(v22);
    uint v24 = v23 << 2u;
    uint v25 = v24 + 3u;
    uint v26 = as_type<uint>(as_type<float>(v20) - as_type<float>(v22));
    uint v27 = ((device uint*)p2)[safe_ix((int)v25,buf_szs[2],4)] & oob_mask((int)v25,buf_szs[2],4);
    uint v28 = v24 + 1u;
    uint v29 = ((device uint*)p2)[safe_ix((int)v28,buf_szs[2],4)] & oob_mask((int)v28,buf_szs[2],4);
    uint v30 = v24 + 2u;
    uint v31 = ((device uint*)p2)[safe_ix((int)v30,buf_szs[2],4)] & oob_mask((int)v30,buf_szs[2],4);
    uint v32 = v24 + 4u;
    uint v33 = v32 + 3u;
    uint v34 = ((device uint*)p2)[safe_ix((int)v33,buf_szs[2],4)] & oob_mask((int)v33,buf_szs[2],4);
    uint v35 = as_type<uint>(as_type<float>(v34) - as_type<float>(v27));
    uint v36 = as_type<uint>(fma(as_type<float>(v26), as_type<float>(v35), as_type<float>(v27)));
    uint v37 = as_type<uint>(as_type<float>(v36) * as_type<float>(1132396544u));
    uint v38 = v32 + 1u;
    uint v39 = ((device uint*)p2)[safe_ix((int)v38,buf_szs[2],4)] & oob_mask((int)v38,buf_szs[2],4);
    uint v40 = as_type<uint>(as_type<float>(v39) - as_type<float>(v29));
    uint v41 = as_type<uint>(fma(as_type<float>(v26), as_type<float>(v40), as_type<float>(v29)));
    uint v42 = as_type<uint>(as_type<float>(v41) * as_type<float>(1132396544u));
    uint v43 = as_type<uint>((int)rint(as_type<float>(v42)));
    uint v44 = v43 & 255u;
    uint v45 = as_type<uint>((int)rint(as_type<float>(v37)));
    uint v46 = v32 + 2u;
    uint v47 = ((device uint*)p2)[safe_ix((int)v46,buf_szs[2],4)] & oob_mask((int)v46,buf_szs[2],4);
    uint v48 = as_type<uint>(as_type<float>(v47) - as_type<float>(v31));
    uint v49 = as_type<uint>(fma(as_type<float>(v26), as_type<float>(v48), as_type<float>(v31)));
    uint v50 = as_type<uint>(as_type<float>(v49) * as_type<float>(1132396544u));
    uint v51 = as_type<uint>((int)rint(as_type<float>(v50)));
    uint v52 = v51 & 255u;
    uint v53 = ((device uint*)p2)[safe_ix((int)v32,buf_szs[2],4)] & oob_mask((int)v32,buf_szs[2],4);
    uint v54 = ((device uint*)p2)[safe_ix((int)v24,buf_szs[2],4)] & oob_mask((int)v24,buf_szs[2],4);
    uint v55 = as_type<uint>(as_type<float>(v53) - as_type<float>(v54));
    uint v56 = as_type<uint>(fma(as_type<float>(v26), as_type<float>(v55), as_type<float>(v54)));
    uint v57 = as_type<uint>(as_type<float>(v56) * as_type<float>(1132396544u));
    uint v58 = as_type<uint>((int)rint(as_type<float>(v57)));
    uint v59 = v58 & 255u;
    uint v60 = v59 | (v44 << 8u);
    uint v61 = v60 | (v52 << 16u);
    uint v62 = v61 | (v45 << 24u);
    ((device uint*)p1)[i] = v62;
}
