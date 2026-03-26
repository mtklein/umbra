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
    constant uint &n [[buffer(0)]],
    constant uint &w [[buffer(5)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    device uchar *p2 [[buffer(3)]],
    constant uint *buf_szs [[buffer(4)]],
    uint2 pos [[thread_position_in_grid]]
) {
    uint i = pos.y * w + pos.x;
    if (i >= n) return;
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
    uint v18 = as_type<uint>(as_type<float>(v15) - as_type<float>(1065353216u));
    uint v19 = as_type<uint>(as_type<float>(v16) - as_type<float>(1065353216u));
    uint v20 = as_type<uint>((int)floor(as_type<float>(v15)));
    uint v21 = as_type<uint>(as_type<float>(v17) - as_type<float>(v4));
    uint v22 = pos.x;
    uint v23 = as_type<uint>((float)(int)v22);
    uint v24 = pos.y;
    uint v25 = as_type<uint>((float)(int)v24);
    uint v26 = as_type<uint>(as_type<float>(v25) * as_type<float>(v10));
    uint v27 = as_type<uint>(fma(as_type<float>(v23), as_type<float>(v9), as_type<float>(v26)));
    uint v28 = as_type<uint>(as_type<float>(v11) + as_type<float>(v27));
    uint v29 = as_type<uint>(as_type<float>(v25) * as_type<float>(v13));
    uint v30 = as_type<uint>(fma(as_type<float>(v23), as_type<float>(v12), as_type<float>(v29)));
    uint v31 = as_type<uint>(as_type<float>(v14) + as_type<float>(v30));
    uint v32 = as_type<uint>(as_type<float>(v28) / as_type<float>(v31));
    uint v33 = as_type<uint>(as_type<float>(v25) * as_type<float>(v7));
    uint v34 = as_type<uint>(fma(as_type<float>(v23), as_type<float>(v6), as_type<float>(v33)));
    uint v35 = as_type<uint>(as_type<float>(v8) + as_type<float>(v34));
    uint v36 = as_type<float>(v32) <  as_type<float>(v16) ? 0xffffffffu : 0u;
    uint v37 = as_type<uint>(as_type<float>(v35) / as_type<float>(v31));
    uint v38 = as_type<float>(v37) <  as_type<float>(v15) ? 0xffffffffu : 0u;
    uint v39 = as_type<uint>(max(as_type<float>(v37), as_type<float>(0u)));
    uint v40 = as_type<uint>(min(as_type<float>(v39), as_type<float>(v18)));
    uint v41 = as_type<uint>(max(as_type<float>(v32), as_type<float>(0u)));
    uint v42 = as_type<uint>(min(as_type<float>(v41), as_type<float>(v19)));
    uint v43 = as_type<uint>((int)floor(as_type<float>(v42)));
    uint v44 = v43 * v20;
    uint v45 = as_type<uint>((int)floor(as_type<float>(v40)));
    uint v46 = v45 + v44;
    uint v47 = (uint)((device ushort*)p2)[safe_ix((int)v46,buf_szs[2],2)] & oob_mask((int)v46,buf_szs[2],2);
    uint v48 = (uint)(int)(short)(ushort)v47;
    uint v49 = as_type<uint>((float)(int)v48);
    uint v50 = as_type<uint>(as_type<float>(v49) * as_type<float>(998277249u));
    uint v51 = as_type<float>(v0) <= as_type<float>(v37) ? 0xffffffffu : 0u;
    uint v52 = v51 & v38;
    uint v53 = as_type<float>(v0) <= as_type<float>(v32) ? 0xffffffffu : 0u;
    uint v54 = v53 & v36;
    uint v55 = v52 & v54;
    uint v56 = (v55 & v50) | (~v55 & v0);
    uint v57 = ((device uint*)p1)[i];
    uint v58 = v57 >> 24u;
    uint v59 = as_type<uint>((float)(int)v58);
    uint v60 = as_type<uint>(as_type<float>(v59) * as_type<float>(998277249u));
    uint v61 = as_type<uint>(fma(as_type<float>(v60), as_type<float>(v21), as_type<float>(v4)));
    uint v62 = as_type<uint>(as_type<float>(v61) - as_type<float>(v60));
    uint v63 = as_type<uint>(fma(as_type<float>(v56), as_type<float>(v62), as_type<float>(v60)));
    uint v64 = as_type<uint>(as_type<float>(v63) * as_type<float>(1132396544u));
    uint v65 = as_type<uint>((int)rint(as_type<float>(v64)));
    uint v66 = v57 >> 8u;
    uint v67 = v66 & 255u;
    uint v68 = as_type<uint>((float)(int)v67);
    uint v69 = as_type<uint>(as_type<float>(v68) * as_type<float>(998277249u));
    uint v70 = as_type<uint>(fma(as_type<float>(v69), as_type<float>(v21), as_type<float>(v2)));
    uint v71 = as_type<uint>(as_type<float>(v70) - as_type<float>(v69));
    uint v72 = as_type<uint>(fma(as_type<float>(v56), as_type<float>(v71), as_type<float>(v69)));
    uint v73 = as_type<uint>(as_type<float>(v72) * as_type<float>(1132396544u));
    uint v74 = as_type<uint>((int)rint(as_type<float>(v73)));
    uint v75 = v74 & 255u;
    uint v76 = v57 >> 16u;
    uint v77 = v76 & 255u;
    uint v78 = as_type<uint>((float)(int)v77);
    uint v79 = as_type<uint>(as_type<float>(v78) * as_type<float>(998277249u));
    uint v80 = as_type<uint>(fma(as_type<float>(v79), as_type<float>(v21), as_type<float>(v3)));
    uint v81 = as_type<uint>(as_type<float>(v80) - as_type<float>(v79));
    uint v82 = as_type<uint>(fma(as_type<float>(v56), as_type<float>(v81), as_type<float>(v79)));
    uint v83 = as_type<uint>(as_type<float>(v82) * as_type<float>(1132396544u));
    uint v84 = as_type<uint>((int)rint(as_type<float>(v83)));
    uint v85 = v84 & 255u;
    uint v86 = v57 & 255u;
    uint v87 = as_type<uint>((float)(int)v86);
    uint v88 = as_type<uint>(as_type<float>(v87) * as_type<float>(998277249u));
    uint v89 = as_type<uint>(fma(as_type<float>(v88), as_type<float>(v21), as_type<float>(v1)));
    uint v90 = as_type<uint>(as_type<float>(v89) - as_type<float>(v88));
    uint v91 = as_type<uint>(fma(as_type<float>(v56), as_type<float>(v90), as_type<float>(v88)));
    uint v92 = as_type<uint>(as_type<float>(v91) * as_type<float>(1132396544u));
    uint v93 = as_type<uint>((int)rint(as_type<float>(v92)));
    uint v94 = v93 & 255u;
    uint v95 = v94 | (v75 << 8u);
    uint v96 = v95 | (v85 << 16u);
    uint v97 = v96 | (v65 << 24u);
    ((device uint*)p1)[i] = v97;
}
