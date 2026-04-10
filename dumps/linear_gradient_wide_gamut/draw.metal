#include <metal_stdlib>
using namespace metal;

int safe_ix(int ix, uint bytes, int elem) {
    int count = (int)(bytes / (uint)elem);
    return clamp(ix, 0, max(count-1, 0));
}
uint oob_mask(int ix, uint bytes, int elem) {
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
    float _si24 = floor(v23);
    float _fr24 = v23 - _si24;
    float _lo24 = as_type<float>(((device uint*)p2)[safe_ix((int)_si24,buf_szs[2],4)] & oob_mask((int)_si24,buf_szs[2],4));
    float _hi24 = as_type<float>(((device uint*)p2)[safe_ix((int)_si24+1,buf_szs[2],4)] & oob_mask((int)_si24+1,buf_szs[2],4));
    float v24 = _lo24 + (_hi24 - _lo24) * _fr24;
    float v25 = max(v24, as_type<float>(0u));
    float v26 = min(v25, as_type<float>(1065353216u));
    float v27 = v26 * as_type<float>(1132396544u);
    uint v28 = (uint)(int)rint(v27);
    float v29 = v23 + v11;
    float _si30 = floor(v29);
    float _fr30 = v29 - _si30;
    float _lo30 = as_type<float>(((device uint*)p2)[safe_ix((int)_si30,buf_szs[2],4)] & oob_mask((int)_si30,buf_szs[2],4));
    float _hi30 = as_type<float>(((device uint*)p2)[safe_ix((int)_si30+1,buf_szs[2],4)] & oob_mask((int)_si30+1,buf_szs[2],4));
    float v30 = _lo30 + (_hi30 - _lo30) * _fr30;
    float v31 = max(v30, as_type<float>(0u));
    float v32 = min(v31, as_type<float>(1065353216u));
    float v33 = v32 * as_type<float>(1132396544u);
    uint v34 = (uint)(int)rint(v33);
    float v35 = as_type<float>(v6) + v23;
    float _si36 = floor(v35);
    float _fr36 = v35 - _si36;
    float _lo36 = as_type<float>(((device uint*)p2)[safe_ix((int)_si36,buf_szs[2],4)] & oob_mask((int)_si36,buf_szs[2],4));
    float _hi36 = as_type<float>(((device uint*)p2)[safe_ix((int)_si36+1,buf_szs[2],4)] & oob_mask((int)_si36+1,buf_szs[2],4));
    float v36 = _lo36 + (_hi36 - _lo36) * _fr36;
    float v37 = max(v36, as_type<float>(0u));
    float v38 = min(v37, as_type<float>(1065353216u));
    float v39 = v38 * as_type<float>(1132396544u);
    uint v40 = (uint)(int)rint(v39);
    float v41 = v23 + v10;
    float _si42 = floor(v41);
    float _fr42 = v41 - _si42;
    float _lo42 = as_type<float>(((device uint*)p2)[safe_ix((int)_si42,buf_szs[2],4)] & oob_mask((int)_si42,buf_szs[2],4));
    float _hi42 = as_type<float>(((device uint*)p2)[safe_ix((int)_si42+1,buf_szs[2],4)] & oob_mask((int)_si42+1,buf_szs[2],4));
    float v42 = _lo42 + (_hi42 - _lo42) * _fr42;
    float v43 = max(v42, as_type<float>(0u));
    float v44 = min(v43, as_type<float>(1065353216u));
    float v45 = v44 * as_type<float>(1132396544u);
    uint v46 = (uint)(int)rint(v45);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = (v28 & 0xFFu) | ((v40 & 0xFFu) << 8u) | ((v46 & 0xFFu) << 16u) | (v34 << 24u);
}
