#include <metal_stdlib>
using namespace metal;


kernel void umbra_entry(
    constant uint &w [[buffer(3)]],
    constant uint *buf_limit [[buffer(4)]],
    constant uint *buf_rbs [[buffer(5)]],
    constant uint &x0 [[buffer(6)]],
    constant uint &y0 [[buffer(7)]],
    device uchar *p0 [[buffer(0)]],
    device uchar *p1 [[buffer(1)]],
    device uchar *p2 [[buffer(2)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= w) return;
    uint x = x0 + pos.x;
    uint y = y0 + pos.y;
    uint v0 = 0u;
    uint v1 = ((device const uint*)p0)[0];
    uint v2 = ((device const uint*)p0)[1];
    uint v3 = ((device const uint*)p0)[2];
    uint v4 = ((device const uint*)p0)[3];
    uint v6 = 1065353216u;
    float v7 = as_type<float>(v6) - as_type<float>(v4);
    uint v8 = ((device uint*)(p2 + y * buf_rbs[2]))[x];
    float v9 = fabs(as_type<float>(v8));
    float v10 = min(v9, as_type<float>(1065353216u));
    device uchar *row11 = p1 + y * buf_rbs[1]; uint ps11 = buf_limit[1];
    uint v11 = (uint)((device ushort*)row11)[x];
    uint v11_1 = (uint)((device ushort*)(row11+ps11))[x];
    uint v11_2 = (uint)((device ushort*)(row11+2*ps11))[x];
    uint v11_3 = (uint)((device ushort*)(row11+3*ps11))[x];
    float v12 = (float)as_type<half>((ushort)v11);
    float v13 = fma(v12, v7, as_type<float>(v1));
    float v14 = v13 - v12;
    float v15 = fma(v10, v14, v12);
    uint v16 = (uint)as_type<ushort>((half)v15);
    float v17 = (float)as_type<half>((ushort)v11_3);
    float v18 = fma(v17, v7, as_type<float>(v4));
    float v19 = v18 - v17;
    float v20 = fma(v10, v19, v17);
    uint v21 = (uint)as_type<ushort>((half)v20);
    float v22 = (float)as_type<half>((ushort)v11_1);
    float v23 = fma(v22, v7, as_type<float>(v2));
    float v24 = v23 - v22;
    float v25 = fma(v10, v24, v22);
    uint v26 = (uint)as_type<ushort>((half)v25);
    float v27 = (float)as_type<half>((ushort)v11_2);
    float v28 = fma(v27, v7, as_type<float>(v3));
    float v29 = v28 - v27;
    float v30 = fma(v10, v29, v27);
    uint v31 = (uint)as_type<ushort>((half)v30);
    {
        device uchar *row = p1 + y * buf_rbs[1]; uint ps = buf_limit[1];
        ((device ushort*)row)[x] = ushort(v16); ((device ushort*)(row+ps))[x] = ushort(v26); ((device ushort*)(row+2*ps))[x] = ushort(v31); ((device ushort*)(row+3*ps))[x] = ushort(v21);
    }
}
