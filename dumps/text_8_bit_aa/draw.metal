#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, count2, stride0, stride1, stride2; };

kernel void umbra_entry(
    constant meta &m [[buffer(3)]],
    device uint *p0 [[buffer(0)]],
    device half4 *p1 [[buffer(1)]],
    device ushort *p2 [[buffer(2)]],
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
    uint v6 = 998277249u;
    uint v7 = 1065353216u;
    float v8 = as_type<float>(v7) - as_type<float>(v4);
    uint v9 = (uint)p2[y * m.stride2 + x];
    uint v10 = (uint)(int)(short)(ushort)v9;
    float v11 = (float)(int)v10;
    float v12 = v11 * as_type<float>(998277249u);
    half4 _px13 = p1[y * m.stride1 + x];
    uint v13 = (uint)as_type<ushort>(_px13.x);
    uint v13_1 = (uint)as_type<ushort>(_px13.y);
    uint v13_2 = (uint)as_type<ushort>(_px13.z);
    uint v13_3 = (uint)as_type<ushort>(_px13.w);
    float v14 = (float)as_type<half>((ushort)v13);
    float v15 = fma(v14, v8, as_type<float>(v1));
    float v16 = v15 - v14;
    float v17 = fma(v12, v16, v14);
    uint v18 = (uint)as_type<ushort>((half)v17);
    float v19 = (float)as_type<half>((ushort)v13_3);
    float v20 = fma(v19, v8, as_type<float>(v4));
    float v21 = v20 - v19;
    float v22 = fma(v12, v21, v19);
    uint v23 = (uint)as_type<ushort>((half)v22);
    float v24 = (float)as_type<half>((ushort)v13_1);
    float v25 = fma(v24, v8, as_type<float>(v2));
    float v26 = v25 - v24;
    float v27 = fma(v12, v26, v24);
    uint v28 = (uint)as_type<ushort>((half)v27);
    float v29 = (float)as_type<half>((ushort)v13_2);
    float v30 = fma(v29, v8, as_type<float>(v3));
    float v31 = v30 - v29;
    float v32 = fma(v12, v31, v29);
    uint v33 = (uint)as_type<ushort>((half)v32);
    p1[y * m.stride1 + x] = half4(as_type<half>((ushort)v18), as_type<half>((ushort)v28), as_type<half>((ushort)v33), as_type<half>((ushort)v23));
}
