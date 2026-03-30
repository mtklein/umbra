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
    uint v6 = ((device const uint*)p0)[4];
    uint v7 = ((device const uint*)p0)[5];
    uint v8 = ((device const uint*)p0)[6];
    uint v9 = ((device const uint*)p0)[7];
    uint v10 = ((device const uint*)p0)[8];
    uint v11 = ((device const uint*)p0)[9];
    uint v12 = ((device const uint*)p0)[10];
    uint v13 = ((device const uint*)p0)[11];
    uint v14 = ((device const uint*)p0)[12];
    uint v15 = ((device const uint*)p0)[13];
    uint v16 = ((device const uint*)p0)[14];
    uint v17 = 1065353216u;
    float v18 = as_type<float>(v15) - as_type<float>(1065353216u);
    float v19 = as_type<float>(v16) - as_type<float>(1065353216u);
    uint v20 = (uint)(int)floor(as_type<float>(v15));
    uint v21 = 998277249u;
    float v22 = as_type<float>(v17) - as_type<float>(v4);
    uint v23 = x0 + pos.x;
    float v24 = (float)(int)v23;
    uint v25 = y0 + pos.y;
    float v26 = (float)(int)v25;
    float v27 = v26 * as_type<float>(v10);
    float v28 = fma(v24, as_type<float>(v9), v27);
    float v29 = as_type<float>(v11) + v28;
    float v30 = v26 * as_type<float>(v13);
    float v31 = fma(v24, as_type<float>(v12), v30);
    float v32 = as_type<float>(v14) + v31;
    float v33 = v29 / v32;
    uint v34 = v33 <  as_type<float>(v16) ? 0xffffffffu : 0u;
    float v35 = v26 * as_type<float>(v7);
    float v36 = fma(v24, as_type<float>(v6), v35);
    float v37 = as_type<float>(v8) + v36;
    float v38 = v37 / v32;
    uint v39 = v38 <  as_type<float>(v15) ? 0xffffffffu : 0u;
    float v40 = max(v38, as_type<float>(0u));
    float v41 = min(v40, v18);
    uint v42 = (uint)(int)floor(v41);
    float v43 = max(v33, as_type<float>(0u));
    float v44 = min(v43, v19);
    uint v45 = (uint)(int)floor(v44);
    uint v46 = v45 * v20;
    uint v47 = v42 + v46;
    uint v48 = (uint)((device ushort*)p2)[safe_ix((int)v47,buf_szs[2],2)] & oob_mask((int)v47,buf_szs[2],2);
    uint v49 = (uint)(int)(short)(ushort)v48;
    float v50 = (float)(int)v49;
    float v51 = v50 * as_type<float>(998277249u);
    uint v52 = as_type<float>(v0) <= v38 ? 0xffffffffu : 0u;
    uint v53 = v52 & v39;
    uint v54 = as_type<float>(v0) <= v33 ? 0xffffffffu : 0u;
    uint v55 = v54 & v34;
    uint v56 = v53 & v55;
    uint v57 = select(v0, as_type<uint>(v51), v56 != 0u);
    float4 v58_c;
    if (planes_p1 == 1) {
        v58_c = tex_p1_0.read(uint2(x,y));
    } else if (planes_p1 == 4) {
        v58_c = float4(tex_p1_0.read(uint2(x,y)).r, tex_p1_1.read(uint2(x,y)).r, tex_p1_2.read(uint2(x,y)).r, tex_p1_3.read(uint2(x,y)).r);
    } else if (fmt_p1 == 0u) {
        v58_c = unpack_unorm4x8_to_float(((device uint*)(p1 + y * buf_rbs[1]))[x]);
    } else if (fmt_p1 == 1u) {
        float3 bgr = unpack_unorm565_to_float(((device ushort*)(p1 + y * buf_rbs[1]))[x]);
        v58_c = float4(bgr.z, bgr.y, bgr.x, 1.0);
    } else if (fmt_p1 == 2u) {
        v58_c = unpack_unorm10a2_to_float(((device uint*)(p1 + y * buf_rbs[1]))[x]);
    } else if (fmt_p1 == 3u) {
        device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
        v58_c = float4(hp[0], hp[1], hp[2], hp[3]);
    } else if (fmt_p1 == 4u) {
        device uchar *row = p1 + y * buf_rbs[1]; uint ps = buf_szs[1]/4;
        v58_c = float4(float(((device half*)row)[x]),float(((device half*)(row+ps))[x]),float(((device half*)(row+2*ps))[x]),float(((device half*)(row+3*ps))[x]));
    } else {
        v58_c = unpack_unorm4x8_srgb_to_float(((device uint*)(p1 + y * buf_rbs[1]))[x]);
    }
    float v58 = v58_c.x;
    float v58_1 = v58_c.y;
    float v58_2 = v58_c.z;
    float v58_3 = v58_c.w;
    float v59 = fma(v58_3, v22, as_type<float>(v4));
    float v60 = v59 - v58_3;
    float v61 = fma(as_type<float>(v57), v60, v58_3);
    float v62 = fma(v58, v22, as_type<float>(v1));
    float v63 = v62 - v58;
    float v64 = fma(as_type<float>(v57), v63, v58);
    float v65 = fma(v58_1, v22, as_type<float>(v2));
    float v66 = v65 - v58_1;
    float v67 = fma(as_type<float>(v57), v66, v58_1);
    float v68 = fma(v58_2, v22, as_type<float>(v3));
    float v69 = v68 - v58_2;
    float v70 = fma(as_type<float>(v57), v69, v58_2);
    float4 sc71 = float4(v64, v67, v70, v61);
    if (planes_p1 == 1) {
        tex_p1_0.write(sc71, uint2(x,y));
    } else if (planes_p1 == 4) {
        tex_p1_0.write(float4(sc71.x,0,0,0), uint2(x,y));
        tex_p1_1.write(float4(sc71.y,0,0,0), uint2(x,y));
        tex_p1_2.write(float4(sc71.z,0,0,0), uint2(x,y));
        tex_p1_3.write(float4(sc71.w,0,0,0), uint2(x,y));
    } else if (fmt_p1 == 0u) {
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = pack_float_to_unorm4x8(clamp(sc71, 0.0, 1.0));
    } else if (fmt_p1 == 1u) {
        ((device ushort*)(p1 + y * buf_rbs[1]))[x] = pack_float_to_unorm565(clamp(sc71.zyx, 0.0, 1.0));
    } else if (fmt_p1 == 2u) {
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = pack_float_to_unorm10a2(clamp(sc71, 0.0, 1.0));
    } else if (fmt_p1 == 3u) {
        device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
        hp[0]=half(sc71.x); hp[1]=half(sc71.y); hp[2]=half(sc71.z); hp[3]=half(sc71.w);
    } else if (fmt_p1 == 4u) {
        device uchar *row = p1 + y * buf_rbs[1]; uint ps = buf_szs[1]/4;
        ((device half*)row)[x] = half(sc71.x); ((device half*)(row+ps))[x] = half(sc71.y); ((device half*)(row+2*ps))[x] = half(sc71.z); ((device half*)(row+3*ps))[x] = half(sc71.w);
    } else {
        ((device uint*)(p1 + y * buf_rbs[1]))[x] = pack_float_to_srgb_unorm4x8(clamp(sc71, 0.0, 1.0));
    }
}
