#include <metal_stdlib>
using namespace metal;

static inline int clamp_ix(int ix, uint bytes, int elem) {
    int hi = (int)(bytes / (uint)elem) - 1;
    if (hi < 0) hi = 0;
    return clamp(ix, 0, hi);
}

kernel void umbra_entry(
    constant uint &n [[buffer(0)]],
    constant uint &w [[buffer(5)]],
    device uchar *p0 [[buffer(1)]],
    device uchar *p1 [[buffer(2)]],
    device uchar *p2 [[buffer(3)]],
    constant uint *buf_szs [[buffer(4)]],
    uint2 pos [[thread_position_in_grid]]
) {
    uint i = pos.y * w + pos.x;
    if (i >= n) return;
    uint v0 = 0u;
    uint v1 = ((device const uint*)p1)[0];
    uint v2 = ((device const uint*)p1)[2];
    uint v3 = ((device const uint*)p1)[3];
    uint v4 = ((device const uint*)p1)[4];
    uint v5 = ((device const uint*)p1)[5];
    uint v7 = 998277249u;
    uint v8 = 1054867456u;
    uint v9 = 1090519040u;
    uint v10 = 1065353216u;
    uint v11 = 255u;
    uint v12 = as_type<uint>(as_type<float>(v10) - as_type<float>(v5));
    uint v13 = 1132396544u;
    uint v14 = pos.y;
    uint v15 = v14 * v1;
    uint v16 = pos.x;
    uint v17 = v16 + v15;
    uint v18 = (uint)((device ushort*)p2)[i];
    uint v19 = (uint)(int)(short)(ushort)v18;
    uint v20 = as_type<uint>((float)(int)v19);
    uint v21 = as_type<uint>(as_type<float>(v7) * as_type<float>(v20));
    uint v22 = as_type<uint>(as_type<float>(v21) - as_type<float>(v8));
    uint v23 = as_type<uint>(as_type<float>(v9) * as_type<float>(v22));
    uint v24 = as_type<uint>(max(as_type<float>(v0), as_type<float>(v23)));
    uint v25 = as_type<uint>(min(as_type<float>(v10), as_type<float>(v24)));
    uint v26 = ((device uint*)p0)[clamp_ix((int)v17,buf_szs[0],4)];
    uint v27 = v26 >> 24u;
    uint v28 = as_type<uint>((float)(int)v27);
    uint v29 = as_type<uint>(as_type<float>(v7) * as_type<float>(v28));
    uint v30 = as_type<uint>(fma(as_type<float>(v29), as_type<float>(v12), as_type<float>(v5)));
    uint v31 = as_type<uint>(as_type<float>(v30) - as_type<float>(v29));
    uint v32 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v31), as_type<float>(v29)));
    uint v33 = as_type<uint>(as_type<float>(v32) * as_type<float>(v13));
    uint v34 = as_type<uint>((int)rint(as_type<float>(v33)));
    uint v35 = v26 & v11;
    uint v36 = as_type<uint>((float)(int)v35);
    uint v37 = v26 >> 8u;
    uint v38 = v11 & v37;
    uint v39 = as_type<uint>((float)(int)v38);
    uint v40 = v26 >> 16u;
    uint v41 = v11 & v40;
    uint v42 = as_type<uint>((float)(int)v41);
    uint v43 = as_type<uint>(as_type<float>(v7) * as_type<float>(v36));
    uint v44 = as_type<uint>(fma(as_type<float>(v43), as_type<float>(v12), as_type<float>(v2)));
    uint v45 = as_type<uint>(as_type<float>(v44) - as_type<float>(v43));
    uint v46 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v45), as_type<float>(v43)));
    uint v47 = as_type<uint>(as_type<float>(v7) * as_type<float>(v39));
    uint v48 = as_type<uint>(fma(as_type<float>(v47), as_type<float>(v12), as_type<float>(v3)));
    uint v49 = as_type<uint>(as_type<float>(v48) - as_type<float>(v47));
    uint v50 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v49), as_type<float>(v47)));
    uint v51 = as_type<uint>(as_type<float>(v7) * as_type<float>(v42));
    uint v52 = as_type<uint>(fma(as_type<float>(v51), as_type<float>(v12), as_type<float>(v4)));
    uint v53 = as_type<uint>(as_type<float>(v52) - as_type<float>(v51));
    uint v54 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v53), as_type<float>(v51)));
    uint v55 = as_type<uint>(as_type<float>(v46) * as_type<float>(v13));
    uint v56 = as_type<uint>((int)rint(as_type<float>(v55)));
    uint v57 = as_type<uint>(as_type<float>(v50) * as_type<float>(v13));
    uint v58 = as_type<uint>((int)rint(as_type<float>(v57)));
    uint v59 = as_type<uint>(as_type<float>(v54) * as_type<float>(v13));
    uint v60 = as_type<uint>((int)rint(as_type<float>(v59)));
    uint v61 = v11 & v60;
    uint v62 = v11 & v56;
    uint v63 = v11 & v58;
    uint v64 = v63 << 8u;
    uint v65 = v62 | v64;
    uint v66 = v61 << 16u;
    uint v67 = v65 | v66;
    uint v68 = v34 << 24u;
    uint v69 = v67 | v68;
    ((device uint*)p0)[clamp_ix((int)v17,buf_szs[0],4)] = v69;
}
