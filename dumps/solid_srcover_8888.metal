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
    constant uint *buf_szs [[buffer(3)]],
    uint i [[thread_position_in_grid]]
) {
    if (i >= n) return;
    uint v0 = 0u;
    uint v1 = ((device const uint*)p1)[2];
    uint v2 = ((device const uint*)p1)[3];
    uint v3 = ((device const uint*)p1)[4];
    uint v4 = ((device const uint*)p1)[5];
    uint v5 = 255u;
    uint v6 = 8u;
    uint v7 = 16u;
    uint v8 = 24u;
    uint v9 = 998277249u;
    uint v10 = 1065353216u;
    uint v11 = as_type<uint>(as_type<float>(v10) - as_type<float>(v4));
    uint v12 = 1132396544u;
    uint v13 = 1056964608u;
    uint v14 = ((device uint*)p0)[i];
    uint v15 = v14 & v5;
    uint v16 = as_type<uint>((float)(int)v15);
    uint v17 = as_type<uint>(as_type<float>(v9) * as_type<float>(v16));
    uint v18 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v11), as_type<float>(v1)));
    uint v19 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v12), as_type<float>(v13)));
    uint v20 = (uint)(int)as_type<float>(v19);
    uint v21 = v5 & v20;
    uint v22 = v14 >> v6;
    uint v23 = v14 >> 8u;
    uint v24 = v22;
    uint v25 = v5 & v24;
    uint v26 = as_type<uint>((float)(int)v25);
    uint v27 = as_type<uint>(as_type<float>(v9) * as_type<float>(v26));
    uint v28 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v11), as_type<float>(v2)));
    uint v29 = as_type<uint>(fma(as_type<float>(v28), as_type<float>(v12), as_type<float>(v13)));
    uint v30 = (uint)(int)as_type<float>(v29);
    uint v31 = v5 & v30;
    uint v32 = v31 << v6;
    uint v33 = v31 << 8u;
    uint v34 = v32;
    uint v35 = v21 | v34;
    uint v36 = v14 >> v7;
    uint v37 = v14 >> 16u;
    uint v38 = v36;
    uint v39 = v5 & v38;
    uint v40 = as_type<uint>((float)(int)v39);
    uint v41 = as_type<uint>(as_type<float>(v9) * as_type<float>(v40));
    uint v42 = as_type<uint>(fma(as_type<float>(v41), as_type<float>(v11), as_type<float>(v3)));
    uint v43 = as_type<uint>(fma(as_type<float>(v42), as_type<float>(v12), as_type<float>(v13)));
    uint v44 = (uint)(int)as_type<float>(v43);
    uint v45 = v5 & v44;
    uint v46 = v45 << v7;
    uint v47 = v45 << 16u;
    uint v48 = v46;
    uint v49 = v35 | v48;
    uint v50 = v14 >> v8;
    uint v51 = v14 >> 24u;
    uint v52 = v50;
    uint v53 = as_type<uint>((float)(int)v52);
    uint v54 = as_type<uint>(as_type<float>(v9) * as_type<float>(v53));
    uint v55 = as_type<uint>(fma(as_type<float>(v54), as_type<float>(v11), as_type<float>(v4)));
    uint v56 = as_type<uint>(fma(as_type<float>(v55), as_type<float>(v12), as_type<float>(v13)));
    uint v57 = (uint)(int)as_type<float>(v56);
    uint v58 = v57 << v8;
    uint v59 = v57 << 24u;
    uint v60 = v58;
    uint v61 = v49 | v60;
    ((device uint*)p0)[i] = v61;
}
