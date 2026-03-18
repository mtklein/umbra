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
    uint v19 = as_type<uint>(as_type<float>(v15) - as_type<float>(v7));
    uint v20 = 1132396544u;
    uint v21 = 1056964608u;
    uint v22 = 0xffffffffu;
    uint v23 = i;
    uint v24 = v23 <  v16 ? 0xffffffffu : 0u;
    uint v25 = v22 & v24;
    uint v26 = ((device uint*)p0)[i];
    uint v27 = v26 & v17;
    uint v28 = as_type<uint>((float)(int)v27);
    uint v29 = v23 + v1;
    uint v30 = as_type<uint>((float)(int)v29);
    uint v31 = as_type<float>(v8) <= as_type<float>(v30) ? 0xffffffffu : 0u;
    uint v32 = as_type<float>(v30) <  as_type<float>(v10) ? 0xffffffffu : 0u;
    uint v33 = v31 & v32;
    uint v34 = v33 & v14;
    uint v35 = (v34 & v15) | (~v34 & v0);
    uint v36 = as_type<uint>(as_type<float>(v18) * as_type<float>(v28));
    uint v37 = as_type<uint>(fma(as_type<float>(v36), as_type<float>(v19), as_type<float>(v4)));
    uint v38 = as_type<uint>(as_type<float>(v37) - as_type<float>(v18) * as_type<float>(v28));
    uint v39 = as_type<uint>(as_type<float>(v35) * as_type<float>(v38));
    uint v40 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v28), as_type<float>(v39)));
    uint v41 = as_type<uint>(fma(as_type<float>(v40), as_type<float>(v20), as_type<float>(v21)));
    uint v42 = (uint)(int)as_type<float>(v41);
    uint v43 = v17 & v42;
    uint v44 = v26 >> 8u;
    uint v45 = v17 & v44;
    uint v46 = as_type<uint>((float)(int)v45);
    uint v47 = as_type<uint>(as_type<float>(v18) * as_type<float>(v46));
    uint v48 = as_type<uint>(fma(as_type<float>(v47), as_type<float>(v19), as_type<float>(v5)));
    uint v49 = as_type<uint>(as_type<float>(v48) - as_type<float>(v18) * as_type<float>(v46));
    uint v50 = as_type<uint>(as_type<float>(v35) * as_type<float>(v49));
    uint v51 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v46), as_type<float>(v50)));
    uint v52 = as_type<uint>(fma(as_type<float>(v51), as_type<float>(v20), as_type<float>(v21)));
    uint v53 = (uint)(int)as_type<float>(v52);
    uint v54 = v17 & v53;
    uint v55 = v54 << 8u;
    uint v56 = v43 | v55;
    uint v57 = v26 >> 16u;
    uint v58 = v17 & v57;
    uint v59 = as_type<uint>((float)(int)v58);
    uint v60 = as_type<uint>(as_type<float>(v18) * as_type<float>(v59));
    uint v61 = as_type<uint>(fma(as_type<float>(v60), as_type<float>(v19), as_type<float>(v6)));
    uint v62 = as_type<uint>(as_type<float>(v61) - as_type<float>(v18) * as_type<float>(v59));
    uint v63 = as_type<uint>(as_type<float>(v35) * as_type<float>(v62));
    uint v64 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v59), as_type<float>(v63)));
    uint v65 = as_type<uint>(fma(as_type<float>(v64), as_type<float>(v20), as_type<float>(v21)));
    uint v66 = (uint)(int)as_type<float>(v65);
    uint v67 = v17 & v66;
    uint v68 = v67 << 16u;
    uint v69 = v56 | v68;
    uint v70 = v26 >> 24u;
    uint v71 = as_type<uint>((float)(int)v70);
    uint v72 = as_type<uint>(as_type<float>(v18) * as_type<float>(v71));
    uint v73 = as_type<uint>(fma(as_type<float>(v72), as_type<float>(v19), as_type<float>(v7)));
    uint v74 = as_type<uint>(as_type<float>(v73) - as_type<float>(v18) * as_type<float>(v71));
    uint v75 = as_type<uint>(as_type<float>(v35) * as_type<float>(v74));
    uint v76 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v71), as_type<float>(v75)));
    uint v77 = as_type<uint>(fma(as_type<float>(v76), as_type<float>(v20), as_type<float>(v21)));
    uint v78 = (uint)(int)as_type<float>(v77);
    uint v79 = v78 << 24u;
    uint v80 = v69 | v79;
    ((device uint*)p0)[i] = v80;
}
