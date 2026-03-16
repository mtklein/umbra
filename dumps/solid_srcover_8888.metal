#include <metal_stdlib>
using namespace metal;

static inline int clamp_ix(int ix, uint bytes, int elem) {
    int hi = (int)(bytes / (uint)elem) - 1;
    if (hi < 0) hi = 0;
    return clamp(ix, 0, hi);
}

kernel void umbra_entry(
    constant uint &n [[buffer(0)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    constant uint *buf_szs [[buffer(3)]],
    uint i [[thread_position_in_grid]]
) {
    if (i >= n) return;
    uint v0 = 0u;
    uint v1 = ((device const uint*)p1)[2];
    uint v2 = ((device const uint*)p1)[3];
    uint v3 = ((device const uint*)p1)[4];
    uint v4 = ((device const uint*)p1)[5];
    uint v5 = 998277249u;
    uint v6 = 1065353216u;
    uint v7 = as_type<uint>(as_type<float>(v6) - as_type<float>(v4));
    uint v8 = 1132396544u;
    uint v9 = 1056964608u;
    uint v10 = (uint)((device uchar*)p0)[i*4+0];
    uint v11 = as_type<uint>((float)(int)v10);
    uint v12 = as_type<uint>(as_type<float>(v5) * as_type<float>(v11));
    uint v13 = as_type<uint>(fma(as_type<float>(v12), as_type<float>(v7), as_type<float>(v1)));
    uint v14 = as_type<uint>(fma(as_type<float>(v13), as_type<float>(v8), as_type<float>(v9)));
    uint v15 = (uint)(int)as_type<float>(v14);
    uint v16 = (uint)((device uchar*)p0)[i*4+1];
    uint v17 = as_type<uint>((float)(int)v16);
    uint v18 = as_type<uint>(as_type<float>(v5) * as_type<float>(v17));
    uint v19 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v7), as_type<float>(v2)));
    uint v20 = as_type<uint>(fma(as_type<float>(v19), as_type<float>(v8), as_type<float>(v9)));
    uint v21 = (uint)(int)as_type<float>(v20);
    uint v22 = (uint)((device uchar*)p0)[i*4+2];
    uint v23 = as_type<uint>((float)(int)v22);
    uint v24 = as_type<uint>(as_type<float>(v5) * as_type<float>(v23));
    uint v25 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v7), as_type<float>(v3)));
    uint v26 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v8), as_type<float>(v9)));
    uint v27 = (uint)(int)as_type<float>(v26);
    uint v28 = (uint)((device uchar*)p0)[i*4+3];
    uint v29 = as_type<uint>((float)(int)v28);
    uint v30 = as_type<uint>(as_type<float>(v5) * as_type<float>(v29));
    uint v31 = as_type<uint>(fma(as_type<float>(v30), as_type<float>(v7), as_type<float>(v4)));
    uint v32 = as_type<uint>(fma(as_type<float>(v31), as_type<float>(v8), as_type<float>(v9)));
    uint v33 = (uint)(int)as_type<float>(v32);
    ((device uchar*)p0)[i*4+0] = (uchar)v15;
    ((device uchar*)p0)[i*4+1] = (uchar)v21;
    ((device uchar*)p0)[i*4+2] = (uchar)v27;
    ((device uchar*)p0)[i*4+3] = (uchar)v33;
}
