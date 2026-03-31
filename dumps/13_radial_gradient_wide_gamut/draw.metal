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
    uint v2 = ((device const uint*)p0)[0];
    uint v3 = ((device const uint*)p0)[1];
    uint v4 = ((device const uint*)p0)[2];
    uint v5 = 1065353216u;
    uint v6 = ((device const uint*)p0)[3];
    uint v7 = 1073741824u;
    float v8 = as_type<float>(v6) - as_type<float>(1065353216u);
    float v9 = as_type<float>(v6) - as_type<float>(1073741824u);
    uint v10 = 2u;
    uint v11 = 4u;
    uint v12 = 1u;
    uint v13 = 3u;
    uint v14 = x0 + pos.x;
    float v15 = (float)(int)v14;
    float v16 = v15 - as_type<float>(v2);
    uint v17 = y0 + pos.y;
    float v18 = (float)(int)v17;
    float v19 = v18 - as_type<float>(v3);
    float v20 = v19 * v19;
    float v21 = fma(v16, v16, v20);
    float v22 = sqrt(v21);
    float v23 = as_type<float>(v4) * v22;
    float v24 = max(v23, as_type<float>(0u));
    float v25 = min(v24, as_type<float>(1065353216u));
    float v26 = v25 * v8;
    float v27 = floor(v26);
    float v28 = min(v9, v27);
    uint v29 = (uint)(int)v28;
    uint v30 = v29 << 2u;
    uint v31 = v30 + 3u;
    uint v32 = ((device uint*)p2)[safe_ix((int)v31,buf_szs[2],4)] & oob_mask((int)v31,buf_szs[2],4);
    uint v33 = v30 + 4u;
    uint v34 = v33 + 3u;
    uint v35 = ((device uint*)p2)[safe_ix((int)v34,buf_szs[2],4)] & oob_mask((int)v34,buf_szs[2],4);
    float v36 = as_type<float>(v35) - as_type<float>(v32);
    uint v37 = v33 + 1u;
    uint v38 = ((device uint*)p2)[safe_ix((int)v37,buf_szs[2],4)] & oob_mask((int)v37,buf_szs[2],4);
    uint v39 = v33 + 2u;
    uint v40 = ((device uint*)p2)[safe_ix((int)v39,buf_szs[2],4)] & oob_mask((int)v39,buf_szs[2],4);
    float v41 = v26 - v28;
    float v42 = fma(v41, v36, as_type<float>(v32));
    uint v43 = v30 + 1u;
    uint v44 = ((device uint*)p2)[safe_ix((int)v43,buf_szs[2],4)] & oob_mask((int)v43,buf_szs[2],4);
    float v45 = as_type<float>(v38) - as_type<float>(v44);
    float v46 = fma(v41, v45, as_type<float>(v44));
    uint v47 = v30 + 2u;
    uint v48 = ((device uint*)p2)[safe_ix((int)v47,buf_szs[2],4)] & oob_mask((int)v47,buf_szs[2],4);
    float v49 = as_type<float>(v40) - as_type<float>(v48);
    float v50 = fma(v41, v49, as_type<float>(v48));
    uint v51 = ((device uint*)p2)[safe_ix((int)v33,buf_szs[2],4)] & oob_mask((int)v33,buf_szs[2],4);
    uint v52 = ((device uint*)p2)[safe_ix((int)v30,buf_szs[2],4)] & oob_mask((int)v30,buf_szs[2],4);
    float v53 = as_type<float>(v51) - as_type<float>(v52);
    float v54 = fma(v41, v53, as_type<float>(v52));
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = pack_float_to_unorm4x8(clamp(float4(v54,v46,v50,v42), 0.0, 1.0));
}
