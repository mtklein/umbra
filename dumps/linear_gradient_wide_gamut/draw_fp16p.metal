#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, count2, stride0, stride1, stride2; };

kernel void umbra_entry(
    constant meta &m [[buffer(3)]],
    device const uint * __restrict p0 [[buffer(0)]],
    device ushort * __restrict p1 [[buffer(1)]],
    device const uint * __restrict p2 [[buffer(2)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= m.w) return;
    uint x = m.x0 + pos.x;
    uint y = m.y0 + pos.y;
    uint v0 = 0u;
    uint v2 = p0[0];
    uint v3 = p0[1];
    uint v4 = p0[2];
    uint v5 = 1065353216u;
    uint v6 = p0[3];
    float v7 = as_type<float>(v6) - as_type<float>(1065353216u);
    uint v8 = 1073741824u;
    float v9 = as_type<float>(v6) - as_type<float>(1073741824u);
    float v10 = as_type<float>(v6) + as_type<float>(v6);
    float v11 = as_type<float>(v6) + v10;
    uint v12 = m.x0 + pos.x;
    float v13 = (float)(int)v12;
    uint v14 = m.y0 + pos.y;
    float v15 = (float)(int)v14;
    float v16 = v15 * as_type<float>(v3);
    float v17 = fma(v13, as_type<float>(v2), v16);
    float v18 = as_type<float>(v4) + v17;
    float v20 = max(v18, as_type<float>(0u));
    #define v21 v20
    float v23 = min(v21, as_type<float>(1065353216u));
    #define v24 v23
    float v25 = v24 * v7;
    float v26 = min(v9, v25);
    float _si27 = floor(v26);
    float _fr27 = v26 - _si27;
    uint _lo27 = p2[min((uint)(int)_si27, m.count2 - 1u)] & (((uint)(int)_si27 < m.count2) ? ~0u : 0u);
    uint _hi27 = p2[min((uint)((int)_si27+1), m.count2 - 1u)] & (((uint)((int)_si27+1) < m.count2) ? ~0u : 0u);
    float v27 = as_type<float>(_lo27) + (as_type<float>(_hi27) - as_type<float>(_lo27)) * _fr27;
    uint v28 = (uint)as_type<ushort>((half)v27);
    float v29 = v26 + v11;
    float _si30 = floor(v29);
    float _fr30 = v29 - _si30;
    uint _lo30 = p2[min((uint)(int)_si30, m.count2 - 1u)] & (((uint)(int)_si30 < m.count2) ? ~0u : 0u);
    uint _hi30 = p2[min((uint)((int)_si30+1), m.count2 - 1u)] & (((uint)((int)_si30+1) < m.count2) ? ~0u : 0u);
    float v30 = as_type<float>(_lo30) + (as_type<float>(_hi30) - as_type<float>(_lo30)) * _fr30;
    uint v31 = (uint)as_type<ushort>((half)v30);
    float v32 = as_type<float>(v6) + v26;
    float _si33 = floor(v32);
    float _fr33 = v32 - _si33;
    uint _lo33 = p2[min((uint)(int)_si33, m.count2 - 1u)] & (((uint)(int)_si33 < m.count2) ? ~0u : 0u);
    uint _hi33 = p2[min((uint)((int)_si33+1), m.count2 - 1u)] & (((uint)((int)_si33+1) < m.count2) ? ~0u : 0u);
    float v33 = as_type<float>(_lo33) + (as_type<float>(_hi33) - as_type<float>(_lo33)) * _fr33;
    uint v34 = (uint)as_type<ushort>((half)v33);
    float v35 = v26 + v10;
    float _si36 = floor(v35);
    float _fr36 = v35 - _si36;
    uint _lo36 = p2[min((uint)(int)_si36, m.count2 - 1u)] & (((uint)(int)_si36 < m.count2) ? ~0u : 0u);
    uint _hi36 = p2[min((uint)((int)_si36+1), m.count2 - 1u)] & (((uint)((int)_si36+1) < m.count2) ? ~0u : 0u);
    float v36 = as_type<float>(_lo36) + (as_type<float>(_hi36) - as_type<float>(_lo36)) * _fr36;
    uint v37 = (uint)as_type<ushort>((half)v36);
    { uint _row = y * m.stride1; uint _ps = m.count1 / 4;
      p1[_row + x] = ushort(v28); p1[_row + x + _ps] = ushort(v34); p1[_row + x + 2*_ps] = ushort(v37); p1[_row + x + 3*_ps] = ushort(v31); }
}
