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
    uint v10 = as_type<uint>(as_type<float>(v9) - as_type<float>(v4));
    uint v11 = pos.x;
    uint v12 = as_type<uint>((float)(int)v11);
    uint v13 = as_type<float>(v12) <  as_type<float>(v7) ? 0xffffffffu : 0u;
    uint v14 = as_type<float>(v5) <= as_type<float>(v12) ? 0xffffffffu : 0u;
    uint v15 = v14 & v13;
    uint v16 = pos.y;
    uint v17 = as_type<uint>((float)(int)v16);
    uint v18 = as_type<float>(v17) <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v19 = as_type<float>(v6) <= as_type<float>(v17) ? 0xffffffffu : 0u;
    uint v20 = v19 & v18;
    uint v21 = v15 & v20;
    uint v22 = (v21 & v9) | (~v21 & v0);
    uint v23 = ((device uint*)p1)[i];
    uint v24 = v23 >> 24u;
    uint v25 = as_type<uint>((float)(int)v24);
    uint v26 = as_type<uint>(as_type<float>(v25) * as_type<float>(998277249u));
    uint v27 = as_type<uint>(as_type<float>(v9) - as_type<float>(v26));
    uint v28 = as_type<uint>(as_type<float>(v26) * as_type<float>(v10));
    uint v29 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v27), as_type<float>(v28)));
    uint v30 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v26), as_type<float>(v29)));
    uint v31 = as_type<uint>(as_type<float>(v30) - as_type<float>(v26));
    uint v32 = as_type<uint>(fma(as_type<float>(v22), as_type<float>(v31), as_type<float>(v26)));
    uint v33 = as_type<uint>(as_type<float>(v32) * as_type<float>(1132396544u));
    uint v34 = v23 >> 8u;
    uint v35 = v34 & 255u;
    uint v36 = as_type<uint>((float)(int)v35);
    uint v37 = as_type<uint>(as_type<float>(v36) * as_type<float>(998277249u));
    uint v38 = as_type<uint>((int)rint(as_type<float>(v33)));
    uint v39 = v23 >> 16u;
    uint v40 = v39 & 255u;
    uint v41 = as_type<uint>((float)(int)v40);
    uint v42 = as_type<uint>(as_type<float>(v41) * as_type<float>(998277249u));
    uint v43 = v23 & 255u;
    uint v44 = as_type<uint>((float)(int)v43);
    uint v45 = as_type<uint>(as_type<float>(v44) * as_type<float>(998277249u));
    uint v46 = as_type<uint>(as_type<float>(v45) * as_type<float>(v10));
    uint v47 = as_type<uint>(fma(as_type<float>(v1), as_type<float>(v27), as_type<float>(v46)));
    uint v48 = as_type<uint>(fma(as_type<float>(v1), as_type<float>(v45), as_type<float>(v47)));
    uint v49 = as_type<uint>(as_type<float>(v48) - as_type<float>(v45));
    uint v50 = as_type<uint>(fma(as_type<float>(v22), as_type<float>(v49), as_type<float>(v45)));
    uint v51 = as_type<uint>(as_type<float>(v50) * as_type<float>(1132396544u));
    uint v52 = as_type<uint>((int)rint(as_type<float>(v51)));
    uint v53 = v52 & 255u;
    uint v54 = as_type<uint>(as_type<float>(v37) * as_type<float>(v10));
    uint v55 = as_type<uint>(fma(as_type<float>(v2), as_type<float>(v27), as_type<float>(v54)));
    uint v56 = as_type<uint>(fma(as_type<float>(v2), as_type<float>(v37), as_type<float>(v55)));
    uint v57 = as_type<uint>(as_type<float>(v56) - as_type<float>(v37));
    uint v58 = as_type<uint>(fma(as_type<float>(v22), as_type<float>(v57), as_type<float>(v37)));
    uint v59 = as_type<uint>(as_type<float>(v58) * as_type<float>(1132396544u));
    uint v60 = as_type<uint>((int)rint(as_type<float>(v59)));
    uint v61 = v60 & 255u;
    uint v62 = v53 | (v61 << 8u);
    uint v63 = as_type<uint>(as_type<float>(v42) * as_type<float>(v10));
    uint v64 = as_type<uint>(fma(as_type<float>(v3), as_type<float>(v27), as_type<float>(v63)));
    uint v65 = as_type<uint>(fma(as_type<float>(v3), as_type<float>(v42), as_type<float>(v64)));
    uint v66 = as_type<uint>(as_type<float>(v65) - as_type<float>(v42));
    uint v67 = as_type<uint>(fma(as_type<float>(v22), as_type<float>(v66), as_type<float>(v42)));
    uint v68 = as_type<uint>(as_type<float>(v67) * as_type<float>(1132396544u));
    uint v69 = as_type<uint>((int)rint(as_type<float>(v68)));
    uint v70 = v69 & 255u;
    uint v71 = v62 | (v70 << 16u);
    uint v72 = v71 | (v38 << 24u);
    ((device uint*)p1)[i] = v72;
}
