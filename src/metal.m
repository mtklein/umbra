#include "program.h"
#include "bb.h"

#if !defined(__APPLE__) || defined(__wasm__)

struct metal_backend { int dummy; };
struct umbra_metal  { int dummy; };
void* umbra_metal_backend_create(void) { return 0; }
void  umbra_metal_backend_free(void *ctx) {
    (void)ctx;
}
struct umbra_metal* umbra_metal(
    void *backend_ctx,
    struct umbra_basic_block const *bb
) { (void)backend_ctx; (void)bb; return 0; }
void umbra_metal_run(
    struct umbra_metal *m, int n, int w, umbra_buf buf[]
) {
    (void)m; (void)n; (void)w; (void)buf;
}
void umbra_metal_begin_batch(void *ctx) {
    (void)ctx;
}
void umbra_metal_flush(void *ctx) {
    (void)ctx;
}
void umbra_metal_free(struct umbra_metal *m) {
    (void)m;
}
void umbra_dump_metal(
    struct umbra_metal const *m, FILE *f
) { (void)m; (void)f; }

#else

#import <Metal/Metal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct umbra_basic_block BB;

struct batch_shared {
    void *mtl;
    char *host;
    long  copy_sz;
};

struct copyback {
    void *host;
    void *mtlbuf;
    long  bytes;
};

struct deref_info { int buf_idx, src_buf, byte_off; };

struct metal_backend {
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
    struct metal_backend *be;
    void *pipeline;
    void *n_buf;
    void *w_buf;
    void *sz_buf;
    void **bufs;
    long  *buf_cap;
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
    return op >= op_uni_16 && op <= op_gather_16;
}
static _Bool is_32(enum op op) {
    return op >= op_uni_32 && op <= op_deref_ptr;
}

