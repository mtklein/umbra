#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, count2, count3, count4, stride0, stride1, stride2, stride3, stride4; };

kernel void umbra_entry(
    constant meta &m [[buffer(0)]],
    device const uint * __restrict p0 [[buffer(1)]],
    device ushort * __restrict p1 [[buffer(2)]],
    device ushort * __restrict p2 [[buffer(3)]],
    device ushort * __restrict p3 [[buffer(4)]],
    device ushort * __restrict p4 [[buffer(5)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= m.w) return;
    uint x = m.x0 + pos.x;
    uint y = m.y0 + pos.y;
    uint v0 = 0u;
    uint v6 = 1065353216u;
    uint v7 = p0[y * m.stride0 + x];
    uint v9 = v7 & 255u;
    #define v10 v9
    float v11 = (float)(int)v10;
    float v13 = v11 * as_type<float>(998277249u);
    #define v14 v13
    uint v16 = v7 >> 8u;
    #define v17 v16
    uint v19 = v17 & 255u;
    #define v20 v19
    float v21 = (float)(int)v20;
    float v23 = v21 * as_type<float>(998277249u);
    #define v24 v23
    uint v26 = v7 >> 16u;
    #define v27 v26
    uint v29 = v27 & 255u;
    #define v30 v29
    float v31 = (float)(int)v30;
    float v33 = v31 * as_type<float>(998277249u);
    #define v34 v33
    uint v35 = v7 >> 24u;
    #define v37 v35
    float v38 = (float)(int)v37;
    float v40 = v38 * as_type<float>(998277249u);
    #define v41 v40
    float v42 = as_type<float>(v6) - v41;
    uint v43 = (uint)p1[y * m.stride1 + x];
    float v44 = (float)as_type<half>((ushort)v43);
    float v45 = fma(v44, v42, v14);
    uint v46 = (uint)as_type<ushort>((half)v45);
    p1[y * m.stride1 + x] = (ushort)v46;

    uint v48 = (uint)p2[y * m.stride2 + x];
    float v49 = (float)as_type<half>((ushort)v48);
    float v50 = fma(v49, v42, v24);
    uint v51 = (uint)as_type<ushort>((half)v50);
    p2[y * m.stride2 + x] = (ushort)v51;

    uint v53 = (uint)p3[y * m.stride3 + x];
    float v54 = (float)as_type<half>((ushort)v53);
    float v55 = fma(v54, v42, v34);
    uint v56 = (uint)as_type<ushort>((half)v55);
    p3[y * m.stride3 + x] = (ushort)v56;

    uint v58 = (uint)p4[y * m.stride4 + x];
    float v59 = (float)as_type<half>((ushort)v58);
    float v60 = fma(v59, v42, v41);
    uint v61 = (uint)as_type<ushort>((half)v60);
    p4[y * m.stride4 + x] = (ushort)v61;
}
