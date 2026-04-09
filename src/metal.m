#include "bb.h"
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

typedef struct umbra_basic_block BB;

struct buf_cache_entry {
    void   *mtl;     // retained id<MTLBuffer>
    void   *host;
    size_t  size;
    _Bool   writable; int :24, :32;
};

struct copyback {
    void *host;
    void *mtlbuf;
    size_t bytes;
};

struct deref_info { int buf_idx, src_buf, off; };

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
    struct copyback        *batch_copy;
    int                     batch_ncopy, batch_copy_cap;
    struct buf_cache_entry *batch_cache;
    int                     batch_cache_n, batch_cache_cap;
    struct uniform_ring_pool uni_pool;
};

struct umbra_metal {
    struct umbra_program base;
    void *pipeline;
    char  *src;
    struct deref_info *deref;
    int    max_ptr;
    int    total_bufs;
    int    tg_size;
    int    n_deref;
};

typedef struct {
    char  *text;
    size_t size, cap;
} Buf;

__attribute__((format(printf, 2, 3)))
static void emit(Buf *b, char const *fmt, ...) {
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

static _Bool is_16(enum op op) {
    return op == op_load_16
        || op == op_store_16
        || op == op_gather_16
        || op == op_load_16x4
        || op == op_load_16x4_planar
        || op == op_store_16x4
        || op == op_store_16x4_planar
        || op == op_i32_from_s16
        || op == op_i32_from_u16
        || op == op_i16_from_i32
        || op == op_f32_from_f16
        || op == op_f16_from_f32;
}
static _Bool is_32(enum op op) {
    return op == op_uniform_32
        || op == op_load_32
        || op == op_load_8x4
        || op == op_gather_uniform_32
        || op == op_gather_32
        || op == op_sample_32
        || op == op_store_32
        || op == op_store_8x4
        || op == op_deref_ptr;
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

        char _fx[40], _fy[40], _fz[40];
        char _ux[40], _uy[40], _uz[40], _uw[40];

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
                int p = ptr_is_deref(inst->ptr)
                    ? deref_buf[ptr_ix(inst->ptr)] : inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b, mixed
                    ? "%suint v%d = p%d_32[%d];\n"
                    : "%suint v%d = "
                      "((device const uint*)"
                      "p%d)[%d];\n",
                     pad, i, p, inst->imm / 4);
            } break;
            case op_load_32: {
                int p = ptr_is_deref(inst->ptr)
                    ? deref_buf[ptr_ix(inst->ptr)] : inst->ptr;
                emit(b,
                     "%suint v%d = ((device uint*)"
                     "(p%d + y * buf_rbs[%d]))[x];\n",
                     pad, i, p, p);
            } break;
            case op_gather_uniform_32:
            case op_gather_32: {
                int p = ptr_is_deref(inst->ptr)
                    ? deref_buf[ptr_ix(inst->ptr)] : inst->ptr;
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
            case op_sample_32: {
                int p = ptr_is_deref(inst->ptr)
                    ? deref_buf[ptr_ix(inst->ptr)] : inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b,
                     "%sfloat _si%d = floor(%s);\n"
                     "%sfloat _fr%d = %s - _si%d;\n",
                     pad, i, fv(_fx, vx, xid, is_f),
                     pad, i, fv(_fx, vx, xid, is_f), i);
                if (mixed) {
                    emit(b,
                         "%sfloat _lo%d = as_type<float>(p%d_32"
                         "[safe_ix((int)_si%d,buf_szs[%d],4)]"
                         " & oob_mask((int)_si%d,buf_szs[%d],4));\n",
                         pad, i, p, i, p, i, p);
                    emit(b,
                         "%sfloat _hi%d = as_type<float>(p%d_32"
                         "[safe_ix((int)_si%d+1,buf_szs[%d],4)]"
                         " & oob_mask((int)_si%d+1,buf_szs[%d],4));\n",
                         pad, i, p, i, p, i, p);
                } else {
                    emit(b,
                         "%sfloat _lo%d = as_type<float>(((device uint*)p%d)"
                         "[safe_ix((int)_si%d,buf_szs[%d],4)]"
                         " & oob_mask((int)_si%d,buf_szs[%d],4));\n",
                         pad, i, p, i, p, i, p);
                    emit(b,
                         "%sfloat _hi%d = as_type<float>(((device uint*)p%d)"
                         "[safe_ix((int)_si%d+1,buf_szs[%d],4)]"
                         " & oob_mask((int)_si%d+1,buf_szs[%d],4));\n",
                         pad, i, p, i, p, i, p);
                }
                emit(b,
                     "%sfloat v%d = _lo%d + (_hi%d - _lo%d) * _fr%d;\n",
                     pad, i, i, i, i, i);
            } break;
            case op_store_32: {
                int p = ptr_is_deref(inst->ptr)
                    ? deref_buf[ptr_ix(inst->ptr)] : inst->ptr;
                emit(b,
                     "%s((device uint*)"
                     "(p%d + y * buf_rbs[%d]))"
                     "[x] = %s;\n",
                     pad, p, p,
                     uv(_uy, vy, yid, is_f));
            } break;
            case op_load_16x4: {
                int p = ptr_is_deref(inst->ptr)
                    ? deref_buf[ptr_ix(inst->ptr)] : inst->ptr;
                emit(b,
                     "%sdevice ushort *hp%d = (device ushort*)"
                     "(p%d + y * buf_rbs[%d]) + x*4;\n"
                     "%suint v%d = (uint)hp%d[0];\n"
                     "%suint v%d_1 = (uint)hp%d[1];\n"
                     "%suint v%d_2 = (uint)hp%d[2];\n"
                     "%suint v%d_3 = (uint)hp%d[3];\n",
                     pad, i, p, p,
                     pad, i, i,
                     pad, i, i,
                     pad, i, i,
                     pad, i, i);
            } break;
            case op_load_16x4_planar: {
                int p = ptr_is_deref(inst->ptr)
                    ? deref_buf[ptr_ix(inst->ptr)] : inst->ptr;
                emit(b,
                     "%sdevice uchar *row%d = p%d + y * buf_rbs[%d];"
                     " uint ps%d = buf_szs[%d]/4;\n"
                     "%suint v%d = (uint)((device ushort*)row%d)[x];\n"
                     "%suint v%d_1 = (uint)((device ushort*)(row%d+ps%d))[x];\n"
                     "%suint v%d_2 = (uint)((device ushort*)(row%d+2*ps%d))[x];\n"
                     "%suint v%d_3 = (uint)((device ushort*)(row%d+3*ps%d))[x];\n",
                     pad, i, p, p, i, p,
                     pad, i, i,
                     pad, i, i, i,
                     pad, i, i, i,
                     pad, i, i, i);
            } break;
            case op_store_16x4: {
                int p = ptr_is_deref(inst->ptr)
                    ? deref_buf[ptr_ix(inst->ptr)] : inst->ptr;
                emit(b,
                     "%s{\n"
                     "%s    device ushort *hp = (device ushort*)"
                     "(p%d + y * buf_rbs[%d]) + x*4;\n"
                     "%s    hp[0] = ushort(%s); hp[1] = ushort(%s);"
                     " hp[2] = ushort(%s); hp[3] = ushort(%s);\n"
                     "%s}\n",
                     pad,
                     pad, p, p,
                     pad,
                     uv(_ux, vx, xid, is_f),
                     uv(_uy, vy, yid, is_f),
                     uv(_uz, vz, zid, is_f),
                     uv(_uw, vw, wid, is_f),
                     pad);
            } break;
            case op_store_16x4_planar: {
                int p = ptr_is_deref(inst->ptr)
                    ? deref_buf[ptr_ix(inst->ptr)] : inst->ptr;
                emit(b,
                     "%s{\n"
                     "%s    device uchar *row = p%d + y * buf_rbs[%d];"
                     " uint ps = buf_szs[%d]/4;\n"
                     "%s    ((device ushort*)row)[x] = ushort(%s);"
                     " ((device ushort*)(row+ps))[x] = ushort(%s);"
                     " ((device ushort*)(row+2*ps))[x] = ushort(%s);"
                     " ((device ushort*)(row+3*ps))[x] = ushort(%s);\n"
                     "%s}\n",
                     pad,
                     pad, p, p, p,
                     pad,
                     uv(_ux, vx, xid, is_f),
                     uv(_uy, vy, yid, is_f),
                     uv(_uz, vz, zid, is_f),
                     uv(_uw, vw, wid, is_f),
                     pad);
            } break;

            case op_load_8x4: {
                int p = ptr_is_deref(inst->ptr)
                    ? deref_buf[ptr_ix(inst->ptr)] : inst->ptr;
                emit(b,
                     "%suint px%d = ((device uint*)"
                     "(p%d + y * buf_rbs[%d]))[x];\n"
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
                int p = ptr_is_deref(inst->ptr)
                    ? deref_buf[ptr_ix(inst->ptr)] : inst->ptr;
                emit(b,
                     "%s((device uint*)(p%d + y * buf_rbs[%d]))[x] ="
                     " (%s & 0xFFu) | ((%s & 0xFFu) << 8u)"
                     " | ((%s & 0xFFu) << 16u) | (%s << 24u);\n",
                     pad, p, p,
                     uv(_ux, vx, xid, is_f),
                     uv(_uy, vy, yid, is_f),
                     uv(_uz, vz, zid, is_f),
                     uv(_uw, vw, wid, is_f));
            } break;

            case op_load_16: {
                int p = ptr_is_deref(inst->ptr)
                    ? deref_buf[ptr_ix(inst->ptr)] : inst->ptr;
                emit(b,
                     "%suint v%d = (uint)"
                     "((device ushort*)"
                     "(p%d + y * buf_rbs[%d]))[x];\n",
                     pad, i, p, p);
            } break;
            case op_gather_16: {
                int p = ptr_is_deref(inst->ptr)
                    ? deref_buf[ptr_ix(inst->ptr)] : inst->ptr;
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
                int p = ptr_is_deref(inst->ptr)
                    ? deref_buf[ptr_ix(inst->ptr)] : inst->ptr;
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

        }

        if (is_store(inst->op) && i+1 < hi) {
            emit(b, "\n");
        }
    }
}

