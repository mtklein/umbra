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
    uint v5 = 1065353216u;
    float v6 = as_type<float>(v5) - as_type<float>(v4);
    half4 _px7 = p1[y * m.stride1 + x];
    uint v7 = (uint)as_type<ushort>(_px7.x);
    uint v7_1 = (uint)as_type<ushort>(_px7.y);
    uint v7_2 = (uint)as_type<ushort>(_px7.z);
    uint v7_3 = (uint)as_type<ushort>(_px7.w);
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
    p1[y * m.stride1 + x] = half4(as_type<half>((ushort)v10), as_type<half>((ushort)v16), as_type<half>((ushort)v19), as_type<half>((ushort)v13));
}
