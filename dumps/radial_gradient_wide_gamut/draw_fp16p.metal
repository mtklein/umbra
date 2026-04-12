#include <metal_stdlib>
using namespace metal;


kernel void umbra_entry(
    constant uint &w [[buffer(3)]],
    constant uint *buf_limit [[buffer(4)]],
    constant uint *buf_stride [[buffer(5)]],
    constant uint &x0 [[buffer(6)]],
    constant uint &y0 [[buffer(7)]],
    device uint *p0 [[buffer(0)]],
    device ushort *p1 [[buffer(1)]],
    device uint *p2 [[buffer(2)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= w) return;
    uint x = x0 + pos.x;
    uint y = y0 + pos.y;
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
    uint v12 = x0 + pos.x;
    float v13 = (float)(int)v12;
    float v14 = v13 - as_type<float>(v2);
    uint v15 = y0 + pos.y;
    float v16 = (float)(int)v15;
    float v17 = v16 - as_type<float>(v3);
    float v18 = v17 * v17;
    float v19 = fma(v14, v14, v18);
    float v20 = precise::sqrt(v19);
    float v21 = as_type<float>(v4) * v20;
    float v22 = max(v21, as_type<float>(0u));
    float v23 = min(v22, as_type<float>(1065353216u));
    float v24 = v23 * v7;
    float v25 = min(v9, v24);
    float _si26 = floor(v25);
    float _fr26 = v25 - _si26;
    uint _lo26 = 0; if ((uint)(int)_si26 < buf_limit[2]) { _lo26 = p2[(int)_si26]; }
    uint _hi26 = 0; if ((uint)((int)_si26+1) < buf_limit[2]) { _hi26 = p2[(int)_si26+1]; }
    float v26 = as_type<float>(_lo26) + (as_type<float>(_hi26) - as_type<float>(_lo26)) * _fr26;
    uint v27 = (uint)as_type<ushort>((half)v26);
    float v28 = v25 + v11;
    float _si29 = floor(v28);
    float _fr29 = v28 - _si29;
    uint _lo29 = 0; if ((uint)(int)_si29 < buf_limit[2]) { _lo29 = p2[(int)_si29]; }
    uint _hi29 = 0; if ((uint)((int)_si29+1) < buf_limit[2]) { _hi29 = p2[(int)_si29+1]; }
    float v29 = as_type<float>(_lo29) + (as_type<float>(_hi29) - as_type<float>(_lo29)) * _fr29;
    uint v30 = (uint)as_type<ushort>((half)v29);
    float v31 = as_type<float>(v6) + v25;
    float _si32 = floor(v31);
    float _fr32 = v31 - _si32;
    uint _lo32 = 0; if ((uint)(int)_si32 < buf_limit[2]) { _lo32 = p2[(int)_si32]; }
    uint _hi32 = 0; if ((uint)((int)_si32+1) < buf_limit[2]) { _hi32 = p2[(int)_si32+1]; }
    float v32 = as_type<float>(_lo32) + (as_type<float>(_hi32) - as_type<float>(_lo32)) * _fr32;
    uint v33 = (uint)as_type<ushort>((half)v32);
    float v34 = v25 + v10;
    float _si35 = floor(v34);
    float _fr35 = v34 - _si35;
    uint _lo35 = 0; if ((uint)(int)_si35 < buf_limit[2]) { _lo35 = p2[(int)_si35]; }
    uint _hi35 = 0; if ((uint)((int)_si35+1) < buf_limit[2]) { _hi35 = p2[(int)_si35+1]; }
    float v35 = as_type<float>(_lo35) + (as_type<float>(_hi35) - as_type<float>(_lo35)) * _fr35;
    uint v36 = (uint)as_type<ushort>((half)v35);
    { uint _row = y * buf_stride[1]; uint _ps = buf_limit[1];
      p1[_row + x] = ushort(v27); p1[_row + x + _ps] = ushort(v33); p1[_row + x + 2*_ps] = ushort(v36); p1[_row + x + 3*_ps] = ushort(v30); }
}
