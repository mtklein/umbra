#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, stride0, stride1; };

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
    uint v5 = p0[4];
    uint v6 = p0[5];
    uint v7 = p0[6];
    uint v8 = p0[7];
    uint v9 = 1065353216u;
    uint _row10 = y * m.stride1; uint _ps10 = m.count1 / 4;
    uint v10 = (uint)p1[_row10 + x];
    uint v10_1 = (uint)p1[_row10 + x + _ps10];
    uint v10_2 = (uint)p1[_row10 + x + 2*_ps10];
    uint v10_3 = (uint)p1[_row10 + x + 3*_ps10];
    float v11 = (float)as_type<half>((ushort)v10);
    float v12 = as_type<float>(v1) - v11;
    float v13 = (float)as_type<half>((ushort)v10_3);
    float v14 = as_type<float>(v4) - v13;
    uint v15 = m.x0 + pos.x;
    float v16 = (float)(int)v15;
    uint v17 = as_type<float>(v5) <= v16 ? 0xffffffffu : 0u;
    uint v18 = v16 <  as_type<float>(v7) ? 0xffffffffu : 0u;
    uint v19 = v17 & v18;
    uint v20 = m.y0 + pos.y;
    float v21 = (float)(int)v20;
    uint v22 = as_type<float>(v6) <= v21 ? 0xffffffffu : 0u;
    uint v23 = v21 <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v24 = v22 & v23;
    uint v25 = v19 & v24;
    uint v26 = select(v0, v9, v25 != 0u);
    float v27 = fma(as_type<float>(v26), v12, v11);
    uint v28 = (uint)as_type<ushort>((half)v27);
    float v29 = fma(as_type<float>(v26), v14, v13);
    uint v30 = (uint)as_type<ushort>((half)v29);
    float v31 = (float)as_type<half>((ushort)v10_1);
    float v32 = as_type<float>(v2) - v31;
    float v33 = fma(as_type<float>(v26), v32, v31);
    uint v34 = (uint)as_type<ushort>((half)v33);
    float v35 = (float)as_type<half>((ushort)v10_2);
    float v36 = as_type<float>(v3) - v35;
    float v37 = fma(as_type<float>(v26), v36, v35);
    uint v38 = (uint)as_type<ushort>((half)v37);
    { uint _row = y * m.stride1; uint _ps = m.count1 / 4;
      p1[_row + x] = ushort(v28); p1[_row + x + _ps] = ushort(v34); p1[_row + x + 2*_ps] = ushort(v38); p1[_row + x + 3*_ps] = ushort(v30); }
}
