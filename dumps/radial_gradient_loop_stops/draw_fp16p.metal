#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, count2, count3, stride0, stride1, stride2, stride3; };

kernel void umbra_entry(
    constant meta &m [[buffer(0)]],
    device const uint * __restrict p0 [[buffer(1)]],
    device ushort * __restrict p1 [[buffer(2)]],
    device const uint * __restrict p2 [[buffer(3)]],
    device const uint * __restrict p3 [[buffer(4)]],
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
    uint v7 = p0[3];
    uint v8 = (uint)(int)as_type<float>(v7);
    uint v11 = v8 - 1u;
    #define v12 v11
    uint v13 = v8 + v8;
    uint v14 = v8 + v13;
    uint v15 = m.x0 + pos.x;
    float v16 = (float)(int)v15;
    float v17 = v16 - as_type<float>(v3);
    uint v18 = m.y0 + pos.y;
    float v19 = (float)(int)v18;
    float v20 = v19 - as_type<float>(v4);
    float v21 = v20 * v20;
    float v22 = fma(v17, v17, v21);
    float v23 = precise::sqrt(v22);
    float v24 = as_type<float>(v5) * v23;
    float v26 = max(v24, as_type<float>(0u));
    #define v27 v26
    float v29 = min(v27, as_type<float>(1065353216u));
    #define v30 v29
    while (var4 < v12) {
    uint v32 = var4;
    uint v34 = p3[min(v32, m.count3 - 1u)] & ((v32 < m.count3) ? ~0u : 0u);
    uint v35 = as_type<float>(v34) <= v30 ? 0xffffffffu : 0u;
    uint v36 = v32 + 1u;
    #define v37 v36
    uint v38 = p3[min(v37, m.count3 - 1u)] & ((v37 < m.count3) ? ~0u : 0u);
    uint v39 = v30 <= as_type<float>(v38) ? 0xffffffffu : 0u;
    uint v40 = v35 & v39;
    if (v40 != 0u) {
    uint v42 = v14 + v37;
    uint v43 = p2[min(v42, m.count2 - 1u)] & ((v42 < m.count2) ? ~0u : 0u);
    float v44 = as_type<float>(v38) - as_type<float>(v34);
    float v45 = v30 - as_type<float>(v34);
    float v46 = v45 / v44;
    uint v47 = v8 + v37;
    uint v48 = p2[min(v47, m.count2 - 1u)] & ((v47 < m.count2) ? ~0u : 0u);
    uint v49 = v13 + v37;
    uint v50 = p2[min(v49, m.count2 - 1u)] & ((v49 < m.count2) ? ~0u : 0u);
    uint v51 = v14 + v32;
    uint v52 = p2[min(v51, m.count2 - 1u)] & ((v51 < m.count2) ? ~0u : 0u);
    float v53 = as_type<float>(v43) - as_type<float>(v52);
    float v54 = fma(v46, v53, as_type<float>(v52));
    var3 = as_type<uint>(v54);

    uint v56 = v8 + v32;
    uint v57 = p2[min(v56, m.count2 - 1u)] & ((v56 < m.count2) ? ~0u : 0u);
    float v58 = as_type<float>(v48) - as_type<float>(v57);
    float v59 = fma(v46, v58, as_type<float>(v57));
    var1 = as_type<uint>(v59);

    uint v61 = v13 + v32;
    uint v62 = p2[min(v61, m.count2 - 1u)] & ((v61 < m.count2) ? ~0u : 0u);
    float v63 = as_type<float>(v50) - as_type<float>(v62);
    float v64 = fma(v46, v63, as_type<float>(v62));
    var2 = as_type<uint>(v64);

    uint v66 = p2[min(v37, m.count2 - 1u)] & ((v37 < m.count2) ? ~0u : 0u);
    uint v67 = p2[min(v32, m.count2 - 1u)] & ((v32 < m.count2) ? ~0u : 0u);
    float v68 = as_type<float>(v66) - as_type<float>(v67);
    float v69 = fma(v46, v68, as_type<float>(v67));
    var0 = as_type<uint>(v69);

    }
    uint v72 = var4;
    uint v74 = v72 + 1u;
    #define v75 v74
    var4 = v75;

    }
    uint v78 = var0;
    uint v79 = (uint)as_type<ushort>((half)as_type<float>(v78));
    uint v80 = var1;
    uint v81 = (uint)as_type<ushort>((half)as_type<float>(v80));
    uint v82 = var2;
    uint v83 = (uint)as_type<ushort>((half)as_type<float>(v82));
    uint v84 = var3;
    uint v85 = (uint)as_type<ushort>((half)as_type<float>(v84));
    { uint _row = y * m.stride1; uint _ps = m.count1 / 4;
      p1[_row + x] = ushort(v79); p1[_row + x + _ps] = ushort(v81); p1[_row + x + 2*_ps] = ushort(v83); p1[_row + x + 3*_ps] = ushort(v85); }
}
