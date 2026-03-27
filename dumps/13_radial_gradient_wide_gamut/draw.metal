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
    uint v18 = as_type<uint>(as_type<float>(v17) - as_type<float>(v2));
    uint v19 = y0 + pos.y;
    uint v20 = as_type<uint>((float)(int)v19);
    uint v21 = as_type<uint>(as_type<float>(v20) - as_type<float>(v3));
    uint v22 = as_type<uint>(as_type<float>(v21) * as_type<float>(v21));
    uint v23 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v18), as_type<float>(v22)));
    uint v24 = as_type<uint>(sqrt(as_type<float>(v23)));
    uint v25 = as_type<uint>(as_type<float>(v4) * as_type<float>(v24));
    uint v26 = as_type<uint>(max(as_type<float>(v25), as_type<float>(0u)));
    uint v27 = as_type<uint>(min(as_type<float>(v26), as_type<float>(1065353216u)));
    uint v28 = as_type<uint>(as_type<float>(v27) * as_type<float>(v8));
    uint v29 = as_type<uint>(floor(as_type<float>(v28)));
    uint v30 = as_type<uint>(min(as_type<float>(v9), as_type<float>(v29)));
    uint v31 = (uint)(int)as_type<float>(v30);
    uint v32 = v31 << 2u;
    uint v33 = v32 + 3u;
    uint v34 = v32 + 4u;
    uint v35 = v34 + 3u;
    uint v36 = v34 + 1u;
    uint v37 = v34 + 2u;
    uint v38 = ((device uint*)p2)[safe_ix((int)v36,buf_szs[2],4)] & oob_mask((int)v36,buf_szs[2],4);
    uint v39 = ((device uint*)p2)[safe_ix((int)v37,buf_szs[2],4)] & oob_mask((int)v37,buf_szs[2],4);
    uint v40 = ((device uint*)p2)[safe_ix((int)v35,buf_szs[2],4)] & oob_mask((int)v35,buf_szs[2],4);
    uint v41 = as_type<uint>(as_type<float>(v28) - as_type<float>(v30));
    uint v42 = ((device uint*)p2)[safe_ix((int)v33,buf_szs[2],4)] & oob_mask((int)v33,buf_szs[2],4);
    uint v43 = as_type<uint>(as_type<float>(v40) - as_type<float>(v42));
    uint v44 = as_type<uint>(fma(as_type<float>(v41), as_type<float>(v43), as_type<float>(v42)));
    uint v45 = as_type<uint>(max(as_type<float>(v44), as_type<float>(0u)));
    uint v46 = as_type<uint>(min(as_type<float>(v45), as_type<float>(1065353216u)));
    uint v47 = as_type<uint>(as_type<float>(v46) * as_type<float>(1132396544u));
    uint v48 = v32 + 1u;
    uint v49 = ((device uint*)p2)[safe_ix((int)v48,buf_szs[2],4)] & oob_mask((int)v48,buf_szs[2],4);
    uint v50 = as_type<uint>(as_type<float>(v38) - as_type<float>(v49));
    uint v51 = as_type<uint>(fma(as_type<float>(v41), as_type<float>(v50), as_type<float>(v49)));
    uint v52 = as_type<uint>(max(as_type<float>(v51), as_type<float>(0u)));
    uint v53 = as_type<uint>(min(as_type<float>(v52), as_type<float>(1065353216u)));
    uint v54 = as_type<uint>(as_type<float>(v53) * as_type<float>(1132396544u));
    uint v55 = v32 + 2u;
    uint v56 = ((device uint*)p2)[safe_ix((int)v55,buf_szs[2],4)] & oob_mask((int)v55,buf_szs[2],4);
    uint v57 = as_type<uint>(as_type<float>(v39) - as_type<float>(v56));
    uint v58 = as_type<uint>(fma(as_type<float>(v41), as_type<float>(v57), as_type<float>(v56)));
    uint v59 = as_type<uint>(max(as_type<float>(v58), as_type<float>(0u)));
    uint v60 = as_type<uint>(min(as_type<float>(v59), as_type<float>(1065353216u)));
    uint v61 = as_type<uint>(as_type<float>(v60) * as_type<float>(1132396544u));
    uint v62 = as_type<uint>((int)rint(as_type<float>(v54)));
    uint v63 = v62 & 255u;
    uint v64 = as_type<uint>((int)rint(as_type<float>(v61)));
    uint v65 = v64 & 255u;
    uint v66 = as_type<uint>((int)rint(as_type<float>(v47)));
    uint v67 = ((device uint*)p2)[safe_ix((int)v34,buf_szs[2],4)] & oob_mask((int)v34,buf_szs[2],4);
    uint v68 = ((device uint*)p2)[safe_ix((int)v32,buf_szs[2],4)] & oob_mask((int)v32,buf_szs[2],4);
    uint v69 = as_type<uint>(as_type<float>(v67) - as_type<float>(v68));
    uint v70 = as_type<uint>(fma(as_type<float>(v41), as_type<float>(v69), as_type<float>(v68)));
    uint v71 = as_type<uint>(max(as_type<float>(v70), as_type<float>(0u)));
    uint v72 = as_type<uint>(min(as_type<float>(v71), as_type<float>(1065353216u)));
    uint v73 = as_type<uint>(as_type<float>(v72) * as_type<float>(1132396544u));
    uint v74 = as_type<uint>((int)rint(as_type<float>(v73)));
    uint v75 = v74 & 255u;
    uint v76 = v75 | (v63 << 8u);
    uint v77 = v76 | (v65 << 16u);
    uint v78 = v77 | (v66 << 24u);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v78;
}
