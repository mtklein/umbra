#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, limit0, limit1, limit2, limit3, limit4, stride0, stride1, stride2, stride3, stride4; };

kernel void umbra_entry(
    constant meta &m [[buffer(5)]],
    device uint *p0 [[buffer(0)]],
    device ushort *p1 [[buffer(1)]],
    device ushort *p2 [[buffer(2)]],
    device ushort *p3 [[buffer(3)]],
    device ushort *p4 [[buffer(4)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= m.w) return;
    uint x = m.x0 + pos.x;
    uint y = m.y0 + pos.y;
    uint v0 = 0u;
    uint v1 = 255u;
    uint v2 = 998277249u;
    uint v3 = 1065353216u;
    uint v4 = p0[y * m.stride0 + x];
    uint v5 = v4 & 255u;
    float v6 = (float)(int)v5;
    float v7 = v6 * as_type<float>(998277249u);
    uint v8 = v4 >> 24u;
    float v9 = (float)(int)v8;
    float v10 = v9 * as_type<float>(998277249u);
    float v11 = as_type<float>(v3) - v10;
    uint v12 = v4 >> 8u;
    uint v13 = v12 & 255u;
    float v14 = (float)(int)v13;
    float v15 = v14 * as_type<float>(998277249u);
    uint v16 = v4 >> 16u;
    uint v17 = v16 & 255u;
    float v18 = (float)(int)v17;
    float v19 = v18 * as_type<float>(998277249u);
    uint v20 = (uint)p1[y * m.stride1 + x];
    float v21 = (float)as_type<half>((ushort)v20);
    float v22 = fma(v21, v11, v7);
    uint v23 = (uint)as_type<ushort>((half)v22);
    p1[y * m.stride1 + x] = (ushort)v23;

    uint v25 = (uint)p2[y * m.stride2 + x];
    float v26 = (float)as_type<half>((ushort)v25);
    float v27 = fma(v26, v11, v15);
    uint v28 = (uint)as_type<ushort>((half)v27);
    p2[y * m.stride2 + x] = (ushort)v28;

    uint v30 = (uint)p3[y * m.stride3 + x];
    float v31 = (float)as_type<half>((ushort)v30);
    float v32 = fma(v31, v11, v19);
    uint v33 = (uint)as_type<ushort>((half)v32);
    p3[y * m.stride3 + x] = (ushort)v33;

    uint v35 = (uint)p4[y * m.stride4 + x];
    float v36 = (float)as_type<half>((ushort)v35);
    float v37 = fma(v36, v11, v10);
    uint v38 = (uint)as_type<ushort>((half)v37);
    p4[y * m.stride4 + x] = (ushort)v38;
}
