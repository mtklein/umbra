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
    uint v6 = 1065353216u;
    uint v7 = as_type<uint>(as_type<float>(v6) - as_type<float>(v4));
    uint v8 = (uint)((device ushort*)p2)[i];
    uint v9 = (uint)(int)(short)(ushort)v8;
    uint v10 = as_type<uint>((float)(int)v9);
    uint v11 = as_type<uint>(as_type<float>(v10) * as_type<float>(998277249u));
    uint v12 = as_type<uint>(as_type<float>(v11) - as_type<float>(1054867456u));
    uint v13 = as_type<uint>(as_type<float>(v12) * as_type<float>(1090519040u));
    uint v14 = as_type<uint>(max(as_type<float>(v13), as_type<float>(0u)));
    uint v15 = as_type<uint>(min(as_type<float>(v14), as_type<float>(1065353216u)));
    uint v16 = ((device uint*)p1)[i];
    uint v17 = v16 >> 24u;
    uint v18 = as_type<uint>((float)(int)v17);
    uint v19 = as_type<uint>(as_type<float>(v18) * as_type<float>(998277249u));
    uint v20 = as_type<uint>(fma(as_type<float>(v19), as_type<float>(v7), as_type<float>(v4)));
    uint v21 = as_type<uint>(as_type<float>(v20) - as_type<float>(v19));
    uint v22 = as_type<uint>(fma(as_type<float>(v15), as_type<float>(v21), as_type<float>(v19)));
    uint v23 = as_type<uint>(as_type<float>(v22) * as_type<float>(1132396544u));
    uint v24 = v16 >> 8u;
    uint v25 = v24 & 255u;
    uint v26 = as_type<uint>((float)(int)v25);
    uint v27 = as_type<uint>(as_type<float>(v26) * as_type<float>(998277249u));
    uint v28 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v7), as_type<float>(v2)));
    uint v29 = as_type<uint>(as_type<float>(v28) - as_type<float>(v27));
    uint v30 = as_type<uint>(fma(as_type<float>(v15), as_type<float>(v29), as_type<float>(v27)));
    uint v31 = as_type<uint>(as_type<float>(v30) * as_type<float>(1132396544u));
    uint v32 = as_type<uint>((int)rint(as_type<float>(v31)));
    uint v33 = v32 & 255u;
    uint v34 = as_type<uint>((int)rint(as_type<float>(v23)));
    uint v35 = v16 >> 16u;
    uint v36 = v35 & 255u;
    uint v37 = as_type<uint>((float)(int)v36);
    uint v38 = as_type<uint>(as_type<float>(v37) * as_type<float>(998277249u));
    uint v39 = as_type<uint>(fma(as_type<float>(v38), as_type<float>(v7), as_type<float>(v3)));
    uint v40 = as_type<uint>(as_type<float>(v39) - as_type<float>(v38));
    uint v41 = as_type<uint>(fma(as_type<float>(v15), as_type<float>(v40), as_type<float>(v38)));
    uint v42 = as_type<uint>(as_type<float>(v41) * as_type<float>(1132396544u));
    uint v43 = as_type<uint>((int)rint(as_type<float>(v42)));
    uint v44 = v43 & 255u;
    uint v45 = v16 & 255u;
    uint v46 = as_type<uint>((float)(int)v45);
    uint v47 = as_type<uint>(as_type<float>(v46) * as_type<float>(998277249u));
    uint v48 = as_type<uint>(fma(as_type<float>(v47), as_type<float>(v7), as_type<float>(v1)));
    uint v49 = as_type<uint>(as_type<float>(v48) - as_type<float>(v47));
    uint v50 = as_type<uint>(fma(as_type<float>(v15), as_type<float>(v49), as_type<float>(v47)));
    uint v51 = as_type<uint>(as_type<float>(v50) * as_type<float>(1132396544u));
    uint v52 = as_type<uint>((int)rint(as_type<float>(v51)));
    uint v53 = v52 & 255u;
    uint v54 = v53 | (v33 << 8u);
    uint v55 = v54 | (v44 << 16u);
    uint v56 = v55 | (v34 << 24u);
    ((device uint*)p1)[i] = v56;
}
