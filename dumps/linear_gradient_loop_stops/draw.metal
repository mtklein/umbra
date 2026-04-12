#include <metal_stdlib>
using namespace metal;


kernel void umbra_entry(
    constant uint &w [[buffer(4)]],
    constant uint *buf_limit [[buffer(5)]],
    constant uint *buf_row_bytes [[buffer(6)]],
    constant uint &x0 [[buffer(7)]],
    constant uint &y0 [[buffer(8)]],
    device uchar *p0 [[buffer(0)]],
    device uchar *p1 [[buffer(1)]],
    device uchar *p2 [[buffer(2)]],
    device uchar *p3 [[buffer(3)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= w) return;
    uint x = x0 + pos.x;
    uint y = y0 + pos.y;
    uint var0 = 0;
    uint var1 = 0;
    uint var2 = 0;
    uint var3 = 0;
    uint var4 = 0;
    uint v0 = 0u;
    uint v3 = ((device const uint*)p0)[0];
    uint v4 = ((device const uint*)p0)[1];
    uint v5 = ((device const uint*)p0)[2];
    uint v6 = 1065353216u;
    uint v7 = ((device const uint*)p0)[3];
    uint v8 = (uint)(int)as_type<float>(v7);
    uint v9 = 1u;
    uint v10 = v8 - 1u;
    uint v11 = v8 + v8;
    uint v12 = v8 + v11;
    uint v13 = x0 + pos.x;
    float v14 = (float)(int)v13;
    uint v15 = y0 + pos.y;
    float v16 = (float)(int)v15;
    float v17 = v16 * as_type<float>(v4);
    float v18 = fma(v14, as_type<float>(v3), v17);
    float v19 = as_type<float>(v5) + v18;
    float v20 = max(v19, as_type<float>(0u));
    float v21 = min(v20, as_type<float>(1065353216u));
    while (var4 < v10) {
    uint v23 = var4;
    uint v24 = v23 + 1u;
    uint v25 = 0; if (v24 < buf_limit[3]) { v25 = ((device uint*)p3)[v24]; }
    uint v26 = v21 <= as_type<float>(v25) ? 0xffffffffu : 0u;
    uint v27 = v12 + v24;
    uint v28 = 0; if (v27 < buf_limit[2]) { v28 = ((device uint*)p2)[v27]; }
    uint v29 = v8 + v24;
    uint v30 = 0; if (v29 < buf_limit[2]) { v30 = ((device uint*)p2)[v29]; }
    uint v31 = v11 + v24;
    uint v32 = 0; if (v31 < buf_limit[2]) { v32 = ((device uint*)p2)[v31]; }
    uint v33 = v12 + v23;
    uint v34 = 0; if (v33 < buf_limit[2]) { v34 = ((device uint*)p2)[v33]; }
    float v35 = as_type<float>(v28) - as_type<float>(v34);
    uint v36 = 0; if (v23 < buf_limit[3]) { v36 = ((device uint*)p3)[v23]; }
    uint v37 = as_type<float>(v36) <= v21 ? 0xffffffffu : 0u;
    uint v38 = v37 & v26;
    float v39 = as_type<float>(v25) - as_type<float>(v36);
    float v40 = v21 - as_type<float>(v36);
    float v41 = v40 / v39;
    float v42 = fma(v41, v35, as_type<float>(v34));
    uint v43 = v8 + v23;
    uint v44 = 0; if (v43 < buf_limit[2]) { v44 = ((device uint*)p2)[v43]; }
    float v45 = as_type<float>(v30) - as_type<float>(v44);
    float v46 = fma(v41, v45, as_type<float>(v44));
    uint v47 = v11 + v23;
    uint v48 = 0; if (v47 < buf_limit[2]) { v48 = ((device uint*)p2)[v47]; }
    float v49 = as_type<float>(v32) - as_type<float>(v48);
    float v50 = fma(v41, v49, as_type<float>(v48));
    uint v51 = 0; if (v24 < buf_limit[2]) { v51 = ((device uint*)p2)[v24]; }
    uint v52 = 0; if (v23 < buf_limit[2]) { v52 = ((device uint*)p2)[v23]; }
    float v53 = as_type<float>(v51) - as_type<float>(v52);
    float v54 = fma(v41, v53, as_type<float>(v52));
    uint v55 = var0;
    uint v56 = select(v55, as_type<uint>(v54), v38 != 0u);
    var0 = v56;

    uint v58 = var1;
    uint v59 = select(v58, as_type<uint>(v46), v38 != 0u);
    var1 = v59;

    uint v61 = var2;
    uint v62 = select(v61, as_type<uint>(v50), v38 != 0u);
    var2 = v62;

    uint v64 = var3;
    uint v65 = select(v64, as_type<uint>(v42), v38 != 0u);
    var3 = v65;

    uint v67 = var4;
    uint v68 = v67 + 1u;
    var4 = v68;

    }
    uint v71 = var0;
    uint v72 = (uint)as_type<ushort>((half)as_type<float>(v71));
    uint v73 = var1;
    uint v74 = (uint)as_type<ushort>((half)as_type<float>(v73));
    uint v75 = var2;
    uint v76 = (uint)as_type<ushort>((half)as_type<float>(v75));
    uint v77 = var3;
    uint v78 = (uint)as_type<ushort>((half)as_type<float>(v77));
    {
        device ushort *hp = (device ushort*)(p1 + y * buf_row_bytes[1]) + x*4;
        hp[0] = ushort(v72); hp[1] = ushort(v74); hp[2] = ushort(v76); hp[3] = ushort(v78);
    }
}
