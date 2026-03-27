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
    uint v14 = 255u;
    uint v15 = 1132396544u;
    uint v16 = x0 + pos.x;
    uint v17 = as_type<uint>((float)(int)v16);
    uint v18 = y0 + pos.y;
    uint v19 = as_type<uint>((float)(int)v18);
    uint v20 = as_type<uint>(as_type<float>(v19) * as_type<float>(v3));
    uint v21 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v2), as_type<float>(v20)));
    uint v22 = as_type<uint>(as_type<float>(v4) + as_type<float>(v21));
    uint v23 = as_type<uint>(max(as_type<float>(v22), as_type<float>(0u)));
    uint v24 = as_type<uint>(min(as_type<float>(v23), as_type<float>(1065353216u)));
    uint v25 = as_type<uint>(as_type<float>(v24) * as_type<float>(v8));
    uint v26 = as_type<uint>(floor(as_type<float>(v25)));
    uint v27 = as_type<uint>(min(as_type<float>(v9), as_type<float>(v26)));
    uint v28 = (uint)(int)as_type<float>(v27);
    uint v29 = v28 << 2u;
    uint v30 = v29 + 3u;
    uint v31 = ((device uint*)p2)[safe_ix((int)v30,buf_szs[2],4)] & oob_mask((int)v30,buf_szs[2],4);
    uint v32 = v29 + 4u;
    uint v33 = v32 + 3u;
    uint v34 = ((device uint*)p2)[safe_ix((int)v33,buf_szs[2],4)] & oob_mask((int)v33,buf_szs[2],4);
    uint v35 = as_type<uint>(as_type<float>(v34) - as_type<float>(v31));
    uint v36 = v32 + 1u;
    uint v37 = ((device uint*)p2)[safe_ix((int)v36,buf_szs[2],4)] & oob_mask((int)v36,buf_szs[2],4);
    uint v38 = v32 + 2u;
    uint v39 = ((device uint*)p2)[safe_ix((int)v38,buf_szs[2],4)] & oob_mask((int)v38,buf_szs[2],4);
    uint v40 = as_type<uint>(as_type<float>(v25) - as_type<float>(v27));
    uint v41 = as_type<uint>(fma(as_type<float>(v40), as_type<float>(v35), as_type<float>(v31)));
    uint v42 = as_type<uint>(max(as_type<float>(v41), as_type<float>(0u)));
    uint v43 = as_type<uint>(min(as_type<float>(v42), as_type<float>(1065353216u)));
    uint v44 = as_type<uint>(as_type<float>(v43) * as_type<float>(1132396544u));
    uint v45 = as_type<uint>((int)rint(as_type<float>(v44)));
    uint v46 = v29 + 1u;
    uint v47 = ((device uint*)p2)[safe_ix((int)v46,buf_szs[2],4)] & oob_mask((int)v46,buf_szs[2],4);
    uint v48 = as_type<uint>(as_type<float>(v37) - as_type<float>(v47));
    uint v49 = as_type<uint>(fma(as_type<float>(v40), as_type<float>(v48), as_type<float>(v47)));
    uint v50 = as_type<uint>(max(as_type<float>(v49), as_type<float>(0u)));
    uint v51 = as_type<uint>(min(as_type<float>(v50), as_type<float>(1065353216u)));
    uint v52 = as_type<uint>(as_type<float>(v51) * as_type<float>(1132396544u));
    uint v53 = as_type<uint>((int)rint(as_type<float>(v52)));
    uint v54 = v53 & 255u;
    uint v55 = v29 + 2u;
    uint v56 = ((device uint*)p2)[safe_ix((int)v55,buf_szs[2],4)] & oob_mask((int)v55,buf_szs[2],4);
    uint v57 = as_type<uint>(as_type<float>(v39) - as_type<float>(v56));
    uint v58 = as_type<uint>(fma(as_type<float>(v40), as_type<float>(v57), as_type<float>(v56)));
    uint v59 = as_type<uint>(max(as_type<float>(v58), as_type<float>(0u)));
    uint v60 = as_type<uint>(min(as_type<float>(v59), as_type<float>(1065353216u)));
    uint v61 = as_type<uint>(as_type<float>(v60) * as_type<float>(1132396544u));
    uint v62 = as_type<uint>((int)rint(as_type<float>(v61)));
    uint v63 = v62 & 255u;
    uint v64 = ((device uint*)p2)[safe_ix((int)v32,buf_szs[2],4)] & oob_mask((int)v32,buf_szs[2],4);
    uint v65 = ((device uint*)p2)[safe_ix((int)v29,buf_szs[2],4)] & oob_mask((int)v29,buf_szs[2],4);
    uint v66 = as_type<uint>(as_type<float>(v64) - as_type<float>(v65));
    uint v67 = as_type<uint>(fma(as_type<float>(v40), as_type<float>(v66), as_type<float>(v65)));
    uint v68 = as_type<uint>(max(as_type<float>(v67), as_type<float>(0u)));
    uint v69 = as_type<uint>(min(as_type<float>(v68), as_type<float>(1065353216u)));
    uint v70 = as_type<uint>(as_type<float>(v69) * as_type<float>(1132396544u));
    uint v71 = as_type<uint>((int)rint(as_type<float>(v70)));
    uint v72 = v71 & 255u;
    uint v73 = v72 | (v54 << 8u);
    uint v74 = v73 | (v63 << 16u);
    uint v75 = v74 | (v45 << 24u);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v75;
}
