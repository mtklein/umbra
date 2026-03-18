#include <metal_stdlib>
using namespace metal;

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
    uint v1 = ((device const uint*)p1)[0];
    uint v2 = 1u;
    uint v3 = ((device const uint*)p1)[1];
    uint v4 = as_type<uint>((float)(int)v3);
    uint v6 = 2u;
    uint v7 = ((device const uint*)p1)[2];
    uint v8 = 3u;
    uint v9 = ((device const uint*)p1)[3];
    uint v10 = 4u;
    uint v11 = ((device const uint*)p1)[4];
    uint v12 = as_type<uint>(as_type<float>(v4) * as_type<float>(v9));
    uint v13 = 1065353216u;
    uint v14 = ((device const uint*)p1)[5];
    uint v15 = 1073741824u;
    uint v16 = as_type<uint>(as_type<float>(v14) - as_type<float>(v13));
    uint v17 = as_type<uint>(as_type<float>(v14) - as_type<float>(v15));
    uint v18 = buf_szs[2] >> 2u;
    uint v19 = 1132396544u;
    uint v20 = 1056964608u;
    uint v21 = 255u;
    uint v22 = buf_szs[0] >> 2u;
    uint v23 = i;
    uint v24 = v23 + v1;
    uint v25 = as_type<uint>((float)(int)v24);
    uint v26 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v7), as_type<float>(v12)));
    uint v27 = as_type<uint>(as_type<float>(v11) + as_type<float>(v26));
    uint v28 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v27)));
    uint v29 = as_type<uint>(min(as_type<float>(v28), as_type<float>(v13)));
    uint v30 = as_type<uint>(as_type<float>(v29) * as_type<float>(v16));
    uint v31 = (uint)(int)as_type<float>(v30);
    uint v32 = as_type<uint>((float)(int)v31);
    uint v33 = as_type<uint>(min(as_type<float>(v17), as_type<float>(v32)));
    uint v34 = as_type<uint>(as_type<float>(v30) - as_type<float>(v33));
    uint v35 = (uint)(int)as_type<float>(v33);
    uint v36 = v35 << 2u;
    uint v37 = 0xffffffffu;
    uint v38 = v36 <  v18 ? 0xffffffffu : 0u;
    uint v39 = v37 & v38;
    uint v40 = v39 ? ((device uint*)p2)[(int)v36] : 0u;
    uint v41 = v10 + v36;
    uint v42 = v41 <  v18 ? 0xffffffffu : 0u;
    uint v43 = v37 & v42;
    uint v44 = v43 ? ((device uint*)p2)[(int)v41] : 0u;
    uint v45 = as_type<uint>(as_type<float>(v44) - as_type<float>(v40));
    uint v46 = as_type<uint>(fma(as_type<float>(v34), as_type<float>(v45), as_type<float>(v40)));
    uint v47 = as_type<uint>(fma(as_type<float>(v46), as_type<float>(v19), as_type<float>(v20)));
    uint v48 = (uint)(int)as_type<float>(v47);
    uint v49 = v48 & v21;
    uint v50 = v2 + v36;
    uint v51 = v50 <  v18 ? 0xffffffffu : 0u;
    uint v52 = v37 & v51;
    uint v53 = v52 ? ((device uint*)p2)[(int)v50] : 0u;
    uint v54 = v2 + v41;
    uint v55 = v54 <  v18 ? 0xffffffffu : 0u;
    uint v56 = v37 & v55;
    uint v57 = v56 ? ((device uint*)p2)[(int)v54] : 0u;
    uint v58 = as_type<uint>(as_type<float>(v57) - as_type<float>(v53));
    uint v59 = as_type<uint>(fma(as_type<float>(v34), as_type<float>(v58), as_type<float>(v53)));
    uint v60 = as_type<uint>(fma(as_type<float>(v59), as_type<float>(v19), as_type<float>(v20)));
    uint v61 = (uint)(int)as_type<float>(v60);
    uint v62 = v61 & v21;
    uint v63 = v62 << 8u;
    uint v64 = v49 | v63;
    uint v65 = v6 + v36;
    uint v66 = v65 <  v18 ? 0xffffffffu : 0u;
    uint v67 = v37 & v66;
    uint v68 = v67 ? ((device uint*)p2)[(int)v65] : 0u;
    uint v69 = v6 + v41;
    uint v70 = v69 <  v18 ? 0xffffffffu : 0u;
    uint v71 = v37 & v70;
    uint v72 = v71 ? ((device uint*)p2)[(int)v69] : 0u;
    uint v73 = as_type<uint>(as_type<float>(v72) - as_type<float>(v68));
    uint v74 = as_type<uint>(fma(as_type<float>(v34), as_type<float>(v73), as_type<float>(v68)));
    uint v75 = as_type<uint>(fma(as_type<float>(v74), as_type<float>(v19), as_type<float>(v20)));
    uint v76 = (uint)(int)as_type<float>(v75);
    uint v77 = v76 & v21;
    uint v78 = v77 << 16u;
    uint v79 = v64 | v78;
    uint v80 = v8 + v36;
    uint v81 = v80 <  v18 ? 0xffffffffu : 0u;
    uint v82 = v37 & v81;
    uint v83 = v82 ? ((device uint*)p2)[(int)v80] : 0u;
    uint v84 = v8 + v41;
    uint v85 = v84 <  v18 ? 0xffffffffu : 0u;
    uint v86 = v37 & v85;
    uint v87 = v86 ? ((device uint*)p2)[(int)v84] : 0u;
    uint v88 = as_type<uint>(as_type<float>(v87) - as_type<float>(v83));
    uint v89 = as_type<uint>(fma(as_type<float>(v34), as_type<float>(v88), as_type<float>(v83)));
    uint v90 = as_type<uint>(fma(as_type<float>(v89), as_type<float>(v19), as_type<float>(v20)));
    uint v91 = (uint)(int)as_type<float>(v90);
    uint v92 = v91 << 24u;
    uint v93 = v79 | v92;
    uint v94 = v23 <  v22 ? 0xffffffffu : 0u;
    uint v95 = v37 & v94;
    ((device uint*)p0)[i] = v93;
}
