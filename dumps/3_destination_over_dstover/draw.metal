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
    uint v28 = as_type<uint>(as_type<float>(v11) * as_type<float>(v27));
    uint v29 = as_type<uint>(as_type<float>(v9) - as_type<float>(v28));
    uint v30 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v29), as_type<float>(v28)));
    uint v31 = as_type<uint>(as_type<float>(v30) - as_type<float>(v28));
    uint v32 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v31), as_type<float>(v28)));
    uint v33 = as_type<uint>(as_type<float>(v32) * as_type<float>(v12));
    uint v34 = as_type<uint>((int)rint(as_type<float>(v33)));
    uint v35 = v25 & v10;
    uint v36 = as_type<uint>((float)(int)v35);
    uint v37 = v25 >> 8u;
    uint v38 = v10 & v37;
    uint v39 = as_type<uint>((float)(int)v38);
    uint v40 = v25 >> 16u;
    uint v41 = v10 & v40;
    uint v42 = as_type<uint>((float)(int)v41);
    uint v43 = as_type<uint>(as_type<float>(v11) * as_type<float>(v36));
    uint v44 = as_type<uint>(fma(as_type<float>(v1), as_type<float>(v29), as_type<float>(v43)));
    uint v45 = as_type<uint>(as_type<float>(v44) - as_type<float>(v43));
    uint v46 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v45), as_type<float>(v43)));
    uint v47 = as_type<uint>(as_type<float>(v11) * as_type<float>(v39));
    uint v48 = as_type<uint>(fma(as_type<float>(v2), as_type<float>(v29), as_type<float>(v47)));
    uint v49 = as_type<uint>(as_type<float>(v48) - as_type<float>(v47));
    uint v50 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v49), as_type<float>(v47)));
    uint v51 = as_type<uint>(as_type<float>(v11) * as_type<float>(v42));
    uint v52 = as_type<uint>(fma(as_type<float>(v3), as_type<float>(v29), as_type<float>(v51)));
    uint v53 = as_type<uint>(as_type<float>(v52) - as_type<float>(v51));
    uint v54 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v53), as_type<float>(v51)));
    uint v55 = as_type<uint>(as_type<float>(v46) * as_type<float>(v12));
    uint v56 = as_type<uint>((int)rint(as_type<float>(v55)));
    uint v57 = as_type<uint>(as_type<float>(v50) * as_type<float>(v12));
    uint v58 = as_type<uint>((int)rint(as_type<float>(v57)));
    uint v59 = as_type<uint>(as_type<float>(v54) * as_type<float>(v12));
    uint v60 = as_type<uint>((int)rint(as_type<float>(v59)));
    uint v61 = v10 & v60;
    uint v62 = v10 & v56;
    uint v63 = v10 & v58;
    uint v64 = v63 << 8u;
    uint v65 = v62 | v64;
    uint v66 = v61 << 16u;
    uint v67 = v65 | v66;
    uint v68 = v34 << 24u;
    uint v69 = v67 | v68;
    ((device uint*)p1)[i] = v69;
}
