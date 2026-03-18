#include "backend.h"
#include "bb.h"

#if !defined(__APPLE__) || defined(__wasm__)

struct umbra_metal { int dummy; };
struct umbra_metal* umbra_metal(
    struct umbra_basic_block const *bb
) { (void)bb; return 0; }
void umbra_metal_run(
    struct umbra_metal *m, int n, umbra_buf buf[]
) {
    (void)m; (void)n; (void)buf;
}
void umbra_metal_begin_batch(struct umbra_metal *m) {
    (void)m;
}
void umbra_metal_flush(struct umbra_metal *m) {
    (void)m;
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

struct umbra_metal {
    void *device;
    void *pipeline;
    void *queue;
    void *n_buf;
    void *sz_buf;
    void **bufs;
    long  *buf_cap;
    void **per_bufs;
    char  *src;
    int    max_ptr;
    int    total_bufs;
    int    tg_size;
    int    n_deref;
    struct deref_info *deref;

    void                *batch_cmdbuf;
    void                *batch_enc;
    void               **batch_bufs;
    int                  batch_nbufs, batch_bufs_cap;
    struct batch_shared *batch_data;
    struct copyback     *batch_copy;
    int                  batch_ncopy, batch_copy_cap;
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
    return op >= op_uni_16 && op <= op_scatter_16;
}
static _Bool is_32(enum op op) {
    return op >= op_uni_32 && op <= op_scatter_32;
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

            case op_imm_32:
                emit(b, "%suint v%d = %uu;\n",
                     pad, i, (uint32_t)inst->imm);
                break;

            case op_deref_ptr: break;

            case op_uni_32: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                if (inst->x) {
                    emit(b, mixed
                        ? "%suint v%d = "
                          "p%d_32[v%d];\n"
                        : "%suint v%d = "
                          "((device const uint*)"
                          "p%d)[v%d];\n",
                         pad, i, p, inst->x);
                } else {
                    emit(b, mixed
                        ? "%suint v%d = p%d_32[%d];\n"
                        : "%suint v%d = "
                          "((device const uint*)"
                          "p%d)[%d];\n",
                         pad, i, p, inst->imm);
                }
            } break;
            case op_load_32: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                if (inst->x) {
                    emit(b, mixed
                        ? "%suint v%d = "
                          "p%d_32[i+(int)v%d];\n"
                        : "%suint v%d = "
                          "((device uint*)p%d)"
                          "[i+(int)v%d];\n",
                         pad, i, p, inst->x);
                } else {
                    emit(b, mixed
                        ? "%suint v%d = p%d_32[i];\n"
                        : "%suint v%d = "
                          "((device uint*)p%d)[i];\n",
                         pad, i, p);
                }
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
            case op_store_32: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                if (inst->x) {
                    emit(b, mixed
                        ? "%sp%d_32[i+(int)v%d]"
                          " = v%d;\n"
                        : "%s((device uint*)p%d)"
                          "[i+(int)v%d] = v%d;\n",
                         pad, p, inst->x, inst->y);
                } else {
                    emit(b, mixed
                        ? "%sp%d_32[i] = v%d;\n"
                        : "%s((device uint*)p%d)"
                          "[i] = v%d;\n",
                         pad, p, inst->y);
                }
            } break;
            case op_scatter_32: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b, mixed
                    ? "%sp%d_32"
                      "[clamp_ix((int)v%d,"
                      "buf_szs[%d],4)] = v%d;\n"
                    : "%s((device uint*)p%d)"
                      "[clamp_ix((int)v%d,"
                      "buf_szs[%d],4)] = v%d;\n",
                     pad, p, inst->x, p, inst->y);
            } break;

            case op_uni_16: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                if (inst->x) {
                    emit(b, mixed
                        ? "%suint v%d = "
                          "(uint)(ushort)"
                          "p%d_16[v%d];\n"
                        : "%suint v%d = (uint)"
                          "((device const ushort*)"
                          "p%d)[v%d];\n",
                         pad, i, p, inst->x);
                } else {
                    emit(b, mixed
                        ? "%suint v%d = "
                          "(uint)(ushort)"
                          "p%d_16[%d];\n"
                        : "%suint v%d = (uint)"
                          "((device const ushort*)"
                          "p%d)[%d];\n",
                         pad, i, p, inst->imm);
                }
            } break;
            case op_load_16: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                if (inst->x) {
                    emit(b, mixed
                        ? "%suint v%d = (uint)"
                          "(ushort)p%d_16"
                          "[i+(int)v%d];\n"
                        : "%suint v%d = (uint)"
                          "((device ushort*)p%d)"
                          "[i+(int)v%d];\n",
                         pad, i, p, inst->x);
                } else {
                    emit(b, mixed
                        ? "%suint v%d = (uint)"
                          "(ushort)p%d_16[i];\n"
                        : "%suint v%d = (uint)"
                          "((device ushort*)p%d)"
                          "[i];\n",
                         pad, i, p);
                }
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
            case op_store_16: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                if (inst->x) {
                    emit(b, mixed
                        ? "%sp%d_16[i+(int)v%d]"
                          " = (ushort)v%d;\n"
                        : "%s((device ushort*)p%d)"
                          "[i+(int)v%d]"
                          " = (ushort)v%d;\n",
                         pad, p, inst->x, inst->y);
                } else {
                    emit(b, mixed
                        ? "%sp%d_16[i]"
                          " = (ushort)v%d;\n"
                        : "%s((device ushort*)p%d)"
                          "[i] = (ushort)v%d;\n",
                         pad, p, inst->y);
                }
            } break;
            case op_scatter_16: {
                int p = inst->ptr < 0
                    ? deref_buf[~inst->ptr] : inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b, mixed
                    ? "%sp%d_16[clamp_ix((int)v%d,"
                      "buf_szs[%d],2)]"
                      " = (ushort)v%d;\n"
                    : "%s((device ushort*)p%d)"
                      "[clamp_ix((int)v%d,"
                      "buf_szs[%d],2)]"
                      " = (ushort)v%d;\n",
                     pad, p, inst->x, p, inst->y);
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

            case op_mul_f32_imm: break;
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
         "    constant uint &n [[buffer(0)]]");
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
         ",\n    uint i"
         " [[thread_position_in_grid]]\n) {\n");
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

