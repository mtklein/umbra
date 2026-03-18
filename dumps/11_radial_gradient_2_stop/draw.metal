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
    constant uint *buf_szs [[buffer(3)]],
    uint i [[thread_position_in_grid]]
) {
    if (i >= n) return;
    uint v0 = 0u;
    uint v1 = ((device const uint*)p1)[0];
    uint v2 = ((device const uint*)p1)[1];
    uint v3 = as_type<uint>((float)(int)v2);
    uint v4 = ((device const uint*)p1)[2];
    uint v5 = ((device const uint*)p1)[3];
    uint v6 = ((device const uint*)p1)[4];
    uint v7 = as_type<uint>(as_type<float>(v3) - as_type<float>(v5));
    uint v8 = as_type<uint>(as_type<float>(v7) * as_type<float>(v7));
    uint v9 = 1065353216u;
    uint v10 = ((device const uint*)p1)[5];
    uint v11 = ((device const uint*)p1)[6];
    uint v12 = ((device const uint*)p1)[7];
    uint v13 = ((device const uint*)p1)[8];
    uint v14 = ((device const uint*)p1)[9];
    uint v15 = ((device const uint*)p1)[10];
    uint v16 = ((device const uint*)p1)[11];
    uint v17 = ((device const uint*)p1)[12];
    uint v18 = as_type<uint>(as_type<float>(v14) - as_type<float>(v10));
    uint v19 = as_type<uint>(as_type<float>(v15) - as_type<float>(v11));
    uint v20 = as_type<uint>(as_type<float>(v16) - as_type<float>(v12));
    uint v21 = as_type<uint>(as_type<float>(v17) - as_type<float>(v13));
    uint v22 = 1132396544u;
    uint v23 = 1056964608u;
    uint v24 = 255u;
    uint v25 = i;
    uint v26 = v25 + v1;
    uint v27 = as_type<uint>((float)(int)v26);
    uint v28 = as_type<uint>(as_type<float>(v27) - as_type<float>(v4));
    uint v29 = as_type<uint>(fma(as_type<float>(v28), as_type<float>(v28), as_type<float>(v8)));
    uint v30 = as_type<uint>(sqrt(as_type<float>(v29)));
    uint v31 = as_type<uint>(as_type<float>(v6) * as_type<float>(v30));
    uint v32 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v31)));
    uint v33 = as_type<uint>(min(as_type<float>(v32), as_type<float>(v9)));
    uint v34 = as_type<uint>(fma(as_type<float>(v33), as_type<float>(v18), as_type<float>(v10)));
    uint v35 = as_type<uint>(fma(as_type<float>(v34), as_type<float>(v22), as_type<float>(v23)));
    uint v36 = (uint)(int)as_type<float>(v35);
    uint v37 = v36 & v24;
    uint v38 = as_type<uint>(fma(as_type<float>(v33), as_type<float>(v19), as_type<float>(v11)));
    uint v39 = as_type<uint>(fma(as_type<float>(v38), as_type<float>(v22), as_type<float>(v23)));
    uint v40 = (uint)(int)as_type<float>(v39);
    uint v41 = v40 & v24;
    uint v42 = v41 << 8u;
    uint v43 = v37 | v42;
    uint v44 = as_type<uint>(fma(as_type<float>(v33), as_type<float>(v20), as_type<float>(v12)));
    uint v45 = as_type<uint>(fma(as_type<float>(v44), as_type<float>(v22), as_type<float>(v23)));
    uint v46 = (uint)(int)as_type<float>(v45);
    uint v47 = v46 & v24;
    uint v48 = v47 << 16u;
    uint v49 = v43 | v48;
    uint v50 = as_type<uint>(fma(as_type<float>(v33), as_type<float>(v21), as_type<float>(v13)));
    uint v51 = as_type<uint>(fma(as_type<float>(v50), as_type<float>(v22), as_type<float>(v23)));
    uint v52 = (uint)(int)as_type<float>(v51);
    uint v53 = v52 << 24u;
    uint v54 = v49 | v53;
    ((device uint*)p0)[i] = v54;
}
