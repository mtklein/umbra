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
    uint v5 = 1132396544u;
    uint v6 = 1056964608u;
    uint v7 = as_type<uint>(fma(as_type<float>(v1), as_type<float>(v5), as_type<float>(v6)));
    uint v8 = (uint)(int)as_type<float>(v7);
    uint v9 = as_type<uint>(fma(as_type<float>(v2), as_type<float>(v5), as_type<float>(v6)));
    uint v10 = (uint)(int)as_type<float>(v9);
    uint v11 = as_type<uint>(fma(as_type<float>(v3), as_type<float>(v5), as_type<float>(v6)));
    uint v12 = (uint)(int)as_type<float>(v11);
    uint v13 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v5), as_type<float>(v6)));
    uint v14 = (uint)(int)as_type<float>(v13);
    ((device uchar*)p0)[i*4+0] = (uchar)v8;
    ((device uchar*)p0)[i*4+1] = (uchar)v10;
    ((device uchar*)p0)[i*4+2] = (uchar)v12;
    ((device uchar*)p0)[i*4+3] = (uchar)v14;
}
