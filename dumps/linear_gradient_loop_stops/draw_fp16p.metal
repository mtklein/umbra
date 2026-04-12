#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, limit0, limit1, limit2, limit3, stride0, stride1, stride2, stride3; };

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
    uint v23 = var4;
    uint v24 = v23 + 1u;
    uint v25 = p3[min(v24, m.limit3 - 1u)] & ((v24 < m.limit3) ? ~0u : 0u);
    uint v26 = v21 <= as_type<float>(v25) ? 0xffffffffu : 0u;
    uint v27 = v12 + v24;
    uint v28 = p2[min(v27, m.limit2 - 1u)] & ((v27 < m.limit2) ? ~0u : 0u);
    uint v29 = v8 + v24;
    uint v30 = p2[min(v29, m.limit2 - 1u)] & ((v29 < m.limit2) ? ~0u : 0u);
    uint v31 = v11 + v24;
    uint v32 = p2[min(v31, m.limit2 - 1u)] & ((v31 < m.limit2) ? ~0u : 0u);
    uint v33 = v12 + v23;
    uint v34 = p2[min(v33, m.limit2 - 1u)] & ((v33 < m.limit2) ? ~0u : 0u);
    float v35 = as_type<float>(v28) - as_type<float>(v34);
    uint v36 = p3[min(v23, m.limit3 - 1u)] & ((v23 < m.limit3) ? ~0u : 0u);
    uint v37 = as_type<float>(v36) <= v21 ? 0xffffffffu : 0u;
    uint v38 = v37 & v26;
    float v39 = as_type<float>(v25) - as_type<float>(v36);
    float v40 = v21 - as_type<float>(v36);
    float v41 = v40 / v39;
    float v42 = fma(v41, v35, as_type<float>(v34));
    var3 = (v38 != 0u) ? as_type<uint>(v42) : var3;

    uint v44 = v8 + v23;
    uint v45 = p2[min(v44, m.limit2 - 1u)] & ((v44 < m.limit2) ? ~0u : 0u);
    float v46 = as_type<float>(v30) - as_type<float>(v45);
    float v47 = fma(v41, v46, as_type<float>(v45));
    var1 = (v38 != 0u) ? as_type<uint>(v47) : var1;

    uint v49 = v11 + v23;
    uint v50 = p2[min(v49, m.limit2 - 1u)] & ((v49 < m.limit2) ? ~0u : 0u);
    float v51 = as_type<float>(v32) - as_type<float>(v50);
    float v52 = fma(v41, v51, as_type<float>(v50));
    var2 = (v38 != 0u) ? as_type<uint>(v52) : var2;

    uint v54 = p2[min(v24, m.limit2 - 1u)] & ((v24 < m.limit2) ? ~0u : 0u);
    uint v55 = p2[min(v23, m.limit2 - 1u)] & ((v23 < m.limit2) ? ~0u : 0u);
    float v56 = as_type<float>(v54) - as_type<float>(v55);
    float v57 = fma(v41, v56, as_type<float>(v55));
    var0 = (v38 != 0u) ? as_type<uint>(v57) : var0;

    uint v59 = var4;
    uint v60 = v59 + 1u;
    var4 = v60;

    }
    uint v63 = var0;
    uint v64 = (uint)as_type<ushort>((half)as_type<float>(v63));
    uint v65 = var1;
    uint v66 = (uint)as_type<ushort>((half)as_type<float>(v65));
    uint v67 = var2;
    uint v68 = (uint)as_type<ushort>((half)as_type<float>(v67));
    uint v69 = var3;
    uint v70 = (uint)as_type<ushort>((half)as_type<float>(v69));
    { uint _row = y * m.stride1; uint _ps = m.limit1;
      p1[_row + x] = ushort(v64); p1[_row + x + _ps] = ushort(v66); p1[_row + x + 2*_ps] = ushort(v68); p1[_row + x + 3*_ps] = ushort(v70); }
}
