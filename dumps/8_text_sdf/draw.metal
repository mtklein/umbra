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
    uint v16 = as_type<uint>(as_type<float>(v6) * as_type<float>(v15));
    uint v17 = as_type<uint>(as_type<float>(v16) - as_type<float>(v7));
    uint v18 = as_type<uint>(as_type<float>(v8) * as_type<float>(v17));
    uint v19 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v18)));
    uint v20 = as_type<uint>(min(as_type<float>(v9), as_type<float>(v19)));
    uint v21 = ((device uint*)p1)[i];
    uint v22 = v21 >> 24u;
    uint v23 = as_type<uint>((float)(int)v22);
    uint v24 = as_type<uint>(as_type<float>(v6) * as_type<float>(v23));
    uint v25 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v11), as_type<float>(v4)));
    uint v26 = as_type<uint>(as_type<float>(v25) - as_type<float>(v24));
    uint v27 = as_type<uint>(fma(as_type<float>(v20), as_type<float>(v26), as_type<float>(v24)));
    uint v28 = as_type<uint>(as_type<float>(v27) * as_type<float>(v12));
    uint v29 = as_type<uint>((int)rint(as_type<float>(v28)));
    uint v30 = v21 & v10;
    uint v31 = as_type<uint>((float)(int)v30);
    uint v32 = v21 >> 8u;
    uint v33 = v10 & v32;
    uint v34 = as_type<uint>((float)(int)v33);
    uint v35 = v21 >> 16u;
    uint v36 = v10 & v35;
    uint v37 = as_type<uint>((float)(int)v36);
    uint v38 = as_type<uint>(as_type<float>(v6) * as_type<float>(v31));
    uint v39 = as_type<uint>(fma(as_type<float>(v38), as_type<float>(v11), as_type<float>(v1)));
    uint v40 = as_type<uint>(as_type<float>(v39) - as_type<float>(v38));
    uint v41 = as_type<uint>(fma(as_type<float>(v20), as_type<float>(v40), as_type<float>(v38)));
    uint v42 = as_type<uint>(as_type<float>(v6) * as_type<float>(v34));
    uint v43 = as_type<uint>(fma(as_type<float>(v42), as_type<float>(v11), as_type<float>(v2)));
    uint v44 = as_type<uint>(as_type<float>(v43) - as_type<float>(v42));
    uint v45 = as_type<uint>(fma(as_type<float>(v20), as_type<float>(v44), as_type<float>(v42)));
    uint v46 = as_type<uint>(as_type<float>(v6) * as_type<float>(v37));
    uint v47 = as_type<uint>(fma(as_type<float>(v46), as_type<float>(v11), as_type<float>(v3)));
    uint v48 = as_type<uint>(as_type<float>(v47) - as_type<float>(v46));
    uint v49 = as_type<uint>(fma(as_type<float>(v20), as_type<float>(v48), as_type<float>(v46)));
    uint v50 = as_type<uint>(as_type<float>(v41) * as_type<float>(v12));
    uint v51 = as_type<uint>((int)rint(as_type<float>(v50)));
    uint v52 = as_type<uint>(as_type<float>(v45) * as_type<float>(v12));
    uint v53 = as_type<uint>((int)rint(as_type<float>(v52)));
    uint v54 = as_type<uint>(as_type<float>(v49) * as_type<float>(v12));
    uint v55 = as_type<uint>((int)rint(as_type<float>(v54)));
    uint v56 = v10 & v55;
    uint v57 = v10 & v51;
    uint v58 = v10 & v53;
    uint v59 = v58 << 8u;
    uint v60 = v57 | v59;
    uint v61 = v56 << 16u;
    uint v62 = v60 | v61;
    uint v63 = v29 << 24u;
    uint v64 = v62 | v63;
    ((device uint*)p1)[i] = v64;
}
