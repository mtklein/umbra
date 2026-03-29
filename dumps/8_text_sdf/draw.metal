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
    uint v10 = as_type<uint>(as_type<float>(v9) - as_type<float>(v4));
    uint v11 = (uint)((device ushort*)(p2 + y * buf_rbs[2]))[x];
    uint v12 = (uint)(int)(short)(ushort)v11;
    uint v13 = as_type<uint>((float)(int)v12);
    uint v14 = as_type<uint>(as_type<float>(v13) * as_type<float>(998277249u));
    uint v15 = as_type<uint>(as_type<float>(v14) - as_type<float>(1054867456u));
    uint v16 = as_type<uint>(as_type<float>(v15) * as_type<float>(1090519040u));
    uint v17 = as_type<uint>(max(as_type<float>(v16), as_type<float>(0u)));
    uint v18 = as_type<uint>(min(as_type<float>(v17), as_type<float>(1065353216u)));
    float4 v19_c;
    if (planes_p1 == 1) {
        v19_c = tex_p1_0.read(uint2(x,y));
    } else if (planes_p1 == 4) {
        v19_c = float4(tex_p1_0.read(uint2(x,y)).r, tex_p1_1.read(uint2(x,y)).r, tex_p1_2.read(uint2(x,y)).r, tex_p1_3.read(uint2(x,y)).r);
    } else if (fmt_p1 == 0u) {
        uint px = ((device uint*)(p1 + y * buf_rbs[1]))[x];
        v19_c = float4(px & 0xFFu, (px>>8)&0xFFu, (px>>16)&0xFFu, px>>24) / 255.0;
    } else if (fmt_p1 == 1u) {
        ushort px = ((device ushort*)(p1 + y * buf_rbs[1]))[x];
        v19_c = float4(float(px>>11)/31.0, float((px>>5)&0x3Fu)/63.0, float(px&0x1Fu)/31.0, 1.0);
    } else if (fmt_p1 == 2u) {
        uint px = ((device uint*)(p1 + y * buf_rbs[1]))[x];
        v19_c = float4(float(px&0x3FFu)/1023.0, float((px>>10)&0x3FFu)/1023.0, float((px>>20)&0x3FFu)/1023.0, float(px>>30)/3.0);
    } else if (fmt_p1 == 3u) {
        device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
        v19_c = float4(hp[0], hp[1], hp[2], hp[3]);
    } else if (fmt_p1 == 4u) {
        device uchar *row = p1 + y * buf_rbs[1]; uint ps = buf_szs[1]/4;
        v19_c = float4(float(((device half*)row)[x]),float(((device half*)(row+ps))[x]),float(((device half*)(row+2*ps))[x]),float(((device half*)(row+3*ps))[x]));
    } else {
        uint px = ((device uint*)(p1 + y * buf_rbs[1]))[x];
        v19_c = float4(px & 0xFFu, (px>>8)&0xFFu, (px>>16)&0xFFu, px>>24) / 255.0;
        for (int ch = 0; ch < 3; ch++) {
            float s = v19_c[ch];
            v19_c[ch] = s < 0.09870274 ? s/12.92 : s*s*(-0.03423264*s*s*s+0.02881829*s*s+0.31312484*s+0.68812025)+0.00333771;
        }
    }
    uint v19 = as_type<uint>(v19_c.x);
    uint v19_1 = as_type<uint>(v19_c.y);
    uint v19_2 = as_type<uint>(v19_c.z);
    uint v19_3 = as_type<uint>(v19_c.w);
    uint v20 = as_type<uint>(fma(as_type<float>(v19_3), as_type<float>(v10), as_type<float>(v4)));
    uint v21 = as_type<uint>(as_type<float>(v20) - as_type<float>(v19_3));
    uint v22 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v21), as_type<float>(v19_3)));
    uint v23 = as_type<uint>(fma(as_type<float>(v19), as_type<float>(v10), as_type<float>(v1)));
    uint v24 = as_type<uint>(as_type<float>(v23) - as_type<float>(v19));
    uint v25 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v24), as_type<float>(v19)));
    uint v26 = as_type<uint>(fma(as_type<float>(v19_1), as_type<float>(v10), as_type<float>(v2)));
    uint v27 = as_type<uint>(as_type<float>(v26) - as_type<float>(v19_1));
    uint v28 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v27), as_type<float>(v19_1)));
    uint v29 = as_type<uint>(fma(as_type<float>(v19_2), as_type<float>(v10), as_type<float>(v3)));
    uint v30 = as_type<uint>(as_type<float>(v29) - as_type<float>(v19_2));
    uint v31 = as_type<uint>(fma(as_type<float>(v18), as_type<float>(v30), as_type<float>(v19_2)));
    float4 sc32 = float4(as_type<float>(v25), as_type<float>(v28), as_type<float>(v31), as_type<float>(v22));
    if (planes_p1 == 1) {
        tex_p1_0.write(sc32, uint2(x,y));
    } else if (planes_p1 == 4) {
        tex_p1_0.write(float4(sc32.x,0,0,0), uint2(x,y));
        tex_p1_1.write(float4(sc32.y,0,0,0), uint2(x,y));
        tex_p1_2.write(float4(sc32.z,0,0,0), uint2(x,y));
        tex_p1_3.write(float4(sc32.w,0,0,0), uint2(x,y));
    } else if (fmt_p1 == 0u) {
        sc32 = clamp(sc32, 0.0, 1.0);
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc32.x*255.0)) | (uint(rint(sc32.y*255.0))<<8) | (uint(rint(sc32.z*255.0))<<16) | (uint(rint(sc32.w*255.0))<<24);
    } else if (fmt_p1 == 1u) {
        sc32 = clamp(sc32, 0.0, 1.0);
        ((device ushort*)(p1 + y * buf_rbs[1]))[x] = ushort((uint(rint(sc32.x*31.0))<<11) | (uint(rint(sc32.y*63.0))<<5) | uint(rint(sc32.z*31.0)));
    } else if (fmt_p1 == 2u) {
        sc32 = clamp(sc32, 0.0, 1.0);
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc32.x*1023.0)) | (uint(rint(sc32.y*1023.0))<<10) | (uint(rint(sc32.z*1023.0))<<20) | (uint(rint(sc32.w*3.0))<<30);
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
            float hi = (1.09732234 + t*(0.02995744 + t*(-0.00546762 + t*0.00012954))) / (0.12201570 + t);
            sc32[ch] = lo < 0.116027 ? lo : hi;
        } sc32 = clamp(sc32, 0.0, 1.0);
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc32.x*255.0)) | (uint(rint(sc32.y*255.0))<<8) | (uint(rint(sc32.z*255.0))<<16) | (uint(rint(sc32.w*255.0))<<24);
    }
}
