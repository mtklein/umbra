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
    constant uint &w [[buffer(2)]],
    constant uint *buf_szs [[buffer(3)]],
    constant uint *buf_rbs [[buffer(4)]],
    constant uint &x0 [[buffer(5)]],
    constant uint &y0 [[buffer(6)]],
    device uchar *p0 [[buffer(0)]],
    device uchar *p1 [[buffer(1)]],
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
    uint v5 = ((device const uint*)p0)[4];
    uint v6 = ((device const uint*)p0)[5];
    uint v7 = ((device const uint*)p0)[6];
    uint v8 = ((device const uint*)p0)[7];
    uint v9 = 1065353216u;
    uint v10 = 998277249u;
    uint v11 = as_type<uint>(as_type<float>(v9) - as_type<float>(v4));
    uint v12 = 1132396544u;
    uint v13 = x0 + pos.x;
    uint v14 = as_type<uint>((float)(int)v13);
    uint v15 = as_type<float>(v14) <  as_type<float>(v7) ? 0xffffffffu : 0u;
    uint v16 = as_type<float>(v5) <= as_type<float>(v14) ? 0xffffffffu : 0u;
    uint v17 = v16 & v15;
    uint v18 = y0 + pos.y;
    uint v19 = as_type<uint>((float)(int)v18);
    uint v20 = as_type<float>(v19) <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v21 = as_type<float>(v6) <= as_type<float>(v19) ? 0xffffffffu : 0u;
    uint v22 = v21 & v20;
    uint v23 = v17 & v22;
    uint v24 = (v23 & v9) | (~v23 & v0);
    uint v25_px = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v25 = v25_px & 0xFFu;
    uint v25_1 = (v25_px >> 8) & 0xFFu;
    uint v25_2 = (v25_px >> 16) & 0xFFu;
    uint v25_3 = v25_px >> 24;
    uint v26 = as_type<uint>((float)(int)v25_3);
    uint v27 = as_type<uint>(as_type<float>(v26) * as_type<float>(998277249u));
    uint v28 = as_type<uint>(fma(as_type<float>(v27), as_type<float>(v11), as_type<float>(v4)));
    uint v29 = as_type<uint>(as_type<float>(v28) - as_type<float>(v27));
    uint v30 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v29), as_type<float>(v27)));
    uint v31 = as_type<uint>(max(as_type<float>(v30), as_type<float>(0u)));
    uint v32 = as_type<uint>(min(as_type<float>(v31), as_type<float>(1065353216u)));
    uint v33 = as_type<uint>(as_type<float>(v32) * as_type<float>(1132396544u));
    uint v34 = as_type<uint>((int)rint(as_type<float>(v33)));
    uint v35 = as_type<uint>((float)(int)v25);
    uint v36 = as_type<uint>(as_type<float>(v35) * as_type<float>(998277249u));
    uint v37 = as_type<uint>(fma(as_type<float>(v36), as_type<float>(v11), as_type<float>(v1)));
    uint v38 = as_type<uint>(as_type<float>(v37) - as_type<float>(v36));
    uint v39 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v38), as_type<float>(v36)));
    uint v40 = as_type<uint>(max(as_type<float>(v39), as_type<float>(0u)));
    uint v41 = as_type<uint>(min(as_type<float>(v40), as_type<float>(1065353216u)));
    uint v42 = as_type<uint>(as_type<float>(v41) * as_type<float>(1132396544u));
    uint v43 = as_type<uint>((int)rint(as_type<float>(v42)));
    uint v44 = as_type<uint>((float)(int)v25_1);
    uint v45 = as_type<uint>(as_type<float>(v44) * as_type<float>(998277249u));
    uint v46 = as_type<uint>(fma(as_type<float>(v45), as_type<float>(v11), as_type<float>(v2)));
    uint v47 = as_type<uint>(as_type<float>(v46) - as_type<float>(v45));
    uint v48 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v47), as_type<float>(v45)));
    uint v49 = as_type<uint>(max(as_type<float>(v48), as_type<float>(0u)));
    uint v50 = as_type<uint>(min(as_type<float>(v49), as_type<float>(1065353216u)));
    uint v51 = as_type<uint>(as_type<float>(v50) * as_type<float>(1132396544u));
    uint v52 = as_type<uint>((int)rint(as_type<float>(v51)));
    uint v53 = as_type<uint>((float)(int)v25_2);
    uint v54 = as_type<uint>(as_type<float>(v53) * as_type<float>(998277249u));
    uint v55 = as_type<uint>(fma(as_type<float>(v54), as_type<float>(v11), as_type<float>(v3)));
    uint v56 = as_type<uint>(as_type<float>(v55) - as_type<float>(v54));
    uint v57 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v56), as_type<float>(v54)));
    uint v58 = as_type<uint>(max(as_type<float>(v57), as_type<float>(0u)));
    uint v59 = as_type<uint>(min(as_type<float>(v58), as_type<float>(1065353216u)));
    uint v60 = as_type<uint>(as_type<float>(v59) * as_type<float>(1132396544u));
    uint v61 = as_type<uint>((int)rint(as_type<float>(v60)));
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = (v43 & 0xFFu) | ((v52 & 0xFFu) << 8) | ((v61 & 0xFFu) << 16) | (v34 << 24);
}