static void emit_ops(Buf *b, BB const *bb,
                     _Bool *ptr_16, _Bool *ptr_32,
                     int const *deref_buf,
                     int lo, int hi,
                     char const *pad) {
    for (int i = lo; i < hi; i++) {
        struct bb_inst const *inst = &bb->inst[i];

        switch (inst->op) {
            case op_iota:
                emit(b, "%suint v%d = i;\n",
                     pad, i);
                break;
            case op_x:
                emit(b, "%suint v%d = pos.x;\n",
                     pad, i);
                break;
            case op_y:
                emit(b, "%suint v%d = pos.y;\n",
                     pad, i);
                break;

            case op_imm_32:
                emit(b, "%suint v%d = %uu;\n",
                     pad, i, (uint32_t)inst->imm);
                break;

            case op_deref_ptr: break;

            case op_uni_32: {
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
            case op_load_next_32: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b, mixed
                    ? "%suint v%d = p%d_32[i];\n"
                    : "%suint v%d = "
                      "((device uint*)p%d)[i];\n",
                     pad, i, p);
            } break;
            case op_load_next_64_lo:
            case op_load_next_64_hi: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                emit(b, "%suint v%d = "
                        "((device uint*)p%d)"
                        "[i*2+%d];\n",
                     pad, i, p,
                     inst->op == op_load_next_64_hi);
            } break;
            case op_gather_32: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b, mixed
                    ? "%suint v%d = p%d_32"
                      "[clamp_ix((int)v%d,"
                      "buf_szs[%d],4)];\n"
                    : "%suint v%d = "
                      "((device uint*)p%d)"
                      "[clamp_ix((int)v%d,"
                      "buf_szs[%d],4)];\n",
                     pad, i, p, inst->x, p);
            } break;
            case op_store_next_32: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b, mixed
                    ? "%sp%d_32[i] = v%d;\n"
                    : "%s((device uint*)p%d)"
                      "[i] = v%d;\n",
                     pad, p, inst->y);
            } break;
            case op_store_next_64: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                emit(b, "%s((device uint*)p%d)"
                        "[i*2] = v%d;\n"
                        "%s((device uint*)p%d)"
                        "[i*2+1] = v%d;\n",
                     pad, p, inst->x,
                     pad, p, inst->y);
            } break;

            case op_uni_16: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b, mixed
                    ? "%suint v%d = "
                      "(uint)(ushort)"
                      "p%d_16[%d];\n"
                    : "%suint v%d = (uint)"
                      "((device const ushort*)"
                      "p%d)[%d];\n",
                     pad, i, p, inst->imm);
            } break;
            case op_load_next_16: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b, mixed
                    ? "%suint v%d = (uint)(ushort)"
                      "p%d_16[i];\n"
                    : "%suint v%d = (uint)"
                      "((device ushort*)p%d)[i];\n",
                     pad, i, p);
            } break;
            case op_gather_16: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b, mixed
                    ? "%suint v%d = (uint)(ushort)"
                      "p%d_16[clamp_ix((int)v%d,"
                      "buf_szs[%d],2)];\n"
                    : "%suint v%d = (uint)"
                      "((device ushort*)p%d)"
                      "[clamp_ix((int)v%d,"
                      "buf_szs[%d],2)];\n",
                     pad, i, p, inst->x, p);
            } break;
            case op_store_next_16: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b, mixed
                    ? "%sp%d_16[i]"
                      " = (ushort)v%d;\n"
                    : "%s((device ushort*)p%d)"
                      "[i] = (ushort)v%d;\n",
                     pad, p, inst->y);
            } break;

            case op_widen_f16:
                emit(b,
                    "%suint v%d = as_type<uint>"
                    "((float)as_type<half>"
                    "((ushort)v%d));\n",
                     pad, i, inst->x);
                break;
            case op_narrow_f32:
                emit(b,
                    "%suint v%d = (uint)"
                    "as_type<ushort>"
                    "((half)as_type<float>"
                    "(v%d));\n",
                     pad, i, inst->x);
                break;

            case op_widen_s16:
                emit(b,
                    "%suint v%d = (uint)(int)"
                    "(short)(ushort)v%d;\n",
                     pad, i, inst->x);
                break;
            case op_widen_u16:
                emit(b,
                    "%suint v%d = (uint)"
                    "(ushort)v%d;\n",
                     pad, i, inst->x);
                break;
            case op_narrow_16:
                emit(b,
                    "%suint v%d = (uint)"
                    "(ushort)v%d;\n",
                     pad, i, inst->x);
                break;

            case op_join:
                emit(b, "%suint v%d = v%d;\n",
                     pad, i, inst->x);
                break;

            case op_shl_imm:
                emit(b, "%suint v%d = v%d << %du;\n",
                     pad, i, inst->x, inst->imm);
                break;
            case op_shr_u32_imm:
                emit(b, "%suint v%d = v%d >> %du;\n",
                     pad, i, inst->x, inst->imm);
                break;
            case op_shr_s32_imm:
                emit(b,
                    "%suint v%d ="
                    " (uint)((int)v%d >> %d);\n",
                    pad, i, inst->x, inst->imm);
                break;
            case op_and_imm:
                emit(b, "%suint v%d = v%d & %uu;\n",
                     pad, i, inst->x,
                     (uint32_t)inst->imm);
                break;

            case op_pack:
                emit(b,
                    "%suint v%d ="
                    " v%d | (v%d << %du);\n",
                    pad, i, inst->x,
                    inst->y, inst->imm);
                break;

            case op_add_f32_imm: case op_sub_f32_imm:
            case op_mul_f32_imm: case op_div_f32_imm:
            case op_min_f32_imm: case op_max_f32_imm:
            case op_add_i32_imm: case op_sub_i32_imm:
            case op_mul_i32_imm:
            case op_or_32_imm:  case op_xor_32_imm:
            case op_eq_f32_imm: case op_lt_f32_imm:
            case op_le_f32_imm:
            case op_eq_i32_imm: case op_lt_s32_imm:
            case op_le_s32_imm:
                break;
            case op_add_f32:
                emit(b, "%suint v%d = as_type<uint>"
                        "(as_type<float>(v%d)"
                        " + as_type<float>(v%d));\n",
                     pad, i, inst->x, inst->y);
                break;
            case op_sub_f32:
                emit(b, "%suint v%d = as_type<uint>"
                        "(as_type<float>(v%d)"
                        " - as_type<float>(v%d));\n",
                     pad, i, inst->x, inst->y);
                break;
            case op_mul_f32:
                emit(b, "%suint v%d = as_type<uint>"
                        "(as_type<float>(v%d)"
                        " * as_type<float>(v%d));\n",
                     pad, i, inst->x, inst->y);
                break;
            case op_div_f32:
                emit(b, "%suint v%d = as_type<uint>"
                        "(as_type<float>(v%d)"
                        " / as_type<float>(v%d));\n",
                     pad, i, inst->x, inst->y);
                break;
            case op_min_f32:
                emit(b, "%suint v%d = as_type<uint>"
                        "(min(as_type<float>(v%d),"
                        " as_type<float>(v%d)));\n",
                     pad, i, inst->x, inst->y);
                break;
            case op_max_f32:
                emit(b, "%suint v%d = as_type<uint>"
                        "(max(as_type<float>(v%d),"
                        " as_type<float>(v%d)));\n",
                     pad, i, inst->x, inst->y);
                break;
            case op_sqrt_f32:
                emit(b, "%suint v%d = as_type<uint>"
                        "(sqrt("
                        "as_type<float>(v%d)));\n",
                     pad, i, inst->x);
                break;
            case op_abs_f32:
                emit(b, "%suint v%d = as_type<uint>"
                        "(fabs("
                        "as_type<float>(v%d)));\n",
                     pad, i, inst->x);
                break;
            case op_neg_f32:
                emit(b, "%suint v%d = as_type<uint>"
                        "(-as_type<float>(v%d));\n",
                     pad, i, inst->x);
                break;
            case op_round_f32:
                emit(b, "%suint v%d = as_type<uint>"
                        "(rint("
                        "as_type<float>(v%d)));\n",
                     pad, i, inst->x);
                break;
            case op_floor_f32:
                emit(b, "%suint v%d = as_type<uint>"
                        "(floor("
                        "as_type<float>(v%d)));\n",
                     pad, i, inst->x);
                break;
            case op_ceil_f32:
                emit(b, "%suint v%d = as_type<uint>"
                        "(ceil("
                        "as_type<float>(v%d)));\n",
                     pad, i, inst->x);
                break;
            case op_round_i32:
                emit(b, "%suint v%d = as_type<uint>"
                        "((int)rint("
                        "as_type<float>(v%d)));\n",
                     pad, i, inst->x);
                break;
            case op_floor_i32:
                emit(b, "%suint v%d = as_type<uint>"
                        "((int)floor("
                        "as_type<float>(v%d)));\n",
                     pad, i, inst->x);
                break;
            case op_ceil_i32:
                emit(b, "%suint v%d = as_type<uint>"
                        "((int)ceil("
                        "as_type<float>(v%d)));\n",
                     pad, i, inst->x);
                break;
            case op_fma_f32:
                emit(b, "%suint v%d = as_type<uint>"
                        "(fma(as_type<float>(v%d),"
                        " as_type<float>(v%d),"
                        " as_type<float>(v%d)));\n",
                     pad, i,
                     inst->x, inst->y, inst->z);
                break;
            case op_fms_f32:
                emit(b, "%suint v%d = as_type<uint>"
                        "(as_type<float>(v%d)"
                        " - as_type<float>(v%d)"
                        " * as_type<float>(v%d));\n",
                     pad, i,
                     inst->z, inst->x, inst->y);
                break;

            case op_add_i32:
                emit(b, "%suint v%d = v%d + v%d;\n",
                     pad, i, inst->x, inst->y);
                break;
            case op_sub_i32:
                emit(b, "%suint v%d = v%d - v%d;\n",
                     pad, i, inst->x, inst->y);
                break;
            case op_mul_i32:
                emit(b, "%suint v%d = v%d * v%d;\n",
                     pad, i, inst->x, inst->y);
                break;
            case op_shl_i32:
                emit(b, "%suint v%d = v%d << v%d;\n",
                     pad, i, inst->x, inst->y);
                break;
            case op_shr_u32:
                emit(b, "%suint v%d = v%d >> v%d;\n",
                     pad, i, inst->x, inst->y);
                break;
            case op_shr_s32:
                emit(b, "%suint v%d = "
                        "(uint)((int)v%d"
                        " >> (int)v%d);\n",
                     pad, i, inst->x, inst->y);
                break;

            case op_and_32:
                emit(b, "%suint v%d = v%d & v%d;\n",
                     pad, i, inst->x, inst->y);
                break;
            case op_or_32:
                emit(b, "%suint v%d = v%d | v%d;\n",
                     pad, i, inst->x, inst->y);
                break;
            case op_xor_32:
                emit(b, "%suint v%d = v%d ^ v%d;\n",
                     pad, i, inst->x, inst->y);
                break;
            case op_sel_32:
                emit(b, "%suint v%d = "
                        "(v%d & v%d) | (~v%d & v%d);\n",
                     pad, i,
                     inst->x, inst->y,
                     inst->x, inst->z);
                break;

            case op_f32_from_i32:
                emit(b, "%suint v%d = "
                        "as_type<uint>"
                        "((float)(int)v%d);\n",
                     pad, i, inst->x);
                break;
            case op_i32_from_f32:
                emit(b, "%suint v%d = "
                        "(uint)(int)"
                        "as_type<float>(v%d);\n",
                     pad, i, inst->x);
                break;

            case op_eq_f32:
                emit(b, "%suint v%d = "
                        "as_type<float>(v%d) == "
                        "as_type<float>(v%d)"
                        " ? 0xffffffffu : 0u;\n",
                     pad, i, inst->x, inst->y);
                break;
            case op_lt_f32:
                emit(b, "%suint v%d = "
                        "as_type<float>(v%d) <  "
                        "as_type<float>(v%d)"
                        " ? 0xffffffffu : 0u;\n",
                     pad, i, inst->x, inst->y);
                break;
            case op_le_f32:
                emit(b, "%suint v%d = "
                        "as_type<float>(v%d) <= "
                        "as_type<float>(v%d)"
                        " ? 0xffffffffu : 0u;\n",
                     pad, i, inst->x, inst->y);
                break;

            case op_eq_i32:
                emit(b, "%suint v%d = "
                        "(int)v%d == (int)v%d"
                        " ? 0xffffffffu : 0u;\n",
                     pad, i, inst->x, inst->y);
                break;
            case op_lt_s32:
                emit(b, "%suint v%d = "
                        "(int)v%d <  (int)v%d"
                        " ? 0xffffffffu : 0u;\n",
                     pad, i, inst->x, inst->y);
                break;
            case op_le_s32:
                emit(b, "%suint v%d = "
                        "(int)v%d <= (int)v%d"
                        " ? 0xffffffffu : 0u;\n",
                     pad, i, inst->x, inst->y);
                break;
            case op_lt_u32:
                emit(b, "%suint v%d = "
                        "v%d <  v%d"
                        " ? 0xffffffffu : 0u;\n",
                     pad, i, inst->x, inst->y);
                break;
            case op_le_u32:
                emit(b, "%suint v%d = "
                        "v%d <= v%d"
                        " ? 0xffffffffu : 0u;\n",
                     pad, i, inst->x, inst->y);
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
            int p = bb->inst[i].ptr < 0
                ? deref_buf[~bb->inst[i].ptr]
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
         "static inline int clamp_ix"
         "(int ix, uint bytes, int elem) {\n");
    emit(&b,
         "    int hi = (int)"
         "(bytes / (uint)elem) - 1;\n");
    emit(&b, "    if (hi < 0) hi = 0;\n");
    emit(&b,
         "    return clamp(ix, 0, hi);\n}\n\n");

    emit(&b, "kernel void umbra_entry(\n");
    emit(&b,
         "    constant uint &n [[buffer(0)]]"
         ",\n    constant uint &w [[buffer(%d)]]",
         total_bufs + 2);
    for (int p = 0; p <= max_ptr; p++) {
        emit(&b,
             ",\n    device uchar *p%d"
             " [[buffer(%d)]]",
             p, p + 1);
    }
    for (int i = 0; i < bb->insts; i++) {
        if (bb->inst[i].op == op_deref_ptr) {
            emit(&b,
                 ",\n    device uchar *p%d"
                 " [[buffer(%d)]]",
                 deref_buf[i],
                 deref_buf[i] + 1);
        }
    }
    emit(&b,
         ",\n    constant uint *buf_szs"
         " [[buffer(%d)]]",
         total_bufs + 1);
    emit(&b,
         ",\n    uint2 pos"
         " [[thread_position_in_grid]]\n) {\n");
    emit(&b, "    uint i = pos.y * w + pos.x;\n");
    emit(&b, "    if (i >= n) return;\n");

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

    emit_ops(&b, bb, ptr_16, ptr_32,
             deref_buf, 0, bb->insts, "    ");
    emit(&b, "}\n");

    free(ptr_16);
    free(ptr_32);

    char *src = malloc((size_t)b.len + 1);
    __builtin_memcpy(src, b.buf, (size_t)b.len);
    src[b.len] = '\0';
    free(b.buf);
    return src;
}

