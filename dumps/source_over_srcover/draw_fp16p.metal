#include <metal_stdlib>
using namespace metal;

int safe_ix(int ix, uint bytes, int elem) {
    int count = (int)(bytes / (uint)elem);
    return clamp(ix, 0, max(count-1, 0));
}
uint oob_mask(int ix, uint bytes, int elem) {
    int count = (int)(bytes / (uint)elem);
    return (ix >= 0 && ix < count) ? ~0u : 0u;
}

kernel void umbra_entry(
    constant uint &w [[buffer(2)]],
    constant uint *buf_szs [[buffer(3)]],
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
    uint v4 = ((device const uint*)p0)[3];
    uint v5 = ((device const uint*)p0)[4];
    uint v6 = ((device const uint*)p0)[5];
    uint v7 = ((device const uint*)p0)[6];
    uint v8 = ((device const uint*)p0)[7];
    uint v9 = 1065353216u;
    float v10 = as_type<float>(v9) - as_type<float>(v4);
    uint v11 = x0 + pos.x;
    float v12 = (float)(int)v11;
    uint v13 = as_type<float>(v5) <= v12 ? 0xffffffffu : 0u;
    uint v14 = v12 <  as_type<float>(v7) ? 0xffffffffu : 0u;
    uint v15 = v13 & v14;
    uint v16 = y0 + pos.y;
    float v17 = (float)(int)v16;
    uint v18 = as_type<float>(v6) <= v17 ? 0xffffffffu : 0u;
    uint v19 = v17 <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v20 = v18 & v19;
    uint v21 = v15 & v20;
    uint v22 = select(v0, v9, v21 != 0u);
    device uchar *row23 = p1 + y * buf_rbs[1]; uint ps23 = buf_szs[1]/4;
    uint v23 = (uint)((device ushort*)row23)[x];
    uint v23_1 = (uint)((device ushort*)(row23+ps23))[x];
    uint v23_2 = (uint)((device ushort*)(row23+2*ps23))[x];
    uint v23_3 = (uint)((device ushort*)(row23+3*ps23))[x];
    float v24 = (float)as_type<half>((ushort)v23);
    float v25 = fma(v24, v10, as_type<float>(v1));
    float v26 = v25 - v24;
    float v27 = fma(as_type<float>(v22), v26, v24);
    uint v28 = (uint)as_type<ushort>((half)v27);
    float v29 = (float)as_type<half>((ushort)v23_3);
    float v30 = fma(v29, v10, as_type<float>(v4));
    float v31 = v30 - v29;
    float v32 = fma(as_type<float>(v22), v31, v29);
    uint v33 = (uint)as_type<ushort>((half)v32);
    float v34 = (float)as_type<half>((ushort)v23_1);
    float v35 = fma(v34, v10, as_type<float>(v2));
    float v36 = v35 - v34;
    float v37 = fma(as_type<float>(v22), v36, v34);
    uint v38 = (uint)as_type<ushort>((half)v37);
    float v39 = (float)as_type<half>((ushort)v23_2);
    float v40 = fma(v39, v10, as_type<float>(v3));
    float v41 = v40 - v39;
    float v42 = fma(as_type<float>(v22), v41, v39);
    uint v43 = (uint)as_type<ushort>((half)v42);
    {
        device uchar *row = p1 + y * buf_rbs[1]; uint ps = buf_szs[1]/4;
        ((device ushort*)row)[x] = ushort(v28); ((device ushort*)(row+ps))[x] = ushort(v38); ((device ushort*)(row+2*ps))[x] = ushort(v43); ((device ushort*)(row+3*ps))[x] = ushort(v33);
    }
}
