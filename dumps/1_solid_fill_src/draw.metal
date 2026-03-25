#include <metal_stdlib>
using namespace metal;

static inline int clamp_ix(int ix, uint bytes, int elem) {
    int hi = (int)(bytes / (uint)elem) - 1;
    if (hi < 0) hi = 0;
    return clamp(ix, 0, hi);
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
    uint v1 = ((device const uint*)p0)[2];
    uint v2 = ((device const uint*)p0)[3];
    uint v3 = ((device const uint*)p0)[4];
    uint v4 = ((device const uint*)p0)[5];
    uint v5 = ((device const uint*)p0)[6];
    uint v6 = ((device const uint*)p0)[7];
    uint v7 = ((device const uint*)p0)[8];
    uint v8 = ((device const uint*)p0)[9];
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
    uint v28 = as_type<uint>(as_type<float>(v11) * as_type<float>(v27));
    uint v29 = as_type<uint>(as_type<float>(v4) - as_type<float>(v28));
    uint v30 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v29), as_type<float>(v28)));
    uint v31 = as_type<uint>(as_type<float>(v30) * as_type<float>(v12));
    uint v32 = as_type<uint>((int)rint(as_type<float>(v31)));
    uint v33 = v25 & v10;
    uint v34 = as_type<uint>((float)(int)v33);
    uint v35 = v25 >> 8u;
    uint v36 = v10 & v35;
    uint v37 = as_type<uint>((float)(int)v36);
    uint v38 = v25 >> 16u;
    uint v39 = v10 & v38;
    uint v40 = as_type<uint>((float)(int)v39);
    uint v41 = as_type<uint>(as_type<float>(v11) * as_type<float>(v34));
    uint v42 = as_type<uint>(as_type<float>(v1) - as_type<float>(v41));
    uint v43 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v42), as_type<float>(v41)));
    uint v44 = as_type<uint>(as_type<float>(v11) * as_type<float>(v37));
    uint v45 = as_type<uint>(as_type<float>(v2) - as_type<float>(v44));
    uint v46 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v45), as_type<float>(v44)));
    uint v47 = as_type<uint>(as_type<float>(v11) * as_type<float>(v40));
    uint v48 = as_type<uint>(as_type<float>(v3) - as_type<float>(v47));
    uint v49 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v48), as_type<float>(v47)));
    uint v50 = as_type<uint>(as_type<float>(v43) * as_type<float>(v12));
    uint v51 = as_type<uint>((int)rint(as_type<float>(v50)));
    uint v52 = as_type<uint>(as_type<float>(v46) * as_type<float>(v12));
    uint v53 = as_type<uint>((int)rint(as_type<float>(v52)));
    uint v54 = as_type<uint>(as_type<float>(v49) * as_type<float>(v12));
    uint v55 = as_type<uint>((int)rint(as_type<float>(v54)));
    uint v56 = v10 & v55;
    uint v57 = v10 & v51;
    uint v58 = v10 & v53;
    uint v59 = v58 << 8u;
    uint v60 = v57 | v59;
    uint v61 = v56 << 16u;
    uint v62 = v60 | v61;
    uint v63 = v32 << 24u;
    uint v64 = v62 | v63;
    ((device uint*)p1)[i] = v64;
}
