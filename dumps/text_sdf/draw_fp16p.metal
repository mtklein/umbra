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
    uint v7 = 1054867456u;
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
    float v24 = v23 - as_type<float>(1054867456u);
    float v26 = v24 * as_type<float>(1090519040u);
    #define v27 v26
    float v29 = max(v27, as_type<float>(0u));
    #define v30 v29
    float v32 = min(v30, as_type<float>(1065353216u));
    #define v33 v32
    float v34 = fma(v33, v14, v12);
    uint v35 = (uint)as_type<ushort>((half)v34);
    float v36 = fma(v33, v17, v15);
    uint v37 = (uint)as_type<ushort>((half)v36);
    float v38 = (float)as_type<half>((ushort)v11_1);
    float v39 = fma(v38, v10, as_type<float>(v2));
    float v40 = v39 - v38;
    float v41 = fma(v33, v40, v38);
    uint v42 = (uint)as_type<ushort>((half)v41);
    float v43 = (float)as_type<half>((ushort)v11_2);
    float v44 = fma(v43, v10, as_type<float>(v3));
    float v45 = v44 - v43;
    float v46 = fma(v33, v45, v43);
    uint v47 = (uint)as_type<ushort>((half)v46);
    { uint _row = y * m.stride1; uint _ps = m.count1 / 4;
      p1[_row + x] = ushort(v35); p1[_row + x + _ps] = ushort(v42); p1[_row + x + 2*_ps] = ushort(v47); p1[_row + x + 3*_ps] = ushort(v37); }
}
