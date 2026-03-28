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
    uint v18 = as_type<uint>(as_type<float>(v15) - as_type<float>(1065353216u));
    uint v19 = as_type<uint>(as_type<float>(v16) - as_type<float>(1065353216u));
    uint v20 = as_type<uint>((int)floor(as_type<float>(v15)));
    uint v21 = 998277249u;
    uint v22 = as_type<uint>(as_type<float>(v17) - as_type<float>(v4));
    uint v23 = 1132396544u;
    uint v24 = x0 + pos.x;
    uint v25 = as_type<uint>((float)(int)v24);
    uint v26 = y0 + pos.y;
    uint v27 = as_type<uint>((float)(int)v26);
    uint v28 = as_type<uint>(as_type<float>(v27) * as_type<float>(v10));
    uint v29 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v9), as_type<float>(v28)));
    uint v30 = as_type<uint>(as_type<float>(v11) + as_type<float>(v29));
    uint v31 = as_type<uint>(as_type<float>(v27) * as_type<float>(v13));
    uint v32 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v12), as_type<float>(v31)));
    uint v33 = as_type<uint>(as_type<float>(v14) + as_type<float>(v32));
    uint v34 = as_type<uint>(as_type<float>(v30) / as_type<float>(v33));
    uint v35 = as_type<float>(v34) <  as_type<float>(v16) ? 0xffffffffu : 0u;
    uint v36 = as_type<uint>(as_type<float>(v27) * as_type<float>(v7));
    uint v37 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v6), as_type<float>(v36)));
    uint v38 = as_type<uint>(as_type<float>(v8) + as_type<float>(v37));
    uint v39 = as_type<uint>(as_type<float>(v38) / as_type<float>(v33));
    uint v40 = as_type<float>(v39) <  as_type<float>(v15) ? 0xffffffffu : 0u;
    uint v41 = as_type<uint>(max(as_type<float>(v39), as_type<float>(0u)));
    uint v42 = as_type<uint>(min(as_type<float>(v41), as_type<float>(v18)));
    uint v43 = as_type<uint>((int)floor(as_type<float>(v42)));
    uint v44 = as_type<uint>(max(as_type<float>(v34), as_type<float>(0u)));
    uint v45 = as_type<uint>(min(as_type<float>(v44), as_type<float>(v19)));
    uint v46 = as_type<uint>((int)floor(as_type<float>(v45)));
    uint v47 = v46 * v20;
    uint v48 = v43 + v47;
    uint v49 = (uint)((device ushort*)p2)[safe_ix((int)v48,buf_szs[2],2)] & oob_mask((int)v48,buf_szs[2],2);
    uint v50 = (uint)(int)(short)(ushort)v49;
    uint v51 = as_type<uint>((float)(int)v50);
    uint v52 = as_type<uint>(as_type<float>(v51) * as_type<float>(998277249u));
    uint v53 = as_type<float>(v0) <= as_type<float>(v39) ? 0xffffffffu : 0u;
    uint v54 = v53 & v40;
    uint v55 = as_type<float>(v0) <= as_type<float>(v34) ? 0xffffffffu : 0u;
    uint v56 = v55 & v35;
    uint v57 = v54 & v56;
    uint v58 = (v57 & v52) | (~v57 & v0);
    uint v60 = (((device uint*)(p1 + y * buf_rbs[1]))[x] >> 24) & 0xFFu;
    uint v61 = as_type<uint>((float)(int)v60);
    uint v62 = as_type<uint>(as_type<float>(v61) * as_type<float>(998277249u));
    uint v63 = as_type<uint>(fma(as_type<float>(v62), as_type<float>(v22), as_type<float>(v4)));
    uint v64 = as_type<uint>(as_type<float>(v63) - as_type<float>(v62));
    uint v65 = as_type<uint>(fma(as_type<float>(v58), as_type<float>(v64), as_type<float>(v62)));
    uint v66 = as_type<uint>(max(as_type<float>(v65), as_type<float>(0u)));
    uint v67 = as_type<uint>(min(as_type<float>(v66), as_type<float>(1065353216u)));
    uint v68 = as_type<uint>(as_type<float>(v67) * as_type<float>(1132396544u));
    uint v69 = as_type<uint>((int)rint(as_type<float>(v68)));
    uint v70 = (((device uint*)(p1 + y * buf_rbs[1]))[x] >> 0) & 0xFFu;
    uint v71 = as_type<uint>((float)(int)v70);
    uint v72 = as_type<uint>(as_type<float>(v71) * as_type<float>(998277249u));
    uint v73 = as_type<uint>(fma(as_type<float>(v72), as_type<float>(v22), as_type<float>(v1)));
    uint v74 = as_type<uint>(as_type<float>(v73) - as_type<float>(v72));
    uint v75 = as_type<uint>(fma(as_type<float>(v58), as_type<float>(v74), as_type<float>(v72)));
    uint v76 = as_type<uint>(max(as_type<float>(v75), as_type<float>(0u)));
    uint v77 = as_type<uint>(min(as_type<float>(v76), as_type<float>(1065353216u)));
    uint v78 = as_type<uint>(as_type<float>(v77) * as_type<float>(1132396544u));
    uint v79 = as_type<uint>((int)rint(as_type<float>(v78)));
    uint v80 = (((device uint*)(p1 + y * buf_rbs[1]))[x] >> 8) & 0xFFu;
    uint v81 = as_type<uint>((float)(int)v80);
    uint v82 = as_type<uint>(as_type<float>(v81) * as_type<float>(998277249u));
    uint v83 = as_type<uint>(fma(as_type<float>(v82), as_type<float>(v22), as_type<float>(v2)));
    uint v84 = as_type<uint>(as_type<float>(v83) - as_type<float>(v82));
    uint v85 = as_type<uint>(fma(as_type<float>(v58), as_type<float>(v84), as_type<float>(v82)));
    uint v86 = as_type<uint>(max(as_type<float>(v85), as_type<float>(0u)));
    uint v87 = as_type<uint>(min(as_type<float>(v86), as_type<float>(1065353216u)));
    uint v88 = as_type<uint>(as_type<float>(v87) * as_type<float>(1132396544u));
    uint v89 = as_type<uint>((int)rint(as_type<float>(v88)));
    uint v90 = (((device uint*)(p1 + y * buf_rbs[1]))[x] >> 16) & 0xFFu;
    uint v91 = as_type<uint>((float)(int)v90);
    uint v92 = as_type<uint>(as_type<float>(v91) * as_type<float>(998277249u));
    uint v93 = as_type<uint>(fma(as_type<float>(v92), as_type<float>(v22), as_type<float>(v3)));
    uint v94 = as_type<uint>(as_type<float>(v93) - as_type<float>(v92));
    uint v95 = as_type<uint>(fma(as_type<float>(v58), as_type<float>(v94), as_type<float>(v92)));
    uint v96 = as_type<uint>(max(as_type<float>(v95), as_type<float>(0u)));
    uint v97 = as_type<uint>(min(as_type<float>(v96), as_type<float>(1065353216u)));
    uint v98 = as_type<uint>(as_type<float>(v97) * as_type<float>(1132396544u));
    uint v99 = as_type<uint>((int)rint(as_type<float>(v98)));
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = (v79 & 0xFFu) | ((v89 & 0xFFu) << 8) | ((v99 & 0xFFu) << 16) | (v69 << 24);
}
