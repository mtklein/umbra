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
    uint v1 = 998277249u;
    uint v2 = 1065353216u;
    uint v3 = (uint)((device uchar*)p0)[i*4+0];
    uint v4 = as_type<uint>((float)(int)v3);
    uint v5 = (uint)(int)((device short*)p1)[i];
    uint v6 = as_type<uint>((float)as_type<half>((ushort)v5));
    uint v7 = (uint)((device uchar*)p0)[i*4+3];
    uint v8 = as_type<uint>((float)(int)v7);
    uint v9 = as_type<uint>(as_type<float>(v2) - as_type<float>(v1) * as_type<float>(v8));
    uint v10 = as_type<uint>(as_type<float>(v6) * as_type<float>(v9));
    uint v11 = as_type<uint>(fma(as_type<float>(v1), as_type<float>(v4), as_type<float>(v10)));
    uint v12 = (uint)as_type<ushort>((half)as_type<float>(v11));
    ((device ushort*)p1)[i] = (ushort)v12;

    uint v14 = (uint)((device uchar*)p0)[i*4+1];
    uint v15 = as_type<uint>((float)(int)v14);
    uint v16 = (uint)(int)((device short*)p2)[i];
    uint v17 = as_type<uint>((float)as_type<half>((ushort)v16));
    uint v18 = as_type<uint>(as_type<float>(v17) * as_type<float>(v9));
    uint v19 = as_type<uint>(fma(as_type<float>(v1), as_type<float>(v15), as_type<float>(v18)));
    uint v20 = (uint)as_type<ushort>((half)as_type<float>(v19));
    ((device ushort*)p2)[i] = (ushort)v20;

    uint v22 = (uint)((device uchar*)p0)[i*4+2];
    uint v23 = as_type<uint>((float)(int)v22);
    uint v24 = (uint)(int)((device short*)p3)[i];
    uint v25 = as_type<uint>((float)as_type<half>((ushort)v24));
    uint v26 = as_type<uint>(as_type<float>(v25) * as_type<float>(v9));
    uint v27 = as_type<uint>(fma(as_type<float>(v1), as_type<float>(v23), as_type<float>(v26)));
    uint v28 = (uint)as_type<ushort>((half)as_type<float>(v27));
    ((device ushort*)p3)[i] = (ushort)v28;

    uint v30 = (uint)(int)((device short*)p4)[i];
    uint v31 = as_type<uint>((float)as_type<half>((ushort)v30));
    uint v32 = as_type<uint>(as_type<float>(v31) * as_type<float>(v9));
    uint v33 = as_type<uint>(fma(as_type<float>(v1), as_type<float>(v8), as_type<float>(v32)));
    uint v34 = (uint)as_type<ushort>((half)as_type<float>(v33));
    ((device ushort*)p4)[i] = (ushort)v34;
}
