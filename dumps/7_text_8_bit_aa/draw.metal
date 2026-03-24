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
    uint v8 = 255u;
    uint v9 = 1065353216u;
    uint v10 = as_type<uint>(as_type<float>(v9) - as_type<float>(v5));
    uint v11 = 1132396544u;
    uint v12 = pos.y;
    uint v13 = v12 * v1;
    uint v14 = pos.x;
    uint v15 = v14 + v13;
    uint v16 = (uint)((device ushort*)p2)[i];
    uint v17 = (uint)(int)(short)(ushort)v16;
    uint v18 = as_type<uint>((float)(int)v17);
    uint v19 = as_type<uint>(as_type<float>(v7) * as_type<float>(v18));
    uint v20 = ((device uint*)p0)[clamp_ix((int)v15,buf_szs[0],4)];
    uint v21 = v20 >> 24u;
    uint v22 = as_type<uint>((float)(int)v21);
    uint v23 = as_type<uint>(as_type<float>(v7) * as_type<float>(v22));
    uint v24 = as_type<uint>(fma(as_type<float>(v23), as_type<float>(v10), as_type<float>(v5)));
    uint v25 = as_type<uint>(as_type<float>(v24) - as_type<float>(v23));
    uint v26 = as_type<uint>(fma(as_type<float>(v19), as_type<float>(v25), as_type<float>(v23)));
    uint v27 = as_type<uint>(as_type<float>(v26) * as_type<float>(v11));
    uint v28 = as_type<uint>((int)rint(as_type<float>(v27)));
    uint v29 = v20 & v8;
    uint v30 = as_type<uint>((float)(int)v29);
    uint v31 = v20 >> 8u;
    uint v32 = v8 & v31;
    uint v33 = as_type<uint>((float)(int)v32);
    uint v34 = v20 >> 16u;
    uint v35 = v8 & v34;
    uint v36 = as_type<uint>((float)(int)v35);
    uint v37 = as_type<uint>(as_type<float>(v7) * as_type<float>(v30));
    uint v38 = as_type<uint>(fma(as_type<float>(v37), as_type<float>(v10), as_type<float>(v2)));
    uint v39 = as_type<uint>(as_type<float>(v38) - as_type<float>(v37));
    uint v40 = as_type<uint>(fma(as_type<float>(v19), as_type<float>(v39), as_type<float>(v37)));
    uint v41 = as_type<uint>(as_type<float>(v7) * as_type<float>(v33));
    uint v42 = as_type<uint>(fma(as_type<float>(v41), as_type<float>(v10), as_type<float>(v3)));
    uint v43 = as_type<uint>(as_type<float>(v42) - as_type<float>(v41));
    uint v44 = as_type<uint>(fma(as_type<float>(v19), as_type<float>(v43), as_type<float>(v41)));
    uint v45 = as_type<uint>(as_type<float>(v7) * as_type<float>(v36));
    uint v46 = as_type<uint>(fma(as_type<float>(v45), as_type<float>(v10), as_type<float>(v4)));
    uint v47 = as_type<uint>(as_type<float>(v46) - as_type<float>(v45));
    uint v48 = as_type<uint>(fma(as_type<float>(v19), as_type<float>(v47), as_type<float>(v45)));
    uint v49 = as_type<uint>(as_type<float>(v40) * as_type<float>(v11));
    uint v50 = as_type<uint>((int)rint(as_type<float>(v49)));
    uint v51 = as_type<uint>(as_type<float>(v44) * as_type<float>(v11));
    uint v52 = as_type<uint>((int)rint(as_type<float>(v51)));
    uint v53 = as_type<uint>(as_type<float>(v48) * as_type<float>(v11));
    uint v54 = as_type<uint>((int)rint(as_type<float>(v53)));
    uint v55 = v8 & v54;
    uint v56 = v8 & v50;
    uint v57 = v8 & v52;
    uint v58 = v57 << 8u;
    uint v59 = v56 | v58;
    uint v60 = v55 << 16u;
    uint v61 = v59 | v60;
    uint v62 = v28 << 24u;
    uint v63 = v61 | v62;
    ((device uint*)p0)[clamp_ix((int)v15,buf_szs[0],4)] = v63;
}
