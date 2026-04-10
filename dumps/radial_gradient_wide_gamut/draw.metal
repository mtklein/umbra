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
    float _si27 = floor(v26);
    float _fr27 = v26 - _si27;
    float _lo27 = as_type<float>(((device uint*)p2)[safe_ix((int)_si27,buf_szs[2],4)] & oob_mask((int)_si27,buf_szs[2],4));
    float _hi27 = as_type<float>(((device uint*)p2)[safe_ix((int)_si27+1,buf_szs[2],4)] & oob_mask((int)_si27+1,buf_szs[2],4));
    float v27 = _lo27 + (_hi27 - _lo27) * _fr27;
    float v28 = max(v27, as_type<float>(0u));
    float v29 = min(v28, as_type<float>(1065353216u));
    float v30 = v29 * as_type<float>(1132396544u);
    uint v31 = (uint)(int)rint(v30);
    float v32 = v26 + v11;
    float _si33 = floor(v32);
    float _fr33 = v32 - _si33;
    float _lo33 = as_type<float>(((device uint*)p2)[safe_ix((int)_si33,buf_szs[2],4)] & oob_mask((int)_si33,buf_szs[2],4));
    float _hi33 = as_type<float>(((device uint*)p2)[safe_ix((int)_si33+1,buf_szs[2],4)] & oob_mask((int)_si33+1,buf_szs[2],4));
    float v33 = _lo33 + (_hi33 - _lo33) * _fr33;
    float v34 = max(v33, as_type<float>(0u));
    float v35 = min(v34, as_type<float>(1065353216u));
    float v36 = v35 * as_type<float>(1132396544u);
    uint v37 = (uint)(int)rint(v36);
    float v38 = as_type<float>(v6) + v26;
    float _si39 = floor(v38);
    float _fr39 = v38 - _si39;
    float _lo39 = as_type<float>(((device uint*)p2)[safe_ix((int)_si39,buf_szs[2],4)] & oob_mask((int)_si39,buf_szs[2],4));
    float _hi39 = as_type<float>(((device uint*)p2)[safe_ix((int)_si39+1,buf_szs[2],4)] & oob_mask((int)_si39+1,buf_szs[2],4));
    float v39 = _lo39 + (_hi39 - _lo39) * _fr39;
    float v40 = max(v39, as_type<float>(0u));
    float v41 = min(v40, as_type<float>(1065353216u));
    float v42 = v41 * as_type<float>(1132396544u);
    uint v43 = (uint)(int)rint(v42);
    float v44 = v26 + v10;
    float _si45 = floor(v44);
    float _fr45 = v44 - _si45;
    float _lo45 = as_type<float>(((device uint*)p2)[safe_ix((int)_si45,buf_szs[2],4)] & oob_mask((int)_si45,buf_szs[2],4));
    float _hi45 = as_type<float>(((device uint*)p2)[safe_ix((int)_si45+1,buf_szs[2],4)] & oob_mask((int)_si45+1,buf_szs[2],4));
    float v45 = _lo45 + (_hi45 - _lo45) * _fr45;
    float v46 = max(v45, as_type<float>(0u));
    float v47 = min(v46, as_type<float>(1065353216u));
    float v48 = v47 * as_type<float>(1132396544u);
    uint v49 = (uint)(int)rint(v48);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = (v31 & 0xFFu) | ((v43 & 0xFFu) << 8u) | ((v49 & 0xFFu) << 16u) | (v37 << 24u);
}
