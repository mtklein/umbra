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
    uint v30 = v12 + v24;
    uint v31 = p2[min(v30, m.count2 - 1u)] & ((v30 < m.count2) ? ~0u : 0u);
    float v32 = as_type<float>(v25) - as_type<float>(v27);
    float v33 = v21 - as_type<float>(v27);
    float v34 = v33 / v32;
    uint v35 = v8 + v24;
    uint v36 = p2[min(v35, m.count2 - 1u)] & ((v35 < m.count2) ? ~0u : 0u);
    uint v37 = v11 + v24;
    uint v38 = p2[min(v37, m.count2 - 1u)] & ((v37 < m.count2) ? ~0u : 0u);
    uint v39 = v8 + v23;
    uint v40 = p2[min(v39, m.count2 - 1u)] & ((v39 < m.count2) ? ~0u : 0u);
    float v41 = as_type<float>(v36) - as_type<float>(v40);
    float v42 = fma(v34, v41, as_type<float>(v40));
    uint v43 = v11 + v23;
    uint v44 = p2[min(v43, m.count2 - 1u)] & ((v43 < m.count2) ? ~0u : 0u);
    float v45 = as_type<float>(v38) - as_type<float>(v44);
    float v46 = fma(v34, v45, as_type<float>(v44));
    uint v47 = v12 + v23;
    uint v48 = p2[min(v47, m.count2 - 1u)] & ((v47 < m.count2) ? ~0u : 0u);
    float v49 = as_type<float>(v31) - as_type<float>(v48);
    float v50 = fma(v34, v49, as_type<float>(v48));
    uint v51 = var1;
    uint v52 = (v29 != 0u) ? as_type<uint>(v42) : v51;
    var1 = v52;

    uint v54 = var2;
    uint v55 = (v29 != 0u) ? as_type<uint>(v46) : v54;
    var2 = v55;

    uint v57 = var3;
    uint v58 = (v29 != 0u) ? as_type<uint>(v50) : v57;
    var3 = v58;

    uint v60 = var4;
    uint v61 = v60 + 1u;
    var4 = v61;

    uint v63 = p2[min(v24, m.count2 - 1u)] & ((v24 < m.count2) ? ~0u : 0u);
    uint v64 = p2[min(v23, m.count2 - 1u)] & ((v23 < m.count2) ? ~0u : 0u);
    float v65 = as_type<float>(v63) - as_type<float>(v64);
    float v66 = fma(v34, v65, as_type<float>(v64));
    uint v67 = var0;
    uint v68 = (v29 != 0u) ? as_type<uint>(v66) : v67;
    var0 = v68;

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
