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
    uint v16 = buf_szs[0] >> 2u;
    uint v17 = 255u;
    uint v18 = 998277249u;
    uint v19 = 1132396544u;
    uint v20 = 1056964608u;
    uint v21 = 0xffffffffu;
    uint v22 = i;
    uint v23 = v22 <  v16 ? 0xffffffffu : 0u;
    uint v24 = v21 & v23;
    uint v25 = ((device uint*)p0)[i];
    uint v26 = v25 & v17;
    uint v27 = as_type<uint>((float)(int)v26);
    uint v28 = v22 + v1;
    uint v29 = as_type<uint>((float)(int)v28);
    uint v30 = as_type<float>(v8) <= as_type<float>(v29) ? 0xffffffffu : 0u;
    uint v31 = as_type<float>(v29) <  as_type<float>(v10) ? 0xffffffffu : 0u;
    uint v32 = v30 & v31;
    uint v33 = v32 & v14;
    uint v34 = (v33 & v15) | (~v33 & v0);
    uint v35 = as_type<uint>(as_type<float>(v4) - as_type<float>(v18) * as_type<float>(v27));
    uint v36 = as_type<uint>(as_type<float>(v34) * as_type<float>(v35));
    uint v37 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v27), as_type<float>(v36)));
    uint v38 = as_type<uint>(fma(as_type<float>(v37), as_type<float>(v19), as_type<float>(v20)));
    uint v39 = (uint)(int)as_type<float>(v38);
    uint v40 = v17 & v39;
    uint v41 = v25 >> 8u;
    uint v42 = v17 & v41;
    uint v43 = as_type<uint>((float)(int)v42);
    uint v44 = as_type<uint>(as_type<float>(v5) - as_type<float>(v18) * as_type<float>(v43));
    uint v45 = as_type<uint>(as_type<float>(v34) * as_type<float>(v44));
    uint v46 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v43), as_type<float>(v45)));
    uint v47 = as_type<uint>(fma(as_type<float>(v46), as_type<float>(v19), as_type<float>(v20)));
    uint v48 = (uint)(int)as_type<float>(v47);
    uint v49 = v17 & v48;
    uint v50 = v49 << 8u;
    uint v51 = v40 | v50;
    uint v52 = v25 >> 16u;
    uint v53 = v17 & v52;
    uint v54 = as_type<uint>((float)(int)v53);
    uint v55 = as_type<uint>(as_type<float>(v6) - as_type<float>(v18) * as_type<float>(v54));
    uint v56 = as_type<uint>(as_type<float>(v34) * as_type<float>(v55));
    uint v57 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v54), as_type<float>(v56)));
    uint v58 = as_type<uint>(fma(as_type<float>(v57), as_type<float>(v19), as_type<float>(v20)));
    uint v59 = (uint)(int)as_type<float>(v58);
    uint v60 = v17 & v59;
    uint v61 = v60 << 16u;
    uint v62 = v51 | v61;
    uint v63 = v25 >> 24u;
    uint v64 = as_type<uint>((float)(int)v63);
    uint v65 = as_type<uint>(as_type<float>(v7) - as_type<float>(v18) * as_type<float>(v64));
    uint v66 = as_type<uint>(as_type<float>(v34) * as_type<float>(v65));
    uint v67 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v64), as_type<float>(v66)));
    uint v68 = as_type<uint>(fma(as_type<float>(v67), as_type<float>(v19), as_type<float>(v20)));
    uint v69 = (uint)(int)as_type<float>(v68);
    uint v70 = v69 << 24u;
    uint v71 = v62 | v70;
    ((device uint*)p0)[i] = v71;
}
