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
    uint v6 = 998277249u;
    uint v7 = 255u;
    uint v8 = 1065353216u;
    uint v9 = as_type<uint>(as_type<float>(v8) - as_type<float>(v4));
    uint v10 = 1132396544u;
    uint v11 = (uint)((device ushort*)p2)[i];
    uint v12 = (uint)(int)(short)(ushort)v11;
    uint v13 = as_type<uint>((float)(int)v12);
    uint v14 = as_type<uint>(as_type<float>(v13) * as_type<float>(998277249u));
    uint v15 = ((device uint*)p1)[i];
    uint v16 = v15 >> 24u;
    uint v17 = as_type<uint>((float)(int)v16);
    uint v18 = as_type<uint>(as_type<float>(v17) * as_type<float>(998277249u));
    uint v19 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v9), as_type<float>(v4)));
    uint v20 = as_type<uint>(as_type<float>(v19) - as_type<float>(v18));
    uint v21 = as_type<uint>(fma(as_type<float>(v14), as_type<float>(v20), as_type<float>(v18)));
    uint v22 = as_type<uint>(as_type<float>(v21) * as_type<float>(1132396544u));
    uint v23 = v15 >> 8u;
    uint v24 = v23 & 255u;
    uint v25 = as_type<uint>((float)(int)v24);
    uint v26 = as_type<uint>(as_type<float>(v25) * as_type<float>(998277249u));
    uint v27 = as_type<uint>(fma(as_type<float>(v26), as_type<float>(v9), as_type<float>(v2)));
    uint v28 = as_type<uint>(as_type<float>(v27) - as_type<float>(v26));
    uint v29 = as_type<uint>(fma(as_type<float>(v14), as_type<float>(v28), as_type<float>(v26)));
    uint v30 = as_type<uint>(as_type<float>(v29) * as_type<float>(1132396544u));
    uint v31 = as_type<uint>((int)rint(as_type<float>(v30)));
    uint v32 = v31 & 255u;
    uint v33 = v15 >> 16u;
    uint v34 = v33 & 255u;
    uint v35 = as_type<uint>((float)(int)v34);
    uint v36 = as_type<uint>(as_type<float>(v35) * as_type<float>(998277249u));
    uint v37 = as_type<uint>(fma(as_type<float>(v36), as_type<float>(v9), as_type<float>(v3)));
    uint v38 = as_type<uint>(as_type<float>(v37) - as_type<float>(v36));
    uint v39 = as_type<uint>(fma(as_type<float>(v14), as_type<float>(v38), as_type<float>(v36)));
    uint v40 = as_type<uint>(as_type<float>(v39) * as_type<float>(1132396544u));
    uint v41 = as_type<uint>((int)rint(as_type<float>(v40)));
    uint v42 = v41 & 255u;
    uint v43 = as_type<uint>((int)rint(as_type<float>(v22)));
    uint v44 = v15 & 255u;
    uint v45 = as_type<uint>((float)(int)v44);
    uint v46 = as_type<uint>(as_type<float>(v45) * as_type<float>(998277249u));
    uint v47 = as_type<uint>(fma(as_type<float>(v46), as_type<float>(v9), as_type<float>(v1)));
    uint v48 = as_type<uint>(as_type<float>(v47) - as_type<float>(v46));
    uint v49 = as_type<uint>(fma(as_type<float>(v14), as_type<float>(v48), as_type<float>(v46)));
    uint v50 = as_type<uint>(as_type<float>(v49) * as_type<float>(1132396544u));
    uint v51 = as_type<uint>((int)rint(as_type<float>(v50)));
    uint v52 = v51 & 255u;
    uint v53 = v52 | (v32 << 8u);
    uint v54 = v53 | (v42 << 16u);
    uint v55 = v54 | (v43 << 24u);
    ((device uint*)p1)[i] = v55;
}
