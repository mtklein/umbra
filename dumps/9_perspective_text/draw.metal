#include <metal_stdlib>
using namespace metal;

static inline int safe_ix(int ix, uint bytes, int elem) {
    int count = (int)(bytes / (uint)elem);
    return clamp(ix, 0, max(count-1, 0));
}
static inline uint oob_mask(int ix, uint bytes, int elem) {
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
    uint v1 = ((device const uint*)p0)[0];
    uint v2 = ((device const uint*)p0)[1];
    uint v3 = ((device const uint*)p0)[2];
    uint v4 = ((device const uint*)p0)[3];
    uint v6 = ((device const uint*)p0)[4];
    uint v7 = ((device const uint*)p0)[5];
    uint v8 = ((device const uint*)p0)[6];
    uint v9 = ((device const uint*)p0)[7];
    uint v10 = ((device const uint*)p0)[8];
    uint v11 = ((device const uint*)p0)[9];
    uint v12 = ((device const uint*)p0)[10];
    uint v13 = ((device const uint*)p0)[11];
    uint v14 = ((device const uint*)p0)[12];
    uint v15 = ((device const uint*)p0)[13];
    uint v16 = ((device const uint*)p0)[14];
    uint v17 = 1065353216u;
    float v18 = as_type<float>(v15) - as_type<float>(1065353216u);
    float v19 = as_type<float>(v16) - as_type<float>(1065353216u);
    uint v20 = (uint)(int)floor(as_type<float>(v15));
    uint v21 = 998277249u;
    uint v22 = 255u;
    float v23 = as_type<float>(v17) - as_type<float>(v4);
    uint v24 = 1132396544u;
    uint v25 = x0 + pos.x;
    float v26 = (float)(int)v25;
    uint v27 = y0 + pos.y;
    float v28 = (float)(int)v27;
    float v29 = v28 * as_type<float>(v10);
    float v30 = fma(v26, as_type<float>(v9), v29);
    float v31 = as_type<float>(v11) + v30;
    float v32 = v28 * as_type<float>(v13);
    float v33 = fma(v26, as_type<float>(v12), v32);
    float v34 = as_type<float>(v14) + v33;
    float v35 = v31 / v34;
    uint v36 = v35 <  as_type<float>(v16) ? 0xffffffffu : 0u;
    float v37 = v28 * as_type<float>(v7);
    float v38 = fma(v26, as_type<float>(v6), v37);
    float v39 = as_type<float>(v8) + v38;
    float v40 = v39 / v34;
    uint v41 = v40 <  as_type<float>(v15) ? 0xffffffffu : 0u;
    float v42 = max(v40, as_type<float>(0u));
    float v43 = min(v42, v18);
    uint v44 = (uint)(int)floor(v43);
    float v45 = max(v35, as_type<float>(0u));
    float v46 = min(v45, v19);
    uint v47 = (uint)(int)floor(v46);
    uint v48 = v47 * v20;
    uint v49 = v44 + v48;
    uint v50 = (uint)((device ushort*)p2)[safe_ix((int)v49,buf_szs[2],2)] & oob_mask((int)v49,buf_szs[2],2);
    uint v51 = (uint)(int)(short)(ushort)v50;
    float v52 = (float)(int)v51;
    float v53 = v52 * as_type<float>(998277249u);
    uint v54 = as_type<float>(v0) <= v40 ? 0xffffffffu : 0u;
    uint v55 = v54 & v41;
    uint v56 = as_type<float>(v0) <= v35 ? 0xffffffffu : 0u;
    uint v57 = v56 & v36;
    uint v58 = v55 & v57;
    uint v59 = select(v0, as_type<uint>(v53), v58 != 0u);
    uint v60 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v61 = v60 >> 24u;
    float v62 = (float)(int)v61;
    float v63 = v62 * as_type<float>(998277249u);
    float v64 = fma(v63, v23, as_type<float>(v4));
    float v65 = v64 - v63;
    float v66 = fma(as_type<float>(v59), v65, v63);
    float v67 = max(v66, as_type<float>(0u));
    float v68 = min(v67, as_type<float>(1065353216u));
    float v69 = v68 * as_type<float>(1132396544u);
    uint v70 = (uint)(int)rint(v69);
    uint v71 = v60 >> 8u;
    uint v72 = v71 & 255u;
    float v73 = (float)(int)v72;
    float v74 = v73 * as_type<float>(998277249u);
    float v75 = fma(v74, v23, as_type<float>(v2));
    float v76 = v75 - v74;
    float v77 = fma(as_type<float>(v59), v76, v74);
    float v78 = max(v77, as_type<float>(0u));
    float v79 = min(v78, as_type<float>(1065353216u));
    float v80 = v79 * as_type<float>(1132396544u);
    uint v81 = (uint)(int)rint(v80);
    uint v82 = v60 >> 16u;
    uint v83 = v82 & 255u;
    float v84 = (float)(int)v83;
    float v85 = v84 * as_type<float>(998277249u);
    float v86 = fma(v85, v23, as_type<float>(v3));
    float v87 = v86 - v85;
    float v88 = fma(as_type<float>(v59), v87, v85);
    float v89 = max(v88, as_type<float>(0u));
    float v90 = min(v89, as_type<float>(1065353216u));
    float v91 = v90 * as_type<float>(1132396544u);
    uint v92 = (uint)(int)rint(v91);
    uint v93 = v60 & 255u;
    float v94 = (float)(int)v93;
    float v95 = v94 * as_type<float>(998277249u);
    float v96 = fma(v95, v23, as_type<float>(v1));
    float v97 = v96 - v95;
    float v98 = fma(as_type<float>(v59), v97, v95);
    float v99 = max(v98, as_type<float>(0u));
    float v100 = min(v99, as_type<float>(1065353216u));
    float v101 = v100 * as_type<float>(1132396544u);
    uint v102 = (uint)(int)rint(v101);
    uint v103 = v102 | (v81 << 8u);
    uint v104 = v103 | (v92 << 16u);
    uint v105 = v104 | (v70 << 24u);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v105;
}
