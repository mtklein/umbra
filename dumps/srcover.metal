#include <metal_stdlib>
using namespace metal;

static inline int safe_ix(int ix, uint bytes, int elem) {
    int count = (int)(bytes / (uint)elem);
    return clamp(ix, 0, max(count-1, 0));
}
static inline uint oob_mask(int ix, uint bytes, int elem) {
    int count = (int)(bytes / (uint)elem);
    return (ix >= 0 && ix < count) ? ~0u : 0u;
}

kernel void umbra_entry(
    constant uint &w [[buffer(5)]],
    constant uint *buf_szs [[buffer(6)]],
    constant uint *buf_rbs [[buffer(7)]],
    constant uint &x0 [[buffer(8)]],
    constant uint &y0 [[buffer(9)]],
    device uchar *p0 [[buffer(0)]],
    device uchar *p1 [[buffer(1)]],
    device uchar *p2 [[buffer(2)]],
    device uchar *p3 [[buffer(3)]],
    device uchar *p4 [[buffer(4)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= w) return;
    uint x = x0 + pos.x;
    uint y = y0 + pos.y;
    uint v0 = 0u;
    uint v1 = 255u;
    uint v2 = 998277249u;
    uint v3 = 1065353216u;
    uint v4 = ((device uint*)(p0 + y * buf_rbs[0]))[x];
    uint v5 = v4 >> 24u;
    uint v6 = as_type<uint>((float)(int)v5);
    uint v7 = as_type<uint>(as_type<float>(v6) * as_type<float>(998277249u));
    uint v8 = as_type<uint>(as_type<float>(v3) - as_type<float>(v7));
    uint v9 = v4 >> 8u;
    uint v10 = v9 & 255u;
    uint v11 = as_type<uint>((float)(int)v10);
    uint v12 = as_type<uint>(as_type<float>(v11) * as_type<float>(998277249u));
    uint v13 = v4 >> 16u;
    uint v14 = v13 & 255u;
    uint v15 = as_type<uint>((float)(int)v14);
    uint v16 = as_type<uint>(as_type<float>(v15) * as_type<float>(998277249u));
    uint v17 = v4 & 255u;
    uint v18 = as_type<uint>((float)(int)v17);
    uint v19 = as_type<uint>(as_type<float>(v18) * as_type<float>(998277249u));
    uint v20 = (uint)((device ushort*)(p1 + y * buf_rbs[1]))[x];
    uint v21 = as_type<uint>((float)as_type<half>((ushort)v20));
    uint v22 = as_type<uint>(fma(as_type<float>(v21), as_type<float>(v8), as_type<float>(v19)));
    uint v23 = (uint)as_type<ushort>((half)as_type<float>(v22));
    ((device ushort*)(p1 + y * buf_rbs[1]))[x] = (ushort)v23;

    uint v25 = (uint)((device ushort*)(p2 + y * buf_rbs[2]))[x];
    uint v26 = as_type<uint>((float)as_type<half>((ushort)v25));
    uint v27 = as_type<uint>(fma(as_type<float>(v26), as_type<float>(v8), as_type<float>(v12)));
    uint v28 = (uint)as_type<ushort>((half)as_type<float>(v27));
    ((device ushort*)(p2 + y * buf_rbs[2]))[x] = (ushort)v28;

    uint v30 = (uint)((device ushort*)(p3 + y * buf_rbs[3]))[x];
    uint v31 = as_type<uint>((float)as_type<half>((ushort)v30));
    uint v32 = as_type<uint>(fma(as_type<float>(v31), as_type<float>(v8), as_type<float>(v16)));
    uint v33 = (uint)as_type<ushort>((half)as_type<float>(v32));
    ((device ushort*)(p3 + y * buf_rbs[3]))[x] = (ushort)v33;

    uint v35 = (uint)((device ushort*)(p4 + y * buf_rbs[4]))[x];
    uint v36 = as_type<uint>((float)as_type<half>((ushort)v35));
    uint v37 = as_type<uint>(fma(as_type<float>(v36), as_type<float>(v8), as_type<float>(v7)));
    uint v38 = (uint)as_type<ushort>((half)as_type<float>(v37));
    ((device ushort*)(p4 + y * buf_rbs[4]))[x] = (ushort)v38;
}
