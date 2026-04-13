#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, count2, stride0, stride1, stride2; };

kernel void umbra_entry(
    constant meta &m [[buffer(3)]],
    device const uint * __restrict p0 [[buffer(0)]],
    device uint * __restrict p1 [[buffer(1)]],
    device const uint * __restrict p2 [[buffer(2)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= m.w) return;
    uint x = m.x0 + pos.x;
    uint y = m.y0 + pos.y;
    uint var0 = 0;
    uint var1 = 0;
    uint v0 = 0u;
    uint v2 = p0[0];
    uint v3 = p0[1];
    uint v4 = p0[2];
    uint v5 = p0[3];
    uint v6 = p0[4];
    uint v7 = p0[5];
    uint v8 = p0[6];
    uint v9 = p0[7];
    uint v10 = p0[8];
    uint v11 = p0[9];
    uint v12 = p0[10];
    uint v13 = 1065353216u;
    uint v14 = 1073741824u;
    uint v15 = 931135488u;
    uint v16 = p0[16];
    uint v17 = m.x0 + pos.x;
    float v18 = (float)(int)v17;
    uint v19 = m.y0 + pos.y;
    float v20 = (float)(int)v19;
    float v21 = v20 * as_type<float>(v9);
    float v22 = fma(v18, as_type<float>(v8), v21);
    float v23 = as_type<float>(v10) + v22;
    float v24 = v20 * as_type<float>(v6);
    float v25 = fma(v18, as_type<float>(v5), v24);
    float v26 = as_type<float>(v7) + v25;
    float v27 = v26 / v23;
    uint v28 = as_type<float>(v0) <= v27 ? 0xffffffffu : 0u;
    uint v29 = v27 <  as_type<float>(v12) ? 0xffffffffu : 0u;
    uint v30 = v28 & v29;
    float v31 = v20 * as_type<float>(v3);
    float v32 = fma(v18, as_type<float>(v2), v31);
    float v33 = as_type<float>(v4) + v32;
    float v34 = v33 / v23;
    uint v35 = as_type<float>(v0) <= v34 ? 0xffffffffu : 0u;
    uint v36 = v34 <  as_type<float>(v11) ? 0xffffffffu : 0u;
    uint v37 = v35 & v36;
    uint v38 = v37 & v30;
    while (var1 < v16) {
    uint v40 = 6u;
    uint v41 = var1;
    uint v42 = v41 * 6u;
    uint v43 = p2[min(v42, m.count2 - 1u)] & ((v42 < m.count2) ? ~0u : 0u);
    float v44 = as_type<float>(v43) - v34;
    uint v45 = 2u;
    uint v46 = v42 + 2u;
    uint v47 = p2[min(v46, m.count2 - 1u)] & ((v46 < m.count2) ? ~0u : 0u);
    float v48 = as_type<float>(v47) - v34;
    float v49 = v48 * as_type<float>(1073741824u);
    float v50 = v44 - v49;
    float v51 = v48 - v44;
    float v52 = v51 * as_type<float>(1073741824u);
    uint v53 = 3u;
    uint v54 = v42 + 3u;
    uint v55 = p2[min(v54, m.count2 - 1u)] & ((v54 < m.count2) ? ~0u : 0u);
    float v56 = as_type<float>(v55) - v27;
    float v57 = v56 * as_type<float>(1073741824u);
    uint v58 = 4u;
    uint v59 = v42 + 4u;
    uint v60 = p2[min(v59, m.count2 - 1u)] & ((v59 < m.count2) ? ~0u : 0u);
    float v61 = as_type<float>(v60) - v34;
    float v62 = v61 + v50;
    uint v63 = 5u;
    uint v64 = v42 + 5u;
    uint v65 = p2[min(v64, m.count2 - 1u)] & ((v64 < m.count2) ? ~0u : 0u);
    float v66 = as_type<float>(v65) - v27;
    uint v67 = 1u;
    uint v68 = v42 + 1u;
    uint v69 = p2[min(v68, m.count2 - 1u)] & ((v68 < m.count2) ? ~0u : 0u);
    float v70 = as_type<float>(v69) - v27;
    float v71 = v70 - v57;
    float v72 = v66 + v71;
    float v73 = fabs(v72);
    uint v74 = as_type<float>(v15) <  v73 ? 0xffffffffu : 0u;
    uint v75 = (v74 != 0u) ? as_type<uint>(v72) : v13;
    float v76 = as_type<float>(v13) / as_type<float>(v75);
    float v77 = v70 - v56;
    float v78 = v77 * v77;
    float v79 = fma(-v70, v72, v78);
    uint v80 = as_type<float>(v0) <= v79 ? 0xffffffffu : 0u;
    uint v81 = v80 & v74;
    float v82 = max(v79, as_type<float>(0u));
    float v83 = precise::sqrt(v82);
    float v84 = v77 - v83;
    float v85 = v76 * v84;
    float v86 = v77 + v83;
    float v87 = v76 * v86;
    uint v88 = as_type<float>(v0) <= v87 ? 0xffffffffu : 0u;
    float v89 = fma(v87, v62, v52);
    float v90 = fma(v87, v89, v44);
    uint v91 = as_type<float>(v0) <  v90 ? 0xffffffffu : 0u;
    float v92 = v72 * v87;
    float v93 = v92 - v77;
    uint v94 = as_type<float>(v0) <  v93 ? 0xffffffffu : 0u;
    float v95 = fabs(v77);
    uint v96 = as_type<float>(v15) <  v95 ? 0xffffffffu : 0u;
    float v97 = v77 * as_type<float>(1073741824u);
    uint v98 = (v96 != 0u) ? as_type<uint>(v97) : v13;
    float v99 = v70 / as_type<float>(v98);
    float v100 = (v74 != 0u) ? v85 : v99;
    uint v101 = as_type<float>(v0) <= v100 ? 0xffffffffu : 0u;
    float v102 = v72 * v100;
    float v103 = v102 - v77;
    uint v104 = as_type<float>(v0) <  v103 ? 0xffffffffu : 0u;
    uint v105 = v100 <  as_type<float>(1065353216u) ? 0xffffffffu : 0u;
    uint v106 = v101 & v105;
    uint v107 = v80 & v106;
    uint v108 = v87 <  as_type<float>(1065353216u) ? 0xffffffffu : 0u;
    uint v109 = v88 & v108;
    uint v110 = v81 & v109;
    uint v111 = v110 & v91;
    float v112 = fma(v100, v62, v52);
    float v113 = fma(v100, v112, v44);
    uint v114 = as_type<float>(v0) <  v113 ? 0xffffffffu : 0u;
    uint v115 = v107 & v114;
    uint v116 = v103 <  as_type<float>(0u) ? 0xffffffffu : 0u;
    uint v117 = 3212836864u;
    uint v118 = (v116 != 0u) ? v117 : v0;
    uint v119 = (v104 != 0u) ? v13 : v118;
    uint v120 = (v115 != 0u) ? v119 : v0;
    uint v121 = v93 <  as_type<float>(0u) ? 0xffffffffu : 0u;
    uint v122 = (v121 != 0u) ? v117 : v0;
    uint v123 = (v94 != 0u) ? v13 : v122;
    uint v124 = (v111 != 0u) ? v123 : v0;
    float v125 = as_type<float>(v120) + as_type<float>(v124);
    uint v126 = (v38 != 0u) ? as_type<uint>(v125) : v0;
    uint v127 = var0;
    float v128 = as_type<float>(v126) + as_type<float>(v127);
    var0 = as_type<uint>(v128);

    uint v130 = var1;
    uint v131 = v130 + 1u;
    var1 = v131;

    }
    uint v134 = var0;
    p1[y * m.stride1 + x] = v134;
}
