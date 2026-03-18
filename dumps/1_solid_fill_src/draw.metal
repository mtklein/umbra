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
    uint v19 = 1056964608u;
    uint v20 = i;
    uint v21 = v20 + v1;
    uint v22 = as_type<uint>((float)(int)v21);
    uint v23 = as_type<float>(v8) <= as_type<float>(v22) ? 0xffffffffu : 0u;
    uint v24 = as_type<float>(v22) <  as_type<float>(v10) ? 0xffffffffu : 0u;
    uint v25 = v23 & v24;
    uint v26 = v25 & v14;
    uint v27 = (v26 & v15) | (~v26 & v0);
    uint v28 = ((device uint*)p0)[i];
    uint v29 = v28 & v16;
    uint v30 = as_type<uint>((float)(int)v29);
    uint v31 = as_type<uint>(as_type<float>(v17) * as_type<float>(v30));
    uint v32 = as_type<uint>(as_type<float>(v4) - as_type<float>(v31));
    uint v33 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v32), as_type<float>(v31)));
    uint v34 = as_type<uint>(as_type<float>(v33) * as_type<float>(v18));
    uint v35 = as_type<uint>(as_type<float>(v19) + as_type<float>(v34));
    uint v36 = (uint)(int)as_type<float>(v35);
    uint v37 = v16 & v36;
    uint v38 = v28 >> 8u;
    uint v39 = v16 & v38;
    uint v40 = as_type<uint>((float)(int)v39);
    uint v41 = as_type<uint>(as_type<float>(v17) * as_type<float>(v40));
    uint v42 = as_type<uint>(as_type<float>(v5) - as_type<float>(v41));
    uint v43 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v42), as_type<float>(v41)));
    uint v44 = as_type<uint>(as_type<float>(v43) * as_type<float>(v18));
    uint v45 = as_type<uint>(as_type<float>(v19) + as_type<float>(v44));
    uint v46 = (uint)(int)as_type<float>(v45);
    uint v47 = v16 & v46;
    uint v48 = v47 << 8u;
    uint v49 = v37 | v48;
    uint v50 = v28 >> 16u;
    uint v51 = v16 & v50;
    uint v52 = as_type<uint>((float)(int)v51);
    uint v53 = as_type<uint>(as_type<float>(v17) * as_type<float>(v52));
    uint v54 = as_type<uint>(as_type<float>(v6) - as_type<float>(v53));
    uint v55 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v54), as_type<float>(v53)));
    uint v56 = as_type<uint>(as_type<float>(v55) * as_type<float>(v18));
    uint v57 = as_type<uint>(as_type<float>(v19) + as_type<float>(v56));
    uint v58 = (uint)(int)as_type<float>(v57);
    uint v59 = v16 & v58;
    uint v60 = v59 << 16u;
    uint v61 = v49 | v60;
    uint v62 = v28 >> 24u;
    uint v63 = as_type<uint>((float)(int)v62);
    uint v64 = as_type<uint>(as_type<float>(v17) * as_type<float>(v63));
    uint v65 = as_type<uint>(as_type<float>(v7) - as_type<float>(v64));
    uint v66 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v65), as_type<float>(v64)));
    uint v67 = as_type<uint>(as_type<float>(v66) * as_type<float>(v18));
    uint v68 = as_type<uint>(as_type<float>(v19) + as_type<float>(v67));
    uint v69 = (uint)(int)as_type<float>(v68);
    uint v70 = v69 << 24u;
    uint v71 = v61 | v70;
    ((device uint*)p0)[i] = v71;
}
