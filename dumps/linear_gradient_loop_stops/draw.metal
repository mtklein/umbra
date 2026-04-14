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
    uint v11 = v8 - 1u;
    #define v12 v11
    uint v13 = v8 + v8;
    uint v14 = v8 + v13;
    uint v15 = m.x0 + pos.x;
    float v16 = (float)(int)v15;
    uint v17 = m.y0 + pos.y;
    float v18 = (float)(int)v17;
    float v19 = v18 * as_type<float>(v4);
    float v20 = fma(v16, as_type<float>(v3), v19);
    float v21 = as_type<float>(v5) + v20;
    float v23 = max(v21, as_type<float>(0u));
    #define v24 v23
    float v26 = min(v24, as_type<float>(1065353216u));
    #define v27 v26
    while (var4 < v12) {
    uint v29 = var4;
    uint v31 = v29 + 1u;
    #define v32 v31
    uint v33 = p3[min(v32, m.count3 - 1u)] & ((v32 < m.count3) ? ~0u : 0u);
    uint v34 = v27 <= as_type<float>(v33) ? 0xffffffffu : 0u;
    uint v35 = p3[min(v29, m.count3 - 1u)] & ((v29 < m.count3) ? ~0u : 0u);
    uint v36 = as_type<float>(v35) <= v27 ? 0xffffffffu : 0u;
    uint v37 = v36 & v34;
    if (v37 != 0u) {
    uint v39 = v14 + v32;
    uint v40 = p2[min(v39, m.count2 - 1u)] & ((v39 < m.count2) ? ~0u : 0u);
    float v41 = as_type<float>(v33) - as_type<float>(v35);
    float v42 = v27 - as_type<float>(v35);
    float v43 = v42 / v41;
    uint v44 = v8 + v32;
    uint v45 = p2[min(v44, m.count2 - 1u)] & ((v44 < m.count2) ? ~0u : 0u);
    uint v46 = v13 + v32;
    uint v47 = p2[min(v46, m.count2 - 1u)] & ((v46 < m.count2) ? ~0u : 0u);
    uint v48 = v14 + v29;
    uint v49 = p2[min(v48, m.count2 - 1u)] & ((v48 < m.count2) ? ~0u : 0u);
    float v50 = as_type<float>(v40) - as_type<float>(v49);
    float v51 = fma(v43, v50, as_type<float>(v49));
    var3 = as_type<uint>(v51);

    uint v53 = v8 + v29;
    uint v54 = p2[min(v53, m.count2 - 1u)] & ((v53 < m.count2) ? ~0u : 0u);
    float v55 = as_type<float>(v45) - as_type<float>(v54);
    float v56 = fma(v43, v55, as_type<float>(v54));
    var1 = as_type<uint>(v56);

    uint v58 = v13 + v29;
    uint v59 = p2[min(v58, m.count2 - 1u)] & ((v58 < m.count2) ? ~0u : 0u);
    float v60 = as_type<float>(v47) - as_type<float>(v59);
    float v61 = fma(v43, v60, as_type<float>(v59));
    var2 = as_type<uint>(v61);

    uint v63 = p2[min(v32, m.count2 - 1u)] & ((v32 < m.count2) ? ~0u : 0u);
    uint v64 = p2[min(v29, m.count2 - 1u)] & ((v29 < m.count2) ? ~0u : 0u);
    float v65 = as_type<float>(v63) - as_type<float>(v64);
    float v66 = fma(v43, v65, as_type<float>(v64));
    var0 = as_type<uint>(v66);

    }
    uint v69 = var4;
    uint v71 = v69 + 1u;
    #define v72 v71
    var4 = v72;

    }
    uint v75 = var0;
    uint v76 = (uint)as_type<ushort>((half)as_type<float>(v75));
    uint v77 = var1;
    uint v78 = (uint)as_type<ushort>((half)as_type<float>(v77));
    uint v79 = var2;
    uint v80 = (uint)as_type<ushort>((half)as_type<float>(v79));
    uint v81 = var3;
    uint v82 = (uint)as_type<ushort>((half)as_type<float>(v81));
    p1[y * m.stride1 + x] = half4(as_type<half>((ushort)v76), as_type<half>((ushort)v78), as_type<half>((ushort)v80), as_type<half>((ushort)v82));
}
