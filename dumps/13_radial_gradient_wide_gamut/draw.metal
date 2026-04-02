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
    uint v33 = v32 << 24u;
    float v34 = as_type<float>(v6) + v26;
    float _si35 = floor(v34);
    float _fr35 = v34 - _si35;
    float _lo35 = as_type<float>(((device uint*)p2)[safe_ix((int)_si35,buf_szs[2],4)] & oob_mask((int)_si35,buf_szs[2],4));
    float _hi35 = as_type<float>(((device uint*)p2)[safe_ix((int)_si35+1,buf_szs[2],4)] & oob_mask((int)_si35+1,buf_szs[2],4));
    float v35 = _lo35 + (_hi35 - _lo35) * _fr35;
    float v36 = max(v35, as_type<float>(0u));
    float v37 = min(v36, as_type<float>(1065353216u));
    float v38 = v37 * as_type<float>(1132396544u);
    uint v39 = (uint)(int)rint(v38);
    uint v40 = v39 << 8u;
    float v41 = v26 + v10;
    float _si42 = floor(v41);
    float _fr42 = v41 - _si42;
    float _lo42 = as_type<float>(((device uint*)p2)[safe_ix((int)_si42,buf_szs[2],4)] & oob_mask((int)_si42,buf_szs[2],4));
    float _hi42 = as_type<float>(((device uint*)p2)[safe_ix((int)_si42+1,buf_szs[2],4)] & oob_mask((int)_si42+1,buf_szs[2],4));
    float v42 = _lo42 + (_hi42 - _lo42) * _fr42;
    float v43 = max(v42, as_type<float>(0u));
    float v44 = min(v43, as_type<float>(1065353216u));
    float v45 = v44 * as_type<float>(1132396544u);
    uint v46 = (uint)(int)rint(v45);
    uint v47 = v46 << 16u;
    float _si48 = floor(v26);
    float _fr48 = v26 - _si48;
    float _lo48 = as_type<float>(((device uint*)p2)[safe_ix((int)_si48,buf_szs[2],4)] & oob_mask((int)_si48,buf_szs[2],4));
    float _hi48 = as_type<float>(((device uint*)p2)[safe_ix((int)_si48+1,buf_szs[2],4)] & oob_mask((int)_si48+1,buf_szs[2],4));
    float v48 = _lo48 + (_hi48 - _lo48) * _fr48;
    float v49 = max(v48, as_type<float>(0u));
    float v50 = min(v49, as_type<float>(1065353216u));
    float v51 = v50 * as_type<float>(1132396544u);
    uint v52 = (uint)(int)rint(v51);
    uint v53 = v52 | v40;
    uint v54 = v53 | v47;
    uint v55 = v54 | v33;
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = v55;
}
