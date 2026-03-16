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
    device uchar *p2 [[buffer(3)]],
    device uchar *p3 [[buffer(4)]],
    device uchar *p4 [[buffer(5)]],
    constant uint *buf_szs [[buffer(6)]],
    uint i [[thread_position_in_grid]]
) {
    if (i >= n) return;
    uint v0 = 0u;
    uint v1 = 998277249u;
    uint v2 = 1065353216u;
    uint v3 = (uint)((device uchar*)p0)[i*4+0];
    uint v4 = as_type<uint>((float)(int)v3);
    uint v5 = as_type<uint>((float)as_type<half>(((device ushort*)p1)[i]));
    uint v6 = (uint)((device uchar*)p0)[i*4+3];
    uint v7 = as_type<uint>((float)(int)v6);
    uint v8 = as_type<uint>(as_type<float>(v2) - as_type<float>(v1) * as_type<float>(v7));
    uint v9 = as_type<uint>(as_type<float>(v5) * as_type<float>(v8));
    uint v10 = as_type<uint>(fma(as_type<float>(v1), as_type<float>(v4), as_type<float>(v9)));
    ((device ushort*)p1)[i] = as_type<ushort>((half)as_type<float>(v10));

    uint v12 = (uint)((device uchar*)p0)[i*4+1];
    uint v13 = as_type<uint>((float)(int)v12);
    uint v14 = as_type<uint>((float)as_type<half>(((device ushort*)p2)[i]));
    uint v15 = as_type<uint>(as_type<float>(v14) * as_type<float>(v8));
    uint v16 = as_type<uint>(fma(as_type<float>(v1), as_type<float>(v13), as_type<float>(v15)));
    ((device ushort*)p2)[i] = as_type<ushort>((half)as_type<float>(v16));

    uint v18 = (uint)((device uchar*)p0)[i*4+2];
    uint v19 = as_type<uint>((float)(int)v18);
    uint v20 = as_type<uint>((float)as_type<half>(((device ushort*)p3)[i]));
    uint v21 = as_type<uint>(as_type<float>(v20) * as_type<float>(v8));
    uint v22 = as_type<uint>(fma(as_type<float>(v1), as_type<float>(v19), as_type<float>(v21)));
    ((device ushort*)p3)[i] = as_type<ushort>((half)as_type<float>(v22));

    uint v24 = as_type<uint>((float)as_type<half>(((device ushort*)p4)[i]));
    uint v25 = as_type<uint>(as_type<float>(v24) * as_type<float>(v8));
    uint v26 = as_type<uint>(fma(as_type<float>(v1), as_type<float>(v7), as_type<float>(v25)));
    ((device ushort*)p4)[i] = as_type<ushort>((half)as_type<float>(v26));
}
