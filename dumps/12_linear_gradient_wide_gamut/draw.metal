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
    uint v32 = as_type<uint>(as_type<float>(v28) - as_type<float>(v31));
    uint v33 = (uint)(int)as_type<float>(v31);
    uint v34 = v33 << 2u;
    uint v35 = ((device uint*)p2)[clamp_ix((int)v34,buf_szs[2],4)];
    uint v36 = v10 + v34;
    uint v37 = ((device uint*)p2)[clamp_ix((int)v36,buf_szs[2],4)];
    uint v38 = as_type<uint>(as_type<float>(v37) - as_type<float>(v35));
    uint v39 = as_type<uint>(fma(as_type<float>(v32), as_type<float>(v38), as_type<float>(v35)));
    uint v40 = as_type<uint>(as_type<float>(v39) * as_type<float>(v18));
    uint v41 = as_type<uint>(as_type<float>(v19) + as_type<float>(v40));
    uint v42 = (uint)(int)as_type<float>(v41);
    uint v43 = v42 & v20;
    uint v44 = v2 + v34;
    uint v45 = ((device uint*)p2)[clamp_ix((int)v44,buf_szs[2],4)];
    uint v46 = v2 + v36;
    uint v47 = ((device uint*)p2)[clamp_ix((int)v46,buf_szs[2],4)];
    uint v48 = as_type<uint>(as_type<float>(v47) - as_type<float>(v45));
    uint v49 = as_type<uint>(fma(as_type<float>(v32), as_type<float>(v48), as_type<float>(v45)));
    uint v50 = as_type<uint>(as_type<float>(v49) * as_type<float>(v18));
    uint v51 = as_type<uint>(as_type<float>(v19) + as_type<float>(v50));
    uint v52 = (uint)(int)as_type<float>(v51);
    uint v53 = v52 & v20;
    uint v54 = v53 << 8u;
    uint v55 = v43 | v54;
    uint v56 = v6 + v34;
    uint v57 = ((device uint*)p2)[clamp_ix((int)v56,buf_szs[2],4)];
    uint v58 = v6 + v36;
    uint v59 = ((device uint*)p2)[clamp_ix((int)v58,buf_szs[2],4)];
    uint v60 = as_type<uint>(as_type<float>(v59) - as_type<float>(v57));
    uint v61 = as_type<uint>(fma(as_type<float>(v32), as_type<float>(v60), as_type<float>(v57)));
    uint v62 = as_type<uint>(as_type<float>(v61) * as_type<float>(v18));
    uint v63 = as_type<uint>(as_type<float>(v19) + as_type<float>(v62));
    uint v64 = (uint)(int)as_type<float>(v63);
    uint v65 = v64 & v20;
    uint v66 = v65 << 16u;
    uint v67 = v55 | v66;
    uint v68 = v8 + v34;
    uint v69 = ((device uint*)p2)[clamp_ix((int)v68,buf_szs[2],4)];
    uint v70 = v8 + v36;
    uint v71 = ((device uint*)p2)[clamp_ix((int)v70,buf_szs[2],4)];
    uint v72 = as_type<uint>(as_type<float>(v71) - as_type<float>(v69));
    uint v73 = as_type<uint>(fma(as_type<float>(v32), as_type<float>(v72), as_type<float>(v69)));
    uint v74 = as_type<uint>(as_type<float>(v73) * as_type<float>(v18));
    uint v75 = as_type<uint>(as_type<float>(v19) + as_type<float>(v74));
    uint v76 = (uint)(int)as_type<float>(v75);
    uint v77 = v76 << 24u;
    uint v78 = v67 | v77;
    ((device uint*)p0)[i] = v78;
}
