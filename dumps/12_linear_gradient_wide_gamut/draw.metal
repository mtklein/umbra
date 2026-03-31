#include <metal_stdlib>
using namespace metal;

constant uint planes_p1 [[function_constant(0)]];
constant uint fmt_p1 [[function_constant(1)]];

enum { FMT_8888=0, FMT_565=1, FMT_1010102=2, FMT_FP16=3, FMT_FP16_PLANAR=4 };

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
    float v8 = as_type<float>(v6) - as_type<float>(1065353216u);
    float v9 = as_type<float>(v6) - as_type<float>(1073741824u);
    uint v10 = 2u;
    uint v11 = 4u;
    uint v12 = 1u;
    uint v13 = 3u;
    uint v14 = x0 + pos.x;
    float v15 = (float)(int)v14;
    uint v16 = y0 + pos.y;
    float v17 = (float)(int)v16;
    float v18 = v17 * as_type<float>(v3);
    float v19 = fma(v15, as_type<float>(v2), v18);
    float v20 = as_type<float>(v4) + v19;
    float v21 = max(v20, as_type<float>(0u));
    float v22 = min(v21, as_type<float>(1065353216u));
    float v23 = v22 * v8;
    float v24 = floor(v23);
    float v25 = min(v9, v24);
    uint v26 = (uint)(int)v25;
    uint v27 = v26 << 2u;
    uint v28 = v27 + 3u;
    uint v29 = ((device uint*)p2)[safe_ix((int)v28,buf_szs[2],4)] & oob_mask((int)v28,buf_szs[2],4);
    uint v30 = v27 + 4u;
    uint v31 = v30 + 3u;
    uint v32 = ((device uint*)p2)[safe_ix((int)v31,buf_szs[2],4)] & oob_mask((int)v31,buf_szs[2],4);
    float v33 = as_type<float>(v32) - as_type<float>(v29);
    uint v34 = v30 + 1u;
    uint v35 = ((device uint*)p2)[safe_ix((int)v34,buf_szs[2],4)] & oob_mask((int)v34,buf_szs[2],4);
    uint v36 = v30 + 2u;
    uint v37 = ((device uint*)p2)[safe_ix((int)v36,buf_szs[2],4)] & oob_mask((int)v36,buf_szs[2],4);
    float v38 = v23 - v25;
    float v39 = fma(v38, v33, as_type<float>(v29));
    uint v40 = v27 + 1u;
    uint v41 = ((device uint*)p2)[safe_ix((int)v40,buf_szs[2],4)] & oob_mask((int)v40,buf_szs[2],4);
    float v42 = as_type<float>(v35) - as_type<float>(v41);
    float v43 = fma(v38, v42, as_type<float>(v41));
    uint v44 = v27 + 2u;
    uint v45 = ((device uint*)p2)[safe_ix((int)v44,buf_szs[2],4)] & oob_mask((int)v44,buf_szs[2],4);
    float v46 = as_type<float>(v37) - as_type<float>(v45);
    float v47 = fma(v38, v46, as_type<float>(v45));
    uint v48 = ((device uint*)p2)[safe_ix((int)v30,buf_szs[2],4)] & oob_mask((int)v30,buf_szs[2],4);
    uint v49 = ((device uint*)p2)[safe_ix((int)v27,buf_szs[2],4)] & oob_mask((int)v27,buf_szs[2],4);
    float v50 = as_type<float>(v48) - as_type<float>(v49);
    float v51 = fma(v38, v50, as_type<float>(v49));
    float4 sc52 = float4(v51, v43, v47, v39);
    if (planes_p1 == 1) {
        float4 tw52 = (fmt_p1 == 3u || fmt_p1 == 4u) ? float4(half4(sc52)) : sc52;
        tex_p1_0.write(tw52, uint2(x,y));
    } else if (planes_p1 == 4) {
        half4 tw52 = half4(sc52);
        tex_p1_0.write(float4(tw52.x,0,0,0), uint2(x,y));
        tex_p1_1.write(float4(tw52.y,0,0,0), uint2(x,y));
        tex_p1_2.write(float4(tw52.z,0,0,0), uint2(x,y));
        tex_p1_3.write(float4(tw52.w,0,0,0), uint2(x,y));
    } else if (fmt_p1 == 0u) {
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = pack_float_to_unorm4x8(clamp(sc52, 0.0, 1.0));
    } else if (fmt_p1 == 1u) {
        ((device ushort*)(p1 + y * buf_rbs[1]))[x] = pack_float_to_unorm565(clamp(sc52.zyx, 0.0, 1.0));
    } else if (fmt_p1 == 2u) {
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = pack_float_to_unorm10a2(clamp(sc52, 0.0, 1.0));
    } else if (fmt_p1 == 3u) {
        device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
        hp[0]=half(sc52.x); hp[1]=half(sc52.y); hp[2]=half(sc52.z); hp[3]=half(sc52.w);
    } else if (fmt_p1 == 4u) {
        device uchar *row = p1 + y * buf_rbs[1]; uint ps = buf_szs[1]/4;
        ((device half*)row)[x] = half(sc52.x); ((device half*)(row+ps))[x] = half(sc52.y); ((device half*)(row+2*ps))[x] = half(sc52.z); ((device half*)(row+3*ps))[x] = half(sc52.w);
    }
}
