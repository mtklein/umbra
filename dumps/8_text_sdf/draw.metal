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
    uint v1 = ((device const uint*)p0)[0];
    uint v2 = ((device const uint*)p0)[1];
    uint v3 = ((device const uint*)p0)[2];
    uint v4 = ((device const uint*)p0)[3];
    uint v6 = 998277249u;
    uint v7 = 1054867456u;
    uint v8 = 1090519040u;
    uint v9 = 1065353216u;
    uint v10 = 255u;
    uint v11 = as_type<uint>(as_type<float>(v9) - as_type<float>(v4));
    uint v12 = 1132396544u;
    uint v13 = (uint)((device ushort*)p2)[i];
    uint v14 = (uint)(int)(short)(ushort)v13;
    uint v15 = as_type<uint>((float)(int)v14);
    uint v16 = as_type<uint>(as_type<float>(v15) * as_type<float>(998277249u));
    uint v17 = as_type<uint>(as_type<float>(v16) - as_type<float>(1054867456u));
    uint v18 = as_type<uint>(as_type<float>(v17) * as_type<float>(1090519040u));
    uint v19 = as_type<uint>(max(as_type<float>(v18), as_type<float>(0u)));
    uint v20 = as_type<uint>(min(as_type<float>(v19), as_type<float>(1065353216u)));
    uint v21 = ((device uint*)p1)[i];
    uint v22 = v21 >> 24u;
    uint v23 = as_type<uint>((float)(int)v22);
    uint v24 = as_type<uint>(as_type<float>(v23) * as_type<float>(998277249u));
    uint v25 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v11), as_type<float>(v4)));
    uint v26 = as_type<uint>(as_type<float>(v25) - as_type<float>(v24));
    uint v27 = as_type<uint>(fma(as_type<float>(v20), as_type<float>(v26), as_type<float>(v24)));
    uint v28 = as_type<uint>(as_type<float>(v27) * as_type<float>(1132396544u));
    uint v29 = as_type<uint>((int)rint(as_type<float>(v28)));
    uint v30 = v21 >> 8u;
    uint v31 = v30 & 255u;
    uint v32 = as_type<uint>((float)(int)v31);
    uint v33 = as_type<uint>(as_type<float>(v32) * as_type<float>(998277249u));
    uint v34 = as_type<uint>(fma(as_type<float>(v33), as_type<float>(v11), as_type<float>(v2)));
    uint v35 = as_type<uint>(as_type<float>(v34) - as_type<float>(v33));
    uint v36 = as_type<uint>(fma(as_type<float>(v20), as_type<float>(v35), as_type<float>(v33)));
    uint v37 = as_type<uint>(as_type<float>(v36) * as_type<float>(1132396544u));
    uint v38 = as_type<uint>((int)rint(as_type<float>(v37)));
    uint v39 = v38 & 255u;
    uint v40 = v21 >> 16u;
    uint v41 = v40 & 255u;
    uint v42 = as_type<uint>((float)(int)v41);
    uint v43 = as_type<uint>(as_type<float>(v42) * as_type<float>(998277249u));
    uint v44 = as_type<uint>(fma(as_type<float>(v43), as_type<float>(v11), as_type<float>(v3)));
    uint v45 = as_type<uint>(as_type<float>(v44) - as_type<float>(v43));
    uint v46 = as_type<uint>(fma(as_type<float>(v20), as_type<float>(v45), as_type<float>(v43)));
    uint v47 = as_type<uint>(as_type<float>(v46) * as_type<float>(1132396544u));
    uint v48 = as_type<uint>((int)rint(as_type<float>(v47)));
    uint v49 = v48 & 255u;
    uint v50 = v21 & 255u;
    uint v51 = as_type<uint>((float)(int)v50);
    uint v52 = as_type<uint>(as_type<float>(v51) * as_type<float>(998277249u));
    uint v53 = as_type<uint>(fma(as_type<float>(v52), as_type<float>(v11), as_type<float>(v1)));
    uint v54 = as_type<uint>(as_type<float>(v53) - as_type<float>(v52));
    uint v55 = as_type<uint>(fma(as_type<float>(v20), as_type<float>(v54), as_type<float>(v52)));
    uint v56 = as_type<uint>(as_type<float>(v55) * as_type<float>(1132396544u));
    uint v57 = as_type<uint>((int)rint(as_type<float>(v56)));
    uint v58 = v57 & 255u;
    uint v59 = v58 | (v39 << 8u);
    uint v60 = v59 | (v49 << 16u);
    uint v61 = v60 | (v29 << 24u);
    ((device uint*)p1)[i] = v61;
}
