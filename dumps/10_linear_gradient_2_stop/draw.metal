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
    constant uint &w [[buffer(2)]],
    constant uint *buf_szs [[buffer(3)]],
    constant uint *buf_rbs [[buffer(4)]],
    constant uint &x0 [[buffer(5)]],
    constant uint &y0 [[buffer(6)]],
    constant uint *buf_fmts [[buffer(7)]],
    constant float *buf_transfers [[buffer(8)]],
    device uchar *p0 [[buffer(0)]],
    device uchar *p1 [[buffer(1)]],
    device uchar *p0_g [[buffer(9)]],
    device uchar *p0_b [[buffer(10)]],
    device uchar *p0_a [[buffer(11)]],
    device uchar *p1_g [[buffer(12)]],
    device uchar *p1_b [[buffer(13)]],
    device uchar *p1_a [[buffer(14)]],
    uint2 pos [[thread_position_in_grid]]
) {
    if (pos.x >= w) return;
    uint x = x0 + pos.x;
    uint y = y0 + pos.y;
    uint v0 = 0u;
    uint v1 = ((device const uint*)p0)[0];
    uint v2 = ((device const uint*)p0)[1];
    uint v3 = ((device const uint*)p0)[2];
    uint v4 = 1065353216u;
    uint v5 = ((device const uint*)p0)[3];
    uint v6 = ((device const uint*)p0)[4];
    uint v7 = ((device const uint*)p0)[5];
    uint v8 = ((device const uint*)p0)[6];
    uint v9 = ((device const uint*)p0)[7];
    uint v10 = ((device const uint*)p0)[8];
    uint v11 = ((device const uint*)p0)[9];
    uint v12 = ((device const uint*)p0)[10];
    uint v13 = as_type<uint>(as_type<float>(v9) - as_type<float>(v5));
    uint v14 = as_type<uint>(as_type<float>(v10) - as_type<float>(v6));
    uint v15 = as_type<uint>(as_type<float>(v11) - as_type<float>(v7));
    uint v16 = as_type<uint>(as_type<float>(v12) - as_type<float>(v8));
    uint v17 = x0 + pos.x;
    uint v18 = as_type<uint>((float)(int)v17);
    uint v19 = y0 + pos.y;
    uint v20 = as_type<uint>((float)(int)v19);
    uint v21 = as_type<uint>(as_type<float>(v20) * as_type<float>(v2));
    uint v22 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v1), as_type<float>(v21)));
    uint v23 = as_type<uint>(as_type<float>(v3) + as_type<float>(v22));
    uint v24 = as_type<uint>(max(as_type<float>(v23), as_type<float>(0u)));
    uint v25 = as_type<uint>(min(as_type<float>(v24), as_type<float>(1065353216u)));
    uint v26 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v16), as_type<float>(v8)));
    uint v27 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v13), as_type<float>(v5)));
    uint v28 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v15), as_type<float>(v7)));
    uint v29 = as_type<uint>(fma(as_type<float>(v25), as_type<float>(v14), as_type<float>(v6)));
    float4 sc30 = float4(as_type<float>(v27), as_type<float>(v29), as_type<float>(v28), as_type<float>(v26));
    { float tf_a = buf_transfers[1*7+0];
      if (tf_a != 0.0) {
        float tf_b = buf_transfers[1*7+1];
        float tf_c = buf_transfers[1*7+2];
        float tf_d = buf_transfers[1*7+3];
        float tf_f = buf_transfers[1*7+5];
        float tf_g = buf_transfers[1*7+6];
        for (int ch = 0; ch < 3; ch++) {
          float xv = sc30[ch];
          sc30[ch] = xv >= tf_d ? tf_a * pow(xv, 1.0 / tf_g) + tf_b : tf_c * xv + tf_f;
        }
      }
    }
    switch (buf_fmts[1]) {
      case 1u: { sc30 = clamp(sc30, 0.0, 1.0);
                ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc30.x*255.0)) | (uint(rint(sc30.y*255.0))<<8) | (uint(rint(sc30.z*255.0))<<16) | (uint(rint(sc30.w*255.0))<<24); break; }
      case 2u: { sc30 = clamp(sc30, 0.0, 1.0);
                ((device ushort*)(p1 + y * buf_rbs[1]))[x] = ushort((uint(rint(sc30.x*31.0))<<11) | (uint(rint(sc30.y*63.0))<<5) | uint(rint(sc30.z*31.0))); break; }
      case 3u: { sc30 = clamp(sc30, 0.0, 1.0);
                ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc30.x*1023.0)) | (uint(rint(sc30.y*1023.0))<<10) | (uint(rint(sc30.z*1023.0))<<20) | (uint(rint(sc30.w*3.0))<<30); break; }
      case 4u: { device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
                hp[0]=half(sc30.x); hp[1]=half(sc30.y); hp[2]=half(sc30.z); hp[3]=half(sc30.w); break; }
      case 7u: { ((device half*)(p1 + y * buf_rbs[1]))[x] = half(sc30.x); ((device half*)(p1_g + y * buf_rbs[1]))[x] = half(sc30.y); ((device half*)(p1_b + y * buf_rbs[1]))[x] = half(sc30.z); ((device half*)(p1_a + y * buf_rbs[1]))[x] = half(sc30.w); break; }
      default: break;
    }
}
