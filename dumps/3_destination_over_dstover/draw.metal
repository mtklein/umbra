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
    uint v29 = as_type<uint>(as_type<float>(v9) - as_type<float>(v28));
    uint v30 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v29), as_type<float>(v28)));
    uint v31 = as_type<uint>(as_type<float>(v30) - as_type<float>(v28));
    uint v32 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v31), as_type<float>(v28)));
    uint v33 = as_type<uint>(as_type<float>(v32) * as_type<float>(1132396544u));
    uint v34 = as_type<uint>((int)rint(as_type<float>(v33)));
    uint v35 = v25 >> 8u;
    uint v36 = v35 & 255u;
    uint v37 = as_type<uint>((float)(int)v36);
    uint v38 = as_type<uint>(as_type<float>(v37) * as_type<float>(998277249u));
    uint v39 = as_type<uint>(fma(as_type<float>(v2), as_type<float>(v29), as_type<float>(v38)));
    uint v40 = as_type<uint>(as_type<float>(v39) - as_type<float>(v38));
    uint v41 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v40), as_type<float>(v38)));
    uint v42 = as_type<uint>(as_type<float>(v41) * as_type<float>(1132396544u));
    uint v43 = as_type<uint>((int)rint(as_type<float>(v42)));
    uint v44 = v43 & 255u;
    uint v45 = v25 >> 16u;
    uint v46 = v45 & 255u;
    uint v47 = as_type<uint>((float)(int)v46);
    uint v48 = as_type<uint>(as_type<float>(v47) * as_type<float>(998277249u));
    uint v49 = as_type<uint>(fma(as_type<float>(v3), as_type<float>(v29), as_type<float>(v48)));
    uint v50 = as_type<uint>(as_type<float>(v49) - as_type<float>(v48));
    uint v51 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v50), as_type<float>(v48)));
    uint v52 = as_type<uint>(as_type<float>(v51) * as_type<float>(1132396544u));
    uint v53 = as_type<uint>((int)rint(as_type<float>(v52)));
    uint v54 = v53 & 255u;
    uint v55 = v25 & 255u;
    uint v56 = as_type<uint>((float)(int)v55);
    uint v57 = as_type<uint>(as_type<float>(v56) * as_type<float>(998277249u));
    uint v58 = as_type<uint>(fma(as_type<float>(v1), as_type<float>(v29), as_type<float>(v57)));
    uint v59 = as_type<uint>(as_type<float>(v58) - as_type<float>(v57));
    uint v60 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v59), as_type<float>(v57)));
    uint v61 = as_type<uint>(as_type<float>(v60) * as_type<float>(1132396544u));
    uint v62 = as_type<uint>((int)rint(as_type<float>(v61)));
    uint v63 = v62 & 255u;
    uint v64 = v63 | (v44 << 8u);
    uint v65 = v64 | (v54 << 16u);
    uint v66 = v65 | (v34 << 24u);
    ((device uint*)p1)[i] = v66;
}
