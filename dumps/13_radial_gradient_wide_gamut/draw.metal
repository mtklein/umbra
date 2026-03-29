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
    constant uint *buf_fmts [[buffer(8)]],
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
    uint v17 = as_type<uint>(as_type<float>(v16) - as_type<float>(v2));
    uint v18 = y0 + pos.y;
    uint v19 = as_type<uint>((float)(int)v18);
    uint v20 = as_type<uint>(as_type<float>(v19) - as_type<float>(v3));
    uint v21 = as_type<uint>(as_type<float>(v20) * as_type<float>(v20));
    uint v22 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v17), as_type<float>(v21)));
    uint v23 = as_type<uint>(sqrt(as_type<float>(v22)));
    uint v24 = as_type<uint>(as_type<float>(v4) * as_type<float>(v23));
    uint v25 = as_type<uint>(max(as_type<float>(v24), as_type<float>(0u)));
    uint v26 = as_type<uint>(min(as_type<float>(v25), as_type<float>(1065353216u)));
    uint v27 = as_type<uint>(as_type<float>(v26) * as_type<float>(v8));
    uint v28 = as_type<uint>(floor(as_type<float>(v27)));
    uint v29 = as_type<uint>(min(as_type<float>(v9), as_type<float>(v28)));
    uint v30 = (uint)(int)as_type<float>(v29);
    uint v31 = v30 << 2u;
    uint v32 = v31 + 3u;
    uint v33 = ((device uint*)p2)[safe_ix((int)v32,buf_szs[2],4)] & oob_mask((int)v32,buf_szs[2],4);
    uint v34 = v31 + 4u;
    uint v35 = v34 + 3u;
    uint v36 = ((device uint*)p2)[safe_ix((int)v35,buf_szs[2],4)] & oob_mask((int)v35,buf_szs[2],4);
    uint v37 = as_type<uint>(as_type<float>(v36) - as_type<float>(v33));
    uint v38 = v34 + 1u;
    uint v39 = ((device uint*)p2)[safe_ix((int)v38,buf_szs[2],4)] & oob_mask((int)v38,buf_szs[2],4);
    uint v40 = v34 + 2u;
    uint v41 = ((device uint*)p2)[safe_ix((int)v40,buf_szs[2],4)] & oob_mask((int)v40,buf_szs[2],4);
    uint v42 = as_type<uint>(as_type<float>(v27) - as_type<float>(v29));
    uint v43 = as_type<uint>(fma(as_type<float>(v42), as_type<float>(v37), as_type<float>(v33)));
    uint v44 = as_type<uint>(max(as_type<float>(v43), as_type<float>(0u)));
    uint v45 = as_type<uint>(min(as_type<float>(v44), as_type<float>(1065353216u)));
    uint v46 = as_type<uint>(as_type<float>(v45) * as_type<float>(1132396544u));
    uint v47 = as_type<uint>((int)rint(as_type<float>(v46)));
    uint v48 = v31 + 1u;
    uint v49 = ((device uint*)p2)[safe_ix((int)v48,buf_szs[2],4)] & oob_mask((int)v48,buf_szs[2],4);
    uint v50 = as_type<uint>(as_type<float>(v39) - as_type<float>(v49));
    uint v51 = as_type<uint>(fma(as_type<float>(v42), as_type<float>(v50), as_type<float>(v49)));
    uint v52 = as_type<uint>(max(as_type<float>(v51), as_type<float>(0u)));
    uint v53 = as_type<uint>(min(as_type<float>(v52), as_type<float>(1065353216u)));
    uint v54 = as_type<uint>(as_type<float>(v53) * as_type<float>(1132396544u));
    uint v55 = as_type<uint>((int)rint(as_type<float>(v54)));
    uint v56 = v31 + 2u;
    uint v57 = ((device uint*)p2)[safe_ix((int)v56,buf_szs[2],4)] & oob_mask((int)v56,buf_szs[2],4);
    uint v58 = as_type<uint>(as_type<float>(v41) - as_type<float>(v57));
    uint v59 = as_type<uint>(fma(as_type<float>(v42), as_type<float>(v58), as_type<float>(v57)));
    uint v60 = as_type<uint>(max(as_type<float>(v59), as_type<float>(0u)));
    uint v61 = as_type<uint>(min(as_type<float>(v60), as_type<float>(1065353216u)));
    uint v62 = as_type<uint>(as_type<float>(v61) * as_type<float>(1132396544u));
    uint v63 = as_type<uint>((int)rint(as_type<float>(v62)));
    uint v64 = ((device uint*)p2)[safe_ix((int)v34,buf_szs[2],4)] & oob_mask((int)v34,buf_szs[2],4);
    uint v65 = ((device uint*)p2)[safe_ix((int)v31,buf_szs[2],4)] & oob_mask((int)v31,buf_szs[2],4);
    uint v66 = as_type<uint>(as_type<float>(v64) - as_type<float>(v65));
    uint v67 = as_type<uint>(fma(as_type<float>(v42), as_type<float>(v66), as_type<float>(v65)));
    uint v68 = as_type<uint>(max(as_type<float>(v67), as_type<float>(0u)));
    uint v69 = as_type<uint>(min(as_type<float>(v68), as_type<float>(1065353216u)));
    uint v70 = as_type<uint>(as_type<float>(v69) * as_type<float>(1132396544u));
    uint v71 = as_type<uint>((int)rint(as_type<float>(v70)));
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = (v71 & 0xFFu) | ((v55 & 0xFFu) << 8) | ((v63 & 0xFFu) << 16) | (v47 << 24);
}
