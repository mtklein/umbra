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
    device uchar *p2 [[buffer(3)]],
    constant uint *buf_szs [[buffer(4)]],
    uint i [[thread_position_in_grid]]
) {
    if (i >= n) return;
    uint v0 = 0u;
    uint v1 = ((device const uint*)p1)[2];
    uint v2 = ((device const uint*)p1)[3];
    uint v3 = ((device const uint*)p1)[4];
    uint v4 = ((device const uint*)p1)[5];
    uint v6 = buf_szs[2] >> 1u;
    uint v7 = 998277249u;
    uint v8 = 1054867456u;
    uint v9 = 1090519040u;
    uint v10 = 1065353216u;
    uint v11 = buf_szs[0] >> 2u;
    uint v12 = 255u;
    uint v13 = as_type<uint>(as_type<float>(v10) - as_type<float>(v4));
    uint v14 = 1132396544u;
    uint v15 = 1056964608u;
    uint v16 = 0xffffffffu;
    uint v17 = i;
    uint v18 = v17 <  v11 ? 0xffffffffu : 0u;
    uint v19 = v16 & v18;
    uint v20 = ((device uint*)p0)[i];
    uint v21 = v20 & v12;
    uint v22 = as_type<uint>((float)(int)v21);
    uint v23 = v17 <  v6 ? 0xffffffffu : 0u;
    uint v24 = v16 & v23;
    uint v25 = (uint)((device ushort*)p2)[i];
    uint v26 = (uint)(int)(short)(ushort)v25;
    uint v27 = as_type<uint>((float)(int)v26);
    uint v28 = as_type<uint>(as_type<float>(v7) * as_type<float>(v27));
    uint v29 = as_type<uint>(as_type<float>(v28) - as_type<float>(v8));
    uint v30 = as_type<uint>(as_type<float>(v9) * as_type<float>(v29));
    uint v31 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v30)));
    uint v32 = as_type<uint>(min(as_type<float>(v10), as_type<float>(v31)));
    uint v33 = as_type<uint>(as_type<float>(v7) * as_type<float>(v22));
    uint v34 = as_type<uint>(fma(as_type<float>(v33), as_type<float>(v13), as_type<float>(v1)));
    uint v35 = as_type<uint>(as_type<float>(v34) - as_type<float>(v7) * as_type<float>(v22));
    uint v36 = as_type<uint>(as_type<float>(v32) * as_type<float>(v35));
    uint v37 = as_type<uint>(fma(as_type<float>(v7), as_type<float>(v22), as_type<float>(v36)));
    uint v38 = as_type<uint>(fma(as_type<float>(v37), as_type<float>(v14), as_type<float>(v15)));
    uint v39 = (uint)(int)as_type<float>(v38);
    uint v40 = v12 & v39;
    uint v41 = v20 >> 8u;
    uint v42 = v12 & v41;
    uint v43 = as_type<uint>((float)(int)v42);
    uint v44 = as_type<uint>(as_type<float>(v7) * as_type<float>(v43));
    uint v45 = as_type<uint>(fma(as_type<float>(v44), as_type<float>(v13), as_type<float>(v2)));
    uint v46 = as_type<uint>(as_type<float>(v45) - as_type<float>(v7) * as_type<float>(v43));
    uint v47 = as_type<uint>(as_type<float>(v32) * as_type<float>(v46));
    uint v48 = as_type<uint>(fma(as_type<float>(v7), as_type<float>(v43), as_type<float>(v47)));
    uint v49 = as_type<uint>(fma(as_type<float>(v48), as_type<float>(v14), as_type<float>(v15)));
    uint v50 = (uint)(int)as_type<float>(v49);
    uint v51 = v12 & v50;
    uint v52 = v51 << 8u;
    uint v53 = v40 | v52;
    uint v54 = v20 >> 16u;
    uint v55 = v12 & v54;
    uint v56 = as_type<uint>((float)(int)v55);
    uint v57 = as_type<uint>(as_type<float>(v7) * as_type<float>(v56));
    uint v58 = as_type<uint>(fma(as_type<float>(v57), as_type<float>(v13), as_type<float>(v3)));
    uint v59 = as_type<uint>(as_type<float>(v58) - as_type<float>(v7) * as_type<float>(v56));
    uint v60 = as_type<uint>(as_type<float>(v32) * as_type<float>(v59));
    uint v61 = as_type<uint>(fma(as_type<float>(v7), as_type<float>(v56), as_type<float>(v60)));
    uint v62 = as_type<uint>(fma(as_type<float>(v61), as_type<float>(v14), as_type<float>(v15)));
    uint v63 = (uint)(int)as_type<float>(v62);
    uint v64 = v12 & v63;
    uint v65 = v64 << 16u;
    uint v66 = v53 | v65;
    uint v67 = v20 >> 24u;
    uint v68 = as_type<uint>((float)(int)v67);
    uint v69 = as_type<uint>(as_type<float>(v7) * as_type<float>(v68));
    uint v70 = as_type<uint>(fma(as_type<float>(v69), as_type<float>(v13), as_type<float>(v4)));
    uint v71 = as_type<uint>(as_type<float>(v70) - as_type<float>(v7) * as_type<float>(v68));
    uint v72 = as_type<uint>(as_type<float>(v32) * as_type<float>(v71));
    uint v73 = as_type<uint>(fma(as_type<float>(v7), as_type<float>(v68), as_type<float>(v72)));
    uint v74 = as_type<uint>(fma(as_type<float>(v73), as_type<float>(v14), as_type<float>(v15)));
    uint v75 = (uint)(int)as_type<float>(v74);
    uint v76 = v75 << 24u;
    uint v77 = v66 | v76;
    ((device uint*)p0)[i] = v77;
}
