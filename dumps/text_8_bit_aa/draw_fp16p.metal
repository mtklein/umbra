#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, count2, stride0, stride1, stride2; };

kernel void umbra_entry(
    constant meta &m [[buffer(3)]],
    device uint *p0 [[buffer(0)]],
    device ushort *p1 [[buffer(1)]],
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
    uint _row9 = y * m.stride1; uint _ps9 = m.count1 / 4;
    uint v9 = (uint)p1[_row9 + x];
    uint v9_1 = (uint)p1[_row9 + x + _ps9];
    uint v9_2 = (uint)p1[_row9 + x + 2*_ps9];
    uint v9_3 = (uint)p1[_row9 + x + 3*_ps9];
    float v10 = (float)as_type<half>((ushort)v9);
    float v11 = fma(v10, v8, as_type<float>(v1));
    float v12 = v11 - v10;
    float v13 = (float)as_type<half>((ushort)v9_3);
    float v14 = fma(v13, v8, as_type<float>(v4));
    float v15 = v14 - v13;
    uint v16 = (uint)p2[y * m.stride2 + x];
    uint v17 = (uint)(int)(short)(ushort)v16;
    float v18 = (float)(int)v17;
    float v19 = v18 * as_type<float>(998277249u);
    float v20 = fma(v19, v12, v10);
    uint v21 = (uint)as_type<ushort>((half)v20);
    float v22 = fma(v19, v15, v13);
    uint v23 = (uint)as_type<ushort>((half)v22);
    float v24 = (float)as_type<half>((ushort)v9_1);
    float v25 = fma(v24, v8, as_type<float>(v2));
    float v26 = v25 - v24;
    float v27 = fma(v19, v26, v24);
    uint v28 = (uint)as_type<ushort>((half)v27);
    float v29 = (float)as_type<half>((ushort)v9_2);
    float v30 = fma(v29, v8, as_type<float>(v3));
    float v31 = v30 - v29;
    float v32 = fma(v19, v31, v29);
    uint v33 = (uint)as_type<ushort>((half)v32);
    { uint _row = y * m.stride1; uint _ps = m.count1 / 4;
      p1[_row + x] = ushort(v21); p1[_row + x + _ps] = ushort(v28); p1[_row + x + 2*_ps] = ushort(v33); p1[_row + x + 3*_ps] = ushort(v23); }
}
