#include <metal_stdlib>
using namespace metal;

int safe_ix(int ix, uint bytes, int elem) {
    int count = (int)(bytes / (uint)elem);
    return clamp(ix, 0, max(count-1, 0));
}
uint oob_mask(int ix, uint bytes, int elem) {
    int count = (int)(bytes / (uint)elem);
    return (ix >= 0 && ix < count) ? ~0u : 0u;
}

kernel void umbra_entry(
    constant uint &w [[buffer(3)]],
    constant uint *buf_szs [[buffer(4)]],
    constant uint *buf_rbs [[buffer(5)]],
    constant uint &x0 [[buffer(6)]],
    constant uint &y0 [[buffer(7)]],
    device uchar *p0 [[buffer(0)]],
    device uchar *p1 [[buffer(1)]],
    device uchar *p2 [[buffer(2)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= w) return;
    uint x = x0 + pos.x;
    uint y = y0 + pos.y;
    uint v0 = 0u;
    uint v2 = ((device const uint*)p0)[0];
    uint v3 = ((device const uint*)p0)[1];
    uint v4 = ((device const uint*)p0)[2];
    uint v5 = ((device const uint*)p0)[3];
    uint v6 = ((device const uint*)p0)[4];
    uint v7 = ((device const uint*)p0)[5];
    uint v8 = ((device const uint*)p0)[6];
    uint v9 = ((device const uint*)p0)[7];
    uint v10 = ((device const uint*)p0)[8];
    uint v11 = ((device const uint*)p0)[9];
    uint v12 = ((device const uint*)p0)[10];
    uint v13 = 1065353216u;
    uint v14 = 1073741824u;
    uint v15 = 931135488u;
    uint v16 = ((device const uint*)p0)[18];
    uint v17 = 6u;
    uint v18 = v16 * 6u;
    uint v19 = ((device uint*)p2)[safe_ix((int)v18,buf_szs[2],4)] & oob_mask((int)v18,buf_szs[2],4);
    uint v20 = 1u;
    uint v21 = v18 + 1u;
    uint v22 = ((device uint*)p2)[safe_ix((int)v21,buf_szs[2],4)] & oob_mask((int)v21,buf_szs[2],4);
    uint v23 = 2u;
    uint v24 = v18 + 2u;
    uint v25 = ((device uint*)p2)[safe_ix((int)v24,buf_szs[2],4)] & oob_mask((int)v24,buf_szs[2],4);
    uint v26 = 3u;
    uint v27 = v18 + 3u;
    uint v28 = ((device uint*)p2)[safe_ix((int)v27,buf_szs[2],4)] & oob_mask((int)v27,buf_szs[2],4);
    uint v29 = 4u;
    uint v30 = v18 + 4u;
    uint v31 = ((device uint*)p2)[safe_ix((int)v30,buf_szs[2],4)] & oob_mask((int)v30,buf_szs[2],4);
    uint v32 = 5u;
    uint v33 = v18 + 5u;
    uint v34 = ((device uint*)p2)[safe_ix((int)v33,buf_szs[2],4)] & oob_mask((int)v33,buf_szs[2],4);
    uint v35 = 3212836864u;
    uint v36 = x0 + pos.x;
    float v37 = (float)(int)v36;
    uint v38 = y0 + pos.y;
    float v39 = (float)(int)v38;
    float v40 = v39 * as_type<float>(v6);
    float v41 = fma(v37, as_type<float>(v5), v40);
    float v42 = as_type<float>(v7) + v41;
    float v43 = v39 * as_type<float>(v9);
    float v44 = fma(v37, as_type<float>(v8), v43);
    float v45 = as_type<float>(v10) + v44;
    float v46 = v42 / v45;
    float v47 = as_type<float>(v34) - v46;
    float v48 = v39 * as_type<float>(v3);
    float v49 = fma(v37, as_type<float>(v2), v48);
    float v50 = as_type<float>(v4) + v49;
    float v51 = v50 / v45;
    float v52 = as_type<float>(v31) - v51;
    uint v53 = v51 <  as_type<float>(v11) ? 0xffffffffu : 0u;
    uint v54 = v46 <  as_type<float>(v12) ? 0xffffffffu : 0u;
    float v55 = as_type<float>(v28) - v46;
    float v56 = v55 * as_type<float>(1073741824u);
    float v57 = as_type<float>(v22) - v46;
    float v58 = v57 - v56;
    float v59 = v47 + v58;
    float v60 = fabs(v59);
    uint v61 = as_type<float>(v15) <  v60 ? 0xffffffffu : 0u;
    uint v62 = select(v13, as_type<uint>(v59), v61 != 0u);
    float v63 = as_type<float>(v13) / as_type<float>(v62);
    float v64 = as_type<float>(v25) - v51;
    float v65 = v64 * as_type<float>(1073741824u);
    float v66 = as_type<float>(v19) - v51;
    float v67 = v66 - v65;
    float v68 = v52 + v67;
    float v69 = v64 - v66;
    float v70 = v69 * as_type<float>(1073741824u);
    float v71 = v57 - v55;
    float v72 = v71 * v71;
    float v73 = fma(-v57, v59, v72);
    float v74 = max(v73, as_type<float>(0u));
    float v75 = precise::sqrt(v74);
    float v76 = v71 + v75;
    float v77 = v63 * v76;
    float v78 = fma(v77, v68, v70);
    float v79 = fma(v77, v78, v66);
    uint v80 = as_type<float>(v0) <  v79 ? 0xffffffffu : 0u;
    float v81 = v59 * v77;
    float v82 = v81 - v71;
    uint v83 = v82 <  as_type<float>(0u) ? 0xffffffffu : 0u;
    uint v84 = select(v0, v35, v83 != 0u);
    uint v85 = as_type<float>(v0) <= v51 ? 0xffffffffu : 0u;
    uint v86 = v85 & v53;
    uint v87 = as_type<float>(v0) <= v46 ? 0xffffffffu : 0u;
    uint v88 = v87 & v54;
    uint v89 = v86 & v88;
    float v90 = v71 - v75;
    float v91 = v63 * v90;
    float v92 = fabs(v71);
    uint v93 = as_type<float>(v15) <  v92 ? 0xffffffffu : 0u;
    float v94 = v71 * as_type<float>(1073741824u);
    uint v95 = select(v13, as_type<uint>(v94), v93 != 0u);
    float v96 = v57 / as_type<float>(v95);
    float v97 = select(v96, v91, v61 != 0u);
    float v98 = v59 * v97;
    float v99 = v98 - v71;
    uint v100 = v99 <  as_type<float>(0u) ? 0xffffffffu : 0u;
    uint v101 = select(v0, v35, v100 != 0u);
    uint v102 = as_type<float>(v0) <= v97 ? 0xffffffffu : 0u;
    uint v103 = v97 <  as_type<float>(1065353216u) ? 0xffffffffu : 0u;
    uint v104 = v102 & v103;
    uint v105 = as_type<float>(v0) <= v73 ? 0xffffffffu : 0u;
    uint v106 = v105 & v61;
    uint v107 = v105 & v104;
    uint v108 = v77 <  as_type<float>(1065353216u) ? 0xffffffffu : 0u;
    uint v109 = as_type<float>(v0) <= v77 ? 0xffffffffu : 0u;
    uint v110 = v109 & v108;
    uint v111 = v106 & v110;
    uint v112 = v111 & v80;
    float v113 = fma(v97, v68, v70);
    float v114 = fma(v97, v113, v66);
    uint v115 = as_type<float>(v0) <  v114 ? 0xffffffffu : 0u;
    uint v116 = v107 & v115;
    uint v117 = as_type<float>(v0) <  v99 ? 0xffffffffu : 0u;
    uint v118 = select(v101, v13, v117 != 0u);
    uint v119 = select(v0, v118, v116 != 0u);
    uint v120 = as_type<float>(v0) <  v82 ? 0xffffffffu : 0u;
    uint v121 = select(v84, v13, v120 != 0u);
    uint v122 = select(v0, v121, v112 != 0u);
    float v123 = as_type<float>(v119) + as_type<float>(v122);
    uint v124 = select(v0, as_type<uint>(v123), v89 != 0u);
    uint v125 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    float v126 = as_type<float>(v124) + as_type<float>(v125);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = as_type<uint>(v126);
}
