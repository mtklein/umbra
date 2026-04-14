#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, count2, stride0, stride1, stride2; };

kernel void main0(
    constant meta &m [[buffer(0)]],
    device const uint * __restrict p0 [[buffer(1)]],
    device half4 * __restrict p1 [[buffer(2)]],
    device const uint * __restrict p2 [[buffer(3)]],
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
    uint v6 = 1065353216u;
    float v7 = as_type<float>(v6) - as_type<float>(v4);
    half4 _px8 = p1[y * m.stride1 + x];
    uint v8 = (uint)as_type<ushort>(_px8.x);
    uint v8_1 = (uint)as_type<ushort>(_px8.y);
    uint v8_2 = (uint)as_type<ushort>(_px8.z);
    uint v8_3 = (uint)as_type<ushort>(_px8.w);
    float v9 = (float)as_type<half>((ushort)v8);
    float v10 = fma(v9, v7, as_type<float>(v1));
    float v11 = v10 - v9;
    float v12 = (float)as_type<half>((ushort)v8_3);
    float v13 = fma(v12, v7, as_type<float>(v4));
    float v14 = v13 - v12;
    uint v15 = p2[y * m.stride2 + x];
    float v16 = fabs(as_type<float>(v15));
    float v18 = min(v16, as_type<float>(1065353216u));
    #define v19 v18
    float v20 = fma(v19, v11, v9);
    uint v21 = (uint)as_type<ushort>((half)v20);
    float v22 = fma(v19, v14, v12);
    uint v23 = (uint)as_type<ushort>((half)v22);
    float v24 = (float)as_type<half>((ushort)v8_1);
    float v25 = fma(v24, v7, as_type<float>(v2));
    float v26 = v25 - v24;
    float v27 = fma(v19, v26, v24);
    uint v28 = (uint)as_type<ushort>((half)v27);
    float v29 = (float)as_type<half>((ushort)v8_2);
    float v30 = fma(v29, v7, as_type<float>(v3));
    float v31 = v30 - v29;
    float v32 = fma(v19, v31, v29);
    uint v33 = (uint)as_type<ushort>((half)v32);
    p1[y * m.stride1 + x] = half4(as_type<half>((ushort)v21), as_type<half>((ushort)v28), as_type<half>((ushort)v33), as_type<half>((ushort)v23));
}
