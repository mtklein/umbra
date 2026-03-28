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
    uint v11 = 1132396544u;
    uint v12 = x0 + pos.x;
    uint v13 = as_type<uint>((float)(int)v12);
    uint v14 = as_type<float>(v13) <  as_type<float>(v7) ? 0xffffffffu : 0u;
    uint v15 = as_type<float>(v5) <= as_type<float>(v13) ? 0xffffffffu : 0u;
    uint v16 = v15 & v14;
    uint v17 = y0 + pos.y;
    uint v18 = as_type<uint>((float)(int)v17);
    uint v19 = as_type<float>(v18) <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v20 = as_type<float>(v6) <= as_type<float>(v18) ? 0xffffffffu : 0u;
    uint v21 = v20 & v19;
    uint v22 = v16 & v21;
    uint v23 = (v22 & v9) | (~v22 & v0);
    uint v25 = (((device uint*)(p1 + y * buf_rbs[1]))[x] >> 24) & 0xFFu;
    uint v26 = as_type<uint>((float)(int)v25);
    uint v27 = as_type<uint>(as_type<float>(v26) * as_type<float>(998277249u));
    uint v28 = as_type<uint>(as_type<float>(v9) - as_type<float>(v27));
    uint v29 = as_type<uint>(fma(as_type<float>(v4), as_type<float>(v28), as_type<float>(v27)));
    uint v30 = as_type<uint>(as_type<float>(v29) - as_type<float>(v27));
    uint v31 = as_type<uint>(fma(as_type<float>(v23), as_type<float>(v30), as_type<float>(v27)));
    uint v32 = as_type<uint>(max(as_type<float>(v31), as_type<float>(0u)));
    uint v33 = as_type<uint>(min(as_type<float>(v32), as_type<float>(1065353216u)));
    uint v34 = as_type<uint>(as_type<float>(v33) * as_type<float>(1132396544u));
    uint v35 = as_type<uint>((int)rint(as_type<float>(v34)));
    uint v36 = (((device uint*)(p1 + y * buf_rbs[1]))[x] >> 0) & 0xFFu;
    uint v37 = as_type<uint>((float)(int)v36);
    uint v38 = as_type<uint>(as_type<float>(v37) * as_type<float>(998277249u));
    uint v39 = as_type<uint>(fma(as_type<float>(v1), as_type<float>(v28), as_type<float>(v38)));
    uint v40 = as_type<uint>(as_type<float>(v39) - as_type<float>(v38));
    uint v41 = as_type<uint>(fma(as_type<float>(v23), as_type<float>(v40), as_type<float>(v38)));
    uint v42 = as_type<uint>(max(as_type<float>(v41), as_type<float>(0u)));
    uint v43 = as_type<uint>(min(as_type<float>(v42), as_type<float>(1065353216u)));
    uint v44 = as_type<uint>(as_type<float>(v43) * as_type<float>(1132396544u));
    uint v45 = as_type<uint>((int)rint(as_type<float>(v44)));
    uint v46 = (((device uint*)(p1 + y * buf_rbs[1]))[x] >> 8) & 0xFFu;
    uint v47 = as_type<uint>((float)(int)v46);
    uint v48 = as_type<uint>(as_type<float>(v47) * as_type<float>(998277249u));
    uint v49 = as_type<uint>(fma(as_type<float>(v2), as_type<float>(v28), as_type<float>(v48)));
    uint v50 = as_type<uint>(as_type<float>(v49) - as_type<float>(v48));
    uint v51 = as_type<uint>(fma(as_type<float>(v23), as_type<float>(v50), as_type<float>(v48)));
    uint v52 = as_type<uint>(max(as_type<float>(v51), as_type<float>(0u)));
    uint v53 = as_type<uint>(min(as_type<float>(v52), as_type<float>(1065353216u)));
    uint v54 = as_type<uint>(as_type<float>(v53) * as_type<float>(1132396544u));
    uint v55 = as_type<uint>((int)rint(as_type<float>(v54)));
    uint v56 = (((device uint*)(p1 + y * buf_rbs[1]))[x] >> 16) & 0xFFu;
    uint v57 = as_type<uint>((float)(int)v56);
    uint v58 = as_type<uint>(as_type<float>(v57) * as_type<float>(998277249u));
    uint v59 = as_type<uint>(fma(as_type<float>(v3), as_type<float>(v28), as_type<float>(v58)));
    uint v60 = as_type<uint>(as_type<float>(v59) - as_type<float>(v58));
    uint v61 = as_type<uint>(fma(as_type<float>(v23), as_type<float>(v60), as_type<float>(v58)));
    uint v62 = as_type<uint>(max(as_type<float>(v61), as_type<float>(0u)));
    uint v63 = as_type<uint>(min(as_type<float>(v62), as_type<float>(1065353216u)));
    uint v64 = as_type<uint>(as_type<float>(v63) * as_type<float>(1132396544u));
    uint v65 = as_type<uint>((int)rint(as_type<float>(v64)));
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = (v45 & 0xFFu) | ((v55 & 0xFFu) << 8) | ((v65 & 0xFFu) << 16) | (v35 << 24);
}
