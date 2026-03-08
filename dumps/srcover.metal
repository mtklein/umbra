#include <metal_stdlib>
using namespace metal;

static inline uint bytes(uint x, int c) {
    uchar b[] = {0, uchar(x&0xff), uchar((x>>8)&0xff), uchar((x>>16)&0xff), uchar((x>>24)&0xff)};
    return uint(b[c&0xf]) | (uint(b[(c>>4)&0xf])<<8) | (uint(b[(c>>8)&0xf])<<16) | (uint(b[(c>>12)&0xf])<<24);
}

kernel void umbra_entry(
    constant uint &n [[buffer(0)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    device uchar *p2 [[buffer(3)]],
    device uchar *p3 [[buffer(4)]],
    device uchar *p4 [[buffer(5)]],
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
    half v8 = v1 * v7;
    half v9 = v2 - v8;
    half v10 = v5 * v9;
    half v11 = fma(v1, v4, v10);
    ((device ushort*)p1)[i] = as_type<ushort>(v11);

    ushort v13 = (ushort)((device uchar*)p0)[i*4+1];
    half v14 = (half)(short)v13;
    half v15 = as_type<half>(((device ushort*)p2)[i]);
    half v16 = v15 * v9;
    half v17 = fma(v1, v14, v16);
    ((device ushort*)p2)[i] = as_type<ushort>(v17);

    ushort v19 = (ushort)((device uchar*)p0)[i*4+2];
    half v20 = (half)(short)v19;
    half v21 = as_type<half>(((device ushort*)p3)[i]);
    half v22 = v21 * v9;
    half v23 = fma(v1, v20, v22);
    ((device ushort*)p3)[i] = as_type<ushort>(v23);

    half v25 = as_type<half>(((device ushort*)p4)[i]);
    half v26 = v25 * v9;
    half v27 = fma(v1, v7, v26);
    ((device ushort*)p4)[i] = as_type<ushort>(v27);
}
