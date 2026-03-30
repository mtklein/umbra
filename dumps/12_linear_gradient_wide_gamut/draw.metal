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
    uint v2 = ((device const uint*)p0)[0];
    uint v3 = ((device const uint*)p0)[1];
    uint v4 = ((device const uint*)p0)[2];
    uint v5 = 1065353216u;
    uint v6 = ((device const uint*)p0)[3];
    uint v7 = 1073741824u;
    uint v8 = as_type<uint>(as_type<float>(v6) - as_type<float>(1065353216u));
    uint v9 = as_type<uint>(as_type<float>(v6) - as_type<float>(1073741824u));
    uint v10 = 2u;
    uint v11 = 4u;
    uint v12 = 1u;
    uint v13 = 3u;
    uint v14 = x0 + pos.x;
    uint v15 = as_type<uint>((float)(int)v14);
    uint v16 = y0 + pos.y;
    uint v17 = as_type<uint>((float)(int)v16);
    uint v18 = as_type<uint>(as_type<float>(v17) * as_type<float>(v3));
    uint v19 = as_type<uint>(fma(as_type<float>(v15), as_type<float>(v2), as_type<float>(v18)));
    uint v20 = as_type<uint>(as_type<float>(v4) + as_type<float>(v19));
    uint v21 = as_type<uint>(max(as_type<float>(v20), as_type<float>(0u)));
    uint v22 = as_type<uint>(min(as_type<float>(v21), as_type<float>(1065353216u)));
    uint v23 = as_type<uint>(as_type<float>(v22) * as_type<float>(v8));
    uint v24 = as_type<uint>(floor(as_type<float>(v23)));
    uint v25 = as_type<uint>(min(as_type<float>(v9), as_type<float>(v24)));
    uint v26 = (uint)(int)as_type<float>(v25);
    uint v27 = v26 << 2u;
    uint v28 = v27 + 3u;
    uint v29 = ((device uint*)p2)[safe_ix((int)v28,buf_szs[2],4)] & oob_mask((int)v28,buf_szs[2],4);
    uint v30 = v27 + 4u;
    uint v31 = v30 + 3u;
    uint v32 = ((device uint*)p2)[safe_ix((int)v31,buf_szs[2],4)] & oob_mask((int)v31,buf_szs[2],4);
    uint v33 = as_type<uint>(as_type<float>(v32) - as_type<float>(v29));
    uint v34 = v30 + 1u;
    uint v35 = ((device uint*)p2)[safe_ix((int)v34,buf_szs[2],4)] & oob_mask((int)v34,buf_szs[2],4);
    uint v36 = v30 + 2u;
    uint v37 = ((device uint*)p2)[safe_ix((int)v36,buf_szs[2],4)] & oob_mask((int)v36,buf_szs[2],4);
    uint v38 = as_type<uint>(as_type<float>(v23) - as_type<float>(v25));
    uint v39 = as_type<uint>(fma(as_type<float>(v38), as_type<float>(v33), as_type<float>(v29)));
    uint v40 = v27 + 1u;
    uint v41 = ((device uint*)p2)[safe_ix((int)v40,buf_szs[2],4)] & oob_mask((int)v40,buf_szs[2],4);
    uint v42 = as_type<uint>(as_type<float>(v35) - as_type<float>(v41));
    uint v43 = as_type<uint>(fma(as_type<float>(v38), as_type<float>(v42), as_type<float>(v41)));
    uint v44 = v27 + 2u;
    uint v45 = ((device uint*)p2)[safe_ix((int)v44,buf_szs[2],4)] & oob_mask((int)v44,buf_szs[2],4);
    uint v46 = as_type<uint>(as_type<float>(v37) - as_type<float>(v45));
    uint v47 = as_type<uint>(fma(as_type<float>(v38), as_type<float>(v46), as_type<float>(v45)));
    uint v48 = ((device uint*)p2)[safe_ix((int)v30,buf_szs[2],4)] & oob_mask((int)v30,buf_szs[2],4);
    uint v49 = ((device uint*)p2)[safe_ix((int)v27,buf_szs[2],4)] & oob_mask((int)v27,buf_szs[2],4);
    uint v50 = as_type<uint>(as_type<float>(v48) - as_type<float>(v49));
    uint v51 = as_type<uint>(fma(as_type<float>(v38), as_type<float>(v50), as_type<float>(v49)));
    float4 sc52 = float4(as_type<float>(v51), as_type<float>(v43), as_type<float>(v47), as_type<float>(v39));
    if (planes_p1 == 1) {
        tex_p1_0.write(sc52, uint2(x,y));
    } else if (planes_p1 == 4) {
        tex_p1_0.write(float4(sc52.x,0,0,0), uint2(x,y));
        tex_p1_1.write(float4(sc52.y,0,0,0), uint2(x,y));
        tex_p1_2.write(float4(sc52.z,0,0,0), uint2(x,y));
        tex_p1_3.write(float4(sc52.w,0,0,0), uint2(x,y));
    } else if (fmt_p1 == 0u) {
        sc52 = clamp(sc52, 0.0, 1.0);
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc52.x*255.0)) | (uint(rint(sc52.y*255.0))<<8) | (uint(rint(sc52.z*255.0))<<16) | (uint(rint(sc52.w*255.0))<<24);
    } else if (fmt_p1 == 1u) {
        sc52 = clamp(sc52, 0.0, 1.0);
        ((device ushort*)(p1 + y * buf_rbs[1]))[x] = ushort((uint(rint(sc52.x*31.0))<<11) | (uint(rint(sc52.y*63.0))<<5) | uint(rint(sc52.z*31.0)));
    } else if (fmt_p1 == 2u) {
        sc52 = clamp(sc52, 0.0, 1.0);
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc52.x*1023.0)) | (uint(rint(sc52.y*1023.0))<<10) | (uint(rint(sc52.z*1023.0))<<20) | (uint(rint(sc52.w*3.0))<<30);
    } else if (fmt_p1 == 3u) {
        device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
        hp[0]=half(sc52.x); hp[1]=half(sc52.y); hp[2]=half(sc52.z); hp[3]=half(sc52.w);
    } else if (fmt_p1 == 4u) {
        device uchar *row = p1 + y * buf_rbs[1]; uint ps = buf_szs[1]/4;
        ((device half*)row)[x] = half(sc52.x); ((device half*)(row+ps))[x] = half(sc52.y); ((device half*)(row+2*ps))[x] = half(sc52.z); ((device half*)(row+3*ps))[x] = half(sc52.w);
    } else {
        for (int ch = 0; ch < 3; ch++) {
            float l = max(sc52[ch], 0.0);
            float t = 1.0/sqrt(max(l, 1e-30));
            float lo = l * 12.92;
            float hi = (1.0545324087e+00 + t*(5.8207426220e-02 + t*(-1.2198361568e-02 + t*(7.9244317021e-04 + t*-2.0467568902e-05)))) / (1.0131348670e-01 + t);
            sc52[ch] = lo < 4.5700869523e-03*12.92 ? lo : hi;
        } sc52 = clamp(sc52, 0.0, 1.0);
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc52.x*255.0)) | (uint(rint(sc52.y*255.0))<<8) | (uint(rint(sc52.z*255.0))<<16) | (uint(rint(sc52.w*255.0))<<24);
    }
}
