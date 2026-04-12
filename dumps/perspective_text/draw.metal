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
    device ushort *p2 [[buffer(2)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= w) return;
    uint x = x0 + pos.x;
    uint y = y0 + pos.y;
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
    uint v23 = x0 + pos.x;
    float v24 = (float)(int)v23;
    uint v25 = y0 + pos.y;
    float v26 = (float)(int)v25;
    float v27 = v26 * as_type<float>(v13);
    float v28 = fma(v24, as_type<float>(v12), v27);
    float v29 = as_type<float>(v14) + v28;
    float v30 = v26 * as_type<float>(v10);
    float v31 = fma(v24, as_type<float>(v9), v30);
    float v32 = as_type<float>(v11) + v31;
    float v33 = v32 / v29;
    uint v34 = as_type<float>(v0) <= v33 ? 0xffffffffu : 0u;
    float v35 = v26 * as_type<float>(v7);
    float v36 = fma(v24, as_type<float>(v6), v35);
    float v37 = as_type<float>(v8) + v36;
    float v38 = v37 / v29;
    uint v39 = as_type<float>(v0) <= v38 ? 0xffffffffu : 0u;
    uint v40 = v38 <  as_type<float>(v15) ? 0xffffffffu : 0u;
    uint v41 = v39 & v40;
    uint v42 = v33 <  as_type<float>(v16) ? 0xffffffffu : 0u;
    uint v43 = v34 & v42;
    uint v44 = v41 & v43;
    float v45 = max(v38, as_type<float>(0u));
    float v46 = min(v45, v18);
    uint v47 = (uint)(int)floor(v46);
    float v48 = max(v33, as_type<float>(0u));
    float v49 = min(v48, v19);
    uint v50 = (uint)(int)floor(v49);
    uint v51 = v50 * v20;
    uint v52 = v47 + v51;
    uint v53 = 0; if (v52 < buf_limit[2]) { v53 = (uint)p2[v52]; }
    uint v54 = (uint)(int)(short)(ushort)v53;
    float v55 = (float)(int)v54;
    float v56 = v55 * as_type<float>(998277249u);
    uint v57 = select(v0, as_type<uint>(v56), v44 != 0u);
    uint _base58 = y * buf_stride[1] + x*4;
    uint v58 = (uint)p1[_base58];
    uint v58_1 = (uint)p1[_base58+1];
    uint v58_2 = (uint)p1[_base58+2];
    uint v58_3 = (uint)p1[_base58+3];
    float v59 = (float)as_type<half>((ushort)v58);
    float v60 = fma(v59, v22, as_type<float>(v1));
    float v61 = v60 - v59;
    float v62 = fma(as_type<float>(v57), v61, v59);
    uint v63 = (uint)as_type<ushort>((half)v62);
    float v64 = (float)as_type<half>((ushort)v58_3);
    float v65 = fma(v64, v22, as_type<float>(v4));
    float v66 = v65 - v64;
    float v67 = fma(as_type<float>(v57), v66, v64);
    uint v68 = (uint)as_type<ushort>((half)v67);
    float v69 = (float)as_type<half>((ushort)v58_1);
    float v70 = fma(v69, v22, as_type<float>(v2));
    float v71 = v70 - v69;
    float v72 = fma(as_type<float>(v57), v71, v69);
    uint v73 = (uint)as_type<ushort>((half)v72);
    float v74 = (float)as_type<half>((ushort)v58_2);
    float v75 = fma(v74, v22, as_type<float>(v3));
    float v76 = v75 - v74;
    float v77 = fma(as_type<float>(v57), v76, v74);
    uint v78 = (uint)as_type<ushort>((half)v77);
    { uint _base = y * buf_stride[1] + x*4;
      p1[_base] = ushort(v63); p1[_base+1] = ushort(v73); p1[_base+2] = ushort(v78); p1[_base+3] = ushort(v68); }
}
