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
    uint v6 = 1065353216u;
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
    float v23 = max(v22, as_type<float>(0u));
    float v24 = min(v23, as_type<float>(1065353216u));
    while (var4 < v10) {
    uint v26 = var4;
    uint v27 = v26 + 1u;
    uint v28 = p3[min(v27, m.count3 - 1u)] & ((v27 < m.count3) ? ~0u : 0u);
    uint v29 = v24 <= as_type<float>(v28) ? 0xffffffffu : 0u;
    uint v30 = p3[min(v26, m.count3 - 1u)] & ((v26 < m.count3) ? ~0u : 0u);
    uint v31 = as_type<float>(v30) <= v24 ? 0xffffffffu : 0u;
    uint v32 = v31 & v29;
    if (v32 != 0u) {
    uint v34 = v12 + v27;
    uint v35 = p2[min(v34, m.count2 - 1u)] & ((v34 < m.count2) ? ~0u : 0u);
    float v36 = as_type<float>(v28) - as_type<float>(v30);
    float v37 = v24 - as_type<float>(v30);
    float v38 = v37 / v36;
    uint v39 = v8 + v27;
    uint v40 = p2[min(v39, m.count2 - 1u)] & ((v39 < m.count2) ? ~0u : 0u);
    uint v41 = v11 + v27;
    uint v42 = p2[min(v41, m.count2 - 1u)] & ((v41 < m.count2) ? ~0u : 0u);
    uint v43 = v12 + v26;
    uint v44 = p2[min(v43, m.count2 - 1u)] & ((v43 < m.count2) ? ~0u : 0u);
    float v45 = as_type<float>(v35) - as_type<float>(v44);
    float v46 = fma(v38, v45, as_type<float>(v44));
    var3 = as_type<uint>(v46);

    uint v48 = v8 + v26;
    uint v49 = p2[min(v48, m.count2 - 1u)] & ((v48 < m.count2) ? ~0u : 0u);
    float v50 = as_type<float>(v40) - as_type<float>(v49);
    float v51 = fma(v38, v50, as_type<float>(v49));
    var1 = as_type<uint>(v51);

    uint v53 = v11 + v26;
    uint v54 = p2[min(v53, m.count2 - 1u)] & ((v53 < m.count2) ? ~0u : 0u);
    float v55 = as_type<float>(v42) - as_type<float>(v54);
    float v56 = fma(v38, v55, as_type<float>(v54));
    var2 = as_type<uint>(v56);

    uint v58 = p2[min(v27, m.count2 - 1u)] & ((v27 < m.count2) ? ~0u : 0u);
    uint v59 = p2[min(v26, m.count2 - 1u)] & ((v26 < m.count2) ? ~0u : 0u);
    float v60 = as_type<float>(v58) - as_type<float>(v59);
    float v61 = fma(v38, v60, as_type<float>(v59));
    var0 = as_type<uint>(v61);

    }
    uint v64 = var4;
    uint v65 = v64 + 1u;
    var4 = v65;

    }
    uint v68 = var0;
    uint v69 = (uint)as_type<ushort>((half)as_type<float>(v68));
    uint v70 = var1;
    uint v71 = (uint)as_type<ushort>((half)as_type<float>(v70));
    uint v72 = var2;
    uint v73 = (uint)as_type<ushort>((half)as_type<float>(v72));
    uint v74 = var3;
    uint v75 = (uint)as_type<ushort>((half)as_type<float>(v74));
    { uint _row = y * m.stride1; uint _ps = m.count1 / 4;
      p1[_row + x] = ushort(v69); p1[_row + x + _ps] = ushort(v71); p1[_row + x + 2*_ps] = ushort(v73); p1[_row + x + 3*_ps] = ushort(v75); }
}
