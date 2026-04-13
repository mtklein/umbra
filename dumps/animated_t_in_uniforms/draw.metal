#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, stride0, stride1; };

kernel void umbra_entry(
    constant meta &m [[buffer(2)]],
    device const uint * __restrict p0 [[buffer(0)]],
    device half4 * __restrict p1 [[buffer(1)]],
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
    p1[y * m.stride1 + x] = half4(as_type<half>((ushort)v5), as_type<half>((ushort)v6), as_type<half>((ushort)v7), as_type<half>((ushort)v8));
}
