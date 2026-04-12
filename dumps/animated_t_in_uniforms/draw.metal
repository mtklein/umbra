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
    uint v5 = (uint)as_type<ushort>((half)as_type<float>(v1));
    uint v6 = (uint)as_type<ushort>((half)as_type<float>(v2));
    uint v7 = (uint)as_type<ushort>((half)as_type<float>(v3));
    uint v8 = (uint)as_type<ushort>((half)as_type<float>(v4));
    { uint _base = y * buf_stride[1] + x*4;
      p1[_base] = ushort(v5); p1[_base+1] = ushort(v6); p1[_base+2] = ushort(v7); p1[_base+3] = ushort(v8); }
}
