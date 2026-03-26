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
    uint v10 = pos.x;
    uint v11 = as_type<uint>((float)(int)v10);
    uint v12 = as_type<float>(v11) <  as_type<float>(v7) ? 0xffffffffu : 0u;
    uint v13 = as_type<float>(v5) <= as_type<float>(v11) ? 0xffffffffu : 0u;
    uint v14 = v13 & v12;
    uint v15 = pos.y;
    uint v16 = as_type<uint>((float)(int)v15);
    uint v17 = as_type<float>(v16) <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v18 = as_type<float>(v6) <= as_type<float>(v16) ? 0xffffffffu : 0u;
    uint v19 = v18 & v17;
    uint v20 = v14 & v19;
    uint v21 = (v20 & v9) | (~v20 & v0);
    uint v22 = ((device uint*)p1)[i];
    uint v23 = v22 >> 24u;
    uint v24 = as_type<uint>((float)(int)v23);
    uint v25 = as_type<uint>(as_type<float>(v24) * as_type<float>(998277249u));
    uint v26 = as_type<uint>(as_type<float>(v9) - as_type<float>(v25));
    uint v27 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v26), as_type<float>(v25)));
    uint v28 = as_type<uint>(as_type<float>(v27) - as_type<float>(v25));
    uint v29 = as_type<uint>(fma(as_type<float>(v21), as_type<float>(v28), as_type<float>(v25)));
    uint v30 = as_type<uint>(as_type<float>(v29) * as_type<float>(1132396544u));
    uint v31 = as_type<uint>((int)rint(as_type<float>(v30)));
    uint v32 = v22 >> 8u;
    uint v33 = v32 & 255u;
    uint v34 = as_type<uint>((float)(int)v33);
    uint v35 = as_type<uint>(as_type<float>(v34) * as_type<float>(998277249u));
    uint v36 = as_type<uint>(fma(as_type<float>(v2), as_type<float>(v26), as_type<float>(v35)));
    uint v37 = as_type<uint>(as_type<float>(v36) - as_type<float>(v35));
    uint v38 = as_type<uint>(fma(as_type<float>(v21), as_type<float>(v37), as_type<float>(v35)));
    uint v39 = as_type<uint>(as_type<float>(v38) * as_type<float>(1132396544u));
    uint v40 = as_type<uint>((int)rint(as_type<float>(v39)));
    uint v41 = v40 & 255u;
    uint v42 = v22 >> 16u;
    uint v43 = v42 & 255u;
    uint v44 = as_type<uint>((float)(int)v43);
    uint v45 = as_type<uint>(as_type<float>(v44) * as_type<float>(998277249u));
    uint v46 = as_type<uint>(fma(as_type<float>(v3), as_type<float>(v26), as_type<float>(v45)));
    uint v47 = as_type<uint>(as_type<float>(v46) - as_type<float>(v45));
    uint v48 = as_type<uint>(fma(as_type<float>(v21), as_type<float>(v47), as_type<float>(v45)));
    uint v49 = as_type<uint>(as_type<float>(v48) * as_type<float>(1132396544u));
    uint v50 = as_type<uint>((int)rint(as_type<float>(v49)));
    uint v51 = v50 & 255u;
    uint v52 = v22 & 255u;
    uint v53 = as_type<uint>((float)(int)v52);
    uint v54 = as_type<uint>(as_type<float>(v53) * as_type<float>(998277249u));
    uint v55 = as_type<uint>(fma(as_type<float>(v1), as_type<float>(v26), as_type<float>(v54)));
    uint v56 = as_type<uint>(as_type<float>(v55) - as_type<float>(v54));
    uint v57 = as_type<uint>(fma(as_type<float>(v21), as_type<float>(v56), as_type<float>(v54)));
    uint v58 = as_type<uint>(as_type<float>(v57) * as_type<float>(1132396544u));
    uint v59 = as_type<uint>((int)rint(as_type<float>(v58)));
    uint v60 = v59 & 255u;
    uint v61 = v60 | (v41 << 8u);
    uint v62 = v61 | (v51 << 16u);
    uint v63 = v62 | (v31 << 24u);
    ((device uint*)p1)[i] = v63;
}
