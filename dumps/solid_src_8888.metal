#include <metal_stdlib>
using namespace metal;

kernel void umbra_entry(
    constant uint &n [[buffer(0)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    device uchar *p2 [[buffer(3)]],
    device uchar *p3 [[buffer(4)]],
    uint i [[thread_position_in_grid]]
) {
    if (i >= n) return;
    uint v0 = 0u;
    half v1 = as_type<half>(((device const ushort*)p3)[0]);
    uint v2 = 1u;
    half v3 = as_type<half>(((device const ushort*)p3)[1]);
    uint v4 = 2u;
    half v5 = as_type<half>(((device const ushort*)p3)[2]);
    uint v6 = 3u;
    half v7 = as_type<half>(((device const ushort*)p3)[3]);
    half v8 = as_type<half>((ushort)23544u);
    half v9 = as_type<half>((ushort)14336u);
    half v10 = fma(v1, v8, v9);
    ushort v11 = (ushort)(short)v10;
    half v12 = fma(v3, v8, v9);
    ushort v13 = (ushort)(short)v12;
    half v14 = fma(v5, v8, v9);
    ushort v15 = (ushort)(short)v14;
    half v16 = fma(v7, v8, v9);
    ushort v17 = (ushort)(short)v16;
    ((device uchar*)p0)[i*4+0] = (uchar)v11;
    ((device uchar*)p0)[i*4+1] = (uchar)v13;
    ((device uchar*)p0)[i*4+2] = (uchar)v15;
    ((device uchar*)p0)[i*4+3] = (uchar)v17;
}
