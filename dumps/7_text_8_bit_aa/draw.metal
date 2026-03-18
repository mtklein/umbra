#include <metal_stdlib>
using namespace metal;

static inline int clamp_ix(int ix, uint bytes, int elem) {
    int hi = (int)(bytes / (uint)elem) - 1;
    if (hi < 0) hi = 0;
    return clamp(ix, 0, hi);
}

kernel void umbra_entry(
    constant uint &n [[buffer(0)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    device uchar *p2 [[buffer(3)]],
    constant uint *buf_szs [[buffer(4)]],
    uint i [[thread_position_in_grid]]
) {
    if (i >= n) return;
    uint v0 = 0u;
    uint v1 = ((device const uint*)p1)[2];
    uint v2 = ((device const uint*)p1)[3];
    uint v3 = ((device const uint*)p1)[4];
    uint v4 = ((device const uint*)p1)[5];
    uint v6 = 998277249u;
    uint v7 = 255u;
    uint v8 = 1065353216u;
    uint v9 = as_type<uint>(as_type<float>(v8) - as_type<float>(v4));
    uint v10 = 1132396544u;
    uint v11 = 1056964608u;
    uint v12 = (uint)((device ushort*)p2)[i];
    uint v13 = (uint)(int)(short)(ushort)v12;
    uint v14 = as_type<uint>((float)(int)v13);
    uint v15 = as_type<uint>(as_type<float>(v6) * as_type<float>(v14));
    uint v16 = ((device uint*)p0)[i];
    uint v17 = v16 >> 24u;
    uint v18 = as_type<uint>((float)(int)v17);
    uint v19 = as_type<uint>(as_type<float>(v6) * as_type<float>(v18));
    uint v20 = as_type<uint>(fma(as_type<float>(v19), as_type<float>(v9), as_type<float>(v4)));
    uint v21 = as_type<uint>(as_type<float>(v20) - as_type<float>(v19));
    uint v22 = as_type<uint>(fma(as_type<float>(v15), as_type<float>(v21), as_type<float>(v19)));
    uint v23 = as_type<uint>(as_type<float>(v22) * as_type<float>(v10));
    uint v24 = as_type<uint>(as_type<float>(v11) + as_type<float>(v23));
    uint v25 = (uint)(int)as_type<float>(v24);
    uint v26 = v16 & v7;
    uint v27 = as_type<uint>((float)(int)v26);
    uint v28 = v16 >> 8u;
    uint v29 = v7 & v28;
    uint v30 = as_type<uint>((float)(int)v29);
    uint v31 = v16 >> 16u;
    uint v32 = v7 & v31;
    uint v33 = as_type<uint>((float)(int)v32);
    uint v34 = as_type<uint>(as_type<float>(v6) * as_type<float>(v27));
    uint v35 = as_type<uint>(fma(as_type<float>(v34), as_type<float>(v9), as_type<float>(v1)));
    uint v36 = as_type<uint>(as_type<float>(v35) - as_type<float>(v34));
    uint v37 = as_type<uint>(fma(as_type<float>(v15), as_type<float>(v36), as_type<float>(v34)));
    uint v38 = as_type<uint>(as_type<float>(v6) * as_type<float>(v30));
    uint v39 = as_type<uint>(fma(as_type<float>(v38), as_type<float>(v9), as_type<float>(v2)));
    uint v40 = as_type<uint>(as_type<float>(v39) - as_type<float>(v38));
    uint v41 = as_type<uint>(fma(as_type<float>(v15), as_type<float>(v40), as_type<float>(v38)));
    uint v42 = as_type<uint>(as_type<float>(v6) * as_type<float>(v33));
    uint v43 = as_type<uint>(fma(as_type<float>(v42), as_type<float>(v9), as_type<float>(v3)));
    uint v44 = as_type<uint>(as_type<float>(v43) - as_type<float>(v42));
    uint v45 = as_type<uint>(fma(as_type<float>(v15), as_type<float>(v44), as_type<float>(v42)));
    uint v46 = as_type<uint>(as_type<float>(v37) * as_type<float>(v10));
    uint v47 = as_type<uint>(as_type<float>(v11) + as_type<float>(v46));
    uint v48 = (uint)(int)as_type<float>(v47);
    uint v49 = as_type<uint>(as_type<float>(v41) * as_type<float>(v10));
    uint v50 = as_type<uint>(as_type<float>(v11) + as_type<float>(v49));
    uint v51 = (uint)(int)as_type<float>(v50);
    uint v52 = as_type<uint>(as_type<float>(v45) * as_type<float>(v10));
    uint v53 = as_type<uint>(as_type<float>(v11) + as_type<float>(v52));
    uint v54 = (uint)(int)as_type<float>(v53);
    uint v55 = v7 & v54;
    uint v56 = v7 & v48;
    uint v57 = v7 & v51;
    uint v58 = v57 << 8u;
    uint v59 = v56 | v58;
    uint v60 = v55 << 16u;
    uint v61 = v59 | v60;
    uint v62 = v25 << 24u;
    uint v63 = v61 | v62;
    ((device uint*)p0)[i] = v63;
}
