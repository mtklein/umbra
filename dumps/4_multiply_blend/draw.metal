#include <metal_stdlib>
using namespace metal;

static inline int safe_ix(int ix, uint bytes, int elem) {
    int count = (int)(bytes / (uint)elem);
    return clamp(ix, 0, max(count-1, 0));
}
static inline uint oob_mask(int ix, uint bytes, int elem) {
    int count = (int)(bytes / (uint)elem);
    return (ix >= 0 && ix < count) ? ~0u : 0u;
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
    uint v29 = as_type<uint>(as_type<float>(v28) * as_type<float>(998277249u));
    uint v30 = as_type<uint>(as_type<float>(v9) - as_type<float>(v29));
    uint v31 = as_type<uint>(as_type<float>(v29) * as_type<float>(v12));
    uint v32 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v30), as_type<float>(v31)));
    uint v33 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v29), as_type<float>(v32)));
    uint v34 = as_type<uint>(as_type<float>(v33) - as_type<float>(v29));
    uint v35 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v34), as_type<float>(v29)));
    uint v36 = as_type<uint>(as_type<float>(v35) * as_type<float>(1132396544u));
    uint v37 = as_type<uint>((int)rint(as_type<float>(v36)));
    uint v38 = v26 >> 8u;
    uint v39 = v38 & 255u;
    uint v40 = as_type<uint>((float)(int)v39);
    uint v41 = as_type<uint>(as_type<float>(v40) * as_type<float>(998277249u));
    uint v42 = v26 >> 16u;
    uint v43 = v42 & 255u;
    uint v44 = as_type<uint>((float)(int)v43);
    uint v45 = as_type<uint>(as_type<float>(v44) * as_type<float>(998277249u));
    uint v46 = v26 & 255u;
    uint v47 = as_type<uint>((float)(int)v46);
    uint v48 = as_type<uint>(as_type<float>(v47) * as_type<float>(998277249u));
    uint v49 = as_type<uint>(as_type<float>(v48) * as_type<float>(v12));
    uint v50 = as_type<uint>(fma(as_type<float>(v1), as_type<float>(v30), as_type<float>(v49)));
    uint v51 = as_type<uint>(fma(as_type<float>(v1), as_type<float>(v48), as_type<float>(v50)));
    uint v52 = as_type<uint>(as_type<float>(v51) - as_type<float>(v48));
    uint v53 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v52), as_type<float>(v48)));
    uint v54 = as_type<uint>(as_type<float>(v53) * as_type<float>(1132396544u));
    uint v55 = as_type<uint>((int)rint(as_type<float>(v54)));
    uint v56 = v55 & 255u;
    uint v57 = as_type<uint>(as_type<float>(v41) * as_type<float>(v12));
    uint v58 = as_type<uint>(fma(as_type<float>(v2), as_type<float>(v30), as_type<float>(v57)));
    uint v59 = as_type<uint>(fma(as_type<float>(v2), as_type<float>(v41), as_type<float>(v58)));
    uint v60 = as_type<uint>(as_type<float>(v59) - as_type<float>(v41));
    uint v61 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v60), as_type<float>(v41)));
    uint v62 = as_type<uint>(as_type<float>(v61) * as_type<float>(1132396544u));
    uint v63 = as_type<uint>((int)rint(as_type<float>(v62)));
    uint v64 = v63 & 255u;
    uint v65 = v56 | (v64 << 8u);
    uint v66 = as_type<uint>(as_type<float>(v45) * as_type<float>(v12));
    uint v67 = as_type<uint>(fma(as_type<float>(v3), as_type<float>(v30), as_type<float>(v66)));
    uint v68 = as_type<uint>(fma(as_type<float>(v3), as_type<float>(v45), as_type<float>(v67)));
    uint v69 = as_type<uint>(as_type<float>(v68) - as_type<float>(v45));
    uint v70 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v69), as_type<float>(v45)));
    uint v71 = as_type<uint>(as_type<float>(v70) * as_type<float>(1132396544u));
    uint v72 = as_type<uint>((int)rint(as_type<float>(v71)));
    uint v73 = v72 & 255u;
    uint v74 = v65 | (v73 << 16u);
    uint v75 = v74 | (v37 << 24u);
    ((device uint*)p1)[i] = v75;
}
