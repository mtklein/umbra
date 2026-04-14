#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, stride0, stride1; };

kernel void umbra_entry(
    constant meta &m [[buffer(0)]],
    device const uint * __restrict p0 [[buffer(1)]],
    device ushort * __restrict p1 [[buffer(2)]],
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
    float v12 = (float)as_type<half>((ushort)v10_3);
    float v13 = as_type<float>(v9) - v12;
    float v14 = fma(as_type<float>(v1), v13, v11);
    float v15 = v14 - v11;
    float v16 = fma(as_type<float>(v4), v13, v12);
    float v17 = v16 - v12;
    uint v18 = m.x0 + pos.x;
    float v19 = (float)(int)v18;
    uint v20 = as_type<float>(v5) <= v19 ? 0xffffffffu : 0u;
    uint v21 = v19 <  as_type<float>(v7) ? 0xffffffffu : 0u;
    uint v22 = v20 & v21;
    uint v23 = m.y0 + pos.y;
    float v24 = (float)(int)v23;
    uint v25 = as_type<float>(v6) <= v24 ? 0xffffffffu : 0u;
    uint v26 = v24 <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v27 = v25 & v26;
    uint v28 = v22 & v27;
    uint v29 = (v28 != 0u) ? v9 : v0;
    float v30 = fma(as_type<float>(v29), v15, v11);
    uint v31 = (uint)as_type<ushort>((half)v30);
    float v32 = fma(as_type<float>(v29), v17, v12);
    uint v33 = (uint)as_type<ushort>((half)v32);
    float v34 = (float)as_type<half>((ushort)v10_1);
    float v35 = fma(as_type<float>(v2), v13, v34);
    float v36 = v35 - v34;
    float v37 = fma(as_type<float>(v29), v36, v34);
    uint v38 = (uint)as_type<ushort>((half)v37);
    float v39 = (float)as_type<half>((ushort)v10_2);
    float v40 = fma(as_type<float>(v3), v13, v39);
    float v41 = v40 - v39;
    float v42 = fma(as_type<float>(v29), v41, v39);
    uint v43 = (uint)as_type<ushort>((half)v42);
    { uint _row = y * m.stride1; uint _ps = m.count1 / 4;
      p1[_row + x] = ushort(v31); p1[_row + x + _ps] = ushort(v38); p1[_row + x + 2*_ps] = ushort(v43); p1[_row + x + 3*_ps] = ushort(v33); }
}
