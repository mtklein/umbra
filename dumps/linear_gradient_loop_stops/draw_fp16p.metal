#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, count2, count3, stride0, stride1, stride2, stride3; };

kernel void umbra_entry(
    constant meta &m [[buffer(4)]],
    device uint *p0 [[buffer(0)]],
    device ushort *p1 [[buffer(1)]],
    device uint *p2 [[buffer(2)]],
    device uint *p3 [[buffer(3)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= m.w) return;
    uint x = m.x0 + pos.x;
    uint y = m.y0 + pos.y;
    uint var0 = 0;
    uint var1 = 0;
    uint var2 = 0;
    uint var3 = 0;
    uint var4 = 0;
    uint v0 = 0u;
    uint v3 = p0[0];
    uint v4 = p0[1];
    uint v5 = p0[2];
    uint v6 = 1065353216u;
    uint v7 = p0[3];
    uint v8 = (uint)(int)as_type<float>(v7);
    uint v9 = 1u;
    uint v10 = v8 - 1u;
    uint v11 = v8 + v8;
    uint v12 = v8 + v11;
    uint v13 = m.x0 + pos.x;
    float v14 = (float)(int)v13;
    uint v15 = m.y0 + pos.y;
    float v16 = (float)(int)v15;
    float v17 = v16 * as_type<float>(v4);
    float v18 = fma(v14, as_type<float>(v3), v17);
    float v19 = as_type<float>(v5) + v18;
    float v20 = max(v19, as_type<float>(0u));
    float v21 = min(v20, as_type<float>(1065353216u));
    while (var4 < v10) {
    uint v23 = var0;
    uint v24 = var1;
    uint v25 = var2;
    uint v26 = var3;
    uint v27 = var4;
    uint v28 = v27 + 1u;
    uint v29 = p3[min(v28, m.count3 - 1u)] & ((v28 < m.count3) ? ~0u : 0u);
    uint v30 = v21 <= as_type<float>(v29) ? 0xffffffffu : 0u;
    uint v31 = v12 + v28;
    uint v32 = p2[min(v31, m.count2 - 1u)] & ((v31 < m.count2) ? ~0u : 0u);
    uint v33 = v8 + v28;
    uint v34 = p2[min(v33, m.count2 - 1u)] & ((v33 < m.count2) ? ~0u : 0u);
    uint v35 = v11 + v28;
    uint v36 = p2[min(v35, m.count2 - 1u)] & ((v35 < m.count2) ? ~0u : 0u);
    uint v37 = p3[min(v27, m.count3 - 1u)] & ((v27 < m.count3) ? ~0u : 0u);
    uint v38 = as_type<float>(v37) <= v21 ? 0xffffffffu : 0u;
    uint v39 = v38 & v30;
    float v40 = as_type<float>(v29) - as_type<float>(v37);
    float v41 = v21 - as_type<float>(v37);
    float v42 = v41 / v40;
    uint v43 = v8 + v27;
    uint v44 = p2[min(v43, m.count2 - 1u)] & ((v43 < m.count2) ? ~0u : 0u);
    float v45 = as_type<float>(v34) - as_type<float>(v44);
    float v46 = fma(v42, v45, as_type<float>(v44));
    uint v47 = select(v24, as_type<uint>(v46), v39 != 0u);
    var1 = v47;

    uint v49 = v11 + v27;
    uint v50 = p2[min(v49, m.count2 - 1u)] & ((v49 < m.count2) ? ~0u : 0u);
    float v51 = as_type<float>(v36) - as_type<float>(v50);
    float v52 = fma(v42, v51, as_type<float>(v50));
    uint v53 = select(v25, as_type<uint>(v52), v39 != 0u);
    var2 = v53;

    uint v55 = v12 + v27;
    uint v56 = p2[min(v55, m.count2 - 1u)] & ((v55 < m.count2) ? ~0u : 0u);
    float v57 = as_type<float>(v32) - as_type<float>(v56);
    float v58 = fma(v42, v57, as_type<float>(v56));
    uint v59 = select(v26, as_type<uint>(v58), v39 != 0u);
    var3 = v59;

    uint v61 = p2[min(v28, m.count2 - 1u)] & ((v28 < m.count2) ? ~0u : 0u);
    uint v62 = p2[min(v27, m.count2 - 1u)] & ((v27 < m.count2) ? ~0u : 0u);
    float v63 = as_type<float>(v61) - as_type<float>(v62);
    float v64 = fma(v42, v63, as_type<float>(v62));
    uint v65 = select(v23, as_type<uint>(v64), v39 != 0u);
    var0 = v65;

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
    { uint _row = y * m.stride1; uint _ps = m.count1 / 4;
      p1[_row + x] = ushort(v72); p1[_row + x + _ps] = ushort(v74); p1[_row + x + 2*_ps] = ushort(v76); p1[_row + x + 3*_ps] = ushort(v78); }
}
