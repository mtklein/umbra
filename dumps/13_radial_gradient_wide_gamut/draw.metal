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
    uint v35 = as_type<uint>(as_type<float>(v31) - as_type<float>(v34));
    uint v36 = (uint)(int)as_type<float>(v34);
    uint v37 = v36 << 2u;
    uint v38 = ((device uint*)p2)[clamp_ix((int)v37,buf_szs[2],4)];
    uint v39 = v10 + v37;
    uint v40 = ((device uint*)p2)[clamp_ix((int)v39,buf_szs[2],4)];
    uint v41 = as_type<uint>(as_type<float>(v40) - as_type<float>(v38));
    uint v42 = as_type<uint>(fma(as_type<float>(v35), as_type<float>(v41), as_type<float>(v38)));
    uint v43 = as_type<uint>(fma(as_type<float>(v42), as_type<float>(v19), as_type<float>(v20)));
    uint v44 = (uint)(int)as_type<float>(v43);
    uint v45 = v44 & v21;
    uint v46 = v2 + v37;
    uint v47 = ((device uint*)p2)[clamp_ix((int)v46,buf_szs[2],4)];
    uint v48 = v2 + v39;
    uint v49 = ((device uint*)p2)[clamp_ix((int)v48,buf_szs[2],4)];
    uint v50 = as_type<uint>(as_type<float>(v49) - as_type<float>(v47));
    uint v51 = as_type<uint>(fma(as_type<float>(v35), as_type<float>(v50), as_type<float>(v47)));
    uint v52 = as_type<uint>(fma(as_type<float>(v51), as_type<float>(v19), as_type<float>(v20)));
    uint v53 = (uint)(int)as_type<float>(v52);
    uint v54 = v53 & v21;
    uint v55 = v54 << 8u;
    uint v56 = v45 | v55;
    uint v57 = v6 + v37;
    uint v58 = ((device uint*)p2)[clamp_ix((int)v57,buf_szs[2],4)];
    uint v59 = v6 + v39;
    uint v60 = ((device uint*)p2)[clamp_ix((int)v59,buf_szs[2],4)];
    uint v61 = as_type<uint>(as_type<float>(v60) - as_type<float>(v58));
    uint v62 = as_type<uint>(fma(as_type<float>(v35), as_type<float>(v61), as_type<float>(v58)));
    uint v63 = as_type<uint>(fma(as_type<float>(v62), as_type<float>(v19), as_type<float>(v20)));
    uint v64 = (uint)(int)as_type<float>(v63);
    uint v65 = v64 & v21;
    uint v66 = v65 << 16u;
    uint v67 = v56 | v66;
    uint v68 = v8 + v37;
    uint v69 = ((device uint*)p2)[clamp_ix((int)v68,buf_szs[2],4)];
    uint v70 = v8 + v39;
    uint v71 = ((device uint*)p2)[clamp_ix((int)v70,buf_szs[2],4)];
    uint v72 = as_type<uint>(as_type<float>(v71) - as_type<float>(v69));
    uint v73 = as_type<uint>(fma(as_type<float>(v35), as_type<float>(v72), as_type<float>(v69)));
    uint v74 = as_type<uint>(fma(as_type<float>(v73), as_type<float>(v19), as_type<float>(v20)));
    uint v75 = (uint)(int)as_type<float>(v74);
    uint v76 = v75 << 24u;
    uint v77 = v67 | v76;
    ((device uint*)p0)[i] = v77;
}
