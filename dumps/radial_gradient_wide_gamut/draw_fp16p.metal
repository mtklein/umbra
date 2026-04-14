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
    uint v6 = p0[3];
    float v8 = as_type<float>(v6) - as_type<float>(1065353216u);
    #define v9 v8
    float v12 = as_type<float>(v6) - as_type<float>(1073741824u);
    #define v13 v12
    float v14 = as_type<float>(v6) + as_type<float>(v6);
    float v15 = as_type<float>(v6) + v14;
    uint v16 = m.x0 + pos.x;
    float v17 = (float)(int)v16;
    float v18 = v17 - as_type<float>(v2);
    uint v19 = m.y0 + pos.y;
    float v20 = (float)(int)v19;
    float v21 = v20 - as_type<float>(v3);
    float v22 = v21 * v21;
    float v23 = fma(v18, v18, v22);
    float v24 = precise::sqrt(v23);
    float v25 = as_type<float>(v4) * v24;
    float v27 = max(v25, as_type<float>(0u));
    #define v28 v27
    float v30 = min(v28, as_type<float>(1065353216u));
    #define v31 v30
    float v32 = v31 * v9;
    float v33 = min(v13, v32);
    float _si34 = floor(v33);
    float _fr34 = v33 - _si34;
    uint _lo34 = p2[min((uint)(int)_si34, m.count2 - 1u)] & (((uint)(int)_si34 < m.count2) ? ~0u : 0u);
    uint _hi34 = p2[min((uint)((int)_si34+1), m.count2 - 1u)] & (((uint)((int)_si34+1) < m.count2) ? ~0u : 0u);
    float v34 = as_type<float>(_lo34) + (as_type<float>(_hi34) - as_type<float>(_lo34)) * _fr34;
    uint v35 = (uint)as_type<ushort>((half)v34);
    float v36 = v33 + v15;
    float _si37 = floor(v36);
    float _fr37 = v36 - _si37;
    uint _lo37 = p2[min((uint)(int)_si37, m.count2 - 1u)] & (((uint)(int)_si37 < m.count2) ? ~0u : 0u);
    uint _hi37 = p2[min((uint)((int)_si37+1), m.count2 - 1u)] & (((uint)((int)_si37+1) < m.count2) ? ~0u : 0u);
    float v37 = as_type<float>(_lo37) + (as_type<float>(_hi37) - as_type<float>(_lo37)) * _fr37;
    uint v38 = (uint)as_type<ushort>((half)v37);
    float v39 = as_type<float>(v6) + v33;
    float _si40 = floor(v39);
    float _fr40 = v39 - _si40;
    uint _lo40 = p2[min((uint)(int)_si40, m.count2 - 1u)] & (((uint)(int)_si40 < m.count2) ? ~0u : 0u);
    uint _hi40 = p2[min((uint)((int)_si40+1), m.count2 - 1u)] & (((uint)((int)_si40+1) < m.count2) ? ~0u : 0u);
    float v40 = as_type<float>(_lo40) + (as_type<float>(_hi40) - as_type<float>(_lo40)) * _fr40;
    uint v41 = (uint)as_type<ushort>((half)v40);
    float v42 = v33 + v14;
    float _si43 = floor(v42);
    float _fr43 = v42 - _si43;
    uint _lo43 = p2[min((uint)(int)_si43, m.count2 - 1u)] & (((uint)(int)_si43 < m.count2) ? ~0u : 0u);
    uint _hi43 = p2[min((uint)((int)_si43+1), m.count2 - 1u)] & (((uint)((int)_si43+1) < m.count2) ? ~0u : 0u);
    float v43 = as_type<float>(_lo43) + (as_type<float>(_hi43) - as_type<float>(_lo43)) * _fr43;
    uint v44 = (uint)as_type<ushort>((half)v43);
    { uint _row = y * m.stride1; uint _ps = m.count1 / 4;
      p1[_row + x] = ushort(v35); p1[_row + x + _ps] = ushort(v41); p1[_row + x + 2*_ps] = ushort(v44); p1[_row + x + 3*_ps] = ushort(v38); }
}
