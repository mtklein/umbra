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
    uint v107 = v104 <  as_type<float>(0u) ? 0xffffffffu : 0u;
    uint v108 = (v107 != 0u) ? v47 : v0;
    uint v109 = (v105 != 0u) ? v13 : v108;
    float v110 = fabs(v76);
    uint v111 = as_type<float>(v15) <  v110 ? 0xffffffffu : 0u;
    uint v112 = v101 <  as_type<float>(1065353216u) ? 0xffffffffu : 0u;
    uint v113 = v102 & v112;
    uint v114 = v106 & v113;
    float v115 = v67 * as_type<float>(1073741824u);
    #define v116 v115
    float v117 = v79 - v116;
    float v118 = v78 + v117;
    float v119 = fma(v101, v118, v83);
    float v120 = fma(v101, v119, v79);
    uint v121 = as_type<float>(v0) <  v120 ? 0xffffffffu : 0u;
    uint v122 = v114 & v121;
    uint v123 = (v122 != 0u) ? v109 : v0;
    float v124 = v76 * as_type<float>(1073741824u);
    #define v126 v124
    uint v127 = (v111 != 0u) ? as_type<uint>(v126) : v13;
    float v128 = v75 / as_type<float>(v127);
    float v129 = (v96 != 0u) ? v99 : v128;
    uint v130 = as_type<float>(v0) <= v129 ? 0xffffffffu : 0u;
    float v131 = v87 * v129;
    float v132 = v131 - v76;
    uint v133 = as_type<float>(v0) <  v132 ? 0xffffffffu : 0u;
    uint v134 = v132 <  as_type<float>(0u) ? 0xffffffffu : 0u;
    uint v135 = (v134 != 0u) ? v47 : v0;
    uint v136 = (v133 != 0u) ? v13 : v135;
    uint v137 = v129 <  as_type<float>(1065353216u) ? 0xffffffffu : 0u;
    uint v138 = v130 & v137;
    uint v139 = v89 & v138;
    float v140 = fma(v129, v118, v83);
    float v141 = fma(v129, v140, v79);
    uint v142 = as_type<float>(v0) <  v141 ? 0xffffffffu : 0u;
    uint v143 = v139 & v142;
    uint v144 = (v143 != 0u) ? v136 : v0;
    float v145 = as_type<float>(v144) + as_type<float>(v123);
    uint v146 = (v73 != 0u) ? as_type<uint>(v145) : v0;
    uint v147 = p1[y * m.stride1 + x];
    float v148 = as_type<float>(v146) + as_type<float>(v147);
    p1[y * m.stride1 + x] = as_type<uint>(v148);
}
