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
    device ushort *hp23 = (device ushort*)(p1 + y * buf_rbs[1]) + x*4;
    uint v23 = (uint)hp23[0];
    uint v23_1 = (uint)hp23[1];
    uint v23_2 = (uint)hp23[2];
    uint v23_3 = (uint)hp23[3];
    float v24 = (float)as_type<half>((ushort)v23);
    float v25 = v24 * v10;
    float v26 = (float)as_type<half>((ushort)v23_3);
    float v27 = as_type<float>(v9) - v26;
    float v28 = fma(as_type<float>(v1), v27, v25);
    float v29 = fma(as_type<float>(v1), v24, v28);
    float v30 = v29 - v24;
    float v31 = fma(as_type<float>(v22), v30, v24);
    uint v32 = (uint)as_type<ushort>((half)v31);
    float v33 = v26 * v10;
    float v34 = fma(as_type<float>(v4), v27, v33);
    float v35 = fma(as_type<float>(v4), v26, v34);
    float v36 = v35 - v26;
    float v37 = fma(as_type<float>(v22), v36, v26);
    uint v38 = (uint)as_type<ushort>((half)v37);
    float v39 = (float)as_type<half>((ushort)v23_1);
    float v40 = v39 * v10;
    float v41 = fma(as_type<float>(v2), v27, v40);
    float v42 = fma(as_type<float>(v2), v39, v41);
    float v43 = v42 - v39;
    float v44 = fma(as_type<float>(v22), v43, v39);
    uint v45 = (uint)as_type<ushort>((half)v44);
    float v46 = (float)as_type<half>((ushort)v23_2);
    float v47 = v46 * v10;
    float v48 = fma(as_type<float>(v3), v27, v47);
    float v49 = fma(as_type<float>(v3), v46, v48);
    float v50 = v49 - v46;
    float v51 = fma(as_type<float>(v22), v50, v46);
    uint v52 = (uint)as_type<ushort>((half)v51);
    {
        device ushort *hp = (device ushort*)(p1 + y * buf_rbs[1]) + x*4;
        hp[0] = ushort(v32); hp[1] = ushort(v45); hp[2] = ushort(v52); hp[3] = ushort(v38);
    }
}
