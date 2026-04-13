#include <metal_stdlib>
using namespace metal;


struct meta { uint w, x0, y0, count0, count1, count2, stride0, stride1, stride2; };

kernel void umbra_entry(
    constant meta &m [[buffer(3)]],
    device uint *p0 [[buffer(0)]],
    device half4 *p1 [[buffer(1)]],
    device ushort *p2 [[buffer(2)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= m.w) return;
    uint x = m.x0 + pos.x;
    uint y = m.y0 + pos.y;
    uint v0 = 0u;
    uint v1 = p0[0];
    uint v2 = p0[1];
    uint v3 = p0[2];
    uint v4 = p0[3];
    uint v6 = p0[4];
    uint v7 = p0[5];
    uint v8 = p0[6];
    uint v9 = p0[7];
    uint v10 = p0[8];
    uint v11 = p0[9];
    uint v12 = p0[10];
    uint v13 = p0[11];
    uint v14 = p0[12];
    uint v15 = p0[13];
    uint v16 = p0[14];
    uint v17 = 1065353216u;
    float v18 = as_type<float>(v15) - as_type<float>(1065353216u);
    float v19 = as_type<float>(v16) - as_type<float>(1065353216u);
    uint v20 = (uint)(int)floor(as_type<float>(v15));
    uint v21 = 998277249u;
    float v22 = as_type<float>(v17) - as_type<float>(v4);
    half4 _px23 = p1[y * m.stride1 + x];
    uint v23 = (uint)as_type<ushort>(_px23.x);
    uint v23_1 = (uint)as_type<ushort>(_px23.y);
    uint v23_2 = (uint)as_type<ushort>(_px23.z);
    uint v23_3 = (uint)as_type<ushort>(_px23.w);
    float v24 = (float)as_type<half>((ushort)v23);
    float v25 = fma(v24, v22, as_type<float>(v1));
    float v26 = v25 - v24;
    uint v27 = m.x0 + pos.x;
    float v28 = (float)(int)v27;
    uint v29 = m.y0 + pos.y;
    float v30 = (float)(int)v29;
    float v31 = v30 * as_type<float>(v13);
    float v32 = fma(v28, as_type<float>(v12), v31);
    float v33 = as_type<float>(v14) + v32;
    float v34 = v30 * as_type<float>(v10);
    float v35 = fma(v28, as_type<float>(v9), v34);
    float v36 = as_type<float>(v11) + v35;
    float v37 = v36 / v33;
    uint v38 = as_type<float>(v0) <= v37 ? 0xffffffffu : 0u;
    float v39 = v30 * as_type<float>(v7);
    float v40 = fma(v28, as_type<float>(v6), v39);
    float v41 = as_type<float>(v8) + v40;
    float v42 = v41 / v33;
    uint v43 = as_type<float>(v0) <= v42 ? 0xffffffffu : 0u;
    uint v44 = v42 <  as_type<float>(v15) ? 0xffffffffu : 0u;
    uint v45 = v43 & v44;
    uint v46 = v37 <  as_type<float>(v16) ? 0xffffffffu : 0u;
    uint v47 = v38 & v46;
    uint v48 = v45 & v47;
    float v49 = max(v42, as_type<float>(0u));
    float v50 = min(v49, v18);
    uint v51 = (uint)(int)floor(v50);
    float v52 = max(v37, as_type<float>(0u));
    float v53 = min(v52, v19);
    uint v54 = (uint)(int)floor(v53);
    uint v55 = v54 * v20;
    uint v56 = v51 + v55;
    uint v57 = (uint)p2[min(v56, m.count2 - 1u)] & ((v56 < m.count2) ? ~0u : 0u);
    uint v58 = (uint)(int)(short)(ushort)v57;
    float v59 = (float)(int)v58;
    float v60 = v59 * as_type<float>(998277249u);
    uint v61 = select(v0, as_type<uint>(v60), v48 != 0u);
    float v62 = fma(as_type<float>(v61), v26, v24);
    uint v63 = (uint)as_type<ushort>((half)v62);
    float v64 = (float)as_type<half>((ushort)v23_3);
    float v65 = fma(v64, v22, as_type<float>(v4));
    float v66 = v65 - v64;
    float v67 = fma(as_type<float>(v61), v66, v64);
    uint v68 = (uint)as_type<ushort>((half)v67);
    float v69 = (float)as_type<half>((ushort)v23_1);
    float v70 = fma(v69, v22, as_type<float>(v2));
    float v71 = v70 - v69;
    float v72 = fma(as_type<float>(v61), v71, v69);
    uint v73 = (uint)as_type<ushort>((half)v72);
    float v74 = (float)as_type<half>((ushort)v23_2);
    float v75 = fma(v74, v22, as_type<float>(v3));
    float v76 = v75 - v74;
    float v77 = fma(as_type<float>(v61), v76, v74);
    uint v78 = (uint)as_type<ushort>((half)v77);
    p1[y * m.stride1 + x] = half4(as_type<half>((ushort)v63), as_type<half>((ushort)v73), as_type<half>((ushort)v78), as_type<half>((ushort)v68));
}
