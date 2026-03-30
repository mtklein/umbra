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
    uint v6 = 998277249u;
    uint v7 = 1054867456u;
    uint v8 = 1090519040u;
    uint v9 = 1065353216u;
    float v10 = as_type<float>(v9) - as_type<float>(v4);
    uint v11 = (uint)((device ushort*)(p2 + y * buf_rbs[2]))[x];
    uint v12 = (uint)(int)(short)(ushort)v11;
    float v13 = (float)(int)v12;
    float v14 = v13 * as_type<float>(998277249u);
    float v15 = v14 - as_type<float>(1054867456u);
    float v16 = v15 * as_type<float>(1090519040u);
    float v17 = max(v16, as_type<float>(0u));
    float v18 = min(v17, as_type<float>(1065353216u));
    float4 v19_c;
    if (planes_p1 == 1) {
        v19_c = tex_p1_0.read(uint2(x,y));
    } else if (planes_p1 == 4) {
        v19_c = float4(tex_p1_0.read(uint2(x,y)).r, tex_p1_1.read(uint2(x,y)).r, tex_p1_2.read(uint2(x,y)).r, tex_p1_3.read(uint2(x,y)).r);
    } else if (fmt_p1 == 0u) {
        v19_c = unpack_unorm4x8_to_float(((device uint*)(p1 + y * buf_rbs[1]))[x]);
    } else if (fmt_p1 == 1u) {
        float3 bgr = unpack_unorm565_to_float(((device ushort*)(p1 + y * buf_rbs[1]))[x]);
        v19_c = float4(bgr.z, bgr.y, bgr.x, 1.0);
    } else if (fmt_p1 == 2u) {
        v19_c = unpack_unorm10a2_to_float(((device uint*)(p1 + y * buf_rbs[1]))[x]);
    } else if (fmt_p1 == 3u) {
        device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
        v19_c = float4(hp[0], hp[1], hp[2], hp[3]);
    } else if (fmt_p1 == 4u) {
        device uchar *row = p1 + y * buf_rbs[1]; uint ps = buf_szs[1]/4;
        v19_c = float4(float(((device half*)row)[x]),float(((device half*)(row+ps))[x]),float(((device half*)(row+2*ps))[x]),float(((device half*)(row+3*ps))[x]));
    } else {
        v19_c = unpack_unorm4x8_to_float(((device uint*)(p1 + y * buf_rbs[1]))[x]);
        for (int ch = 0; ch < 3; ch++) {
            float s = v19_c[ch];
            float a=-4.82083022594e-01,b=1.84310853481e+00,c=-2.79252314568e+00,d=2.05758404732e+00,e=-4.18130934238e-01,f=7.89776027203e-01;
            float inner = ((a*s+b)*s+c)*s+d; float mid = (inner*s+e)*s+f; float s2 = s*s;
            v19_c[ch] = s < 5.76281473041e-02 ? s/12.92 : mid*s2 + (1.0-(a+b+c+d+e+f));
        }
    }
    float v19 = v19_c.x;
    float v19_1 = v19_c.y;
    float v19_2 = v19_c.z;
    float v19_3 = v19_c.w;
    float v20 = fma(v19_3, v10, as_type<float>(v4));
    float v21 = v20 - v19_3;
    float v22 = fma(v18, v21, v19_3);
    float v23 = fma(v19, v10, as_type<float>(v1));
    float v24 = v23 - v19;
    float v25 = fma(v18, v24, v19);
    float v26 = fma(v19_1, v10, as_type<float>(v2));
    float v27 = v26 - v19_1;
    float v28 = fma(v18, v27, v19_1);
    float v29 = fma(v19_2, v10, as_type<float>(v3));
    float v30 = v29 - v19_2;
    float v31 = fma(v18, v30, v19_2);
    float4 sc32 = float4(v25, v28, v31, v22);
    if (planes_p1 == 1) {
        tex_p1_0.write(sc32, uint2(x,y));
    } else if (planes_p1 == 4) {
        tex_p1_0.write(float4(sc32.x,0,0,0), uint2(x,y));
        tex_p1_1.write(float4(sc32.y,0,0,0), uint2(x,y));
        tex_p1_2.write(float4(sc32.z,0,0,0), uint2(x,y));
        tex_p1_3.write(float4(sc32.w,0,0,0), uint2(x,y));
    } else if (fmt_p1 == 0u) {
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = pack_float_to_unorm4x8(clamp(sc32, 0.0, 1.0));
    } else if (fmt_p1 == 1u) {
        ((device ushort*)(p1 + y * buf_rbs[1]))[x] = pack_float_to_unorm565(clamp(sc32.zyx, 0.0, 1.0));
    } else if (fmt_p1 == 2u) {
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = pack_float_to_unorm10a2(clamp(sc32, 0.0, 1.0));
    } else if (fmt_p1 == 3u) {
        device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
        hp[0]=half(sc32.x); hp[1]=half(sc32.y); hp[2]=half(sc32.z); hp[3]=half(sc32.w);
    } else if (fmt_p1 == 4u) {
        device uchar *row = p1 + y * buf_rbs[1]; uint ps = buf_szs[1]/4;
        ((device half*)row)[x] = half(sc32.x); ((device half*)(row+ps))[x] = half(sc32.y); ((device half*)(row+2*ps))[x] = half(sc32.z); ((device half*)(row+3*ps))[x] = half(sc32.w);
    } else {
        for (int ch = 0; ch < 3; ch++) {
            float l = max(sc32[ch], 0.0);
            float t = 1.0/sqrt(max(l, 1e-30));
            float lo = l * 12.92;
            float hi = (1.0545324087e+00 + t*(5.8207426220e-02 + t*(-1.2198361568e-02 + t*(7.9244317021e-04 + t*-2.0467568902e-05)))) / (1.0131348670e-01 + t);
            sc32[ch] = lo < 4.5700869523e-03*12.92 ? lo : hi;
        }
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = pack_float_to_unorm4x8(clamp(sc32, 0.0, 1.0));
    }
}
