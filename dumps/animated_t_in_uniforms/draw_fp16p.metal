#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, limit0, limit1, stride0, stride1; };

kernel void umbra_entry(
    constant meta &m [[buffer(2)]],
    device uint *p0 [[buffer(0)]],
    device ushort *p1 [[buffer(1)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= m.w) return;
    uint x = m.x0 + pos.x;
    uint y = m.y0 + pos.y;
    uint v0 = 0u;
    uint v1 = p0[0];
    uint v2 = p0[1];
    uint v3 = p0[2];
    uint v4 = p0[3];
    uint v5 = (uint)as_type<ushort>((half)as_type<float>(v1));
    uint v6 = (uint)as_type<ushort>((half)as_type<float>(v2));
    uint v7 = (uint)as_type<ushort>((half)as_type<float>(v3));
    uint v8 = (uint)as_type<ushort>((half)as_type<float>(v4));
    { uint _row = y * m.stride1; uint _ps = m.limit1 / 4;
      p1[_row + x] = ushort(v5); p1[_row + x + _ps] = ushort(v6); p1[_row + x + 2*_ps] = ushort(v7); p1[_row + x + 3*_ps] = ushort(v8); }
}
