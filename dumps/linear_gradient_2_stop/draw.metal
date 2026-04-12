#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, limit0, limit1, stride0, stride1; };

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
    uint v4 = 1065353216u;
    uint v5 = p0[3];
    uint v6 = p0[4];
    uint v7 = p0[5];
    uint v8 = p0[6];
    uint v9 = p0[7];
    uint v10 = p0[8];
    uint v11 = p0[9];
    uint v12 = p0[10];
    float v13 = as_type<float>(v9) - as_type<float>(v5);
    float v14 = as_type<float>(v10) - as_type<float>(v6);
    float v15 = as_type<float>(v11) - as_type<float>(v7);
    float v16 = as_type<float>(v12) - as_type<float>(v8);
    uint v17 = m.x0 + pos.x;
    float v18 = (float)(int)v17;
    uint v19 = m.y0 + pos.y;
    float v20 = (float)(int)v19;
    float v21 = v20 * as_type<float>(v2);
    float v22 = fma(v18, as_type<float>(v1), v21);
    float v23 = as_type<float>(v3) + v22;
    float v24 = max(v23, as_type<float>(0u));
    float v25 = min(v24, as_type<float>(1065353216u));
    float v26 = fma(v25, v13, as_type<float>(v5));
    uint v27 = (uint)as_type<ushort>((half)v26);
    float v28 = fma(v25, v16, as_type<float>(v8));
    uint v29 = (uint)as_type<ushort>((half)v28);
    float v30 = fma(v25, v14, as_type<float>(v6));
    uint v31 = (uint)as_type<ushort>((half)v30);
    float v32 = fma(v25, v15, as_type<float>(v7));
    uint v33 = (uint)as_type<ushort>((half)v32);
    p1[y * m.stride1 + x] = half4(as_type<half>((ushort)v27), as_type<half>((ushort)v31), as_type<half>((ushort)v33), as_type<half>((ushort)v29));
}
