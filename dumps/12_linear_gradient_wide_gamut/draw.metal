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
    uint v12 = as_type<uint>(as_type<float>(v4) * as_type<float>(v9));
    uint v13 = 1065353216u;
    uint v14 = ((device const uint*)p1)[5];
    uint v15 = 1073741824u;
    uint v16 = as_type<uint>(as_type<float>(v14) - as_type<float>(v13));
    uint v17 = as_type<uint>(as_type<float>(v14) - as_type<float>(v15));
    uint v18 = 1132396544u;
    uint v19 = 1056964608u;
    uint v20 = 255u;
    uint v21 = i;
    uint v22 = v21 + v1;
    uint v23 = as_type<uint>((float)(int)v22);
    uint v24 = as_type<uint>(fma(as_type<float>(v23), as_type<float>(v7), as_type<float>(v12)));
    uint v25 = as_type<uint>(as_type<float>(v11) + as_type<float>(v24));
    uint v26 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v25)));
    uint v27 = as_type<uint>(min(as_type<float>(v26), as_type<float>(v13)));
    uint v28 = as_type<uint>(as_type<float>(v27) * as_type<float>(v16));
    uint v29 = (uint)(int)as_type<float>(v28);
    uint v30 = as_type<uint>((float)(int)v29);
    uint v31 = as_type<uint>(min(as_type<float>(v17), as_type<float>(v30)));
    uint v32 = (uint)(int)as_type<float>(v31);
    uint v33 = v32 << 2u;
    uint v34 = v10 + v33;
    uint v35 = as_type<uint>(as_type<float>(v28) - as_type<float>(v31));
    uint v36 = v2 + v34;
    uint v37 = v6 + v34;
    uint v38 = v8 + v34;
    uint v39 = ((device uint*)p2)[clamp_ix((int)v38,buf_szs[2],4)];
    uint v40 = v2 + v33;
    uint v41 = ((device uint*)p2)[clamp_ix((int)v40,buf_szs[2],4)];
    uint v42 = v6 + v33;
    uint v43 = ((device uint*)p2)[clamp_ix((int)v42,buf_szs[2],4)];
    uint v44 = v8 + v33;
    uint v45 = ((device uint*)p2)[clamp_ix((int)v44,buf_szs[2],4)];
    uint v46 = as_type<uint>(as_type<float>(v39) - as_type<float>(v45));
    uint v47 = as_type<uint>(fma(as_type<float>(v35), as_type<float>(v46), as_type<float>(v45)));
    uint v48 = as_type<uint>(as_type<float>(v47) * as_type<float>(v18));
    uint v49 = as_type<uint>(as_type<float>(v19) + as_type<float>(v48));
    uint v50 = (uint)(int)as_type<float>(v49);
    uint v51 = ((device uint*)p2)[clamp_ix((int)v36,buf_szs[2],4)];
    uint v52 = as_type<uint>(as_type<float>(v51) - as_type<float>(v41));
    uint v53 = as_type<uint>(fma(as_type<float>(v35), as_type<float>(v52), as_type<float>(v41)));
    uint v54 = ((device uint*)p2)[clamp_ix((int)v37,buf_szs[2],4)];
    uint v55 = as_type<uint>(as_type<float>(v54) - as_type<float>(v43));
    uint v56 = as_type<uint>(fma(as_type<float>(v35), as_type<float>(v55), as_type<float>(v43)));
    uint v57 = ((device uint*)p2)[clamp_ix((int)v34,buf_szs[2],4)];
    uint v58 = ((device uint*)p2)[clamp_ix((int)v33,buf_szs[2],4)];
    uint v59 = as_type<uint>(as_type<float>(v57) - as_type<float>(v58));
    uint v60 = as_type<uint>(fma(as_type<float>(v35), as_type<float>(v59), as_type<float>(v58)));
    uint v61 = as_type<uint>(as_type<float>(v60) * as_type<float>(v18));
    uint v62 = as_type<uint>(as_type<float>(v19) + as_type<float>(v61));
    uint v63 = (uint)(int)as_type<float>(v62);
    uint v64 = as_type<uint>(as_type<float>(v53) * as_type<float>(v18));
    uint v65 = as_type<uint>(as_type<float>(v19) + as_type<float>(v64));
    uint v66 = (uint)(int)as_type<float>(v65);
    uint v67 = as_type<uint>(as_type<float>(v56) * as_type<float>(v18));
    uint v68 = as_type<uint>(as_type<float>(v19) + as_type<float>(v67));
    uint v69 = (uint)(int)as_type<float>(v68);
    uint v70 = v69 & v20;
    uint v71 = v63 & v20;
    uint v72 = v66 & v20;
    uint v73 = v72 << 8u;
    uint v74 = v71 | v73;
    uint v75 = v70 << 16u;
    uint v76 = v74 | v75;
    uint v77 = v50 << 24u;
    uint v78 = v76 | v77;
    ((device uint*)p0)[i] = v78;
}
