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
    uint v17 = v16 & v7;
    uint v18 = as_type<uint>((float)(int)v17);
    uint v19 = as_type<uint>(as_type<float>(v6) * as_type<float>(v18));
    uint v20 = as_type<uint>(fma(as_type<float>(v19), as_type<float>(v9), as_type<float>(v1)));
    uint v21 = as_type<uint>(as_type<float>(v20) - as_type<float>(v19));
    uint v22 = as_type<uint>(fma(as_type<float>(v15), as_type<float>(v21), as_type<float>(v19)));
    uint v23 = as_type<uint>(as_type<float>(v22) * as_type<float>(v10));
    uint v24 = as_type<uint>(as_type<float>(v11) + as_type<float>(v23));
    uint v25 = (uint)(int)as_type<float>(v24);
    uint v26 = v7 & v25;
    uint v27 = v16 >> 8u;
    uint v28 = v7 & v27;
    uint v29 = as_type<uint>((float)(int)v28);
    uint v30 = as_type<uint>(as_type<float>(v6) * as_type<float>(v29));
    uint v31 = as_type<uint>(fma(as_type<float>(v30), as_type<float>(v9), as_type<float>(v2)));
    uint v32 = as_type<uint>(as_type<float>(v31) - as_type<float>(v30));
    uint v33 = as_type<uint>(fma(as_type<float>(v15), as_type<float>(v32), as_type<float>(v30)));
    uint v34 = as_type<uint>(as_type<float>(v33) * as_type<float>(v10));
    uint v35 = as_type<uint>(as_type<float>(v11) + as_type<float>(v34));
    uint v36 = (uint)(int)as_type<float>(v35);
    uint v37 = v7 & v36;
    uint v38 = v37 << 8u;
    uint v39 = v26 | v38;
    uint v40 = v16 >> 16u;
    uint v41 = v7 & v40;
    uint v42 = as_type<uint>((float)(int)v41);
    uint v43 = as_type<uint>(as_type<float>(v6) * as_type<float>(v42));
    uint v44 = as_type<uint>(fma(as_type<float>(v43), as_type<float>(v9), as_type<float>(v3)));
    uint v45 = as_type<uint>(as_type<float>(v44) - as_type<float>(v43));
    uint v46 = as_type<uint>(fma(as_type<float>(v15), as_type<float>(v45), as_type<float>(v43)));
    uint v47 = as_type<uint>(as_type<float>(v46) * as_type<float>(v10));
    uint v48 = as_type<uint>(as_type<float>(v11) + as_type<float>(v47));
    uint v49 = (uint)(int)as_type<float>(v48);
    uint v50 = v7 & v49;
    uint v51 = v50 << 16u;
    uint v52 = v39 | v51;
    uint v53 = v16 >> 24u;
    uint v54 = as_type<uint>((float)(int)v53);
    uint v55 = as_type<uint>(as_type<float>(v6) * as_type<float>(v54));
    uint v56 = as_type<uint>(fma(as_type<float>(v55), as_type<float>(v9), as_type<float>(v4)));
    uint v57 = as_type<uint>(as_type<float>(v56) - as_type<float>(v55));
    uint v58 = as_type<uint>(fma(as_type<float>(v15), as_type<float>(v57), as_type<float>(v55)));
    uint v59 = as_type<uint>(as_type<float>(v58) * as_type<float>(v10));
    uint v60 = as_type<uint>(as_type<float>(v11) + as_type<float>(v59));
    uint v61 = (uint)(int)as_type<float>(v60);
    uint v62 = v61 << 24u;
    uint v63 = v52 | v62;
    ((device uint*)p0)[i] = v63;
}
