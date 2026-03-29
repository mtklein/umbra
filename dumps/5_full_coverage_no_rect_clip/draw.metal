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
    device uchar *p0 [[buffer(0)]],
    device uchar *p1 [[buffer(1)]],
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
    uint v5 = 1065353216u;
    uint v6 = as_type<uint>(as_type<float>(v5) - as_type<float>(v4));
    float4 v7_c;
    switch (buf_fmts[1]) {
      case 1u: { uint px = ((device uint*)(p1 + y * buf_rbs[1]))[x];
                v7_c = float4(px & 0xFFu, (px>>8)&0xFFu, (px>>16)&0xFFu, px>>24) / 255.0; break; }
      case 2u: { ushort px = ((device ushort*)(p1 + y * buf_rbs[1]))[x];
                v7_c = float4(float(px>>11)/31.0, float((px>>5)&0x3Fu)/63.0, float(px&0x1Fu)/31.0, 1.0); break; }
      case 3u: { uint px = ((device uint*)(p1 + y * buf_rbs[1]))[x];
                v7_c = float4(float(px&0x3FFu)/1023.0, float((px>>10)&0x3FFu)/1023.0, float((px>>20)&0x3FFu)/1023.0, float(px>>30)/3.0); break; }
      case 4u: { device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
                v7_c = float4(hp[0], hp[1], hp[2], hp[3]); break; }
      case 7u: { device uchar *row = p1 + y * buf_rbs[1]; uint ps = buf_szs[1]/4;
                v7_c = float4(float(((device half*)row)[x]),float(((device half*)(row+ps))[x]),float(((device half*)(row+2*ps))[x]),float(((device half*)(row+3*ps))[x])); break; }
      case 8u: { uint px = ((device uint*)(p1 + y * buf_rbs[1]))[x];
                v7_c = float4(px & 0xFFu, (px>>8)&0xFFu, (px>>16)&0xFFu, px>>24) / 255.0;
                for (int ch = 0; ch < 3; ch++) {
                  float xv = v7_c[ch];
                  v7_c[ch] = xv >= 0.04045 ? pow((xv+0.055)/1.055, 2.4) : xv/12.92;
                } break; }
      default: v7_c = float4(0); break;
    }
    uint v7 = as_type<uint>(v7_c.x);
    uint v7_1 = as_type<uint>(v7_c.y);
    uint v7_2 = as_type<uint>(v7_c.z);
    uint v7_3 = as_type<uint>(v7_c.w);
    uint v8 = as_type<uint>(fma(as_type<float>(v7_3), as_type<float>(v6), as_type<float>(v4)));
    uint v9 = as_type<uint>(fma(as_type<float>(v7), as_type<float>(v6), as_type<float>(v1)));
    uint v10 = as_type<uint>(fma(as_type<float>(v7_2), as_type<float>(v6), as_type<float>(v3)));
    uint v11 = as_type<uint>(fma(as_type<float>(v7_1), as_type<float>(v6), as_type<float>(v2)));
    float4 sc12 = float4(as_type<float>(v9), as_type<float>(v11), as_type<float>(v10), as_type<float>(v8));
    switch (buf_fmts[1]) {
      case 1u: { sc12 = clamp(sc12, 0.0, 1.0);
                ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc12.x*255.0)) | (uint(rint(sc12.y*255.0))<<8) | (uint(rint(sc12.z*255.0))<<16) | (uint(rint(sc12.w*255.0))<<24); break; }
      case 2u: { sc12 = clamp(sc12, 0.0, 1.0);
                ((device ushort*)(p1 + y * buf_rbs[1]))[x] = ushort((uint(rint(sc12.x*31.0))<<11) | (uint(rint(sc12.y*63.0))<<5) | uint(rint(sc12.z*31.0))); break; }
      case 3u: { sc12 = clamp(sc12, 0.0, 1.0);
                ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc12.x*1023.0)) | (uint(rint(sc12.y*1023.0))<<10) | (uint(rint(sc12.z*1023.0))<<20) | (uint(rint(sc12.w*3.0))<<30); break; }
      case 4u: { device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
                hp[0]=half(sc12.x); hp[1]=half(sc12.y); hp[2]=half(sc12.z); hp[3]=half(sc12.w); break; }
      case 7u: { device uchar *row = p1 + y * buf_rbs[1]; uint ps = buf_szs[1]/4;
                ((device half*)row)[x] = half(sc12.x); ((device half*)(row+ps))[x] = half(sc12.y); ((device half*)(row+2*ps))[x] = half(sc12.z); ((device half*)(row+3*ps))[x] = half(sc12.w); break; }
      case 8u: { for (int ch = 0; ch < 3; ch++) {
                  float xv = sc12[ch];
                  sc12[ch] = xv >= 0.0031308 ? 1.055*pow(xv,1.0/2.4)-0.055 : 12.92*xv;
                } sc12 = clamp(sc12, 0.0, 1.0);
                ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc12.x*255.0)) | (uint(rint(sc12.y*255.0))<<8) | (uint(rint(sc12.z*255.0))<<16) | (uint(rint(sc12.w*255.0))<<24); break; }
      default: break;
    }
}
