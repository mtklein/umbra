#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, count2, stride0, stride1, stride2; };

kernel void umbra_entry(
    constant meta &m [[buffer(3)]],
    device const uint * __restrict p0 [[buffer(0)]],
    device ushort * __restrict p1 [[buffer(1)]],
    device const ushort * __restrict p2 [[buffer(2)]],
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
    uint v6 = 998277249u;
    uint v7 = 1054867456u;
    uint v8 = 1090519040u;
    uint v9 = 1065353216u;
    float v10 = as_type<float>(v9) - as_type<float>(v4);
    uint _row11 = y * m.stride1; uint _ps11 = m.count1 / 4;
    uint v11 = (uint)p1[_row11 + x];
    uint v11_1 = (uint)p1[_row11 + x + _ps11];
    uint v11_2 = (uint)p1[_row11 + x + 2*_ps11];
    uint v11_3 = (uint)p1[_row11 + x + 3*_ps11];
    float v12 = (float)as_type<half>((ushort)v11);
    float v13 = fma(v12, v10, as_type<float>(v1));
    float v14 = v13 - v12;
    uint v15 = (uint)p2[y * m.stride2 + x];
    uint v16 = (uint)(int)(short)(ushort)v15;
    float v17 = (float)(int)v16;
    float v18 = v17 * as_type<float>(998277249u);
    float v19 = v18 - as_type<float>(1054867456u);
    float v20 = v19 * as_type<float>(1090519040u);
    float v21 = max(v20, as_type<float>(0u));
    float v22 = min(v21, as_type<float>(1065353216u));
    float v23 = fma(v22, v14, v12);
    uint v24 = (uint)as_type<ushort>((half)v23);
    float v25 = (float)as_type<half>((ushort)v11_3);
    float v26 = fma(v25, v10, as_type<float>(v4));
    float v27 = v26 - v25;
    float v28 = fma(v22, v27, v25);
    uint v29 = (uint)as_type<ushort>((half)v28);
    float v30 = (float)as_type<half>((ushort)v11_1);
    float v31 = fma(v30, v10, as_type<float>(v2));
    float v32 = v31 - v30;
    float v33 = fma(v22, v32, v30);
    uint v34 = (uint)as_type<ushort>((half)v33);
    float v35 = (float)as_type<half>((ushort)v11_2);
    float v36 = fma(v35, v10, as_type<float>(v3));
    float v37 = v36 - v35;
    float v38 = fma(v22, v37, v35);
    uint v39 = (uint)as_type<ushort>((half)v38);
    { uint _row = y * m.stride1; uint _ps = m.count1 / 4;
      p1[_row + x] = ushort(v24); p1[_row + x + _ps] = ushort(v34); p1[_row + x + 2*_ps] = ushort(v39); p1[_row + x + 3*_ps] = ushort(v29); }
}
