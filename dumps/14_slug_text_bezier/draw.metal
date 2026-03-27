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
    uint v1 = ((device const uint*)p0)[0];
    uint v2 = ((device const uint*)p0)[1];
    uint v3 = ((device const uint*)p0)[2];
    uint v4 = ((device const uint*)p0)[3];
    uint v6 = 1065353216u;
    uint v7 = 255u;
    uint v8 = 998277249u;
    uint v9 = as_type<uint>(as_type<float>(v6) - as_type<float>(v4));
    uint v10 = 1132396544u;
    uint v11 = ((device uint*)(p2 + y * buf_rbs[2]))[x];
    uint v12 = as_type<uint>(fabs(as_type<float>(v11)));
    uint v13 = as_type<uint>(min(as_type<float>(v12), as_type<float>(1065353216u)));
    uint v14 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v15 = v14 >> 24u;
    uint v16 = as_type<uint>((float)(int)v15);
    uint v17 = as_type<uint>(as_type<float>(v16) * as_type<float>(998277249u));
    uint v18 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v9), as_type<float>(v4)));
    uint v19 = as_type<uint>(as_type<float>(v18) - as_type<float>(v17));
    uint v20 = as_type<uint>(fma(as_type<float>(v13), as_type<float>(v19), as_type<float>(v17)));
    uint v21 = as_type<uint>(max(as_type<float>(v20), as_type<float>(0u)));
    uint v22 = as_type<uint>(min(as_type<float>(v21), as_type<float>(1065353216u)));
    uint v23 = as_type<uint>(as_type<float>(v22) * as_type<float>(1132396544u));
    uint v24 = as_type<uint>((int)rint(as_type<float>(v23)));
    uint v25 = v14 >> 8u;
    uint v26 = v25 & 255u;
    uint v27 = as_type<uint>((float)(int)v26);
    uint v28 = as_type<uint>(as_type<float>(v27) * as_type<float>(998277249u));
    uint v29 = as_type<uint>(fma(as_type<float>(v28), as_type<float>(v9), as_type<float>(v2)));
    uint v30 = as_type<uint>(as_type<float>(v29) - as_type<float>(v28));
    uint v31 = as_type<uint>(fma(as_type<float>(v13), as_type<float>(v30), as_type<float>(v28)));
    uint v32 = as_type<uint>(max(as_type<float>(v31), as_type<float>(0u)));
    uint v33 = as_type<uint>(min(as_type<float>(v32), as_type<float>(1065353216u)));
    uint v34 = as_type<uint>(as_type<float>(v33) * as_type<float>(1132396544u));
    uint v35 = as_type<uint>((int)rint(as_type<float>(v34)));
    uint v36 = v35 & 255u;
    uint v37 = v14 >> 16u;
    uint v38 = v37 & 255u;
    uint v39 = as_type<uint>((float)(int)v38);
    uint v40 = as_type<uint>(as_type<float>(v39) * as_type<float>(998277249u));
    uint v41 = as_type<uint>(fma(as_type<float>(v40), as_type<float>(v9), as_type<float>(v3)));
    uint v42 = as_type<uint>(as_type<float>(v41) - as_type<float>(v40));
    uint v43 = as_type<uint>(fma(as_type<float>(v13), as_type<float>(v42), as_type<float>(v40)));
    uint v44 = as_type<uint>(max(as_type<float>(v43), as_type<float>(0u)));
    uint v45 = as_type<uint>(min(as_type<float>(v44), as_type<float>(1065353216u)));
    uint v46 = as_type<uint>(as_type<float>(v45) * as_type<float>(1132396544u));
    uint v47 = as_type<uint>((int)rint(as_type<float>(v46)));
    uint v48 = v47 & 255u;
    uint v49 = v14 & 255u;
    uint v50 = as_type<uint>((float)(int)v49);
    uint v51 = as_type<uint>(as_type<float>(v50) * as_type<float>(998277249u));
    uint v52 = as_type<uint>(fma(as_type<float>(v51), as_type<float>(v9), as_type<float>(v1)));
    uint v53 = as_type<uint>(as_type<float>(v52) - as_type<float>(v51));
    uint v54 = as_type<uint>(fma(as_type<float>(v13), as_type<float>(v53), as_type<float>(v51)));
    uint v55 = as_type<uint>(max(as_type<float>(v54), as_type<float>(0u)));
    uint v56 = as_type<uint>(min(as_type<float>(v55), as_type<float>(1065353216u)));
    uint v57 = as_type<uint>(as_type<float>(v56) * as_type<float>(1132396544u));
    uint v58 = as_type<uint>((int)rint(as_type<float>(v57)));
    uint v59 = v58 & 255u;
    uint v60 = v59 | (v36 << 8u);
    uint v61 = v60 | (v48 << 16u);
    uint v62 = v61 | (v24 << 24u);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v62;
}
