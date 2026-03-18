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
    uint v1 = ((device const uint*)p1)[0];
    uint v2 = ((device const uint*)p1)[1];
    uint v3 = as_type<uint>((float)(int)v2);
    uint v4 = ((device const uint*)p1)[2];
    uint v5 = ((device const uint*)p1)[3];
    uint v6 = ((device const uint*)p1)[4];
    uint v7 = ((device const uint*)p1)[5];
    uint v8 = ((device const uint*)p1)[6];
    uint v9 = ((device const uint*)p1)[7];
    uint v10 = ((device const uint*)p1)[8];
    uint v11 = ((device const uint*)p1)[9];
    uint v12 = as_type<float>(v9) <= as_type<float>(v3) ? 0xffffffffu : 0u;
    uint v13 = as_type<float>(v3) <  as_type<float>(v11) ? 0xffffffffu : 0u;
    uint v14 = v12 & v13;
    uint v15 = 1065353216u;
    uint v16 = 255u;
    uint v17 = 998277249u;
    uint v18 = 1132396544u;
    uint v19 = 1056964608u;
    uint v20 = ((device uint*)p0)[i];
    uint v21 = v20 & v16;
    uint v22 = as_type<uint>((float)(int)v21);
    uint v23 = i;
    uint v24 = v23 + v1;
    uint v25 = as_type<uint>((float)(int)v24);
    uint v26 = as_type<float>(v8) <= as_type<float>(v25) ? 0xffffffffu : 0u;
    uint v27 = as_type<float>(v25) <  as_type<float>(v10) ? 0xffffffffu : 0u;
    uint v28 = v26 & v27;
    uint v29 = v28 & v14;
    uint v30 = (v29 & v15) | (~v29 & v0);
    uint v31 = as_type<uint>(as_type<float>(v4) - as_type<float>(v17) * as_type<float>(v22));
    uint v32 = as_type<uint>(as_type<float>(v30) * as_type<float>(v31));
    uint v33 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v22), as_type<float>(v32)));
    uint v34 = as_type<uint>(fma(as_type<float>(v33), as_type<float>(v18), as_type<float>(v19)));
    uint v35 = (uint)(int)as_type<float>(v34);
    uint v36 = v16 & v35;
    uint v37 = v20 >> 8u;
    uint v38 = v16 & v37;
    uint v39 = as_type<uint>((float)(int)v38);
    uint v40 = as_type<uint>(as_type<float>(v5) - as_type<float>(v17) * as_type<float>(v39));
    uint v41 = as_type<uint>(as_type<float>(v30) * as_type<float>(v40));
    uint v42 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v39), as_type<float>(v41)));
    uint v43 = as_type<uint>(fma(as_type<float>(v42), as_type<float>(v18), as_type<float>(v19)));
    uint v44 = (uint)(int)as_type<float>(v43);
    uint v45 = v16 & v44;
    uint v46 = v45 << 8u;
    uint v47 = v36 | v46;
    uint v48 = v20 >> 16u;
    uint v49 = v16 & v48;
    uint v50 = as_type<uint>((float)(int)v49);
    uint v51 = as_type<uint>(as_type<float>(v6) - as_type<float>(v17) * as_type<float>(v50));
    uint v52 = as_type<uint>(as_type<float>(v30) * as_type<float>(v51));
    uint v53 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v50), as_type<float>(v52)));
    uint v54 = as_type<uint>(fma(as_type<float>(v53), as_type<float>(v18), as_type<float>(v19)));
    uint v55 = (uint)(int)as_type<float>(v54);
    uint v56 = v16 & v55;
    uint v57 = v56 << 16u;
    uint v58 = v47 | v57;
    uint v59 = v20 >> 24u;
    uint v60 = as_type<uint>((float)(int)v59);
    uint v61 = as_type<uint>(as_type<float>(v7) - as_type<float>(v17) * as_type<float>(v60));
    uint v62 = as_type<uint>(as_type<float>(v30) * as_type<float>(v61));
    uint v63 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v60), as_type<float>(v62)));
    uint v64 = as_type<uint>(fma(as_type<float>(v63), as_type<float>(v18), as_type<float>(v19)));
    uint v65 = (uint)(int)as_type<float>(v64);
    uint v66 = v65 << 24u;
    uint v67 = v58 | v66;
    ((device uint*)p0)[i] = v67;
}
