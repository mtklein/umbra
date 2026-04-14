#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, count2, stride0, stride1, stride2; };

kernel void umbra_entry(
    constant meta &m [[buffer(3)]],
    device const uint * __restrict p0 [[buffer(0)]],
    device ushort * __restrict p1 [[buffer(1)]],
    device const ushort * __restrict p2 [[buffer(2)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= m.w) return;
    uint x = m.x0 + pos.x;
    uint y = m.y0 + pos.y;
    uint v0 = 0u;
    uint v1 = p0[0];
    uint v2 = p0[1];
    uint v3 = p0[2];
    uint v4 = p0[3];
    uint v6 = p0[4];
    uint v7 = p0[5];
    uint v8 = p0[6];
    uint v9 = p0[7];
    uint v10 = p0[8];
    uint v11 = p0[9];
    uint v12 = p0[10];
    uint v13 = p0[11];
    uint v14 = p0[12];
    uint v15 = p0[13];
    uint v16 = p0[14];
    uint v17 = 1065353216u;
    float v18 = as_type<float>(v15) - as_type<float>(1065353216u);
    float v19 = as_type<float>(v16) - as_type<float>(1065353216u);
    uint v20 = (uint)(int)floor(as_type<float>(v15));
    float v22 = as_type<float>(v17) - as_type<float>(v4);
    uint _row23 = y * m.stride1; uint _ps23 = m.count1 / 4;
    uint v23 = (uint)p1[_row23 + x];
    uint v23_1 = (uint)p1[_row23 + x + _ps23];
    uint v23_2 = (uint)p1[_row23 + x + 2*_ps23];
    uint v23_3 = (uint)p1[_row23 + x + 3*_ps23];
    float v24 = (float)as_type<half>((ushort)v23);
    float v25 = fma(v24, v22, as_type<float>(v1));
    float v26 = v25 - v24;
    uint v27 = m.x0 + pos.x;
    float v28 = (float)(int)v27;
    uint v29 = m.y0 + pos.y;
    float v30 = (float)(int)v29;
    float v31 = v30 * as_type<float>(v13);
    float v32 = fma(v28, as_type<float>(v12), v31);
    float v33 = as_type<float>(v14) + v32;
    float v34 = v30 * as_type<float>(v10);
    float v35 = fma(v28, as_type<float>(v9), v34);
    float v36 = as_type<float>(v11) + v35;
    float v37 = v36 / v33;
    uint v38 = as_type<float>(v0) <= v37 ? 0xffffffffu : 0u;
    float v39 = v30 * as_type<float>(v7);
    float v40 = fma(v28, as_type<float>(v6), v39);
    float v41 = as_type<float>(v8) + v40;
    float v42 = v41 / v33;
    uint v43 = as_type<float>(v0) <= v42 ? 0xffffffffu : 0u;
    uint v44 = v42 <  as_type<float>(v15) ? 0xffffffffu : 0u;
    uint v45 = v43 & v44;
    uint v46 = v37 <  as_type<float>(v16) ? 0xffffffffu : 0u;
    uint v47 = v38 & v46;
    uint v48 = v45 & v47;
    float v49 = (float)as_type<half>((ushort)v23_3);
    float v50 = fma(v49, v22, as_type<float>(v4));
    float v51 = v50 - v49;
    float v52 = max(v42, as_type<float>(0u));
    #define v54 v52
    float v55 = min(v54, v18);
    uint v56 = (uint)(int)floor(v55);
    float v57 = max(v37, as_type<float>(0u));
    #define v59 v57
    float v60 = min(v59, v19);
    uint v61 = (uint)(int)floor(v60);
    uint v62 = v61 * v20;
    uint v63 = v56 + v62;
    uint v64 = (uint)p2[min(v63, m.count2 - 1u)] & ((v63 < m.count2) ? ~0u : 0u);
    uint v65 = (uint)(int)(short)(ushort)v64;
    float v66 = (float)(int)v65;
    float v68 = v66 * as_type<float>(998277249u);
    #define v69 v68
    uint v70 = (v48 != 0u) ? as_type<uint>(v69) : v0;
    float v71 = fma(as_type<float>(v70), v26, v24);
    uint v72 = (uint)as_type<ushort>((half)v71);
    float v73 = fma(as_type<float>(v70), v51, v49);
    uint v74 = (uint)as_type<ushort>((half)v73);
    float v75 = (float)as_type<half>((ushort)v23_1);
    float v76 = fma(v75, v22, as_type<float>(v2));
    float v77 = v76 - v75;
    float v78 = fma(as_type<float>(v70), v77, v75);
    uint v79 = (uint)as_type<ushort>((half)v78);
    float v80 = (float)as_type<half>((ushort)v23_2);
    float v81 = fma(v80, v22, as_type<float>(v3));
    float v82 = v81 - v80;
    float v83 = fma(as_type<float>(v70), v82, v80);
    uint v84 = (uint)as_type<ushort>((half)v83);
    { uint _row = y * m.stride1; uint _ps = m.count1 / 4;
      p1[_row + x] = ushort(v72); p1[_row + x + _ps] = ushort(v79); p1[_row + x + 2*_ps] = ushort(v84); p1[_row + x + 3*_ps] = ushort(v74); }
}
