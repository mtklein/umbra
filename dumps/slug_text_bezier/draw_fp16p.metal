#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, count2, stride0, stride1, stride2; };

kernel void umbra_entry(
    constant meta &m [[buffer(3)]],
    device const uint * __restrict p0 [[buffer(0)]],
    device ushort * __restrict p1 [[buffer(1)]],
    device const uint * __restrict p2 [[buffer(2)]],
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
    uint v6 = 1065353216u;
    float v7 = as_type<float>(v6) - as_type<float>(v4);
    uint _row8 = y * m.stride1; uint _ps8 = m.count1 / 4;
    uint v8 = (uint)p1[_row8 + x];
    uint v8_1 = (uint)p1[_row8 + x + _ps8];
    uint v8_2 = (uint)p1[_row8 + x + 2*_ps8];
    uint v8_3 = (uint)p1[_row8 + x + 3*_ps8];
    float v9 = (float)as_type<half>((ushort)v8);
    float v10 = fma(v9, v7, as_type<float>(v1));
    float v11 = v10 - v9;
    float v12 = (float)as_type<half>((ushort)v8_3);
    float v13 = fma(v12, v7, as_type<float>(v4));
    float v14 = v13 - v12;
    uint v15 = p2[y * m.stride2 + x];
    float v16 = fabs(as_type<float>(v15));
    float v17 = min(v16, as_type<float>(1065353216u));
    float v18 = fma(v17, v11, v9);
    uint v19 = (uint)as_type<ushort>((half)v18);
    float v20 = fma(v17, v14, v12);
    uint v21 = (uint)as_type<ushort>((half)v20);
    float v22 = (float)as_type<half>((ushort)v8_1);
    float v23 = fma(v22, v7, as_type<float>(v2));
    float v24 = v23 - v22;
    float v25 = fma(v17, v24, v22);
    uint v26 = (uint)as_type<ushort>((half)v25);
    float v27 = (float)as_type<half>((ushort)v8_2);
    float v28 = fma(v27, v7, as_type<float>(v3));
    float v29 = v28 - v27;
    float v30 = fma(v17, v29, v27);
    uint v31 = (uint)as_type<ushort>((half)v30);
    { uint _row = y * m.stride1; uint _ps = m.count1 / 4;
      p1[_row + x] = ushort(v19); p1[_row + x + _ps] = ushort(v26); p1[_row + x + 2*_ps] = ushort(v31); p1[_row + x + 3*_ps] = ushort(v21); }
}
