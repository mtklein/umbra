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
    float v22 = as_type<float>(v17) - as_type<float>(v4);
    uint v23 = 1132396544u;
    uint v24 = x0 + pos.x;
    float v25 = (float)(int)v24;
    uint v26 = y0 + pos.y;
    float v27 = (float)(int)v26;
    float v28 = v27 * as_type<float>(v10);
    float v29 = fma(v25, as_type<float>(v9), v28);
    float v30 = as_type<float>(v11) + v29;
    float v31 = v27 * as_type<float>(v13);
    float v32 = fma(v25, as_type<float>(v12), v31);
    float v33 = as_type<float>(v14) + v32;
    float v34 = v30 / v33;
    uint v35 = v34 <  as_type<float>(v16) ? 0xffffffffu : 0u;
    float v36 = v27 * as_type<float>(v7);
    float v37 = fma(v25, as_type<float>(v6), v36);
    float v38 = as_type<float>(v8) + v37;
    float v39 = v38 / v33;
    uint v40 = v39 <  as_type<float>(v15) ? 0xffffffffu : 0u;
    float v41 = max(v39, as_type<float>(0u));
    float v42 = min(v41, v18);
    uint v43 = (uint)(int)floor(v42);
    float v44 = max(v34, as_type<float>(0u));
    float v45 = min(v44, v19);
    uint v46 = (uint)(int)floor(v45);
    uint v47 = v46 * v20;
    uint v48 = v43 + v47;
    uint v49 = (uint)((device ushort*)p2)[safe_ix((int)v48,buf_szs[2],2)] & oob_mask((int)v48,buf_szs[2],2);
    uint v50 = (uint)(int)(short)(ushort)v49;
    float v51 = (float)(int)v50;
    float v52 = v51 * as_type<float>(998277249u);
    uint v53 = as_type<float>(v0) <= v39 ? 0xffffffffu : 0u;
    uint v54 = v53 & v40;
    uint v55 = as_type<float>(v0) <= v34 ? 0xffffffffu : 0u;
    uint v56 = v55 & v35;
    uint v57 = v54 & v56;
    uint v58 = select(v0, as_type<uint>(v52), v57 != 0u);
    uint px59 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v59 = px59 & 0xFFu;
    uint v59_1 = (px59 >> 8u) & 0xFFu;
    uint v59_2 = (px59 >> 16u) & 0xFFu;
    uint v59_3 = px59 >> 24u;
    float v60 = (float)(int)v59_3;
    float v61 = v60 * as_type<float>(998277249u);
    float v62 = fma(v61, v22, as_type<float>(v4));
    float v63 = v62 - v61;
    float v64 = fma(as_type<float>(v58), v63, v61);
    float v65 = max(v64, as_type<float>(0u));
    float v66 = min(v65, as_type<float>(1065353216u));
    float v67 = v66 * as_type<float>(1132396544u);
    uint v68 = (uint)(int)rint(v67);
    float v69 = (float)(int)v59;
    float v70 = v69 * as_type<float>(998277249u);
    float v71 = fma(v70, v22, as_type<float>(v1));
    float v72 = v71 - v70;
    float v73 = fma(as_type<float>(v58), v72, v70);
    float v74 = max(v73, as_type<float>(0u));
    float v75 = min(v74, as_type<float>(1065353216u));
    float v76 = v75 * as_type<float>(1132396544u);
    uint v77 = (uint)(int)rint(v76);
    float v78 = (float)(int)v59_1;
    float v79 = v78 * as_type<float>(998277249u);
    float v80 = fma(v79, v22, as_type<float>(v2));
    float v81 = v80 - v79;
    float v82 = fma(as_type<float>(v58), v81, v79);
    float v83 = max(v82, as_type<float>(0u));
    float v84 = min(v83, as_type<float>(1065353216u));
    float v85 = v84 * as_type<float>(1132396544u);
    uint v86 = (uint)(int)rint(v85);
    float v87 = (float)(int)v59_2;
    float v88 = v87 * as_type<float>(998277249u);
    float v89 = fma(v88, v22, as_type<float>(v3));
    float v90 = v89 - v88;
    float v91 = fma(as_type<float>(v58), v90, v88);
    float v92 = max(v91, as_type<float>(0u));
    float v93 = min(v92, as_type<float>(1065353216u));
    float v94 = v93 * as_type<float>(1132396544u);
    uint v95 = (uint)(int)rint(v94);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = (v77 & 0xFFu) | ((v86 & 0xFFu) << 8u) | ((v95 & 0xFFu) << 16u) | (v68 << 24u);
}
