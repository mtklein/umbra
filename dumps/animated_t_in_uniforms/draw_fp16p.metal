#include <metal_stdlib>
using namespace metal;


kernel void umbra_entry(
    constant uint &w [[buffer(2)]],
    constant uint *buf_limit [[buffer(3)]],
    constant uint *buf_rbs [[buffer(4)]],
    constant uint &x0 [[buffer(5)]],
    constant uint &y0 [[buffer(6)]],
    device uchar *p0 [[buffer(0)]],
    device uchar *p1 [[buffer(1)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= w) return;
    uint x = x0 + pos.x;
    uint y = y0 + pos.y;
    uint v0 = 0u;
    uint v1 = ((device const uint*)p0)[0];
    uint v2 = ((device const uint*)p0)[1];
    uint v3 = ((device const uint*)p0)[2];
    uint v4 = ((device const uint*)p0)[3];
    uint v5 = (uint)as_type<ushort>((half)as_type<float>(v1));
    uint v6 = (uint)as_type<ushort>((half)as_type<float>(v2));
    uint v7 = (uint)as_type<ushort>((half)as_type<float>(v3));
    uint v8 = (uint)as_type<ushort>((half)as_type<float>(v4));
    {
        device uchar *row = p1 + y * buf_rbs[1]; uint ps = buf_limit[1];
        ((device ushort*)row)[x] = ushort(v5); ((device ushort*)(row+ps))[x] = ushort(v6); ((device ushort*)(row+2*ps))[x] = ushort(v7); ((device ushort*)(row+3*ps))[x] = ushort(v8);
    }
}