static char* build_source(BB const *bb,
                           int *out_max_ptr,
                           int *out_total_bufs,
                           int *out_deref_buf) {
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
            int p = ptr_is_deref(bb->inst[i].ptr)
                ? deref_buf[ptr_ix(bb->inst[i].ptr)]
                : bb->inst[i].ptr;
            if (is_32(op)) { ptr_32[p] = 1; }
            else if (is_16(op)) { ptr_16[p] = 1; }
        }
    }

    Buf b = {0};

    emit(&b,
         "#include <metal_stdlib>\n"
         "using namespace metal;\n\n");

    emit(&b,
         "int safe_ix"
         "(int ix, uint bytes, int elem) {\n");
    emit(&b,
         "    int count = (int)"
         "(bytes / (uint)elem);\n");
    emit(&b,
         "    return clamp(ix, 0,"
         " max(count-1, 0));\n}\n");
    emit(&b,
         "uint oob_mask"
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
        }
    }
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

static void umbra_metal_backend_free(struct metal_backend *be) {
    if (be) {
        uniform_ring_pool_free(&be->uni_pool);
        @autoreleasepool {
            if (be->device) {
                (void)(__bridge_transfer id)be->device;
            }
            if (be->queue) {
                (void)(__bridge_transfer id)be->queue;
            }
        }
        free(be->batch_copy);
        free(be->batch_cache);
        free(be);
    }
}


