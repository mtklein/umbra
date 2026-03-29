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
    constant uint &w [[buffer(3)]],
    constant uint *buf_szs [[buffer(4)]],
    constant uint *buf_rbs [[buffer(5)]],
    constant uint &x0 [[buffer(6)]],
    constant uint &y0 [[buffer(7)]],
    constant uint *buf_fmts [[buffer(8)]],
    device uchar *p0 [[buffer(0)]],
    device uchar *p1 [[buffer(1)]],
    device uchar *p2 [[buffer(2)]],
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
    uint v18 = as_type<uint>(as_type<float>(v15) - as_type<float>(1065353216u));
    uint v19 = as_type<uint>(as_type<float>(v16) - as_type<float>(1065353216u));
    uint v20 = as_type<uint>((int)floor(as_type<float>(v15)));
    uint v21 = 998277249u;
    uint v22 = as_type<uint>(as_type<float>(v17) - as_type<float>(v4));
    uint v23 = x0 + pos.x;
    uint v24 = as_type<uint>((float)(int)v23);
    uint v25 = y0 + pos.y;
    uint v26 = as_type<uint>((float)(int)v25);
    uint v27 = as_type<uint>(as_type<float>(v26) * as_type<float>(v10));
    uint v28 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v9), as_type<float>(v27)));
    uint v29 = as_type<uint>(as_type<float>(v11) + as_type<float>(v28));
    uint v30 = as_type<uint>(as_type<float>(v26) * as_type<float>(v13));
    uint v31 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v12), as_type<float>(v30)));
    uint v32 = as_type<uint>(as_type<float>(v14) + as_type<float>(v31));
    uint v33 = as_type<uint>(as_type<float>(v29) / as_type<float>(v32));
    uint v34 = as_type<float>(v33) <  as_type<float>(v16) ? 0xffffffffu : 0u;
    uint v35 = as_type<uint>(as_type<float>(v26) * as_type<float>(v7));
    uint v36 = as_type<uint>(fma(as_type<float>(v24), as_type<float>(v6), as_type<float>(v35)));
    uint v37 = as_type<uint>(as_type<float>(v8) + as_type<float>(v36));
    uint v38 = as_type<uint>(as_type<float>(v37) / as_type<float>(v32));
    uint v39 = as_type<float>(v38) <  as_type<float>(v15) ? 0xffffffffu : 0u;
    uint v40 = as_type<uint>(max(as_type<float>(v38), as_type<float>(0u)));
    uint v41 = as_type<uint>(min(as_type<float>(v40), as_type<float>(v18)));
    uint v42 = as_type<uint>((int)floor(as_type<float>(v41)));
    uint v43 = as_type<uint>(max(as_type<float>(v33), as_type<float>(0u)));
    uint v44 = as_type<uint>(min(as_type<float>(v43), as_type<float>(v19)));
    uint v45 = as_type<uint>((int)floor(as_type<float>(v44)));
    uint v46 = v45 * v20;
    uint v47 = v42 + v46;
    uint v48 = (uint)((device ushort*)p2)[safe_ix((int)v47,buf_szs[2],2)] & oob_mask((int)v47,buf_szs[2],2);
    uint v49 = (uint)(int)(short)(ushort)v48;
    uint v50 = as_type<uint>((float)(int)v49);
    uint v51 = as_type<uint>(as_type<float>(v50) * as_type<float>(998277249u));
    uint v52 = as_type<float>(v0) <= as_type<float>(v38) ? 0xffffffffu : 0u;
    uint v53 = v52 & v39;
    uint v54 = as_type<float>(v0) <= as_type<float>(v33) ? 0xffffffffu : 0u;
    uint v55 = v54 & v34;
    uint v56 = v53 & v55;
    uint v57 = (v56 & v51) | (~v56 & v0);
    float4 v58_c;
    switch (buf_fmts[1]) {
      case 1u: { uint px = ((device uint*)(p1 + y * buf_rbs[1]))[x];
                v58_c = float4(px & 0xFFu, (px>>8)&0xFFu, (px>>16)&0xFFu, px>>24) / 255.0; break; }
      case 2u: { ushort px = ((device ushort*)(p1 + y * buf_rbs[1]))[x];
                v58_c = float4(float(px>>11)/31.0, float((px>>5)&0x3Fu)/63.0, float(px&0x1Fu)/31.0, 1.0); break; }
      case 3u: { uint px = ((device uint*)(p1 + y * buf_rbs[1]))[x];
                v58_c = float4(float(px&0x3FFu)/1023.0, float((px>>10)&0x3FFu)/1023.0, float((px>>20)&0x3FFu)/1023.0, float(px>>30)/3.0); break; }
      case 4u: { device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
                v58_c = float4(hp[0], hp[1], hp[2], hp[3]); break; }
      case 5u: { half h = ((device half*)(p1 + y * buf_rbs[1]))[x];
                v58_c = float4(float(h), 0, 0, 1); break; }
      case 6u: { float f = ((device float*)(p1 + y * buf_rbs[1]))[x];
                v58_c = float4(f, 0, 0, 1); break; }
      default: v58_c = float4(0); break;
    }
    uint v58 = as_type<uint>(v58_c.x);
    uint v58_1 = as_type<uint>(v58_c.y);
    uint v58_2 = as_type<uint>(v58_c.z);
    uint v58_3 = as_type<uint>(v58_c.w);
    uint v59 = as_type<uint>(fma(as_type<float>(v58_3), as_type<float>(v22), as_type<float>(v4)));
    uint v60 = as_type<uint>(as_type<float>(v59) - as_type<float>(v58_3));
    uint v61 = as_type<uint>(fma(as_type<float>(v57), as_type<float>(v60), as_type<float>(v58_3)));
    uint v62 = as_type<uint>(fma(as_type<float>(v58), as_type<float>(v22), as_type<float>(v1)));
    uint v63 = as_type<uint>(as_type<float>(v62) - as_type<float>(v58));
    uint v64 = as_type<uint>(fma(as_type<float>(v57), as_type<float>(v63), as_type<float>(v58)));
    uint v65 = as_type<uint>(fma(as_type<float>(v58_1), as_type<float>(v22), as_type<float>(v2)));
    uint v66 = as_type<uint>(as_type<float>(v65) - as_type<float>(v58_1));
    uint v67 = as_type<uint>(fma(as_type<float>(v57), as_type<float>(v66), as_type<float>(v58_1)));
    uint v68 = as_type<uint>(fma(as_type<float>(v58_2), as_type<float>(v22), as_type<float>(v3)));
    uint v69 = as_type<uint>(as_type<float>(v68) - as_type<float>(v58_2));
    uint v70 = as_type<uint>(fma(as_type<float>(v57), as_type<float>(v69), as_type<float>(v58_2)));
    float4 sc71 = float4(as_type<float>(v64), as_type<float>(v67), as_type<float>(v70), as_type<float>(v61));
    switch (buf_fmts[1]) {
      case 1u: { sc71 = clamp(sc71, 0.0, 1.0);
                ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc71.x*255.0)) | (uint(rint(sc71.y*255.0))<<8) | (uint(rint(sc71.z*255.0))<<16) | (uint(rint(sc71.w*255.0))<<24); break; }
      case 2u: { sc71 = clamp(sc71, 0.0, 1.0);
                ((device ushort*)(p1 + y * buf_rbs[1]))[x] = ushort((uint(rint(sc71.x*31.0))<<11) | (uint(rint(sc71.y*63.0))<<5) | uint(rint(sc71.z*31.0))); break; }
      case 3u: { sc71 = clamp(sc71, 0.0, 1.0);
                ((device uint*)(p1 + y * buf_rbs[1]))[x] = uint(rint(sc71.x*1023.0)) | (uint(rint(sc71.y*1023.0))<<10) | (uint(rint(sc71.z*1023.0))<<20) | (uint(rint(sc71.w*3.0))<<30); break; }
      case 4u: { device half *hp = (device half*)(p1 + y * buf_rbs[1]) + x*4;
                hp[0]=half(sc71.x); hp[1]=half(sc71.y); hp[2]=half(sc71.z); hp[3]=half(sc71.w); break; }
      case 5u: ((device half*)(p1 + y * buf_rbs[1]))[x] = half(sc71.x); break;
      case 6u: ((device float*)(p1 + y * buf_rbs[1]))[x] = sc71.x; break;
      default: break;
    }
}
