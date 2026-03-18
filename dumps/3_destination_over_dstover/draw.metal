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
    uint v20 = ((device uint*)p0)[i];
    uint v21 = v20 & v16;
    uint v22 = as_type<uint>((float)(int)v21);
    uint v23 = i;
    uint v24 = v23 + v1;
    uint v25 = as_type<uint>((float)(int)v24);
    uint v26 = as_type<float>(v8) <= as_type<float>(v25) ? 0xffffffffu : 0u;
    uint v27 = as_type<float>(v25) <  as_type<float>(v10) ? 0xffffffffu : 0u;
    uint v28 = v26 & v27;
    uint v29 = v28 & v14;
    uint v30 = (v29 & v15) | (~v29 & v0);
    uint v31 = v20 >> 24u;
    uint v32 = as_type<uint>((float)(int)v31);
    uint v33 = as_type<uint>(as_type<float>(v15) - as_type<float>(v17) * as_type<float>(v32));
    uint v34 = as_type<uint>(as_type<float>(v4) * as_type<float>(v33));
    uint v35 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v22), as_type<float>(v34)));
    uint v36 = as_type<uint>(as_type<float>(v35) - as_type<float>(v17) * as_type<float>(v22));
    uint v37 = as_type<uint>(as_type<float>(v30) * as_type<float>(v36));
    uint v38 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v22), as_type<float>(v37)));
    uint v39 = as_type<uint>(fma(as_type<float>(v38), as_type<float>(v18), as_type<float>(v19)));
    uint v40 = (uint)(int)as_type<float>(v39);
    uint v41 = v16 & v40;
    uint v42 = v20 >> 8u;
    uint v43 = v16 & v42;
    uint v44 = as_type<uint>((float)(int)v43);
    uint v45 = as_type<uint>(as_type<float>(v5) * as_type<float>(v33));
    uint v46 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v44), as_type<float>(v45)));
    uint v47 = as_type<uint>(as_type<float>(v46) - as_type<float>(v17) * as_type<float>(v44));
    uint v48 = as_type<uint>(as_type<float>(v30) * as_type<float>(v47));
    uint v49 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v44), as_type<float>(v48)));
    uint v50 = as_type<uint>(fma(as_type<float>(v49), as_type<float>(v18), as_type<float>(v19)));
    uint v51 = (uint)(int)as_type<float>(v50);
    uint v52 = v16 & v51;
    uint v53 = v52 << 8u;
    uint v54 = v41 | v53;
    uint v55 = v20 >> 16u;
    uint v56 = v16 & v55;
    uint v57 = as_type<uint>((float)(int)v56);
    uint v58 = as_type<uint>(as_type<float>(v6) * as_type<float>(v33));
    uint v59 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v57), as_type<float>(v58)));
    uint v60 = as_type<uint>(as_type<float>(v59) - as_type<float>(v17) * as_type<float>(v57));
    uint v61 = as_type<uint>(as_type<float>(v30) * as_type<float>(v60));
    uint v62 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v57), as_type<float>(v61)));
    uint v63 = as_type<uint>(fma(as_type<float>(v62), as_type<float>(v18), as_type<float>(v19)));
    uint v64 = (uint)(int)as_type<float>(v63);
    uint v65 = v16 & v64;
    uint v66 = v65 << 16u;
    uint v67 = v54 | v66;
    uint v68 = as_type<uint>(as_type<float>(v7) * as_type<float>(v33));
    uint v69 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v32), as_type<float>(v68)));
    uint v70 = as_type<uint>(as_type<float>(v69) - as_type<float>(v17) * as_type<float>(v32));
    uint v71 = as_type<uint>(as_type<float>(v30) * as_type<float>(v70));
    uint v72 = as_type<uint>(fma(as_type<float>(v17), as_type<float>(v32), as_type<float>(v71)));
    uint v73 = as_type<uint>(fma(as_type<float>(v72), as_type<float>(v18), as_type<float>(v19)));
    uint v74 = (uint)(int)as_type<float>(v73);
    uint v75 = v74 << 24u;
    uint v76 = v67 | v75;
    ((device uint*)p0)[i] = v76;
}
