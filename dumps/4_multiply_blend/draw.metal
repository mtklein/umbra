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
    uint v1 = ((device const uint*)p0)[0];
    uint v2 = ((device const uint*)p0)[1];
    uint v3 = ((device const uint*)p0)[2];
    uint v4 = ((device const uint*)p0)[3];
    uint v5 = ((device const uint*)p0)[4];
    uint v6 = ((device const uint*)p0)[5];
    uint v7 = ((device const uint*)p0)[6];
    uint v8 = ((device const uint*)p0)[7];
    uint v9 = 1065353216u;
    uint v10 = 255u;
    uint v11 = 998277249u;
    uint v12 = as_type<uint>(as_type<float>(v9) - as_type<float>(v4));
    uint v13 = 1132396544u;
    uint v14 = pos.x;
    uint v15 = as_type<uint>((float)(int)v14);
    uint v16 = as_type<float>(v15) <  as_type<float>(v7) ? 0xffffffffu : 0u;
    uint v17 = as_type<float>(v5) <= as_type<float>(v15) ? 0xffffffffu : 0u;
    uint v18 = v17 & v16;
    uint v19 = pos.y;
    uint v20 = as_type<uint>((float)(int)v19);
    uint v21 = as_type<float>(v20) <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v22 = as_type<float>(v6) <= as_type<float>(v20) ? 0xffffffffu : 0u;
    uint v23 = v22 & v21;
    uint v24 = v18 & v23;
    uint v25 = (v24 & v9) | (~v24 & v0);
    uint v26 = ((device uint*)p1)[i];
    uint v27 = v26 >> 24u;
    uint v28 = as_type<uint>((float)(int)v27);
    uint v29 = as_type<uint>(as_type<float>(v11) * as_type<float>(v28));
    uint v30 = as_type<uint>(as_type<float>(v9) - as_type<float>(v29));
    uint v31 = as_type<uint>(as_type<float>(v29) * as_type<float>(v12));
    uint v32 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v30), as_type<float>(v31)));
    uint v33 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v29), as_type<float>(v32)));
    uint v34 = as_type<uint>(as_type<float>(v33) - as_type<float>(v29));
    uint v35 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v34), as_type<float>(v29)));
    uint v36 = as_type<uint>(as_type<float>(v35) * as_type<float>(v13));
    uint v37 = as_type<uint>((int)rint(as_type<float>(v36)));
    uint v38 = v26 & v10;
    uint v39 = as_type<uint>((float)(int)v38);
    uint v40 = v26 >> 8u;
    uint v41 = v10 & v40;
    uint v42 = as_type<uint>((float)(int)v41);
    uint v43 = v26 >> 16u;
    uint v44 = v10 & v43;
    uint v45 = as_type<uint>((float)(int)v44);
    uint v46 = as_type<uint>(as_type<float>(v11) * as_type<float>(v39));
    uint v47 = as_type<uint>(as_type<float>(v11) * as_type<float>(v42));
    uint v48 = as_type<uint>(as_type<float>(v11) * as_type<float>(v45));
    uint v49 = as_type<uint>(as_type<float>(v46) * as_type<float>(v12));
    uint v50 = as_type<uint>(fma(as_type<float>(v1), as_type<float>(v30), as_type<float>(v49)));
    uint v51 = as_type<uint>(fma(as_type<float>(v1), as_type<float>(v46), as_type<float>(v50)));
    uint v52 = as_type<uint>(as_type<float>(v51) - as_type<float>(v46));
    uint v53 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v52), as_type<float>(v46)));
    uint v54 = as_type<uint>(as_type<float>(v47) * as_type<float>(v12));
    uint v55 = as_type<uint>(fma(as_type<float>(v2), as_type<float>(v30), as_type<float>(v54)));
    uint v56 = as_type<uint>(fma(as_type<float>(v2), as_type<float>(v47), as_type<float>(v55)));
    uint v57 = as_type<uint>(as_type<float>(v56) - as_type<float>(v47));
    uint v58 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v57), as_type<float>(v47)));
    uint v59 = as_type<uint>(as_type<float>(v48) * as_type<float>(v12));
    uint v60 = as_type<uint>(fma(as_type<float>(v3), as_type<float>(v30), as_type<float>(v59)));
    uint v61 = as_type<uint>(fma(as_type<float>(v3), as_type<float>(v48), as_type<float>(v60)));
    uint v62 = as_type<uint>(as_type<float>(v61) - as_type<float>(v48));
    uint v63 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v62), as_type<float>(v48)));
    uint v64 = as_type<uint>(as_type<float>(v53) * as_type<float>(v13));
    uint v65 = as_type<uint>((int)rint(as_type<float>(v64)));
    uint v66 = as_type<uint>(as_type<float>(v58) * as_type<float>(v13));
    uint v67 = as_type<uint>((int)rint(as_type<float>(v66)));
    uint v68 = as_type<uint>(as_type<float>(v63) * as_type<float>(v13));
    uint v69 = as_type<uint>((int)rint(as_type<float>(v68)));
    uint v70 = v10 & v69;
    uint v71 = v10 & v65;
    uint v72 = v10 & v67;
    uint v73 = v72 << 8u;
    uint v74 = v71 | v73;
    uint v75 = v70 << 16u;
    uint v76 = v74 | v75;
    uint v77 = v37 << 24u;
    uint v78 = v76 | v77;
    ((device uint*)p1)[i] = v78;
}
