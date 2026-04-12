#include <metal_stdlib>
using namespace metal;


kernel void umbra_entry(
    constant uint &w [[buffer(3)]],
    constant uint *buf_limit [[buffer(4)]],
    constant uint *buf_stride [[buffer(5)]],
    constant uint &x0 [[buffer(6)]],
    constant uint &y0 [[buffer(7)]],
    device uint *p0 [[buffer(0)]],
    device ushort *p1 [[buffer(1)]],
    device ushort *p2 [[buffer(2)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= w) return;
    uint x = x0 + pos.x;
    uint y = y0 + pos.y;
    uint v0 = 0u;
    uint v1 = p0[0];
    uint v2 = p0[1];
    uint v3 = p0[2];
    uint v4 = p0[3];
    uint v6 = 998277249u;
    uint v7 = 1054867456u;
    uint v8 = 1090519040u;
    uint v9 = 1065353216u;
    float v10 = as_type<float>(v9) - as_type<float>(v4);
    uint v11 = (uint)p2[y * buf_stride[2] + x];
    uint v12 = (uint)(int)(short)(ushort)v11;
    float v13 = (float)(int)v12;
    float v14 = v13 * as_type<float>(998277249u);
    float v15 = v14 - as_type<float>(1054867456u);
    float v16 = v15 * as_type<float>(1090519040u);
    float v17 = max(v16, as_type<float>(0u));
    float v18 = min(v17, as_type<float>(1065353216u));
    uint _row19 = y * buf_stride[1]; uint _ps19 = buf_limit[1];
    uint v19 = (uint)p1[_row19 + x];
    uint v19_1 = (uint)p1[_row19 + x + _ps19];
    uint v19_2 = (uint)p1[_row19 + x + 2*_ps19];
    uint v19_3 = (uint)p1[_row19 + x + 3*_ps19];
    float v20 = (float)as_type<half>((ushort)v19);
    float v21 = fma(v20, v10, as_type<float>(v1));
    float v22 = v21 - v20;
    float v23 = fma(v18, v22, v20);
    uint v24 = (uint)as_type<ushort>((half)v23);
    float v25 = (float)as_type<half>((ushort)v19_3);
    float v26 = fma(v25, v10, as_type<float>(v4));
    float v27 = v26 - v25;
    float v28 = fma(v18, v27, v25);
    uint v29 = (uint)as_type<ushort>((half)v28);
    float v30 = (float)as_type<half>((ushort)v19_1);
    float v31 = fma(v30, v10, as_type<float>(v2));
    float v32 = v31 - v30;
    float v33 = fma(v18, v32, v30);
    uint v34 = (uint)as_type<ushort>((half)v33);
    float v35 = (float)as_type<half>((ushort)v19_2);
    float v36 = fma(v35, v10, as_type<float>(v3));
    float v37 = v36 - v35;
    float v38 = fma(v18, v37, v35);
    uint v39 = (uint)as_type<ushort>((half)v38);
    { uint _row = y * buf_stride[1]; uint _ps = buf_limit[1];
      p1[_row + x] = ushort(v24); p1[_row + x + _ps] = ushort(v34); p1[_row + x + 2*_ps] = ushort(v39); p1[_row + x + 3*_ps] = ushort(v29); }
}
