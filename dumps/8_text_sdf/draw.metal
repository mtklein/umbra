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
    uint v14 = ((device uint*)p0)[i];
    uint v15 = v14 & v10;
    uint v16 = as_type<uint>((float)(int)v15);
    uint v17 = (uint)((device ushort*)p2)[i];
    uint v18 = (uint)(int)(short)(ushort)v17;
    uint v19 = as_type<uint>((float)(int)v18);
    uint v20 = as_type<uint>(as_type<float>(v6) * as_type<float>(v19));
    uint v21 = as_type<uint>(as_type<float>(v20) - as_type<float>(v7));
    uint v22 = as_type<uint>(as_type<float>(v8) * as_type<float>(v21));
    uint v23 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v22)));
    uint v24 = as_type<uint>(min(as_type<float>(v9), as_type<float>(v23)));
    uint v25 = as_type<uint>(as_type<float>(v6) * as_type<float>(v16));
    uint v26 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v11), as_type<float>(v1)));
    uint v27 = as_type<uint>(as_type<float>(v26) - as_type<float>(v6) * as_type<float>(v16));
    uint v28 = as_type<uint>(as_type<float>(v24) * as_type<float>(v27));
    uint v29 = as_type<uint>(fma(as_type<float>(v6), as_type<float>(v16), as_type<float>(v28)));
    uint v30 = as_type<uint>(fma(as_type<float>(v29), as_type<float>(v12), as_type<float>(v13)));
    uint v31 = (uint)(int)as_type<float>(v30);
    uint v32 = v10 & v31;
    uint v33 = v14 >> 8u;
    uint v34 = v10 & v33;
    uint v35 = as_type<uint>((float)(int)v34);
    uint v36 = as_type<uint>(as_type<float>(v6) * as_type<float>(v35));
    uint v37 = as_type<uint>(fma(as_type<float>(v36), as_type<float>(v11), as_type<float>(v2)));
    uint v38 = as_type<uint>(as_type<float>(v37) - as_type<float>(v6) * as_type<float>(v35));
    uint v39 = as_type<uint>(as_type<float>(v24) * as_type<float>(v38));
    uint v40 = as_type<uint>(fma(as_type<float>(v6), as_type<float>(v35), as_type<float>(v39)));
    uint v41 = as_type<uint>(fma(as_type<float>(v40), as_type<float>(v12), as_type<float>(v13)));
    uint v42 = (uint)(int)as_type<float>(v41);
    uint v43 = v10 & v42;
    uint v44 = v43 << 8u;
    uint v45 = v32 | v44;
    uint v46 = v14 >> 16u;
    uint v47 = v10 & v46;
    uint v48 = as_type<uint>((float)(int)v47);
    uint v49 = as_type<uint>(as_type<float>(v6) * as_type<float>(v48));
    uint v50 = as_type<uint>(fma(as_type<float>(v49), as_type<float>(v11), as_type<float>(v3)));
    uint v51 = as_type<uint>(as_type<float>(v50) - as_type<float>(v6) * as_type<float>(v48));
    uint v52 = as_type<uint>(as_type<float>(v24) * as_type<float>(v51));
    uint v53 = as_type<uint>(fma(as_type<float>(v6), as_type<float>(v48), as_type<float>(v52)));
    uint v54 = as_type<uint>(fma(as_type<float>(v53), as_type<float>(v12), as_type<float>(v13)));
    uint v55 = (uint)(int)as_type<float>(v54);
    uint v56 = v10 & v55;
    uint v57 = v56 << 16u;
    uint v58 = v45 | v57;
    uint v59 = v14 >> 24u;
    uint v60 = as_type<uint>((float)(int)v59);
    uint v61 = as_type<uint>(as_type<float>(v6) * as_type<float>(v60));
    uint v62 = as_type<uint>(fma(as_type<float>(v61), as_type<float>(v11), as_type<float>(v4)));
    uint v63 = as_type<uint>(as_type<float>(v62) - as_type<float>(v6) * as_type<float>(v60));
    uint v64 = as_type<uint>(as_type<float>(v24) * as_type<float>(v63));
    uint v65 = as_type<uint>(fma(as_type<float>(v6), as_type<float>(v60), as_type<float>(v64)));
    uint v66 = as_type<uint>(fma(as_type<float>(v65), as_type<float>(v12), as_type<float>(v13)));
    uint v67 = (uint)(int)as_type<float>(v66);
    uint v68 = v67 << 24u;
    uint v69 = v58 | v68;
    ((device uint*)p0)[i] = v69;
}
