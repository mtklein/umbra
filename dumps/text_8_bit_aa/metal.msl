#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, count2, stride0, stride1, stride2; };

kernel void main0(
    constant meta &m [[buffer(0)]],
    device const uint * __restrict p0 [[buffer(1)]],
    device half4 * __restrict p1 [[buffer(2)]],
    device const ushort * __restrict p2 [[buffer(3)]],
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
    uint v7 = 1065353216u;
    float v8 = as_type<float>(v7) - as_type<float>(v4);
    half4 _px9 = p1[y * m.stride1 + x];
    uint v9 = (uint)as_type<ushort>(_px9.x);
    uint v9_1 = (uint)as_type<ushort>(_px9.y);
    uint v9_2 = (uint)as_type<ushort>(_px9.z);
    uint v9_3 = (uint)as_type<ushort>(_px9.w);
    float v10 = (float)as_type<half>((ushort)v9);
    float v11 = fma(v10, v8, as_type<float>(v1));
    float v12 = v11 - v10;
    float v13 = (float)as_type<half>((ushort)v9_3);
    float v14 = fma(v13, v8, as_type<float>(v4));
    float v15 = v14 - v13;
    uint v16 = (uint)p2[y * m.stride2 + x];
    uint v17 = (uint)(int)(short)(ushort)v16;
    float v18 = (float)(int)v17;
    float v20 = v18 * as_type<float>(998277249u);
    #define v21 v20
    float v22 = fma(v21, v12, v10);
    uint v23 = (uint)as_type<ushort>((half)v22);
    float v24 = fma(v21, v15, v13);
    uint v25 = (uint)as_type<ushort>((half)v24);
    float v26 = (float)as_type<half>((ushort)v9_1);
    float v27 = fma(v26, v8, as_type<float>(v2));
    float v28 = v27 - v26;
    float v29 = fma(v21, v28, v26);
    uint v30 = (uint)as_type<ushort>((half)v29);
    float v31 = (float)as_type<half>((ushort)v9_2);
    float v32 = fma(v31, v8, as_type<float>(v3));
    float v33 = v32 - v31;
    float v34 = fma(v21, v33, v31);
    uint v35 = (uint)as_type<ushort>((half)v34);
    p1[y * m.stride1 + x] = half4(as_type<half>((ushort)v23), as_type<half>((ushort)v30), as_type<half>((ushort)v35), as_type<half>((ushort)v25));
}
