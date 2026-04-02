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
    float v7 = as_type<float>(v6) - as_type<float>(1065353216u);
    uint v8 = 1073741824u;
    float v9 = as_type<float>(v6) - as_type<float>(1073741824u);
    float v10 = as_type<float>(v6) + as_type<float>(v6);
    float v11 = as_type<float>(v6) + v10;
    uint v12 = 1132396544u;
    uint v13 = x0 + pos.x;
    float v14 = (float)(int)v13;
    uint v15 = y0 + pos.y;
    float v16 = (float)(int)v15;
    float v17 = v16 * as_type<float>(v3);
    float v18 = fma(v14, as_type<float>(v2), v17);
    float v19 = as_type<float>(v4) + v18;
    float v20 = max(v19, as_type<float>(0u));
    float v21 = min(v20, as_type<float>(1065353216u));
    float v22 = v21 * v7;
    float v23 = min(v9, v22);
    float v24 = v23 + v11;
    float _si25 = floor(v24);
    float _fr25 = v24 - _si25;
    float _lo25 = as_type<float>(((device uint*)p2)[safe_ix((int)_si25,buf_szs[2],4)] & oob_mask((int)_si25,buf_szs[2],4));
    float _hi25 = as_type<float>(((device uint*)p2)[safe_ix((int)_si25+1,buf_szs[2],4)] & oob_mask((int)_si25+1,buf_szs[2],4));
    float v25 = _lo25 + (_hi25 - _lo25) * _fr25;
    float v26 = max(v25, as_type<float>(0u));
    float v27 = min(v26, as_type<float>(1065353216u));
    float v28 = v27 * as_type<float>(1132396544u);
    uint v29 = (uint)(int)rint(v28);
    uint v30 = v29 << 24u;
    float v31 = as_type<float>(v6) + v23;
    float _si32 = floor(v31);
    float _fr32 = v31 - _si32;
    float _lo32 = as_type<float>(((device uint*)p2)[safe_ix((int)_si32,buf_szs[2],4)] & oob_mask((int)_si32,buf_szs[2],4));
    float _hi32 = as_type<float>(((device uint*)p2)[safe_ix((int)_si32+1,buf_szs[2],4)] & oob_mask((int)_si32+1,buf_szs[2],4));
    float v32 = _lo32 + (_hi32 - _lo32) * _fr32;
    float v33 = max(v32, as_type<float>(0u));
    float v34 = min(v33, as_type<float>(1065353216u));
    float v35 = v34 * as_type<float>(1132396544u);
    uint v36 = (uint)(int)rint(v35);
    uint v37 = v36 << 8u;
    float v38 = v23 + v10;
    float _si39 = floor(v38);
    float _fr39 = v38 - _si39;
    float _lo39 = as_type<float>(((device uint*)p2)[safe_ix((int)_si39,buf_szs[2],4)] & oob_mask((int)_si39,buf_szs[2],4));
    float _hi39 = as_type<float>(((device uint*)p2)[safe_ix((int)_si39+1,buf_szs[2],4)] & oob_mask((int)_si39+1,buf_szs[2],4));
    float v39 = _lo39 + (_hi39 - _lo39) * _fr39;
    float v40 = max(v39, as_type<float>(0u));
    float v41 = min(v40, as_type<float>(1065353216u));
    float v42 = v41 * as_type<float>(1132396544u);
    uint v43 = (uint)(int)rint(v42);
    uint v44 = v43 << 16u;
    float _si45 = floor(v23);
    float _fr45 = v23 - _si45;
    float _lo45 = as_type<float>(((device uint*)p2)[safe_ix((int)_si45,buf_szs[2],4)] & oob_mask((int)_si45,buf_szs[2],4));
    float _hi45 = as_type<float>(((device uint*)p2)[safe_ix((int)_si45+1,buf_szs[2],4)] & oob_mask((int)_si45+1,buf_szs[2],4));
    float v45 = _lo45 + (_hi45 - _lo45) * _fr45;
    float v46 = max(v45, as_type<float>(0u));
    float v47 = min(v46, as_type<float>(1065353216u));
    float v48 = v47 * as_type<float>(1132396544u);
    uint v49 = (uint)(int)rint(v48);
    uint v50 = v49 | v37;
    uint v51 = v50 | v44;
    uint v52 = v51 | v30;
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v52;
}
