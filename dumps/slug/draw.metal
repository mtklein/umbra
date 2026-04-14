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
    uint v41 = var1;
    uint v43 = v41 * 6u;
    #define v44 v43
    uint v45 = p2[min(v44, m.count2 - 1u)] & ((v44 < m.count2) ? ~0u : 0u);
    float v46 = as_type<float>(v45) - v34;
    uint v49 = v44 + 2u;
    #define v50 v49
    uint v51 = p2[min(v50, m.count2 - 1u)] & ((v50 < m.count2) ? ~0u : 0u);
    float v52 = as_type<float>(v51) - v34;
    float v54 = v52 - v46;
    float v56 = v54 * as_type<float>(1073741824u);
    #define v57 v56
    uint v60 = v44 + 3u;
    #define v61 v60
    uint v62 = p2[min(v61, m.count2 - 1u)] & ((v61 < m.count2) ? ~0u : 0u);
    float v63 = as_type<float>(v62) - v27;
    uint v67 = v44 + 4u;
    #define v68 v67
    uint v69 = p2[min(v68, m.count2 - 1u)] & ((v68 < m.count2) ? ~0u : 0u);
    float v70 = as_type<float>(v69) - v34;
    uint v73 = v44 + 5u;
    #define v74 v73
    uint v75 = p2[min(v74, m.count2 - 1u)] & ((v74 < m.count2) ? ~0u : 0u);
    float v76 = as_type<float>(v75) - v27;
    uint v79 = var1;
    uint v81 = v79 + 1u;
    #define v82 v81
    var1 = v82;

    uint v84 = v44 + 1u;
    #define v85 v84
    uint v86 = p2[min(v85, m.count2 - 1u)] & ((v85 < m.count2) ? ~0u : 0u);
    float v87 = as_type<float>(v86) - v27;
    float v88 = v87 - v63;
    float v89 = v88 * v88;
    float v90 = v63 * as_type<float>(1073741824u);
    #define v91 v90
    float v92 = v87 - v91;
    float v93 = v76 + v92;
    float v94 = fma(-v87, v93, v89);
    uint v95 = as_type<float>(v0) <= v94 ? 0xffffffffu : 0u;
    float v96 = max(v94, as_type<float>(0u));
    #define v98 v96
    float v99 = precise::sqrt(v98);
    float v100 = v88 - v99;
    float v101 = fabs(v93);
    uint v102 = as_type<float>(v15) <  v101 ? 0xffffffffu : 0u;
    uint v103 = (v102 != 0u) ? as_type<uint>(v93) : v13;
    float v104 = as_type<float>(v13) / as_type<float>(v103);
    float v105 = v104 * v100;
    float v106 = v88 + v99;
    float v107 = v104 * v106;
    uint v108 = as_type<float>(v0) <= v107 ? 0xffffffffu : 0u;
    float v109 = v93 * v107;
    float v110 = v109 - v88;
    uint v111 = as_type<float>(v0) <  v110 ? 0xffffffffu : 0u;
    uint v112 = v95 & v102;
    float v113 = fabs(v88);
    uint v114 = as_type<float>(v15) <  v113 ? 0xffffffffu : 0u;
    float v115 = v52 * as_type<float>(1073741824u);
    #define v116 v115
    float v117 = v46 - v116;
    float v118 = v70 + v117;
    float v119 = fma(v107, v118, v57);
    float v120 = fma(v107, v119, v46);
    uint v121 = as_type<float>(v0) <  v120 ? 0xffffffffu : 0u;
    uint v122 = v110 <  as_type<float>(0u) ? 0xffffffffu : 0u;
    #define v124 v122
    uint v125 = 3212836864u;
    uint v126 = (v124 != 0u) ? v125 : v0;
    uint v127 = (v111 != 0u) ? v13 : v126;
    float v129 = v88 * as_type<float>(1073741824u);
    #define v130 v129
    uint v131 = (v114 != 0u) ? as_type<uint>(v130) : v13;
    float v132 = v87 / as_type<float>(v131);
    float v133 = (v102 != 0u) ? v105 : v132;
    uint v134 = as_type<float>(v0) <= v133 ? 0xffffffffu : 0u;
    float v135 = v93 * v133;
    float v136 = v135 - v88;
    uint v137 = as_type<float>(v0) <  v136 ? 0xffffffffu : 0u;
    float v138 = fma(v133, v118, v57);
    float v139 = fma(v133, v138, v46);
    uint v140 = as_type<float>(v0) <  v139 ? 0xffffffffu : 0u;
    uint v141 = v136 <  as_type<float>(0u) ? 0xffffffffu : 0u;
    #define v143 v141
    uint v144 = (v143 != 0u) ? v125 : v0;
    uint v145 = (v137 != 0u) ? v13 : v144;
    uint v146 = v133 <  as_type<float>(1065353216u) ? 0xffffffffu : 0u;
    #define v148 v146
    uint v149 = v134 & v148;
    uint v150 = v95 & v149;
    uint v151 = v150 & v140;
    uint v152 = (v151 != 0u) ? v145 : v0;
    uint v154 = v107 <  as_type<float>(1065353216u) ? 0xffffffffu : 0u;
    #define v155 v154
    uint v156 = v108 & v155;
    uint v157 = v112 & v156;
    uint v158 = v157 & v121;
    uint v159 = (v158 != 0u) ? v127 : v0;
    float v160 = as_type<float>(v152) + as_type<float>(v159);
    uint v161 = (v38 != 0u) ? as_type<uint>(v160) : v0;
    uint v162 = var0;
    float v163 = as_type<float>(v161) + as_type<float>(v162);
    var0 = as_type<uint>(v163);

    }
    uint v166 = var0;
    p1[y * m.stride1 + x] = v166;
}
