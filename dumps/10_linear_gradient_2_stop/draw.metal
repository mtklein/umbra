#include <metal_stdlib>
using namespace metal;

static inline int clamp_ix(int ix, uint bytes, int elem) {
    int hi = (int)(bytes / (uint)elem) - 1;
    if (hi < 0) hi = 0;
    return clamp(ix, 0, hi);
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
    uint v17 = 1132396544u;
    uint v18 = 255u;
    uint v19 = pos.x;
    uint v20 = as_type<uint>((float)(int)v19);
    uint v21 = pos.y;
    uint v22 = as_type<uint>((float)(int)v21);
    uint v23 = as_type<uint>(as_type<float>(v22) * as_type<float>(v2));
    uint v24 = as_type<uint>(fma(as_type<float>(v20), as_type<float>(v1), as_type<float>(v23)));
    uint v25 = as_type<uint>(as_type<float>(v3) + as_type<float>(v24));
    uint v26 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v25)));
    uint v27 = as_type<uint>(min(as_type<float>(v26), as_type<float>(v4)));
    uint v28 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v16), as_type<float>(v8)));
    uint v29 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v13), as_type<float>(v5)));
    uint v30 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v14), as_type<float>(v6)));
    uint v31 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v15), as_type<float>(v7)));
    uint v32 = as_type<uint>(as_type<float>(v28) * as_type<float>(v17));
    uint v33 = as_type<uint>((int)rint(as_type<float>(v32)));
    uint v34 = as_type<uint>(as_type<float>(v29) * as_type<float>(v17));
    uint v35 = as_type<uint>((int)rint(as_type<float>(v34)));
    uint v36 = as_type<uint>(as_type<float>(v30) * as_type<float>(v17));
    uint v37 = as_type<uint>((int)rint(as_type<float>(v36)));
    uint v38 = as_type<uint>(as_type<float>(v31) * as_type<float>(v17));
    uint v39 = as_type<uint>((int)rint(as_type<float>(v38)));
    uint v40 = v39 & v18;
    uint v41 = v35 & v18;
    uint v42 = v37 & v18;
    uint v43 = v42 << 8u;
    uint v44 = v41 | v43;
    uint v45 = v40 << 16u;
    uint v46 = v44 | v45;
    uint v47 = v33 << 24u;
    uint v48 = v46 | v47;
    ((device uint*)p1)[i] = v48;
}