void* umbra_metal_backend_create(void) {
    @autoreleasepool {
        id<MTLDevice> device =
            MTLCreateSystemDefaultDevice();
        if (!device) { return 0; }
        id<MTLCommandQueue> queue =
            [device newCommandQueue];
        if (!queue) { return 0; }
        struct metal_backend *be =
            calloc(1, sizeof *be);
        be->device =
            (__bridge_retained void*)device;
        be->queue =
            (__bridge_retained void*)queue;
        return be;
    }
}

void umbra_metal_backend_free(void *ctx) {
    struct metal_backend *be = ctx;
    if (!be) { return; }
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

struct umbra_metal* umbra_metal(
    void *backend_ctx, BB const *bb
) {
    struct metal_backend *be = backend_ctx;
    if (!be) { return 0; }
    @autoreleasepool {
        id<MTLDevice> device =
            (__bridge id<MTLDevice>)be->device;

        BB *resolved =
            umbra_resolve_joins(bb, NULL);

        int *deref_buf = calloc(
            (size_t)resolved->insts,
            sizeof *deref_buf);
        int max_ptr = -1, total_bufs = 0;
        char *src = build_source(
            resolved, &max_ptr, &total_bufs,
            deref_buf);

        int n_deref = 0;
        for (int i = 0; i < resolved->insts; i++) {
            if (resolved->inst[i].op == op_deref_ptr) {
                n_deref++;
            }
        }
        struct deref_info *di = calloc(
            (size_t)(n_deref ? n_deref : 1),
            sizeof *di);
        {
            int d = 0;
            for (int i = 0;
                 i < resolved->insts; i++) {
                if (resolved->inst[i].op
                        == op_deref_ptr) {
                    di[d].buf_idx = deref_buf[i];
                    di[d].src_buf =
                        resolved->inst[i].ptr;
                    di[d].byte_off =
                        resolved->inst[i].imm;
                    d++;
                }
            }
        }
        umbra_basic_block_free(resolved);

        NSString *source =
            [NSString stringWithUTF8String:src];
        NSError *error = nil;
        id<MTLLibrary> library =
            [device newLibraryWithSource:source
                                 options:nil
                                   error:&error];
        if (!library) {
            NSLog(@"Metal compile error: %@", error);
            free(deref_buf);
            free(src);
            return 0;
        }

        id<MTLFunction> func =
            [library
             newFunctionWithName:@"umbra_entry"];
        if (!func) {
            free(deref_buf); free(src); return 0;
        }

        id<MTLComputePipelineState> pipeline =
            [device
             newComputePipelineStateWithFunction:func
                                           error:&error];
        if (!pipeline) {
            free(deref_buf); free(src); return 0;
        }

        id<MTLBuffer> n_buf =
            [device
             newBufferWithLength:sizeof(uint32_t)
             options:MTLResourceStorageModeShared];
        id<MTLBuffer> w_buf =
            [device
             newBufferWithLength:sizeof(uint32_t)
             options:MTLResourceStorageModeShared];
        id<MTLBuffer> sz_buf =
            [device
             newBufferWithLength:
                 (NSUInteger)(total_bufs + 1)
                 * sizeof(uint32_t)
             options:MTLResourceStorageModeShared];

        struct umbra_metal *m =
            calloc(1, sizeof *m);
        m->be = be;
        m->pipeline =
            (__bridge_retained void*)pipeline;
        m->n_buf    =
            (__bridge_retained void*)n_buf;
        m->w_buf    =
            (__bridge_retained void*)w_buf;
        m->sz_buf   =
            (__bridge_retained void*)sz_buf;
        m->src        = src;
        m->max_ptr    = max_ptr;
        m->total_bufs = total_bufs;
        m->tg_size    =
            (int)pipeline.maxTotalThreadsPerThreadgroup;
        m->bufs     = calloc((size_t)total_bufs,
                             sizeof *m->bufs);
        m->buf_cap  = calloc((size_t)total_bufs,
                             sizeof *m->buf_cap);
        m->per_bufs = calloc((size_t)total_bufs,
                             sizeof *m->per_bufs);
        m->deref   = di;
        m->n_deref = n_deref;

        free(deref_buf);
        return m;
    }
}

static void batch_add_copy(
    struct metal_backend *be,
    void *host, void *raw_ptr, long bytes
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
    struct umbra_metal *m, int n, int w, umbra_buf buf[],
    id<MTLComputeCommandEncoder> enc,
    _Bool batching
) {
    struct metal_backend *be = m->be;
    id<MTLDevice> device =
        (__bridge id<MTLDevice>)be->device;

    int tb = m->total_bufs;
    uint32_t *szs_data = calloc((size_t)(tb + 1),
                                sizeof *szs_data);
    for (int i = 0; i <= m->max_ptr; i++) {
        if (buf[i].ptr && buf[i].sz) {
            long bytes = buf[i].sz < 0
                ? -buf[i].sz : buf[i].sz;
            szs_data[i] = (uint32_t)bytes;
        }
    }

    id<MTLBuffer> per_n = nil;
    id<MTLBuffer> per_w = nil;
    __builtin_memset(m->per_bufs, 0,
                     (size_t)tb * sizeof(void*));

    if (batching) {
        uint32_t n32 = (uint32_t)n;
        per_n =
            [device newBufferWithLength:sizeof n32
                    options:
                        MTLResourceStorageModeShared];
        *(uint32_t*)per_n.contents = n32;
        batch_retain_buf(
            be, (__bridge_retained void*)per_n);
        uint32_t w32 = (uint32_t)w;
        per_w =
            [device newBufferWithLength:sizeof w32
                    options:
                        MTLResourceStorageModeShared];
        *(uint32_t*)per_w.contents = w32;
        batch_retain_buf(
            be, (__bridge_retained void*)per_w);
    } else {
        per_n =
            (__bridge id<MTLBuffer>)m->n_buf;
        *(uint32_t*)per_n.contents = (uint32_t)n;
        per_w =
            (__bridge id<MTLBuffer>)m->w_buf;
        *(uint32_t*)per_w.contents = (uint32_t)w;
    }

    long offsets[32] = {0};
    for (int i = 0; i <= m->max_ptr; i++) {
        if (!buf[i].ptr || !buf[i].sz) { continue; }
        long bytes = buf[i].sz < 0
            ? -buf[i].sz : buf[i].sz;
        if (batching) {
            struct batch_shared *sh =
                &m->batch_data[i];
            char *ptr = buf[i].ptr;
            _Bool overlap = sh->mtl
                && ptr >= sh->host
                && ptr <  sh->host + sh->copy_sz;
            if (overlap) {
                offsets[i] = ptr - sh->host;
                m->per_bufs[i] = sh->mtl;
            } else {
                id<MTLBuffer> tmp =
                    [device
                     newBufferWithLength:
                         (NSUInteger)bytes
                     options:
                         MTLResourceStorageModeShared];
                __builtin_memcpy(
                    tmp.contents,
                    ptr, (size_t)bytes);
                void *retained =
                    (__bridge_retained void*)tmp;
                if (!sh->mtl) {
                    sh->mtl     = retained;
                    sh->host    = ptr;
                    sh->copy_sz = buf[i].sz > 0
                        ? bytes : 0;
                }
                batch_retain_buf(be, retained);
                if (buf[i].sz > 0) {
                    batch_add_copy(
                        be, ptr,
                        retained, bytes);
                }
                offsets[i] = 0;
                m->per_bufs[i] = retained;
            }
        } else {
            if (bytes > m->buf_cap[i]) {
                if (m->bufs[i]) {
                    (void)(__bridge_transfer id)
                        m->bufs[i];
                }
                m->buf_cap[i] = bytes;
                id<MTLBuffer> b =
                    [device
                     newBufferWithLength:
                         (NSUInteger)bytes
                     options:
                         MTLResourceStorageModeShared];
                m->bufs[i] =
                    (__bridge_retained void*)b;
            }
            __builtin_memcpy(
                ((__bridge id<MTLBuffer>)
                    m->bufs[i]).contents,
                buf[i].ptr, (size_t)bytes);
            m->per_bufs[i] = m->bufs[i];
        }
    }

    for (int d = 0; d < m->n_deref; d++) {
        void *base = buf[m->deref[d].src_buf].ptr;
        void *derived;
        long  dsz;
        __builtin_memcpy(
            &derived,
            (char*)base + m->deref[d].byte_off,
            sizeof derived);
        __builtin_memcpy(
            &dsz,
            (char*)base + m->deref[d].byte_off + 8,
            sizeof dsz);
        long bytes = dsz < 0 ? -dsz : dsz;
        int bi = m->deref[d].buf_idx;
        if (batching) {
            struct batch_shared *sh =
                &m->batch_data[bi];
            char *dptr = derived;
            _Bool overlap = sh->mtl
                && dptr >= sh->host
                && dptr <  sh->host + sh->copy_sz;
            if (overlap) {
                offsets[bi] = dptr - sh->host;
                m->per_bufs[bi] = sh->mtl;
            } else {
                id<MTLBuffer> tmp =
                    [device
                     newBufferWithLength:
                         (NSUInteger)bytes
                     options:
                         MTLResourceStorageModeShared];
                __builtin_memcpy(
                    tmp.contents,
                    dptr, (size_t)bytes);
                void *retained =
                    (__bridge_retained void*)tmp;
                if (!sh->mtl) {
                    sh->mtl     = retained;
                    sh->host    = dptr;
                    sh->copy_sz = dsz > 0
                        ? bytes : 0;
                }
                batch_retain_buf(be, retained);
                if (dsz > 0) {
                    batch_add_copy(
                        be, dptr,
                        retained, bytes);
                }
                offsets[bi] = 0;
                m->per_bufs[bi] = retained;
            }
        } else {
            if (bytes > m->buf_cap[bi]) {
                if (m->bufs[bi]) {
                    (void)(__bridge_transfer id)
                        m->bufs[bi];
                }
                m->buf_cap[bi] = bytes;
                id<MTLBuffer> b =
                    [device
                     newBufferWithLength:
                         (NSUInteger)bytes
                     options:
                         MTLResourceStorageModeShared];
                m->bufs[bi] =
                    (__bridge_retained void*)b;
            }
            __builtin_memcpy(
                ((__bridge id<MTLBuffer>)
                    m->bufs[bi]).contents,
                derived, (size_t)bytes);
            m->per_bufs[bi] = m->bufs[bi];
        }
        szs_data[bi] = (uint32_t)bytes;
    }

    size_t sz_bytes = (size_t)(tb + 1)
                    * sizeof(uint32_t);
    id<MTLBuffer> per_sz;
    if (batching) {
        per_sz =
            [device newBufferWithLength:sz_bytes
                    options:
                        MTLResourceStorageModeShared];
        __builtin_memcpy(
            per_sz.contents, szs_data, sz_bytes);
        batch_retain_buf(
            be, (__bridge_retained void*)per_sz);
    } else {
        per_sz =
            (__bridge id<MTLBuffer>)m->sz_buf;
        __builtin_memcpy(
            per_sz.contents,
            szs_data, sz_bytes);
    }

    [enc setBuffer:per_n offset:0 atIndex:0];
    for (int i = 0; i < m->total_bufs; i++) {
        if (m->per_bufs[i]) {
            [enc setBuffer:
                (__bridge id<MTLBuffer>)
                    m->per_bufs[i]
                offset:(NSUInteger)offsets[i]
               atIndex:(NSUInteger)(i + 1)];
        }
    }
    [enc setBuffer:per_sz
            offset:0
           atIndex:(NSUInteger)(m->total_bufs + 1)];
    [enc setBuffer:per_w
            offset:0
           atIndex:(NSUInteger)(m->total_bufs + 2)];

    int h = (n + w - 1) / w;
    MTLSize grid =
        MTLSizeMake((NSUInteger)w, (NSUInteger)h, 1);
    // Pick a 2D threadgroup size that fits within tg_size.
    int gx = 1, gy = m->tg_size;
    for (int x = 1; x * x <= m->tg_size; x++) {
        if (m->tg_size % x != 0) { continue; }
        int y = m->tg_size / x;
        if (x <= w && y <= h) { gx = x; gy = y; }
    }
    MTLSize group =
        MTLSizeMake((NSUInteger)gx,
                    (NSUInteger)gy, 1);
    [enc dispatchThreads:grid
       threadsPerThreadgroup:group];
    free(szs_data);
}

void umbra_metal_run(
    struct umbra_metal *m, int n, int w, umbra_buf buf[]
) {
    if (!m || n <= 0) { return; }
    struct metal_backend *be = m->be;
    if (!be->batch_cmdbuf) {
        umbra_metal_begin_batch(be);
    }
    @autoreleasepool {
        id<MTLComputeCommandEncoder> enc =
            (__bridge
             id<MTLComputeCommandEncoder>)
                be->batch_enc;
        [enc setComputePipelineState:
            (__bridge
             id<MTLComputePipelineState>)
                m->pipeline];
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
        encode_dispatch(m, n, w, buf, enc, 1);
    }
}

void umbra_metal_begin_batch(void *ctx) {
    struct metal_backend *be = ctx;
    if (!be || be->batch_cmdbuf) { return; }
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

void umbra_metal_flush(void *ctx) {
    struct metal_backend *be = ctx;
    if (!be || !be->batch_cmdbuf) { return; }
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

void umbra_metal_free(struct umbra_metal *m) {
    if (!m) { return; }
    @autoreleasepool {
        if (m->pipeline) {
            (void)(__bridge_transfer id)
                m->pipeline;
        }
        if (m->n_buf) {
            (void)(__bridge_transfer id)m->n_buf;
        }
        if (m->w_buf) {
            (void)(__bridge_transfer id)m->w_buf;
        }
        if (m->sz_buf) {
            (void)(__bridge_transfer id)m->sz_buf;
        }
        for (int p = 0; p < m->total_bufs; p++) {
            if (m->bufs[p]) {
                (void)(__bridge_transfer id)
                    m->bufs[p];
            }
        }
    }
    free(m->bufs);
    free(m->buf_cap);
    free(m->per_bufs);
    free(m->deref);
    free(m->batch_data);
    free(m->src);
    free(m);
}

void umbra_dump_metal(
    struct umbra_metal const *m, FILE *f
) {
    if (m && m->src) { fputs(m->src, f); }
}

#endif
