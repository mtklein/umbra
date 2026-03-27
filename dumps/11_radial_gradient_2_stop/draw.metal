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
    constant uint &w [[buffer(2)]],
    constant uint *buf_szs [[buffer(3)]],
    constant uint *buf_rbs [[buffer(4)]],
    constant uint &x0 [[buffer(5)]],
    constant uint &y0 [[buffer(6)]],
    device uchar *p0 [[buffer(0)]],
    device uchar *p1 [[buffer(1)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= w) return;
    uint x = x0 + pos.x;
    uint y = y0 + pos.y;
    uint v0 = 0u;
    uint v1 = ((device const uint*)p0)[0];
    uint v2 = ((device const uint*)p0)[1];
    uint v3 = ((device const uint*)p0)[2];
    uint v4 = 1065353216u;
    uint v5 = ((device const uint*)p0)[3];
    uint v6 = ((device const uint*)p0)[4];
    uint v7 = ((device const uint*)p0)[5];
    uint v8 = ((device const uint*)p0)[6];
    uint v9 = ((device const uint*)p0)[7];
    uint v10 = ((device const uint*)p0)[8];
    uint v11 = ((device const uint*)p0)[9];
    uint v12 = ((device const uint*)p0)[10];
    uint v13 = as_type<uint>(as_type<float>(v9) - as_type<float>(v5));
    uint v14 = as_type<uint>(as_type<float>(v10) - as_type<float>(v6));
    uint v15 = as_type<uint>(as_type<float>(v11) - as_type<float>(v7));
    uint v16 = as_type<uint>(as_type<float>(v12) - as_type<float>(v8));
    uint v17 = 255u;
    uint v18 = 1132396544u;
    uint v19 = x0 + pos.x;
    uint v20 = as_type<uint>((float)(int)v19);
    uint v21 = as_type<uint>(as_type<float>(v20) - as_type<float>(v1));
    uint v22 = y0 + pos.y;
    uint v23 = as_type<uint>((float)(int)v22);
    uint v24 = as_type<uint>(as_type<float>(v23) - as_type<float>(v2));
    uint v25 = as_type<uint>(as_type<float>(v24) * as_type<float>(v24));
    uint v26 = as_type<uint>(fma(as_type<float>(v21), as_type<float>(v21), as_type<float>(v25)));
    uint v27 = as_type<uint>(sqrt(as_type<float>(v26)));
    uint v28 = as_type<uint>(as_type<float>(v3) * as_type<float>(v27));
    uint v29 = as_type<uint>(max(as_type<float>(v28), as_type<float>(0u)));
    uint v30 = as_type<uint>(min(as_type<float>(v29), as_type<float>(1065353216u)));
    uint v31 = as_type<uint>(fma(as_type<float>(v30), as_type<float>(v16), as_type<float>(v8)));
    uint v32 = as_type<uint>(max(as_type<float>(v31), as_type<float>(0u)));
    uint v33 = as_type<uint>(min(as_type<float>(v32), as_type<float>(1065353216u)));
    uint v34 = as_type<uint>(as_type<float>(v33) * as_type<float>(1132396544u));
    uint v35 = as_type<uint>((int)rint(as_type<float>(v34)));
    uint v36 = as_type<uint>(fma(as_type<float>(v30), as_type<float>(v13), as_type<float>(v5)));
    uint v37 = as_type<uint>(max(as_type<float>(v36), as_type<float>(0u)));
    uint v38 = as_type<uint>(min(as_type<float>(v37), as_type<float>(1065353216u)));
    uint v39 = as_type<uint>(as_type<float>(v38) * as_type<float>(1132396544u));
    uint v40 = as_type<uint>((int)rint(as_type<float>(v39)));
    uint v41 = v40 & 255u;
    uint v42 = as_type<uint>(fma(as_type<float>(v30), as_type<float>(v14), as_type<float>(v6)));
    uint v43 = as_type<uint>(max(as_type<float>(v42), as_type<float>(0u)));
    uint v44 = as_type<uint>(min(as_type<float>(v43), as_type<float>(1065353216u)));
    uint v45 = as_type<uint>(as_type<float>(v44) * as_type<float>(1132396544u));
    uint v46 = as_type<uint>((int)rint(as_type<float>(v45)));
    uint v47 = v46 & 255u;
    uint v48 = v41 | (v47 << 8u);
    uint v49 = as_type<uint>(fma(as_type<float>(v30), as_type<float>(v15), as_type<float>(v7)));
    uint v50 = as_type<uint>(max(as_type<float>(v49), as_type<float>(0u)));
    uint v51 = as_type<uint>(min(as_type<float>(v50), as_type<float>(1065353216u)));
    uint v52 = as_type<uint>(as_type<float>(v51) * as_type<float>(1132396544u));
    uint v53 = as_type<uint>((int)rint(as_type<float>(v52)));
    uint v54 = v53 & 255u;
    uint v55 = v48 | (v54 << 16u);
    uint v56 = v55 | (v35 << 24u);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v56;
}
