#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, count2, count3, count4, stride0, stride1, stride2, stride3, stride4; };

kernel void umbra_entry(
    constant meta &m [[buffer(5)]],
    device const uint * __restrict p0 [[buffer(0)]],
    device ushort * __restrict p1 [[buffer(1)]],
    device ushort * __restrict p2 [[buffer(2)]],
    device ushort * __restrict p3 [[buffer(3)]],
    device ushort * __restrict p4 [[buffer(4)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= m.w) return;
    uint x = m.x0 + pos.x;
    uint y = m.y0 + pos.y;
    uint v0 = 0u;
    uint v1 = 255u;
    uint v3 = 1065353216u;
    uint v4 = p0[y * m.stride0 + x];
    uint v5 = v4 & 255u;
    float v6 = (float)(int)v5;
    float v8 = v6 * as_type<float>(998277249u);
    #define v9 v8
    uint v10 = v4 >> 24u;
    float v11 = (float)(int)v10;
    float v13 = v11 * as_type<float>(998277249u);
    #define v14 v13
    float v15 = as_type<float>(v3) - v14;
    uint v16 = v4 >> 8u;
    uint v17 = v16 & 255u;
    float v18 = (float)(int)v17;
    float v20 = v18 * as_type<float>(998277249u);
    #define v21 v20
    uint v22 = v4 >> 16u;
    uint v23 = v22 & 255u;
    float v24 = (float)(int)v23;
    float v26 = v24 * as_type<float>(998277249u);
    #define v27 v26
    uint v28 = (uint)p1[y * m.stride1 + x];
    float v29 = (float)as_type<half>((ushort)v28);
    float v30 = fma(v29, v15, v9);
    uint v31 = (uint)as_type<ushort>((half)v30);
    p1[y * m.stride1 + x] = (ushort)v31;

    uint v33 = (uint)p2[y * m.stride2 + x];
    float v34 = (float)as_type<half>((ushort)v33);
    float v35 = fma(v34, v15, v21);
    uint v36 = (uint)as_type<ushort>((half)v35);
    p2[y * m.stride2 + x] = (ushort)v36;

    uint v38 = (uint)p3[y * m.stride3 + x];
    float v39 = (float)as_type<half>((ushort)v38);
    float v40 = fma(v39, v15, v27);
    uint v41 = (uint)as_type<ushort>((half)v40);
    p3[y * m.stride3 + x] = (ushort)v41;

    uint v43 = (uint)p4[y * m.stride4 + x];
    float v44 = (float)as_type<half>((ushort)v43);
    float v45 = fma(v44, v15, v14);
    uint v46 = (uint)as_type<ushort>((half)v45);
    p4[y * m.stride4 + x] = (ushort)v46;
}
