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
    uint v6 = ((device const uint*)p1)[6];
    uint v7 = ((device const uint*)p1)[7];
    uint v8 = ((device const uint*)p1)[8];
    uint v9 = ((device const uint*)p1)[9];
    uint v10 = 1065353216u;
    uint v11 = 255u;
    uint v12 = 998277249u;
    uint v13 = 1132396544u;
    uint v14 = pos.x;
    uint v15 = as_type<uint>((float)(int)v14);
    uint v16 = as_type<float>(v15) <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v17 = as_type<float>(v6) <= as_type<float>(v15) ? 0xffffffffu : 0u;
    uint v18 = v17 & v16;
    uint v19 = pos.y;
    uint v20 = v19 * v1;
    uint v21 = as_type<uint>((float)(int)v19);
    uint v22 = as_type<float>(v21) <  as_type<float>(v9) ? 0xffffffffu : 0u;
    uint v23 = as_type<float>(v7) <= as_type<float>(v21) ? 0xffffffffu : 0u;
    uint v24 = v23 & v22;
    uint v25 = v18 & v24;
    uint v26 = (v25 & v10) | (~v25 & v0);
    uint v27 = v14 + v20;
    uint v28 = ((device uint*)p0)[clamp_ix((int)v27,buf_szs[0],4)];
    uint v29 = v28 >> 24u;
    uint v30 = as_type<uint>((float)(int)v29);
    uint v31 = as_type<uint>(as_type<float>(v12) * as_type<float>(v30));
    uint v32 = as_type<uint>(as_type<float>(v5) - as_type<float>(v31));
    uint v33 = as_type<uint>(fma(as_type<float>(v26), as_type<float>(v32), as_type<float>(v31)));
    uint v34 = as_type<uint>(as_type<float>(v33) * as_type<float>(v13));
    uint v35 = as_type<uint>((int)rint(as_type<float>(v34)));
    uint v36 = v28 & v11;
    uint v37 = as_type<uint>((float)(int)v36);
    uint v38 = v28 >> 8u;
    uint v39 = v11 & v38;
    uint v40 = as_type<uint>((float)(int)v39);
    uint v41 = v28 >> 16u;
    uint v42 = v11 & v41;
    uint v43 = as_type<uint>((float)(int)v42);
    uint v44 = as_type<uint>(as_type<float>(v12) * as_type<float>(v37));
    uint v45 = as_type<uint>(as_type<float>(v2) - as_type<float>(v44));
    uint v46 = as_type<uint>(fma(as_type<float>(v26), as_type<float>(v45), as_type<float>(v44)));
    uint v47 = as_type<uint>(as_type<float>(v12) * as_type<float>(v40));
    uint v48 = as_type<uint>(as_type<float>(v3) - as_type<float>(v47));
    uint v49 = as_type<uint>(fma(as_type<float>(v26), as_type<float>(v48), as_type<float>(v47)));
    uint v50 = as_type<uint>(as_type<float>(v12) * as_type<float>(v43));
    uint v51 = as_type<uint>(as_type<float>(v4) - as_type<float>(v50));
    uint v52 = as_type<uint>(fma(as_type<float>(v26), as_type<float>(v51), as_type<float>(v50)));
    uint v53 = as_type<uint>(as_type<float>(v46) * as_type<float>(v13));
    uint v54 = as_type<uint>((int)rint(as_type<float>(v53)));
    uint v55 = as_type<uint>(as_type<float>(v49) * as_type<float>(v13));
    uint v56 = as_type<uint>((int)rint(as_type<float>(v55)));
    uint v57 = as_type<uint>(as_type<float>(v52) * as_type<float>(v13));
    uint v58 = as_type<uint>((int)rint(as_type<float>(v57)));
    uint v59 = v11 & v58;
    uint v60 = v11 & v54;
    uint v61 = v11 & v56;
    uint v62 = v61 << 8u;
    uint v63 = v60 | v62;
    uint v64 = v59 << 16u;
    uint v65 = v63 | v64;
    uint v66 = v35 << 24u;
    uint v67 = v65 | v66;
    ((device uint*)p0)[clamp_ix((int)v27,buf_szs[0],4)] = v67;
}
