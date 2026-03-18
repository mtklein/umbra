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
    uint v1 = ((device const uint*)p1)[0];
    uint v2 = 1u;
    uint v3 = ((device const uint*)p1)[1];
    uint v4 = as_type<uint>((float)(int)v3);
    uint v6 = 2u;
    uint v7 = ((device const uint*)p1)[2];
    uint v8 = 3u;
    uint v9 = ((device const uint*)p1)[3];
    uint v10 = 4u;
    uint v11 = ((device const uint*)p1)[4];
    uint v12 = as_type<uint>(as_type<float>(v4) - as_type<float>(v9));
    uint v13 = as_type<uint>(as_type<float>(v12) * as_type<float>(v12));
    uint v14 = 1065353216u;
    uint v15 = ((device const uint*)p1)[5];
    uint v16 = 1073741824u;
    uint v17 = as_type<uint>(as_type<float>(v15) - as_type<float>(v14));
    uint v18 = as_type<uint>(as_type<float>(v15) - as_type<float>(v16));
    uint v19 = buf_szs[2] >> 2u;
    uint v20 = 1132396544u;
    uint v21 = 1056964608u;
    uint v22 = 255u;
    uint v23 = buf_szs[0] >> 2u;
    uint v24 = i;
    uint v25 = v24 + v1;
    uint v26 = as_type<uint>((float)(int)v25);
    uint v27 = as_type<uint>(as_type<float>(v26) - as_type<float>(v7));
    uint v28 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v27), as_type<float>(v13)));
    uint v29 = as_type<uint>(sqrt(as_type<float>(v28)));
    uint v30 = as_type<uint>(as_type<float>(v11) * as_type<float>(v29));
    uint v31 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v30)));
    uint v32 = as_type<uint>(min(as_type<float>(v31), as_type<float>(v14)));
    uint v33 = as_type<uint>(as_type<float>(v32) * as_type<float>(v17));
    uint v34 = (uint)(int)as_type<float>(v33);
    uint v35 = as_type<uint>((float)(int)v34);
    uint v36 = as_type<uint>(min(as_type<float>(v18), as_type<float>(v35)));
    uint v37 = as_type<uint>(as_type<float>(v33) - as_type<float>(v36));
    uint v38 = (uint)(int)as_type<float>(v36);
    uint v39 = v38 << 2u;
    uint v40 = 0xffffffffu;
    uint v41 = v39 <  v19 ? 0xffffffffu : 0u;
    uint v42 = v40 & v41;
    uint v43 = v42 ? ((device uint*)p2)[(int)v39] : 0u;
    uint v44 = v10 + v39;
    uint v45 = v44 <  v19 ? 0xffffffffu : 0u;
    uint v46 = v40 & v45;
    uint v47 = v46 ? ((device uint*)p2)[(int)v44] : 0u;
    uint v48 = as_type<uint>(as_type<float>(v47) - as_type<float>(v43));
    uint v49 = as_type<uint>(fma(as_type<float>(v37), as_type<float>(v48), as_type<float>(v43)));
    uint v50 = as_type<uint>(fma(as_type<float>(v49), as_type<float>(v20), as_type<float>(v21)));
    uint v51 = (uint)(int)as_type<float>(v50);
    uint v52 = v51 & v22;
    uint v53 = v2 + v39;
    uint v54 = v53 <  v19 ? 0xffffffffu : 0u;
    uint v55 = v40 & v54;
    uint v56 = v55 ? ((device uint*)p2)[(int)v53] : 0u;
    uint v57 = v2 + v44;
    uint v58 = v57 <  v19 ? 0xffffffffu : 0u;
    uint v59 = v40 & v58;
    uint v60 = v59 ? ((device uint*)p2)[(int)v57] : 0u;
    uint v61 = as_type<uint>(as_type<float>(v60) - as_type<float>(v56));
    uint v62 = as_type<uint>(fma(as_type<float>(v37), as_type<float>(v61), as_type<float>(v56)));
    uint v63 = as_type<uint>(fma(as_type<float>(v62), as_type<float>(v20), as_type<float>(v21)));
    uint v64 = (uint)(int)as_type<float>(v63);
    uint v65 = v64 & v22;
    uint v66 = v65 << 8u;
    uint v67 = v52 | v66;
    uint v68 = v6 + v39;
    uint v69 = v68 <  v19 ? 0xffffffffu : 0u;
    uint v70 = v40 & v69;
    uint v71 = v70 ? ((device uint*)p2)[(int)v68] : 0u;
    uint v72 = v6 + v44;
    uint v73 = v72 <  v19 ? 0xffffffffu : 0u;
    uint v74 = v40 & v73;
    uint v75 = v74 ? ((device uint*)p2)[(int)v72] : 0u;
    uint v76 = as_type<uint>(as_type<float>(v75) - as_type<float>(v71));
    uint v77 = as_type<uint>(fma(as_type<float>(v37), as_type<float>(v76), as_type<float>(v71)));
    uint v78 = as_type<uint>(fma(as_type<float>(v77), as_type<float>(v20), as_type<float>(v21)));
    uint v79 = (uint)(int)as_type<float>(v78);
    uint v80 = v79 & v22;
    uint v81 = v80 << 16u;
    uint v82 = v67 | v81;
    uint v83 = v8 + v39;
    uint v84 = v83 <  v19 ? 0xffffffffu : 0u;
    uint v85 = v40 & v84;
    uint v86 = v85 ? ((device uint*)p2)[(int)v83] : 0u;
    uint v87 = v8 + v44;
    uint v88 = v87 <  v19 ? 0xffffffffu : 0u;
    uint v89 = v40 & v88;
    uint v90 = v89 ? ((device uint*)p2)[(int)v87] : 0u;
    uint v91 = as_type<uint>(as_type<float>(v90) - as_type<float>(v86));
    uint v92 = as_type<uint>(fma(as_type<float>(v37), as_type<float>(v91), as_type<float>(v86)));
    uint v93 = as_type<uint>(fma(as_type<float>(v92), as_type<float>(v20), as_type<float>(v21)));
    uint v94 = (uint)(int)as_type<float>(v93);
    uint v95 = v94 << 24u;
    uint v96 = v82 | v95;
    uint v97 = v24 <  v23 ? 0xffffffffu : 0u;
    uint v98 = v40 & v97;
    ((device uint*)p0)[i] = v96;
}
