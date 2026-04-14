#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, count2, count3, stride0, stride1, stride2, stride3; };

kernel void umbra_entry(
    constant meta &m [[buffer(4)]],
    device const uint * __restrict p0 [[buffer(0)]],
    device half4 * __restrict p1 [[buffer(1)]],
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
    uint v15 = m.y0 + pos.y;
    float v16 = (float)(int)v15;
    float v17 = v16 * as_type<float>(v4);
    float v18 = fma(v14, as_type<float>(v3), v17);
    float v19 = as_type<float>(v5) + v18;
    float v21 = max(v19, as_type<float>(0u));
    #define v22 v21
    float v24 = min(v22, as_type<float>(1065353216u));
    #define v25 v24
    while (var4 < v10) {
    uint v27 = var4;
    uint v29 = v27 + 1u;
    #define v30 v29
    uint v31 = p3[min(v30, m.count3 - 1u)] & ((v30 < m.count3) ? ~0u : 0u);
    uint v32 = v25 <= as_type<float>(v31) ? 0xffffffffu : 0u;
    uint v33 = p3[min(v27, m.count3 - 1u)] & ((v27 < m.count3) ? ~0u : 0u);
    uint v34 = as_type<float>(v33) <= v25 ? 0xffffffffu : 0u;
    uint v35 = v34 & v32;
    if (v35 != 0u) {
    uint v37 = v12 + v30;
    uint v38 = p2[min(v37, m.count2 - 1u)] & ((v37 < m.count2) ? ~0u : 0u);
    float v39 = as_type<float>(v31) - as_type<float>(v33);
    float v40 = v25 - as_type<float>(v33);
    float v41 = v40 / v39;
    uint v42 = v8 + v30;
    uint v43 = p2[min(v42, m.count2 - 1u)] & ((v42 < m.count2) ? ~0u : 0u);
    uint v44 = v11 + v30;
    uint v45 = p2[min(v44, m.count2 - 1u)] & ((v44 < m.count2) ? ~0u : 0u);
    uint v46 = v12 + v27;
    uint v47 = p2[min(v46, m.count2 - 1u)] & ((v46 < m.count2) ? ~0u : 0u);
    float v48 = as_type<float>(v38) - as_type<float>(v47);
    float v49 = fma(v41, v48, as_type<float>(v47));
    var3 = as_type<uint>(v49);

    uint v51 = v8 + v27;
    uint v52 = p2[min(v51, m.count2 - 1u)] & ((v51 < m.count2) ? ~0u : 0u);
    float v53 = as_type<float>(v43) - as_type<float>(v52);
    float v54 = fma(v41, v53, as_type<float>(v52));
    var1 = as_type<uint>(v54);

    uint v56 = v11 + v27;
    uint v57 = p2[min(v56, m.count2 - 1u)] & ((v56 < m.count2) ? ~0u : 0u);
    float v58 = as_type<float>(v45) - as_type<float>(v57);
    float v59 = fma(v41, v58, as_type<float>(v57));
    var2 = as_type<uint>(v59);

    uint v61 = p2[min(v30, m.count2 - 1u)] & ((v30 < m.count2) ? ~0u : 0u);
    uint v62 = p2[min(v27, m.count2 - 1u)] & ((v27 < m.count2) ? ~0u : 0u);
    float v63 = as_type<float>(v61) - as_type<float>(v62);
    float v64 = fma(v41, v63, as_type<float>(v62));
    var0 = as_type<uint>(v64);

    }
    uint v67 = var4;
    uint v69 = v67 + 1u;
    #define v70 v69
    var4 = v70;

    }
    uint v73 = var0;
    uint v74 = (uint)as_type<ushort>((half)as_type<float>(v73));
    uint v75 = var1;
    uint v76 = (uint)as_type<ushort>((half)as_type<float>(v75));
    uint v77 = var2;
    uint v78 = (uint)as_type<ushort>((half)as_type<float>(v77));
    uint v79 = var3;
    uint v80 = (uint)as_type<ushort>((half)as_type<float>(v79));
    p1[y * m.stride1 + x] = half4(as_type<half>((ushort)v74), as_type<half>((ushort)v76), as_type<half>((ushort)v78), as_type<half>((ushort)v80));
}
