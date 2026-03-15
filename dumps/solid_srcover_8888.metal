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
    half v5 = as_type<half>((ushort)7172u);
    half v6 = as_type<half>((ushort)15360u);
    half v7 = v6 - v4;
    half v8 = as_type<half>((ushort)23544u);
    half v9 = as_type<half>((ushort)14336u);
    ushort v10 = (ushort)((device uchar*)p0)[i*4+0];
    half v11 = (half)(short)v10;
    half v12 = v5 * v11;
    half v13 = fma(v12, v7, v1);
    half v14 = fma(v13, v8, v9);
    ushort v15 = (ushort)(short)v14;
    ushort v16 = (ushort)((device uchar*)p0)[i*4+1];
    half v17 = (half)(short)v16;
    half v18 = v5 * v17;
    half v19 = fma(v18, v7, v2);
    half v20 = fma(v19, v8, v9);
    ushort v21 = (ushort)(short)v20;
    ushort v22 = (ushort)((device uchar*)p0)[i*4+2];
    half v23 = (half)(short)v22;
    half v24 = v5 * v23;
    half v25 = fma(v24, v7, v3);
    half v26 = fma(v25, v8, v9);
    ushort v27 = (ushort)(short)v26;
    ushort v28 = (ushort)((device uchar*)p0)[i*4+3];
    half v29 = (half)(short)v28;
    half v30 = v5 * v29;
    half v31 = fma(v30, v7, v4);
    half v32 = fma(v31, v8, v9);
    ushort v33 = (ushort)(short)v32;
    ((device uchar*)p0)[i*4+0] = (uchar)v15;
    ((device uchar*)p0)[i*4+1] = (uchar)v21;
    ((device uchar*)p0)[i*4+2] = (uchar)v27;
    ((device uchar*)p0)[i*4+3] = (uchar)v33;
}
