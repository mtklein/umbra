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
    constant uint &w [[buffer(2)]],
    constant uint *buf_szs [[buffer(3)]],
    constant uint *buf_rbs [[buffer(4)]],
    constant uint &x0 [[buffer(5)]],
    constant uint &y0 [[buffer(6)]],
    device uchar *p0 [[buffer(0)]],
    device uchar *p1 [[buffer(1)]],
    texture2d<float, access::write> tex_p1_0 [[texture(0)]],
    texture2d<float, access::write> tex_p1_1 [[texture(1)]],
    texture2d<float, access::write> tex_p1_2 [[texture(2)]],
    texture2d<float, access::write> tex_p1_3 [[texture(3)]],
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
    float v13 = as_type<float>(v9) - as_type<float>(v5);
    float v14 = as_type<float>(v10) - as_type<float>(v6);
    float v15 = as_type<float>(v11) - as_type<float>(v7);
    float v16 = as_type<float>(v12) - as_type<float>(v8);
    uint v17 = x0 + pos.x;
    float v18 = (float)(int)v17;
    float v19 = v18 - as_type<float>(v1);
    uint v20 = y0 + pos.y;
    float v21 = (float)(int)v20;
    float v22 = v21 - as_type<float>(v2);
    float v23 = v22 * v22;
    float v24 = fma(v19, v19, v23);
    float v25 = sqrt(v24);
    float v26 = as_type<float>(v3) * v25;
    float v27 = max(v26, as_type<float>(0u));
    float v28 = min(v27, as_type<float>(1065353216u));
    float v29 = fma(v28, v16, as_type<float>(v8));
    float v30 = fma(v28, v13, as_type<float>(v5));
    float v31 = fma(v28, v15, as_type<float>(v7));
    float v32 = fma(v28, v14, as_type<float>(v6));
    float4 sc33 = float4(v30, v32, v31, v29);
    if (planes_p1 == 1) {
        tex_p1_0.write(sc33, uint2(x,y));
    } else if (planes_p1 == 4) {
        tex_p1_0.write(float4(sc33.x,0,0,0), uint2(x,y));
        tex_p1_1.write(float4(sc33.y,0,0,0), uint2(x,y));
        tex_p1_2.write(float4(sc33.z,0,0,0), uint2(x,y));
        tex_p1_3.write(float4(sc33.w,0,0,0), uint2(x,y));
    } else if (fmt_p1 == 0u) {
        sc33 = clamp(sc33, 0.0, 1.0);
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc33.x*255.0)) | (uint(rint(sc33.y*255.0))<<8) | (uint(rint(sc33.z*255.0))<<16) | (uint(rint(sc33.w*255.0))<<24);
    } else if (fmt_p1 == 1u) {
        sc33 = clamp(sc33, 0.0, 1.0);
        ((device ushort*)(p1 + y * buf_rbs[1]))[x] = ushort((uint(rint(sc33.x*31.0))<<11) | (uint(rint(sc33.y*63.0))<<5) | uint(rint(sc33.z*31.0)));
    } else if (fmt_p1 == 2u) {
        sc33 = clamp(sc33, 0.0, 1.0);
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc33.x*1023.0)) | (uint(rint(sc33.y*1023.0))<<10) | (uint(rint(sc33.z*1023.0))<<20) | (uint(rint(sc33.w*3.0))<<30);
    } else if (fmt_p1 == 3u) {
        device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
        hp[0]=half(sc33.x); hp[1]=half(sc33.y); hp[2]=half(sc33.z); hp[3]=half(sc33.w);
    } else if (fmt_p1 == 4u) {
        device uchar *row = p1 + y * buf_rbs[1]; uint ps = buf_szs[1]/4;
        ((device half*)row)[x] = half(sc33.x); ((device half*)(row+ps))[x] = half(sc33.y); ((device half*)(row+2*ps))[x] = half(sc33.z); ((device half*)(row+3*ps))[x] = half(sc33.w);
    } else {
        for (int ch = 0; ch < 3; ch++) {
            float l = max(sc33[ch], 0.0);
            float t = 1.0/sqrt(max(l, 1e-30));
            float lo = l * 12.92;
            float hi = (1.0545324087e+00 + t*(5.8207426220e-02 + t*(-1.2198361568e-02 + t*(7.9244317021e-04 + t*-2.0467568902e-05)))) / (1.0131348670e-01 + t);
            sc33[ch] = lo < 4.5700869523e-03*12.92 ? lo : hi;
        } sc33 = clamp(sc33, 0.0, 1.0);
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc33.x*255.0)) | (uint(rint(sc33.y*255.0))<<8) | (uint(rint(sc33.z*255.0))<<16) | (uint(rint(sc33.w*255.0))<<24);
    }
}
