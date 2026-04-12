#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, limit0, limit1, limit2, stride0, stride1, stride2; };

kernel void umbra_entry(
    constant meta &m [[buffer(3)]],
    device uint *p0 [[buffer(0)]],
    device ushort *p1 [[buffer(1)]],
    device uint *p2 [[buffer(2)]],
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
    uint v8 = p2[y * m.stride2 + x];
    float v9 = fabs(as_type<float>(v8));
    float v10 = min(v9, as_type<float>(1065353216u));
    uint _row11 = y * m.stride1; uint _ps11 = m.limit1;
    uint v11 = (uint)p1[_row11 + x];
    uint v11_1 = (uint)p1[_row11 + x + _ps11];
    uint v11_2 = (uint)p1[_row11 + x + 2*_ps11];
    uint v11_3 = (uint)p1[_row11 + x + 3*_ps11];
    float v12 = (float)as_type<half>((ushort)v11);
    float v13 = fma(v12, v7, as_type<float>(v1));
    float v14 = v13 - v12;
    float v15 = fma(v10, v14, v12);
    uint v16 = (uint)as_type<ushort>((half)v15);
    float v17 = (float)as_type<half>((ushort)v11_3);
    float v18 = fma(v17, v7, as_type<float>(v4));
    float v19 = v18 - v17;
    float v20 = fma(v10, v19, v17);
    uint v21 = (uint)as_type<ushort>((half)v20);
    float v22 = (float)as_type<half>((ushort)v11_1);
    float v23 = fma(v22, v7, as_type<float>(v2));
    float v24 = v23 - v22;
    float v25 = fma(v10, v24, v22);
    uint v26 = (uint)as_type<ushort>((half)v25);
    float v27 = (float)as_type<half>((ushort)v11_2);
    float v28 = fma(v27, v7, as_type<float>(v3));
    float v29 = v28 - v27;
    float v30 = fma(v10, v29, v27);
    uint v31 = (uint)as_type<ushort>((half)v30);
    { uint _row = y * m.stride1; uint _ps = m.limit1;
      p1[_row + x] = ushort(v16); p1[_row + x + _ps] = ushort(v26); p1[_row + x + 2*_ps] = ushort(v31); p1[_row + x + 3*_ps] = ushort(v21); }
}
