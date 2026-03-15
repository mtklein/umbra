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
    half v1 = as_type<half>((ushort)7172u);
    half v2 = as_type<half>((ushort)15360u);
    ushort v3 = (ushort)((device uchar*)p0)[i*4+0];
    half v4 = (half)(short)v3;
    half v5 = as_type<half>(((device ushort*)p1)[i]);
    ushort v6 = (ushort)((device uchar*)p0)[i*4+3];
    half v7 = (half)(short)v6;
    half v8 = v2 - v1 * v7;
    half v9 = v5 * v8;
    half v10 = fma(v1, v4, v9);
    ((device ushort*)p1)[i] = as_type<ushort>(v10);

    ushort v12 = (ushort)((device uchar*)p0)[i*4+1];
    half v13 = (half)(short)v12;
    half v14 = as_type<half>(((device ushort*)p2)[i]);
    half v15 = v14 * v8;
    half v16 = fma(v1, v13, v15);
    ((device ushort*)p2)[i] = as_type<ushort>(v16);

    ushort v18 = (ushort)((device uchar*)p0)[i*4+2];
    half v19 = (half)(short)v18;
    half v20 = as_type<half>(((device ushort*)p3)[i]);
    half v21 = v20 * v8;
    half v22 = fma(v1, v19, v21);
    ((device ushort*)p3)[i] = as_type<ushort>(v22);

    half v24 = as_type<half>(((device ushort*)p4)[i]);
    half v25 = v24 * v8;
    half v26 = fma(v1, v7, v25);
    ((device ushort*)p4)[i] = as_type<ushort>(v26);
}
