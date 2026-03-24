#include <metal_stdlib>
using namespace metal;

static inline int clamp_ix(int ix, uint bytes, int elem) {
    int hi = (int)(bytes / (uint)elem) - 1;
    if (hi < 0) hi = 0;
    return clamp(ix, 0, hi);
}

kernel void umbra_entry(
    constant uint &n [[buffer(0)]],
    constant uint &w [[buffer(4)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    constant uint *buf_szs [[buffer(3)]],
    uint2 pos [[thread_position_in_grid]]
) {
    uint i = pos.y * w + pos.x;
    if (i >= n) return;
    uint v0 = 0u;
    uint v1 = ((device const uint*)p1)[0];
    uint v2 = ((device const uint*)p1)[2];
    uint v3 = ((device const uint*)p1)[3];
    uint v4 = ((device const uint*)p1)[4];
    uint v5 = ((device const uint*)p1)[5];
    uint v6 = ((device const uint*)p1)[6];
    uint v7 = ((device const uint*)p1)[7];
    uint v8 = ((device const uint*)p1)[8];
    uint v9 = ((device const uint*)p1)[9];
    uint v10 = 1065353216u;
    uint v11 = 255u;
    uint v12 = 998277249u;
    uint v13 = as_type<uint>(as_type<float>(v10) - as_type<float>(v5));
    uint v14 = 1132396544u;
    uint v15 = pos.x;
    uint v16 = as_type<uint>((float)(int)v15);
    uint v17 = as_type<float>(v16) <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v18 = as_type<float>(v6) <= as_type<float>(v16) ? 0xffffffffu : 0u;
    uint v19 = v18 & v17;
    uint v20 = pos.y;
    uint v21 = v20 * v1;
    uint v22 = as_type<uint>((float)(int)v20);
    uint v23 = as_type<float>(v22) <  as_type<float>(v9) ? 0xffffffffu : 0u;
    uint v24 = as_type<float>(v7) <= as_type<float>(v22) ? 0xffffffffu : 0u;
    uint v25 = v24 & v23;
    uint v26 = v19 & v25;
    uint v27 = (v26 & v10) | (~v26 & v0);
    uint v28 = v15 + v21;
    uint v29 = ((device uint*)p0)[clamp_ix((int)v28,buf_szs[0],4)];
    uint v30 = v29 >> 24u;
    uint v31 = as_type<uint>((float)(int)v30);
    uint v32 = as_type<uint>(as_type<float>(v12) * as_type<float>(v31));
    uint v33 = as_type<uint>(as_type<float>(v10) - as_type<float>(v32));
    uint v34 = as_type<uint>(as_type<float>(v32) * as_type<float>(v13));
    uint v35 = as_type<uint>(fma(as_type<float>(v5), as_type<float>(v33), as_type<float>(v34)));
    uint v36 = as_type<uint>(fma(as_type<float>(v5), as_type<float>(v32), as_type<float>(v35)));
    uint v37 = as_type<uint>(as_type<float>(v36) - as_type<float>(v32));
    uint v38 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v37), as_type<float>(v32)));
    uint v39 = as_type<uint>(as_type<float>(v38) * as_type<float>(v14));
    uint v40 = as_type<uint>((int)rint(as_type<float>(v39)));
    uint v41 = v29 & v11;
    uint v42 = as_type<uint>((float)(int)v41);
    uint v43 = v29 >> 8u;
    uint v44 = v11 & v43;
    uint v45 = as_type<uint>((float)(int)v44);
    uint v46 = v29 >> 16u;
    uint v47 = v11 & v46;
    uint v48 = as_type<uint>((float)(int)v47);
    uint v49 = as_type<uint>(as_type<float>(v12) * as_type<float>(v42));
    uint v50 = as_type<uint>(as_type<float>(v12) * as_type<float>(v45));
    uint v51 = as_type<uint>(as_type<float>(v12) * as_type<float>(v48));
    uint v52 = as_type<uint>(as_type<float>(v49) * as_type<float>(v13));
    uint v53 = as_type<uint>(fma(as_type<float>(v2), as_type<float>(v33), as_type<float>(v52)));
    uint v54 = as_type<uint>(fma(as_type<float>(v2), as_type<float>(v49), as_type<float>(v53)));
    uint v55 = as_type<uint>(as_type<float>(v54) - as_type<float>(v49));
    uint v56 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v55), as_type<float>(v49)));
    uint v57 = as_type<uint>(as_type<float>(v50) * as_type<float>(v13));
    uint v58 = as_type<uint>(fma(as_type<float>(v3), as_type<float>(v33), as_type<float>(v57)));
    uint v59 = as_type<uint>(fma(as_type<float>(v3), as_type<float>(v50), as_type<float>(v58)));
    uint v60 = as_type<uint>(as_type<float>(v59) - as_type<float>(v50));
    uint v61 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v60), as_type<float>(v50)));
    uint v62 = as_type<uint>(as_type<float>(v51) * as_type<float>(v13));
    uint v63 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v33), as_type<float>(v62)));
    uint v64 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v51), as_type<float>(v63)));
    uint v65 = as_type<uint>(as_type<float>(v64) - as_type<float>(v51));
    uint v66 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v65), as_type<float>(v51)));
    uint v67 = as_type<uint>(as_type<float>(v56) * as_type<float>(v14));
    uint v68 = as_type<uint>((int)rint(as_type<float>(v67)));
    uint v69 = as_type<uint>(as_type<float>(v61) * as_type<float>(v14));
    uint v70 = as_type<uint>((int)rint(as_type<float>(v69)));
    uint v71 = as_type<uint>(as_type<float>(v66) * as_type<float>(v14));
    uint v72 = as_type<uint>((int)rint(as_type<float>(v71)));
    uint v73 = v11 & v72;
    uint v74 = v11 & v68;
    uint v75 = v11 & v70;
    uint v76 = v75 << 8u;
    uint v77 = v74 | v76;
    uint v78 = v73 << 16u;
    uint v79 = v77 | v78;
    uint v80 = v40 << 24u;
    uint v81 = v79 | v80;
    ((device uint*)p0)[clamp_ix((int)v28,buf_szs[0],4)] = v81;
}
