#include <metal_stdlib>
using namespace metal;

kernel void umbra_entry(
    constant uint &n [[buffer(0)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    device uchar *p2 [[buffer(3)]],
    device uchar *p3 [[buffer(4)]],
    device uchar *p4 [[buffer(5)]],
    constant uint *buf_szs [[buffer(6)]],
    uint i [[thread_position_in_grid]]
) {
    if (i >= n) return;
    uint v0 = 0u;
    uint v1 = buf_szs[0] >> 2u;
    uint v2 = 255u;
    uint v3 = 998277249u;
    uint v4 = buf_szs[1] >> 1u;
    uint v5 = buf_szs[2] >> 1u;
    uint v6 = buf_szs[3] >> 1u;
    uint v7 = buf_szs[4] >> 1u;
    uint v8 = 1065353216u;
    uint v9 = 0xffffffffu;
    uint v10 = i;
    uint v11 = v10 <  v4 ? 0xffffffffu : 0u;
    uint v12 = v9 & v11;
    uint v13 = v10 <  v1 ? 0xffffffffu : 0u;
    uint v14 = v9 & v13;
    uint v15 = ((device uint*)p0)[i];
    uint v16 = v15 & v2;
    uint v17 = as_type<uint>((float)(int)v16);
    uint v18 = (uint)((device ushort*)p1)[i];
    uint v19 = as_type<uint>((float)as_type<half>((ushort)v18));
    uint v20 = v15 >> 24u;
    uint v21 = as_type<uint>((float)(int)v20);
    uint v22 = as_type<uint>(as_type<float>(v8) - as_type<float>(v3) * as_type<float>(v21));
    uint v23 = as_type<uint>(as_type<float>(v19) * as_type<float>(v22));
    uint v24 = as_type<uint>(fma(as_type<float>(v3), as_type<float>(v17), as_type<float>(v23)));
    uint v25 = (uint)as_type<ushort>((half)as_type<float>(v24));
    ((device ushort*)p1)[i] = (ushort)v25;

    uint v27 = v10 <  v5 ? 0xffffffffu : 0u;
    uint v28 = v9 & v27;
    uint v29 = v15 >> 8u;
    uint v30 = v2 & v29;
    uint v31 = as_type<uint>((float)(int)v30);
    uint v32 = (uint)((device ushort*)p2)[i];
    uint v33 = as_type<uint>((float)as_type<half>((ushort)v32));
    uint v34 = as_type<uint>(as_type<float>(v33) * as_type<float>(v22));
    uint v35 = as_type<uint>(fma(as_type<float>(v3), as_type<float>(v31), as_type<float>(v34)));
    uint v36 = (uint)as_type<ushort>((half)as_type<float>(v35));
    ((device ushort*)p2)[i] = (ushort)v36;

    uint v38 = v10 <  v6 ? 0xffffffffu : 0u;
    uint v39 = v9 & v38;
    uint v40 = v15 >> 16u;
    uint v41 = v2 & v40;
    uint v42 = as_type<uint>((float)(int)v41);
    uint v43 = (uint)((device ushort*)p3)[i];
    uint v44 = as_type<uint>((float)as_type<half>((ushort)v43));
    uint v45 = as_type<uint>(as_type<float>(v44) * as_type<float>(v22));
    uint v46 = as_type<uint>(fma(as_type<float>(v3), as_type<float>(v42), as_type<float>(v45)));
    uint v47 = (uint)as_type<ushort>((half)as_type<float>(v46));
    ((device ushort*)p3)[i] = (ushort)v47;

    uint v49 = v10 <  v7 ? 0xffffffffu : 0u;
    uint v50 = v9 & v49;
    uint v51 = (uint)((device ushort*)p4)[i];
    uint v52 = as_type<uint>((float)as_type<half>((ushort)v51));
    uint v53 = as_type<uint>(as_type<float>(v52) * as_type<float>(v22));
    uint v54 = as_type<uint>(fma(as_type<float>(v3), as_type<float>(v21), as_type<float>(v53)));
    uint v55 = (uint)as_type<ushort>((half)as_type<float>(v54));
    ((device ushort*)p4)[i] = (ushort)v55;
}
