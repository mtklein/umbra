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
    uint v6 = 998277249u;
    uint v7 = 1054867456u;
    uint v8 = 1090519040u;
    uint v9 = 1065353216u;
    uint v10 = 255u;
    uint v11 = as_type<uint>(as_type<float>(v9) - as_type<float>(v4));
    uint v12 = 1132396544u;
    uint v13 = (uint)((device ushort*)(p2 + y * buf_rbs[2]))[x];
    uint v14 = (uint)(int)(short)(ushort)v13;
    uint v15 = as_type<uint>((float)(int)v14);
    uint v16 = as_type<uint>(as_type<float>(v15) * as_type<float>(998277249u));
    uint v17 = as_type<uint>(as_type<float>(v16) - as_type<float>(1054867456u));
    uint v18 = as_type<uint>(as_type<float>(v17) * as_type<float>(1090519040u));
    uint v19 = as_type<uint>(max(as_type<float>(v18), as_type<float>(0u)));
    uint v20 = as_type<uint>(min(as_type<float>(v19), as_type<float>(1065353216u)));
    uint v21 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v22 = v21 >> 24u;
    uint v23 = as_type<uint>((float)(int)v22);
    uint v24 = as_type<uint>(as_type<float>(v23) * as_type<float>(998277249u));
    uint v25 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v11), as_type<float>(v4)));
    uint v26 = as_type<uint>(as_type<float>(v25) - as_type<float>(v24));
    uint v27 = as_type<uint>(fma(as_type<float>(v20), as_type<float>(v26), as_type<float>(v24)));
    uint v28 = as_type<uint>(max(as_type<float>(v27), as_type<float>(0u)));
    uint v29 = as_type<uint>(min(as_type<float>(v28), as_type<float>(1065353216u)));
    uint v30 = as_type<uint>(as_type<float>(v29) * as_type<float>(1132396544u));
    uint v31 = as_type<uint>((int)rint(as_type<float>(v30)));
    uint v32 = v21 >> 8u;
    uint v33 = v32 & 255u;
    uint v34 = as_type<uint>((float)(int)v33);
    uint v35 = as_type<uint>(as_type<float>(v34) * as_type<float>(998277249u));
    uint v36 = as_type<uint>(fma(as_type<float>(v35), as_type<float>(v11), as_type<float>(v2)));
    uint v37 = as_type<uint>(as_type<float>(v36) - as_type<float>(v35));
    uint v38 = as_type<uint>(fma(as_type<float>(v20), as_type<float>(v37), as_type<float>(v35)));
    uint v39 = as_type<uint>(max(as_type<float>(v38), as_type<float>(0u)));
    uint v40 = as_type<uint>(min(as_type<float>(v39), as_type<float>(1065353216u)));
    uint v41 = as_type<uint>(as_type<float>(v40) * as_type<float>(1132396544u));
    uint v42 = as_type<uint>((int)rint(as_type<float>(v41)));
    uint v43 = v42 & 255u;
    uint v44 = v21 >> 16u;
    uint v45 = v44 & 255u;
    uint v46 = as_type<uint>((float)(int)v45);
    uint v47 = as_type<uint>(as_type<float>(v46) * as_type<float>(998277249u));
    uint v48 = as_type<uint>(fma(as_type<float>(v47), as_type<float>(v11), as_type<float>(v3)));
    uint v49 = as_type<uint>(as_type<float>(v48) - as_type<float>(v47));
    uint v50 = as_type<uint>(fma(as_type<float>(v20), as_type<float>(v49), as_type<float>(v47)));
    uint v51 = as_type<uint>(max(as_type<float>(v50), as_type<float>(0u)));
    uint v52 = as_type<uint>(min(as_type<float>(v51), as_type<float>(1065353216u)));
    uint v53 = as_type<uint>(as_type<float>(v52) * as_type<float>(1132396544u));
    uint v54 = as_type<uint>((int)rint(as_type<float>(v53)));
    uint v55 = v54 & 255u;
    uint v56 = v21 & 255u;
    uint v57 = as_type<uint>((float)(int)v56);
    uint v58 = as_type<uint>(as_type<float>(v57) * as_type<float>(998277249u));
    uint v59 = as_type<uint>(fma(as_type<float>(v58), as_type<float>(v11), as_type<float>(v1)));
    uint v60 = as_type<uint>(as_type<float>(v59) - as_type<float>(v58));
    uint v61 = as_type<uint>(fma(as_type<float>(v20), as_type<float>(v60), as_type<float>(v58)));
    uint v62 = as_type<uint>(max(as_type<float>(v61), as_type<float>(0u)));
    uint v63 = as_type<uint>(min(as_type<float>(v62), as_type<float>(1065353216u)));
    uint v64 = as_type<uint>(as_type<float>(v63) * as_type<float>(1132396544u));
    uint v65 = as_type<uint>((int)rint(as_type<float>(v64)));
    uint v66 = v65 & 255u;
    uint v67 = v66 | (v43 << 8u);
    uint v68 = v67 | (v55 << 16u);
    uint v69 = v68 | (v31 << 24u);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v69;
}
