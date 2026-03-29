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
    constant uint *buf_fmts [[buffer(8)]],
    constant float *buf_transfers [[buffer(9)]],
    constant uint *buf_plane_strides [[buffer(10)]],
    device uchar *p0 [[buffer(0)]],
    device uchar *p1 [[buffer(1)]],
    device uchar *p2 [[buffer(2)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= w) return;
    uint x = x0 + pos.x;
    uint y = y0 + pos.y;
    uint v0 = 0u;
    uint v2 = ((device const uint*)p0)[0];
    uint v3 = ((device const uint*)p0)[1];
    uint v4 = ((device const uint*)p0)[2];
    uint v5 = 1065353216u;
    uint v6 = ((device const uint*)p0)[3];
    uint v7 = 1073741824u;
    uint v8 = as_type<uint>(as_type<float>(v6) - as_type<float>(1065353216u));
    uint v9 = as_type<uint>(as_type<float>(v6) - as_type<float>(1073741824u));
    uint v10 = 2u;
    uint v11 = 4u;
    uint v12 = 1u;
    uint v13 = 3u;
    uint v14 = x0 + pos.x;
    uint v15 = as_type<uint>((float)(int)v14);
    uint v16 = as_type<uint>(as_type<float>(v15) - as_type<float>(v2));
    uint v17 = y0 + pos.y;
    uint v18 = as_type<uint>((float)(int)v17);
    uint v19 = as_type<uint>(as_type<float>(v18) - as_type<float>(v3));
    uint v20 = as_type<uint>(as_type<float>(v19) * as_type<float>(v19));
    uint v21 = as_type<uint>(fma(as_type<float>(v16), as_type<float>(v16), as_type<float>(v20)));
    uint v22 = as_type<uint>(sqrt(as_type<float>(v21)));
    uint v23 = as_type<uint>(as_type<float>(v4) * as_type<float>(v22));
    uint v24 = as_type<uint>(max(as_type<float>(v23), as_type<float>(0u)));
    uint v25 = as_type<uint>(min(as_type<float>(v24), as_type<float>(1065353216u)));
    uint v26 = as_type<uint>(as_type<float>(v25) * as_type<float>(v8));
    uint v27 = as_type<uint>(floor(as_type<float>(v26)));
    uint v28 = as_type<uint>(min(as_type<float>(v9), as_type<float>(v27)));
    uint v29 = (uint)(int)as_type<float>(v28);
    uint v30 = v29 << 2u;
    uint v31 = v30 + 3u;
    uint v32 = ((device uint*)p2)[safe_ix((int)v31,buf_szs[2],4)] & oob_mask((int)v31,buf_szs[2],4);
    uint v33 = v30 + 4u;
    uint v34 = v33 + 3u;
    uint v35 = ((device uint*)p2)[safe_ix((int)v34,buf_szs[2],4)] & oob_mask((int)v34,buf_szs[2],4);
    uint v36 = as_type<uint>(as_type<float>(v35) - as_type<float>(v32));
    uint v37 = v33 + 1u;
    uint v38 = ((device uint*)p2)[safe_ix((int)v37,buf_szs[2],4)] & oob_mask((int)v37,buf_szs[2],4);
    uint v39 = v33 + 2u;
    uint v40 = ((device uint*)p2)[safe_ix((int)v39,buf_szs[2],4)] & oob_mask((int)v39,buf_szs[2],4);
    uint v41 = as_type<uint>(as_type<float>(v26) - as_type<float>(v28));
    uint v42 = as_type<uint>(fma(as_type<float>(v41), as_type<float>(v36), as_type<float>(v32)));
    uint v43 = v30 + 1u;
    uint v44 = ((device uint*)p2)[safe_ix((int)v43,buf_szs[2],4)] & oob_mask((int)v43,buf_szs[2],4);
    uint v45 = as_type<uint>(as_type<float>(v38) - as_type<float>(v44));
    uint v46 = as_type<uint>(fma(as_type<float>(v41), as_type<float>(v45), as_type<float>(v44)));
    uint v47 = v30 + 2u;
    uint v48 = ((device uint*)p2)[safe_ix((int)v47,buf_szs[2],4)] & oob_mask((int)v47,buf_szs[2],4);
    uint v49 = as_type<uint>(as_type<float>(v40) - as_type<float>(v48));
    uint v50 = as_type<uint>(fma(as_type<float>(v41), as_type<float>(v49), as_type<float>(v48)));
    uint v51 = ((device uint*)p2)[safe_ix((int)v33,buf_szs[2],4)] & oob_mask((int)v33,buf_szs[2],4);
    uint v52 = ((device uint*)p2)[safe_ix((int)v30,buf_szs[2],4)] & oob_mask((int)v30,buf_szs[2],4);
    uint v53 = as_type<uint>(as_type<float>(v51) - as_type<float>(v52));
    uint v54 = as_type<uint>(fma(as_type<float>(v41), as_type<float>(v53), as_type<float>(v52)));
    float4 sc55 = float4(as_type<float>(v54), as_type<float>(v46), as_type<float>(v50), as_type<float>(v42));
    { float tf_a = buf_transfers[1*7+0];
      if (tf_a != 0.0) {
        float tf_b = buf_transfers[1*7+1];
        float tf_c = buf_transfers[1*7+2];
        float tf_d = buf_transfers[1*7+3];
        float tf_f = buf_transfers[1*7+5];
        float tf_g = buf_transfers[1*7+6];
        for (int ch = 0; ch < 3; ch++) {
          float xv = sc55[ch];
          sc55[ch] = xv >= tf_d ? tf_a * pow(xv, 1.0 / tf_g) + tf_b : tf_c * xv + tf_f;
        }
      }
    }
    switch (buf_fmts[1]) {
      case 1u: { sc55 = clamp(sc55, 0.0, 1.0);
                ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc55.x*255.0)) | (uint(rint(sc55.y*255.0))<<8) | (uint(rint(sc55.z*255.0))<<16) | (uint(rint(sc55.w*255.0))<<24); break; }
      case 2u: { sc55 = clamp(sc55, 0.0, 1.0);
                ((device ushort*)(p1 + y * buf_rbs[1]))[x] = ushort((uint(rint(sc55.x*31.0))<<11) | (uint(rint(sc55.y*63.0))<<5) | uint(rint(sc55.z*31.0))); break; }
      case 3u: { sc55 = clamp(sc55, 0.0, 1.0);
                ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc55.x*1023.0)) | (uint(rint(sc55.y*1023.0))<<10) | (uint(rint(sc55.z*1023.0))<<20) | (uint(rint(sc55.w*3.0))<<30); break; }
      case 4u: { device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
                hp[0]=half(sc55.x); hp[1]=half(sc55.y); hp[2]=half(sc55.z); hp[3]=half(sc55.w); break; }
      case 7u: { device uchar *row = p1 + y * buf_rbs[1]; uint ps = buf_plane_strides[1];
                ((device half*)row)[x] = half(sc55.x); ((device half*)(row+ps))[x] = half(sc55.y); ((device half*)(row+2*ps))[x] = half(sc55.z); ((device half*)(row+3*ps))[x] = half(sc55.w); break; }
      default: break;
    }
}
