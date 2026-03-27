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
    constant uint &w [[buffer(2)]],
    constant uint *buf_szs [[buffer(3)]],
    constant uint *buf_rbs [[buffer(4)]],
    constant uint &x0 [[buffer(5)]],
    constant uint &y0 [[buffer(6)]],
    device uchar *p0 [[buffer(0)]],
    device uchar *p1 [[buffer(1)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= w) return;
    uint x = x0 + pos.x;
    uint y = y0 + pos.y;
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
    uint v14 = x0 + pos.x;
    uint v15 = as_type<uint>((float)(int)v14);
    uint v16 = as_type<float>(v15) <  as_type<float>(v7) ? 0xffffffffu : 0u;
    uint v17 = as_type<float>(v5) <= as_type<float>(v15) ? 0xffffffffu : 0u;
    uint v18 = v17 & v16;
    uint v19 = y0 + pos.y;
    uint v20 = as_type<uint>((float)(int)v19);
    uint v21 = as_type<float>(v20) <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v22 = as_type<float>(v6) <= as_type<float>(v20) ? 0xffffffffu : 0u;
    uint v23 = v22 & v21;
    uint v24 = v18 & v23;
    uint v25 = (v24 & v9) | (~v24 & v0);
    uint v26 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v27 = v26 >> 24u;
    uint v28 = as_type<uint>((float)(int)v27);
    uint v29 = as_type<uint>(as_type<float>(v28) * as_type<float>(998277249u));
    uint v30 = as_type<uint>(as_type<float>(v29) * as_type<float>(v12));
    uint v31 = v26 >> 8u;
    uint v32 = v31 & 255u;
    uint v33 = as_type<uint>((float)(int)v32);
    uint v34 = as_type<uint>(as_type<float>(v33) * as_type<float>(998277249u));
    uint v35 = v26 >> 16u;
    uint v36 = v35 & 255u;
    uint v37 = as_type<uint>((float)(int)v36);
    uint v38 = as_type<uint>(as_type<float>(v37) * as_type<float>(998277249u));
    uint v39 = v26 & 255u;
    uint v40 = as_type<uint>((float)(int)v39);
    uint v41 = as_type<uint>(as_type<float>(v40) * as_type<float>(998277249u));
    uint v42 = as_type<uint>(as_type<float>(v41) * as_type<float>(v12));
    uint v43 = as_type<uint>(as_type<float>(v34) * as_type<float>(v12));
    uint v44 = as_type<uint>(as_type<float>(v38) * as_type<float>(v12));
    uint v45 = as_type<uint>(as_type<float>(v9) - as_type<float>(v29));
    uint v46 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v45), as_type<float>(v30)));
    uint v47 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v29), as_type<float>(v46)));
    uint v48 = as_type<uint>(fma(as_type<float>(v1), as_type<float>(v45), as_type<float>(v42)));
    uint v49 = as_type<uint>(fma(as_type<float>(v1), as_type<float>(v41), as_type<float>(v48)));
    uint v50 = as_type<uint>(fma(as_type<float>(v2), as_type<float>(v45), as_type<float>(v43)));
    uint v51 = as_type<uint>(fma(as_type<float>(v2), as_type<float>(v34), as_type<float>(v50)));
    uint v52 = as_type<uint>(fma(as_type<float>(v3), as_type<float>(v45), as_type<float>(v44)));
    uint v53 = as_type<uint>(fma(as_type<float>(v3), as_type<float>(v38), as_type<float>(v52)));
    uint v54 = as_type<uint>(as_type<float>(v49) - as_type<float>(v41));
    uint v55 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v54), as_type<float>(v41)));
    uint v56 = as_type<uint>(as_type<float>(v51) - as_type<float>(v34));
    uint v57 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v56), as_type<float>(v34)));
    uint v58 = as_type<uint>(as_type<float>(v53) - as_type<float>(v38));
    uint v59 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v58), as_type<float>(v38)));
    uint v60 = as_type<uint>(as_type<float>(v47) - as_type<float>(v29));
    uint v61 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v60), as_type<float>(v29)));
    uint v62 = as_type<uint>(max(as_type<float>(v55), as_type<float>(0u)));
    uint v63 = as_type<uint>(max(as_type<float>(v57), as_type<float>(0u)));
    uint v64 = as_type<uint>(max(as_type<float>(v59), as_type<float>(0u)));
    uint v65 = as_type<uint>(max(as_type<float>(v61), as_type<float>(0u)));
    uint v66 = as_type<uint>(min(as_type<float>(v65), as_type<float>(1065353216u)));
    uint v67 = as_type<uint>(as_type<float>(v66) * as_type<float>(1132396544u));
    uint v68 = as_type<uint>(min(as_type<float>(v62), as_type<float>(1065353216u)));
    uint v69 = as_type<uint>(as_type<float>(v68) * as_type<float>(1132396544u));
    uint v70 = as_type<uint>(min(as_type<float>(v63), as_type<float>(1065353216u)));
    uint v71 = as_type<uint>(as_type<float>(v70) * as_type<float>(1132396544u));
    uint v72 = as_type<uint>(min(as_type<float>(v64), as_type<float>(1065353216u)));
    uint v73 = as_type<uint>(as_type<float>(v72) * as_type<float>(1132396544u));
    uint v74 = as_type<uint>((int)rint(as_type<float>(v69)));
    uint v75 = as_type<uint>((int)rint(as_type<float>(v71)));
    uint v76 = v74 & 255u;
    uint v77 = v75 & 255u;
    uint v78 = v76 | (v77 << 8u);
    uint v79 = as_type<uint>((int)rint(as_type<float>(v73)));
    uint v80 = v79 & 255u;
    uint v81 = v78 | (v80 << 16u);
    uint v82 = as_type<uint>((int)rint(as_type<float>(v67)));
    uint v83 = v81 | (v82 << 24u);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v83;
}
