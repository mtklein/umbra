#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, count2, stride0, stride1, stride2; };

kernel void umbra_entry(
    constant meta &m [[buffer(0)]],
    device const uint * __restrict p0 [[buffer(1)]],
    device ushort * __restrict p1 [[buffer(2)]],
    device const ushort * __restrict p2 [[buffer(3)]],
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
    float v19 = as_type<float>(v15) - as_type<float>(1065353216u);
    #define v20 v19
    float v22 = as_type<float>(v16) - as_type<float>(1065353216u);
    #define v23 v22
    uint v24 = (uint)(int)floor(as_type<float>(v15));
    float v26 = as_type<float>(v17) - as_type<float>(v4);
    uint _row27 = y * m.stride1; uint _ps27 = m.count1 / 4;
    uint v27 = (uint)p1[_row27 + x];
    uint v27_1 = (uint)p1[_row27 + x + _ps27];
    uint v27_2 = (uint)p1[_row27 + x + 2*_ps27];
    uint v27_3 = (uint)p1[_row27 + x + 3*_ps27];
    float v28 = (float)as_type<half>((ushort)v27);
    float v29 = fma(v28, v26, as_type<float>(v1));
    float v30 = v29 - v28;
    uint v31 = m.x0 + pos.x;
    float v32 = (float)(int)v31;
    uint v33 = m.y0 + pos.y;
    float v34 = (float)(int)v33;
    float v35 = v34 * as_type<float>(v13);
    float v36 = fma(v32, as_type<float>(v12), v35);
    float v37 = as_type<float>(v14) + v36;
    float v38 = v34 * as_type<float>(v10);
    float v39 = fma(v32, as_type<float>(v9), v38);
    float v40 = as_type<float>(v11) + v39;
    float v41 = v40 / v37;
    uint v42 = as_type<float>(v0) <= v41 ? 0xffffffffu : 0u;
    float v43 = v34 * as_type<float>(v7);
    float v44 = fma(v32, as_type<float>(v6), v43);
    float v45 = as_type<float>(v8) + v44;
    float v46 = v45 / v37;
    uint v47 = as_type<float>(v0) <= v46 ? 0xffffffffu : 0u;
    uint v48 = v46 <  as_type<float>(v15) ? 0xffffffffu : 0u;
    uint v49 = v47 & v48;
    uint v50 = v41 <  as_type<float>(v16) ? 0xffffffffu : 0u;
    uint v51 = v42 & v50;
    uint v52 = v49 & v51;
    float v53 = (float)as_type<half>((ushort)v27_3);
    float v54 = fma(v53, v26, as_type<float>(v4));
    float v55 = v54 - v53;
    float v56 = max(v46, as_type<float>(0u));
    #define v58 v56
    float v59 = min(v58, v20);
    uint v60 = (uint)(int)floor(v59);
    float v61 = max(v41, as_type<float>(0u));
    #define v63 v61
    float v64 = min(v63, v23);
    uint v65 = (uint)(int)floor(v64);
    uint v66 = v65 * v24;
    uint v67 = v60 + v66;
    uint v68 = (uint)p2[min(v67, m.count2 - 1u)] & ((v67 < m.count2) ? ~0u : 0u);
    uint v69 = (uint)(int)(short)(ushort)v68;
    float v70 = (float)(int)v69;
    float v72 = v70 * as_type<float>(998277249u);
    #define v73 v72
    uint v74 = (v52 != 0u) ? as_type<uint>(v73) : v0;
    float v75 = fma(as_type<float>(v74), v30, v28);
    uint v76 = (uint)as_type<ushort>((half)v75);
    float v77 = fma(as_type<float>(v74), v55, v53);
    uint v78 = (uint)as_type<ushort>((half)v77);
    float v79 = (float)as_type<half>((ushort)v27_1);
    float v80 = fma(v79, v26, as_type<float>(v2));
    float v81 = v80 - v79;
    float v82 = fma(as_type<float>(v74), v81, v79);
    uint v83 = (uint)as_type<ushort>((half)v82);
    float v84 = (float)as_type<half>((ushort)v27_2);
    float v85 = fma(v84, v26, as_type<float>(v3));
    float v86 = v85 - v84;
    float v87 = fma(as_type<float>(v74), v86, v84);
    uint v88 = (uint)as_type<ushort>((half)v87);
    { uint _row = y * m.stride1; uint _ps = m.count1 / 4;
      p1[_row + x] = ushort(v76); p1[_row + x + _ps] = ushort(v83); p1[_row + x + 2*_ps] = ushort(v88); p1[_row + x + 3*_ps] = ushort(v78); }
}
