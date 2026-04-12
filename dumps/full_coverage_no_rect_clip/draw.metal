#include <metal_stdlib>
using namespace metal;


kernel void umbra_entry(
    constant uint &w [[buffer(2)]],
    constant uint *buf_limit [[buffer(3)]],
    constant uint *buf_stride [[buffer(4)]],
    constant uint &x0 [[buffer(5)]],
    constant uint &y0 [[buffer(6)]],
    device uint *p0 [[buffer(0)]],
    device ushort *p1 [[buffer(1)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= w) return;
    uint x = x0 + pos.x;
    uint y = y0 + pos.y;
    uint v0 = 0u;
    uint v1 = p0[0];
    uint v2 = p0[1];
    uint v3 = p0[2];
    uint v4 = p0[3];
    uint v5 = 1065353216u;
    float v6 = as_type<float>(v5) - as_type<float>(v4);
    uint _base7 = y * buf_stride[1] + x*4;
    uint v7 = (uint)p1[_base7];
    uint v7_1 = (uint)p1[_base7+1];
    uint v7_2 = (uint)p1[_base7+2];
    uint v7_3 = (uint)p1[_base7+3];
    float v8 = (float)as_type<half>((ushort)v7);
    float v9 = fma(v8, v6, as_type<float>(v1));
    uint v10 = (uint)as_type<ushort>((half)v9);
    float v11 = (float)as_type<half>((ushort)v7_3);
    float v12 = fma(v11, v6, as_type<float>(v4));
    uint v13 = (uint)as_type<ushort>((half)v12);
    float v14 = (float)as_type<half>((ushort)v7_1);
    float v15 = fma(v14, v6, as_type<float>(v2));
    uint v16 = (uint)as_type<ushort>((half)v15);
    float v17 = (float)as_type<half>((ushort)v7_2);
    float v18 = fma(v17, v6, as_type<float>(v3));
    uint v19 = (uint)as_type<ushort>((half)v18);
    { uint _base = y * buf_stride[1] + x*4;
      p1[_base] = ushort(v10); p1[_base+1] = ushort(v16); p1[_base+2] = ushort(v19); p1[_base+3] = ushort(v13); }
}
