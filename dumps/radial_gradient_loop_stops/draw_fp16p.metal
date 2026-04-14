#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, count2, count3, stride0, stride1, stride2, stride3; };

kernel void umbra_entry(
    constant meta &m [[buffer(4)]],
    device const uint * __restrict p0 [[buffer(0)]],
    device ushort * __restrict p1 [[buffer(1)]],
    device const uint * __restrict p2 [[buffer(2)]],
    device const uint * __restrict p3 [[buffer(3)]],
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
    uint v9 = 1u;
    uint v10 = v8 - 1u;
    uint v11 = v8 + v8;
    uint v12 = v8 + v11;
    uint v13 = m.x0 + pos.x;
    float v14 = (float)(int)v13;
    float v15 = v14 - as_type<float>(v3);
    uint v16 = m.y0 + pos.y;
    float v17 = (float)(int)v16;
    float v18 = v17 - as_type<float>(v4);
    float v19 = v18 * v18;
    float v20 = fma(v15, v15, v19);
    float v21 = precise::sqrt(v20);
    float v22 = as_type<float>(v5) * v21;
    float v24 = max(v22, as_type<float>(0u));
    #define v25 v24
    float v27 = min(v25, as_type<float>(1065353216u));
    #define v28 v27
    while (var4 < v10) {
    uint v30 = var4;
    uint v32 = v30 + 1u;
    #define v33 v32
    uint v34 = p3[min(v33, m.count3 - 1u)] & ((v33 < m.count3) ? ~0u : 0u);
    uint v35 = v28 <= as_type<float>(v34) ? 0xffffffffu : 0u;
    uint v36 = p3[min(v30, m.count3 - 1u)] & ((v30 < m.count3) ? ~0u : 0u);
    uint v37 = as_type<float>(v36) <= v28 ? 0xffffffffu : 0u;
    uint v38 = v37 & v35;
    if (v38 != 0u) {
    uint v40 = v12 + v33;
    uint v41 = p2[min(v40, m.count2 - 1u)] & ((v40 < m.count2) ? ~0u : 0u);
    float v42 = as_type<float>(v34) - as_type<float>(v36);
    float v43 = v28 - as_type<float>(v36);
    float v44 = v43 / v42;
    uint v45 = v8 + v33;
    uint v46 = p2[min(v45, m.count2 - 1u)] & ((v45 < m.count2) ? ~0u : 0u);
    uint v47 = v11 + v33;
    uint v48 = p2[min(v47, m.count2 - 1u)] & ((v47 < m.count2) ? ~0u : 0u);
    uint v49 = v12 + v30;
    uint v50 = p2[min(v49, m.count2 - 1u)] & ((v49 < m.count2) ? ~0u : 0u);
    float v51 = as_type<float>(v41) - as_type<float>(v50);
    float v52 = fma(v44, v51, as_type<float>(v50));
    var3 = as_type<uint>(v52);

    uint v54 = v8 + v30;
    uint v55 = p2[min(v54, m.count2 - 1u)] & ((v54 < m.count2) ? ~0u : 0u);
    float v56 = as_type<float>(v46) - as_type<float>(v55);
    float v57 = fma(v44, v56, as_type<float>(v55));
    var1 = as_type<uint>(v57);

    uint v59 = v11 + v30;
    uint v60 = p2[min(v59, m.count2 - 1u)] & ((v59 < m.count2) ? ~0u : 0u);
    float v61 = as_type<float>(v48) - as_type<float>(v60);
    float v62 = fma(v44, v61, as_type<float>(v60));
    var2 = as_type<uint>(v62);

    uint v64 = p2[min(v33, m.count2 - 1u)] & ((v33 < m.count2) ? ~0u : 0u);
    uint v65 = p2[min(v30, m.count2 - 1u)] & ((v30 < m.count2) ? ~0u : 0u);
    float v66 = as_type<float>(v64) - as_type<float>(v65);
    float v67 = fma(v44, v66, as_type<float>(v65));
    var0 = as_type<uint>(v67);

    }
    uint v70 = var4;
    uint v72 = v70 + 1u;
    #define v73 v72
    var4 = v73;

    }
    uint v76 = var0;
    uint v77 = (uint)as_type<ushort>((half)as_type<float>(v76));
    uint v78 = var1;
    uint v79 = (uint)as_type<ushort>((half)as_type<float>(v78));
    uint v80 = var2;
    uint v81 = (uint)as_type<ushort>((half)as_type<float>(v80));
    uint v82 = var3;
    uint v83 = (uint)as_type<ushort>((half)as_type<float>(v82));
    { uint _row = y * m.stride1; uint _ps = m.count1 / 4;
      p1[_row + x] = ushort(v77); p1[_row + x + _ps] = ushort(v79); p1[_row + x + 2*_ps] = ushort(v81); p1[_row + x + 3*_ps] = ushort(v83); }
}
