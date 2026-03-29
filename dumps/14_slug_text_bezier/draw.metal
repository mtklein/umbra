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
    uint v6 = 1065353216u;
    uint v7 = as_type<uint>(as_type<float>(v6) - as_type<float>(v4));
    uint v8 = ((device uint*)(p2 + y * buf_rbs[2]))[x];
    uint v9 = as_type<uint>(fabs(as_type<float>(v8)));
    uint v10 = as_type<uint>(min(as_type<float>(v9), as_type<float>(1065353216u)));
    float4 v11_c;
    switch (buf_fmts[1]) {
      case 1u: { uint px = ((device uint*)(p1 + y * buf_rbs[1]))[x];
                v11_c = float4(px & 0xFFu, (px>>8)&0xFFu, (px>>16)&0xFFu, px>>24) / 255.0; break; }
      case 2u: { ushort px = ((device ushort*)(p1 + y * buf_rbs[1]))[x];
                v11_c = float4(float(px>>11)/31.0, float((px>>5)&0x3Fu)/63.0, float(px&0x1Fu)/31.0, 1.0); break; }
      case 3u: { uint px = ((device uint*)(p1 + y * buf_rbs[1]))[x];
                v11_c = float4(float(px&0x3FFu)/1023.0, float((px>>10)&0x3FFu)/1023.0, float((px>>20)&0x3FFu)/1023.0, float(px>>30)/3.0); break; }
      case 4u: { device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
                v11_c = float4(hp[0], hp[1], hp[2], hp[3]); break; }
      case 5u: { half h = ((device half*)(p1 + y * buf_rbs[1]))[x];
                v11_c = float4(float(h), 0, 0, 1); break; }
      case 6u: { float f = ((device float*)(p1 + y * buf_rbs[1]))[x];
                v11_c = float4(f, 0, 0, 1); break; }
      default: v11_c = float4(0); break;
    }
    uint v11 = as_type<uint>(v11_c.x);
    uint v11_1 = as_type<uint>(v11_c.y);
    uint v11_2 = as_type<uint>(v11_c.z);
    uint v11_3 = as_type<uint>(v11_c.w);
    uint v12 = as_type<uint>(fma(as_type<float>(v11_3), as_type<float>(v7), as_type<float>(v4)));
    uint v13 = as_type<uint>(as_type<float>(v12) - as_type<float>(v11_3));
    uint v14 = as_type<uint>(fma(as_type<float>(v10), as_type<float>(v13), as_type<float>(v11_3)));
    uint v15 = as_type<uint>(fma(as_type<float>(v11), as_type<float>(v7), as_type<float>(v1)));
    uint v16 = as_type<uint>(as_type<float>(v15) - as_type<float>(v11));
    uint v17 = as_type<uint>(fma(as_type<float>(v10), as_type<float>(v16), as_type<float>(v11)));
    uint v18 = as_type<uint>(fma(as_type<float>(v11_1), as_type<float>(v7), as_type<float>(v2)));
    uint v19 = as_type<uint>(as_type<float>(v18) - as_type<float>(v11_1));
    uint v20 = as_type<uint>(fma(as_type<float>(v10), as_type<float>(v19), as_type<float>(v11_1)));
    uint v21 = as_type<uint>(fma(as_type<float>(v11_2), as_type<float>(v7), as_type<float>(v3)));
    uint v22 = as_type<uint>(as_type<float>(v21) - as_type<float>(v11_2));
    uint v23 = as_type<uint>(fma(as_type<float>(v10), as_type<float>(v22), as_type<float>(v11_2)));
    float4 sc24 = float4(as_type<float>(v17), as_type<float>(v20), as_type<float>(v23), as_type<float>(v14));
    switch (buf_fmts[1]) {
      case 1u: { sc24 = clamp(sc24, 0.0, 1.0);
                ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc24.x*255.0)) | (uint(rint(sc24.y*255.0))<<8) | (uint(rint(sc24.z*255.0))<<16) | (uint(rint(sc24.w*255.0))<<24); break; }
      case 2u: { sc24 = clamp(sc24, 0.0, 1.0);
                ((device ushort*)(p1 + y * buf_rbs[1]))[x] = ushort((uint(rint(sc24.x*31.0))<<11) | (uint(rint(sc24.y*63.0))<<5) | uint(rint(sc24.z*31.0))); break; }
      case 3u: { sc24 = clamp(sc24, 0.0, 1.0);
                ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc24.x*1023.0)) | (uint(rint(sc24.y*1023.0))<<10) | (uint(rint(sc24.z*1023.0))<<20) | (uint(rint(sc24.w*3.0))<<30); break; }
      case 4u: { device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
                hp[0]=half(sc24.x); hp[1]=half(sc24.y); hp[2]=half(sc24.z); hp[3]=half(sc24.w); break; }
      case 5u: ((device half*)(p1 + y * buf_rbs[1]))[x] = half(sc24.x); break;
      case 6u: ((device float*)(p1 + y * buf_rbs[1]))[x] = sc24.x; break;
      default: break;
    }
}
