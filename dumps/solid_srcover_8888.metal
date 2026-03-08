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
    half v8 = as_type<half>((ushort)7172u);
    half v9 = as_type<half>((ushort)15360u);
    half v10 = v9 - v7;
    half v11 = as_type<half>((ushort)23544u);
    half v12 = as_type<half>((ushort)14336u);
    ushort v13 = (ushort)((device uchar*)p0)[i*4+0];
    half v14 = (half)(short)v13;
    half v15 = v8 * v14;
    half v16 = fma(v15, v10, v1);
    half v17 = fma(v16, v11, v12);
    ushort v18 = (ushort)(short)v17;
    ushort v19 = (ushort)((device uchar*)p0)[i*4+1];
    half v20 = (half)(short)v19;
    half v21 = v8 * v20;
    half v22 = fma(v21, v10, v3);
    half v23 = fma(v22, v11, v12);
    ushort v24 = (ushort)(short)v23;
    ushort v25 = (ushort)((device uchar*)p0)[i*4+2];
    half v26 = (half)(short)v25;
    half v27 = v8 * v26;
    half v28 = fma(v27, v10, v5);
    half v29 = fma(v28, v11, v12);
    ushort v30 = (ushort)(short)v29;
    ushort v31 = (ushort)((device uchar*)p0)[i*4+3];
    half v32 = (half)(short)v31;
    half v33 = v8 * v32;
    half v34 = fma(v33, v10, v7);
    half v35 = fma(v34, v11, v12);
    ushort v36 = (ushort)(short)v35;
    ((device uchar*)p0)[i*4+0] = (uchar)v18;
    ((device uchar*)p0)[i*4+1] = (uchar)v24;
    ((device uchar*)p0)[i*4+2] = (uchar)v30;
    ((device uchar*)p0)[i*4+3] = (uchar)v36;
}
