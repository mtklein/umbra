#include "bb.h"
#include <assert.h>

#if !defined(__APPLE__) || defined(__wasm__)

struct umbra_backend *umbra_backend_metal(void) { return 0; }

#else

#import <Metal/Metal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct umbra_basic_block BB;

struct batch_shared {
    void *mtl;
    char *host;
    size_t copy_sz;
};

struct copyback {
    void *host;
    void *mtlbuf;
    size_t bytes;
};

struct deref_info { int buf_idx, src_buf, byte_off; };

struct color_buf_info {
    int buf_idx;
    int tex_base;
    _Bool read, write, pad_[2];
};

struct pipeline_entry {
    uint64_t key;
    void    *pipeline;
    int      tg_size, :32;
};

struct metal_backend {
    struct umbra_backend base;
    void *device;
    void *queue;
    void *batch_cmdbuf;
    void *batch_enc;
    void               **batch_bufs;
    int                  batch_nbufs, batch_bufs_cap;
    struct copyback     *batch_copy;
    int                  batch_ncopy, batch_copy_cap;
    int                  batch_gen, :32;
};

struct umbra_metal {
    struct umbra_program base;
    void *library;
    struct pipeline_entry *pipelines;
    int    n_pipelines, pipelines_cap;
    struct color_buf_info *color_bufs;
    int    n_color_bufs, n_textures;
    void **per_bufs;
    char  *src;
    int    max_ptr;
    int    total_bufs;
    int    tg_size;
    int    n_deref;
    struct deref_info    *deref;
    struct batch_shared  *batch_data;
    int                  batch_gen, :32;
};

typedef struct {
    char *buf;
    int   len, cap;
} Buf;

__attribute__((format(printf, 2, 3)))
static void emit(Buf *b, char const *fmt, ...) {
    va_list ap;
    for (;;) {
        va_start(ap, fmt);
        int n = vsnprintf(b->buf + b->len,
                          (size_t)(b->cap - b->len),
                          fmt, ap);
        va_end(ap);
        if (b->len + n < b->cap) {
            b->len += n;
            return;
        }
        b->cap = b->cap ? 2*b->cap : 4096;
        b->buf = realloc(b->buf, (size_t)b->cap);
    }
}

static _Bool is_16(enum op op) {
    return op == op_load_16
        || op == op_store_16
        || op == op_gather_16
        || op == op_i32_from_s16
        || op == op_i32_from_u16
        || op == op_i16_from_i32
        || op == op_f32_from_f16
        || op == op_f16_from_f32;
}
static _Bool is_32(enum op op) {
    return op == op_uniform_32
        || op == op_load_32
        || op == op_load_color
        || op == op_gather_uniform_32
        || op == op_gather_32
        || op == op_store_32
        || op == op_store_color
        || op == op_deref_ptr;
}

static _Bool produces_float(enum op op) {
    return op == op_add_f32     || op == op_sub_f32
        || op == op_mul_f32     || op == op_div_f32
        || op == op_min_f32     || op == op_max_f32
        || op == op_sqrt_f32    || op == op_abs_f32    || op == op_neg_f32
        || op == op_round_f32   || op == op_floor_f32  || op == op_ceil_f32
        || op == op_fma_f32     || op == op_fms_f32
        || op == op_add_f32_imm || op == op_sub_f32_imm
        || op == op_mul_f32_imm || op == op_div_f32_imm
        || op == op_min_f32_imm || op == op_max_f32_imm
        || op == op_f32_from_i32
        || op == op_f32_from_f16
        || op == op_load_color;
}

static char const *fv(char *tmp, char const *vn,
                      int id, _Bool const *is_f) {
    if (is_f[id]) { return vn; }
    snprintf(tmp, 40, "as_type<float>(%s)", vn);
    return tmp;
}
static char const *uv(char *tmp, char const *vn,
                      int id, _Bool const *is_f) {
    if (!is_f[id]) { return vn; }
    snprintf(tmp, 40, "as_type<uint>(%s)", vn);
    return tmp;
}

