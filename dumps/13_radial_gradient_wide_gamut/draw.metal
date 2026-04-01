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
    float v15 = v14 - as_type<float>(v2);
    uint v16 = y0 + pos.y;
    float v17 = (float)(int)v16;
    float v18 = v17 - as_type<float>(v3);
    float v19 = v18 * v18;
    float v20 = fma(v15, v15, v19);
    float v21 = precise::sqrt(v20);
    float v22 = as_type<float>(v4) * v21;
    float v23 = max(v22, as_type<float>(0u));
    float v24 = min(v23, as_type<float>(1065353216u));
    float v25 = v24 * v7;
    float v26 = min(v9, v25);
    float v27 = v26 + v11;
    float _si28 = floor(v27);
    float _fr28 = v27 - _si28;
    float _lo28 = as_type<float>(((device uint*)p2)[safe_ix((int)_si28,buf_szs[2],4)] & oob_mask((int)_si28,buf_szs[2],4));
    float _hi28 = as_type<float>(((device uint*)p2)[safe_ix((int)_si28+1,buf_szs[2],4)] & oob_mask((int)_si28+1,buf_szs[2],4));
    float v28 = _lo28 + (_hi28 - _lo28) * _fr28;
    float v29 = max(v28, as_type<float>(0u));
    float v30 = min(v29, as_type<float>(1065353216u));
    float v31 = v30 * as_type<float>(1132396544u);
    uint v32 = (uint)(int)rint(v31);
    float v33 = as_type<float>(v6) + v26;
    float _si34 = floor(v33);
    float _fr34 = v33 - _si34;
    float _lo34 = as_type<float>(((device uint*)p2)[safe_ix((int)_si34,buf_szs[2],4)] & oob_mask((int)_si34,buf_szs[2],4));
    float _hi34 = as_type<float>(((device uint*)p2)[safe_ix((int)_si34+1,buf_szs[2],4)] & oob_mask((int)_si34+1,buf_szs[2],4));
    float v34 = _lo34 + (_hi34 - _lo34) * _fr34;
    float v35 = max(v34, as_type<float>(0u));
    float v36 = min(v35, as_type<float>(1065353216u));
    float v37 = v36 * as_type<float>(1132396544u);
    uint v38 = (uint)(int)rint(v37);
    float v39 = v26 + v10;
    float _si40 = floor(v39);
    float _fr40 = v39 - _si40;
    float _lo40 = as_type<float>(((device uint*)p2)[safe_ix((int)_si40,buf_szs[2],4)] & oob_mask((int)_si40,buf_szs[2],4));
    float _hi40 = as_type<float>(((device uint*)p2)[safe_ix((int)_si40+1,buf_szs[2],4)] & oob_mask((int)_si40+1,buf_szs[2],4));
    float v40 = _lo40 + (_hi40 - _lo40) * _fr40;
    float v41 = max(v40, as_type<float>(0u));
    float v42 = min(v41, as_type<float>(1065353216u));
    float v43 = v42 * as_type<float>(1132396544u);
    uint v44 = (uint)(int)rint(v43);
    float _si45 = floor(v26);
    float _fr45 = v26 - _si45;
    float _lo45 = as_type<float>(((device uint*)p2)[safe_ix((int)_si45,buf_szs[2],4)] & oob_mask((int)_si45,buf_szs[2],4));
    float _hi45 = as_type<float>(((device uint*)p2)[safe_ix((int)_si45+1,buf_szs[2],4)] & oob_mask((int)_si45+1,buf_szs[2],4));
    float v45 = _lo45 + (_hi45 - _lo45) * _fr45;
    float v46 = max(v45, as_type<float>(0u));
    float v47 = min(v46, as_type<float>(1065353216u));
    float v48 = v47 * as_type<float>(1132396544u);
    uint v49 = (uint)(int)rint(v48);
    uint v50 = v49 | (v38 << 8u);
    uint v51 = v50 | (v44 << 16u);
    uint v52 = v51 | (v32 << 24u);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v52;
}
