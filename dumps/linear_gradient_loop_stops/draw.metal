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
    constant uint &w [[buffer(4)]],
    constant uint *buf_szs [[buffer(5)]],
    constant uint *buf_rbs [[buffer(6)]],
    constant uint &x0 [[buffer(7)]],
    constant uint &y0 [[buffer(8)]],
    device uchar *p0 [[buffer(0)]],
    device uchar *p1 [[buffer(1)]],
    device uchar *p2 [[buffer(2)]],
    device uchar *p3 [[buffer(3)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= w) return;
    uint x = x0 + pos.x;
    uint y = y0 + pos.y;
    uint var0 = 0;
    uint var1 = 0;
    uint var2 = 0;
    uint var3 = 0;
    uint var4 = 0;
    uint v0 = 0u;
    uint v3 = ((device const uint*)p0)[0];
    uint v4 = ((device const uint*)p0)[1];
    uint v5 = ((device const uint*)p0)[2];
    uint v6 = 1065353216u;
    uint v7 = ((device const uint*)p0)[3];
    uint v8 = (uint)(int)as_type<float>(v7);
    uint v9 = 1u;
    uint v10 = v8 - 1u;
    uint v11 = v8 + v8;
    uint v12 = v8 + v11;
    uint v13 = 1132396544u;
    uint v14 = x0 + pos.x;
    float v15 = (float)(int)v14;
    uint v16 = y0 + pos.y;
    float v17 = (float)(int)v16;
    float v18 = v17 * as_type<float>(v4);
    float v19 = fma(v15, as_type<float>(v3), v18);
    float v20 = as_type<float>(v5) + v19;
    float v21 = max(v20, as_type<float>(0u));
    float v22 = min(v21, as_type<float>(1065353216u));
    while (var4 < v10) {
    uint v24 = var4;
    uint v25 = v24 + 1u;
    uint v26 = ((device uint*)p3)[safe_ix((int)v25,buf_szs[3],4)] & oob_mask((int)v25,buf_szs[3],4);
    uint v27 = v22 <= as_type<float>(v26) ? 0xffffffffu : 0u;
    uint v28 = v12 + v25;
    uint v29 = ((device uint*)p2)[safe_ix((int)v28,buf_szs[2],4)] & oob_mask((int)v28,buf_szs[2],4);
    uint v30 = v8 + v25;
    uint v31 = ((device uint*)p2)[safe_ix((int)v30,buf_szs[2],4)] & oob_mask((int)v30,buf_szs[2],4);
    uint v32 = v11 + v25;
    uint v33 = ((device uint*)p2)[safe_ix((int)v32,buf_szs[2],4)] & oob_mask((int)v32,buf_szs[2],4);
    uint v34 = v12 + v24;
    uint v35 = ((device uint*)p2)[safe_ix((int)v34,buf_szs[2],4)] & oob_mask((int)v34,buf_szs[2],4);
    float v36 = as_type<float>(v29) - as_type<float>(v35);
    uint v37 = ((device uint*)p3)[safe_ix((int)v24,buf_szs[3],4)] & oob_mask((int)v24,buf_szs[3],4);
    uint v38 = as_type<float>(v37) <= v22 ? 0xffffffffu : 0u;
    uint v39 = v38 & v27;
    float v40 = as_type<float>(v26) - as_type<float>(v37);
    float v41 = v22 - as_type<float>(v37);
    float v42 = v41 / v40;
    float v43 = fma(v42, v36, as_type<float>(v35));
    uint v44 = v8 + v24;
    uint v45 = ((device uint*)p2)[safe_ix((int)v44,buf_szs[2],4)] & oob_mask((int)v44,buf_szs[2],4);
    float v46 = as_type<float>(v31) - as_type<float>(v45);
    float v47 = fma(v42, v46, as_type<float>(v45));
    uint v48 = v11 + v24;
    uint v49 = ((device uint*)p2)[safe_ix((int)v48,buf_szs[2],4)] & oob_mask((int)v48,buf_szs[2],4);
    float v50 = as_type<float>(v33) - as_type<float>(v49);
    float v51 = fma(v42, v50, as_type<float>(v49));
    uint v52 = ((device uint*)p2)[safe_ix((int)v25,buf_szs[2],4)] & oob_mask((int)v25,buf_szs[2],4);
    uint v53 = ((device uint*)p2)[safe_ix((int)v24,buf_szs[2],4)] & oob_mask((int)v24,buf_szs[2],4);
    float v54 = as_type<float>(v52) - as_type<float>(v53);
    float v55 = fma(v42, v54, as_type<float>(v53));
    uint v56 = var0;
    uint v57 = select(v56, as_type<uint>(v55), v39 != 0u);
    var0 = v57;

    uint v59 = var1;
    uint v60 = select(v59, as_type<uint>(v47), v39 != 0u);
    var1 = v60;

    uint v62 = var2;
    uint v63 = select(v62, as_type<uint>(v51), v39 != 0u);
    var2 = v63;

    uint v65 = var3;
    uint v66 = select(v65, as_type<uint>(v43), v39 != 0u);
    var3 = v66;

    uint v68 = var4;
    uint v69 = v68 + 1u;
    var4 = v69;

    }
    uint v72 = var0;
    float v73 = max(as_type<float>(v72), as_type<float>(0u));
    float v74 = min(v73, as_type<float>(1065353216u));
    float v75 = v74 * as_type<float>(1132396544u);
    uint v76 = (uint)(int)rint(v75);
    uint v77 = var1;
    float v78 = max(as_type<float>(v77), as_type<float>(0u));
    float v79 = min(v78, as_type<float>(1065353216u));
    float v80 = v79 * as_type<float>(1132396544u);
    uint v81 = (uint)(int)rint(v80);
    uint v82 = var2;
    float v83 = max(as_type<float>(v82), as_type<float>(0u));
    float v84 = min(v83, as_type<float>(1065353216u));
    float v85 = v84 * as_type<float>(1132396544u);
    uint v86 = (uint)(int)rint(v85);
    uint v87 = var3;
    float v88 = max(as_type<float>(v87), as_type<float>(0u));
    float v89 = min(v88, as_type<float>(1065353216u));
    float v90 = v89 * as_type<float>(1132396544u);
    uint v91 = (uint)(int)rint(v90);
    ((device uint*)(p1 + y * buf_rbs[1]))[x] = (v76 & 0xFFu) | ((v81 & 0xFFu) << 8u) | ((v86 & 0xFFu) << 16u) | (v91 << 24u);
}
