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
    uint v5 = ((device const uint*)p0)[4];
    uint v6 = ((device const uint*)p0)[5];
    uint v7 = ((device const uint*)p0)[6];
    uint v8 = ((device const uint*)p0)[7];
    uint v9 = 1065353216u;
    uint v10 = x0 + pos.x;
    float v11 = (float)(int)v10;
    uint v12 = v11 <  as_type<float>(v7) ? 0xffffffffu : 0u;
    uint v13 = as_type<float>(v5) <= v11 ? 0xffffffffu : 0u;
    uint v14 = v13 & v12;
    uint v15 = y0 + pos.y;
    float v16 = (float)(int)v15;
    uint v17 = v16 <  as_type<float>(v8) ? 0xffffffffu : 0u;
    uint v18 = as_type<float>(v6) <= v16 ? 0xffffffffu : 0u;
    uint v19 = v18 & v17;
    uint v20 = v14 & v19;
    uint v21 = select(v0, v9, v20 != 0u);
    float4 v22_c;
    if (planes_p1 == 1) {
        v22_c = tex_p1_0.read(uint2(x,y));
    } else if (planes_p1 == 4) {
        v22_c = float4(tex_p1_0.read(uint2(x,y)).r, tex_p1_1.read(uint2(x,y)).r, tex_p1_2.read(uint2(x,y)).r, tex_p1_3.read(uint2(x,y)).r);
    } else if (fmt_p1 == 0u) {
        v22_c = unpack_unorm4x8_to_float(((device uint*)(p1 + y * buf_rbs[1]))[x]);
    } else if (fmt_p1 == 1u) {
        float3 bgr = unpack_unorm565_to_float(((device ushort*)(p1 + y * buf_rbs[1]))[x]);
        v22_c = float4(bgr.z, bgr.y, bgr.x, 1.0);
    } else if (fmt_p1 == 2u) {
        v22_c = unpack_unorm10a2_to_float(((device uint*)(p1 + y * buf_rbs[1]))[x]);
    } else if (fmt_p1 == 3u) {
        device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
        v22_c = float4(hp[0], hp[1], hp[2], hp[3]);
    } else if (fmt_p1 == 4u) {
        device uchar *row = p1 + y * buf_rbs[1]; uint ps = buf_szs[1]/4;
        v22_c = float4(float(((device half*)row)[x]),float(((device half*)(row+ps))[x]),float(((device half*)(row+2*ps))[x]),float(((device half*)(row+3*ps))[x]));
    } else {
        v22_c = unpack_unorm4x8_to_float(((device uint*)(p1 + y * buf_rbs[1]))[x]);
        for (int ch = 0; ch < 3; ch++) {
            float s = v22_c[ch];
            float a=-4.82083022594e-01,b=1.84310853481e+00,c=-2.79252314568e+00,d=2.05758404732e+00,e=-4.18130934238e-01,f=7.89776027203e-01;
            float inner = ((a*s+b)*s+c)*s+d; float mid = (inner*s+e)*s+f; float s2 = s*s;
            v22_c[ch] = s < 5.76281473041e-02 ? s/12.92 : mid*s2 + (1.0-(a+b+c+d+e+f));
        }
    }
    float v22 = v22_c.x;
    float v22_1 = v22_c.y;
    float v22_2 = v22_c.z;
    float v22_3 = v22_c.w;
    float v23 = as_type<float>(v9) - v22_3;
    float v24 = fma(as_type<float>(v4), v23, v22_3);
    float v25 = v24 - v22_3;
    float v26 = fma(as_type<float>(v21), v25, v22_3);
    float v27 = fma(as_type<float>(v1), v23, v22);
    float v28 = v27 - v22;
    float v29 = fma(as_type<float>(v21), v28, v22);
    float v30 = fma(as_type<float>(v2), v23, v22_1);
    float v31 = v30 - v22_1;
    float v32 = fma(as_type<float>(v21), v31, v22_1);
    float v33 = fma(as_type<float>(v3), v23, v22_2);
    float v34 = v33 - v22_2;
    float v35 = fma(as_type<float>(v21), v34, v22_2);
    float4 sc36 = float4(v29, v32, v35, v26);
    if (planes_p1 == 1) {
        tex_p1_0.write(sc36, uint2(x,y));
    } else if (planes_p1 == 4) {
        tex_p1_0.write(float4(sc36.x,0,0,0), uint2(x,y));
        tex_p1_1.write(float4(sc36.y,0,0,0), uint2(x,y));
        tex_p1_2.write(float4(sc36.z,0,0,0), uint2(x,y));
        tex_p1_3.write(float4(sc36.w,0,0,0), uint2(x,y));
    } else if (fmt_p1 == 0u) {
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = pack_float_to_unorm4x8(clamp(sc36, 0.0, 1.0));
    } else if (fmt_p1 == 1u) {
        ((device ushort*)(p1 + y * buf_rbs[1]))[x] = pack_float_to_unorm565(clamp(sc36.zyx, 0.0, 1.0));
    } else if (fmt_p1 == 2u) {
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = pack_float_to_unorm10a2(clamp(sc36, 0.0, 1.0));
    } else if (fmt_p1 == 3u) {
        device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
        hp[0]=half(sc36.x); hp[1]=half(sc36.y); hp[2]=half(sc36.z); hp[3]=half(sc36.w);
    } else if (fmt_p1 == 4u) {
        device uchar *row = p1 + y * buf_rbs[1]; uint ps = buf_szs[1]/4;
        ((device half*)row)[x] = half(sc36.x); ((device half*)(row+ps))[x] = half(sc36.y); ((device half*)(row+2*ps))[x] = half(sc36.z); ((device half*)(row+3*ps))[x] = half(sc36.w);
    } else {
        for (int ch = 0; ch < 3; ch++) {
            float l = max(sc36[ch], 0.0);
            float t = 1.0/sqrt(max(l, 1e-30));
            float lo = l * 12.92;
            float hi = (1.0545324087e+00 + t*(5.8207426220e-02 + t*(-1.2198361568e-02 + t*(7.9244317021e-04 + t*-2.0467568902e-05)))) / (1.0131348670e-01 + t);
            sc36[ch] = lo < 4.5700869523e-03*12.92 ? lo : hi;
        }
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = pack_float_to_unorm4x8(clamp(sc36, 0.0, 1.0));
    }
}
