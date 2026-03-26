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
    constant uint &w [[buffer(4)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    constant uint *buf_szs [[buffer(3)]],
    uint2 pos [[thread_position_in_grid]]
) {
    uint i = pos.y * w + pos.x;
    if (i >= n) return;
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
    uint v19 = pos.x;
    uint v20 = as_type<uint>((float)(int)v19);
    uint v21 = as_type<uint>(as_type<float>(v20) - as_type<float>(v1));
    uint v22 = pos.y;
    uint v23 = as_type<uint>((float)(int)v22);
    uint v24 = as_type<uint>(as_type<float>(v23) - as_type<float>(v2));
    uint v25 = as_type<uint>(as_type<float>(v24) * as_type<float>(v24));
    uint v26 = as_type<uint>(fma(as_type<float>(v21), as_type<float>(v21), as_type<float>(v25)));
    uint v27 = as_type<uint>(sqrt(as_type<float>(v26)));
    uint v28 = as_type<uint>(as_type<float>(v3) * as_type<float>(v27));
    uint v29 = as_type<uint>(max(as_type<float>(v28), as_type<float>(0u)));
    uint v30 = as_type<uint>(min(as_type<float>(v29), as_type<float>(1065353216u)));
    uint v31 = as_type<uint>(fma(as_type<float>(v30), as_type<float>(v16), as_type<float>(v8)));
    uint v32 = as_type<uint>(fma(as_type<float>(v30), as_type<float>(v13), as_type<float>(v5)));
    uint v33 = as_type<uint>(fma(as_type<float>(v30), as_type<float>(v14), as_type<float>(v6)));
    uint v34 = as_type<uint>(fma(as_type<float>(v30), as_type<float>(v15), as_type<float>(v7)));
    uint v35 = as_type<uint>(max(as_type<float>(v32), as_type<float>(0u)));
    uint v36 = as_type<uint>(max(as_type<float>(v33), as_type<float>(0u)));
    uint v37 = as_type<uint>(max(as_type<float>(v34), as_type<float>(0u)));
    uint v38 = as_type<uint>(max(as_type<float>(v31), as_type<float>(0u)));
    uint v39 = as_type<uint>(min(as_type<float>(v38), as_type<float>(1065353216u)));
    uint v40 = as_type<uint>(as_type<float>(v39) * as_type<float>(1132396544u));
    uint v41 = as_type<uint>(min(as_type<float>(v35), as_type<float>(1065353216u)));
    uint v42 = as_type<uint>(as_type<float>(v41) * as_type<float>(1132396544u));
    uint v43 = as_type<uint>(min(as_type<float>(v36), as_type<float>(1065353216u)));
    uint v44 = as_type<uint>(as_type<float>(v43) * as_type<float>(1132396544u));
    uint v45 = as_type<uint>(min(as_type<float>(v37), as_type<float>(1065353216u)));
    uint v46 = as_type<uint>(as_type<float>(v45) * as_type<float>(1132396544u));
    uint v47 = as_type<uint>((int)rint(as_type<float>(v42)));
    uint v48 = as_type<uint>((int)rint(as_type<float>(v44)));
    uint v49 = v47 & 255u;
    uint v50 = v48 & 255u;
    uint v51 = v49 | (v50 << 8u);
    uint v52 = as_type<uint>((int)rint(as_type<float>(v46)));
    uint v53 = v52 & 255u;
    uint v54 = v51 | (v53 << 16u);
    uint v55 = as_type<uint>((int)rint(as_type<float>(v40)));
    uint v56 = v54 | (v55 << 24u);
    ((device uint*)p1)[i] = v56;
}
