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
    uint v1 = 1065353216u;
    uint v2 = ((device uint*)p0)[i];
    uint v3 = v2 >> 24u;
    uint v4 = as_type<uint>((float)(int)v3);
    uint v5 = as_type<uint>(as_type<float>(v4) * as_type<float>(998277249u));
    uint v6 = as_type<uint>(as_type<float>(v1) - as_type<float>(v5));
    uint v7 = v2 >> 8u;
    uint v8 = v7 & 255u;
    uint v9 = as_type<uint>((float)(int)v8);
    uint v10 = as_type<uint>(as_type<float>(v9) * as_type<float>(998277249u));
    uint v11 = v2 >> 16u;
    uint v12 = v11 & 255u;
    uint v13 = as_type<uint>((float)(int)v12);
    uint v14 = as_type<uint>(as_type<float>(v13) * as_type<float>(998277249u));
    uint v15 = v2 & 255u;
    uint v16 = as_type<uint>((float)(int)v15);
    uint v17 = as_type<uint>(as_type<float>(v16) * as_type<float>(998277249u));
    uint v18 = (uint)((device ushort*)p1)[i];
    uint v19 = as_type<uint>((float)as_type<half>((ushort)v18));
    uint v20 = as_type<uint>(fma(as_type<float>(v19), as_type<float>(v6), as_type<float>(v17)));
    uint v21 = (uint)as_type<ushort>((half)as_type<float>(v20));
    ((device ushort*)p1)[i] = (ushort)v21;

    uint v23 = (uint)((device ushort*)p2)[i];
    uint v24 = as_type<uint>((float)as_type<half>((ushort)v23));
    uint v25 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v6), as_type<float>(v10)));
    uint v26 = (uint)as_type<ushort>((half)as_type<float>(v25));
    ((device ushort*)p2)[i] = (ushort)v26;

    uint v28 = (uint)((device ushort*)p3)[i];
    uint v29 = as_type<uint>((float)as_type<half>((ushort)v28));
    uint v30 = as_type<uint>(fma(as_type<float>(v29), as_type<float>(v6), as_type<float>(v14)));
    uint v31 = (uint)as_type<ushort>((half)as_type<float>(v30));
    ((device ushort*)p3)[i] = (ushort)v31;

    uint v33 = (uint)((device ushort*)p4)[i];
    uint v34 = as_type<uint>((float)as_type<half>((ushort)v33));
    uint v35 = as_type<uint>(fma(as_type<float>(v34), as_type<float>(v6), as_type<float>(v5)));
    uint v36 = (uint)as_type<ushort>((half)as_type<float>(v35));
    ((device ushort*)p4)[i] = (ushort)v36;
}