static void emit_ops(Buf *b, BB const *bb,
                     _Bool *ptr_16, _Bool *ptr_32,
                     int const *deref_buf,
                     _Bool *is_f,
                     int lo, int hi,
                     char const *pad) {
    for (int i = lo; i < hi; i++) {
        is_f[i] = produces_float(bb->inst[i].op);
    }

    char vx[16], vy[16], vz[16], vw[16];
#define VNAME(buf, vid, ch) \
    ((ch) ? (void)snprintf(buf, sizeof buf, "v%d_%d", (vid), (ch)) \
           : (void)snprintf(buf, sizeof buf, "v%d", (vid)), buf)

    for (int i = lo; i < hi; i++) {
        struct bb_inst const *inst = &bb->inst[i];
        int xid = (int)inst->x.id, yid = (int)inst->y.id,
            zid = (int)inst->z.id, wid = (int)inst->w.id;
        VNAME(vx, xid, (int)inst->x.chan);
        VNAME(vy, yid, (int)inst->y.chan);
        VNAME(vz, zid, (int)inst->z.chan);
        VNAME(vw, wid, (int)inst->w.chan);

        char _fx[40], _fy[40], _fz[40], _fw[40];
        char _ux[40], _uy[40], _uz[40];

        switch (inst->op) {
            case op_x:
                emit(b, "%suint v%d = x0 + pos.x;\n", pad, i);
                break;
            case op_y:
                emit(b, "%suint v%d = y0 + pos.y;\n", pad, i);
                break;

            case op_imm_32:
                emit(b, "%suint v%d = %uu;\n",
                     pad, i, (uint32_t)inst->imm);
                break;

            case op_deref_ptr: break;

            case op_uniform_32: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b, mixed
                    ? "%suint v%d = p%d_32[%d];\n"
                    : "%suint v%d = "
                      "((device const uint*)"
                      "p%d)[%d];\n",
                     pad, i, p, inst->imm);
            } break;
            case op_load_32: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                emit(b,
                     "%suint v%d = ((device uint*)"
                     "(p%d + y * buf_rbs[%d]))[x];\n",
                     pad, i, p, p);
            } break;
            case op_gather_uniform_32:
            case op_gather_32: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b, mixed
                    ? "%suint v%d = p%d_32"
                      "[safe_ix((int)%s,"
                      "buf_szs[%d],4)]"
                      " & oob_mask((int)%s,"
                      "buf_szs[%d],4);\n"
                    : "%suint v%d = "
                      "((device uint*)p%d)"
                      "[safe_ix((int)%s,"
                      "buf_szs[%d],4)]"
                      " & oob_mask((int)%s,"
                      "buf_szs[%d],4);\n",
                     pad, i, p, vx, p,
                     vx, p);
            } break;
            case op_store_32: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                emit(b,
                     "%s((device uint*)"
                     "(p%d + y * buf_rbs[%d]))"
                     "[x] = %s;\n",
                     pad, p, p,
                     uv(_uy, vy, yid, is_f));
            } break;
            case op_load_color: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                emit(b,
                     "%sfloat4 v%d_c;\n"
                     "%sif (planes_p%d == 1) {\n"
                     "%s    v%d_c = tex_p%d_0.read(uint2(x,y));\n"
                     "%s} else if (planes_p%d == 4) {\n"
                     "%s    v%d_c = float4(tex_p%d_0.read(uint2(x,y)).r,"
                     " tex_p%d_1.read(uint2(x,y)).r,"
                     " tex_p%d_2.read(uint2(x,y)).r,"
                     " tex_p%d_3.read(uint2(x,y)).r);\n"
                     "%s} else if (fmt_p%d == %du) {\n"
                     "%s    v%d_c = unpack_unorm4x8_to_float("
                     "((device uint*)(p%d + y * buf_rbs[%d]))[x]);\n"
                     "%s} else if (fmt_p%d == %du) {\n"
                     "%s    float3 bgr = unpack_unorm565_to_float("
                     "((device ushort*)(p%d + y * buf_rbs[%d]))[x]);\n"
                     "%s    v%d_c = float4(bgr.z, bgr.y, bgr.x, 1.0);\n"
                     "%s} else if (fmt_p%d == %du) {\n"
                     "%s    v%d_c = unpack_unorm10a2_to_float("
                     "((device uint*)(p%d + y * buf_rbs[%d]))[x]);\n"
                     "%s} else if (fmt_p%d == %du) {\n"
                     "%s    device half *hp = (device half*)"
                     "(p%d + y * buf_rbs[%d]) + x*4;\n"
                     "%s    v%d_c = float4(hp[0], hp[1], hp[2], hp[3]);\n"
                     "%s} else if (fmt_p%d == %du) {\n"
                     "%s    device uchar *row = p%d + y * buf_rbs[%d];"
                     " uint ps = buf_szs[%d]/4;\n"
                     "%s    v%d_c = float4("
                     "float(((device half*)row)[x]),"
                     "float(((device half*)(row+ps))[x]),"
                     "float(((device half*)(row+2*ps))[x]),"
                     "float(((device half*)(row+3*ps))[x]));\n"
                     "%s} else {\n"
                     "%s    v%d_c = float4(0);\n"
                     "%s}\n"
                     "%sfloat v%d = v%d_c.x;\n"
                     "%sfloat v%d_1 = v%d_c.y;\n"
                     "%sfloat v%d_2 = v%d_c.z;\n"
                     "%sfloat v%d_3 = v%d_c.w;\n",
                     pad, i,
                     pad, p,
                     pad, i, p,
                     pad, p,
                     pad, i, p, p, p, p,
                     pad, p, umbra_fmt_8888,
                     pad, i, p, p,
                     pad, p, umbra_fmt_565,
                     pad, p, p,
                     pad, i,
                     pad, p, umbra_fmt_1010102,
                     pad, i, p, p,
                     pad, p, umbra_fmt_fp16,
                     pad, p, p,
                     pad, i,
                     pad, p, umbra_fmt_fp16_planar,
                     pad, p, p, p,
                     pad, i,
                     pad,
                     pad, i,
                     pad,
                     pad, i, i,
                     pad, i, i,
                     pad, i, i,
                     pad, i, i);
            } break;
            case op_store_color: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                emit(b,
                     "%sfloat4 sc%d = float4(%s, %s, %s, %s);\n"
                     // Pre-round for fp16 textures: tex.write() to
                     // RGBA16Float truncates float32→float16 instead of
                     // rounding to nearest-even.
                     // E.g. 0x3f59999a (0.850000024) → fp16 0x3acc (0.849609)
                     // instead of the correct 0x3acd (0.850098).
                     "%sif (planes_p%d == 1) {\n"
                     "%s    float4 tw%d = (fmt_p%d == %du || fmt_p%d == %du)"
                     " ? float4(half4(sc%d)) : sc%d;\n"
                     "%s    tex_p%d_0.write(tw%d, uint2(x,y));\n"
                     "%s} else if (planes_p%d == 4) {\n"
                     "%s    half4 tw%d = half4(sc%d);\n"
                     "%s    tex_p%d_0.write(float4(tw%d.x,0,0,0), uint2(x,y));\n"
                     "%s    tex_p%d_1.write(float4(tw%d.y,0,0,0), uint2(x,y));\n"
                     "%s    tex_p%d_2.write(float4(tw%d.z,0,0,0), uint2(x,y));\n"
                     "%s    tex_p%d_3.write(float4(tw%d.w,0,0,0), uint2(x,y));\n"
                     "%s} else if (fmt_p%d == %du) {\n"
                     "%s    ((device uint*)(p%d + y * buf_rbs[%d]))[x]"
                     " = pack_float_to_unorm4x8(clamp(sc%d, 0.0, 1.0));\n"
                     "%s} else if (fmt_p%d == %du) {\n"
                     "%s    ((device ushort*)(p%d + y * buf_rbs[%d]))[x]"
                     " = pack_float_to_unorm565("
                     "clamp(sc%d.zyx, 0.0, 1.0));\n"
                     "%s} else if (fmt_p%d == %du) {\n"
                     "%s    ((device uint*)(p%d + y * buf_rbs[%d]))[x]"
                     " = pack_float_to_unorm10a2(clamp(sc%d, 0.0, 1.0));\n"
                     "%s} else if (fmt_p%d == %du) {\n"
                     "%s    device half *hp = (device half*)"
                     "(p%d + y * buf_rbs[%d]) + x*4;\n"
                     "%s    hp[0]=half(sc%d.x); hp[1]=half(sc%d.y);"
                     " hp[2]=half(sc%d.z); hp[3]=half(sc%d.w);\n"
                     "%s} else if (fmt_p%d == %du) {\n"
                     "%s    device uchar *row = p%d + y * buf_rbs[%d];"
                     " uint ps = buf_szs[%d]/4;\n"
                     "%s    ((device half*)row)[x] = half(sc%d.x);"
                     " ((device half*)(row+ps))[x] = half(sc%d.y);"
                     " ((device half*)(row+2*ps))[x] = half(sc%d.z);"
                     " ((device half*)(row+3*ps))[x] = half(sc%d.w);\n"
                     "%s}\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f),
                     fv(_fy, vy, yid, is_f),
                     fv(_fz, vz, zid, is_f),
                     fv(_fw, vw, wid, is_f),
                     pad, p,
                     pad, i, p, umbra_fmt_fp16, p, umbra_fmt_fp16_planar, i, i,
                     pad, p, i,
                     pad, p,
                     pad, i, i,
                     pad, p, i,
                     pad, p, i,
                     pad, p, i,
                     pad, p, i,
                     pad, p, umbra_fmt_8888,
                     pad, p, p, i,
                     pad, p, umbra_fmt_565,
                     pad, p, p, i,
                     pad, p, umbra_fmt_1010102,
                     pad, p, p, i,
                     pad, p, umbra_fmt_fp16,
                     pad, p, p,
                     pad, i, i, i, i,
                     pad, p, umbra_fmt_fp16_planar,
                     pad, p, p, p,
                     pad, i, i, i, i,
                     pad);
            } break;

            case op_load_16: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                emit(b,
                     "%suint v%d = (uint)"
                     "((device ushort*)"
                     "(p%d + y * buf_rbs[%d]))[x];\n",
                     pad, i, p, p);
            } break;
            case op_gather_16: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b, mixed
                    ? "%suint v%d = (uint)(ushort)"
                      "p%d_16[safe_ix((int)%s,"
                      "buf_szs[%d],2)]"
                      " & oob_mask((int)%s,"
                      "buf_szs[%d],2);\n"
                    : "%suint v%d = (uint)"
                      "((device ushort*)p%d)"
                      "[safe_ix((int)%s,"
                      "buf_szs[%d],2)]"
                      " & oob_mask((int)%s,"
                      "buf_szs[%d],2);\n",
                     pad, i, p, vx, p,
                     vx, p);
            } break;
            case op_store_16: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                emit(b,
                     "%s((device ushort*)"
                     "(p%d + y * buf_rbs[%d]))"
                     "[x] = (ushort)%s;\n",
                     pad, p, p,
                     uv(_uy, vy, yid, is_f));
            } break;

            case op_f32_from_f16:
                emit(b,
                    "%sfloat v%d ="
                    " (float)as_type<half>"
                    "((ushort)%s);\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f));
                break;
            case op_f16_from_f32:
                emit(b,
                    "%suint v%d = (uint)"
                    "as_type<ushort>"
                    "((half)%s);\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f));
                break;

            case op_i32_from_s16:
                emit(b,
                    "%suint v%d = (uint)(int)"
                    "(short)(ushort)%s;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f));
                break;
            case op_i32_from_u16:
                emit(b,
                    "%suint v%d = (uint)"
                    "(ushort)%s;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f));
                break;
            case op_i16_from_i32:
                emit(b,
                    "%suint v%d = (uint)"
                    "(ushort)%s;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f));
                break;

            case op_shl_i32_imm:
                emit(b, "%suint v%d = %s << %du;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     inst->imm);
                break;
            case op_shr_u32_imm:
                emit(b, "%suint v%d = %s >> %du;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     inst->imm);
                break;
            case op_shr_s32_imm:
                emit(b,
                    "%suint v%d ="
                    " (uint)((int)%s >> %d);\n",
                    pad, i,
                    uv(_ux, vx, xid, is_f),
                    inst->imm);
                break;
            case op_and_32_imm:
                emit(b, "%suint v%d = %s & %uu;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     (uint32_t)inst->imm);
                break;

            case op_pack:
                emit(b,
                    "%suint v%d ="
                    " %s | (%s << %du);\n",
                    pad, i,
                    uv(_ux, vx, xid, is_f),
                    uv(_uy, vy, yid, is_f),
                    inst->imm);
                break;

            case op_add_f32_imm:
                emit(b, "%sfloat v%d = %s"
                        " + as_type<float>(%uu);\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f),
                     (uint32_t)inst->imm);
                break;
            case op_sub_f32_imm:
                emit(b, "%sfloat v%d = %s"
                        " - as_type<float>(%uu);\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f),
                     (uint32_t)inst->imm);
                break;
            case op_mul_f32_imm:
                emit(b, "%sfloat v%d = %s"
                        " * as_type<float>(%uu);\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f),
                     (uint32_t)inst->imm);
                break;
            case op_div_f32_imm:
                emit(b, "%sfloat v%d = %s"
                        " / as_type<float>(%uu);\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f),
                     (uint32_t)inst->imm);
                break;
            case op_min_f32_imm:
                emit(b, "%sfloat v%d ="
                        " min(%s,"
                        " as_type<float>(%uu));\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f),
                     (uint32_t)inst->imm);
                break;
            case op_max_f32_imm:
                emit(b, "%sfloat v%d ="
                        " max(%s,"
                        " as_type<float>(%uu));\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f),
                     (uint32_t)inst->imm);
                break;
            case op_add_i32_imm:
                emit(b, "%suint v%d = %s + %uu;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     (uint32_t)inst->imm);
                break;
            case op_sub_i32_imm:
                emit(b, "%suint v%d = %s - %uu;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     (uint32_t)inst->imm);
                break;
            case op_mul_i32_imm:
                emit(b, "%suint v%d = %s * %uu;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     (uint32_t)inst->imm);
                break;
            case op_or_32_imm:
                emit(b, "%suint v%d = %s | %uu;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     (uint32_t)inst->imm);
                break;
            case op_xor_32_imm:
                emit(b, "%suint v%d = %s ^ %uu;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     (uint32_t)inst->imm);
                break;
            case op_eq_f32_imm:
                emit(b, "%suint v%d = "
                        "%s == as_type<float>(%uu)"
                        " ? 0xffffffffu : 0u;\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f),
                     (uint32_t)inst->imm);
                break;
            case op_lt_f32_imm:
                emit(b, "%suint v%d = "
                        "%s <  as_type<float>(%uu)"
                        " ? 0xffffffffu : 0u;\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f),
                     (uint32_t)inst->imm);
                break;
            case op_le_f32_imm:
                emit(b, "%suint v%d = "
                        "%s <= as_type<float>(%uu)"
                        " ? 0xffffffffu : 0u;\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f),
                     (uint32_t)inst->imm);
                break;
            case op_eq_i32_imm:
                emit(b, "%suint v%d = "
                        "(int)%s == (int)%uu"
                        " ? 0xffffffffu : 0u;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     (uint32_t)inst->imm);
                break;
            case op_lt_s32_imm:
                emit(b, "%suint v%d = "
                        "(int)%s <  (int)%uu"
                        " ? 0xffffffffu : 0u;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     (uint32_t)inst->imm);
                break;
            case op_le_s32_imm:
                emit(b, "%suint v%d = "
                        "(int)%s <= (int)%uu"
                        " ? 0xffffffffu : 0u;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     (uint32_t)inst->imm);
                break;
            case op_add_f32:
                emit(b, "%sfloat v%d = %s + %s;\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f),
                     fv(_fy, vy, yid, is_f));
                break;
            case op_sub_f32:
                emit(b, "%sfloat v%d = %s - %s;\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f),
                     fv(_fy, vy, yid, is_f));
                break;
            case op_mul_f32:
                emit(b, "%sfloat v%d = %s * %s;\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f),
                     fv(_fy, vy, yid, is_f));
                break;
            case op_div_f32:
                emit(b, "%sfloat v%d = %s / %s;\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f),
                     fv(_fy, vy, yid, is_f));
                break;
            case op_min_f32:
                emit(b, "%sfloat v%d ="
                        " min(%s, %s);\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f),
                     fv(_fy, vy, yid, is_f));
                break;
            case op_max_f32:
                emit(b, "%sfloat v%d ="
                        " max(%s, %s);\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f),
                     fv(_fy, vy, yid, is_f));
                break;
            case op_sqrt_f32:
                emit(b, "%sfloat v%d ="
                        " sqrt(%s);\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f));
                break;
            case op_abs_f32:
                emit(b, "%sfloat v%d ="
                        " fabs(%s);\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f));
                break;
            case op_neg_f32:
                emit(b, "%sfloat v%d ="
                        " -%s;\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f));
                break;
            case op_round_f32:
                emit(b, "%sfloat v%d ="
                        " rint(%s);\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f));
                break;
            case op_floor_f32:
                emit(b, "%sfloat v%d ="
                        " floor(%s);\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f));
                break;
            case op_ceil_f32:
                emit(b, "%sfloat v%d ="
                        " ceil(%s);\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f));
                break;
            case op_round_i32:
                emit(b, "%suint v%d ="
                        " (uint)(int)rint(%s);\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f));
                break;
            case op_floor_i32:
                emit(b, "%suint v%d ="
                        " (uint)(int)floor(%s);\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f));
                break;
            case op_ceil_i32:
                emit(b, "%suint v%d ="
                        " (uint)(int)ceil(%s);\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f));
                break;
            case op_fma_f32:
                emit(b, "%sfloat v%d ="
                        " fma(%s, %s, %s);\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f),
                     fv(_fy, vy, yid, is_f),
                     fv(_fz, vz, zid, is_f));
                break;
            case op_fms_f32:
                emit(b, "%sfloat v%d ="
                        " fma(-%s, %s, %s);\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f),
                     fv(_fy, vy, yid, is_f),
                     fv(_fz, vz, zid, is_f));
                break;

            case op_add_i32:
                emit(b, "%suint v%d = %s + %s;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     uv(_uy, vy, yid, is_f));
                break;
            case op_sub_i32:
                emit(b, "%suint v%d = %s - %s;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     uv(_uy, vy, yid, is_f));
                break;
            case op_mul_i32:
                emit(b, "%suint v%d = %s * %s;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     uv(_uy, vy, yid, is_f));
                break;
            case op_shl_i32:
                emit(b, "%suint v%d = %s << %s;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     uv(_uy, vy, yid, is_f));
                break;
            case op_shr_u32:
                emit(b, "%suint v%d = %s >> %s;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     uv(_uy, vy, yid, is_f));
                break;
            case op_shr_s32:
                emit(b, "%suint v%d = "
                        "(uint)((int)%s"
                        " >> (int)%s);\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     uv(_uy, vy, yid, is_f));
                break;

            case op_and_32:
                emit(b, "%suint v%d = %s & %s;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     uv(_uy, vy, yid, is_f));
                break;
            case op_or_32:
                emit(b, "%suint v%d = %s | %s;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     uv(_uy, vy, yid, is_f));
                break;
            case op_xor_32:
                emit(b, "%suint v%d = %s ^ %s;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     uv(_uy, vy, yid, is_f));
                break;
            case op_sel_32: {
                _Bool yf = is_f[yid], zf = is_f[zid];
                if (yf && zf) {
                    is_f[i] = 1;
                    emit(b, "%sfloat v%d ="
                            " select(%s, %s,"
                            " %s != 0u);\n",
                         pad, i,
                         fv(_fz, vz, zid, is_f),
                         fv(_fy, vy, yid, is_f),
                         uv(_ux, vx, xid, is_f));
                } else {
                    emit(b, "%suint v%d ="
                            " select(%s, %s,"
                            " %s != 0u);\n",
                         pad, i,
                         uv(_uz, vz, zid, is_f),
                         uv(_uy, vy, yid, is_f),
                         uv(_ux, vx, xid, is_f));
                }
            } break;

            case op_f32_from_i32:
                emit(b, "%sfloat v%d ="
                        " (float)(int)%s;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f));
                break;
            case op_i32_from_f32:
                emit(b, "%suint v%d ="
                        " (uint)(int)%s;\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f));
                break;

            case op_eq_f32:
                emit(b, "%suint v%d = "
                        "%s == %s"
                        " ? 0xffffffffu : 0u;\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f),
                     fv(_fy, vy, yid, is_f));
                break;
            case op_lt_f32:
                emit(b, "%suint v%d = "
                        "%s <  %s"
                        " ? 0xffffffffu : 0u;\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f),
                     fv(_fy, vy, yid, is_f));
                break;
            case op_le_f32:
                emit(b, "%suint v%d = "
                        "%s <= %s"
                        " ? 0xffffffffu : 0u;\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f),
                     fv(_fy, vy, yid, is_f));
                break;

            case op_eq_i32:
                emit(b, "%suint v%d = "
                        "(int)%s == (int)%s"
                        " ? 0xffffffffu : 0u;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     uv(_uy, vy, yid, is_f));
                break;
            case op_lt_s32:
                emit(b, "%suint v%d = "
                        "(int)%s <  (int)%s"
                        " ? 0xffffffffu : 0u;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     uv(_uy, vy, yid, is_f));
                break;
            case op_le_s32:
                emit(b, "%suint v%d = "
                        "(int)%s <= (int)%s"
                        " ? 0xffffffffu : 0u;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     uv(_uy, vy, yid, is_f));
                break;
            case op_lt_u32:
                emit(b, "%suint v%d = "
                        "%s <  %s"
                        " ? 0xffffffffu : 0u;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     uv(_uy, vy, yid, is_f));
                break;
            case op_le_u32:
                emit(b, "%suint v%d = "
                        "%s <= %s"
                        " ? 0xffffffffu : 0u;\n",
                     pad, i,
                     uv(_ux, vx, xid, is_f),
                     uv(_uy, vy, yid, is_f));
                break;

        }

        if (is_store(inst->op) && i+1 < hi) {
            emit(b, "\n");
        }
    }
}

static char* build_source(BB const *bb,
                           int *out_max_ptr,
                           int *out_total_bufs,
                           int *out_deref_buf,
                           struct color_buf_info **out_color_bufs,
                           int *out_n_color_bufs,
                           int *out_n_textures) {
    int max_ptr = -1;
    for (int i = 0; i < bb->insts; i++) {
        if (has_ptr(bb->inst[i].op)
                && bb->inst[i].ptr >= 0) {
            if (bb->inst[i].ptr > max_ptr) {
                max_ptr = bb->inst[i].ptr;
            }
        }
    }
    *out_max_ptr = max_ptr;

    int *deref_buf = out_deref_buf;
    int next_buf = max_ptr + 1;
    for (int i = 0; i < bb->insts; i++) {
        if (bb->inst[i].op == op_deref_ptr) {
            deref_buf[i] = next_buf++;
        }
    }
    int total_bufs = next_buf;
    *out_total_bufs = total_bufs;

    _Bool *ptr_16 = calloc((size_t)(total_bufs + 1), 1);
    _Bool *ptr_32 = calloc((size_t)(total_bufs + 1), 1);
    for (int i = 0; i < bb->insts; i++) {
        enum op op = bb->inst[i].op;
        if (has_ptr(op) && op != op_deref_ptr) {
            int p = bb->inst[i].ptr < 0
                ? deref_buf[~bb->inst[i].ptr]
                : bb->inst[i].ptr;
            if (is_32(op)) { ptr_32[p] = 1; }
            else if (is_16(op)) { ptr_16[p] = 1; }
        }
    }

    int n_color_bufs = 0;
    struct color_buf_info *color_bufs = calloc((size_t)(total_bufs + 1), sizeof *color_bufs);
    for (int i = 0; i < bb->insts; i++) {
        enum op op = bb->inst[i].op;
        if (op != op_load_color && op != op_store_color) { continue; }
        int p = bb->inst[i].ptr < 0
            ? deref_buf[~bb->inst[i].ptr]
            : bb->inst[i].ptr;
        int found = -1;
        for (int c = 0; c < n_color_bufs; c++) {
            if (color_bufs[c].buf_idx == p) { found = c; break; }
        }
        if (found < 0) {
            found = n_color_bufs++;
            color_bufs[found].buf_idx = p;
        }
        if (op == op_load_color)  { color_bufs[found].read  = 1; }
        if (op == op_store_color) { color_bufs[found].write = 1; }
    }
    int n_textures = 0;
    for (int c = 0; c < n_color_bufs; c++) {
        color_bufs[c].tex_base = n_textures;
        n_textures += 4;
    }
    *out_color_bufs  = color_bufs;
    *out_n_color_bufs = n_color_bufs;
    *out_n_textures  = n_textures;

    Buf b = {0};

    emit(&b,
         "#include <metal_stdlib>\n"
         "using namespace metal;\n\n");

    int fc = 0;
    for (int c = 0; c < n_color_bufs; c++) {
        int p = color_bufs[c].buf_idx;
        emit(&b,
             "constant uint planes_p%d"
             " [[function_constant(%d)]];\n",
             p, fc++);
        emit(&b,
             "constant uint fmt_p%d"
             " [[function_constant(%d)]];\n",
             p, fc++);
    }
    if (n_color_bufs) {
        emit(&b, "\n");
        emit(&b,
             "enum { FMT_8888=%d, FMT_565=%d,"
             " FMT_1010102=%d, FMT_FP16=%d,"
             " FMT_FP16_PLANAR=%d };\n\n",
             umbra_fmt_8888, umbra_fmt_565,
             umbra_fmt_1010102, umbra_fmt_fp16,
             umbra_fmt_fp16_planar);
    }

    emit(&b,
         "static inline int safe_ix"
         "(int ix, uint bytes, int elem) {\n");
    emit(&b,
         "    int count = (int)"
         "(bytes / (uint)elem);\n");
    emit(&b,
         "    return clamp(ix, 0,"
         " max(count-1, 0));\n}\n");
    emit(&b,
         "static inline uint oob_mask"
         "(int ix, uint bytes, int elem) {\n");
    emit(&b,
         "    int count = (int)"
         "(bytes / (uint)elem);\n");
    emit(&b,
         "    return (ix >= 0 && ix < count)"
         " ? ~0u : 0u;\n}\n\n");

    emit(&b, "kernel void umbra_entry(\n");
    emit(&b,
         "    constant uint &w [[buffer(%d)]]"
         ",\n    constant uint *buf_szs [[buffer(%d)]]"
         ",\n    constant uint *buf_rbs [[buffer(%d)]]"
         ",\n    constant uint &x0 [[buffer(%d)]]"
         ",\n    constant uint &y0 [[buffer(%d)]]",
         total_bufs + 0,
         total_bufs + 1,
         total_bufs + 2,
         total_bufs + 3,
         total_bufs + 4);
    for (int p = 0; p <= max_ptr; p++) {
        emit(&b,
             ",\n    device uchar *p%d"
             " [[buffer(%d)]]",
             p, p);
    }
    for (int i = 0; i < bb->insts; i++) {
        if (bb->inst[i].op == op_deref_ptr) {
            emit(&b,
                 ",\n    device uchar *p%d"
                 " [[buffer(%d)]]",
                 deref_buf[i],
                 deref_buf[i]);
        }
    }
    for (int c = 0; c < n_color_bufs; c++) {
        int p = color_bufs[c].buf_idx;
        int tb = color_bufs[c].tex_base;
        char const *acc = (color_bufs[c].read && color_bufs[c].write)
            ? "read_write"
            : color_bufs[c].write ? "write" : "read";
        for (int t = 0; t < 4; t++) {
            emit(&b,
                 ",\n    texture2d<float, access::%s>"
                 " tex_p%d_%d [[texture(%d)]]",
                 acc, p, t, tb + t);
        }
    }
    emit(&b,
         ",\n    uint2 pos"
         " [[thread_position_in_grid]]\n) {\n");
    emit(&b,
         "    if (pos.x >= w) return;\n"
         "    uint x = x0 + pos.x;\n"
         "    uint y = y0 + pos.y;\n");

    for (int p = 0; p < total_bufs; p++) {
        if (ptr_32[p] && ptr_16[p]) {
            emit(&b,
                 "    device uint   *p%d_32"
                 " = (device uint*)p%d;\n",
                 p, p);
            emit(&b,
                 "    device ushort *p%d_16"
                 " = (device ushort*)p%d;\n",
                 p, p);
        }
    }

    _Bool *is_f = calloc((size_t)(bb->insts + 1), 1);
    emit_ops(&b, bb, ptr_16, ptr_32,
             deref_buf, is_f,
             0, bb->insts, "    ");
    emit(&b, "}\n");

    free(is_f);
    free(ptr_16);
    free(ptr_32);

    char *src = malloc((size_t)b.len + 1);
    __builtin_memcpy(src, b.buf, (size_t)b.len);
    src[b.len] = '\0';
    free(b.buf);
    return src;
}

static struct metal_backend* umbra_metal_backend_create(void) {
    @autoreleasepool {
        id<MTLDevice> device =
            MTLCreateSystemDefaultDevice();
        id<MTLCommandQueue> queue = device
            ? [device newCommandQueue] : nil;
        if (device && queue) {
            struct metal_backend *be =
                calloc(1, sizeof *be);
            be->device =
                (__bridge_retained void*)device;
            be->queue =
                (__bridge_retained void*)queue;
            return be;
        }
        return 0;
    }
}

static void umbra_metal_backend_free(struct metal_backend *be) {
    if (be) {
        @autoreleasepool {
            if (be->device) {
                (void)(__bridge_transfer id)be->device;
            }
            if (be->queue) {
                (void)(__bridge_transfer id)be->queue;
            }
        }
        free(be->batch_bufs);
        free(be->batch_copy);
        free(be);
    }
}

static MTLPixelFormat fmt_to_mtl(umbra_fmt fmt) {
    switch (fmt) {
    case umbra_fmt_8888:        return MTLPixelFormatRGBA8Unorm;
    case umbra_fmt_565:         return MTLPixelFormatR16Uint;
    case umbra_fmt_1010102:     return MTLPixelFormatRGB10A2Unorm;
    case umbra_fmt_fp16:        return MTLPixelFormatRGBA16Float;
    case umbra_fmt_fp16_planar: return MTLPixelFormatR16Float;
    }
    return MTLPixelFormatRGBA8Unorm;
}

static int fmt_to_planes(umbra_fmt fmt) {
    switch (fmt) {
    case umbra_fmt_8888:        return 1;
    case umbra_fmt_1010102:     return 1;
    case umbra_fmt_fp16:        return 1;
    case umbra_fmt_fp16_planar: return 4;
    case umbra_fmt_565:         return 0;
    }
    return 0;
}

static id<MTLComputePipelineState> make_pipeline(
    struct umbra_metal *m,
    id<MTLDevice> device,
    uint32_t const *planes,
    uint32_t const *fmts,
    int *out_tg_size
) {
    id<MTLLibrary> lib = (__bridge id<MTLLibrary>)m->library;
    MTLFunctionConstantValues *cv = [MTLFunctionConstantValues new];
    int fc = 0;
    for (int c = 0; c < m->n_color_bufs; c++) {
        [cv setConstantValue:&planes[c] type:MTLDataTypeUInt atIndex:(NSUInteger)fc++];
        [cv setConstantValue:&fmts[c]   type:MTLDataTypeUInt atIndex:(NSUInteger)fc++];
    }
    NSError *error = nil;
    id<MTLFunction> func = [lib newFunctionWithName:@"umbra_entry"
                                     constantValues:cv
                                              error:&error];
    if (!func) { return nil; }
    id<MTLComputePipelineState> pso =
        [device newComputePipelineStateWithFunction:func error:&error];
    if (pso && out_tg_size) {
        *out_tg_size = (int)pso.maxTotalThreadsPerThreadgroup;
    }
    return pso;
}

static id<MTLComputePipelineState> get_pipeline(
    struct umbra_metal *m,
    umbra_buf buf[],
    id<MTLDevice> device,
    int *out_tg_size
) {
    uint32_t planes[64], fmts[64];
    uint64_t key = 0;
    for (int c = 0; c < m->n_color_bufs; c++) {
        int bi = m->color_bufs[c].buf_idx;
        int pl = (bi <= m->max_ptr) ? fmt_to_planes(buf[bi].fmt) : 0;
        uint32_t fm = (bi <= m->max_ptr) ? (uint32_t)buf[bi].fmt : 0;
        if (pl > 0 && bi <= m->max_ptr) {
            if (buf[bi].row_bytes == 0) { pl = 0; }
            else {
                NSUInteger min_align = [device
                    minimumLinearTextureAlignmentForPixelFormat:
                        fmt_to_mtl(buf[bi].fmt)];
                if (min_align > 0 && (buf[bi].row_bytes % min_align) != 0) {
                    pl = 0;
                }
            }
        }
        planes[c] = (uint32_t)pl;
        fmts[c]   = fm;
        key = key * 131 + (uint64_t)pl * 7 + fm;
    }
    for (int i = 0; i < m->n_pipelines; i++) {
        if (m->pipelines[i].key == key) {
            *out_tg_size = m->pipelines[i].tg_size;
            return (__bridge id<MTLComputePipelineState>)m->pipelines[i].pipeline;
        }
    }
    int tg = 0;
    id<MTLComputePipelineState> pso = make_pipeline(m, device, planes, fmts, &tg);
    if (!pso) { return nil; }
    if (m->n_pipelines >= m->pipelines_cap) {
        m->pipelines_cap = m->pipelines_cap ? 2 * m->pipelines_cap : 8;
        m->pipelines = realloc(m->pipelines,
                               (size_t)m->pipelines_cap * sizeof *m->pipelines);
    }
    m->pipelines[m->n_pipelines++] = (struct pipeline_entry){
        .key=key, .pipeline=(__bridge_retained void*)pso, .tg_size=tg,
    };
    *out_tg_size = tg;
    return pso;
}

static struct umbra_metal* umbra_metal(
    struct metal_backend *be, BB const *bb
) {
    if (!be) { return 0; }

    struct umbra_metal *result = 0;
    @autoreleasepool {
        id<MTLDevice> device = (__bridge id<MTLDevice>)be->device;

        int *deref_buf = calloc((size_t)bb->insts, sizeof *deref_buf);
        int  max_ptr = -1, total_bufs = 0;
        struct color_buf_info *color_bufs = 0;
        int n_color_bufs = 0, n_textures = 0;
        char *src = build_source(bb, &max_ptr, &total_bufs, deref_buf,
                                 &color_bufs, &n_color_bufs, &n_textures);

        int n_deref = 0;
        for (int i = 0; i < bb->insts; i++) {
            if (bb->inst[i].op == op_deref_ptr) {
                n_deref++;
            }
        }
        struct deref_info *di = calloc((size_t)(n_deref ? n_deref : 1), sizeof *di);
        {
            int d = 0;
            for (int i = 0; i < bb->insts; i++) {
                if (bb->inst[i].op == op_deref_ptr) {
                    di[d].buf_idx  = deref_buf[i];
                    di[d].src_buf  = bb->inst[i].ptr;
                    di[d].byte_off = bb->inst[i].imm;
                    d++;
                }
            }
        }

        NSString *source = [NSString stringWithUTF8String:src];
        NSError *error = nil;
        MTLCompileOptions *opts = [MTLCompileOptions new];
        if (@available(macOS 15.0, *)) {
            opts.mathMode = MTLMathModeSafe;
        } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            opts.fastMathEnabled = NO;
#pragma clang diagnostic pop
        }

        id<MTLLibrary> library;
        struct umbra_metal *m;

        library = [device newLibraryWithSource:source options:opts error:&error];
        if (!library) {
            NSLog(@"Metal compile error: %@", error);
            goto fail;
        }

        m = calloc(1, sizeof *m);
        m->library       = (__bridge_retained void*)library;
        m->color_bufs    = color_bufs;
        m->n_color_bufs  = n_color_bufs;
        m->n_textures    = n_textures;
        m->src           = src;
        m->max_ptr       = max_ptr;
        m->total_bufs    = total_bufs;
        m->per_bufs      = calloc((size_t)total_bufs, sizeof *m->per_bufs);
        m->deref         = di;
        m->n_deref       = n_deref;

        {
            uint32_t *def_planes = calloc((size_t)(n_color_bufs + 1), sizeof *def_planes);
            uint32_t *def_fmts   = calloc((size_t)(n_color_bufs + 1), sizeof *def_fmts);
            int tg = 0;
            id<MTLComputePipelineState> pso = make_pipeline(m, device, def_planes, def_fmts, &tg);
            free(def_planes);
            free(def_fmts);
            if (!pso) { goto fail2; }
            m->tg_size = tg;
            m->pipelines_cap = 8;
            m->pipelines = calloc((size_t)m->pipelines_cap, sizeof *m->pipelines);
            m->pipelines[0] = (struct pipeline_entry){
                .key=0, .pipeline=(__bridge_retained void*)pso, .tg_size=tg,
            };
            m->n_pipelines = 1;
        }

        free(deref_buf);
        result = m;
        goto out;

    fail2:
        (void)(__bridge_transfer id)m->library;
        free(m->per_bufs);
        free(m);
    fail:
        free(deref_buf);
        free(di);
        free(color_bufs);
        free(src);
    out:;
    }
    return result;
}

static void batch_add_copy(
    struct metal_backend *be,
    void *host, void *raw_ptr, size_t bytes
) {
    if (be->batch_ncopy >= be->batch_copy_cap) {
        be->batch_copy_cap = be->batch_copy_cap
            ? 2 * be->batch_copy_cap : 64;
        be->batch_copy = realloc(
            be->batch_copy,
            (size_t)be->batch_copy_cap
                * sizeof(struct copyback));
    }
    be->batch_copy[be->batch_ncopy++] =
        (struct copyback){host, raw_ptr, bytes};
}

static void batch_retain_buf(
    struct metal_backend *be, void *retained
) {
    if (be->batch_nbufs >= be->batch_bufs_cap) {
        be->batch_bufs_cap = be->batch_bufs_cap
            ? 2 * be->batch_bufs_cap : 64;
        be->batch_bufs = realloc(
            be->batch_bufs,
            (size_t)be->batch_bufs_cap
                * sizeof(void*));
    }
    be->batch_bufs[be->batch_nbufs++] = retained;
}


static void encode_dispatch(
    struct umbra_metal *m,
    int l, int t, int r, int b,
    umbra_buf buf[],
    id<MTLComputeCommandEncoder> enc
) {
    int w = r - l, h = b - t, x0 = l, y0 = t;
    struct metal_backend *be = (struct metal_backend*)m->base.backend;
    id<MTLDevice> device =
        (__bridge id<MTLDevice>)be->device;

    int tg_size = 0;
    id<MTLComputePipelineState> pso = get_pipeline(m, buf, device, &tg_size);
    assert(pso);
    [enc setComputePipelineState:pso];

    int tb = m->total_bufs;
    uint32_t *szs_data  = calloc((size_t)(tb + 1), sizeof *szs_data);
    uint32_t *rbs_data  = calloc((size_t)(tb + 1), sizeof *rbs_data);
    for (int i = 0; i <= m->max_ptr; i++) {
        if (buf[i].ptr && buf[i].sz) {
            szs_data[i] = (uint32_t)buf[i].sz;
        }
        rbs_data[i]  = (uint32_t)buf[i].row_bytes;
    }

    __builtin_memset(m->per_bufs, 0,
                     (size_t)tb * sizeof(void*));

    uint32_t w32 = (uint32_t)w;
    id<MTLBuffer> per_w =
        [device newBufferWithLength:sizeof w32
                options:
                    MTLResourceStorageModeShared];
    *(uint32_t*)per_w.contents = w32;
    batch_retain_buf(
        be, (__bridge_retained void*)per_w);
    uint32_t x032 = (uint32_t)x0;
    id<MTLBuffer> per_x0 =
        [device newBufferWithLength:sizeof x032
                options:
                    MTLResourceStorageModeShared];
    *(uint32_t*)per_x0.contents = x032;
    batch_retain_buf(
        be, (__bridge_retained void*)per_x0);
    uint32_t y032 = (uint32_t)y0;
    id<MTLBuffer> per_y0 =
        [device newBufferWithLength:sizeof y032
                options:
                    MTLResourceStorageModeShared];
    *(uint32_t*)per_y0.contents = y032;
    batch_retain_buf(
        be, (__bridge_retained void*)per_y0);

    size_t offsets[32] = {0};
    for (int i = 0; i <= m->max_ptr; i++) {
        if (!buf[i].ptr || !buf[i].sz) { continue; }
        size_t bytes = buf[i].sz;
        struct batch_shared *sh =
            &m->batch_data[i];
        char *ptr = buf[i].ptr;
        _Bool overlap = sh->mtl
            && ptr >= sh->host
            && ptr <  sh->host + sh->copy_sz;
        if (overlap) {
            offsets[i] = (size_t)(ptr - sh->host);
            m->per_bufs[i] = sh->mtl;
        } else {
            size_t pg = (size_t)sysconf(_SC_PAGESIZE);
            size_t aligned_sz = (bytes + pg - 1) & ~(pg - 1);
            _Bool can_nocopy = ((uintptr_t)ptr & (pg - 1)) == 0;
            id<MTLBuffer> tmp;
            if (can_nocopy) {
                tmp = [device
                    newBufferWithBytesNoCopy:ptr
                                     length:(NSUInteger)aligned_sz
                                    options:MTLResourceStorageModeShared
                                deallocator:nil];
            } else {
                tmp = [device
                    newBufferWithLength:(NSUInteger)bytes
                                options:MTLResourceStorageModeShared];
                __builtin_memcpy(tmp.contents, ptr, bytes);
            }
            void *retained =
                (__bridge_retained void*)tmp;
            if (!sh->mtl) {
                sh->mtl     = retained;
                sh->host    = ptr;
                sh->copy_sz = !buf[i].read_only
                    ? bytes : 0;
            }
            batch_retain_buf(be, retained);
            if (!buf[i].read_only && !can_nocopy) {
                batch_add_copy(
                    be, ptr,
                    retained, bytes);
            }
            offsets[i] = 0;
            m->per_bufs[i] = retained;
        }
    }

    for (int d = 0; d < m->n_deref; d++) {
        void *base = buf[m->deref[d].src_buf].ptr;
        void *derived;
        ptrdiff_t dsz;
        size_t drb;
        __builtin_memcpy(
            &derived,
            (char*)base + m->deref[d].byte_off,
            sizeof derived);
        __builtin_memcpy(
            &dsz,
            (char*)base + m->deref[d].byte_off + 8,
            sizeof dsz);
        __builtin_memcpy(
            &drb,
            (char*)base + m->deref[d].byte_off + 16,
            sizeof drb);
        size_t bytes = dsz < 0 ? (size_t)-dsz : (size_t)dsz;
        _Bool deref_read_only = dsz < 0;
        int bi = m->deref[d].buf_idx;
        struct batch_shared *sh =
            &m->batch_data[bi];
        char *dptr = derived;
        _Bool overlap = sh->mtl
            && dptr >= sh->host
            && dptr <  sh->host + sh->copy_sz;
        if (overlap) {
            offsets[bi] = (size_t)(dptr - sh->host);
            m->per_bufs[bi] = sh->mtl;
        } else {
            size_t pg = (size_t)sysconf(_SC_PAGESIZE);
            size_t aligned_sz = (bytes + pg - 1) & ~(pg - 1);
            _Bool can_nocopy = ((uintptr_t)dptr & (pg - 1)) == 0;
            id<MTLBuffer> tmp;
            if (can_nocopy) {
                tmp = [device
                    newBufferWithBytesNoCopy:dptr
                                     length:(NSUInteger)aligned_sz
                                    options:MTLResourceStorageModeShared
                                deallocator:nil];
            } else {
                tmp = [device
                    newBufferWithLength:(NSUInteger)bytes
                                options:MTLResourceStorageModeShared];
                __builtin_memcpy(tmp.contents, dptr, bytes);
            }
            void *retained =
                (__bridge_retained void*)tmp;
            if (!sh->mtl) {
                sh->mtl     = retained;
                sh->host    = dptr;
                sh->copy_sz = !deref_read_only
                    ? bytes : 0;
            }
            batch_retain_buf(be, retained);
            if (!deref_read_only && !can_nocopy) {
                batch_add_copy(
                    be, dptr,
                    retained, bytes);
            }
            offsets[bi] = 0;
            m->per_bufs[bi] = retained;
        }
        szs_data[bi] = (uint32_t)bytes;
        rbs_data[bi] = (uint32_t)drb;
    }

    size_t sz_bytes = (size_t)(tb + 1)
                    * sizeof(uint32_t);
    id<MTLBuffer> per_sz =
        [device newBufferWithLength:sz_bytes
                options:
                    MTLResourceStorageModeShared];
    __builtin_memcpy(
        per_sz.contents, szs_data, sz_bytes);
    batch_retain_buf(
        be, (__bridge_retained void*)per_sz);
    id<MTLBuffer> per_rbs =
        [device newBufferWithLength:sz_bytes
                options:
                    MTLResourceStorageModeShared];
    __builtin_memcpy(
        per_rbs.contents, rbs_data, sz_bytes);
    batch_retain_buf(
        be, (__bridge_retained void*)per_rbs);
    for (int i = 0; i < m->total_bufs; i++) {
        if (m->per_bufs[i]) {
            [enc setBuffer:
                (__bridge id<MTLBuffer>)
                    m->per_bufs[i]
                offset:(NSUInteger)offsets[i]
               atIndex:(NSUInteger)i];
        }
    }
    [enc setBuffer:per_w
            offset:0
           atIndex:(NSUInteger)(m->total_bufs + 0)];
    [enc setBuffer:per_sz
            offset:0
           atIndex:(NSUInteger)(m->total_bufs + 1)];
    [enc setBuffer:per_rbs
            offset:0
           atIndex:(NSUInteger)(m->total_bufs + 2)];
    [enc setBuffer:per_x0
            offset:0
           atIndex:(NSUInteger)(m->total_bufs + 3)];
    [enc setBuffer:per_y0
            offset:0
           atIndex:(NSUInteger)(m->total_bufs + 4)];

    for (int c = 0; c < m->n_color_bufs; c++) {
        int bi = m->color_bufs[c].buf_idx;
        if (bi > m->max_ptr || !m->per_bufs[bi]) { continue; }
        int planes = fmt_to_planes(buf[bi].fmt);
        if (planes <= 0) { continue; }
        if (buf[bi].row_bytes == 0) { continue; }
        NSUInteger min_align = [device
            minimumLinearTextureAlignmentForPixelFormat:
                fmt_to_mtl(buf[bi].fmt)];
        if (min_align > 0 && (buf[bi].row_bytes % min_align) != 0) { continue; }

        id<MTLBuffer> mtlBuf =
            (__bridge id<MTLBuffer>)m->per_bufs[bi];
        size_t pb = umbra_fmt_size(buf[bi].fmt);
        int tex_base = m->color_bufs[c].tex_base;

        if (planes == 1) {
            NSUInteger tw = (NSUInteger)(buf[bi].row_bytes / pb);
            NSUInteger th = (NSUInteger)(buf[bi].sz / buf[bi].row_bytes);
            MTLTextureDescriptor *desc = [MTLTextureDescriptor
                texture2DDescriptorWithPixelFormat:fmt_to_mtl(buf[bi].fmt)
                                             width:tw
                                            height:th
                                         mipmapped:NO];
            desc.usage = (MTLTextureUsage)(MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite);
            desc.storageMode = MTLStorageModeShared;
            id<MTLTexture> tex = [mtlBuf
                newTextureWithDescriptor:desc
                                  offset:(NSUInteger)offsets[bi]
                             bytesPerRow:(NSUInteger)buf[bi].row_bytes];
            if (tex) {
                [enc setTexture:tex atIndex:(NSUInteger)tex_base];
                batch_retain_buf(be, (__bridge_retained void*)tex);
            }
        } else if (planes == 4) {
            size_t plane_stride = buf[bi].sz / 4;
            NSUInteger tw = (NSUInteger)(buf[bi].row_bytes / 2);
            NSUInteger th = (NSUInteger)(plane_stride / buf[bi].row_bytes);
            for (int p = 0; p < 4; p++) {
                MTLTextureDescriptor *desc = [MTLTextureDescriptor
                    texture2DDescriptorWithPixelFormat:MTLPixelFormatR16Float
                                                 width:tw
                                                height:th
                                             mipmapped:NO];
                desc.usage = (MTLTextureUsage)(MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite);
                desc.storageMode = MTLStorageModeShared;
                id<MTLTexture> tex = [mtlBuf
                    newTextureWithDescriptor:desc
                                      offset:(NSUInteger)(offsets[bi] + (size_t)p * plane_stride)
                                 bytesPerRow:(NSUInteger)buf[bi].row_bytes];
                if (tex) {
                    [enc setTexture:tex atIndex:(NSUInteger)(tex_base + p)];
                    batch_retain_buf(be, (__bridge_retained void*)tex);
                }
            }
        }
    }

    MTLSize grid =
        MTLSizeMake((NSUInteger)w, (NSUInteger)h, 1);
    int gx = 1, gy = 1;
    for (int x = 1; x * x <= tg_size; x++) {
        if (tg_size % x != 0) { continue; }
        int y = tg_size / x;
        if (x <= w && y <= h) { gx = x; gy = y; }
        if (y <= w && x <= h) { gx = y; gy = x; }
    }
    MTLSize group =
        MTLSizeMake((NSUInteger)gx,
                    (NSUInteger)gy, 1);
    [enc dispatchThreads:grid
       threadsPerThreadgroup:group];
    free(szs_data);
    free(rbs_data);
}

static void umbra_metal_begin_batch(struct metal_backend *be);

static void umbra_metal_run(
    struct umbra_metal *m, int l, int t, int r, int b, umbra_buf buf[]
) {
    int w = r - l, h = b - t;
    assert(m);
    if (w > 0 && h > 0) {
        struct metal_backend *be = (struct metal_backend*)m->base.backend;
        if (!be->batch_cmdbuf) {
            umbra_metal_begin_batch(be);
        }
        @autoreleasepool {
            id<MTLComputeCommandEncoder> enc =
                (__bridge
                 id<MTLComputeCommandEncoder>)
                    be->batch_enc;
            if (!m->batch_data) {
                m->batch_data = calloc(
                    (size_t)m->total_bufs,
                    sizeof *m->batch_data);
            }
            if (m->batch_gen != be->batch_gen) {
                m->batch_gen = be->batch_gen;
                __builtin_memset(
                    m->batch_data, 0,
                    (size_t)m->total_bufs
                        * sizeof *m->batch_data);
            }
            encode_dispatch(
                m, l, t, r, b, buf, enc);
        }
    }
}

static void umbra_metal_begin_batch(struct metal_backend *be) {
    if (be && !be->batch_cmdbuf) {
        be->batch_gen++;
        @autoreleasepool {
            id<MTLCommandQueue> queue =
                (__bridge id<MTLCommandQueue>)
                    be->queue;
            id<MTLCommandBuffer> cmdbuf =
                [queue
                 commandBufferWithUnretainedReferences];
            id<MTLComputeCommandEncoder> enc =
                [cmdbuf computeCommandEncoder];
            be->batch_cmdbuf =
                (__bridge_retained void*)cmdbuf;
            be->batch_enc =
                (__bridge_retained void*)enc;
        }
    }
}

static void umbra_metal_flush(struct metal_backend *be) {
    if (be && be->batch_cmdbuf) {
        @autoreleasepool {
            id<MTLComputeCommandEncoder> enc =
                (__bridge_transfer
                 id<MTLComputeCommandEncoder>)
                    be->batch_enc;
            id<MTLCommandBuffer> cmdbuf =
                (__bridge_transfer id<MTLCommandBuffer>)
                    be->batch_cmdbuf;
            be->batch_enc    = NULL;
            be->batch_cmdbuf = NULL;

            [enc endEncoding];
            [cmdbuf commit];
            [cmdbuf waitUntilCompleted];

            for (int i = 0; i < be->batch_ncopy; i++) {
                struct copyback *c =
                    &be->batch_copy[i];
                id<MTLBuffer> mtlbuf =
                    (__bridge id<MTLBuffer>)c->mtlbuf;
                __builtin_memcpy(
                    c->host,
                    mtlbuf.contents,
                    (size_t)c->bytes);
            }
            be->batch_ncopy = 0;

            for (int i = 0; i < be->batch_nbufs; i++) {
                (void)(__bridge_transfer id)
                    be->batch_bufs[i];
            }
            be->batch_nbufs = 0;
        }
    }
}

static void umbra_metal_free(struct umbra_metal *m) {
    assert(m);
    @autoreleasepool {
        if (m->library) {
            (void)(__bridge_transfer id)m->library;
        }
        for (int i = 0; i < m->n_pipelines; i++) {
            if (m->pipelines[i].pipeline) {
                (void)(__bridge_transfer id)m->pipelines[i].pipeline;
            }
        }
    }
    free(m->pipelines);
    free(m->color_bufs);
    free(m->per_bufs);
    free(m->deref);
    free(m->batch_data);
    free(m->src);
    free(m);
}

static void umbra_dump_metal(
    struct umbra_metal const *m, FILE *f
) {
    assert(m);
    if (m->src) {
        fputs(m->src, f);
    }
}

static void run_metal(struct umbra_program *prog, int l, int t, int r, int b, umbra_buf buf[]) {
    umbra_metal_run((struct umbra_metal*)prog, l, t, r, b, buf);
}
static void dump_metal(struct umbra_program const *prog, FILE *f) { umbra_dump_metal((struct umbra_metal const*)prog, f); }
static void free_metal(struct umbra_program *prog) { umbra_metal_free((struct umbra_metal*)prog); }
static struct umbra_program *compile_metal(struct umbra_backend           *be,
                                           struct umbra_basic_block const *bb) {
    struct umbra_metal *const m = umbra_metal((struct metal_backend*)be, bb);
    assert(m);
    m->base = (struct umbra_program){
        .queue   = run_metal,
        .dump    = dump_metal,
        .free    = free_metal,
        .backend = be,
    };
    return &m->base;
}
static void flush_be_metal(struct umbra_backend *be) { umbra_metal_flush((struct metal_backend*)be); }
static void free_be_metal(struct umbra_backend *be) {
    struct metal_backend *mbe = (struct metal_backend*)be;
    umbra_metal_flush(mbe);
    umbra_metal_backend_free(mbe);
}
struct umbra_backend *umbra_backend_metal(void) {
    struct metal_backend *const mbe = umbra_metal_backend_create();
    if (mbe) {
        mbe->base = (struct umbra_backend){
            .compile = compile_metal,
            .flush   = flush_be_metal,
            .free    = free_be_metal,
        };
        return &mbe->base;
    }
    return 0;
}

#endif
