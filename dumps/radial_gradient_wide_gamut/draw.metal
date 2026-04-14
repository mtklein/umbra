#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, count2, stride0, stride1, stride2; };

kernel void umbra_entry(
    constant meta &m [[buffer(3)]],
    device const uint * __restrict p0 [[buffer(0)]],
    device half4 * __restrict p1 [[buffer(1)]],
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
    float v14 = v13 - as_type<float>(v2);
    uint v15 = m.y0 + pos.y;
    float v16 = (float)(int)v15;
    float v17 = v16 - as_type<float>(v3);
    float v18 = v17 * v17;
    float v19 = fma(v14, v14, v18);
    float v20 = precise::sqrt(v19);
    float v21 = as_type<float>(v4) * v20;
    float v23 = max(v21, as_type<float>(0u));
    #define v24 v23
    float v26 = min(v24, as_type<float>(1065353216u));
    #define v27 v26
    float v28 = v27 * v7;
    float v29 = min(v9, v28);
    float _si30 = floor(v29);
    float _fr30 = v29 - _si30;
    uint _lo30 = p2[min((uint)(int)_si30, m.count2 - 1u)] & (((uint)(int)_si30 < m.count2) ? ~0u : 0u);
    uint _hi30 = p2[min((uint)((int)_si30+1), m.count2 - 1u)] & (((uint)((int)_si30+1) < m.count2) ? ~0u : 0u);
    float v30 = as_type<float>(_lo30) + (as_type<float>(_hi30) - as_type<float>(_lo30)) * _fr30;
    uint v31 = (uint)as_type<ushort>((half)v30);
    float v32 = v29 + v11;
    float _si33 = floor(v32);
    float _fr33 = v32 - _si33;
    uint _lo33 = p2[min((uint)(int)_si33, m.count2 - 1u)] & (((uint)(int)_si33 < m.count2) ? ~0u : 0u);
    uint _hi33 = p2[min((uint)((int)_si33+1), m.count2 - 1u)] & (((uint)((int)_si33+1) < m.count2) ? ~0u : 0u);
    float v33 = as_type<float>(_lo33) + (as_type<float>(_hi33) - as_type<float>(_lo33)) * _fr33;
    uint v34 = (uint)as_type<ushort>((half)v33);
    float v35 = as_type<float>(v6) + v29;
    float _si36 = floor(v35);
    float _fr36 = v35 - _si36;
    uint _lo36 = p2[min((uint)(int)_si36, m.count2 - 1u)] & (((uint)(int)_si36 < m.count2) ? ~0u : 0u);
    uint _hi36 = p2[min((uint)((int)_si36+1), m.count2 - 1u)] & (((uint)((int)_si36+1) < m.count2) ? ~0u : 0u);
    float v36 = as_type<float>(_lo36) + (as_type<float>(_hi36) - as_type<float>(_lo36)) * _fr36;
    uint v37 = (uint)as_type<ushort>((half)v36);
    float v38 = v29 + v10;
    float _si39 = floor(v38);
    float _fr39 = v38 - _si39;
    uint _lo39 = p2[min((uint)(int)_si39, m.count2 - 1u)] & (((uint)(int)_si39 < m.count2) ? ~0u : 0u);
    uint _hi39 = p2[min((uint)((int)_si39+1), m.count2 - 1u)] & (((uint)((int)_si39+1) < m.count2) ? ~0u : 0u);
    float v39 = as_type<float>(_lo39) + (as_type<float>(_hi39) - as_type<float>(_lo39)) * _fr39;
    uint v40 = (uint)as_type<ushort>((half)v39);
    p1[y * m.stride1 + x] = half4(as_type<half>((ushort)v31), as_type<half>((ushort)v37), as_type<half>((ushort)v40), as_type<half>((ushort)v34));
}