static struct umbra_metal* umbra_metal(
    struct metal_backend *be, BB const *bb
) {
    struct umbra_metal *result = 0;
    @autoreleasepool {
        id<MTLDevice> device = (__bridge id<MTLDevice>)be->device;

        int *deref_buf = calloc((size_t)bb->insts, sizeof *deref_buf);
        int  max_ptr = -1, total_bufs = 0;
        char *src = build_source(bb, &max_ptr, &total_bufs, deref_buf);

        int n_deref = total_bufs - max_ptr - 1;
        struct deref_info *di = calloc((size_t)(n_deref ? n_deref : 1), sizeof *di);
        {
            int d = 0;
            for (int i = 0; i < bb->insts; i++) {
                if (bb->inst[i].op == op_deref_ptr) {
                    di[d].buf_idx  = deref_buf[i];
                    di[d].src_buf  = bb->inst[i].ptr;
                    di[d].off = bb->inst[i].imm;
                    d++;
                }
            }
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
            struct umbra_metal *m = calloc(1, sizeof *m);
            m->pipeline      = (__bridge_retained void*)pso;
            m->tg_size       = (int)pso.maxTotalThreadsPerThreadgroup;
            m->src           = src;
            m->max_ptr       = max_ptr;
            m->total_bufs    = total_bufs;
            m->deref         = di;
            m->n_deref       = n_deref;

            free(deref_buf);
            result = m;
            goto out;
        }

    fail:
        free(deref_buf);
        free(di);
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

// Returns an index into be->batch_cache. The entry is valid until
// umbra_metal_flush. Writable lookups reuse any existing entry for the same
// host pointer. Read-only lookups reuse only an existing *writable* entry
// for the same host pointer — that's the cross-program hand-off case (e.g.
// slug acc writes wind_buf, slug draw reads it via a read-only deref). A
// read-only request with no matching writable entry creates a fresh
// snapshot, because the host may have mutated the bytes since any prior
// read-only entry was made (e.g. slug acc loop counter in the uniforms
// buffer). Copyback for writable buffers is tracked exactly once, at entry
// creation time.
static int cache_buf(struct metal_backend *be, void *host, size_t bytes,
                     _Bool read_only) {
    for (int i = 0; i < be->batch_cache_n; i++) {
        struct buf_cache_entry *ce = &be->batch_cache[i];
        if (ce->host == host && ce->size >= bytes && (!read_only || ce->writable)) {
            return i;
        }
    }

    if (be->batch_cache_n >= be->batch_cache_cap) {
        be->batch_cache_cap = be->batch_cache_cap ? 2 * be->batch_cache_cap : 16;
        be->batch_cache = realloc(be->batch_cache,
            (size_t)be->batch_cache_cap * sizeof *be->batch_cache);
    }
    int idx = be->batch_cache_n++;
    struct buf_cache_entry *ce = &be->batch_cache[idx];
    *ce = (struct buf_cache_entry){0};

    id<MTLDevice> device = (__bridge id<MTLDevice>)be->device;
    // MoltenVK exemplar: always allocate a fresh shared MTLBuffer and copy
    // host data in. The newBufferWithBytesNoCopy fast path interacts badly
    // with Metal's hazard tracking on at least some driver paths (Rosetta in
    // particular), and going through the copy path is what MoltenVK does.
    id<MTLBuffer> tmp =
        [device newBufferWithLength:(NSUInteger)(bytes ? bytes : 1)
                            options:MTLResourceStorageModeShared];
    if (host && bytes) {
        __builtin_memcpy(tmp.contents, host, bytes);
    }
    ce->mtl      = (__bridge_retained void*)tmp;
    ce->host     = host;
    ce->size     = bytes;
    ce->writable = !read_only;
    if (!read_only && host && bytes) {
        batch_add_copy(be, host, ce->mtl, bytes);
    }
    return idx;
}

static void encode_dispatch(
    struct umbra_metal *m,
    int l, int t, int r, int b,
    struct umbra_buf buf[],
    id<MTLComputeCommandEncoder> enc
) {
    int w = r - l, h = b - t, x0 = l, y0 = t;
    struct metal_backend *be = (struct metal_backend*)m->base.backend;

    id<MTLComputePipelineState> pso =
        (__bridge id<MTLComputePipelineState>)m->pipeline;
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

    // For each top-level / deref'd binding we need an MTLBuffer + offset.
    // Read-only flat buffers go through the per-batch uniform ring (bump-
    // allocated into a chunk MTLBuffer, no growth in the cmdbuf inline area).
    // Everything else (writable, row-structured, deref'd) goes through
    // cache_buf so the writable->readonly handoff path is preserved.
    void   *bind_handle[32];
    size_t  bind_offset[32];
    for (int i = 0; i < tb; i++) { bind_handle[i] = 0; bind_offset[i] = 0; }

    for (int i = 0; i <= m->max_ptr; i++) {
        if (buf[i].ptr && buf[i].sz) {
            if (buf[i].read_only && !buf[i].row_bytes) {
                struct uniform_ring_loc loc =
                    uniform_ring_pool_alloc(&be->uni_pool, buf[i].ptr, buf[i].sz);
                bind_handle[i] = loc.handle;
                bind_offset[i] = loc.offset;
            } else {
                int idx = cache_buf(be, buf[i].ptr, buf[i].sz, buf[i].read_only);
                bind_handle[i] = be->batch_cache[idx].mtl;
            }
        }
    }

    for (int d = 0; d < m->n_deref; d++) {
        void *base = buf[m->deref[d].src_buf].ptr;
        void *derived;
        ptrdiff_t dsz;
        size_t drb;
        __builtin_memcpy(
            &derived,
            (char*)base + m->deref[d].off,
            sizeof derived);
        __builtin_memcpy(
            &dsz,
            (char*)base + m->deref[d].off + 8,
            sizeof dsz);
        __builtin_memcpy(
            &drb,
            (char*)base + m->deref[d].off + 16,
            sizeof drb);
        size_t const bytes           = dsz < 0 ? (size_t)-dsz : (size_t)dsz;
        _Bool  const deref_read_only = dsz < 0;
        int    const bi              = m->deref[d].buf_idx;
        int    const idx             = cache_buf(be, derived, bytes, deref_read_only);
        bind_handle[bi] = be->batch_cache[idx].mtl;
        szs_data[bi] = (uint32_t)bytes;
        rbs_data[bi] = (uint32_t)drb;
    }

    for (int i = 0; i < tb; i++) {
        if (bind_handle[i]) {
            [enc setBuffer:(__bridge id<MTLBuffer>)bind_handle[i]
                    offset:(NSUInteger)bind_offset[i]
                   atIndex:(NSUInteger)i];
        }
    }

    // Tiny per-dispatch metadata: width, x0, y0, sizes[], row_bytes[].
    // These are write-once-read-once-by-one-shader and small enough to go
    // through setBytes:, the documented Metal idiom for inline data.
    uint32_t const w32      = (uint32_t)w,
                   x032     = (uint32_t)x0,
                   y032     = (uint32_t)y0;
    size_t   const sz_bytes = (size_t)(tb + 1) * sizeof(uint32_t);
    [enc setBytes:&w32  length:sizeof w32  atIndex:(NSUInteger)(m->total_bufs + 0)];
    [enc setBytes:szs_data length:sz_bytes atIndex:(NSUInteger)(m->total_bufs + 1)];
    [enc setBytes:rbs_data length:sz_bytes atIndex:(NSUInteger)(m->total_bufs + 2)];
    [enc setBytes:&x032 length:sizeof x032 atIndex:(NSUInteger)(m->total_bufs + 3)];
    [enc setBytes:&y032 length:sizeof y032 atIndex:(NSUInteger)(m->total_bufs + 4)];

    int const tg_size = m->tg_size;
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

static void metal_submit_cmdbuf(struct metal_backend *be);

static void umbra_metal_run(
    struct umbra_metal *m, int l, int t, int r, int b, struct umbra_buf buf[]
) {
    int w = r - l, h = b - t;
    if (w <= 0 || h <= 0) { return; }

    struct metal_backend *be = (struct metal_backend*)m->base.backend;

    @autoreleasepool {
        if (!be->batch_cmdbuf) {
            id<MTLCommandQueue> queue =
                (__bridge id<MTLCommandQueue>)be->queue;
            be->batch_cmdbuf =
                (__bridge_retained void*)[queue commandBuffer];
        }
        // One MTLDispatchTypeSerial compute encoder for the entire batch:
        // matches what MoltenVK does for consecutive vkCmdDispatch calls.
        // Serial dispatch type guarantees that dispatch N+1 starts after
        // dispatch N completes within the encoder, and Metal's hazard
        // tracking on the plain commandBuffer (NOT
        // commandBufferWithUnretainedReferences) propagates writes between
        // dispatches. So no MTLFence chain or per-dispatch encoder churn
        // is needed; per-dispatch CPU overhead drops to setBuffer +
        // dispatchThreads.
        if (!be->batch_enc) {
            id<MTLCommandBuffer> cmdbuf =
                (__bridge id<MTLCommandBuffer>)be->batch_cmdbuf;
            be->batch_enc = (__bridge_retained void*)
                [cmdbuf computeCommandEncoderWithDispatchType:MTLDispatchTypeSerial];
        }
        id<MTLComputeCommandEncoder> enc =
            (__bridge id<MTLComputeCommandEncoder>)be->batch_enc;
        encode_dispatch(m, l, t, r, b, buf, enc);
    }

    if (uniform_ring_pool_should_rotate(&be->uni_pool)) {
        metal_submit_cmdbuf(be);
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
    be->frame_committed[be->uni_pool.cur] = be->batch_cmdbuf;
    be->batch_cmdbuf                      = NULL;
    uniform_ring_pool_rotate(&be->uni_pool);
}

static void umbra_metal_flush(struct metal_backend *be) {
    metal_submit_cmdbuf(be);
    uniform_ring_pool_drain_all(&be->uni_pool);
    @autoreleasepool {
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

        for (int i = 0; i < be->batch_cache_n; i++) {
            (void)(__bridge_transfer id)
                be->batch_cache[i].mtl;
        }
        be->batch_cache_n = 0;
    }
}

static void umbra_metal_free(struct umbra_metal *m) {
    @autoreleasepool {
        if (m->pipeline) {
            (void)(__bridge_transfer id)m->pipeline;
        }
    }
    free(m->deref);
    free(m->src);
    free(m);
}

static void umbra_dump_metal(
    struct umbra_metal const *m, FILE *f
) {
    if (m->src) {
        fputs(m->src, f);
    }
}

static void run_metal(struct umbra_program *prog, int l, int t, int r, int b, struct umbra_buf buf[]) {
    umbra_metal_run((struct umbra_metal*)prog, l, t, r, b, buf);
}
static void dump_metal(struct umbra_program const *prog, FILE *f) { umbra_dump_metal((struct umbra_metal const*)prog, f); }
static void free_metal(struct umbra_program *prog) { umbra_metal_free((struct umbra_metal*)prog); }
static struct umbra_program *compile_metal(struct umbra_backend           *be,
                                           BB const *bb) {
    struct umbra_metal *m = umbra_metal((struct metal_backend*)be, bb);
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
static int ring_rotations_metal(struct umbra_backend const *be) {
    return ((struct metal_backend const*)be)->uni_pool.rotations;
}
struct umbra_backend *umbra_backend_metal(void) {
    struct metal_backend *mbe = umbra_metal_backend_create();
    if (mbe) {
        mbe->base = (struct umbra_backend){
            .compile        = compile_metal,
            .flush          = flush_be_metal,
            .free           = free_be_metal,
            .ring_rotations = ring_rotations_metal,
        };
        return &mbe->base;
    }
    return 0;
}

#endif
