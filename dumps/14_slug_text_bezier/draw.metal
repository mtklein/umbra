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
    uint v7 = 1065353216u;
    uint v8 = 255u;
    uint v9 = 998277249u;
    uint v10 = as_type<uint>(as_type<float>(v7) - as_type<float>(v5));
    uint v11 = 1132396544u;
    uint v12 = pos.y;
    uint v13 = v12 * v1;
    uint v14 = pos.x;
    uint v15 = v14 + v13;
    uint v16 = ((device uint*)p2)[i];
    uint v17 = as_type<uint>(fabs(as_type<float>(v16)));
    uint v18 = as_type<uint>(min(as_type<float>(v17), as_type<float>(v7)));
    uint v19 = ((device uint*)p0)[clamp_ix((int)v15,buf_szs[0],4)];
    uint v20 = v19 >> 24u;
    uint v21 = as_type<uint>((float)(int)v20);
    uint v22 = as_type<uint>(as_type<float>(v9) * as_type<float>(v21));
    uint v23 = as_type<uint>(fma(as_type<float>(v22), as_type<float>(v10), as_type<float>(v5)));
    uint v24 = as_type<uint>(as_type<float>(v23) - as_type<float>(v22));
    uint v25 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v24), as_type<float>(v22)));
    uint v26 = as_type<uint>(as_type<float>(v25) * as_type<float>(v11));
    uint v27 = as_type<uint>((int)rint(as_type<float>(v26)));
    uint v28 = v19 & v8;
    uint v29 = as_type<uint>((float)(int)v28);
    uint v30 = v19 >> 8u;
    uint v31 = v8 & v30;
    uint v32 = as_type<uint>((float)(int)v31);
    uint v33 = v19 >> 16u;
    uint v34 = v8 & v33;
    uint v35 = as_type<uint>((float)(int)v34);
    uint v36 = as_type<uint>(as_type<float>(v9) * as_type<float>(v29));
    uint v37 = as_type<uint>(fma(as_type<float>(v36), as_type<float>(v10), as_type<float>(v2)));
    uint v38 = as_type<uint>(as_type<float>(v37) - as_type<float>(v36));
    uint v39 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v38), as_type<float>(v36)));
    uint v40 = as_type<uint>(as_type<float>(v9) * as_type<float>(v32));
    uint v41 = as_type<uint>(fma(as_type<float>(v40), as_type<float>(v10), as_type<float>(v3)));
    uint v42 = as_type<uint>(as_type<float>(v41) - as_type<float>(v40));
    uint v43 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v42), as_type<float>(v40)));
    uint v44 = as_type<uint>(as_type<float>(v9) * as_type<float>(v35));
    uint v45 = as_type<uint>(fma(as_type<float>(v44), as_type<float>(v10), as_type<float>(v4)));
    uint v46 = as_type<uint>(as_type<float>(v45) - as_type<float>(v44));
    uint v47 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v46), as_type<float>(v44)));
    uint v48 = as_type<uint>(as_type<float>(v39) * as_type<float>(v11));
    uint v49 = as_type<uint>((int)rint(as_type<float>(v48)));
    uint v50 = as_type<uint>(as_type<float>(v43) * as_type<float>(v11));
    uint v51 = as_type<uint>((int)rint(as_type<float>(v50)));
    uint v52 = as_type<uint>(as_type<float>(v47) * as_type<float>(v11));
    uint v53 = as_type<uint>((int)rint(as_type<float>(v52)));
    uint v54 = v8 & v53;
    uint v55 = v8 & v49;
    uint v56 = v8 & v51;
    uint v57 = v56 << 8u;
    uint v58 = v55 | v57;
    uint v59 = v54 << 16u;
    uint v60 = v58 | v59;
    uint v61 = v27 << 24u;
    uint v62 = v60 | v61;
    ((device uint*)p0)[clamp_ix((int)v15,buf_szs[0],4)] = v62;
}
