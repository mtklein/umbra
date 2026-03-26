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
    constant uint &n [[buffer(0)]],
    constant uint &w [[buffer(7)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    device uchar *p2 [[buffer(3)]],
    device uchar *p3 [[buffer(4)]],
    device uchar *p4 [[buffer(5)]],
    constant uint *buf_szs [[buffer(6)]],
    uint2 pos [[thread_position_in_grid]]
) {
    uint i = pos.y * w + pos.x;
    if (i >= n) return;
    uint v0 = 0u;
    uint v1 = 255u;
    uint v2 = 998277249u;
    uint v3 = 1065353216u;
    uint v4 = ((device uint*)p0)[i];
    uint v5 = v4 >> 24u;
    uint v6 = as_type<uint>((float)(int)v5);
    uint v7 = as_type<uint>(as_type<float>(v2) * as_type<float>(v6));
    uint v8 = as_type<uint>(as_type<float>(v3) - as_type<float>(v7));
    uint v9 = v4 & v1;
    uint v10 = as_type<uint>((float)(int)v9);
    uint v11 = v4 >> 8u;
    uint v12 = v1 & v11;
    uint v13 = as_type<uint>((float)(int)v12);
    uint v14 = v4 >> 16u;
    uint v15 = v1 & v14;
    uint v16 = as_type<uint>((float)(int)v15);
    uint v17 = as_type<uint>(as_type<float>(v2) * as_type<float>(v10));
    uint v18 = as_type<uint>(as_type<float>(v2) * as_type<float>(v13));
    uint v19 = as_type<uint>(as_type<float>(v2) * as_type<float>(v16));
    uint v20 = (uint)((device ushort*)p1)[i];
    uint v21 = as_type<uint>((float)as_type<half>((ushort)v20));
    uint v22 = as_type<uint>(fma(as_type<float>(v21), as_type<float>(v8), as_type<float>(v17)));
    uint v23 = (uint)as_type<ushort>((half)as_type<float>(v22));
    ((device ushort*)p1)[i] = (ushort)v23;

    uint v25 = (uint)((device ushort*)p2)[i];
    uint v26 = as_type<uint>((float)as_type<half>((ushort)v25));
    uint v27 = as_type<uint>(fma(as_type<float>(v26), as_type<float>(v8), as_type<float>(v18)));
    uint v28 = (uint)as_type<ushort>((half)as_type<float>(v27));
    ((device ushort*)p2)[i] = (ushort)v28;

    uint v30 = (uint)((device ushort*)p3)[i];
    uint v31 = as_type<uint>((float)as_type<half>((ushort)v30));
    uint v32 = as_type<uint>(fma(as_type<float>(v31), as_type<float>(v8), as_type<float>(v19)));
    uint v33 = (uint)as_type<ushort>((half)as_type<float>(v32));
    ((device ushort*)p3)[i] = (ushort)v33;

    uint v35 = (uint)((device ushort*)p4)[i];
    uint v36 = as_type<uint>((float)as_type<half>((ushort)v35));
    uint v37 = as_type<uint>(fma(as_type<float>(v36), as_type<float>(v8), as_type<float>(v7)));
    uint v38 = (uint)as_type<ushort>((half)as_type<float>(v37));
    ((device ushort*)p4)[i] = (ushort)v38;
}
