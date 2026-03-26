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
    constant uint &w [[buffer(5)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    device uchar *p2 [[buffer(3)]],
    constant uint *buf_szs [[buffer(4)]],
    uint2 pos [[thread_position_in_grid]]
) {
    uint i = pos.y * w + pos.x;
    if (i >= n) return;
    uint v0 = 0u;
    uint v1 = ((device const uint*)p0)[0];
    uint v2 = ((device const uint*)p0)[1];
    uint v3 = ((device const uint*)p0)[2];
    uint v4 = ((device const uint*)p0)[3];
    uint v6 = 1065353216u;
    uint v7 = as_type<uint>(as_type<float>(v6) - as_type<float>(v4));
    uint v8 = (uint)((device ushort*)p2)[i];
    uint v9 = (uint)(int)(short)(ushort)v8;
    uint v10 = as_type<uint>((float)(int)v9);
    uint v11 = as_type<uint>(as_type<float>(v10) * as_type<float>(998277249u));
    uint v12 = ((device uint*)p1)[i];
    uint v13 = v12 >> 24u;
    uint v14 = as_type<uint>((float)(int)v13);
    uint v15 = as_type<uint>(as_type<float>(v14) * as_type<float>(998277249u));
    uint v16 = as_type<uint>(fma(as_type<float>(v15), as_type<float>(v7), as_type<float>(v4)));
    uint v17 = as_type<uint>(as_type<float>(v16) - as_type<float>(v15));
    uint v18 = as_type<uint>(fma(as_type<float>(v11), as_type<float>(v17), as_type<float>(v15)));
    uint v19 = as_type<uint>(as_type<float>(v18) * as_type<float>(1132396544u));
    uint v20 = v12 >> 8u;
    uint v21 = v20 & 255u;
    uint v22 = as_type<uint>((float)(int)v21);
    uint v23 = as_type<uint>(as_type<float>(v22) * as_type<float>(998277249u));
    uint v24 = as_type<uint>(fma(as_type<float>(v23), as_type<float>(v7), as_type<float>(v2)));
    uint v25 = as_type<uint>(as_type<float>(v24) - as_type<float>(v23));
    uint v26 = as_type<uint>(fma(as_type<float>(v11), as_type<float>(v25), as_type<float>(v23)));
    uint v27 = as_type<uint>(as_type<float>(v26) * as_type<float>(1132396544u));
    uint v28 = as_type<uint>((int)rint(as_type<float>(v27)));
    uint v29 = v12 >> 16u;
    uint v30 = v29 & 255u;
    uint v31 = as_type<uint>((float)(int)v30);
    uint v32 = as_type<uint>(as_type<float>(v31) * as_type<float>(998277249u));
    uint v33 = as_type<uint>(fma(as_type<float>(v32), as_type<float>(v7), as_type<float>(v3)));
    uint v34 = as_type<uint>(as_type<float>(v33) - as_type<float>(v32));
    uint v35 = as_type<uint>(fma(as_type<float>(v11), as_type<float>(v34), as_type<float>(v32)));
    uint v36 = as_type<uint>(as_type<float>(v35) * as_type<float>(1132396544u));
    uint v37 = v28 & 255u;
    uint v38 = as_type<uint>((int)rint(as_type<float>(v36)));
    uint v39 = v12 & 255u;
    uint v40 = as_type<uint>((float)(int)v39);
    uint v41 = as_type<uint>(as_type<float>(v40) * as_type<float>(998277249u));
    uint v42 = as_type<uint>(fma(as_type<float>(v41), as_type<float>(v7), as_type<float>(v1)));
    uint v43 = as_type<uint>(as_type<float>(v42) - as_type<float>(v41));
    uint v44 = as_type<uint>(fma(as_type<float>(v11), as_type<float>(v43), as_type<float>(v41)));
    uint v45 = as_type<uint>(as_type<float>(v44) * as_type<float>(1132396544u));
    uint v46 = as_type<uint>((int)rint(as_type<float>(v45)));
    uint v47 = v46 & 255u;
    uint v48 = v47 | (v37 << 8u);
    uint v49 = v38 & 255u;
    uint v50 = v48 | (v49 << 16u);
    uint v51 = as_type<uint>((int)rint(as_type<float>(v19)));
    uint v52 = v50 | (v51 << 24u);
    ((device uint*)p1)[i] = v52;
}
