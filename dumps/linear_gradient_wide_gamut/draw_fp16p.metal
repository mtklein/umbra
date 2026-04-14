#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, count2, stride0, stride1, stride2; };

kernel void umbra_entry(
    constant meta &m [[buffer(0)]],
    device const uint * __restrict p0 [[buffer(1)]],
    device ushort * __restrict p1 [[buffer(2)]],
    device const uint * __restrict p2 [[buffer(3)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= m.w) return;
    uint x = m.x0 + pos.x;
    uint y = m.y0 + pos.y;
    uint v0 = 0u;
    uint v2 = p0[0];
    uint v3 = p0[1];
    uint v4 = p0[2];
    uint v6 = p0[3];
    float v8 = as_type<float>(v6) - as_type<float>(1065353216u);
    #define v9 v8
    float v12 = as_type<float>(v6) - as_type<float>(1073741824u);
    #define v13 v12
    float v14 = as_type<float>(v6) + as_type<float>(v6);
    float v15 = as_type<float>(v6) + v14;
    uint v16 = m.x0 + pos.x;
    float v17 = (float)(int)v16;
    uint v18 = m.y0 + pos.y;
    float v19 = (float)(int)v18;
    float v20 = v19 * as_type<float>(v3);
    float v21 = fma(v17, as_type<float>(v2), v20);
    float v22 = as_type<float>(v4) + v21;
    float v24 = max(v22, as_type<float>(0u));
    #define v25 v24
    float v27 = min(v25, as_type<float>(1065353216u));
    #define v28 v27
    float v29 = v28 * v9;
    float v30 = min(v13, v29);
    float _si31 = floor(v30);
    float _fr31 = v30 - _si31;
    uint _lo31 = p2[min((uint)(int)_si31, m.count2 - 1u)] & (((uint)(int)_si31 < m.count2) ? ~0u : 0u);
    uint _hi31 = p2[min((uint)((int)_si31+1), m.count2 - 1u)] & (((uint)((int)_si31+1) < m.count2) ? ~0u : 0u);
    float v31 = as_type<float>(_lo31) + (as_type<float>(_hi31) - as_type<float>(_lo31)) * _fr31;
    uint v32 = (uint)as_type<ushort>((half)v31);
    float v33 = v30 + v15;
    float _si34 = floor(v33);
    float _fr34 = v33 - _si34;
    uint _lo34 = p2[min((uint)(int)_si34, m.count2 - 1u)] & (((uint)(int)_si34 < m.count2) ? ~0u : 0u);
    uint _hi34 = p2[min((uint)((int)_si34+1), m.count2 - 1u)] & (((uint)((int)_si34+1) < m.count2) ? ~0u : 0u);
    float v34 = as_type<float>(_lo34) + (as_type<float>(_hi34) - as_type<float>(_lo34)) * _fr34;
    uint v35 = (uint)as_type<ushort>((half)v34);
    float v36 = as_type<float>(v6) + v30;
    float _si37 = floor(v36);
    float _fr37 = v36 - _si37;
    uint _lo37 = p2[min((uint)(int)_si37, m.count2 - 1u)] & (((uint)(int)_si37 < m.count2) ? ~0u : 0u);
    uint _hi37 = p2[min((uint)((int)_si37+1), m.count2 - 1u)] & (((uint)((int)_si37+1) < m.count2) ? ~0u : 0u);
    float v37 = as_type<float>(_lo37) + (as_type<float>(_hi37) - as_type<float>(_lo37)) * _fr37;
    uint v38 = (uint)as_type<ushort>((half)v37);
    float v39 = v30 + v14;
    float _si40 = floor(v39);
    float _fr40 = v39 - _si40;
    uint _lo40 = p2[min((uint)(int)_si40, m.count2 - 1u)] & (((uint)(int)_si40 < m.count2) ? ~0u : 0u);
    uint _hi40 = p2[min((uint)((int)_si40+1), m.count2 - 1u)] & (((uint)((int)_si40+1) < m.count2) ? ~0u : 0u);
    float v40 = as_type<float>(_lo40) + (as_type<float>(_hi40) - as_type<float>(_lo40)) * _fr40;
    uint v41 = (uint)as_type<ushort>((half)v40);
    { uint _row = y * m.stride1; uint _ps = m.count1 / 4;
      p1[_row + x] = ushort(v32); p1[_row + x + _ps] = ushort(v38); p1[_row + x + 2*_ps] = ushort(v41); p1[_row + x + 3*_ps] = ushort(v35); }
}
