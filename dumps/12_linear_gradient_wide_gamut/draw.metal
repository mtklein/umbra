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
    constant uint &w [[buffer(3)]],
    constant uint *buf_szs [[buffer(4)]],
    constant uint *buf_rbs [[buffer(5)]],
    constant uint &x0 [[buffer(6)]],
    constant uint &y0 [[buffer(7)]],
    device uchar *p0 [[buffer(0)]],
    device uchar *p1 [[buffer(1)]],
    device uchar *p2 [[buffer(2)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= w) return;
    uint x = x0 + pos.x;
    uint y = y0 + pos.y;
    uint v0 = 0u;
    uint v2 = ((device const uint*)p0)[0];
    uint v3 = ((device const uint*)p0)[1];
    uint v4 = ((device const uint*)p0)[2];
    uint v5 = 1065353216u;
    uint v6 = ((device const uint*)p0)[3];
    uint v7 = 1073741824u;
    uint v8 = as_type<uint>(as_type<float>(v6) - as_type<float>(1065353216u));
    uint v9 = as_type<uint>(as_type<float>(v6) - as_type<float>(1073741824u));
    uint v10 = 2u;
    uint v11 = 4u;
    uint v12 = 1u;
    uint v13 = 3u;
    uint v14 = 1132396544u;
    uint v15 = x0 + pos.x;
    uint v16 = as_type<uint>((float)(int)v15);
    uint v17 = y0 + pos.y;
    uint v18 = as_type<uint>((float)(int)v17);
    uint v19 = as_type<uint>(as_type<float>(v18) * as_type<float>(v3));
    uint v20 = as_type<uint>(fma(as_type<float>(v16), as_type<float>(v2), as_type<float>(v19)));
    uint v21 = as_type<uint>(as_type<float>(v4) + as_type<float>(v20));
    uint v22 = as_type<uint>(max(as_type<float>(v21), as_type<float>(0u)));
    uint v23 = as_type<uint>(min(as_type<float>(v22), as_type<float>(1065353216u)));
    uint v24 = as_type<uint>(as_type<float>(v23) * as_type<float>(v8));
    uint v25 = as_type<uint>(floor(as_type<float>(v24)));
    uint v26 = as_type<uint>(min(as_type<float>(v9), as_type<float>(v25)));
    uint v27 = (uint)(int)as_type<float>(v26);
    uint v28 = v27 << 2u;
    uint v29 = v28 + 3u;
    uint v30 = ((device uint*)p2)[safe_ix((int)v29,buf_szs[2],4)] & oob_mask((int)v29,buf_szs[2],4);
    uint v31 = v28 + 4u;
    uint v32 = v31 + 3u;
    uint v33 = ((device uint*)p2)[safe_ix((int)v32,buf_szs[2],4)] & oob_mask((int)v32,buf_szs[2],4);
    uint v34 = as_type<uint>(as_type<float>(v33) - as_type<float>(v30));
    uint v35 = v31 + 1u;
    uint v36 = ((device uint*)p2)[safe_ix((int)v35,buf_szs[2],4)] & oob_mask((int)v35,buf_szs[2],4);
    uint v37 = v31 + 2u;
    uint v38 = ((device uint*)p2)[safe_ix((int)v37,buf_szs[2],4)] & oob_mask((int)v37,buf_szs[2],4);
    uint v39 = as_type<uint>(as_type<float>(v24) - as_type<float>(v26));
    uint v40 = as_type<uint>(fma(as_type<float>(v39), as_type<float>(v34), as_type<float>(v30)));
    uint v41 = as_type<uint>(max(as_type<float>(v40), as_type<float>(0u)));
    uint v42 = as_type<uint>(min(as_type<float>(v41), as_type<float>(1065353216u)));
    uint v43 = as_type<uint>(as_type<float>(v42) * as_type<float>(1132396544u));
    uint v44 = as_type<uint>((int)rint(as_type<float>(v43)));
    uint v45 = v28 + 1u;
    uint v46 = ((device uint*)p2)[safe_ix((int)v45,buf_szs[2],4)] & oob_mask((int)v45,buf_szs[2],4);
    uint v47 = as_type<uint>(as_type<float>(v36) - as_type<float>(v46));
    uint v48 = as_type<uint>(fma(as_type<float>(v39), as_type<float>(v47), as_type<float>(v46)));
    uint v49 = as_type<uint>(max(as_type<float>(v48), as_type<float>(0u)));
    uint v50 = as_type<uint>(min(as_type<float>(v49), as_type<float>(1065353216u)));
    uint v51 = as_type<uint>(as_type<float>(v50) * as_type<float>(1132396544u));
    uint v52 = as_type<uint>((int)rint(as_type<float>(v51)));
    uint v53 = v28 + 2u;
    uint v54 = ((device uint*)p2)[safe_ix((int)v53,buf_szs[2],4)] & oob_mask((int)v53,buf_szs[2],4);
    uint v55 = as_type<uint>(as_type<float>(v38) - as_type<float>(v54));
    uint v56 = as_type<uint>(fma(as_type<float>(v39), as_type<float>(v55), as_type<float>(v54)));
    uint v57 = as_type<uint>(max(as_type<float>(v56), as_type<float>(0u)));
    uint v58 = as_type<uint>(min(as_type<float>(v57), as_type<float>(1065353216u)));
    uint v59 = as_type<uint>(as_type<float>(v58) * as_type<float>(1132396544u));
    uint v60 = as_type<uint>((int)rint(as_type<float>(v59)));
    uint v61 = ((device uint*)p2)[safe_ix((int)v31,buf_szs[2],4)] & oob_mask((int)v31,buf_szs[2],4);
    uint v62 = ((device uint*)p2)[safe_ix((int)v28,buf_szs[2],4)] & oob_mask((int)v28,buf_szs[2],4);
    uint v63 = as_type<uint>(as_type<float>(v61) - as_type<float>(v62));
    uint v64 = as_type<uint>(fma(as_type<float>(v39), as_type<float>(v63), as_type<float>(v62)));
    uint v65 = as_type<uint>(max(as_type<float>(v64), as_type<float>(0u)));
    uint v66 = as_type<uint>(min(as_type<float>(v65), as_type<float>(1065353216u)));
    uint v67 = as_type<uint>(as_type<float>(v66) * as_type<float>(1132396544u));
    uint v68 = as_type<uint>((int)rint(as_type<float>(v67)));
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = (v68 & 0xFFu) | ((v52 & 0xFFu) << 8) | ((v60 & 0xFFu) << 16) | (v44 << 24);
}
