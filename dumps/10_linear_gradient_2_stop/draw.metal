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
    uint v19 = y0 + pos.y;
    float v20 = (float)(int)v19;
    float v21 = v20 * as_type<float>(v2);
    float v22 = fma(v18, as_type<float>(v1), v21);
    float v23 = as_type<float>(v3) + v22;
    float v24 = max(v23, as_type<float>(0u));
    float v25 = min(v24, as_type<float>(1065353216u));
    float v26 = fma(v25, v16, as_type<float>(v8));
    float v27 = fma(v25, v13, as_type<float>(v5));
    float v28 = fma(v25, v15, as_type<float>(v7));
    float v29 = fma(v25, v14, as_type<float>(v6));
    float4 sc30 = float4(v27, v29, v28, v26);
    if (planes_p1 == 1) {
        float4 tw30 = (fmt_p1 == 3u || fmt_p1 == 4u) ? float4(half4(sc30)) : sc30;
        tex_p1_0.write(tw30, uint2(x,y));
    } else if (planes_p1 == 4) {
        half4 tw30 = half4(sc30);
        tex_p1_0.write(float4(tw30.x,0,0,0), uint2(x,y));
        tex_p1_1.write(float4(tw30.y,0,0,0), uint2(x,y));
        tex_p1_2.write(float4(tw30.z,0,0,0), uint2(x,y));
        tex_p1_3.write(float4(tw30.w,0,0,0), uint2(x,y));
    } else if (fmt_p1 == 0u) {
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = pack_float_to_unorm4x8(clamp(sc30, 0.0, 1.0));
    } else if (fmt_p1 == 1u) {
        ((device ushort*)(p1 + y * buf_rbs[1]))[x] = pack_float_to_unorm565(clamp(sc30.zyx, 0.0, 1.0));
    } else if (fmt_p1 == 2u) {
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = pack_float_to_unorm10a2(clamp(sc30, 0.0, 1.0));
    } else if (fmt_p1 == 3u) {
        device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
        hp[0]=half(sc30.x); hp[1]=half(sc30.y); hp[2]=half(sc30.z); hp[3]=half(sc30.w);
    } else if (fmt_p1 == 4u) {
        device uchar *row = p1 + y * buf_rbs[1]; uint ps = buf_szs[1]/4;
        ((device half*)row)[x] = half(sc30.x); ((device half*)(row+ps))[x] = half(sc30.y); ((device half*)(row+2*ps))[x] = half(sc30.z); ((device half*)(row+3*ps))[x] = half(sc30.w);
    } else {
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = pack_float_to_srgb_unorm4x8(clamp(sc30, 0.0, 1.0));
    }
}
