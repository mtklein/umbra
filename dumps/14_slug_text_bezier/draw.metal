#include <metal_stdlib>
using namespace metal;

constant uint planes_p1 [[function_constant(0)]];
constant uint fmt_p1 [[function_constant(1)]];

enum { FMT_8888=0, FMT_565=1, FMT_1010102=2, FMT_FP16=3, FMT_FP16_PLANAR=4, FMT_SRGB=5 };

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
    device uchar *p0 [[buffer(0)]],
    device uchar *p1 [[buffer(1)]],
    device uchar *p2 [[buffer(2)]],
    texture2d<float, access::read_write> tex_p1_0 [[texture(0)]],
    texture2d<float, access::read_write> tex_p1_1 [[texture(1)]],
    texture2d<float, access::read_write> tex_p1_2 [[texture(2)]],
    texture2d<float, access::read_write> tex_p1_3 [[texture(3)]],
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
    float v7 = as_type<float>(v6) - as_type<float>(v4);
    uint v8 = ((device uint*)(p2 + y * buf_rbs[2]))[x];
    float v9 = fabs(as_type<float>(v8));
    float v10 = min(v9, as_type<float>(1065353216u));
    float4 v11_c;
    if (planes_p1 == 1) {
        v11_c = tex_p1_0.read(uint2(x,y));
    } else if (planes_p1 == 4) {
        v11_c = float4(tex_p1_0.read(uint2(x,y)).r, tex_p1_1.read(uint2(x,y)).r, tex_p1_2.read(uint2(x,y)).r, tex_p1_3.read(uint2(x,y)).r);
    } else if (fmt_p1 == 0u) {
        v11_c = unpack_unorm4x8_to_float(((device uint*)(p1 + y * buf_rbs[1]))[x]);
    } else if (fmt_p1 == 1u) {
        ushort px = ((device ushort*)(p1 + y * buf_rbs[1]))[x];
        v11_c = float4(float(px>>11)/31.0, float((px>>5)&0x3Fu)/63.0, float(px&0x1Fu)/31.0, 1.0);
    } else if (fmt_p1 == 2u) {
        v11_c = unpack_unorm10a2_to_float(((device uint*)(p1 + y * buf_rbs[1]))[x]);
    } else if (fmt_p1 == 3u) {
        device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
        v11_c = float4(hp[0], hp[1], hp[2], hp[3]);
    } else if (fmt_p1 == 4u) {
        device uchar *row = p1 + y * buf_rbs[1]; uint ps = buf_szs[1]/4;
        v11_c = float4(float(((device half*)row)[x]),float(((device half*)(row+ps))[x]),float(((device half*)(row+2*ps))[x]),float(((device half*)(row+3*ps))[x]));
    } else {
        v11_c = unpack_unorm4x8_to_float(((device uint*)(p1 + y * buf_rbs[1]))[x]);
        for (int ch = 0; ch < 3; ch++) {
            float s = v11_c[ch];
            float a=-4.82083022594e-01,b=1.84310853481e+00,c=-2.79252314568e+00,d=2.05758404732e+00,e=-4.18130934238e-01,f=7.89776027203e-01;
            float inner = ((a*s+b)*s+c)*s+d; float mid = (inner*s+e)*s+f; float s2 = s*s;
            v11_c[ch] = s < 5.76281473041e-02 ? s/12.92 : mid*s2 + (1.0-(a+b+c+d+e+f));
        }
    }
    float v11 = v11_c.x;
    float v11_1 = v11_c.y;
    float v11_2 = v11_c.z;
    float v11_3 = v11_c.w;
    float v12 = fma(v11_3, v7, as_type<float>(v4));
    float v13 = v12 - v11_3;
    float v14 = fma(v10, v13, v11_3);
    float v15 = fma(v11, v7, as_type<float>(v1));
    float v16 = v15 - v11;
    float v17 = fma(v10, v16, v11);
    float v18 = fma(v11_1, v7, as_type<float>(v2));
    float v19 = v18 - v11_1;
    float v20 = fma(v10, v19, v11_1);
    float v21 = fma(v11_2, v7, as_type<float>(v3));
    float v22 = v21 - v11_2;
    float v23 = fma(v10, v22, v11_2);
    float4 sc24 = float4(v17, v20, v23, v14);
    if (planes_p1 == 1) {
        tex_p1_0.write(sc24, uint2(x,y));
    } else if (planes_p1 == 4) {
        tex_p1_0.write(float4(sc24.x,0,0,0), uint2(x,y));
        tex_p1_1.write(float4(sc24.y,0,0,0), uint2(x,y));
        tex_p1_2.write(float4(sc24.z,0,0,0), uint2(x,y));
        tex_p1_3.write(float4(sc24.w,0,0,0), uint2(x,y));
    } else if (fmt_p1 == 0u) {
        sc24 = clamp(sc24, 0.0, 1.0);
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc24.x*255.0)) | (uint(rint(sc24.y*255.0))<<8) | (uint(rint(sc24.z*255.0))<<16) | (uint(rint(sc24.w*255.0))<<24);
    } else if (fmt_p1 == 1u) {
        sc24 = clamp(sc24, 0.0, 1.0);
        ((device ushort*)(p1 + y * buf_rbs[1]))[x] = ushort((uint(rint(sc24.x*31.0))<<11) | (uint(rint(sc24.y*63.0))<<5) | uint(rint(sc24.z*31.0)));
    } else if (fmt_p1 == 2u) {
        sc24 = clamp(sc24, 0.0, 1.0);
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc24.x*1023.0)) | (uint(rint(sc24.y*1023.0))<<10) | (uint(rint(sc24.z*1023.0))<<20) | (uint(rint(sc24.w*3.0))<<30);
    } else if (fmt_p1 == 3u) {
        device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
        hp[0]=half(sc24.x); hp[1]=half(sc24.y); hp[2]=half(sc24.z); hp[3]=half(sc24.w);
    } else if (fmt_p1 == 4u) {
        device uchar *row = p1 + y * buf_rbs[1]; uint ps = buf_szs[1]/4;
        ((device half*)row)[x] = half(sc24.x); ((device half*)(row+ps))[x] = half(sc24.y); ((device half*)(row+2*ps))[x] = half(sc24.z); ((device half*)(row+3*ps))[x] = half(sc24.w);
    } else {
        for (int ch = 0; ch < 3; ch++) {
            float l = max(sc24[ch], 0.0);
            float t = 1.0/sqrt(max(l, 1e-30));
            float lo = l * 12.92;
            float hi = (1.0545324087e+00 + t*(5.8207426220e-02 + t*(-1.2198361568e-02 + t*(7.9244317021e-04 + t*-2.0467568902e-05)))) / (1.0131348670e-01 + t);
            sc24[ch] = lo < 4.5700869523e-03*12.92 ? lo : hi;
        } sc24 = clamp(sc24, 0.0, 1.0);
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc24.x*255.0)) | (uint(rint(sc24.y*255.0))<<8) | (uint(rint(sc24.z*255.0))<<16) | (uint(rint(sc24.w*255.0))<<24);
    }
}