struct umbra_metal* umbra_metal(BB const *bb) {
    @autoreleasepool {
        id<MTLDevice> device =
            MTLCreateSystemDefaultDevice();
        if (!device) { return 0; }

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

        id<MTLCommandQueue> queue =
            [device newCommandQueue];
        if (!queue) {
            free(deref_buf); free(src); return 0;
        }

        id<MTLBuffer> n_buf =
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
        m->device   =
            (__bridge_retained void*)device;
        m->pipeline =
            (__bridge_retained void*)pipeline;
        m->queue    =
            (__bridge_retained void*)queue;
        m->n_buf    =
            (__bridge_retained void*)n_buf;
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
    struct umbra_metal *m,
    void *host, void *raw_ptr, long bytes
) {
    if (m->batch_ncopy >= m->batch_copy_cap) {
        m->batch_copy_cap = m->batch_copy_cap
            ? 2 * m->batch_copy_cap : 64;
        m->batch_copy = realloc(
            m->batch_copy,
            (size_t)m->batch_copy_cap
                * sizeof(struct copyback));
    }
    m->batch_copy[m->batch_ncopy++] =
        (struct copyback){host, raw_ptr, bytes};
}

static void batch_retain_buf(
    struct umbra_metal *m, void *retained
) {
    if (m->batch_nbufs >= m->batch_bufs_cap) {
        m->batch_bufs_cap = m->batch_bufs_cap
            ? 2 * m->batch_bufs_cap : 64;
        m->batch_bufs = realloc(
            m->batch_bufs,
            (size_t)m->batch_bufs_cap
                * sizeof(void*));
    }
    m->batch_bufs[m->batch_nbufs++] = retained;
}


static void encode_dispatch(
    struct umbra_metal *m, int n, umbra_buf buf[],
    id<MTLComputeCommandEncoder> enc,
    _Bool batching
) {
    id<MTLDevice> device =
        (__bridge id<MTLDevice>)m->device;

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
            m, (__bridge_retained void*)per_n);
    } else {
        per_n =
            (__bridge id<MTLBuffer>)m->n_buf;
        *(uint32_t*)per_n.contents = (uint32_t)n;
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
                } else {
                    batch_retain_buf(m, retained);
                    if (buf[i].sz > 0) {
                        batch_add_copy(
                            m, ptr,
                            retained, bytes);
                    }
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
                } else {
                    batch_retain_buf(m, retained);
                    if (dsz > 0) {
                        batch_add_copy(
                            m, dptr,
                            retained, bytes);
                    }
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
            m, (__bridge_retained void*)per_sz);
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

    MTLSize grid =
        MTLSizeMake((NSUInteger)n, 1, 1);
    MTLSize group =
        MTLSizeMake((NSUInteger)m->tg_size, 1, 1);
    [enc dispatchThreads:grid
       threadsPerThreadgroup:group];
    free(szs_data);
}

