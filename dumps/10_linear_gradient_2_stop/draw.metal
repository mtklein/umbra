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
    uint v5 = 1065353216u;
    uint v6 = ((device const uint*)p1)[5];
    uint v7 = ((device const uint*)p1)[6];
    uint v8 = ((device const uint*)p1)[7];
    uint v9 = ((device const uint*)p1)[8];
    uint v10 = ((device const uint*)p1)[9];
    uint v11 = ((device const uint*)p1)[10];
    uint v12 = ((device const uint*)p1)[11];
    uint v13 = ((device const uint*)p1)[12];
    uint v14 = as_type<uint>(as_type<float>(v10) - as_type<float>(v6));
    uint v15 = as_type<uint>(as_type<float>(v11) - as_type<float>(v7));
    uint v16 = as_type<uint>(as_type<float>(v12) - as_type<float>(v8));
    uint v17 = as_type<uint>(as_type<float>(v13) - as_type<float>(v9));
    uint v18 = 1132396544u;
    uint v19 = 255u;
    uint v20 = pos.x;
    uint v21 = as_type<uint>((float)(int)v20);
    uint v22 = pos.y;
    uint v23 = v22 * v1;
    uint v24 = as_type<uint>((float)(int)v22);
    uint v25 = as_type<uint>(as_type<float>(v24) * as_type<float>(v3));
    uint v26 = as_type<uint>(fma(as_type<float>(v21), as_type<float>(v2), as_type<float>(v25)));
    uint v27 = as_type<uint>(as_type<float>(v4) + as_type<float>(v26));
    uint v28 = v20 + v23;
    uint v29 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v27)));
    uint v30 = as_type<uint>(min(as_type<float>(v29), as_type<float>(v5)));
    uint v31 = as_type<uint>(fma(as_type<float>(v30), as_type<float>(v17), as_type<float>(v9)));
    uint v32 = as_type<uint>(fma(as_type<float>(v30), as_type<float>(v14), as_type<float>(v6)));
    uint v33 = as_type<uint>(fma(as_type<float>(v30), as_type<float>(v15), as_type<float>(v7)));
    uint v34 = as_type<uint>(fma(as_type<float>(v30), as_type<float>(v16), as_type<float>(v8)));
    uint v35 = as_type<uint>(as_type<float>(v31) * as_type<float>(v18));
    uint v36 = as_type<uint>((int)rint(as_type<float>(v35)));
    uint v37 = as_type<uint>(as_type<float>(v32) * as_type<float>(v18));
    uint v38 = as_type<uint>((int)rint(as_type<float>(v37)));
    uint v39 = as_type<uint>(as_type<float>(v33) * as_type<float>(v18));
    uint v40 = as_type<uint>((int)rint(as_type<float>(v39)));
    uint v41 = as_type<uint>(as_type<float>(v34) * as_type<float>(v18));
    uint v42 = as_type<uint>((int)rint(as_type<float>(v41)));
    uint v43 = v42 & v19;
    uint v44 = v38 & v19;
    uint v45 = v40 & v19;
    uint v46 = v45 << 8u;
    uint v47 = v44 | v46;
    uint v48 = v43 << 16u;
    uint v49 = v47 | v48;
    uint v50 = v36 << 24u;
    uint v51 = v49 | v50;
    ((device uint*)p0)[clamp_ix((int)v28,buf_szs[0],4)] = v51;
}
