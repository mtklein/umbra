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
    uint v1 = ((device const uint*)p1)[2];
    uint v2 = ((device const uint*)p1)[3];
    uint v3 = ((device const uint*)p1)[4];
    uint v4 = ((device const uint*)p1)[5];
    uint v5 = buf_szs[0] >> 2u;
    uint v6 = 255u;
    uint v7 = 998277249u;
    uint v8 = 1065353216u;
    uint v9 = as_type<uint>(as_type<float>(v8) - as_type<float>(v4));
    uint v10 = 1132396544u;
    uint v11 = 1056964608u;
    uint v12 = 0xffffffffu;
    uint v13 = i;
    uint v14 = v13 <  v5 ? 0xffffffffu : 0u;
    uint v15 = v12 & v14;
    uint v16 = ((device uint*)p0)[i];
    uint v17 = v16 & v6;
    uint v18 = as_type<uint>((float)(int)v17);
    uint v19 = as_type<uint>(as_type<float>(v7) * as_type<float>(v18));
    uint v20 = as_type<uint>(fma(as_type<float>(v19), as_type<float>(v9), as_type<float>(v1)));
    uint v21 = as_type<uint>(fma(as_type<float>(v20), as_type<float>(v10), as_type<float>(v11)));
    uint v22 = (uint)(int)as_type<float>(v21);
    uint v23 = v6 & v22;
    uint v24 = v16 >> 8u;
    uint v25 = v6 & v24;
    uint v26 = as_type<uint>((float)(int)v25);
    uint v27 = as_type<uint>(as_type<float>(v7) * as_type<float>(v26));
    uint v28 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v9), as_type<float>(v2)));
    uint v29 = as_type<uint>(fma(as_type<float>(v28), as_type<float>(v10), as_type<float>(v11)));
    uint v30 = (uint)(int)as_type<float>(v29);
    uint v31 = v6 & v30;
    uint v32 = v31 << 8u;
    uint v33 = v23 | v32;
    uint v34 = v16 >> 16u;
    uint v35 = v6 & v34;
    uint v36 = as_type<uint>((float)(int)v35);
    uint v37 = as_type<uint>(as_type<float>(v7) * as_type<float>(v36));
    uint v38 = as_type<uint>(fma(as_type<float>(v37), as_type<float>(v9), as_type<float>(v3)));
    uint v39 = as_type<uint>(fma(as_type<float>(v38), as_type<float>(v10), as_type<float>(v11)));
    uint v40 = (uint)(int)as_type<float>(v39);
    uint v41 = v6 & v40;
    uint v42 = v41 << 16u;
    uint v43 = v33 | v42;
    uint v44 = v16 >> 24u;
    uint v45 = as_type<uint>((float)(int)v44);
    uint v46 = as_type<uint>(as_type<float>(v7) * as_type<float>(v45));
    uint v47 = as_type<uint>(fma(as_type<float>(v46), as_type<float>(v9), as_type<float>(v4)));
    uint v48 = as_type<uint>(fma(as_type<float>(v47), as_type<float>(v10), as_type<float>(v11)));
    uint v49 = (uint)(int)as_type<float>(v48);
    uint v50 = v49 << 24u;
    uint v51 = v43 | v50;
    ((device uint*)p0)[i] = v51;
}
