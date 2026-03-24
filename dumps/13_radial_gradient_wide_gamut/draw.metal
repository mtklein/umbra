#include <metal_stdlib>
using namespace metal;

static inline int clamp_ix(int ix, uint bytes, int elem) {
    int hi = (int)(bytes / (uint)elem) - 1;
    if (hi < 0) hi = 0;
    return clamp(ix, 0, hi);
}

kernel void umbra_entry(
    constant uint &n [[buffer(0)]],
    constant uint &w [[buffer(5)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    device uchar *p2 [[buffer(3)]],
    constant uint *buf_szs [[buffer(4)]],
    uint2 pos [[thread_position_in_grid]]
) {
    uint i = pos.y * w + pos.x;
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
    uint v20 = 255u;
    uint v21 = i;
    uint v22 = v21 + v1;
    uint v23 = as_type<uint>((float)(int)v22);
    uint v24 = as_type<uint>(as_type<float>(v23) - as_type<float>(v7));
    uint v25 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v24), as_type<float>(v13)));
    uint v26 = as_type<uint>(sqrt(as_type<float>(v25)));
    uint v27 = as_type<uint>(as_type<float>(v11) * as_type<float>(v26));
    uint v28 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v27)));
    uint v29 = as_type<uint>(min(as_type<float>(v28), as_type<float>(v14)));
    uint v30 = as_type<uint>(as_type<float>(v29) * as_type<float>(v17));
    uint v31 = as_type<uint>(floor(as_type<float>(v30)));
    uint v32 = as_type<uint>(min(as_type<float>(v18), as_type<float>(v31)));
    uint v33 = (uint)(int)as_type<float>(v32);
    uint v34 = v33 << 2u;
    uint v35 = v10 + v34;
    uint v36 = as_type<uint>(as_type<float>(v30) - as_type<float>(v32));
    uint v37 = v2 + v35;
    uint v38 = v6 + v35;
    uint v39 = v8 + v35;
    uint v40 = ((device uint*)p2)[clamp_ix((int)v39,buf_szs[2],4)];
    uint v41 = v2 + v34;
    uint v42 = ((device uint*)p2)[clamp_ix((int)v41,buf_szs[2],4)];
    uint v43 = v6 + v34;
    uint v44 = ((device uint*)p2)[clamp_ix((int)v43,buf_szs[2],4)];
    uint v45 = v8 + v34;
    uint v46 = ((device uint*)p2)[clamp_ix((int)v45,buf_szs[2],4)];
    uint v47 = as_type<uint>(as_type<float>(v40) - as_type<float>(v46));
    uint v48 = as_type<uint>(fma(as_type<float>(v36), as_type<float>(v47), as_type<float>(v46)));
    uint v49 = as_type<uint>(as_type<float>(v48) * as_type<float>(v19));
    uint v50 = as_type<uint>((int)rint(as_type<float>(v49)));
    uint v51 = ((device uint*)p2)[clamp_ix((int)v37,buf_szs[2],4)];
    uint v52 = as_type<uint>(as_type<float>(v51) - as_type<float>(v42));
    uint v53 = as_type<uint>(fma(as_type<float>(v36), as_type<float>(v52), as_type<float>(v42)));
    uint v54 = ((device uint*)p2)[clamp_ix((int)v38,buf_szs[2],4)];
    uint v55 = as_type<uint>(as_type<float>(v54) - as_type<float>(v44));
    uint v56 = as_type<uint>(fma(as_type<float>(v36), as_type<float>(v55), as_type<float>(v44)));
    uint v57 = ((device uint*)p2)[clamp_ix((int)v35,buf_szs[2],4)];
    uint v58 = ((device uint*)p2)[clamp_ix((int)v34,buf_szs[2],4)];
    uint v59 = as_type<uint>(as_type<float>(v57) - as_type<float>(v58));
    uint v60 = as_type<uint>(fma(as_type<float>(v36), as_type<float>(v59), as_type<float>(v58)));
    uint v61 = as_type<uint>(as_type<float>(v60) * as_type<float>(v19));
    uint v62 = as_type<uint>((int)rint(as_type<float>(v61)));
    uint v63 = as_type<uint>(as_type<float>(v53) * as_type<float>(v19));
    uint v64 = as_type<uint>((int)rint(as_type<float>(v63)));
    uint v65 = as_type<uint>(as_type<float>(v56) * as_type<float>(v19));
    uint v66 = as_type<uint>((int)rint(as_type<float>(v65)));
    uint v67 = v66 & v20;
    uint v68 = v62 & v20;
    uint v69 = v64 & v20;
    uint v70 = v69 << 8u;
    uint v71 = v68 | v70;
    uint v72 = v67 << 16u;
    uint v73 = v71 | v72;
    uint v74 = v50 << 24u;
    uint v75 = v73 | v74;
    ((device uint*)p0)[i] = v75;
}
