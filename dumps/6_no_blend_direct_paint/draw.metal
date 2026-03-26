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
    constant uint &w [[buffer(4)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    constant uint *buf_szs [[buffer(3)]],
    uint2 pos [[thread_position_in_grid]]
) {
    uint i = pos.y * w + pos.x;
    if (i >= n) return;
    uint v0 = 0u;
    uint v1 = ((device const uint*)p0)[0];
    uint v2 = ((device const uint*)p0)[1];
    uint v3 = ((device const uint*)p0)[2];
    uint v4 = ((device const uint*)p0)[3];
    uint v5 = ((device const uint*)p0)[4];
    uint v6 = ((device const uint*)p0)[5];
    uint v7 = ((device const uint*)p0)[6];
    uint v8 = ((device const uint*)p0)[7];
    uint v9 = 1065353216u;
    uint v10 = 255u;
    uint v11 = 998277249u;
    uint v12 = 1132396544u;
    uint v13 = pos.x;
    uint v14 = as_type<uint>((float)(int)v13);
    uint v15 = as_type<float>(v14) <  as_type<float>(v7) ? 0xffffffffu : 0u;
    uint v16 = as_type<float>(v5) <= as_type<float>(v14) ? 0xffffffffu : 0u;
    uint v17 = v16 & v15;
    uint v18 = pos.y;
    uint v19 = as_type<uint>((float)(int)v18);
    uint v20 = as_type<float>(v19) <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v21 = as_type<float>(v6) <= as_type<float>(v19) ? 0xffffffffu : 0u;
    uint v22 = v21 & v20;
    uint v23 = v17 & v22;
    uint v24 = (v23 & v9) | (~v23 & v0);
    uint v25 = ((device uint*)p1)[i];
    uint v26 = v25 >> 24u;
    uint v27 = as_type<uint>((float)(int)v26);
    uint v28 = as_type<uint>(as_type<float>(v27) * as_type<float>(998277249u));
    uint v29 = as_type<uint>(as_type<float>(v4) - as_type<float>(v28));
    uint v30 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v29), as_type<float>(v28)));
    uint v31 = as_type<uint>(as_type<float>(v30) * as_type<float>(1132396544u));
    uint v32 = as_type<uint>((int)rint(as_type<float>(v31)));
    uint v33 = v25 >> 8u;
    uint v34 = v33 & 255u;
    uint v35 = as_type<uint>((float)(int)v34);
    uint v36 = as_type<uint>(as_type<float>(v35) * as_type<float>(998277249u));
    uint v37 = as_type<uint>(as_type<float>(v2) - as_type<float>(v36));
    uint v38 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v37), as_type<float>(v36)));
    uint v39 = as_type<uint>(as_type<float>(v38) * as_type<float>(1132396544u));
    uint v40 = as_type<uint>((int)rint(as_type<float>(v39)));
    uint v41 = v40 & 255u;
    uint v42 = v25 >> 16u;
    uint v43 = v42 & 255u;
    uint v44 = as_type<uint>((float)(int)v43);
    uint v45 = as_type<uint>(as_type<float>(v44) * as_type<float>(998277249u));
    uint v46 = as_type<uint>(as_type<float>(v3) - as_type<float>(v45));
    uint v47 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v46), as_type<float>(v45)));
    uint v48 = as_type<uint>(as_type<float>(v47) * as_type<float>(1132396544u));
    uint v49 = as_type<uint>((int)rint(as_type<float>(v48)));
    uint v50 = v49 & 255u;
    uint v51 = v25 & 255u;
    uint v52 = as_type<uint>((float)(int)v51);
    uint v53 = as_type<uint>(as_type<float>(v52) * as_type<float>(998277249u));
    uint v54 = as_type<uint>(as_type<float>(v1) - as_type<float>(v53));
    uint v55 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v54), as_type<float>(v53)));
    uint v56 = as_type<uint>(as_type<float>(v55) * as_type<float>(1132396544u));
    uint v57 = as_type<uint>((int)rint(as_type<float>(v56)));
    uint v58 = v57 & 255u;
    uint v59 = v58 | (v41 << 8u);
    uint v60 = v59 | (v50 << 16u);
    uint v61 = v60 | (v32 << 24u);
    ((device uint*)p1)[i] = v61;
}
