#include <metal_stdlib>
using namespace metal;

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
    uint v19 = as_type<uint>(as_type<float>(v15) - as_type<float>(v7));
    uint v20 = 1132396544u;
    uint v21 = 1056964608u;
    uint v22 = 0xffffffffu;
    uint v23 = i;
    uint v24 = v23 <  v16 ? 0xffffffffu : 0u;
    uint v25 = v22 & v24;
    uint v26 = ((device uint*)p0)[i];
    uint v27 = v26 & v17;
    uint v28 = as_type<uint>((float)(int)v27);
    uint v29 = v23 + v1;
    uint v30 = as_type<uint>((float)(int)v29);
    uint v31 = as_type<float>(v8) <= as_type<float>(v30) ? 0xffffffffu : 0u;
    uint v32 = as_type<float>(v30) <  as_type<float>(v10) ? 0xffffffffu : 0u;
    uint v33 = v31 & v32;
    uint v34 = v33 & v14;
    uint v35 = (v34 & v15) | (~v34 & v0);
    uint v36 = as_type<uint>(as_type<float>(v18) * as_type<float>(v28));
    uint v37 = v26 >> 24u;
    uint v38 = as_type<uint>((float)(int)v37);
    uint v39 = as_type<uint>(as_type<float>(v15) - as_type<float>(v18) * as_type<float>(v38));
    uint v40 = as_type<uint>(as_type<float>(v36) * as_type<float>(v19));
    uint v41 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v39), as_type<float>(v40)));
    uint v42 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v36), as_type<float>(v41)));
    uint v43 = as_type<uint>(as_type<float>(v42) - as_type<float>(v18) * as_type<float>(v28));
    uint v44 = as_type<uint>(as_type<float>(v35) * as_type<float>(v43));
    uint v45 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v28), as_type<float>(v44)));
    uint v46 = as_type<uint>(fma(as_type<float>(v45), as_type<float>(v20), as_type<float>(v21)));
    uint v47 = (uint)(int)as_type<float>(v46);
    uint v48 = v17 & v47;
    uint v49 = v26 >> 8u;
    uint v50 = v17 & v49;
    uint v51 = as_type<uint>((float)(int)v50);
    uint v52 = as_type<uint>(as_type<float>(v18) * as_type<float>(v51));
    uint v53 = as_type<uint>(as_type<float>(v52) * as_type<float>(v19));
    uint v54 = as_type<uint>(fma(as_type<float>(v5), as_type<float>(v39), as_type<float>(v53)));
    uint v55 = as_type<uint>(fma(as_type<float>(v5), as_type<float>(v52), as_type<float>(v54)));
    uint v56 = as_type<uint>(as_type<float>(v55) - as_type<float>(v18) * as_type<float>(v51));
    uint v57 = as_type<uint>(as_type<float>(v35) * as_type<float>(v56));
    uint v58 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v51), as_type<float>(v57)));
    uint v59 = as_type<uint>(fma(as_type<float>(v58), as_type<float>(v20), as_type<float>(v21)));
    uint v60 = (uint)(int)as_type<float>(v59);
    uint v61 = v17 & v60;
    uint v62 = v61 << 8u;
    uint v63 = v48 | v62;
    uint v64 = v26 >> 16u;
    uint v65 = v17 & v64;
    uint v66 = as_type<uint>((float)(int)v65);
    uint v67 = as_type<uint>(as_type<float>(v18) * as_type<float>(v66));
    uint v68 = as_type<uint>(as_type<float>(v67) * as_type<float>(v19));
    uint v69 = as_type<uint>(fma(as_type<float>(v6), as_type<float>(v39), as_type<float>(v68)));
    uint v70 = as_type<uint>(fma(as_type<float>(v6), as_type<float>(v67), as_type<float>(v69)));
    uint v71 = as_type<uint>(as_type<float>(v70) - as_type<float>(v18) * as_type<float>(v66));
    uint v72 = as_type<uint>(as_type<float>(v35) * as_type<float>(v71));
    uint v73 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v66), as_type<float>(v72)));
    uint v74 = as_type<uint>(fma(as_type<float>(v73), as_type<float>(v20), as_type<float>(v21)));
    uint v75 = (uint)(int)as_type<float>(v74);
    uint v76 = v17 & v75;
    uint v77 = v76 << 16u;
    uint v78 = v63 | v77;
    uint v79 = as_type<uint>(as_type<float>(v18) * as_type<float>(v38));
    uint v80 = as_type<uint>(as_type<float>(v79) * as_type<float>(v19));
    uint v81 = as_type<uint>(fma(as_type<float>(v7), as_type<float>(v39), as_type<float>(v80)));
    uint v82 = as_type<uint>(fma(as_type<float>(v7), as_type<float>(v79), as_type<float>(v81)));
    uint v83 = as_type<uint>(as_type<float>(v82) - as_type<float>(v18) * as_type<float>(v38));
    uint v84 = as_type<uint>(as_type<float>(v35) * as_type<float>(v83));
    uint v85 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v38), as_type<float>(v84)));
    uint v86 = as_type<uint>(fma(as_type<float>(v85), as_type<float>(v20), as_type<float>(v21)));
    uint v87 = (uint)(int)as_type<float>(v86);
    uint v88 = v87 << 24u;
    uint v89 = v78 | v88;
    ((device uint*)p0)[i] = v89;
}
