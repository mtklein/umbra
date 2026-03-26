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
    uint v5 = 1065353216u;
    uint v6 = ((device const uint*)p0)[3];
    uint v7 = 1073741824u;
    uint v8 = as_type<uint>(as_type<float>(v6) - as_type<float>(v5));
    uint v9 = as_type<uint>(as_type<float>(v6) - as_type<float>(v7));
    uint v10 = 2u;
    uint v11 = 4u;
    uint v12 = 1u;
    uint v13 = 3u;
    uint v14 = 1132396544u;
    uint v15 = 255u;
    uint v16 = pos.x;
    uint v17 = as_type<uint>((float)(int)v16);
    uint v18 = pos.y;
    uint v19 = as_type<uint>((float)(int)v18);
    uint v20 = as_type<uint>(as_type<float>(v19) * as_type<float>(v3));
    uint v21 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v2), as_type<float>(v20)));
    uint v22 = as_type<uint>(as_type<float>(v4) + as_type<float>(v21));
    uint v23 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v22)));
    uint v24 = as_type<uint>(min(as_type<float>(v23), as_type<float>(v5)));
    uint v25 = as_type<uint>(as_type<float>(v24) * as_type<float>(v8));
    uint v26 = as_type<uint>(floor(as_type<float>(v25)));
    uint v27 = as_type<uint>(min(as_type<float>(v9), as_type<float>(v26)));
    uint v28 = (uint)(int)as_type<float>(v27);
    uint v29 = v28 << 2u;
    uint v30 = v29 + v11;
    uint v31 = as_type<uint>(as_type<float>(v25) - as_type<float>(v27));
    uint v32 = v30 + v12;
    uint v33 = v10 + v30;
    uint v34 = v30 + v13;
    uint v35 = ((device uint*)p2)[safe_ix((int)v34,buf_szs[2],4)] & oob_mask((int)v34,buf_szs[2],4);
    uint v36 = v29 + v12;
    uint v37 = ((device uint*)p2)[safe_ix((int)v36,buf_szs[2],4)] & oob_mask((int)v36,buf_szs[2],4);
    uint v38 = v10 + v29;
    uint v39 = ((device uint*)p2)[safe_ix((int)v38,buf_szs[2],4)] & oob_mask((int)v38,buf_szs[2],4);
    uint v40 = v29 + v13;
    uint v41 = ((device uint*)p2)[safe_ix((int)v40,buf_szs[2],4)] & oob_mask((int)v40,buf_szs[2],4);
    uint v42 = as_type<uint>(as_type<float>(v35) - as_type<float>(v41));
    uint v43 = as_type<uint>(fma(as_type<float>(v31), as_type<float>(v42), as_type<float>(v41)));
    uint v44 = as_type<uint>(as_type<float>(v43) * as_type<float>(v14));
    uint v45 = as_type<uint>((int)rint(as_type<float>(v44)));
    uint v46 = ((device uint*)p2)[safe_ix((int)v32,buf_szs[2],4)] & oob_mask((int)v32,buf_szs[2],4);
    uint v47 = as_type<uint>(as_type<float>(v46) - as_type<float>(v37));
    uint v48 = as_type<uint>(fma(as_type<float>(v31), as_type<float>(v47), as_type<float>(v37)));
    uint v49 = ((device uint*)p2)[safe_ix((int)v33,buf_szs[2],4)] & oob_mask((int)v33,buf_szs[2],4);
    uint v50 = as_type<uint>(as_type<float>(v49) - as_type<float>(v39));
    uint v51 = as_type<uint>(fma(as_type<float>(v31), as_type<float>(v50), as_type<float>(v39)));
    uint v52 = ((device uint*)p2)[safe_ix((int)v30,buf_szs[2],4)] & oob_mask((int)v30,buf_szs[2],4);
    uint v53 = ((device uint*)p2)[safe_ix((int)v29,buf_szs[2],4)] & oob_mask((int)v29,buf_szs[2],4);
    uint v54 = as_type<uint>(as_type<float>(v52) - as_type<float>(v53));
    uint v55 = as_type<uint>(fma(as_type<float>(v31), as_type<float>(v54), as_type<float>(v53)));
    uint v56 = as_type<uint>(as_type<float>(v55) * as_type<float>(v14));
    uint v57 = as_type<uint>((int)rint(as_type<float>(v56)));
    uint v58 = as_type<uint>(as_type<float>(v48) * as_type<float>(v14));
    uint v59 = as_type<uint>((int)rint(as_type<float>(v58)));
    uint v60 = as_type<uint>(as_type<float>(v51) * as_type<float>(v14));
    uint v61 = as_type<uint>((int)rint(as_type<float>(v60)));
    uint v62 = v61 & v15;
    uint v63 = v57 & v15;
    uint v64 = v59 & v15;
    uint v65 = v64 << 8u;
    uint v66 = v63 | v65;
    uint v67 = v62 << 16u;
    uint v68 = v66 | v67;
    uint v69 = v45 << 24u;
    uint v70 = v68 | v69;
    ((device uint*)p1)[i] = v70;
}
