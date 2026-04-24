#include "assume.h"
#include "flat_ir.h"
#include "gpu_buf_cache.h"
#include "uniform_ring.h"

#if !defined(__APPLE__) || defined(__wasm__)

struct umbra_backend* umbra_backend_metal(void) { return 0; }

#else

#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef void* id;
typedef void* SEL;
typedef unsigned long NSUInteger;
typedef struct { NSUInteger width, height, depth; } MTLSize;
extern void* MTLCreateSystemDefaultDevice(void);
extern void* objc_autoreleasePoolPush(void);
extern void  objc_autoreleasePoolPop(void*);
// Typed declarations of objc_msgSend, one per distinct calling convention
// used in this file.  __asm__ labels alias them all to _objc_msgSend so the
// linker sees one symbol.  This avoids casting through incompatible function
// pointers (which GCC 15 warns about and can't be suppressed), and it avoids
// passing arguments through variadics (which uses a different ABI on arm64
// and crashes at runtime).
//
extern id      msg       (id, SEL)                                     __asm__("_objc_msgSend");
extern double  msg_f64   (id, SEL)                                     __asm__("_objc_msgSend");
extern id      msg_s     (id, SEL, char const*)                        __asm__("_objc_msgSend");
extern id      msg_u     (id, SEL, NSUInteger)                         __asm__("_objc_msgSend");
extern id      msg_uu    (id, SEL, NSUInteger, NSUInteger)             __asm__("_objc_msgSend");
extern id      msg_p     (id, SEL, id)                                 __asm__("_objc_msgSend");
extern id      msg_pp    (id, SEL, id, id*)                            __asm__("_objc_msgSend");
extern id      msg_ppp   (id, SEL, id, id, id*)                        __asm__("_objc_msgSend");
extern id      msg_vuup  (id, SEL, void*, NSUInteger, NSUInteger, id)  __asm__("_objc_msgSend");
extern void    msg_v_p   (id, SEL, id)                                 __asm__("_objc_msgSend");
extern void    msg_v_vuu (id, SEL, void*, NSUInteger, NSUInteger)      __asm__("_objc_msgSend");
extern void    msg_v_puu (id, SEL, id, NSUInteger, NSUInteger)         __asm__("_objc_msgSend");
extern void    msg_v_ss  (id, SEL, MTLSize, MTLSize)                   __asm__("_objc_msgSend");

extern void* objc_getClass(char const*);
extern void* sel_registerName(char const*);

enum {
    MTLResourceStorageModeShared = 0,
    MTLDispatchTypeSerial        = 0,
};

static SEL sel(char const *name) { return sel_registerName(name); }
static void  release (void *obj) { (void)msg(obj, sel("release")); }
static id    retain  (id obj)   { return msg(obj, sel("retain")); }
static void* contents(void *buf){ return msg(buf, sel("contents")); }
static id nsstr(char const *s) {
    return msg_s((id)objc_getClass("NSString"), sel("stringWithUTF8String:"), s);
}

// Selectors hit every dispatch / batch / submit.  Populated once in
// metal_backend_create(); sel_registerName is idempotent across backend
// instances so the cache is process-global.
static SEL SEL_setComputePipelineState,
           SEL_setBytes_length_atIndex,
           SEL_setBuffer_offset_atIndex,
           SEL_dispatchThreads_threadsPerThreadgroup,
           SEL_commandBuffer,
           SEL_computeCommandEncoderWithDispatchType,
           SEL_endEncoding,
           SEL_commit;

static void init_sel_cache(void) {
    SEL_setComputePipelineState               = sel("setComputePipelineState:");
    SEL_setBytes_length_atIndex               = sel("setBytes:length:atIndex:");
    SEL_setBuffer_offset_atIndex              = sel("setBuffer:offset:atIndex:");
    SEL_dispatchThreads_threadsPerThreadgroup = sel("dispatchThreads:threadsPerThreadgroup:");
    SEL_commandBuffer                         = sel("commandBuffer");
    SEL_computeCommandEncoderWithDispatchType = sel("computeCommandEncoderWithDispatchType:");
    SEL_endEncoding                           = sel("endEncoding");
    SEL_commit                                = sel("commit");
}

typedef struct umbra_flat_ir IR;

enum { METAL_N_FRAMES = 2 };

