#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, stride0, stride1; };

kernel void umbra_entry(
    constant meta &m [[buffer(2)]],
    device uint *p0 [[buffer(0)]],
    device half4 *p1 [[buffer(1)]],
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
    uint v10 = m.x0 + pos.x;
    float v11 = (float)(int)v10;
    uint v12 = as_type<float>(v5) <= v11 ? 0xffffffffu : 0u;
    uint v13 = v11 <  as_type<float>(v7) ? 0xffffffffu : 0u;
    uint v14 = v12 & v13;
    uint v15 = m.y0 + pos.y;
    float v16 = (float)(int)v15;
    uint v17 = as_type<float>(v6) <= v16 ? 0xffffffffu : 0u;
    uint v18 = v16 <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v19 = v17 & v18;
    uint v20 = v14 & v19;
    uint v21 = select(v0, v9, v20 != 0u);
    half4 _px22 = p1[y * m.stride1 + x];
    uint v22 = (uint)as_type<ushort>(_px22.x);
    uint v22_1 = (uint)as_type<ushort>(_px22.y);
    uint v22_2 = (uint)as_type<ushort>(_px22.z);
    uint v22_3 = (uint)as_type<ushort>(_px22.w);
    float v23 = (float)as_type<half>((ushort)v22);
    float v24 = as_type<float>(v1) - v23;
    float v25 = fma(as_type<float>(v21), v24, v23);
    uint v26 = (uint)as_type<ushort>((half)v25);
    float v27 = (float)as_type<half>((ushort)v22_3);
    float v28 = as_type<float>(v4) - v27;
    float v29 = fma(as_type<float>(v21), v28, v27);
    uint v30 = (uint)as_type<ushort>((half)v29);
    float v31 = (float)as_type<half>((ushort)v22_1);
    float v32 = as_type<float>(v2) - v31;
    float v33 = fma(as_type<float>(v21), v32, v31);
    uint v34 = (uint)as_type<ushort>((half)v33);
    float v35 = (float)as_type<half>((ushort)v22_2);
    float v36 = as_type<float>(v3) - v35;
    float v37 = fma(as_type<float>(v21), v36, v35);
    uint v38 = (uint)as_type<ushort>((half)v37);
    p1[y * m.stride1 + x] = half4(as_type<half>((ushort)v26), as_type<half>((ushort)v34), as_type<half>((ushort)v38), as_type<half>((ushort)v30));
}
