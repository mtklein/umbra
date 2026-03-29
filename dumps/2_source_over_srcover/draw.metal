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
    uint v5 = ((device const uint*)p0)[4];
    uint v6 = ((device const uint*)p0)[5];
    uint v7 = ((device const uint*)p0)[6];
    uint v8 = ((device const uint*)p0)[7];
    uint v9 = 1065353216u;
    uint v10 = as_type<uint>(as_type<float>(v9) - as_type<float>(v4));
    uint v11 = x0 + pos.x;
    uint v12 = as_type<uint>((float)(int)v11);
    uint v13 = as_type<float>(v12) <  as_type<float>(v7) ? 0xffffffffu : 0u;
    uint v14 = as_type<float>(v5) <= as_type<float>(v12) ? 0xffffffffu : 0u;
    uint v15 = v14 & v13;
    uint v16 = y0 + pos.y;
    uint v17 = as_type<uint>((float)(int)v16);
    uint v18 = as_type<float>(v17) <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v19 = as_type<float>(v6) <= as_type<float>(v17) ? 0xffffffffu : 0u;
    uint v20 = v19 & v18;
    uint v21 = v15 & v20;
    uint v22 = (v21 & v9) | (~v21 & v0);
    float4 v23_c;
    switch (buf_fmts[1]) {
      case 1u: { uint px = ((device uint*)(p1 + y * buf_rbs[1]))[x];
                v23_c = float4(px & 0xFFu, (px>>8)&0xFFu, (px>>16)&0xFFu, px>>24) / 255.0; break; }
      case 2u: { ushort px = ((device ushort*)(p1 + y * buf_rbs[1]))[x];
                v23_c = float4(float(px>>11)/31.0, float((px>>5)&0x3Fu)/63.0, float(px&0x1Fu)/31.0, 1.0); break; }
      case 3u: { uint px = ((device uint*)(p1 + y * buf_rbs[1]))[x];
                v23_c = float4(float(px&0x3FFu)/1023.0, float((px>>10)&0x3FFu)/1023.0, float((px>>20)&0x3FFu)/1023.0, float(px>>30)/3.0); break; }
      case 4u: { device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
                v23_c = float4(hp[0], hp[1], hp[2], hp[3]); break; }
      case 7u: { device uchar *row = p1 + y * buf_rbs[1]; uint ps = buf_szs[1]/4;
                v23_c = float4(float(((device half*)row)[x]),float(((device half*)(row+ps))[x]),float(((device half*)(row+2*ps))[x]),float(((device half*)(row+3*ps))[x])); break; }
      case 8u: { uint px = ((device uint*)(p1 + y * buf_rbs[1]))[x];
                v23_c = float4(px & 0xFFu, (px>>8)&0xFFu, (px>>16)&0xFFu, px>>24) / 255.0;
                for (int ch = 0; ch < 3; ch++) {
                  float xv = v23_c[ch];
                  v23_c[ch] = xv >= 0.04045 ? pow((xv+0.055)/1.055, 2.4) : xv/12.92;
                } break; }
      default: v23_c = float4(0); break;
    }
    uint v23 = as_type<uint>(v23_c.x);
    uint v23_1 = as_type<uint>(v23_c.y);
    uint v23_2 = as_type<uint>(v23_c.z);
    uint v23_3 = as_type<uint>(v23_c.w);
    uint v24 = as_type<uint>(fma(as_type<float>(v23_3), as_type<float>(v10), as_type<float>(v4)));
    uint v25 = as_type<uint>(as_type<float>(v24) - as_type<float>(v23_3));
    uint v26 = as_type<uint>(fma(as_type<float>(v22), as_type<float>(v25), as_type<float>(v23_3)));
    uint v27 = as_type<uint>(fma(as_type<float>(v23), as_type<float>(v10), as_type<float>(v1)));
    uint v28 = as_type<uint>(as_type<float>(v27) - as_type<float>(v23));
    uint v29 = as_type<uint>(fma(as_type<float>(v22), as_type<float>(v28), as_type<float>(v23)));
    uint v30 = as_type<uint>(fma(as_type<float>(v23_1), as_type<float>(v10), as_type<float>(v2)));
    uint v31 = as_type<uint>(as_type<float>(v30) - as_type<float>(v23_1));
    uint v32 = as_type<uint>(fma(as_type<float>(v22), as_type<float>(v31), as_type<float>(v23_1)));
    uint v33 = as_type<uint>(fma(as_type<float>(v23_2), as_type<float>(v10), as_type<float>(v3)));
    uint v34 = as_type<uint>(as_type<float>(v33) - as_type<float>(v23_2));
    uint v35 = as_type<uint>(fma(as_type<float>(v22), as_type<float>(v34), as_type<float>(v23_2)));
    float4 sc36 = float4(as_type<float>(v29), as_type<float>(v32), as_type<float>(v35), as_type<float>(v26));
    switch (buf_fmts[1]) {
      case 1u: { sc36 = clamp(sc36, 0.0, 1.0);
                ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc36.x*255.0)) | (uint(rint(sc36.y*255.0))<<8) | (uint(rint(sc36.z*255.0))<<16) | (uint(rint(sc36.w*255.0))<<24); break; }
      case 2u: { sc36 = clamp(sc36, 0.0, 1.0);
                ((device ushort*)(p1 + y * buf_rbs[1]))[x] = ushort((uint(rint(sc36.x*31.0))<<11) | (uint(rint(sc36.y*63.0))<<5) | uint(rint(sc36.z*31.0))); break; }
      case 3u: { sc36 = clamp(sc36, 0.0, 1.0);
                ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc36.x*1023.0)) | (uint(rint(sc36.y*1023.0))<<10) | (uint(rint(sc36.z*1023.0))<<20) | (uint(rint(sc36.w*3.0))<<30); break; }
      case 4u: { device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
                hp[0]=half(sc36.x); hp[1]=half(sc36.y); hp[2]=half(sc36.z); hp[3]=half(sc36.w); break; }
      case 7u: { device uchar *row = p1 + y * buf_rbs[1]; uint ps = buf_szs[1]/4;
                ((device half*)row)[x] = half(sc36.x); ((device half*)(row+ps))[x] = half(sc36.y); ((device half*)(row+2*ps))[x] = half(sc36.z); ((device half*)(row+3*ps))[x] = half(sc36.w); break; }
      case 8u: { for (int ch = 0; ch < 3; ch++) {
                  float xv = sc36[ch];
                  sc36[ch] = xv >= 0.0031308 ? 1.055*pow(xv,1.0/2.4)-0.055 : 12.92*xv;
                } sc36 = clamp(sc36, 0.0, 1.0);
                ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc36.x*255.0)) | (uint(rint(sc36.y*255.0))<<8) | (uint(rint(sc36.z*255.0))<<16) | (uint(rint(sc36.w*255.0))<<24); break; }
      default: break;
    }
}
