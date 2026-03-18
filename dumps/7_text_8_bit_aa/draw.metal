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
    uint v1 = ((device const uint*)p1)[2];
    uint v2 = ((device const uint*)p1)[3];
    uint v3 = ((device const uint*)p1)[4];
    uint v4 = ((device const uint*)p1)[5];
    uint v6 = buf_szs[2] >> 1u;
    uint v7 = 998277249u;
    uint v8 = buf_szs[0] >> 2u;
    uint v9 = 255u;
    uint v10 = 1065353216u;
    uint v11 = as_type<uint>(as_type<float>(v10) - as_type<float>(v4));
    uint v12 = 1132396544u;
    uint v13 = 1056964608u;
    uint v14 = 0xffffffffu;
    uint v15 = i;
    uint v16 = v15 <  v8 ? 0xffffffffu : 0u;
    uint v17 = v14 & v16;
    uint v18 = ((device uint*)p0)[i];
    uint v19 = v18 & v9;
    uint v20 = as_type<uint>((float)(int)v19);
    uint v21 = v15 <  v6 ? 0xffffffffu : 0u;
    uint v22 = v14 & v21;
    uint v23 = (uint)((device ushort*)p2)[i];
    uint v24 = (uint)(int)(short)(ushort)v23;
    uint v25 = as_type<uint>((float)(int)v24);
    uint v26 = as_type<uint>(as_type<float>(v7) * as_type<float>(v25));
    uint v27 = as_type<uint>(as_type<float>(v7) * as_type<float>(v20));
    uint v28 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v11), as_type<float>(v1)));
    uint v29 = as_type<uint>(as_type<float>(v28) - as_type<float>(v7) * as_type<float>(v20));
    uint v30 = as_type<uint>(as_type<float>(v26) * as_type<float>(v29));
    uint v31 = as_type<uint>(fma(as_type<float>(v7), as_type<float>(v20), as_type<float>(v30)));
    uint v32 = as_type<uint>(fma(as_type<float>(v31), as_type<float>(v12), as_type<float>(v13)));
    uint v33 = (uint)(int)as_type<float>(v32);
    uint v34 = v9 & v33;
    uint v35 = v18 >> 8u;
    uint v36 = v9 & v35;
    uint v37 = as_type<uint>((float)(int)v36);
    uint v38 = as_type<uint>(as_type<float>(v7) * as_type<float>(v37));
    uint v39 = as_type<uint>(fma(as_type<float>(v38), as_type<float>(v11), as_type<float>(v2)));
    uint v40 = as_type<uint>(as_type<float>(v39) - as_type<float>(v7) * as_type<float>(v37));
    uint v41 = as_type<uint>(as_type<float>(v26) * as_type<float>(v40));
    uint v42 = as_type<uint>(fma(as_type<float>(v7), as_type<float>(v37), as_type<float>(v41)));
    uint v43 = as_type<uint>(fma(as_type<float>(v42), as_type<float>(v12), as_type<float>(v13)));
    uint v44 = (uint)(int)as_type<float>(v43);
    uint v45 = v9 & v44;
    uint v46 = v45 << 8u;
    uint v47 = v34 | v46;
    uint v48 = v18 >> 16u;
    uint v49 = v9 & v48;
    uint v50 = as_type<uint>((float)(int)v49);
    uint v51 = as_type<uint>(as_type<float>(v7) * as_type<float>(v50));
    uint v52 = as_type<uint>(fma(as_type<float>(v51), as_type<float>(v11), as_type<float>(v3)));
    uint v53 = as_type<uint>(as_type<float>(v52) - as_type<float>(v7) * as_type<float>(v50));
    uint v54 = as_type<uint>(as_type<float>(v26) * as_type<float>(v53));
    uint v55 = as_type<uint>(fma(as_type<float>(v7), as_type<float>(v50), as_type<float>(v54)));
    uint v56 = as_type<uint>(fma(as_type<float>(v55), as_type<float>(v12), as_type<float>(v13)));
    uint v57 = (uint)(int)as_type<float>(v56);
    uint v58 = v9 & v57;
    uint v59 = v58 << 16u;
    uint v60 = v47 | v59;
    uint v61 = v18 >> 24u;
    uint v62 = as_type<uint>((float)(int)v61);
    uint v63 = as_type<uint>(as_type<float>(v7) * as_type<float>(v62));
    uint v64 = as_type<uint>(fma(as_type<float>(v63), as_type<float>(v11), as_type<float>(v4)));
    uint v65 = as_type<uint>(as_type<float>(v64) - as_type<float>(v7) * as_type<float>(v62));
    uint v66 = as_type<uint>(as_type<float>(v26) * as_type<float>(v65));
    uint v67 = as_type<uint>(fma(as_type<float>(v7), as_type<float>(v62), as_type<float>(v66)));
    uint v68 = as_type<uint>(fma(as_type<float>(v67), as_type<float>(v12), as_type<float>(v13)));
    uint v69 = (uint)(int)as_type<float>(v68);
    uint v70 = v69 << 24u;
    uint v71 = v60 | v70;
    ((device uint*)p0)[i] = v71;
}
