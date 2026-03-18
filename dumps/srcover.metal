#include <metal_stdlib>
using namespace metal;

static inline int clamp_ix(int ix, uint bytes, int elem) {
    int hi = (int)(bytes / (uint)elem) - 1;
    if (hi < 0) hi = 0;
    return clamp(ix, 0, hi);
}

kernel void umbra_entry(
    constant uint &n [[buffer(0)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    device uchar *p2 [[buffer(3)]],
    device uchar *p3 [[buffer(4)]],
    device uchar *p4 [[buffer(5)]],
    constant uint *buf_szs [[buffer(6)]],
    uint i [[thread_position_in_grid]]
) {
    if (i >= n) return;
    uint v0 = 0u;
    uint v1 = 255u;
    uint v2 = 998277249u;
    uint v3 = 1065353216u;
    uint v4 = ((device uint*)p0)[i];
    uint v5 = v4 & v1;
    uint v6 = as_type<uint>((float)(int)v5);
    uint v7 = (uint)((device ushort*)p1)[i];
    uint v8 = as_type<uint>((float)as_type<half>((ushort)v7));
    uint v9 = v4 >> 24u;
    uint v10 = as_type<uint>((float)(int)v9);
    uint v11 = as_type<uint>(as_type<float>(v3) - as_type<float>(v2) * as_type<float>(v10));
    uint v12 = as_type<uint>(as_type<float>(v8) * as_type<float>(v11));
    uint v13 = as_type<uint>(fma(as_type<float>(v2), as_type<float>(v6), as_type<float>(v12)));
    uint v14 = (uint)as_type<ushort>((half)as_type<float>(v13));
    ((device ushort*)p1)[i] = (ushort)v14;

    uint v16 = v4 >> 8u;
    uint v17 = v1 & v16;
    uint v18 = as_type<uint>((float)(int)v17);
    uint v19 = (uint)((device ushort*)p2)[i];
    uint v20 = as_type<uint>((float)as_type<half>((ushort)v19));
    uint v21 = as_type<uint>(as_type<float>(v20) * as_type<float>(v11));
    uint v22 = as_type<uint>(fma(as_type<float>(v2), as_type<float>(v18), as_type<float>(v21)));
    uint v23 = (uint)as_type<ushort>((half)as_type<float>(v22));
    ((device ushort*)p2)[i] = (ushort)v23;

    uint v25 = v4 >> 16u;
    uint v26 = v1 & v25;
    uint v27 = as_type<uint>((float)(int)v26);
    uint v28 = (uint)((device ushort*)p3)[i];
    uint v29 = as_type<uint>((float)as_type<half>((ushort)v28));
    uint v30 = as_type<uint>(as_type<float>(v29) * as_type<float>(v11));
    uint v31 = as_type<uint>(fma(as_type<float>(v2), as_type<float>(v27), as_type<float>(v30)));
    uint v32 = (uint)as_type<ushort>((half)as_type<float>(v31));
    ((device ushort*)p3)[i] = (ushort)v32;

    uint v34 = (uint)((device ushort*)p4)[i];
    uint v35 = as_type<uint>((float)as_type<half>((ushort)v34));
    uint v36 = as_type<uint>(as_type<float>(v35) * as_type<float>(v11));
    uint v37 = as_type<uint>(fma(as_type<float>(v2), as_type<float>(v10), as_type<float>(v36)));
    uint v38 = (uint)as_type<ushort>((half)as_type<float>(v37));
    ((device ushort*)p4)[i] = (ushort)v38;
}
