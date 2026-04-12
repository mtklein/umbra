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
    uint v5 = ((device const uint*)p0)[4];
    uint v6 = ((device const uint*)p0)[5];
    uint v7 = ((device const uint*)p0)[6];
    uint v8 = ((device const uint*)p0)[7];
    uint v9 = 1065353216u;
    uint v10 = x0 + pos.x;
    float v11 = (float)(int)v10;
    uint v12 = as_type<float>(v5) <= v11 ? 0xffffffffu : 0u;
    uint v13 = v11 <  as_type<float>(v7) ? 0xffffffffu : 0u;
    uint v14 = v12 & v13;
    uint v15 = y0 + pos.y;
    float v16 = (float)(int)v15;
    uint v17 = as_type<float>(v6) <= v16 ? 0xffffffffu : 0u;
    uint v18 = v16 <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v19 = v17 & v18;
    uint v20 = v14 & v19;
    uint v21 = select(v0, v9, v20 != 0u);
    device ushort *hp22 = (device ushort*)(p1 + y * buf_row_bytes[1]) + x*4;
    uint v22 = (uint)hp22[0];
    uint v22_1 = (uint)hp22[1];
    uint v22_2 = (uint)hp22[2];
    uint v22_3 = (uint)hp22[3];
    float v23 = (float)as_type<half>((ushort)v22);
    float v24 = (float)as_type<half>((ushort)v22_3);
    float v25 = as_type<float>(v9) - v24;
    float v26 = fma(as_type<float>(v1), v25, v23);
    float v27 = v26 - v23;
    float v28 = fma(as_type<float>(v21), v27, v23);
    uint v29 = (uint)as_type<ushort>((half)v28);
    float v30 = fma(as_type<float>(v4), v25, v24);
    float v31 = v30 - v24;
    float v32 = fma(as_type<float>(v21), v31, v24);
    uint v33 = (uint)as_type<ushort>((half)v32);
    float v34 = (float)as_type<half>((ushort)v22_1);
    float v35 = fma(as_type<float>(v2), v25, v34);
    float v36 = v35 - v34;
    float v37 = fma(as_type<float>(v21), v36, v34);
    uint v38 = (uint)as_type<ushort>((half)v37);
    float v39 = (float)as_type<half>((ushort)v22_2);
    float v40 = fma(as_type<float>(v3), v25, v39);
    float v41 = v40 - v39;
    float v42 = fma(as_type<float>(v21), v41, v39);
    uint v43 = (uint)as_type<ushort>((half)v42);
    {
        device ushort *hp = (device ushort*)(p1 + y * buf_row_bytes[1]) + x*4;
        hp[0] = ushort(v29); hp[1] = ushort(v38); hp[2] = ushort(v43); hp[3] = ushort(v33);
    }
}
