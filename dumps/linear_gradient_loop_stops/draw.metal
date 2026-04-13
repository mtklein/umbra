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
    uint v25 = p3[min(v24, m.count3 - 1u)] & ((v24 < m.count3) ? ~0u : 0u);
    uint v26 = v21 <= as_type<float>(v25) ? 0xffffffffu : 0u;
    uint v27 = p3[min(v23, m.count3 - 1u)] & ((v23 < m.count3) ? ~0u : 0u);
    uint v28 = as_type<float>(v27) <= v21 ? 0xffffffffu : 0u;
    uint v29 = v28 & v26;
    if (v29 != 0u) {
    uint v31 = v12 + v24;
    uint v32 = p2[min(v31, m.count2 - 1u)] & ((v31 < m.count2) ? ~0u : 0u);
    float v33 = as_type<float>(v25) - as_type<float>(v27);
    float v34 = v21 - as_type<float>(v27);
    float v35 = v34 / v33;
    uint v36 = v8 + v24;
    uint v37 = p2[min(v36, m.count2 - 1u)] & ((v36 < m.count2) ? ~0u : 0u);
    uint v38 = v11 + v24;
    uint v39 = p2[min(v38, m.count2 - 1u)] & ((v38 < m.count2) ? ~0u : 0u);
    uint v40 = v12 + v23;
    uint v41 = p2[min(v40, m.count2 - 1u)] & ((v40 < m.count2) ? ~0u : 0u);
    float v42 = as_type<float>(v32) - as_type<float>(v41);
    float v43 = fma(v35, v42, as_type<float>(v41));
    var3 = as_type<uint>(v43);

    uint v45 = v8 + v23;
    uint v46 = p2[min(v45, m.count2 - 1u)] & ((v45 < m.count2) ? ~0u : 0u);
    float v47 = as_type<float>(v37) - as_type<float>(v46);
    float v48 = fma(v35, v47, as_type<float>(v46));
    var1 = as_type<uint>(v48);

    uint v50 = v11 + v23;
    uint v51 = p2[min(v50, m.count2 - 1u)] & ((v50 < m.count2) ? ~0u : 0u);
    float v52 = as_type<float>(v39) - as_type<float>(v51);
    float v53 = fma(v35, v52, as_type<float>(v51));
    var2 = as_type<uint>(v53);

    uint v55 = p2[min(v24, m.count2 - 1u)] & ((v24 < m.count2) ? ~0u : 0u);
    uint v56 = p2[min(v23, m.count2 - 1u)] & ((v23 < m.count2) ? ~0u : 0u);
    float v57 = as_type<float>(v55) - as_type<float>(v56);
    float v58 = fma(v35, v57, as_type<float>(v56));
    var0 = as_type<uint>(v58);

    }
    uint v61 = var4;
    uint v62 = v61 + 1u;
    var4 = v62;

    }
    uint v65 = var0;
    uint v66 = (uint)as_type<ushort>((half)as_type<float>(v65));
    uint v67 = var1;
    uint v68 = (uint)as_type<ushort>((half)as_type<float>(v67));
    uint v69 = var2;
    uint v70 = (uint)as_type<ushort>((half)as_type<float>(v69));
    uint v71 = var3;
    uint v72 = (uint)as_type<ushort>((half)as_type<float>(v71));
    p1[y * m.stride1 + x] = half4(as_type<half>((ushort)v66), as_type<half>((ushort)v68), as_type<half>((ushort)v70), as_type<half>((ushort)v72));
}
