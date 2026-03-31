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
    uint v1 = ((device const uint*)p0)[0];
    uint v2 = ((device const uint*)p0)[1];
    uint v3 = ((device const uint*)p0)[2];
    uint v4 = ((device const uint*)p0)[3];
    uint v6 = ((device const uint*)p0)[4];
    uint v7 = ((device const uint*)p0)[5];
    uint v8 = ((device const uint*)p0)[6];
    uint v9 = ((device const uint*)p0)[7];
    uint v10 = ((device const uint*)p0)[8];
    uint v11 = ((device const uint*)p0)[9];
    uint v12 = ((device const uint*)p0)[10];
    uint v13 = ((device const uint*)p0)[11];
    uint v14 = ((device const uint*)p0)[12];
    uint v15 = ((device const uint*)p0)[13];
    uint v16 = ((device const uint*)p0)[14];
    uint v17 = 1065353216u;
    float v18 = as_type<float>(v15) - as_type<float>(1065353216u);
    float v19 = as_type<float>(v16) - as_type<float>(1065353216u);
    uint v20 = (uint)(int)floor(as_type<float>(v15));
    uint v21 = 998277249u;
    uint v22 = 255u;
    float v23 = as_type<float>(v17) - as_type<float>(v4);
    uint v24 = x0 + pos.x;
    float v25 = (float)(int)v24;
    uint v26 = y0 + pos.y;
    float v27 = (float)(int)v26;
    float v28 = v27 * as_type<float>(v10);
    float v29 = fma(v25, as_type<float>(v9), v28);
    float v30 = as_type<float>(v11) + v29;
    float v31 = v27 * as_type<float>(v13);
    float v32 = fma(v25, as_type<float>(v12), v31);
    float v33 = as_type<float>(v14) + v32;
    float v34 = v30 / v33;
    uint v35 = v34 <  as_type<float>(v16) ? 0xffffffffu : 0u;
    float v36 = v27 * as_type<float>(v7);
    float v37 = fma(v25, as_type<float>(v6), v36);
    float v38 = as_type<float>(v8) + v37;
    float v39 = v38 / v33;
    uint v40 = v39 <  as_type<float>(v15) ? 0xffffffffu : 0u;
    float v41 = max(v39, as_type<float>(0u));
    float v42 = min(v41, v18);
    uint v43 = (uint)(int)floor(v42);
    float v44 = max(v34, as_type<float>(0u));
    float v45 = min(v44, v19);
    uint v46 = (uint)(int)floor(v45);
    uint v47 = v46 * v20;
    uint v48 = v43 + v47;
    uint v49 = (uint)((device ushort*)p2)[safe_ix((int)v48,buf_szs[2],2)] & oob_mask((int)v48,buf_szs[2],2);
    uint v50 = (uint)(int)(short)(ushort)v49;
    float v51 = (float)(int)v50;
    float v52 = v51 * as_type<float>(998277249u);
    uint v53 = as_type<float>(v0) <= v39 ? 0xffffffffu : 0u;
    uint v54 = v53 & v40;
    uint v55 = as_type<float>(v0) <= v34 ? 0xffffffffu : 0u;
    uint v56 = v55 & v35;
    uint v57 = v54 & v56;
    uint v58 = select(v0, as_type<uint>(v52), v57 != 0u);
    uint v59 = ((device uint*)(p1 + y * buf_rbs[1]))[x];
    uint v60 = v59 >> 24u;
    float v61 = (float)(int)v60;
    float v62 = v61 * as_type<float>(998277249u);
    float v63 = fma(v62, v23, as_type<float>(v4));
    float v64 = v63 - v62;
    float v65 = fma(as_type<float>(v58), v64, v62);
    uint v66 = v59 >> 8u;
    uint v67 = v66 & 255u;
    float v68 = (float)(int)v67;
    float v69 = v68 * as_type<float>(998277249u);
    float v70 = fma(v69, v23, as_type<float>(v2));
    float v71 = v70 - v69;
    float v72 = fma(as_type<float>(v58), v71, v69);
    uint v73 = v59 >> 16u;
    uint v74 = v73 & 255u;
    float v75 = (float)(int)v74;
    float v76 = v75 * as_type<float>(998277249u);
    float v77 = fma(v76, v23, as_type<float>(v3));
    float v78 = v77 - v76;
    float v79 = fma(as_type<float>(v58), v78, v76);
    uint v80 = v59 & 255u;
    float v81 = (float)(int)v80;
    float v82 = v81 * as_type<float>(998277249u);
    float v83 = fma(v82, v23, as_type<float>(v1));
    float v84 = v83 - v82;
    float v85 = fma(as_type<float>(v58), v84, v82);
    {
        uint ri = uint(int(rint(clamp(v85, 0.0f, 1.0f) * 255.0f)));
        uint gi = uint(int(rint(clamp(v72, 0.0f, 1.0f) * 255.0f)));
        uint bi = uint(int(rint(clamp(v79, 0.0f, 1.0f) * 255.0f)));
        uint ai = uint(int(rint(clamp(v65, 0.0f, 1.0f) * 255.0f)));
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = ri | (gi << 8) | (bi << 16) | (ai << 24);
    }
}
