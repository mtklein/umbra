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
    uint v4 = ((device const uint*)p0)[3];
    uint v5 = ((device const uint*)p0)[4];
    uint v6 = ((device const uint*)p0)[5];
    uint v7 = ((device const uint*)p0)[6];
    uint v8 = ((device const uint*)p0)[7];
    uint v9 = 1065353216u;
    uint v10 = x0 + pos.x;
    uint v11 = as_type<uint>((float)(int)v10);
    uint v12 = as_type<float>(v11) <  as_type<float>(v7) ? 0xffffffffu : 0u;
    uint v13 = as_type<float>(v5) <= as_type<float>(v11) ? 0xffffffffu : 0u;
    uint v14 = v13 & v12;
    uint v15 = y0 + pos.y;
    uint v16 = as_type<uint>((float)(int)v15);
    uint v17 = as_type<float>(v16) <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v18 = as_type<float>(v6) <= as_type<float>(v16) ? 0xffffffffu : 0u;
    uint v19 = v18 & v17;
    uint v20 = v14 & v19;
    uint v21 = (v20 & v9) | (~v20 & v0);
    float4 v22_c;
    switch (buf_fmts[1]) {
      case 1u: { uint px = ((device uint*)(p1 + y * buf_rbs[1]))[x];
                v22_c = float4(px & 0xFFu, (px>>8)&0xFFu, (px>>16)&0xFFu, px>>24) / 255.0; break; }
      case 2u: { ushort px = ((device ushort*)(p1 + y * buf_rbs[1]))[x];
                v22_c = float4(float(px>>11)/31.0, float((px>>5)&0x3Fu)/63.0, float(px&0x1Fu)/31.0, 1.0); break; }
      case 3u: { uint px = ((device uint*)(p1 + y * buf_rbs[1]))[x];
                v22_c = float4(float(px&0x3FFu)/1023.0, float((px>>10)&0x3FFu)/1023.0, float((px>>20)&0x3FFu)/1023.0, float(px>>30)/3.0); break; }
      case 4u: { device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
                v22_c = float4(hp[0], hp[1], hp[2], hp[3]); break; }
      case 7u: { v22_c = float4(float(((device half*)(p1 + y * buf_rbs[1]))[x]),float(((device half*)(p1_g + y * buf_rbs[1]))[x]),float(((device half*)(p1_b + y * buf_rbs[1]))[x]),float(((device half*)(p1_a + y * buf_rbs[1]))[x])); break; }
      default: v22_c = float4(0); break;
    }
    { float tf_a = buf_transfers[1*7+0];
      if (tf_a != 0.0) {
        float tf_b = buf_transfers[1*7+1];
        float tf_c = buf_transfers[1*7+2];
        float tf_e = buf_transfers[1*7+4];
        float tf_f = buf_transfers[1*7+5];
        float tf_g = buf_transfers[1*7+6];
        for (int ch = 0; ch < 3; ch++) {
          float xv = v22_c[ch];
          v22_c[ch] = xv >= tf_e ? pow((xv - tf_b) / tf_a, tf_g) : (xv - tf_f) / tf_c;
        }
      }
    }
    uint v22 = as_type<uint>(v22_c.x);
    uint v22_1 = as_type<uint>(v22_c.y);
    uint v22_2 = as_type<uint>(v22_c.z);
    uint v22_3 = as_type<uint>(v22_c.w);
    uint v23 = as_type<uint>(as_type<float>(v1) - as_type<float>(v22));
    uint v24 = as_type<uint>(fma(as_type<float>(v21), as_type<float>(v23), as_type<float>(v22)));
    uint v25 = as_type<uint>(as_type<float>(v2) - as_type<float>(v22_1));
    uint v26 = as_type<uint>(fma(as_type<float>(v21), as_type<float>(v25), as_type<float>(v22_1)));
    uint v27 = as_type<uint>(as_type<float>(v3) - as_type<float>(v22_2));
    uint v28 = as_type<uint>(fma(as_type<float>(v21), as_type<float>(v27), as_type<float>(v22_2)));
    uint v29 = as_type<uint>(as_type<float>(v4) - as_type<float>(v22_3));
    uint v30 = as_type<uint>(fma(as_type<float>(v21), as_type<float>(v29), as_type<float>(v22_3)));
    float4 sc31 = float4(as_type<float>(v24), as_type<float>(v26), as_type<float>(v28), as_type<float>(v30));
    { float tf_a = buf_transfers[1*7+0];
      if (tf_a != 0.0) {
        float tf_b = buf_transfers[1*7+1];
        float tf_c = buf_transfers[1*7+2];
        float tf_d = buf_transfers[1*7+3];
        float tf_f = buf_transfers[1*7+5];
        float tf_g = buf_transfers[1*7+6];
        for (int ch = 0; ch < 3; ch++) {
          float xv = sc31[ch];
          sc31[ch] = xv >= tf_d ? tf_a * pow(xv, 1.0 / tf_g) + tf_b : tf_c * xv + tf_f;
        }
      }
    }
    switch (buf_fmts[1]) {
      case 1u: { sc31 = clamp(sc31, 0.0, 1.0);
                ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc31.x*255.0)) | (uint(rint(sc31.y*255.0))<<8) | (uint(rint(sc31.z*255.0))<<16) | (uint(rint(sc31.w*255.0))<<24); break; }
      case 2u: { sc31 = clamp(sc31, 0.0, 1.0);
                ((device ushort*)(p1 + y * buf_rbs[1]))[x] = ushort((uint(rint(sc31.x*31.0))<<11) | (uint(rint(sc31.y*63.0))<<5) | uint(rint(sc31.z*31.0))); break; }
      case 3u: { sc31 = clamp(sc31, 0.0, 1.0);
                ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc31.x*1023.0)) | (uint(rint(sc31.y*1023.0))<<10) | (uint(rint(sc31.z*1023.0))<<20) | (uint(rint(sc31.w*3.0))<<30); break; }
      case 4u: { device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
                hp[0]=half(sc31.x); hp[1]=half(sc31.y); hp[2]=half(sc31.z); hp[3]=half(sc31.w); break; }
      case 7u: { ((device half*)(p1 + y * buf_rbs[1]))[x] = half(sc31.x); ((device half*)(p1_g + y * buf_rbs[1]))[x] = half(sc31.y); ((device half*)(p1_b + y * buf_rbs[1]))[x] = half(sc31.z); ((device half*)(p1_a + y * buf_rbs[1]))[x] = half(sc31.w); break; }
      default: break;
    }
}
