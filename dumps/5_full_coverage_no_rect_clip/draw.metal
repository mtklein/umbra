#include <metal_stdlib>
using namespace metal;

static inline int clamp_ix(int ix, uint bytes, int elem) {
    int hi = (int)(bytes / (uint)elem) - 1;
    if (hi < 0) hi = 0;
    return clamp(ix, 0, hi);
}

kernel void umbra_entry(
    constant uint &n [[buffer(0)]],
    constant uint &w [[buffer(4)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    constant uint *buf_szs [[buffer(3)]],
    uint2 pos [[thread_position_in_grid]]
) {
    uint i = pos.y * w + pos.x;
    if (i >= n) return;
    uint v0 = 0u;
    uint v1 = ((device const uint*)p1)[0];
    uint v2 = ((device const uint*)p1)[2];
    uint v3 = ((device const uint*)p1)[3];
    uint v4 = ((device const uint*)p1)[4];
    uint v5 = ((device const uint*)p1)[5];
    uint v6 = 255u;
    uint v7 = 998277249u;
    uint v8 = 1065353216u;
    uint v9 = as_type<uint>(as_type<float>(v8) - as_type<float>(v5));
    uint v10 = 1132396544u;
    uint v11 = pos.y;
    uint v12 = v11 * v1;
    uint v13 = pos.x;
    uint v14 = v13 + v12;
    uint v15 = ((device uint*)p0)[clamp_ix((int)v14,buf_szs[0],4)];
    uint v16 = v15 >> 24u;
    uint v17 = as_type<uint>((float)(int)v16);
    uint v18 = as_type<uint>(as_type<float>(v7) * as_type<float>(v17));
    uint v19 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v9), as_type<float>(v5)));
    uint v20 = as_type<uint>(as_type<float>(v19) * as_type<float>(v10));
    uint v21 = as_type<uint>((int)rint(as_type<float>(v20)));
    uint v22 = v15 & v6;
    uint v23 = as_type<uint>((float)(int)v22);
    uint v24 = v15 >> 8u;
    uint v25 = v6 & v24;
    uint v26 = as_type<uint>((float)(int)v25);
    uint v27 = v15 >> 16u;
    uint v28 = v6 & v27;
    uint v29 = as_type<uint>((float)(int)v28);
    uint v30 = as_type<uint>(as_type<float>(v7) * as_type<float>(v23));
    uint v31 = as_type<uint>(fma(as_type<float>(v30), as_type<float>(v9), as_type<float>(v2)));
    uint v32 = as_type<uint>(as_type<float>(v7) * as_type<float>(v26));
    uint v33 = as_type<uint>(fma(as_type<float>(v32), as_type<float>(v9), as_type<float>(v3)));
    uint v34 = as_type<uint>(as_type<float>(v7) * as_type<float>(v29));
    uint v35 = as_type<uint>(fma(as_type<float>(v34), as_type<float>(v9), as_type<float>(v4)));
    uint v36 = as_type<uint>(as_type<float>(v31) * as_type<float>(v10));
    uint v37 = as_type<uint>((int)rint(as_type<float>(v36)));
    uint v38 = as_type<uint>(as_type<float>(v33) * as_type<float>(v10));
    uint v39 = as_type<uint>((int)rint(as_type<float>(v38)));
    uint v40 = as_type<uint>(as_type<float>(v35) * as_type<float>(v10));
    uint v41 = as_type<uint>((int)rint(as_type<float>(v40)));
    uint v42 = v6 & v41;
    uint v43 = v6 & v37;
    uint v44 = v6 & v39;
    uint v45 = v44 << 8u;
    uint v46 = v43 | v45;
    uint v47 = v42 << 16u;
    uint v48 = v46 | v47;
    uint v49 = v21 << 24u;
    uint v50 = v48 | v49;
    ((device uint*)p0)[clamp_ix((int)v14,buf_szs[0],4)] = v50;
}
