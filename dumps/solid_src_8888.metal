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
    half v1 = as_type<half>(((device const ushort*)p1)[4]);
    half v2 = as_type<half>(((device const ushort*)p1)[5]);
    half v3 = as_type<half>(((device const ushort*)p1)[6]);
    half v4 = as_type<half>(((device const ushort*)p1)[7]);
    half v5 = as_type<half>((ushort)23544u);
    half v6 = as_type<half>((ushort)14336u);
    half v7 = fma(v1, v5, v6);
    ushort v8 = (ushort)(short)v7;
    half v9 = fma(v2, v5, v6);
    ushort v10 = (ushort)(short)v9;
    half v11 = fma(v3, v5, v6);
    ushort v12 = (ushort)(short)v11;
    half v13 = fma(v4, v5, v6);
    ushort v14 = (ushort)(short)v13;
    ((device uchar*)p0)[i*4+0] = (uchar)v8;
    ((device uchar*)p0)[i*4+1] = (uchar)v10;
    ((device uchar*)p0)[i*4+2] = (uchar)v12;
    ((device uchar*)p0)[i*4+3] = (uchar)v14;
}
