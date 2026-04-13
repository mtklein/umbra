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
    uint v5 = p0[4];
    uint v6 = p0[5];
    uint v7 = p0[6];
    uint v8 = p0[7];
    uint v9 = 1065353216u;
    float v10 = as_type<float>(v9) - as_type<float>(v4);
    half4 _px11 = p1[y * m.stride1 + x];
    uint v11 = (uint)as_type<ushort>(_px11.x);
    uint v11_1 = (uint)as_type<ushort>(_px11.y);
    uint v11_2 = (uint)as_type<ushort>(_px11.z);
    uint v11_3 = (uint)as_type<ushort>(_px11.w);
    float v12 = (float)as_type<half>((ushort)v11);
    float v13 = fma(v12, v10, as_type<float>(v1));
    float v14 = v13 - v12;
    float v15 = (float)as_type<half>((ushort)v11_3);
    float v16 = fma(v15, v10, as_type<float>(v4));
    float v17 = v16 - v15;
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
    float v30 = fma(as_type<float>(v29), v14, v12);
    uint v31 = (uint)as_type<ushort>((half)v30);
    float v32 = fma(as_type<float>(v29), v17, v15);
    uint v33 = (uint)as_type<ushort>((half)v32);
    float v34 = (float)as_type<half>((ushort)v11_1);
    float v35 = fma(v34, v10, as_type<float>(v2));
    float v36 = v35 - v34;
    float v37 = fma(as_type<float>(v29), v36, v34);
    uint v38 = (uint)as_type<ushort>((half)v37);
    float v39 = (float)as_type<half>((ushort)v11_2);
    float v40 = fma(v39, v10, as_type<float>(v3));
    float v41 = v40 - v39;
    float v42 = fma(as_type<float>(v29), v41, v39);
    uint v43 = (uint)as_type<ushort>((half)v42);
    p1[y * m.stride1 + x] = half4(as_type<half>((ushort)v31), as_type<half>((ushort)v38), as_type<half>((ushort)v43), as_type<half>((ushort)v33));
}
