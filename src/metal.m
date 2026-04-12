#include "basic_block.h"
#include "gpu_buf_cache.h"
#include "uniform_ring.h"

#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#if !defined(__APPLE__) || defined(__wasm__)

struct umbra_backend *umbra_backend_metal(void) { return 0; }

#else

#import <Metal/Metal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

typedef struct umbra_basic_block BB;

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

enum { METAL_N_FRAMES = 2 };

struct metal_backend {
    struct umbra_backend base;
    void *device;
    void *queue;
    void *batch_cmdbuf;     // currently-encoding cmdbuf for uni_pool.cur, or NULL
    void *batch_enc;        // single MTLDispatchTypeSerial compute encoder
                            // open on batch_cmdbuf for the entire batch;
                            // ended in metal_submit_cmdbuf right before commit
    void *frame_committed[METAL_N_FRAMES];  // last committed cmdbuf per frame, or NULL
    struct gpu_buf_cache cache;
    struct uniform_ring_pool uni_pool;
    double gpu_time_accum;
    double encode_time_accum;
    double submit_time_accum;
    int    total_dispatches;
    int    total_submits;
};

struct metal_program {
    struct umbra_program base;
    void *pipeline;
    char     *src;
    struct deref_info *deref;
    uint8_t  *buf_rw;
    uint8_t  *buf_shift;
    uint8_t  *buf_row_shift;
    int    max_ptr;
    int    total_bufs;
    int    tg_size;
    int    n_deref;
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
        || op == op_mul_f32     || op == op_div_f32
        || op == op_min_f32     || op == op_max_f32
        || op == op_sqrt_f32    || op == op_abs_f32
        || op == op_round_f32   || op == op_floor_f32  || op == op_ceil_f32
        || op == op_fma_f32     || op == op_fms_f32
        || op == op_add_f32_imm || op == op_sub_f32_imm
        || op == op_mul_f32_imm || op == op_div_f32_imm
        || op == op_min_f32_imm || op == op_max_f32_imm
        || op == op_f32_from_i32
        || op == op_f32_from_f16
        || op == op_sample_32;
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

static void emit_ops(SrcBuf *b, BB const *bb,
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

            case op_deref_ptr: break;

            case op_uniform_32: {
                int p = inst->ptr.deref
                    ? deref_buf[inst->ptr.ix] : inst->ptr.bits;
                emit(b,
                     "%suint v%d = p%d[%d];\n",
                     pad, i, p, inst->imm / 4);
            } break;
            case op_load_32: {
                int p = inst->ptr.deref
                    ? deref_buf[inst->ptr.ix] : inst->ptr.bits;
                emit(b,
                     "%suint v%d = p%d"
                     "[y * m.stride%d + x];\n",
                     pad, i, p, p);
            } break;
            case op_gather_uniform_32:
            case op_gather_32: {
                int p = inst->ptr.deref
                    ? deref_buf[inst->ptr.ix] : inst->ptr.bits;
                emit(b,
                     "%suint v%d = 0;"
                     " if (%s < m.limit%d)"
                     " { v%d = p%d[%s]; }\n",
                     pad, i, vx, p,
                     i, p, vx);
            } break;
            case op_sample_32: {
                int p = inst->ptr.deref
                    ? deref_buf[inst->ptr.ix] : inst->ptr.bits;
                emit(b,
                     "%sfloat _si%d = floor(%s);\n"
                     "%sfloat _fr%d = %s - _si%d;\n",
                     pad, i, fv(_fx, vx, xid, is_f),
                     pad, i, fv(_fx, vx, xid, is_f), i);
                emit(b,
                     "%suint _lo%d = 0;"
                     " if ((uint)(int)_si%d < m.limit%d)"
                     " { _lo%d = p%d[(int)_si%d]; }\n",
                     pad, i, i, p, i, p, i);
                emit(b,
                     "%suint _hi%d = 0;"
                     " if ((uint)((int)_si%d+1) < m.limit%d)"
                     " { _hi%d = p%d[(int)_si%d+1]; }\n",
                     pad, i, i, p, i, p, i);
                emit(b,
                     "%sfloat v%d = as_type<float>(_lo%d)"
                     " + (as_type<float>(_hi%d)"
                     " - as_type<float>(_lo%d)) * _fr%d;\n",
                     pad, i, i, i, i, i);
            } break;
            case op_store_32: {
                int p = inst->ptr.deref
                    ? deref_buf[inst->ptr.ix] : inst->ptr.bits;
                emit(b,
                     "%sp%d[y * m.stride%d + x]"
                     " = %s;\n",
                     pad, p, p,
                     uv(_uy, vy, yid, is_f));
            } break;
            case op_load_16x4: {
                int p = inst->ptr.deref
                    ? deref_buf[inst->ptr.ix] : inst->ptr.bits;
                emit(b,
                     "%sulong _px%d = p%d"
                     "[y * m.stride%d + x];\n"
                     "%suint v%d = (uint)(_px%d) & 0xFFFFu;\n"
                     "%suint v%d_1 = (uint)(_px%d >> 16) & 0xFFFFu;\n"
                     "%suint v%d_2 = (uint)(_px%d >> 32) & 0xFFFFu;\n"
                     "%suint v%d_3 = (uint)(_px%d >> 48);\n",
                     pad, i, p, p,
                     pad, i, i,
                     pad, i, i,
                     pad, i, i,
                     pad, i, i);
            } break;
            case op_load_16x4_planar: {
                int p = inst->ptr.deref
                    ? deref_buf[inst->ptr.ix] : inst->ptr.bits;
                emit(b,
                     "%suint _row%d = y * m.stride%d;"
                     " uint _ps%d = m.limit%d;\n"
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
                int p = inst->ptr.deref
                    ? deref_buf[inst->ptr.ix] : inst->ptr.bits;
                emit(b,
                     "%sp%d[y * m.stride%d + x] ="
                     " (ulong)(%s & 0xFFFFu)"
                     " | ((ulong)(%s & 0xFFFFu) << 16)"
                     " | ((ulong)(%s & 0xFFFFu) << 32)"
                     " | ((ulong)(%s) << 48);\n",
                     pad, p, p,
                     uv(_ux, vx, xid, is_f),
                     uv(_uy, vy, yid, is_f),
                     uv(_uz, vz, zid, is_f),
                     uv(_uw, vw, wid, is_f));
            } break;
            case op_store_16x4_planar: {
                int p = inst->ptr.deref
                    ? deref_buf[inst->ptr.ix] : inst->ptr.bits;
                emit(b,
                     "%s{ uint _row = y * m.stride%d;"
                     " uint _ps = m.limit%d;\n"
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
                int p = inst->ptr.deref
                    ? deref_buf[inst->ptr.ix] : inst->ptr.bits;
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
                int p = inst->ptr.deref
                    ? deref_buf[inst->ptr.ix] : inst->ptr.bits;
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
                int p = inst->ptr.deref
                    ? deref_buf[inst->ptr.ix] : inst->ptr.bits;
                emit(b,
                     "%suint v%d = (uint)"
                     "p%d[y * m.stride%d + x];\n",
                     pad, i, p, p);
            } break;
            case op_gather_16: {
                int p = inst->ptr.deref
                    ? deref_buf[inst->ptr.ix] : inst->ptr.bits;
                emit(b,
                     "%suint v%d = 0;"
                     " if (%s < m.limit%d)"
                     " { v%d = (uint)p%d[%s]; }\n",
                     pad, i, vx, p,
                     i, p, vx);
            } break;
            case op_store_16: {
                int p = inst->ptr.deref
                    ? deref_buf[inst->ptr.ix] : inst->ptr.bits;
                emit(b,
                     "%sp%d[y * m.stride%d + x]"
                     " = (ushort)%s;\n",
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
                        " precise::sqrt(%s);\n",
                     pad, i,
                     fv(_fx, vx, xid, is_f));
                break;
            case op_abs_f32:
                emit(b, "%sfloat v%d ="
                        " fabs(%s);\n",
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

            case op_loop_begin:
                emit(b, "%swhile (var%d < %s) {\n",
                     pad, inst->imm, vx);
                break;
            case op_loop_end:
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

static char* build_source(BB const *bb,
                           int *out_max_ptr,
                           int *out_total_bufs,
                           int *out_deref_buf,
                           uint8_t **out_buf_shift,
                           uint8_t **out_buf_row_shift) {
    int max_ptr = -1;
    for (int i = 0; i < bb->insts; i++) {
        if (op_has_ptr(bb->inst[i].op)
                && bb->inst[i].ptr.bits >= 0) {
            if (bb->inst[i].ptr.bits > max_ptr) {
                max_ptr = bb->inst[i].ptr.bits;
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

    uint8_t *buf_shift     = calloc((size_t)(total_bufs + 1), sizeof *buf_shift);
    uint8_t *buf_row_shift = calloc((size_t)(total_bufs + 1), sizeof *buf_row_shift);
    *out_buf_shift     = buf_shift;
    *out_buf_row_shift = buf_row_shift;
    for (int i = 0; i < bb->insts; i++) {
        if (!op_has_ptr(bb->inst[i].op)) { continue; }
        int bi = bb->inst[i].ptr.deref ? deref_buf[bb->inst[i].ptr.ix]
                                      : bb->inst[i].ptr.bits;
        if (bb->inst[i].op == op_load_16x4_planar
         || bb->inst[i].op == op_store_16x4_planar) { buf_shift[bi] = 3; buf_row_shift[bi] = 1; }
        else if (bb->inst[i].op == op_load_16x4
              || bb->inst[i].op == op_store_16x4) { buf_shift[bi] = 3; buf_row_shift[bi] = 3; }
        else if (bb->inst[i].op == op_gather_16
              || bb->inst[i].op == op_load_16
              || bb->inst[i].op == op_store_16) { buf_shift[bi] = 1; buf_row_shift[bi] = 1; }
        else                                    { buf_shift[bi] = 2; buf_row_shift[bi] = 2; }
    }

    SrcBuf b = {0};

    emit(&b,
         "#include <metal_stdlib>\n"
         "using namespace metal;\n\n");

    emit(&b, "\n");

    emit(&b, "struct meta { uint w, x0, y0");
    for (int i = 0; i < total_bufs; i++) {
        emit(&b, ", limit%d", i);
    }
    for (int i = 0; i < total_bufs; i++) {
        emit(&b, ", stride%d", i);
    }
    emit(&b, "; };\n\n");

    emit(&b, "kernel void umbra_entry(\n");
    emit(&b,
         "    constant meta &m [[buffer(%d)]]",
         total_bufs);
    for (int p = 0; p <= max_ptr; p++) {
        char const *type = buf_row_shift[p] == 3 ? "ulong"
                         : buf_shift[p]     == 2 ? "uint" : "ushort";
        emit(&b,
             ",\n    device %s *p%d"
             " [[buffer(%d)]]",
             type, p, p);
    }
    for (int i = 0; i < bb->insts; i++) {
        if (bb->inst[i].op == op_deref_ptr) {
            int db = deref_buf[i];
            char const *type = buf_row_shift[db] == 3 ? "ulong"
                             : buf_shift[db]     == 2 ? "uint" : "ushort";
            emit(&b,
                 ",\n    device %s *p%d"
                 " [[buffer(%d)]]",
                 type, db, db);
        }
    }
    emit(&b,
         ",\n    uint2 pos"
         " [[thread_position_in_grid]]\n) {\n");
    emit(&b,
         "    if (pos.x >= m.w) return;\n"
         "    uint x = m.x0 + pos.x;\n"
         "    uint y = m.y0 + pos.y;\n");
    for (int i = 0; i < bb->n_vars; i++) {
        emit(&b, "    uint var%d = 0;\n", i);
    }

    _Bool *is_f = calloc((size_t)(bb->insts + 1), 1);
    emit_ops(&b, bb, deref_buf, is_f, 0, bb->insts, "    ");
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
    @autoreleasepool {
        id<MTLDevice> device = (__bridge id<MTLDevice>)be->device;
        size_t cap = min_bytes > METAL_RING_HIGH_WATER ? min_bytes : METAL_RING_HIGH_WATER;
        id<MTLBuffer> buf = [device newBufferWithLength:(NSUInteger)cap
                                                options:MTLResourceStorageModeShared];
        return (struct uniform_ring_chunk){
            .handle=(__bridge_retained void*)buf,
            .mapped=buf.contents,
            .cap   =cap,
            .used  =0,
        };
    }
}

static void metal_ring_free_chunk(void *handle, void *ctx) {
    (void)ctx;
    @autoreleasepool {
        (void)(__bridge_transfer id<MTLBuffer>)handle;
    }
}

// uniform_ring_pool wait_frame callback. Releases any in-flight cmdbuf
// stashed in frame_committed[frame] and waits for it to retire. The pool
// itself resets the ring after this returns.
static void metal_wait_frame(int frame, void *ctx) {
    struct metal_backend *be = ctx;
    if (be->frame_committed[frame]) {
        @autoreleasepool {
            id<MTLCommandBuffer> prior =
                (__bridge_transfer id<MTLCommandBuffer>)be->frame_committed[frame];
            be->frame_committed[frame] = NULL;
            [prior waitUntilCompleted];
            be->gpu_time_accum += prior.GPUEndTime - prior.GPUStartTime;
        }
    }
}

static struct metal_backend* metal_backend_create(void) {
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
            be->uni_pool = (struct uniform_ring_pool){
                .n         =METAL_N_FRAMES,
                .high_water=METAL_RING_HIGH_WATER,
                .ctx       =be,
                .wait_frame=metal_wait_frame,
            };
            for (int i = 0; i < METAL_N_FRAMES; i++) {
                be->uni_pool.rings[i] = (struct uniform_ring){
                    .align     =16,
                    .ctx       =be,
                    .new_chunk =metal_ring_new_chunk,
                    .free_chunk=metal_ring_free_chunk,
                };
            }
            return be;
        }
        return 0;
    }
}

static void metal_backend_free(struct metal_backend *be) {
    if (be) {
        uniform_ring_pool_free(&be->uni_pool);
        @autoreleasepool {
            gpu_buf_cache_free(&be->cache);
            if (be->device) {
                (void)(__bridge_transfer id)be->device;
            }
            if (be->queue) {
                (void)(__bridge_transfer id)be->queue;
            }
        }
        free(be);
    }
}


static struct metal_program* metal_program(
    struct metal_backend *be, BB const *bb
) {
    struct metal_program *result = 0;
    @autoreleasepool {
        id<MTLDevice> device = (__bridge id<MTLDevice>)be->device;

        int *deref_buf = calloc((size_t)bb->insts, sizeof *deref_buf);
        int  max_ptr = -1, total_bufs = 0;
        uint8_t *buf_shift = NULL, *buf_row_shift = NULL;
        char *src = build_source(bb, &max_ptr, &total_bufs, deref_buf,
                                 &buf_shift, &buf_row_shift);

        int n_deref = total_bufs - max_ptr - 1;
        struct deref_info *di = calloc((size_t)(n_deref ? n_deref : 1), sizeof *di);
        {
            int d = 0;
            for (int i = 0; i < bb->insts; i++) {
                if (bb->inst[i].op == op_deref_ptr) {
                    di[d].buf_idx  = deref_buf[i];
                    di[d].src_buf  = bb->inst[i].ptr.bits;
                    di[d].off = bb->inst[i].imm;
                    d++;
                }
            }
        }

        uint8_t *buf_rw = calloc((size_t)(total_bufs + 1), sizeof *buf_rw);
        for (int i = 0; i < bb->insts; i++) {
            if (!op_has_ptr(bb->inst[i].op)) { continue; }
            int bi = bb->inst[i].ptr.deref ? deref_buf[bb->inst[i].ptr.ix]
                                           : bb->inst[i].ptr.bits;
            buf_rw[bi] |= op_is_store(bb->inst[i].op) ? BUF_WRITTEN : BUF_READ;
        }

        NSError *error = nil;
        id<MTLLibrary> library = nil;
        id<MTLFunction> func = nil;
        id<MTLComputePipelineState> pso = nil;

        MTLCompileOptions *opts = [MTLCompileOptions new];
        if (@available(macOS 15.0, *)) {
            opts.mathMode = MTLMathModeSafe;
        } else {
            opts.fastMathEnabled = NO;
        }

        NSString *source = [NSString stringWithUTF8String:src];
        library = [device newLibraryWithSource:source
                                       options:opts
                                         error:&error];
        if (!library) {
            NSLog(@"Metal compile error: %@", error);
            goto fail;
        }

        func = [library newFunctionWithName:@"umbra_entry"];
        if (!func) { goto fail; }
        pso = [device newComputePipelineStateWithFunction:func error:&error];
        if (!pso) { goto fail; }

        {
            struct metal_program *p = calloc(1, sizeof *p);
            p->pipeline      = (__bridge_retained void*)pso;
            p->tg_size       = (int)pso.maxTotalThreadsPerThreadgroup;
            p->src           = src;
            p->max_ptr       = max_ptr;
            p->total_bufs    = total_bufs;
            p->deref         = di;
            p->n_deref       = n_deref;
            p->buf_rw        = buf_rw;
            p->buf_shift     = buf_shift;
            p->buf_row_shift = buf_row_shift;

            free(deref_buf);
            result = p;
            goto out;
        }

    fail:
        free(deref_buf);
        free(di);
        free(buf_rw);
        free(buf_shift);
        free(buf_row_shift);
        free(src);
    out:;
    }
    return result;
}

static gpu_buf metal_cache_alloc(size_t size, void *ctx) {
    struct metal_backend *be = ctx;
    id<MTLDevice> device = (__bridge id<MTLDevice>)be->device;
    size_t alloc_size = size ? size : 1;
    id<MTLBuffer> buf = [device newBufferWithLength:(NSUInteger)alloc_size
                                            options:MTLResourceStorageModeShared];
    return (gpu_buf){.ptr = (__bridge_retained void *)buf, .size = alloc_size};
}

static void metal_cache_upload(gpu_buf buf, void const *host, size_t bytes, void *ctx) {
    (void)ctx;
    id<MTLBuffer> mtl = (__bridge id<MTLBuffer>)buf.ptr;
    __builtin_memcpy(mtl.contents, host, bytes);
}

static void metal_cache_download(gpu_buf buf, void *host, size_t bytes, void *ctx) {
    (void)ctx;
    id<MTLBuffer> mtl = (__bridge id<MTLBuffer>)buf.ptr;
    __builtin_memcpy(host, mtl.contents, bytes);
}

static gpu_buf metal_cache_import(void *host, size_t bytes, void *ctx) {
    struct metal_backend *be = ctx;
    size_t page = (size_t)getpagesize();
    if (!host || ((uintptr_t)host % page) != 0) { return (gpu_buf){0}; }
    size_t import_sz = (bytes + page - 1) & ~(page - 1);
    id<MTLDevice> device = (__bridge id<MTLDevice>)be->device;
    id<MTLBuffer> buf = [device newBufferWithBytesNoCopy:host
                                                  length:(NSUInteger)import_sz
                                                 options:MTLResourceStorageModeShared
                                             deallocator:nil];
    if (!buf) { return (gpu_buf){0}; }
    return (gpu_buf){.ptr = (__bridge_retained void *)buf, .size = bytes};
}

static void metal_cache_release(gpu_buf buf, void *ctx) {
    (void)ctx;
    (void)(__bridge_transfer id<MTLBuffer>)buf.ptr;
}

static void encode_dispatch(
    struct metal_program *p,
    int l, int t, int r, int b,
    struct umbra_buf buf[],
    id<MTLComputeCommandEncoder> enc
) {
    int w = r - l, h = b - t, x0 = l, y0 = t;
    struct metal_backend *be = (struct metal_backend*)p->base.backend;

    id<MTLComputePipelineState> pso =
        (__bridge id<MTLComputePipelineState>)p->pipeline;
    [enc setComputePipelineState:pso];

    int tb = p->total_bufs;
    uint32_t szs_data[33] = {0};
    uint32_t rbs_data[33] = {0};
    for (int i = 0; i <= p->max_ptr; i++) {
        if (buf[i].ptr && buf[i].sz) {
            szs_data[i] = (uint32_t)(buf[i].sz >> p->buf_shift[i]);
        }
        rbs_data[i]  = (uint32_t)(buf[i].row_bytes >> p->buf_row_shift[i]);
    }

    // For each top-level / deref'd binding we need an MTLBuffer + offset.
    // Read-only flat buffers go through the per-batch uniform ring (bump-
    // allocated into a chunk MTLBuffer, no growth in the cmdbuf inline area).
    // Everything else (writable, row-structured, deref'd) goes through
    // cache_buf so the writable->readonly handoff path is preserved.
    void   *bind_handle[32];
    size_t  bind_offset[32];
    for (int i = 0; i < tb; i++) { bind_handle[i] = 0; bind_offset[i] = 0; }

    for (int i = 0; i <= p->max_ptr; i++) {
        if (buf[i].ptr && buf[i].sz) {
            uint8_t const rw = p->buf_rw[i];
            if (!(rw & BUF_WRITTEN) && !buf[i].row_bytes) {
                struct uniform_ring_loc loc =
                    uniform_ring_pool_alloc(&be->uni_pool, buf[i].ptr, buf[i].sz);
                bind_handle[i] = loc.handle;
                bind_offset[i] = loc.offset;
            } else {
                int idx = gpu_buf_cache_get(&be->cache, buf[i].ptr, buf[i].sz, rw);
                bind_handle[i] = be->cache.entry[idx].buf.ptr;
            }
        }
    }

    for (int d = 0; d < p->n_deref; d++) {
        void *base = buf[p->deref[d].src_buf].ptr;
        void  *derived;
        size_t dsz, drb;
        __builtin_memcpy(&derived, (char*)base + p->deref[d].off,      sizeof derived);
        __builtin_memcpy(&dsz,     (char*)base + p->deref[d].off + 8,  sizeof dsz);
        __builtin_memcpy(&drb,     (char*)base + p->deref[d].off + 16, sizeof drb);
        int const bi  = p->deref[d].buf_idx;
        int const idx = gpu_buf_cache_get(&be->cache, derived, dsz, p->buf_rw[bi]);
        bind_handle[bi] = be->cache.entry[idx].buf.ptr;
        szs_data[bi] = (uint32_t)(dsz >> p->buf_shift[bi]);
        rbs_data[bi] = (uint32_t)(drb >> p->buf_row_shift[bi]);
    }

    for (int i = 0; i < tb; i++) {
        if (bind_handle[i]) {
            [enc setBuffer:(__bridge id<MTLBuffer>)bind_handle[i]
                    offset:(NSUInteger)bind_offset[i]
                   atIndex:(NSUInteger)i];
        }
    }

    // Pack all per-dispatch metadata into one struct: {w, x0, y0, limits[], strides[]}.
    // Single setBytes call, single buffer binding — matches MoltenVK's pattern.
    uint32_t meta[67] = {0};
    meta[0] = (uint32_t)w;
    meta[1] = (uint32_t)x0;
    meta[2] = (uint32_t)y0;
    __builtin_memcpy(meta + 3,      szs_data, (size_t)tb * sizeof(uint32_t));
    __builtin_memcpy(meta + 3 + tb, rbs_data, (size_t)tb * sizeof(uint32_t));
    size_t const meta_bytes = (size_t)(3 + 2 * tb) * sizeof(uint32_t);
    [enc setBytes:meta length:meta_bytes atIndex:(NSUInteger)p->total_bufs];

    // TODO: try hardcoded (64,1,1) threadgroup — simpler, matches SPIR-V path.
    int const tg_size = p->tg_size;
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
    be->total_dispatches++;
}

static void metal_submit_cmdbuf(struct metal_backend *be);

static void metal_program_queue(
    struct metal_program *p, int l, int t, int r, int b, struct umbra_buf buf[]
) {
    int w = r - l, h = b - t;
    if (w <= 0 || h <= 0) { return; }

    struct metal_backend *be = (struct metal_backend*)p->base.backend;

    @autoreleasepool {
        if (!be->batch_cmdbuf) {
            id<MTLCommandQueue> queue =
                (__bridge id<MTLCommandQueue>)be->queue;
            be->batch_cmdbuf =
                (__bridge_retained void*)[queue commandBuffer];
        }
        if (!be->batch_enc) {
            id<MTLCommandBuffer> cmdbuf =
                (__bridge id<MTLCommandBuffer>)be->batch_cmdbuf;
            be->batch_enc = (__bridge_retained void*)
                [cmdbuf computeCommandEncoderWithDispatchType:MTLDispatchTypeSerial];
        }
        id<MTLComputeCommandEncoder> enc =
            (__bridge id<MTLComputeCommandEncoder>)be->batch_enc;
        double const t0 = now();
        encode_dispatch(p, l, t, r, b, buf, enc);
        be->encode_time_accum += now() - t0;
    }
}

// Backpressure release: commit the current frame's cmdbuf without blocking,
// stash it in frame_committed[cur], then rotate the pool. Pool calls
// metal_wait_frame on the new cur (waiting only if its prior cmdbuf from
// the previous cycle is still in flight) and resets that ring. Writebacks
// and cache_buf entries stay live across rotation so the next sub-batch
// keeps binding the same writable MTLBuffers.
static void metal_submit_cmdbuf(struct metal_backend *be) {
    if (!be->batch_cmdbuf) { return; }
    double const t0 = now();
    @autoreleasepool {
        if (be->batch_enc) {
            id<MTLComputeCommandEncoder> enc =
                (__bridge_transfer id<MTLComputeCommandEncoder>)be->batch_enc;
            be->batch_enc = NULL;
            [enc endEncoding];
        }
        id<MTLCommandBuffer> cmdbuf =
            (__bridge id<MTLCommandBuffer>)be->batch_cmdbuf;
        [cmdbuf commit];
    }
    be->submit_time_accum += now() - t0;
    be->total_submits++;
    be->frame_committed[be->uni_pool.cur] = be->batch_cmdbuf;
    be->batch_cmdbuf                      = NULL;
    uniform_ring_pool_rotate(&be->uni_pool);
}

static void metal_flush(struct metal_backend *be) {
    metal_submit_cmdbuf(be);
    uniform_ring_pool_drain_all(&be->uni_pool);
    @autoreleasepool {
        gpu_buf_cache_copyback (&be->cache);
        gpu_buf_cache_end_batch(&be->cache);
    }
}

static void metal_program_free(struct metal_program *p) {
    @autoreleasepool {
        if (p->pipeline) {
            (void)(__bridge_transfer id)p->pipeline;
        }
    }
    free(p->deref);
    free(p->buf_rw);
    free(p->buf_shift);
    free(p->buf_row_shift);
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

static void run_metal(struct umbra_program *prog, int l, int t, int r, int b, struct umbra_buf buf[]) {
    metal_program_queue((struct metal_program*)prog, l, t, r, b, buf);
}
static void dump_metal(struct umbra_program const *prog, FILE *f) { metal_program_dump((struct metal_program const*)prog, f); }
static void free_metal(struct umbra_program *prog) { metal_program_free((struct metal_program*)prog); }
static struct umbra_program *compile_metal(struct umbra_backend           *be,
                                           BB const *bb) {
    struct metal_program *p = metal_program((struct metal_backend*)be, bb);
    p->base = (struct umbra_program){
        .queue   = run_metal,
        .dump    = dump_metal,
        .free    = free_metal,
        .backend = be,
    };
    return &p->base;
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
        .encode_sec             = mbe->encode_time_accum,
        .submit_sec             = mbe->submit_time_accum,
        .uniform_ring_rotations = mbe->uni_pool.rotations,
        .dispatches             = mbe->total_dispatches,
        .submits                = mbe->total_submits,
        .upload_bytes           = mbe->cache.upload_bytes,
    };
}
struct umbra_backend *umbra_backend_metal(void) {
    struct metal_backend *mbe = metal_backend_create();
    if (mbe) {
        mbe->base = (struct umbra_backend){
            .compile        = compile_metal,
            .flush          = flush_be_metal,
            .free           = free_be_metal,
            .stats          = stats_metal,
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
