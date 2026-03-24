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
    uint v22 = as_type<uint>(as_type<float>(v21) - as_type<float>(v2));
    uint v23 = pos.y;
    uint v24 = v23 * v1;
    uint v25 = as_type<uint>((float)(int)v23);
    uint v26 = as_type<uint>(as_type<float>(v25) - as_type<float>(v3));
    uint v27 = as_type<uint>(as_type<float>(v26) * as_type<float>(v26));
    uint v28 = as_type<uint>(fma(as_type<float>(v22), as_type<float>(v22), as_type<float>(v27)));
    uint v29 = as_type<uint>(sqrt(as_type<float>(v28)));
    uint v30 = as_type<uint>(as_type<float>(v4) * as_type<float>(v29));
    uint v31 = v20 + v24;
    uint v32 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v30)));
    uint v33 = as_type<uint>(min(as_type<float>(v32), as_type<float>(v5)));
    uint v34 = as_type<uint>(fma(as_type<float>(v33), as_type<float>(v17), as_type<float>(v9)));
    uint v35 = as_type<uint>(fma(as_type<float>(v33), as_type<float>(v14), as_type<float>(v6)));
    uint v36 = as_type<uint>(fma(as_type<float>(v33), as_type<float>(v15), as_type<float>(v7)));
    uint v37 = as_type<uint>(fma(as_type<float>(v33), as_type<float>(v16), as_type<float>(v8)));
    uint v38 = as_type<uint>(as_type<float>(v34) * as_type<float>(v18));
    uint v39 = as_type<uint>((int)rint(as_type<float>(v38)));
    uint v40 = as_type<uint>(as_type<float>(v35) * as_type<float>(v18));
    uint v41 = as_type<uint>((int)rint(as_type<float>(v40)));
    uint v42 = as_type<uint>(as_type<float>(v36) * as_type<float>(v18));
    uint v43 = as_type<uint>((int)rint(as_type<float>(v42)));
    uint v44 = as_type<uint>(as_type<float>(v37) * as_type<float>(v18));
    uint v45 = as_type<uint>((int)rint(as_type<float>(v44)));
    uint v46 = v45 & v19;
    uint v47 = v41 & v19;
    uint v48 = v43 & v19;
    uint v49 = v48 << 8u;
    uint v50 = v47 | v49;
    uint v51 = v46 << 16u;
    uint v52 = v50 | v51;
    uint v53 = v39 << 24u;
    uint v54 = v52 | v53;
    ((device uint*)p0)[clamp_ix((int)v31,buf_szs[0],4)] = v54;
}
