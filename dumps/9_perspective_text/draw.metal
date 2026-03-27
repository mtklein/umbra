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
    constant uint &stride [[buffer(6)]],
    constant uint &x0 [[buffer(7)]],
    constant uint &y0 [[buffer(8)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    device uchar *p2 [[buffer(3)]],
    constant uint *buf_szs [[buffer(4)]],
    uint2 pos [[thread_position_in_grid]]
) {
    uint i = (y0 + pos.y) * stride + x0 + pos.x;
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
    uint v21 = 998277249u;
    uint v22 = 255u;
    uint v23 = as_type<uint>(as_type<float>(v17) - as_type<float>(v4));
    uint v24 = 1132396544u;
    uint v25 = x0 + pos.x;
    uint v26 = as_type<uint>((float)(int)v25);
    uint v27 = y0 + pos.y;
    uint v28 = as_type<uint>((float)(int)v27);
    uint v29 = as_type<uint>(as_type<float>(v28) * as_type<float>(v10));
    uint v30 = as_type<uint>(fma(as_type<float>(v26), as_type<float>(v9), as_type<float>(v29)));
    uint v31 = as_type<uint>(as_type<float>(v11) + as_type<float>(v30));
    uint v32 = as_type<uint>(as_type<float>(v28) * as_type<float>(v13));
    uint v33 = as_type<uint>(fma(as_type<float>(v26), as_type<float>(v12), as_type<float>(v32)));
    uint v34 = as_type<uint>(as_type<float>(v14) + as_type<float>(v33));
    uint v35 = as_type<uint>(as_type<float>(v31) / as_type<float>(v34));
    uint v36 = as_type<uint>(as_type<float>(v28) * as_type<float>(v7));
    uint v37 = as_type<uint>(fma(as_type<float>(v26), as_type<float>(v6), as_type<float>(v36)));
    uint v38 = as_type<uint>(as_type<float>(v8) + as_type<float>(v37));
    uint v39 = as_type<float>(v35) <  as_type<float>(v16) ? 0xffffffffu : 0u;
    uint v40 = as_type<uint>(as_type<float>(v38) / as_type<float>(v34));
    uint v41 = as_type<float>(v40) <  as_type<float>(v15) ? 0xffffffffu : 0u;
    uint v42 = as_type<uint>(max(as_type<float>(v40), as_type<float>(0u)));
    uint v43 = as_type<uint>(min(as_type<float>(v42), as_type<float>(v18)));
    uint v44 = as_type<uint>(max(as_type<float>(v35), as_type<float>(0u)));
    uint v45 = as_type<uint>(min(as_type<float>(v44), as_type<float>(v19)));
    uint v46 = as_type<uint>((int)floor(as_type<float>(v45)));
    uint v47 = v46 * v20;
    uint v48 = as_type<uint>((int)floor(as_type<float>(v43)));
    uint v49 = v48 + v47;
    uint v50 = (uint)((device ushort*)p2)[safe_ix((int)v49,buf_szs[2],2)] & oob_mask((int)v49,buf_szs[2],2);
    uint v51 = (uint)(int)(short)(ushort)v50;
    uint v52 = as_type<uint>((float)(int)v51);
    uint v53 = as_type<uint>(as_type<float>(v52) * as_type<float>(998277249u));
    uint v54 = as_type<float>(v0) <= as_type<float>(v40) ? 0xffffffffu : 0u;
    uint v55 = v54 & v41;
    uint v56 = as_type<float>(v0) <= as_type<float>(v35) ? 0xffffffffu : 0u;
    uint v57 = v56 & v39;
    uint v58 = v55 & v57;
    uint v59 = (v58 & v53) | (~v58 & v0);
    uint v60 = ((device uint*)p1)[i];
    uint v61 = v60 >> 24u;
    uint v62 = as_type<uint>((float)(int)v61);
    uint v63 = as_type<uint>(as_type<float>(v62) * as_type<float>(998277249u));
    uint v64 = as_type<uint>(fma(as_type<float>(v63), as_type<float>(v23), as_type<float>(v4)));
    uint v65 = as_type<uint>(as_type<float>(v64) - as_type<float>(v63));
    uint v66 = as_type<uint>(fma(as_type<float>(v59), as_type<float>(v65), as_type<float>(v63)));
    uint v67 = as_type<uint>(max(as_type<float>(v66), as_type<float>(0u)));
    uint v68 = as_type<uint>(min(as_type<float>(v67), as_type<float>(1065353216u)));
    uint v69 = as_type<uint>(as_type<float>(v68) * as_type<float>(1132396544u));
    uint v70 = as_type<uint>((int)rint(as_type<float>(v69)));
    uint v71 = v60 >> 8u;
    uint v72 = v71 & 255u;
    uint v73 = as_type<uint>((float)(int)v72);
    uint v74 = as_type<uint>(as_type<float>(v73) * as_type<float>(998277249u));
    uint v75 = as_type<uint>(fma(as_type<float>(v74), as_type<float>(v23), as_type<float>(v2)));
    uint v76 = as_type<uint>(as_type<float>(v75) - as_type<float>(v74));
    uint v77 = as_type<uint>(fma(as_type<float>(v59), as_type<float>(v76), as_type<float>(v74)));
    uint v78 = as_type<uint>(max(as_type<float>(v77), as_type<float>(0u)));
    uint v79 = as_type<uint>(min(as_type<float>(v78), as_type<float>(1065353216u)));
    uint v80 = as_type<uint>(as_type<float>(v79) * as_type<float>(1132396544u));
    uint v81 = as_type<uint>((int)rint(as_type<float>(v80)));
    uint v82 = v81 & 255u;
    uint v83 = v60 >> 16u;
    uint v84 = v83 & 255u;
    uint v85 = as_type<uint>((float)(int)v84);
    uint v86 = as_type<uint>(as_type<float>(v85) * as_type<float>(998277249u));
    uint v87 = as_type<uint>(fma(as_type<float>(v86), as_type<float>(v23), as_type<float>(v3)));
    uint v88 = as_type<uint>(as_type<float>(v87) - as_type<float>(v86));
    uint v89 = as_type<uint>(fma(as_type<float>(v59), as_type<float>(v88), as_type<float>(v86)));
    uint v90 = as_type<uint>(max(as_type<float>(v89), as_type<float>(0u)));
    uint v91 = as_type<uint>(min(as_type<float>(v90), as_type<float>(1065353216u)));
    uint v92 = as_type<uint>(as_type<float>(v91) * as_type<float>(1132396544u));
    uint v93 = as_type<uint>((int)rint(as_type<float>(v92)));
    uint v94 = v93 & 255u;
    uint v95 = v60 & 255u;
    uint v96 = as_type<uint>((float)(int)v95);
    uint v97 = as_type<uint>(as_type<float>(v96) * as_type<float>(998277249u));
    uint v98 = as_type<uint>(fma(as_type<float>(v97), as_type<float>(v23), as_type<float>(v1)));
    uint v99 = as_type<uint>(as_type<float>(v98) - as_type<float>(v97));
    uint v100 = as_type<uint>(fma(as_type<float>(v59), as_type<float>(v99), as_type<float>(v97)));
    uint v101 = as_type<uint>(max(as_type<float>(v100), as_type<float>(0u)));
    uint v102 = as_type<uint>(min(as_type<float>(v101), as_type<float>(1065353216u)));
    uint v103 = as_type<uint>(as_type<float>(v102) * as_type<float>(1132396544u));
    uint v104 = as_type<uint>((int)rint(as_type<float>(v103)));
    uint v105 = v104 & 255u;
    uint v106 = v105 | (v82 << 8u);
    uint v107 = v106 | (v94 << 16u);
    uint v108 = v107 | (v70 << 24u);
    ((device uint*)p1)[i] = v108;
}
