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
    uint v19 = 1132396544u;
    uint v20 = 1056964608u;
    uint v21 = 255u;
    uint v22 = i;
    uint v23 = v22 + v1;
    uint v24 = as_type<uint>((float)(int)v23);
    uint v25 = as_type<uint>(as_type<float>(v24) - as_type<float>(v7));
    uint v26 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v25), as_type<float>(v13)));
    uint v27 = as_type<uint>(sqrt(as_type<float>(v26)));
    uint v28 = as_type<uint>(as_type<float>(v11) * as_type<float>(v27));
    uint v29 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v28)));
    uint v30 = as_type<uint>(min(as_type<float>(v29), as_type<float>(v14)));
    uint v31 = as_type<uint>(as_type<float>(v30) * as_type<float>(v17));
    uint v32 = (uint)(int)as_type<float>(v31);
    uint v33 = as_type<uint>((float)(int)v32);
    uint v34 = as_type<uint>(min(as_type<float>(v18), as_type<float>(v33)));
    uint v35 = (uint)(int)as_type<float>(v34);
    uint v36 = v35 << 2u;
    uint v37 = v10 + v36;
    uint v38 = as_type<uint>(as_type<float>(v31) - as_type<float>(v34));
    uint v39 = v2 + v37;
    uint v40 = v6 + v37;
    uint v41 = v8 + v37;
    uint v42 = ((device uint*)p2)[clamp_ix((int)v41,buf_szs[2],4)];
    uint v43 = v2 + v36;
    uint v44 = ((device uint*)p2)[clamp_ix((int)v43,buf_szs[2],4)];
    uint v45 = v6 + v36;
    uint v46 = ((device uint*)p2)[clamp_ix((int)v45,buf_szs[2],4)];
    uint v47 = v8 + v36;
    uint v48 = ((device uint*)p2)[clamp_ix((int)v47,buf_szs[2],4)];
    uint v49 = as_type<uint>(as_type<float>(v42) - as_type<float>(v48));
    uint v50 = as_type<uint>(fma(as_type<float>(v38), as_type<float>(v49), as_type<float>(v48)));
    uint v51 = as_type<uint>(as_type<float>(v50) * as_type<float>(v19));
    uint v52 = as_type<uint>(as_type<float>(v20) + as_type<float>(v51));
    uint v53 = (uint)(int)as_type<float>(v52);
    uint v54 = ((device uint*)p2)[clamp_ix((int)v39,buf_szs[2],4)];
    uint v55 = as_type<uint>(as_type<float>(v54) - as_type<float>(v44));
    uint v56 = as_type<uint>(fma(as_type<float>(v38), as_type<float>(v55), as_type<float>(v44)));
    uint v57 = ((device uint*)p2)[clamp_ix((int)v40,buf_szs[2],4)];
    uint v58 = as_type<uint>(as_type<float>(v57) - as_type<float>(v46));
    uint v59 = as_type<uint>(fma(as_type<float>(v38), as_type<float>(v58), as_type<float>(v46)));
    uint v60 = ((device uint*)p2)[clamp_ix((int)v37,buf_szs[2],4)];
    uint v61 = ((device uint*)p2)[clamp_ix((int)v36,buf_szs[2],4)];
    uint v62 = as_type<uint>(as_type<float>(v60) - as_type<float>(v61));
    uint v63 = as_type<uint>(fma(as_type<float>(v38), as_type<float>(v62), as_type<float>(v61)));
    uint v64 = as_type<uint>(as_type<float>(v63) * as_type<float>(v19));
    uint v65 = as_type<uint>(as_type<float>(v20) + as_type<float>(v64));
    uint v66 = (uint)(int)as_type<float>(v65);
    uint v67 = as_type<uint>(as_type<float>(v56) * as_type<float>(v19));
    uint v68 = as_type<uint>(as_type<float>(v20) + as_type<float>(v67));
    uint v69 = (uint)(int)as_type<float>(v68);
    uint v70 = as_type<uint>(as_type<float>(v59) * as_type<float>(v19));
    uint v71 = as_type<uint>(as_type<float>(v20) + as_type<float>(v70));
    uint v72 = (uint)(int)as_type<float>(v71);
    uint v73 = v72 & v21;
    uint v74 = v66 & v21;
    uint v75 = v69 & v21;
    uint v76 = v75 << 8u;
    uint v77 = v74 | v76;
    uint v78 = v73 << 16u;
    uint v79 = v77 | v78;
    uint v80 = v53 << 24u;
    uint v81 = v79 | v80;
    ((device uint*)p0)[i] = v81;
}