void umbra_metal_run(
    struct umbra_metal *m, int n, umbra_buf buf[]
) {
    if (!m || n <= 0) { return; }

    if (m->batch_cmdbuf) {
        @autoreleasepool {
            id<MTLComputeCommandEncoder> enc =
                (__bridge
                 id<MTLComputeCommandEncoder>)
                    m->batch_enc;
            encode_dispatch(m, n, buf, enc, 1);
        }
        return;
    }

    @autoreleasepool {
        id<MTLCommandQueue> queue =
            (__bridge id<MTLCommandQueue>)
                m->queue;

        id<MTLCommandBuffer> cmdbuf =
            [queue
             commandBufferWithUnretainedReferences];
        id<MTLComputeCommandEncoder> enc =
            [cmdbuf computeCommandEncoder];
        [enc setComputePipelineState:
            (__bridge
             id<MTLComputePipelineState>)
                m->pipeline];

        encode_dispatch(m, n, buf, enc, 0);

        [enc endEncoding];
        [cmdbuf commit];
        [cmdbuf waitUntilCompleted];

        for (int i = 0; i <= m->max_ptr; i++) {
            if (!buf[i].ptr || !m->bufs[i]
                    || buf[i].sz <= 0) {
                continue;
            }
            __builtin_memcpy(
                buf[i].ptr,
                ((__bridge id<MTLBuffer>)
                    m->bufs[i]).contents,
                (size_t)buf[i].sz);
        }

        for (int d = 0; d < m->n_deref; d++) {
            void *base =
                buf[m->deref[d].src_buf].ptr;
            long dsz;
            __builtin_memcpy(
                &dsz,
                (char*)base
                    + m->deref[d].byte_off + 8,
                sizeof dsz);
            if (dsz <= 0) { continue; }
            void *derived;
            __builtin_memcpy(
                &derived,
                (char*)base
                    + m->deref[d].byte_off,
                sizeof derived);
            int bi = m->deref[d].buf_idx;
            __builtin_memcpy(
                derived,
                ((__bridge id<MTLBuffer>)
                    m->bufs[bi]).contents,
                (size_t)dsz);
        }
    }
}

void umbra_metal_begin_batch(
    struct umbra_metal *m
) {
    if (!m || m->batch_cmdbuf) { return; }
    @autoreleasepool {
        id<MTLCommandQueue> queue =
            (__bridge id<MTLCommandQueue>)
                m->queue;
        id<MTLCommandBuffer> cmdbuf =
            [queue
             commandBufferWithUnretainedReferences];
        id<MTLComputeCommandEncoder> enc =
            [cmdbuf computeCommandEncoder];
        [enc setComputePipelineState:
            (__bridge
             id<MTLComputePipelineState>)
                m->pipeline];
        m->batch_cmdbuf =
            (__bridge_retained void*)cmdbuf;
        m->batch_enc =
            (__bridge_retained void*)enc;

        if (!m->batch_data) {
            m->batch_data = calloc(
                (size_t)m->total_bufs,
                sizeof *m->batch_data);
        } else {
            __builtin_memset(
                m->batch_data, 0,
                (size_t)m->total_bufs
                    * sizeof *m->batch_data);
        }
    }
}

void umbra_metal_flush(struct umbra_metal *m) {
    if (!m || !m->batch_cmdbuf) { return; }
    @autoreleasepool {
        id<MTLComputeCommandEncoder> enc =
            (__bridge_transfer
             id<MTLComputeCommandEncoder>)
                m->batch_enc;
        id<MTLCommandBuffer> cmdbuf =
            (__bridge_transfer id<MTLCommandBuffer>)
                m->batch_cmdbuf;
        m->batch_enc    = NULL;
        m->batch_cmdbuf = NULL;

        [enc endEncoding];
        [cmdbuf commit];
        [cmdbuf waitUntilCompleted];

        for (int i = 0; i < m->total_bufs; i++) {
            struct batch_shared *sh =
                &m->batch_data[i];
            if (sh->mtl && sh->copy_sz > 0) {
                __builtin_memcpy(
                    sh->host,
                    ((__bridge id<MTLBuffer>)
                        sh->mtl).contents,
                    (size_t)sh->copy_sz);
            }
            if (sh->mtl) {
                (void)(__bridge_transfer id)
                    sh->mtl;
            }
        }

        for (int i = 0; i < m->batch_ncopy; i++) {
            struct copyback *c =
                &m->batch_copy[i];
            id<MTLBuffer> mtlbuf =
                (__bridge id<MTLBuffer>)c->mtlbuf;
            __builtin_memcpy(
                c->host,
                mtlbuf.contents,
                (size_t)c->bytes);
        }
        m->batch_ncopy = 0;

        for (int i = 0; i < m->batch_nbufs; i++) {
            (void)(__bridge_transfer id)
                m->batch_bufs[i];
        }
        m->batch_nbufs = 0;
    }
}

void umbra_metal_free(struct umbra_metal *m) {
    if (!m) { return; }
    umbra_metal_flush(m);
    @autoreleasepool {
        if (m->device) {
            (void)(__bridge_transfer id)m->device;
        }
        if (m->pipeline) {
            (void)(__bridge_transfer id)
                m->pipeline;
        }
        if (m->queue) {
            (void)(__bridge_transfer id)m->queue;
        }
        if (m->n_buf) {
            (void)(__bridge_transfer id)m->n_buf;
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
    free(m->batch_bufs);
    free(m->batch_data);
    free(m->batch_copy);
    free(m->src);
    free(m);
}

void umbra_dump_metal(
    struct umbra_metal const *m, FILE *f
) {
    if (m && m->src) { fputs(m->src, f); }
}

#endif
