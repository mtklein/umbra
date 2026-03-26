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
    constant uint &w [[buffer(5)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    device uchar *p2 [[buffer(3)]],
    constant uint *buf_szs [[buffer(4)]],
    uint2 pos [[thread_position_in_grid]]
) {
    uint i = pos.y * w + pos.x;
    if (i >= n) return;
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
    uint v16 = pos.x;
    uint v17 = as_type<uint>((float)(int)v16);
    uint v18 = as_type<uint>(as_type<float>(v17) - as_type<float>(v2));
    uint v19 = pos.y;
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
    uint v45 = as_type<uint>(as_type<float>(v44) * as_type<float>(1132396544u));
    uint v46 = v32 + 1u;
    uint v47 = ((device uint*)p2)[safe_ix((int)v46,buf_szs[2],4)] & oob_mask((int)v46,buf_szs[2],4);
    uint v48 = as_type<uint>(as_type<float>(v38) - as_type<float>(v47));
    uint v49 = as_type<uint>(fma(as_type<float>(v41), as_type<float>(v48), as_type<float>(v47)));
    uint v50 = as_type<uint>(as_type<float>(v49) * as_type<float>(1132396544u));
    uint v51 = as_type<uint>((int)rint(as_type<float>(v50)));
    uint v52 = v51 & 255u;
    uint v53 = v32 + 2u;
    uint v54 = ((device uint*)p2)[safe_ix((int)v53,buf_szs[2],4)] & oob_mask((int)v53,buf_szs[2],4);
    uint v55 = as_type<uint>(as_type<float>(v39) - as_type<float>(v54));
    uint v56 = as_type<uint>(fma(as_type<float>(v41), as_type<float>(v55), as_type<float>(v54)));
    uint v57 = as_type<uint>(as_type<float>(v56) * as_type<float>(1132396544u));
    uint v58 = as_type<uint>((int)rint(as_type<float>(v57)));
    uint v59 = v58 & 255u;
    uint v60 = as_type<uint>((int)rint(as_type<float>(v45)));
    uint v61 = ((device uint*)p2)[safe_ix((int)v34,buf_szs[2],4)] & oob_mask((int)v34,buf_szs[2],4);
    uint v62 = ((device uint*)p2)[safe_ix((int)v32,buf_szs[2],4)] & oob_mask((int)v32,buf_szs[2],4);
    uint v63 = as_type<uint>(as_type<float>(v61) - as_type<float>(v62));
    uint v64 = as_type<uint>(fma(as_type<float>(v41), as_type<float>(v63), as_type<float>(v62)));
    uint v65 = as_type<uint>(as_type<float>(v64) * as_type<float>(1132396544u));
    uint v66 = as_type<uint>((int)rint(as_type<float>(v65)));
    uint v67 = v66 & 255u;
    uint v68 = v67 | (v52 << 8u);
    uint v69 = v68 | (v59 << 16u);
    uint v70 = v69 | (v60 << 24u);
    ((device uint*)p1)[i] = v70;
}
