#include <metal_stdlib>
using namespace metal;


kernel void umbra_entry(
    constant uint &w [[buffer(2)]],
    constant uint *buf_limit [[buffer(3)]],
    constant uint *buf_row_bytes [[buffer(4)]],
    constant uint &x0 [[buffer(5)]],
    constant uint &y0 [[buffer(6)]],
    device uchar *p0 [[buffer(0)]],
    device uchar *p1 [[buffer(1)]],
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
    uint v5 = 1065353216u;
    float v6 = as_type<float>(v5) - as_type<float>(v4);
    device uchar *row7 = p1 + y * buf_row_bytes[1]; uint ps7 = buf_limit[1];
    uint v7 = (uint)((device ushort*)row7)[x];
    uint v7_1 = (uint)((device ushort*)(row7+ps7))[x];
    uint v7_2 = (uint)((device ushort*)(row7+2*ps7))[x];
    uint v7_3 = (uint)((device ushort*)(row7+3*ps7))[x];
    float v8 = (float)as_type<half>((ushort)v7);
    float v9 = fma(v8, v6, as_type<float>(v1));
    uint v10 = (uint)as_type<ushort>((half)v9);
    float v11 = (float)as_type<half>((ushort)v7_3);
    float v12 = fma(v11, v6, as_type<float>(v4));
    uint v13 = (uint)as_type<ushort>((half)v12);
    float v14 = (float)as_type<half>((ushort)v7_1);
    float v15 = fma(v14, v6, as_type<float>(v2));
    uint v16 = (uint)as_type<ushort>((half)v15);
    float v17 = (float)as_type<half>((ushort)v7_2);
    float v18 = fma(v17, v6, as_type<float>(v3));
    uint v19 = (uint)as_type<ushort>((half)v18);
    {
        device uchar *row = p1 + y * buf_row_bytes[1]; uint ps = buf_limit[1];
        ((device ushort*)row)[x] = ushort(v10); ((device ushort*)(row+ps))[x] = ushort(v16); ((device ushort*)(row+2*ps))[x] = ushort(v19); ((device ushort*)(row+3*ps))[x] = ushort(v13);
    }
}
