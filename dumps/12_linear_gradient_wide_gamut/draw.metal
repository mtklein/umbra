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
    float v30 = as_type<float>(v6) + v23;
    float _si31 = floor(v30);
    float _fr31 = v30 - _si31;
    float _lo31 = as_type<float>(((device uint*)p2)[safe_ix((int)_si31,buf_szs[2],4)] & oob_mask((int)_si31,buf_szs[2],4));
    float _hi31 = as_type<float>(((device uint*)p2)[safe_ix((int)_si31+1,buf_szs[2],4)] & oob_mask((int)_si31+1,buf_szs[2],4));
    float v31 = _lo31 + (_hi31 - _lo31) * _fr31;
    float v32 = max(v31, as_type<float>(0u));
    float v33 = min(v32, as_type<float>(1065353216u));
    float v34 = v33 * as_type<float>(1132396544u);
    uint v35 = (uint)(int)rint(v34);
    float v36 = v23 + v10;
    float _si37 = floor(v36);
    float _fr37 = v36 - _si37;
    float _lo37 = as_type<float>(((device uint*)p2)[safe_ix((int)_si37,buf_szs[2],4)] & oob_mask((int)_si37,buf_szs[2],4));
    float _hi37 = as_type<float>(((device uint*)p2)[safe_ix((int)_si37+1,buf_szs[2],4)] & oob_mask((int)_si37+1,buf_szs[2],4));
    float v37 = _lo37 + (_hi37 - _lo37) * _fr37;
    float v38 = max(v37, as_type<float>(0u));
    float v39 = min(v38, as_type<float>(1065353216u));
    float v40 = v39 * as_type<float>(1132396544u);
    uint v41 = (uint)(int)rint(v40);
    float _si42 = floor(v23);
    float _fr42 = v23 - _si42;
    float _lo42 = as_type<float>(((device uint*)p2)[safe_ix((int)_si42,buf_szs[2],4)] & oob_mask((int)_si42,buf_szs[2],4));
    float _hi42 = as_type<float>(((device uint*)p2)[safe_ix((int)_si42+1,buf_szs[2],4)] & oob_mask((int)_si42+1,buf_szs[2],4));
    float v42 = _lo42 + (_hi42 - _lo42) * _fr42;
    float v43 = max(v42, as_type<float>(0u));
    float v44 = min(v43, as_type<float>(1065353216u));
    float v45 = v44 * as_type<float>(1132396544u);
    uint v46 = (uint)(int)rint(v45);
    uint v47 = v46 | (v35 << 8u);
    uint v48 = v47 | (v41 << 16u);
    uint v49 = v48 | (v29 << 24u);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v49;
}
