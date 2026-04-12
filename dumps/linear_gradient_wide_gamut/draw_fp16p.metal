#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, limit0, limit1, limit2, stride0, stride1, stride2; };

kernel void umbra_entry(
    constant meta &m [[buffer(3)]],
    device uint *p0 [[buffer(0)]],
    device ushort *p1 [[buffer(1)]],
    device uint *p2 [[buffer(2)]],
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
    float v19 = max(v18, as_type<float>(0u));
    float v20 = min(v19, as_type<float>(1065353216u));
    float v21 = v20 * v7;
    float v22 = min(v9, v21);
    float _si23 = floor(v22);
    float _fr23 = v22 - _si23;
    uint _lo23 = 0; if ((uint)(int)_si23 < m.limit2) { _lo23 = p2[(int)_si23]; }
    uint _hi23 = 0; if ((uint)((int)_si23+1) < m.limit2) { _hi23 = p2[(int)_si23+1]; }
    float v23 = as_type<float>(_lo23) + (as_type<float>(_hi23) - as_type<float>(_lo23)) * _fr23;
    uint v24 = (uint)as_type<ushort>((half)v23);
    float v25 = v22 + v11;
    float _si26 = floor(v25);
    float _fr26 = v25 - _si26;
    uint _lo26 = 0; if ((uint)(int)_si26 < m.limit2) { _lo26 = p2[(int)_si26]; }
    uint _hi26 = 0; if ((uint)((int)_si26+1) < m.limit2) { _hi26 = p2[(int)_si26+1]; }
    float v26 = as_type<float>(_lo26) + (as_type<float>(_hi26) - as_type<float>(_lo26)) * _fr26;
    uint v27 = (uint)as_type<ushort>((half)v26);
    float v28 = as_type<float>(v6) + v22;
    float _si29 = floor(v28);
    float _fr29 = v28 - _si29;
    uint _lo29 = 0; if ((uint)(int)_si29 < m.limit2) { _lo29 = p2[(int)_si29]; }
    uint _hi29 = 0; if ((uint)((int)_si29+1) < m.limit2) { _hi29 = p2[(int)_si29+1]; }
    float v29 = as_type<float>(_lo29) + (as_type<float>(_hi29) - as_type<float>(_lo29)) * _fr29;
    uint v30 = (uint)as_type<ushort>((half)v29);
    float v31 = v22 + v10;
    float _si32 = floor(v31);
    float _fr32 = v31 - _si32;
    uint _lo32 = 0; if ((uint)(int)_si32 < m.limit2) { _lo32 = p2[(int)_si32]; }
    uint _hi32 = 0; if ((uint)((int)_si32+1) < m.limit2) { _hi32 = p2[(int)_si32+1]; }
    float v32 = as_type<float>(_lo32) + (as_type<float>(_hi32) - as_type<float>(_lo32)) * _fr32;
    uint v33 = (uint)as_type<ushort>((half)v32);
    { uint _row = y * m.stride1; uint _ps = m.limit1;
      p1[_row + x] = ushort(v24); p1[_row + x + _ps] = ushort(v30); p1[_row + x + 2*_ps] = ushort(v33); p1[_row + x + 3*_ps] = ushort(v27); }
}
