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
    uint v3 = 2u;
    uint v4 = ((device const uint*)p1)[2];
    uint v5 = 3u;
    uint v6 = ((device const uint*)p1)[3];
    uint v7 = 4u;
    uint v8 = ((device const uint*)p1)[4];
    uint v9 = 1065353216u;
    uint v10 = ((device const uint*)p1)[5];
    uint v11 = 1073741824u;
    uint v12 = as_type<uint>(as_type<float>(v10) - as_type<float>(v9));
    uint v13 = as_type<uint>(as_type<float>(v10) - as_type<float>(v11));
    uint v14 = 1u;
    uint v15 = 1132396544u;
    uint v16 = 255u;
    uint v17 = pos.x;
    uint v18 = as_type<uint>((float)(int)v17);
    uint v19 = as_type<uint>(as_type<float>(v18) - as_type<float>(v4));
    uint v20 = pos.y;
    uint v21 = v20 * v1;
    uint v22 = as_type<uint>((float)(int)v20);
    uint v23 = as_type<uint>(as_type<float>(v22) - as_type<float>(v6));
    uint v24 = as_type<uint>(as_type<float>(v23) * as_type<float>(v23));
    uint v25 = as_type<uint>(fma(as_type<float>(v19), as_type<float>(v19), as_type<float>(v24)));
    uint v26 = as_type<uint>(sqrt(as_type<float>(v25)));
    uint v27 = as_type<uint>(as_type<float>(v8) * as_type<float>(v26));
    uint v28 = v17 + v21;
    uint v29 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v27)));
    uint v30 = as_type<uint>(min(as_type<float>(v29), as_type<float>(v9)));
    uint v31 = as_type<uint>(as_type<float>(v30) * as_type<float>(v12));
    uint v32 = as_type<uint>(floor(as_type<float>(v31)));
    uint v33 = as_type<uint>(min(as_type<float>(v13), as_type<float>(v32)));
    uint v34 = (uint)(int)as_type<float>(v33);
    uint v35 = v34 << 2u;
    uint v36 = v7 + v35;
    uint v37 = as_type<uint>(as_type<float>(v31) - as_type<float>(v33));
    uint v38 = v36 + v14;
    uint v39 = v3 + v36;
    uint v40 = v5 + v36;
    uint v41 = ((device uint*)p2)[clamp_ix((int)v40,buf_szs[2],4)];
    uint v42 = v35 + v14;
    uint v43 = ((device uint*)p2)[clamp_ix((int)v42,buf_szs[2],4)];
    uint v44 = v3 + v35;
    uint v45 = ((device uint*)p2)[clamp_ix((int)v44,buf_szs[2],4)];
    uint v46 = v5 + v35;
    uint v47 = ((device uint*)p2)[clamp_ix((int)v46,buf_szs[2],4)];
    uint v48 = as_type<uint>(as_type<float>(v41) - as_type<float>(v47));
    uint v49 = as_type<uint>(fma(as_type<float>(v37), as_type<float>(v48), as_type<float>(v47)));
    uint v50 = as_type<uint>(as_type<float>(v49) * as_type<float>(v15));
    uint v51 = as_type<uint>((int)rint(as_type<float>(v50)));
    uint v52 = ((device uint*)p2)[clamp_ix((int)v38,buf_szs[2],4)];
    uint v53 = as_type<uint>(as_type<float>(v52) - as_type<float>(v43));
    uint v54 = as_type<uint>(fma(as_type<float>(v37), as_type<float>(v53), as_type<float>(v43)));
    uint v55 = ((device uint*)p2)[clamp_ix((int)v39,buf_szs[2],4)];
    uint v56 = as_type<uint>(as_type<float>(v55) - as_type<float>(v45));
    uint v57 = as_type<uint>(fma(as_type<float>(v37), as_type<float>(v56), as_type<float>(v45)));
    uint v58 = ((device uint*)p2)[clamp_ix((int)v36,buf_szs[2],4)];
    uint v59 = ((device uint*)p2)[clamp_ix((int)v35,buf_szs[2],4)];
    uint v60 = as_type<uint>(as_type<float>(v58) - as_type<float>(v59));
    uint v61 = as_type<uint>(fma(as_type<float>(v37), as_type<float>(v60), as_type<float>(v59)));
    uint v62 = as_type<uint>(as_type<float>(v61) * as_type<float>(v15));
    uint v63 = as_type<uint>((int)rint(as_type<float>(v62)));
    uint v64 = as_type<uint>(as_type<float>(v54) * as_type<float>(v15));
    uint v65 = as_type<uint>((int)rint(as_type<float>(v64)));
    uint v66 = as_type<uint>(as_type<float>(v57) * as_type<float>(v15));
    uint v67 = as_type<uint>((int)rint(as_type<float>(v66)));
    uint v68 = v67 & v16;
    uint v69 = v63 & v16;
    uint v70 = v65 & v16;
    uint v71 = v70 << 8u;
    uint v72 = v69 | v71;
    uint v73 = v68 << 16u;
    uint v74 = v72 | v73;
    uint v75 = v51 << 24u;
    uint v76 = v74 | v75;
    ((device uint*)p0)[clamp_ix((int)v28,buf_szs[0],4)] = v76;
}
