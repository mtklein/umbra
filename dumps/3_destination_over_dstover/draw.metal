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
    uint v16 = buf_szs[0] >> 2u;
    uint v17 = 255u;
    uint v18 = 998277249u;
    uint v19 = 1132396544u;
    uint v20 = 1056964608u;
    uint v21 = 0xffffffffu;
    uint v22 = i;
    uint v23 = v22 <  v16 ? 0xffffffffu : 0u;
    uint v24 = v21 & v23;
    uint v25 = ((device uint*)p0)[i];
    uint v26 = v25 & v17;
    uint v27 = as_type<uint>((float)(int)v26);
    uint v28 = v22 + v1;
    uint v29 = as_type<uint>((float)(int)v28);
    uint v30 = as_type<float>(v8) <= as_type<float>(v29) ? 0xffffffffu : 0u;
    uint v31 = as_type<float>(v29) <  as_type<float>(v10) ? 0xffffffffu : 0u;
    uint v32 = v30 & v31;
    uint v33 = v32 & v14;
    uint v34 = (v33 & v15) | (~v33 & v0);
    uint v35 = v25 >> 24u;
    uint v36 = as_type<uint>((float)(int)v35);
    uint v37 = as_type<uint>(as_type<float>(v15) - as_type<float>(v18) * as_type<float>(v36));
    uint v38 = as_type<uint>(as_type<float>(v4) * as_type<float>(v37));
    uint v39 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v27), as_type<float>(v38)));
    uint v40 = as_type<uint>(as_type<float>(v39) - as_type<float>(v18) * as_type<float>(v27));
    uint v41 = as_type<uint>(as_type<float>(v34) * as_type<float>(v40));
    uint v42 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v27), as_type<float>(v41)));
    uint v43 = as_type<uint>(fma(as_type<float>(v42), as_type<float>(v19), as_type<float>(v20)));
    uint v44 = (uint)(int)as_type<float>(v43);
    uint v45 = v17 & v44;
    uint v46 = v25 >> 8u;
    uint v47 = v17 & v46;
    uint v48 = as_type<uint>((float)(int)v47);
    uint v49 = as_type<uint>(as_type<float>(v5) * as_type<float>(v37));
    uint v50 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v48), as_type<float>(v49)));
    uint v51 = as_type<uint>(as_type<float>(v50) - as_type<float>(v18) * as_type<float>(v48));
    uint v52 = as_type<uint>(as_type<float>(v34) * as_type<float>(v51));
    uint v53 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v48), as_type<float>(v52)));
    uint v54 = as_type<uint>(fma(as_type<float>(v53), as_type<float>(v19), as_type<float>(v20)));
    uint v55 = (uint)(int)as_type<float>(v54);
    uint v56 = v17 & v55;
    uint v57 = v56 << 8u;
    uint v58 = v45 | v57;
    uint v59 = v25 >> 16u;
    uint v60 = v17 & v59;
    uint v61 = as_type<uint>((float)(int)v60);
    uint v62 = as_type<uint>(as_type<float>(v6) * as_type<float>(v37));
    uint v63 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v61), as_type<float>(v62)));
    uint v64 = as_type<uint>(as_type<float>(v63) - as_type<float>(v18) * as_type<float>(v61));
    uint v65 = as_type<uint>(as_type<float>(v34) * as_type<float>(v64));
    uint v66 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v61), as_type<float>(v65)));
    uint v67 = as_type<uint>(fma(as_type<float>(v66), as_type<float>(v19), as_type<float>(v20)));
    uint v68 = (uint)(int)as_type<float>(v67);
    uint v69 = v17 & v68;
    uint v70 = v69 << 16u;
    uint v71 = v58 | v70;
    uint v72 = as_type<uint>(as_type<float>(v7) * as_type<float>(v37));
    uint v73 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v36), as_type<float>(v72)));
    uint v74 = as_type<uint>(as_type<float>(v73) - as_type<float>(v18) * as_type<float>(v36));
    uint v75 = as_type<uint>(as_type<float>(v34) * as_type<float>(v74));
    uint v76 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v36), as_type<float>(v75)));
    uint v77 = as_type<uint>(fma(as_type<float>(v76), as_type<float>(v19), as_type<float>(v20)));
    uint v78 = (uint)(int)as_type<float>(v77);
    uint v79 = v78 << 24u;
    uint v80 = v71 | v79;
    ((device uint*)p0)[i] = v80;
}
