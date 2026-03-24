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
    uint v18 = as_type<uint>(as_type<float>(v15) - as_type<float>(v7));
    uint v19 = 1132396544u;
    uint v20 = i;
    uint v21 = v20 + v1;
    uint v22 = as_type<uint>((float)(int)v21);
    uint v23 = as_type<float>(v22) <  as_type<float>(v10) ? 0xffffffffu : 0u;
    uint v24 = as_type<float>(v8) <= as_type<float>(v22) ? 0xffffffffu : 0u;
    uint v25 = v24 & v23;
    uint v26 = v25 & v14;
    uint v27 = (v26 & v15) | (~v26 & v0);
    uint v28 = ((device uint*)p0)[i];
    uint v29 = v28 >> 24u;
    uint v30 = as_type<uint>((float)(int)v29);
    uint v31 = as_type<uint>(as_type<float>(v17) * as_type<float>(v30));
    uint v32 = as_type<uint>(as_type<float>(v15) - as_type<float>(v31));
    uint v33 = as_type<uint>(as_type<float>(v31) * as_type<float>(v18));
    uint v34 = as_type<uint>(fma(as_type<float>(v7), as_type<float>(v32), as_type<float>(v33)));
    uint v35 = as_type<uint>(fma(as_type<float>(v7), as_type<float>(v31), as_type<float>(v34)));
    uint v36 = as_type<uint>(as_type<float>(v35) - as_type<float>(v31));
    uint v37 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v36), as_type<float>(v31)));
    uint v38 = as_type<uint>(as_type<float>(v37) * as_type<float>(v19));
    uint v39 = as_type<uint>((int)rint(as_type<float>(v38)));
    uint v40 = v28 & v16;
    uint v41 = as_type<uint>((float)(int)v40);
    uint v42 = v28 >> 8u;
    uint v43 = v16 & v42;
    uint v44 = as_type<uint>((float)(int)v43);
    uint v45 = v28 >> 16u;
    uint v46 = v16 & v45;
    uint v47 = as_type<uint>((float)(int)v46);
    uint v48 = as_type<uint>(as_type<float>(v17) * as_type<float>(v41));
    uint v49 = as_type<uint>(as_type<float>(v17) * as_type<float>(v44));
    uint v50 = as_type<uint>(as_type<float>(v17) * as_type<float>(v47));
    uint v51 = as_type<uint>(as_type<float>(v48) * as_type<float>(v18));
    uint v52 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v32), as_type<float>(v51)));
    uint v53 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v48), as_type<float>(v52)));
    uint v54 = as_type<uint>(as_type<float>(v53) - as_type<float>(v48));
    uint v55 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v54), as_type<float>(v48)));
    uint v56 = as_type<uint>(as_type<float>(v49) * as_type<float>(v18));
    uint v57 = as_type<uint>(fma(as_type<float>(v5), as_type<float>(v32), as_type<float>(v56)));
    uint v58 = as_type<uint>(fma(as_type<float>(v5), as_type<float>(v49), as_type<float>(v57)));
    uint v59 = as_type<uint>(as_type<float>(v58) - as_type<float>(v49));
    uint v60 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v59), as_type<float>(v49)));
    uint v61 = as_type<uint>(as_type<float>(v50) * as_type<float>(v18));
    uint v62 = as_type<uint>(fma(as_type<float>(v6), as_type<float>(v32), as_type<float>(v61)));
    uint v63 = as_type<uint>(fma(as_type<float>(v6), as_type<float>(v50), as_type<float>(v62)));
    uint v64 = as_type<uint>(as_type<float>(v63) - as_type<float>(v50));
    uint v65 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v64), as_type<float>(v50)));
    uint v66 = as_type<uint>(as_type<float>(v55) * as_type<float>(v19));
    uint v67 = as_type<uint>((int)rint(as_type<float>(v66)));
    uint v68 = as_type<uint>(as_type<float>(v60) * as_type<float>(v19));
    uint v69 = as_type<uint>((int)rint(as_type<float>(v68)));
    uint v70 = as_type<uint>(as_type<float>(v65) * as_type<float>(v19));
    uint v71 = as_type<uint>((int)rint(as_type<float>(v70)));
    uint v72 = v16 & v71;
    uint v73 = v16 & v67;
    uint v74 = v16 & v69;
    uint v75 = v74 << 8u;
    uint v76 = v73 | v75;
    uint v77 = v72 << 16u;
    uint v78 = v76 | v77;
    uint v79 = v39 << 24u;
    uint v80 = v78 | v79;
    ((device uint*)p0)[i] = v80;
}
