#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, stride0, stride1; };

kernel void main0(
    constant meta &m [[buffer(0)]],
    device const uint * __restrict p0 [[buffer(1)]],
    device half4 * __restrict p1 [[buffer(2)]],
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
    float v13 = v12 * v10;
    float v14 = (float)as_type<half>((ushort)v11_3);
    float v15 = as_type<float>(v9) - v14;
    float v16 = fma(as_type<float>(v1), v15, v13);
    float v17 = fma(as_type<float>(v1), v12, v16);
    float v18 = v17 - v12;
    float v19 = v14 * v10;
    float v20 = fma(as_type<float>(v4), v15, v19);
    float v21 = fma(as_type<float>(v4), v14, v20);
    float v22 = v21 - v14;
    uint v23 = m.x0 + pos.x;
    float v24 = (float)(int)v23;
    uint v25 = as_type<float>(v5) <= v24 ? 0xffffffffu : 0u;
    uint v26 = v24 <  as_type<float>(v7) ? 0xffffffffu : 0u;
    uint v27 = v25 & v26;
    uint v28 = m.y0 + pos.y;
    float v29 = (float)(int)v28;
    uint v30 = as_type<float>(v6) <= v29 ? 0xffffffffu : 0u;
    uint v31 = v29 <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v32 = v30 & v31;
    uint v33 = v27 & v32;
    uint v34 = (v33 != 0u) ? v9 : v0;
    float v35 = fma(as_type<float>(v34), v18, v12);
    uint v36 = (uint)as_type<ushort>((half)v35);
    float v37 = fma(as_type<float>(v34), v22, v14);
    uint v38 = (uint)as_type<ushort>((half)v37);
    float v39 = (float)as_type<half>((ushort)v11_1);
    float v40 = v39 * v10;
    float v41 = fma(as_type<float>(v2), v15, v40);
    float v42 = fma(as_type<float>(v2), v39, v41);
    float v43 = v42 - v39;
    float v44 = fma(as_type<float>(v34), v43, v39);
    uint v45 = (uint)as_type<ushort>((half)v44);
    float v46 = (float)as_type<half>((ushort)v11_2);
    float v47 = v46 * v10;
    float v48 = fma(as_type<float>(v3), v15, v47);
    float v49 = fma(as_type<float>(v3), v46, v48);
    float v50 = v49 - v46;
    float v51 = fma(as_type<float>(v34), v50, v46);
    uint v52 = (uint)as_type<ushort>((half)v51);
    p1[y * m.stride1 + x] = half4(as_type<half>((ushort)v36), as_type<half>((ushort)v45), as_type<half>((ushort)v52), as_type<half>((ushort)v38));
}
