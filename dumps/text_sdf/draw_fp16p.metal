#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, count2, stride0, stride1, stride2; };

kernel void umbra_entry(
    constant meta &m [[buffer(0)]],
    device const uint * __restrict p0 [[buffer(1)]],
    device ushort * __restrict p1 [[buffer(2)]],
    device const ushort * __restrict p2 [[buffer(3)]],
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
    float v15 = (float)as_type<half>((ushort)v11_3);
    float v16 = fma(v15, v10, as_type<float>(v4));
    float v17 = v16 - v15;
    uint v18 = (uint)p2[y * m.stride2 + x];
    uint v19 = (uint)(int)(short)(ushort)v18;
    float v20 = (float)(int)v19;
    float v22 = v20 * as_type<float>(998277249u);
    #define v23 v22
    float v25 = v23 - as_type<float>(1054867456u);
    #define v26 v25
    float v28 = v26 * as_type<float>(1090519040u);
    #define v29 v28
    float v31 = max(v29, as_type<float>(0u));
    #define v32 v31
    float v34 = min(v32, as_type<float>(1065353216u));
    #define v35 v34
    float v36 = fma(v35, v14, v12);
    uint v37 = (uint)as_type<ushort>((half)v36);
    float v38 = fma(v35, v17, v15);
    uint v39 = (uint)as_type<ushort>((half)v38);
    float v40 = (float)as_type<half>((ushort)v11_1);
    float v41 = fma(v40, v10, as_type<float>(v2));
    float v42 = v41 - v40;
    float v43 = fma(v35, v42, v40);
    uint v44 = (uint)as_type<ushort>((half)v43);
    float v45 = (float)as_type<half>((ushort)v11_2);
    float v46 = fma(v45, v10, as_type<float>(v3));
    float v47 = v46 - v45;
    float v48 = fma(v35, v47, v45);
    uint v49 = (uint)as_type<ushort>((half)v48);
    { uint _row = y * m.stride1; uint _ps = m.count1 / 4;
      p1[_row + x] = ushort(v37); p1[_row + x + _ps] = ushort(v44); p1[_row + x + 2*_ps] = ushort(v49); p1[_row + x + 3*_ps] = ushort(v39); }
}
