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
    uint v26 = as_type<uint>(as_type<float>(v4) - as_type<float>(v25));
    uint v27 = as_type<uint>(fma(as_type<float>(v21), as_type<float>(v26), as_type<float>(v25)));
    uint v28 = as_type<uint>(as_type<float>(v27) * as_type<float>(1132396544u));
    uint v29 = as_type<uint>((int)rint(as_type<float>(v28)));
    uint v30 = v22 >> 8u;
    uint v31 = v30 & 255u;
    uint v32 = as_type<uint>((float)(int)v31);
    uint v33 = as_type<uint>(as_type<float>(v32) * as_type<float>(998277249u));
    uint v34 = as_type<uint>(as_type<float>(v2) - as_type<float>(v33));
    uint v35 = as_type<uint>(fma(as_type<float>(v21), as_type<float>(v34), as_type<float>(v33)));
    uint v36 = as_type<uint>(as_type<float>(v35) * as_type<float>(1132396544u));
    uint v37 = as_type<uint>((int)rint(as_type<float>(v36)));
    uint v38 = v37 & 255u;
    uint v39 = v22 >> 16u;
    uint v40 = v39 & 255u;
    uint v41 = as_type<uint>((float)(int)v40);
    uint v42 = as_type<uint>(as_type<float>(v41) * as_type<float>(998277249u));
    uint v43 = as_type<uint>(as_type<float>(v3) - as_type<float>(v42));
    uint v44 = as_type<uint>(fma(as_type<float>(v21), as_type<float>(v43), as_type<float>(v42)));
    uint v45 = as_type<uint>(as_type<float>(v44) * as_type<float>(1132396544u));
    uint v46 = as_type<uint>((int)rint(as_type<float>(v45)));
    uint v47 = v46 & 255u;
    uint v48 = v22 & 255u;
    uint v49 = as_type<uint>((float)(int)v48);
    uint v50 = as_type<uint>(as_type<float>(v49) * as_type<float>(998277249u));
    uint v51 = as_type<uint>(as_type<float>(v1) - as_type<float>(v50));
    uint v52 = as_type<uint>(fma(as_type<float>(v21), as_type<float>(v51), as_type<float>(v50)));
    uint v53 = as_type<uint>(as_type<float>(v52) * as_type<float>(1132396544u));
    uint v54 = as_type<uint>((int)rint(as_type<float>(v53)));
    uint v55 = v54 & 255u;
    uint v56 = v55 | (v38 << 8u);
    uint v57 = v56 | (v47 << 16u);
    uint v58 = v57 | (v29 << 24u);
    ((device uint*)p1)[i] = v58;
}
