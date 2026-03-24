#include <metal_stdlib>
using namespace metal;

static inline int clamp_ix(int ix, uint bytes, int elem) {
    int hi = (int)(bytes / (uint)elem) - 1;
    if (hi < 0) hi = 0;
    return clamp(ix, 0, hi);
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
    uint v1 = ((device const uint*)p1)[2];
    uint v2 = ((device const uint*)p1)[3];
    uint v3 = ((device const uint*)p1)[4];
    uint v4 = ((device const uint*)p1)[5];
    uint v6 = 998277249u;
    uint v7 = 255u;
    uint v8 = 1065353216u;
    uint v9 = as_type<uint>(as_type<float>(v8) - as_type<float>(v4));
    uint v10 = 1132396544u;
    uint v11 = (uint)((device ushort*)p2)[i];
    uint v12 = (uint)(int)(short)(ushort)v11;
    uint v13 = as_type<uint>((float)(int)v12);
    uint v14 = as_type<uint>(as_type<float>(v6) * as_type<float>(v13));
    uint v15 = ((device uint*)p0)[i];
    uint v16 = v15 >> 24u;
    uint v17 = as_type<uint>((float)(int)v16);
    uint v18 = as_type<uint>(as_type<float>(v6) * as_type<float>(v17));
    uint v19 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v9), as_type<float>(v4)));
    uint v20 = as_type<uint>(as_type<float>(v19) - as_type<float>(v18));
    uint v21 = as_type<uint>(fma(as_type<float>(v14), as_type<float>(v20), as_type<float>(v18)));
    uint v22 = as_type<uint>(as_type<float>(v21) * as_type<float>(v10));
    uint v23 = as_type<uint>((int)rint(as_type<float>(v22)));
    uint v24 = v15 & v7;
    uint v25 = as_type<uint>((float)(int)v24);
    uint v26 = v15 >> 8u;
    uint v27 = v7 & v26;
    uint v28 = as_type<uint>((float)(int)v27);
    uint v29 = v15 >> 16u;
    uint v30 = v7 & v29;
    uint v31 = as_type<uint>((float)(int)v30);
    uint v32 = as_type<uint>(as_type<float>(v6) * as_type<float>(v25));
    uint v33 = as_type<uint>(fma(as_type<float>(v32), as_type<float>(v9), as_type<float>(v1)));
    uint v34 = as_type<uint>(as_type<float>(v33) - as_type<float>(v32));
    uint v35 = as_type<uint>(fma(as_type<float>(v14), as_type<float>(v34), as_type<float>(v32)));
    uint v36 = as_type<uint>(as_type<float>(v6) * as_type<float>(v28));
    uint v37 = as_type<uint>(fma(as_type<float>(v36), as_type<float>(v9), as_type<float>(v2)));
    uint v38 = as_type<uint>(as_type<float>(v37) - as_type<float>(v36));
    uint v39 = as_type<uint>(fma(as_type<float>(v14), as_type<float>(v38), as_type<float>(v36)));
    uint v40 = as_type<uint>(as_type<float>(v6) * as_type<float>(v31));
    uint v41 = as_type<uint>(fma(as_type<float>(v40), as_type<float>(v9), as_type<float>(v3)));
    uint v42 = as_type<uint>(as_type<float>(v41) - as_type<float>(v40));
    uint v43 = as_type<uint>(fma(as_type<float>(v14), as_type<float>(v42), as_type<float>(v40)));
    uint v44 = as_type<uint>(as_type<float>(v35) * as_type<float>(v10));
    uint v45 = as_type<uint>((int)rint(as_type<float>(v44)));
    uint v46 = as_type<uint>(as_type<float>(v39) * as_type<float>(v10));
    uint v47 = as_type<uint>((int)rint(as_type<float>(v46)));
    uint v48 = as_type<uint>(as_type<float>(v43) * as_type<float>(v10));
    uint v49 = as_type<uint>((int)rint(as_type<float>(v48)));
    uint v50 = v7 & v49;
    uint v51 = v7 & v45;
    uint v52 = v7 & v47;
    uint v53 = v52 << 8u;
    uint v54 = v51 | v53;
    uint v55 = v50 << 16u;
    uint v56 = v54 | v55;
    uint v57 = v23 << 24u;
    uint v58 = v56 | v57;
    ((device uint*)p0)[i] = v58;
}