struct metal_backend {
    struct umbra_backend base;
    void *device;
    void *queue;
    void *batch_cmdbuf;     // currently-encoding cmdbuf for uni_pool.cur, or NULL
    void *batch_enc;        // serial compute encoder on batch_cmdbuf, or NULL
    void *batch_pipeline;   // last pipeline set on batch_enc, or NULL
    void *frame_committed[METAL_N_FRAMES];  // last committed cmdbuf per frame, or NULL
    struct gpu_buf_cache cache;
    struct uniform_ring_pool uni_pool;
    double gpu_time_accum;
    int    total_dispatches;
    int    total_submits;
};

struct metal_program {
    struct umbra_program base;
    void *pipeline;
    char *src;
    struct buffer_metadata *buf;
    int    total_bufs;
    int    bindings;
    struct buffer_binding *binding;
};

typedef struct {
    char  *text;
    size_t size, cap;
} SrcBuf;

__attribute__((format(printf, 2, 3)))
static void emit(SrcBuf *b, char const *fmt, ...) {
    va_list ap;
    for (;;) {
        va_start(ap, fmt);
        int n = vsnprintf(b->text + b->size,
                          b->cap - b->size,
                          fmt, ap);
        va_end(ap);
        if (b->size + (size_t)n < b->cap) {
            b->size += (size_t)n;
            return;
        }
        b->cap = b->cap ? 2*b->cap : 4096;
        b->text = realloc(b->text, b->cap);
    }
}

static _Bool produces_float(enum op op) {
    return op == op_add_f32     || op == op_sub_f32
        || op == op_mul_f32
        || op == op_min_f32     || op == op_max_f32
        || op == op_abs_f32     || op == op_square_f32
        || op == op_round_f32   || op == op_floor_f32  || op == op_ceil_f32
        || op == op_fma_f32     || op == op_fms_f32
        || op == op_square_add_f32 || op == op_square_sub_f32
        || op == op_f32_from_i32
        || op == op_f32_from_f16;
}

static char const* fv(char *tmp, char const *vn,
                      int id, _Bool const *is_f) {
    if (!is_f[id]) {
        snprintf(tmp, 40, "as_type<float>(%s)", vn);
        return tmp;
    }
    return vn;
}
static char const* uv(char *tmp, char const *vn,
                      int id, _Bool const *is_f) {
    if (is_f[id]) {
        snprintf(tmp, 40, "as_type<uint>(%s)", vn);
        return tmp;
    }
    return vn;
}

