#include <metal_stdlib>
using namespace metal;


kernel void umbra_entry(
    constant uint &w [[buffer(2)]],
    constant uint *buf_limit [[buffer(3)]],
    constant uint *buf_rbs [[buffer(4)]],
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
    uint v4 = 1065353216u;
    uint v5 = ((device const uint*)p0)[3];
    uint v6 = ((device const uint*)p0)[4];
    uint v7 = ((device const uint*)p0)[5];
    uint v8 = ((device const uint*)p0)[6];
    uint v9 = ((device const uint*)p0)[7];
    uint v10 = ((device const uint*)p0)[8];
    uint v11 = ((device const uint*)p0)[9];
    uint v12 = ((device const uint*)p0)[10];
    float v13 = as_type<float>(v9) - as_type<float>(v5);
    float v14 = as_type<float>(v10) - as_type<float>(v6);
    float v15 = as_type<float>(v11) - as_type<float>(v7);
    float v16 = as_type<float>(v12) - as_type<float>(v8);
    uint v17 = x0 + pos.x;
    float v18 = (float)(int)v17;
    float v19 = v18 - as_type<float>(v1);
    uint v20 = y0 + pos.y;
    float v21 = (float)(int)v20;
    float v22 = v21 - as_type<float>(v2);
    float v23 = v22 * v22;
    float v24 = fma(v19, v19, v23);
    float v25 = precise::sqrt(v24);
    float v26 = as_type<float>(v3) * v25;
    float v27 = max(v26, as_type<float>(0u));
    float v28 = min(v27, as_type<float>(1065353216u));
    float v29 = fma(v28, v13, as_type<float>(v5));
    uint v30 = (uint)as_type<ushort>((half)v29);
    float v31 = fma(v28, v16, as_type<float>(v8));
    uint v32 = (uint)as_type<ushort>((half)v31);
    float v33 = fma(v28, v14, as_type<float>(v6));
    uint v34 = (uint)as_type<ushort>((half)v33);
    float v35 = fma(v28, v15, as_type<float>(v7));
    uint v36 = (uint)as_type<ushort>((half)v35);
    {
        device uchar *row = p1 + y * buf_rbs[1]; uint ps = buf_limit[1];
        ((device ushort*)row)[x] = ushort(v30); ((device ushort*)(row+ps))[x] = ushort(v34); ((device ushort*)(row+2*ps))[x] = ushort(v36); ((device ushort*)(row+3*ps))[x] = ushort(v32);
    }
}
