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
    uint v7 = as_type<uint>(as_type<float>(v2) * as_type<float>(v6));
    uint v8 = (uint)((device ushort*)p1)[i];
    uint v9 = as_type<uint>((float)as_type<half>((ushort)v8));
    uint v10 = v4 >> 24u;
    uint v11 = as_type<uint>((float)(int)v10);
    uint v12 = as_type<uint>(as_type<float>(v2) * as_type<float>(v11));
    uint v13 = as_type<uint>(as_type<float>(v3) - as_type<float>(v12));
    uint v14 = as_type<uint>(fma(as_type<float>(v9), as_type<float>(v13), as_type<float>(v7)));
    uint v15 = (uint)as_type<ushort>((half)as_type<float>(v14));
    ((device ushort*)p1)[i] = (ushort)v15;

    uint v17 = v4 >> 8u;
    uint v18 = v1 & v17;
    uint v19 = as_type<uint>((float)(int)v18);
    uint v20 = as_type<uint>(as_type<float>(v2) * as_type<float>(v19));
    uint v21 = (uint)((device ushort*)p2)[i];
    uint v22 = as_type<uint>((float)as_type<half>((ushort)v21));
    uint v23 = as_type<uint>(fma(as_type<float>(v22), as_type<float>(v13), as_type<float>(v20)));
    uint v24 = (uint)as_type<ushort>((half)as_type<float>(v23));
    ((device ushort*)p2)[i] = (ushort)v24;

    uint v26 = v4 >> 16u;
    uint v27 = v1 & v26;
    uint v28 = as_type<uint>((float)(int)v27);
    uint v29 = as_type<uint>(as_type<float>(v2) * as_type<float>(v28));
    uint v30 = (uint)((device ushort*)p3)[i];
    uint v31 = as_type<uint>((float)as_type<half>((ushort)v30));
    uint v32 = as_type<uint>(fma(as_type<float>(v31), as_type<float>(v13), as_type<float>(v29)));
    uint v33 = (uint)as_type<ushort>((half)as_type<float>(v32));
    ((device ushort*)p3)[i] = (ushort)v33;

    uint v35 = (uint)((device ushort*)p4)[i];
    uint v36 = as_type<uint>((float)as_type<half>((ushort)v35));
    uint v37 = as_type<uint>(fma(as_type<float>(v36), as_type<float>(v13), as_type<float>(v12)));
    uint v38 = (uint)as_type<ushort>((half)as_type<float>(v37));
    ((device ushort*)p4)[i] = (ushort)v38;
}