static void emit_ops(SrcBuf *b, IR const *ir,
                     _Bool *is_f,
                     int lo, int hi, char const *pad) {
    for (int i = lo; i < hi; i++) {
        is_f[i] = produces_float(ir->inst[i].op);
    }

    char vx[16], vy[16], vz[16], vw[16];
#define VNAME(buf, vid, ch) \
    ((ch) ? (void)snprintf(buf, sizeof buf, "v%d_%d", (vid), (ch)) \
           : (void)snprintf(buf, sizeof buf, "v%d", (vid)), buf)

    for (int i = lo; i < hi; i++) {
        struct ir_inst const *inst = &ir->inst[i];
        int xid = inst->x.id, yid = inst->y.id,
            zid = inst->z.id, wid = inst->w.id;
        VNAME(vx, xid, (int)inst->x.chan);
        VNAME(vy, yid, (int)inst->y.chan);
        VNAME(vz, zid, (int)inst->z.chan);
        VNAME(vw, wid, (int)inst->w.chan);

        char _fx[40], _fy[40], _fz[40];
        char _ux[40], _uy[40], _uz[40], _uw[40];

        switch (inst->op) {
            case op_x:
                emit(b, "%suint v%d = m.x0 + pos.x;\n", pad, i);
                break;
            case op_y:
                emit(b, "%suint v%d = m.y0 + pos.y;\n", pad, i);
                break;

            case op_imm_32:
                emit(b, "%suint v%d = %uu;\n",
                     pad, i, (uint32_t)inst->imm);
                break;

            case op_uniform_32: {
                int p = inst->ptr.bits;
                emit(b,
                     "%suint v%d = p%d[%d];\n",
                     pad, i, p, inst->imm);
            } break;
            case op_load_32: {
                int p = inst->ptr.bits;
                emit(b,
                     "%suint v%d = p%d"
                     "[y * m.stride%d + x];\n",
                     pad, i, p, p);
            } break;
            case op_gather_uniform_32:
            case op_gather_32: {
                int p = inst->ptr.bits;
                emit(b,
                     "%suint v%d = p%d"
                     "[min(%s, m.count%d - 1u)]"
                     " & ((%s < m.count%d)"
                     " ? ~0u : 0u);\n",
                     pad, i, p,
                     vx, p, vx, p);
            } break;
            case op_store_32: {
                int p = inst->ptr.bits;
                emit(b,
                     "%sp%d[y * m.stride%d + x]"
                     " = %s;\n",
                     pad, p, p,
                     uv(_uy, vy, yid, is_f));
            } break;
            case op_load_16x4: {
                int p = inst->ptr.bits;
                emit(b,
                     "%shalf4 _px%d = p%d"
                     "[y * m.stride%d + x];\n"
                     "%suint v%d = (uint)as_type<ushort>(_px%d.x);\n"
                     "%suint v%d_1 = (uint)as_type<ushort>(_px%d.y);\n"
                     "%suint v%d_2 = (uint)as_type<ushort>(_px%d.z);\n"
                     "%suint v%d_3 = (uint)as_type<ushort>(_px%d.w);\n",
                     pad, i, p, p,
                     pad, i, i,
                     pad, i, i,
                     pad, i, i,
                     pad, i, i);
            } break;
            case op_load_16x4_planar: {
                int p = inst->ptr.bits;
                emit(b,
                     "%suint _row%d = y * m.stride%d;"
                     " uint _ps%d = m.count%d / 4;\n"
                     "%suint v%d = (uint)p%d[_row%d + x];\n"
                     "%suint v%d_1 = (uint)p%d[_row%d + x + _ps%d];\n"
                     "%suint v%d_2 = (uint)p%d[_row%d + x + 2*_ps%d];\n"
                     "%suint v%d_3 = (uint)p%d[_row%d + x + 3*_ps%d];\n",
                     pad, i, p, i, p,
                     pad, i, p, i,
                     pad, i, p, i, i,
                     pad, i, p, i, i,
                     pad, i, p, i, i);
            } break;
            case op_store_16x4: {
                int p = inst->ptr.bits;
                emit(b,
                     "%sp%d[y * m.stride%d + x] ="
                     " half4(as_type<half>((ushort)%s),"
                     " as_type<half>((ushort)%s),"
                     " as_type<half>((ushort)%s),"
                     " as_type<half>((ushort)%s));\n",
                     pad, p, p,
                     uv(_ux, vx, xid, is_f),
                     uv(_uy, vy, yid, is_f),
                     uv(_uz, vz, zid, is_f),
                     uv(_uw, vw, wid, is_f));
            } break;
            case op_store_16x4_planar: {
                int p = inst->ptr.bits;
                emit(b,
                     "%s{ uint _row = y * m.stride%d;"
                     " uint _ps = m.count%d / 4;\n"
                     "%s  p%d[_row + x] = ushort(%s);"
                     " p%d[_row + x + _ps] = ushort(%s);"
                     " p%d[_row + x + 2*_ps] = ushort(%s);"
                     " p%d[_row + x + 3*_ps] = ushort(%s); }\n",
                     pad, p, p,
                     pad, p, uv(_ux, vx, xid, is_f),
                     p, uv(_uy, vy, yid, is_f),
                     p, uv(_uz, vz, zid, is_f),
                     p, uv(_uw, vw, wid, is_f));
            } break;

            case op_load_8x4: {
                int p = inst->ptr.bits;
                emit(b,
                     "%suint px%d = p%d"
                     "[y * m.stride%d + x];\n"
                     "%suint v%d = px%d & 0xFFu;\n"
                     "%suint v%d_1 = (px%d >> 8u) & 0xFFu;\n"
                     "%suint v%d_2 = (px%d >> 16u) & 0xFFu;\n"
                     "%suint v%d_3 = px%d >> 24u;\n",
                     pad, i, p, p,
                     pad, i, i,
                     pad, i, i,
                     pad, i, i,
                     pad, i, i);
            } break;
            case op_store_8x4: {
                int p = inst->ptr.bits;
                emit(b,
                     "%sp%d[y * m.stride%d + x] ="
                     " (%s & 0xFFu) | ((%s & 0xFFu) << 8u)"
                     " | ((%s & 0xFFu) << 16u) | (%s << 24u);\n",
                     pad, p, p,
                     uv(_ux, vx, xid, is_f),
                     uv(_uy, vy, yid, is_f),
                     uv(_uz, vz, zid, is_f),
                     uv(_uw, vw, wid, is_f));
            } break;

            case op_load_16: {
                int p = inst->ptr.bits;
                emit(b,
                     "%suint v%d = (uint)"
                     "p%d[y * m.stride%d + x];\n",
                     pad, i, p, p);
            } break;
            case op_gather_16: {
                int p = inst->ptr.bits;
                emit(b,
                     "%suint v%d = (uint)p%d"
                     "[min(%s, m.count%d - 1u)]"
                     " & ((%s < m.count%d)"
                     " ? ~0u : 0u);\n",
                     pad, i, p,
                     vx, p, vx, p);
            } break;
            case op_store_16: {
                int p = inst->ptr.bits;
                emit(b,
                     "%sp%d[y * m.stride%d + x]"
                     " = (ushort)%s;\n",
                     pad, p, p,
                     uv(_uy, vy, yid, is_f));
            } break;

            case op_load_8: {
                int p = inst->ptr.bits;
                emit(b,
                     "%suint v%d = (uint)"
                     "p%d[y * m.stride%d + x];\n",
                     pad, i, p, p);
            } break;
            case op_gather_8: {
                int p = inst->ptr.bits;
                emit(b,
                     "%suint v%d = (uint)p%d"
                     "[min(%s, m.count%d - 1u)]"
                     " & ((%s < m.count%d)"
                     " ? ~0u : 0u);\n",
                     pad, i, p,
                     vx, p, vx, p);
            } break;
            case op_store_8: {
                int p = inst->ptr.bits;
                emit(b,
                     "%sp%d[y * m.stride%d + x]"
                     " = (uchar)%s;\n",
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

            // Wrap unfused add/sub/mul in fma() so Metal's compiler can't
            // reassociate them into different roundings.  Matches what
            // SPIRV-Cross emits via spv_fadd/spv_fsub/spv_fmul, keeping all
            // backends bit-exact even under fast math.
            case op_add_f32:
                emit(b, "%sfloat v%d = fma(1.0f, %s, %s);\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f),
                     fv(_fy, vy, yid, is_f));
                break;
            case op_sub_f32:
                emit(b, "%sfloat v%d = fma(-1.0f, %s, %s);\n",
                     pad, i,
                     fv(_fy, vy, yid, is_f),
                     fv(_fx, vx, xid, is_f));
                break;
            case op_mul_f32:
                emit(b, "%sfloat v%d = fma(%s, %s, 0.0f);\n",
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
            case op_abs_f32:
                emit(b, "%sfloat v%d ="
                        " fabs(%s);\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f));
                break;
            case op_square_f32: {
                char const *s = fv(_fx, vx, xid, is_f);
                emit(b, "%sfloat v%d = fma(%s, %s, 0.0f);\n",
                     pad, i, s, s);
            } break;
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
            case op_square_add_f32: {
                char const *xs = fv(_fx, vx, xid, is_f);
                emit(b, "%sfloat v%d ="
                        " fma(%s, %s, %s);\n",
                     pad, i, xs, xs,
                     fv(_fy, vy, yid, is_f));
            } break;
            case op_square_sub_f32: {
                char const *xs = fv(_fx, vx, xid, is_f);
                emit(b, "%sfloat v%d ="
                        " fma(-%s, %s, %s);\n",
                     pad, i, xs, xs,
                     fv(_fy, vy, yid, is_f));
            } break;

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
                            " (%s != 0u) ? %s : %s;\n",
                         pad, i,
                         uv(_ux, vx, xid, is_f),
                         fv(_fy, vy, yid, is_f),
                         fv(_fz, vz, zid, is_f));
                } else {
                    emit(b, "%suint v%d ="
                            " (%s != 0u) ? %s : %s;\n",
                         pad, i,
                         uv(_ux, vx, xid, is_f),
                         uv(_uy, vy, yid, is_f),
                         uv(_uz, vz, zid, is_f));
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

            case op_loop_begin:
                emit(b, "%swhile (var%d < %s) {\n",
                     pad, inst->imm, vx);
                break;
            case op_loop_end:
                emit(b, "%s}\n", pad);
                break;
            case op_if_begin:
                emit(b, "%sif (%s != 0u) {\n", pad, uv(_ux, vx, xid, is_f));
                break;
            case op_if_end:
                emit(b, "%s}\n", pad);
                break;
            case op_load_var:
                emit(b, "%suint v%d = var%d;\n", pad, i, inst->imm);
                break;
            case op_store_var:
                emit(b, "%svar%d = %s;\n",
                     pad, inst->imm, uv(_uy, vy, yid, is_f));
                break;
        }

        if (op_is_store(inst->op) && i+1 < hi) {
            emit(b, "\n");
        }
    }
}

static char* build_source(IR const *ir) {
    int const total_bufs = ir->total_bufs;
    struct buffer_metadata const *buf = ir->buf;

    SrcBuf b = {0};

    emit(&b,
         "#pragma METAL fp contract(off)\n"
         "#include <metal_stdlib>\n"
         "using namespace metal;\n\n");

    emit(&b, "\n");

    emit(&b, "struct meta { uint w, x0, y0");
    for (int i = 0; i < total_bufs; i++) {
        emit(&b, ", count%d", i);
    }
    for (int i = 0; i < total_bufs; i++) {
        emit(&b, ", stride%d", i);
    }
    emit(&b, "; };\n\n");

    emit(&b, "kernel void main0(\n");
    emit(&b,
         "    constant meta &m [[buffer(0)]]");
    for (int p = 0; p < total_bufs; p++) {
        char const *type = buf[p].shift == 3 ? "half4"
                         : buf[p].shift == 2 ? "uint"
                         : buf[p].shift == 1 ? "ushort"
                                             : "uchar";
        char const *qual = (buf[p].rw & BUF_WRITTEN) ? "device" : "device const";
        emit(&b,
             ",\n    %s %s * __restrict p%d"
             " [[buffer(%d)]]",
             qual, type, p, p + 1);
    }
    emit(&b,
         ",\n    uint2 pos"
         " [[thread_position_in_grid]]\n) {\n");
    emit(&b,
         "    if (pos.x >= m.w) return;\n"
         "    uint x = m.x0 + pos.x;\n"
         "    uint y = m.y0 + pos.y;\n");
    for (int i = 0; i < ir->vars; i++) {
        emit(&b, "    uint var%d;\n", i);
    }

    int const n = ir->insts;
    _Bool *is_f = calloc((size_t)(n + 1), 1);
    emit_ops(&b, ir, is_f, 0, n, "    ");
    emit(&b, "}\n");

    free(is_f);

    char *src = malloc(b.size + 1);
    __builtin_memcpy(src, b.text, b.size);
    src[b.size] = '\0';
    free(b.text);
    return src;
}

enum { METAL_RING_HIGH_WATER = 64 * 1024 };

static struct uniform_ring_chunk metal_ring_new_chunk(size_t min_bytes, void *ctx) {
    struct metal_backend *be = ctx;
    void *pool = objc_autoreleasePoolPush();
    size_t cap = min_bytes > METAL_RING_HIGH_WATER ? min_bytes : METAL_RING_HIGH_WATER;
    id buf = msg_uu(
        be->device, sel("newBufferWithLength:options:"),
        (NSUInteger)cap, (NSUInteger)MTLResourceStorageModeShared);
    struct uniform_ring_chunk chunk = {
        .handle=(void*)buf,
        .mapped=contents(buf),
        .cap   =cap,
        .used  =0,
    };
    objc_autoreleasePoolPop(pool);
    return chunk;
}

static void metal_ring_free_chunk(void *handle, void *ctx) {
    (void)ctx;
    release(handle);
}

static void metal_wait_frame(int frame, void *ctx) {
    struct metal_backend *be = ctx;
    if (be->frame_committed[frame]) {
        id prior = (id)be->frame_committed[frame];
        be->frame_committed[frame] = NULL;
        (void)msg(prior, sel("waitUntilCompleted"));
        be->gpu_time_accum +=
            msg_f64(prior, sel("GPUEndTime"))
          - msg_f64(prior, sel("GPUStartTime"));
        release(prior);
    }
}

static struct metal_backend* metal_backend_create(void) {
    init_sel_cache();
    void *pool = objc_autoreleasePoolPush();
    id device = (id)MTLCreateSystemDefaultDevice();
    id queue = device
        ? msg(device, sel("newCommandQueue"))
        : (id)0;
    struct metal_backend *be = 0;
    if (device && queue) {
        be = calloc(1, sizeof *be);
        be->device = (void*)retain(device);
        be->queue  = (void*)queue;
        be->uni_pool = (struct uniform_ring_pool){
            .n=METAL_N_FRAMES,
            .high_water=METAL_RING_HIGH_WATER,
            .ctx=be,
            .wait_frame=metal_wait_frame,
        };
        for (int i = 0; i < METAL_N_FRAMES; i++) {
            be->uni_pool.rings[i] = (struct uniform_ring){
                .align=16,
                .ctx=be,
                .new_chunk=metal_ring_new_chunk,
                .free_chunk=metal_ring_free_chunk,
            };
        }
    }
    objc_autoreleasePoolPop(pool);
    return be;
}

static void metal_backend_free(struct metal_backend *be) {
    if (be) {
        uniform_ring_pool_free(&be->uni_pool);
        void *pool = objc_autoreleasePoolPush();
        gpu_buf_cache_free(&be->cache);
        release(be->device);
        release(be->queue);
        objc_autoreleasePoolPop(pool);
        free(be);
    }
}

static struct metal_program* metal_program(
    struct metal_backend *be, IR const *ir
) {
    struct metal_program *result = 0;
    void *pool = objc_autoreleasePoolPush();
    {
        char *src = build_source(ir);

        int    const total_bufs = ir->total_bufs;
        size_t const meta_bytes = (size_t)total_bufs * sizeof *ir->buf;
        struct buffer_metadata *buf = malloc(meta_bytes);
        __builtin_memcpy(buf, ir->buf, meta_bytes);

        char const *override = getenv("UMBRA_METAL_OVERRIDE");
        if (override) {
            int want_insts = 0;
            char const *path = strchr(override, ':');
            if (path) {
                want_insts = atoi(override);
                path++;
            } else {
                path = override;
            }
            if (!want_insts || want_insts == ir->insts) {
                FILE *of = fopen(path, "r");
                if (of) {
                    fseek(of, 0, SEEK_END);
                    long sz = ftell(of);
                    fseek(of, 0, SEEK_SET);
                    free(src);
                    src = malloc((size_t)sz + 1);
                    fread(src, 1, (size_t)sz, of);
                    src[sz] = '\0';
                    fclose(of);
                    fprintf(stderr, "UMBRA_METAL_OVERRIDE: replaced %d-inst program with %s\n",
                            ir->insts, path);
                } else {
                    fprintf(stderr, "UMBRA_METAL_OVERRIDE: could not open %s\n", path);
                }
            } else {
                fprintf(stderr, "UMBRA_METAL_OVERRIDE: skipping %d-inst program (want %d)\n",
                        ir->insts, want_insts);
            }
        }

        id error = 0;
        id library = 0;
        id func = 0;
        id pso = 0;

        id opts = msg(
            (id)objc_getClass("MTLCompileOptions"), sel("new"));

        id source = nsstr(src);
        library = msg_ppp(
            be->device, sel("newLibraryWithSource:options:error:"),
            source, opts, &error);
        if (!library) {
            if (error) {
                id desc = msg(error, sel("localizedDescription"));
                char const *utf8 = (char const *)msg(desc, sel("UTF8String"));
                fprintf(stderr, "Metal compile error: %s\n", utf8 ? utf8 : "(no details)");
            } else {
                fprintf(stderr, "Metal compile error (no error object)\n");
            }
            goto fail;
        }

        func = msg_p(
            library, sel("newFunctionWithName:"), nsstr("main0"));
        if (!func) { goto fail; }
        pso = msg_pp(
            be->device, sel("newComputePipelineStateWithFunction:error:"),
            func, &error);
        if (!pso) { goto fail; }

        {
            struct metal_program *p = calloc(1, sizeof *p);
            p->pipeline      = (void*)pso;
            p->src           = src;
            p->total_bufs    = total_bufs;
            p->buf           = buf;
            p->bindings      = ir->bindings;
            if (p->bindings) {
                size_t const sz = (size_t)p->bindings * sizeof *p->binding;
                p->binding = malloc(sz);
                __builtin_memcpy(p->binding, ir->binding, sz);
            }

            result = p;
            goto out;
        }

    fail:
        free(buf);
        free(src);
    out:;
    }
    objc_autoreleasePoolPop(pool);
    return result;
}

static gpu_buf metal_cache_alloc(size_t size, void *ctx) {
    struct metal_backend *be = ctx;
    size_t alloc_size = size ? size : 1;
    id buf = msg_uu(
        be->device, sel("newBufferWithLength:options:"),
        (NSUInteger)alloc_size, (NSUInteger)MTLResourceStorageModeShared);
    return (gpu_buf){.ptr = (void*)buf, .size = alloc_size};
}

static void metal_cache_upload(gpu_buf buf, void const *host, size_t bytes, void *ctx) {
    (void)ctx;
    __builtin_memcpy(contents(buf.ptr), host, bytes);
}

static void metal_cache_download(gpu_buf buf, void *host, size_t bytes, void *ctx) {
    (void)ctx;
    __builtin_memcpy(host, contents(buf.ptr), bytes);
}

static gpu_buf metal_cache_import(void *host, size_t bytes, void *ctx) {
    struct metal_backend *be = ctx;
    size_t page = (size_t)getpagesize();
    if (host && ((uintptr_t)host % page) == 0) {
        size_t import_sz = (bytes + page - 1) & ~(page - 1);
        id buf = msg_vuup(
            be->device, sel("newBufferWithBytesNoCopy:length:options:deallocator:"),
            host, (NSUInteger)import_sz, (NSUInteger)MTLResourceStorageModeShared, (id)0);
        if (buf) {
            return (gpu_buf){.ptr = (void*)buf, .size = bytes};
        }
    }
    return (gpu_buf){0};
}

static void metal_cache_release(gpu_buf buf, void *ctx) {
    (void)ctx;
    release(buf.ptr);
}

static void encode_dispatch(struct metal_program *p,
                            int l, int t, int r, int b,
                            struct umbra_buf buf[], id enc) {
    struct metal_backend *be = (struct metal_backend*)p->base.backend;
    int const w = r - l,
              h = b - t;

    if (be->batch_pipeline != p->pipeline) {
        (void)msg_v_p(enc, SEL_setComputePipelineState, (id)p->pipeline);
        be->batch_pipeline = p->pipeline;
    }

    int const tb = p->total_bufs;
    assume(tb <= 32);

    _Bool pinned[32] = {0};
    for (int i = 0; i < p->bindings; i++) {
        pinned[p->binding[i].ix] = binding_is_uniform(p->binding[i].kind);
    }

    void   *bind_handle[32] = {0};
    size_t  bind_offset[32] = {0};
    for (int i = 0; i < tb; i++) {
        if (buf[i].ptr && buf[i].count) {
            size_t const bytes = (size_t)buf[i].count << p->buf[i].shift;
            uint8_t const rw = (uint8_t)(p->buf[i].rw
                             | (p->buf[i].host_readonly ? BUF_HOST_READONLY : 0));
            if (!(rw & BUF_WRITTEN) && pinned[i]) {
                struct uniform_ring_loc loc =
                    uniform_ring_pool_alloc(&be->uni_pool, buf[i].ptr, bytes);
                bind_handle[i] = loc.handle;
                bind_offset[i] = loc.offset;
            } else {
                int idx = gpu_buf_cache_get(&be->cache, buf[i].ptr, bytes, rw);
                bind_handle[i] = be->cache.entry[idx].buf.ptr;
            }
        }
    }

    uint32_t meta[3 + 2*32];
    meta[0] = (uint32_t)w;
    meta[1] = (uint32_t)l;
    meta[2] = (uint32_t)t;
    for (int i = 0; i < tb; i++) {
        (meta)[3+i   ] = (uint32_t)buf[i].count;
        (meta)[3+i+tb] = (uint32_t)buf[i].stride;
    }
    size_t const meta_bytes = (size_t)(3 + 2*tb) * sizeof(uint32_t);
    (void)msg_v_vuu(enc, SEL_setBytes_length_atIndex,
                    meta, (NSUInteger)meta_bytes, (NSUInteger)0);

    for (int i = 0; i < tb; i++) {
        if (bind_handle[i]) {
            (void)msg_v_puu(enc, SEL_setBuffer_offset_atIndex,
                            (id)bind_handle[i], (NSUInteger)bind_offset[i], (NSUInteger)(i + 1));
        }
    }

    MTLSize grid  = {(NSUInteger)w, (NSUInteger)h, 1};
    MTLSize group = {(NSUInteger)64, 1, 1};
    msg_v_ss(enc, SEL_dispatchThreads_threadsPerThreadgroup, grid, group);
    be->total_dispatches++;
}

static void metal_submit_cmdbuf(struct metal_backend *be);

static void metal_program_queue(struct metal_program *p, int l, int t, int r, int b,
                                struct umbra_late_binding const *late, int lates) {
    int w = r - l, h = b - t;
    if (w <= 0 || h <= 0) { return; }

    struct metal_backend *be = (struct metal_backend*)p->base.backend;

    assume(p->total_bufs <= 32);
    struct umbra_buf buf[32];
    resolve_bindings(buf, p->binding, p->bindings, late, lates);

    void *pool = objc_autoreleasePoolPush();
    {
        if (!be->batch_cmdbuf) {
            be->batch_cmdbuf = (void*)retain(msg(be->queue, SEL_commandBuffer));
        }
        if (!be->batch_enc) {
            be->batch_enc = (void*)retain(msg_u(
                (id)be->batch_cmdbuf, SEL_computeCommandEncoderWithDispatchType,
                (NSUInteger)MTLDispatchTypeSerial));
        }
        encode_dispatch(p, l, t, r, b, buf, (id)be->batch_enc);
    }
    objc_autoreleasePoolPop(pool);
}

static void metal_submit_cmdbuf(struct metal_backend *be) {
    if (be->batch_cmdbuf) {
        void *pool = objc_autoreleasePoolPush();
        {
            if (be->batch_enc) {
                (void)msg((id)be->batch_enc, SEL_endEncoding);
                release(be->batch_enc);
                be->batch_enc      = NULL;
                be->batch_pipeline = NULL;
            }
            (void)msg((id)be->batch_cmdbuf, SEL_commit);
        }
        objc_autoreleasePoolPop(pool);
        be->total_submits++;
        be->frame_committed[be->uni_pool.cur] = be->batch_cmdbuf;
        be->batch_cmdbuf                      = NULL;
        uniform_ring_pool_rotate(&be->uni_pool);
    }
}

static void metal_flush(struct metal_backend *be) {
    metal_submit_cmdbuf(be);
    uniform_ring_pool_drain_all(&be->uni_pool);
    void *pool = objc_autoreleasePoolPush();
    {
        gpu_buf_cache_copyback (&be->cache);
        gpu_buf_cache_end_batch(&be->cache);
    }
    objc_autoreleasePoolPop(pool);
}

static void metal_program_free(struct metal_program *p) {
    if (p->pipeline) {
        release(p->pipeline);
    }
    free(p->buf);
    free(p->binding);
    free(p->src);
    free(p);
}

static void metal_program_dump(
    struct metal_program const *p, FILE *f
) {
    if (p->src) {
        fputs(p->src, f);
    }
}

static void run_metal(struct umbra_program *prog, int l, int t, int r, int b,
                      struct umbra_late_binding const *late, int lates) {
    metal_program_queue((struct metal_program*)prog, l, t, r, b, late, lates);
}
static void dump_metal(struct umbra_program const *prog, FILE *f) {
    metal_program_dump((struct metal_program const*)prog, f);
}
static void free_metal(struct umbra_program *prog) {
    metal_program_free((struct metal_program*)prog);
}
static struct umbra_program* compile_metal(struct umbra_backend *be, IR const *ir) {
    struct metal_program *p = metal_program((struct metal_backend*)be, ir);
    if (p) {
        p->base = (struct umbra_program){
            .queue   = run_metal,
            .dump    = dump_metal,
            .free    = free_metal,
            .backend = be,
        };
        return &p->base;
    }
    return NULL;
}
static void flush_be_metal(struct umbra_backend *be) { metal_flush((struct metal_backend*)be); }
static void free_be_metal(struct umbra_backend *be) {
    struct metal_backend *mbe = (struct metal_backend*)be;
    metal_flush(mbe);
    metal_backend_free(mbe);
}
static struct umbra_backend_stats stats_metal(struct umbra_backend const *be) {
    struct metal_backend const *mbe = (struct metal_backend const*)be;
    return (struct umbra_backend_stats){
        .gpu_sec                = mbe->gpu_time_accum,
        .uniform_ring_rotations = mbe->uni_pool.rotations,
        .dispatches             = mbe->total_dispatches,
        .submits                = mbe->total_submits,
        .upload_bytes           = mbe->cache.upload_bytes,
    };
}
struct umbra_backend* umbra_backend_metal(void) {
    struct metal_backend *mbe = metal_backend_create();
    if (mbe) {
        mbe->base = (struct umbra_backend){
            .compile = compile_metal,
            .flush   = flush_be_metal,
            .free    = free_be_metal,
            .stats   = stats_metal,
        };
        mbe->cache = (struct gpu_buf_cache){
            .ops = {metal_cache_alloc, metal_cache_upload, metal_cache_download,
                    metal_cache_import, metal_cache_release},
            .ctx = mbe,
        };
        return &mbe->base;
    }
    return 0;
}

#endif
