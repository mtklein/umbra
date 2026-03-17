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
    uint v2 = 8u;
    uint v3 = 16u;
    uint v4 = 24u;
    uint v5 = 998277249u;
    uint v6 = 1065353216u;
    uint v7 = ((device uint*)p0)[i];
    uint v8 = v7 & v1;
    uint v9 = as_type<uint>((float)(int)v8);
    uint v10 = (uint)((device ushort*)p1)[i];
    uint v11 = as_type<uint>((float)as_type<half>((ushort)v10));
    uint v12 = v7 >> v4;
    uint v13 = as_type<uint>((float)(int)v12);
    uint v14 = as_type<uint>(as_type<float>(v6) - as_type<float>(v5) * as_type<float>(v13));
    uint v15 = as_type<uint>(as_type<float>(v11) * as_type<float>(v14));
    uint v16 = as_type<uint>(fma(as_type<float>(v5), as_type<float>(v9), as_type<float>(v15)));
    uint v17 = (uint)as_type<ushort>((half)as_type<float>(v16));
    ((device ushort*)p1)[i] = (ushort)v17;

    uint v19 = v7 >> v2;
    uint v20 = v1 & v19;
    uint v21 = as_type<uint>((float)(int)v20);
    uint v22 = (uint)((device ushort*)p2)[i];
    uint v23 = as_type<uint>((float)as_type<half>((ushort)v22));
    uint v24 = as_type<uint>(as_type<float>(v23) * as_type<float>(v14));
    uint v25 = as_type<uint>(fma(as_type<float>(v5), as_type<float>(v21), as_type<float>(v24)));
    uint v26 = (uint)as_type<ushort>((half)as_type<float>(v25));
    ((device ushort*)p2)[i] = (ushort)v26;

    uint v28 = v7 >> v3;
    uint v29 = v1 & v28;
    uint v30 = as_type<uint>((float)(int)v29);
    uint v31 = (uint)((device ushort*)p3)[i];
    uint v32 = as_type<uint>((float)as_type<half>((ushort)v31));
    uint v33 = as_type<uint>(as_type<float>(v32) * as_type<float>(v14));
    uint v34 = as_type<uint>(fma(as_type<float>(v5), as_type<float>(v30), as_type<float>(v33)));
    uint v35 = (uint)as_type<ushort>((half)as_type<float>(v34));
    ((device ushort*)p3)[i] = (ushort)v35;

    uint v37 = (uint)((device ushort*)p4)[i];
    uint v38 = as_type<uint>((float)as_type<half>((ushort)v37));
    uint v39 = as_type<uint>(as_type<float>(v38) * as_type<float>(v14));
    uint v40 = as_type<uint>(fma(as_type<float>(v5), as_type<float>(v13), as_type<float>(v39)));
    uint v41 = (uint)as_type<ushort>((half)as_type<float>(v40));
    ((device ushort*)p4)[i] = (ushort)v41;
}
