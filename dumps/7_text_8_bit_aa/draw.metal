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
    constant uint &stride [[buffer(6)]],
    constant uint &x0 [[buffer(7)]],
    constant uint &y0 [[buffer(8)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    device uchar *p2 [[buffer(3)]],
    constant uint *buf_szs [[buffer(4)]],
    uint2 pos [[thread_position_in_grid]]
) {
    uint i = (y0 + pos.y) * stride + x0 + pos.x;
    if (i >= n) return;
    uint v0 = 0u;
    uint v1 = ((device const uint*)p0)[0];
    uint v2 = ((device const uint*)p0)[1];
    uint v3 = ((device const uint*)p0)[2];
    uint v4 = ((device const uint*)p0)[3];
    uint v6 = 998277249u;
    uint v7 = 255u;
    uint v8 = 1065353216u;
    uint v9 = as_type<uint>(as_type<float>(v8) - as_type<float>(v4));
    uint v10 = 1132396544u;
    uint v11 = (uint)((device ushort*)p2)[i];
    uint v12 = (uint)(int)(short)(ushort)v11;
    uint v13 = as_type<uint>((float)(int)v12);
    uint v14 = as_type<uint>(as_type<float>(v13) * as_type<float>(998277249u));
    uint v15 = ((device uint*)p1)[i];
    uint v16 = v15 >> 24u;
    uint v17 = as_type<uint>((float)(int)v16);
    uint v18 = as_type<uint>(as_type<float>(v17) * as_type<float>(998277249u));
    uint v19 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v9), as_type<float>(v4)));
    uint v20 = as_type<uint>(as_type<float>(v19) - as_type<float>(v18));
    uint v21 = as_type<uint>(fma(as_type<float>(v14), as_type<float>(v20), as_type<float>(v18)));
    uint v22 = as_type<uint>(max(as_type<float>(v21), as_type<float>(0u)));
    uint v23 = as_type<uint>(min(as_type<float>(v22), as_type<float>(1065353216u)));
    uint v24 = as_type<uint>(as_type<float>(v23) * as_type<float>(1132396544u));
    uint v25 = v15 >> 8u;
    uint v26 = v25 & 255u;
    uint v27 = as_type<uint>((float)(int)v26);
    uint v28 = as_type<uint>(as_type<float>(v27) * as_type<float>(998277249u));
    uint v29 = as_type<uint>(fma(as_type<float>(v28), as_type<float>(v9), as_type<float>(v2)));
    uint v30 = as_type<uint>(as_type<float>(v29) - as_type<float>(v28));
    uint v31 = as_type<uint>(fma(as_type<float>(v14), as_type<float>(v30), as_type<float>(v28)));
    uint v32 = as_type<uint>(max(as_type<float>(v31), as_type<float>(0u)));
    uint v33 = as_type<uint>(min(as_type<float>(v32), as_type<float>(1065353216u)));
    uint v34 = as_type<uint>(as_type<float>(v33) * as_type<float>(1132396544u));
    uint v35 = v15 >> 16u;
    uint v36 = v35 & 255u;
    uint v37 = as_type<uint>((float)(int)v36);
    uint v38 = as_type<uint>(as_type<float>(v37) * as_type<float>(998277249u));
    uint v39 = as_type<uint>(fma(as_type<float>(v38), as_type<float>(v9), as_type<float>(v3)));
    uint v40 = as_type<uint>(as_type<float>(v39) - as_type<float>(v38));
    uint v41 = as_type<uint>(fma(as_type<float>(v14), as_type<float>(v40), as_type<float>(v38)));
    uint v42 = as_type<uint>(max(as_type<float>(v41), as_type<float>(0u)));
    uint v43 = as_type<uint>(min(as_type<float>(v42), as_type<float>(1065353216u)));
    uint v44 = as_type<uint>(as_type<float>(v43) * as_type<float>(1132396544u));
    uint v45 = v15 & 255u;
    uint v46 = as_type<uint>((float)(int)v45);
    uint v47 = as_type<uint>(as_type<float>(v46) * as_type<float>(998277249u));
    uint v48 = as_type<uint>(fma(as_type<float>(v47), as_type<float>(v9), as_type<float>(v1)));
    uint v49 = as_type<uint>(as_type<float>(v48) - as_type<float>(v47));
    uint v50 = as_type<uint>(fma(as_type<float>(v14), as_type<float>(v49), as_type<float>(v47)));
    uint v51 = as_type<uint>(max(as_type<float>(v50), as_type<float>(0u)));
    uint v52 = as_type<uint>(min(as_type<float>(v51), as_type<float>(1065353216u)));
    uint v53 = as_type<uint>(as_type<float>(v52) * as_type<float>(1132396544u));
    uint v54 = as_type<uint>((int)rint(as_type<float>(v53)));
    uint v55 = as_type<uint>((int)rint(as_type<float>(v34)));
    uint v56 = v54 & 255u;
    uint v57 = v55 & 255u;
    uint v58 = v56 | (v57 << 8u);
    uint v59 = as_type<uint>((int)rint(as_type<float>(v44)));
    uint v60 = v59 & 255u;
    uint v61 = v58 | (v60 << 16u);
    uint v62 = as_type<uint>((int)rint(as_type<float>(v24)));
    uint v63 = v61 | (v62 << 24u);
    ((device uint*)p1)[i] = v63;
}
