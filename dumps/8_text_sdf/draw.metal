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
    uint v7 = 1054867456u;
    uint v8 = 1090519040u;
    uint v9 = 1065353216u;
    uint v10 = 255u;
    uint v11 = as_type<uint>(as_type<float>(v9) - as_type<float>(v4));
    uint v12 = 1132396544u;
    uint v13 = 1056964608u;
    uint v14 = (uint)((device ushort*)p2)[i];
    uint v15 = (uint)(int)(short)(ushort)v14;
    uint v16 = as_type<uint>((float)(int)v15);
    uint v17 = as_type<uint>(as_type<float>(v6) * as_type<float>(v16));
    uint v18 = as_type<uint>(as_type<float>(v17) - as_type<float>(v7));
    uint v19 = as_type<uint>(as_type<float>(v8) * as_type<float>(v18));
    uint v20 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v19)));
    uint v21 = as_type<uint>(min(as_type<float>(v9), as_type<float>(v20)));
    uint v22 = ((device uint*)p0)[i];
    uint v23 = v22 >> 24u;
    uint v24 = as_type<uint>((float)(int)v23);
    uint v25 = as_type<uint>(as_type<float>(v6) * as_type<float>(v24));
    uint v26 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v11), as_type<float>(v4)));
    uint v27 = as_type<uint>(as_type<float>(v26) - as_type<float>(v25));
    uint v28 = as_type<uint>(fma(as_type<float>(v21), as_type<float>(v27), as_type<float>(v25)));
    uint v29 = as_type<uint>(as_type<float>(v28) * as_type<float>(v12));
    uint v30 = as_type<uint>(as_type<float>(v13) + as_type<float>(v29));
    uint v31 = (uint)(int)as_type<float>(v30);
    uint v32 = v22 & v10;
    uint v33 = as_type<uint>((float)(int)v32);
    uint v34 = v22 >> 8u;
    uint v35 = v10 & v34;
    uint v36 = as_type<uint>((float)(int)v35);
    uint v37 = v22 >> 16u;
    uint v38 = v10 & v37;
    uint v39 = as_type<uint>((float)(int)v38);
    uint v40 = as_type<uint>(as_type<float>(v6) * as_type<float>(v33));
    uint v41 = as_type<uint>(fma(as_type<float>(v40), as_type<float>(v11), as_type<float>(v1)));
    uint v42 = as_type<uint>(as_type<float>(v41) - as_type<float>(v40));
    uint v43 = as_type<uint>(fma(as_type<float>(v21), as_type<float>(v42), as_type<float>(v40)));
    uint v44 = as_type<uint>(as_type<float>(v6) * as_type<float>(v36));
    uint v45 = as_type<uint>(fma(as_type<float>(v44), as_type<float>(v11), as_type<float>(v2)));
    uint v46 = as_type<uint>(as_type<float>(v45) - as_type<float>(v44));
    uint v47 = as_type<uint>(fma(as_type<float>(v21), as_type<float>(v46), as_type<float>(v44)));
    uint v48 = as_type<uint>(as_type<float>(v6) * as_type<float>(v39));
    uint v49 = as_type<uint>(fma(as_type<float>(v48), as_type<float>(v11), as_type<float>(v3)));
    uint v50 = as_type<uint>(as_type<float>(v49) - as_type<float>(v48));
    uint v51 = as_type<uint>(fma(as_type<float>(v21), as_type<float>(v50), as_type<float>(v48)));
    uint v52 = as_type<uint>(as_type<float>(v43) * as_type<float>(v12));
    uint v53 = as_type<uint>(as_type<float>(v13) + as_type<float>(v52));
    uint v54 = (uint)(int)as_type<float>(v53);
    uint v55 = as_type<uint>(as_type<float>(v47) * as_type<float>(v12));
    uint v56 = as_type<uint>(as_type<float>(v13) + as_type<float>(v55));
    uint v57 = (uint)(int)as_type<float>(v56);
    uint v58 = as_type<uint>(as_type<float>(v51) * as_type<float>(v12));
    uint v59 = as_type<uint>(as_type<float>(v13) + as_type<float>(v58));
    uint v60 = (uint)(int)as_type<float>(v59);
    uint v61 = v10 & v60;
    uint v62 = v10 & v54;
    uint v63 = v10 & v57;
    uint v64 = v63 << 8u;
    uint v65 = v62 | v64;
    uint v66 = v61 << 16u;
    uint v67 = v65 | v66;
    uint v68 = v31 << 24u;
    uint v69 = v67 | v68;
    ((device uint*)p0)[i] = v69;
}
