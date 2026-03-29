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
    constant uint *buf_fmts [[buffer(8)]],
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
    uint v10 = as_type<uint>(as_type<float>(v9) - as_type<float>(v4));
    uint v11 = 1132396544u;
    uint v12 = (uint)((device ushort*)(p2 + y * buf_rbs[2]))[x];
    uint v13 = (uint)(int)(short)(ushort)v12;
    uint v14 = as_type<uint>((float)(int)v13);
    uint v15 = as_type<uint>(as_type<float>(v14) * as_type<float>(998277249u));
    uint v16 = as_type<uint>(as_type<float>(v15) - as_type<float>(1054867456u));
    uint v17 = as_type<uint>(as_type<float>(v16) * as_type<float>(1090519040u));
    uint v18 = as_type<uint>(max(as_type<float>(v17), as_type<float>(0u)));
    uint v19 = as_type<uint>(min(as_type<float>(v18), as_type<float>(1065353216u)));
    uint v20_px = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v20 = v20_px & 0xFFu;
    uint v20_1 = (v20_px >> 8) & 0xFFu;
    uint v20_2 = (v20_px >> 16) & 0xFFu;
    uint v20_3 = v20_px >> 24;
    uint v21 = as_type<uint>((float)(int)v20_3);
    uint v22 = as_type<uint>(as_type<float>(v21) * as_type<float>(998277249u));
    uint v23 = as_type<uint>(fma(as_type<float>(v22), as_type<float>(v10), as_type<float>(v4)));
    uint v24 = as_type<uint>(as_type<float>(v23) - as_type<float>(v22));
    uint v25 = as_type<uint>(fma(as_type<float>(v19), as_type<float>(v24), as_type<float>(v22)));
    uint v26 = as_type<uint>(max(as_type<float>(v25), as_type<float>(0u)));
    uint v27 = as_type<uint>(min(as_type<float>(v26), as_type<float>(1065353216u)));
    uint v28 = as_type<uint>(as_type<float>(v27) * as_type<float>(1132396544u));
    uint v29 = as_type<uint>((int)rint(as_type<float>(v28)));
    uint v30 = as_type<uint>((float)(int)v20);
    uint v31 = as_type<uint>(as_type<float>(v30) * as_type<float>(998277249u));
    uint v32 = as_type<uint>(fma(as_type<float>(v31), as_type<float>(v10), as_type<float>(v1)));
    uint v33 = as_type<uint>(as_type<float>(v32) - as_type<float>(v31));
    uint v34 = as_type<uint>(fma(as_type<float>(v19), as_type<float>(v33), as_type<float>(v31)));
    uint v35 = as_type<uint>(max(as_type<float>(v34), as_type<float>(0u)));
    uint v36 = as_type<uint>(min(as_type<float>(v35), as_type<float>(1065353216u)));
    uint v37 = as_type<uint>(as_type<float>(v36) * as_type<float>(1132396544u));
    uint v38 = as_type<uint>((int)rint(as_type<float>(v37)));
    uint v39 = as_type<uint>((float)(int)v20_1);
    uint v40 = as_type<uint>(as_type<float>(v39) * as_type<float>(998277249u));
    uint v41 = as_type<uint>(fma(as_type<float>(v40), as_type<float>(v10), as_type<float>(v2)));
    uint v42 = as_type<uint>(as_type<float>(v41) - as_type<float>(v40));
    uint v43 = as_type<uint>(fma(as_type<float>(v19), as_type<float>(v42), as_type<float>(v40)));
    uint v44 = as_type<uint>(max(as_type<float>(v43), as_type<float>(0u)));
    uint v45 = as_type<uint>(min(as_type<float>(v44), as_type<float>(1065353216u)));
    uint v46 = as_type<uint>(as_type<float>(v45) * as_type<float>(1132396544u));
    uint v47 = as_type<uint>((int)rint(as_type<float>(v46)));
    uint v48 = as_type<uint>((float)(int)v20_2);
    uint v49 = as_type<uint>(as_type<float>(v48) * as_type<float>(998277249u));
    uint v50 = as_type<uint>(fma(as_type<float>(v49), as_type<float>(v10), as_type<float>(v3)));
    uint v51 = as_type<uint>(as_type<float>(v50) - as_type<float>(v49));
    uint v52 = as_type<uint>(fma(as_type<float>(v19), as_type<float>(v51), as_type<float>(v49)));
    uint v53 = as_type<uint>(max(as_type<float>(v52), as_type<float>(0u)));
    uint v54 = as_type<uint>(min(as_type<float>(v53), as_type<float>(1065353216u)));
    uint v55 = as_type<uint>(as_type<float>(v54) * as_type<float>(1132396544u));
    uint v56 = as_type<uint>((int)rint(as_type<float>(v55)));
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = (v38 & 0xFFu) | ((v47 & 0xFFu) << 8) | ((v56 & 0xFFu) << 16) | (v29 << 24);
}
