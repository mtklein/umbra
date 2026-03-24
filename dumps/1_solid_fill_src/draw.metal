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
    uint v2 = ((device const uint*)p1)[1];
    uint v3 = as_type<uint>((float)(int)v2);
    uint v4 = ((device const uint*)p1)[2];
    uint v5 = ((device const uint*)p1)[3];
    uint v6 = ((device const uint*)p1)[4];
    uint v7 = ((device const uint*)p1)[5];
    uint v8 = ((device const uint*)p1)[6];
    uint v9 = ((device const uint*)p1)[7];
    uint v10 = ((device const uint*)p1)[8];
    uint v11 = ((device const uint*)p1)[9];
    uint v12 = as_type<float>(v9) <= as_type<float>(v3) ? 0xffffffffu : 0u;
    uint v13 = as_type<float>(v3) <  as_type<float>(v11) ? 0xffffffffu : 0u;
    uint v14 = v12 & v13;
    uint v15 = 1065353216u;
    uint v16 = 255u;
    uint v17 = 998277249u;
    uint v18 = 1132396544u;
    uint v19 = i;
    uint v20 = v19 + v1;
    uint v21 = as_type<uint>((float)(int)v20);
    uint v22 = as_type<float>(v21) <  as_type<float>(v10) ? 0xffffffffu : 0u;
    uint v23 = as_type<float>(v8) <= as_type<float>(v21) ? 0xffffffffu : 0u;
    uint v24 = v23 & v22;
    uint v25 = v24 & v14;
    uint v26 = (v25 & v15) | (~v25 & v0);
    uint v27 = ((device uint*)p0)[i];
    uint v28 = v27 >> 24u;
    uint v29 = as_type<uint>((float)(int)v28);
    uint v30 = as_type<uint>(as_type<float>(v17) * as_type<float>(v29));
    uint v31 = as_type<uint>(as_type<float>(v7) - as_type<float>(v30));
    uint v32 = as_type<uint>(fma(as_type<float>(v26), as_type<float>(v31), as_type<float>(v30)));
    uint v33 = as_type<uint>(as_type<float>(v32) * as_type<float>(v18));
    uint v34 = as_type<uint>((int)rint(as_type<float>(v33)));
    uint v35 = v27 & v16;
    uint v36 = as_type<uint>((float)(int)v35);
    uint v37 = v27 >> 8u;
    uint v38 = v16 & v37;
    uint v39 = as_type<uint>((float)(int)v38);
    uint v40 = v27 >> 16u;
    uint v41 = v16 & v40;
    uint v42 = as_type<uint>((float)(int)v41);
    uint v43 = as_type<uint>(as_type<float>(v17) * as_type<float>(v36));
    uint v44 = as_type<uint>(as_type<float>(v4) - as_type<float>(v43));
    uint v45 = as_type<uint>(fma(as_type<float>(v26), as_type<float>(v44), as_type<float>(v43)));
    uint v46 = as_type<uint>(as_type<float>(v17) * as_type<float>(v39));
    uint v47 = as_type<uint>(as_type<float>(v5) - as_type<float>(v46));
    uint v48 = as_type<uint>(fma(as_type<float>(v26), as_type<float>(v47), as_type<float>(v46)));
    uint v49 = as_type<uint>(as_type<float>(v17) * as_type<float>(v42));
    uint v50 = as_type<uint>(as_type<float>(v6) - as_type<float>(v49));
    uint v51 = as_type<uint>(fma(as_type<float>(v26), as_type<float>(v50), as_type<float>(v49)));
    uint v52 = as_type<uint>(as_type<float>(v45) * as_type<float>(v18));
    uint v53 = as_type<uint>((int)rint(as_type<float>(v52)));
    uint v54 = as_type<uint>(as_type<float>(v48) * as_type<float>(v18));
    uint v55 = as_type<uint>((int)rint(as_type<float>(v54)));
    uint v56 = as_type<uint>(as_type<float>(v51) * as_type<float>(v18));
    uint v57 = as_type<uint>((int)rint(as_type<float>(v56)));
    uint v58 = v16 & v57;
    uint v59 = v16 & v53;
    uint v60 = v16 & v55;
    uint v61 = v60 << 8u;
    uint v62 = v59 | v61;
    uint v63 = v58 << 16u;
    uint v64 = v62 | v63;
    uint v65 = v34 << 24u;
    uint v66 = v64 | v65;
    ((device uint*)p0)[i] = v66;
}
