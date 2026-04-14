#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, count2, stride0, stride1, stride2; };

kernel void main0(
    constant meta &m [[buffer(0)]],
    device const uint * __restrict p0 [[buffer(1)]],
    device uint * __restrict p1 [[buffer(2)]],
    device const uint * __restrict p2 [[buffer(3)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= m.w) return;
    uint x = m.x0 + pos.x;
    uint y = m.y0 + pos.y;
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
    uint v19 = v16 * 6u;
    #define v20 v19
    uint v21 = p2[min(v20, m.count2 - 1u)] & ((v20 < m.count2) ? ~0u : 0u);
    uint v24 = v20 + 1u;
    #define v25 v24
    uint v26 = p2[min(v25, m.count2 - 1u)] & ((v25 < m.count2) ? ~0u : 0u);
    uint v29 = v20 + 2u;
    #define v30 v29
    uint v31 = p2[min(v30, m.count2 - 1u)] & ((v30 < m.count2) ? ~0u : 0u);
    uint v34 = v20 + 3u;
    #define v35 v34
    uint v36 = p2[min(v35, m.count2 - 1u)] & ((v35 < m.count2) ? ~0u : 0u);
    uint v39 = v20 + 4u;
    #define v40 v39
    uint v41 = p2[min(v40, m.count2 - 1u)] & ((v40 < m.count2) ? ~0u : 0u);
    uint v44 = v20 + 5u;
    #define v45 v44
    uint v46 = p2[min(v45, m.count2 - 1u)] & ((v45 < m.count2) ? ~0u : 0u);
    uint v47 = 3212836864u;
    uint v48 = m.x0 + pos.x;
    float v49 = (float)(int)v48;
    uint v50 = m.y0 + pos.y;
    float v51 = (float)(int)v50;
    float v52 = v51 * as_type<float>(v9);
    float v53 = fma(v49, as_type<float>(v8), v52);
    float v54 = as_type<float>(v10) + v53;
    float v55 = v51 * as_type<float>(v6);
    float v56 = fma(v49, as_type<float>(v5), v55);
    float v57 = as_type<float>(v7) + v56;
    float v58 = v57 / v54;
    uint v59 = as_type<float>(v0) <= v58 ? 0xffffffffu : 0u;
    float v60 = as_type<float>(v36) - v58;
    float v62 = v51 * as_type<float>(v3);
    float v63 = fma(v49, as_type<float>(v2), v62);
    float v64 = as_type<float>(v4) + v63;
    float v65 = v64 / v54;
    uint v66 = as_type<float>(v0) <= v65 ? 0xffffffffu : 0u;
    float v67 = as_type<float>(v31) - v65;
    uint v69 = v65 <  as_type<float>(v11) ? 0xffffffffu : 0u;
    uint v70 = v66 & v69;
    uint v71 = v58 <  as_type<float>(v12) ? 0xffffffffu : 0u;
    uint v72 = v59 & v71;
    uint v73 = v70 & v72;
    float v74 = as_type<float>(v46) - v58;
    float v75 = as_type<float>(v26) - v58;
    float v76 = v75 - v60;
    float v77 = v76 * v76;
    float v78 = as_type<float>(v41) - v65;
    float v79 = as_type<float>(v21) - v65;
    float v80 = v67 - v79;
    float v82 = v80 * as_type<float>(1073741824u);
    #define v83 v82
    float v84 = v60 * as_type<float>(1073741824u);
    #define v85 v84
    float v86 = v75 - v85;
    float v87 = v74 + v86;
    float v88 = fma(-v75, v87, v77);
    uint v89 = as_type<float>(v0) <= v88 ? 0xffffffffu : 0u;
    float v90 = max(v88, as_type<float>(0u));
    #define v92 v90
    float v93 = precise::sqrt(v92);
    float v94 = v76 - v93;
    float v95 = fabs(v87);
    uint v96 = as_type<float>(v15) <  v95 ? 0xffffffffu : 0u;
    uint v97 = (v96 != 0u) ? as_type<uint>(v87) : v13;
    float v98 = as_type<float>(v13) / as_type<float>(v97);
    float v99 = v98 * v94;
    float v100 = v76 + v93;
    float v101 = v98 * v100;
    uint v102 = as_type<float>(v0) <= v101 ? 0xffffffffu : 0u;
    float v103 = v87 * v101;
    float v104 = v103 - v76;
    uint v105 = as_type<float>(v0) <  v104 ? 0xffffffffu : 0u;
    uint v106 = v89 & v96;
    float v107 = fabs(v76);
    uint v108 = as_type<float>(v15) <  v107 ? 0xffffffffu : 0u;
    float v109 = v67 * as_type<float>(1073741824u);
    #define v110 v109
    float v111 = v79 - v110;
    float v112 = v78 + v111;
    float v113 = fma(v101, v112, v83);
    float v114 = fma(v101, v113, v79);
    uint v115 = as_type<float>(v0) <  v114 ? 0xffffffffu : 0u;
    uint v116 = v104 <  as_type<float>(0u) ? 0xffffffffu : 0u;
    #define v118 v116
    uint v119 = (v118 != 0u) ? v47 : v0;
    uint v120 = (v105 != 0u) ? v13 : v119;
    float v121 = v76 * as_type<float>(1073741824u);
    #define v123 v121
    uint v124 = (v108 != 0u) ? as_type<uint>(v123) : v13;
    float v125 = v75 / as_type<float>(v124);
    float v126 = (v96 != 0u) ? v99 : v125;
    uint v127 = as_type<float>(v0) <= v126 ? 0xffffffffu : 0u;
    float v128 = v87 * v126;
    float v129 = v128 - v76;
    uint v130 = as_type<float>(v0) <  v129 ? 0xffffffffu : 0u;
    float v131 = fma(v126, v112, v83);
    float v132 = fma(v126, v131, v79);
    uint v133 = as_type<float>(v0) <  v132 ? 0xffffffffu : 0u;
    uint v134 = v129 <  as_type<float>(0u) ? 0xffffffffu : 0u;
    #define v136 v134
    uint v137 = (v136 != 0u) ? v47 : v0;
    uint v138 = (v130 != 0u) ? v13 : v137;
    uint v139 = v126 <  as_type<float>(1065353216u) ? 0xffffffffu : 0u;
    #define v141 v139
    uint v142 = v127 & v141;
    uint v143 = v89 & v142;
    uint v144 = v143 & v133;
    uint v145 = (v144 != 0u) ? v138 : v0;
    uint v146 = v101 <  as_type<float>(1065353216u) ? 0xffffffffu : 0u;
    #define v148 v146
    uint v149 = v102 & v148;
    uint v150 = v106 & v149;
    uint v151 = v150 & v115;
    uint v152 = (v151 != 0u) ? v120 : v0;
    float v153 = as_type<float>(v145) + as_type<float>(v152);
    uint v154 = (v73 != 0u) ? as_type<uint>(v153) : v0;
    uint v155 = p1[y * m.stride1 + x];
    float v156 = as_type<float>(v154) + as_type<float>(v155);
    p1[y * m.stride1 + x] = as_type<uint>(v156);
}
