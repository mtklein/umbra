#include "../umbra.h"
#include "bb.h"

#if !defined(__APPLE__) || defined(__wasm__)

struct umbra_metal { int dummy; };
struct umbra_metal* umbra_metal(struct umbra_basic_block const *bb) { (void)bb; return 0; }
void umbra_metal_run(struct umbra_metal *m, int n, void *p0, void *p1, void *p2, void *p3, void *p4, void *p5) {
    (void)m; (void)n; (void)p0; (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
}
void umbra_metal_free(struct umbra_metal *m) { (void)m; }
void umbra_metal_dump(struct umbra_metal const *m, FILE *f) { (void)m; (void)f; }

#else

#import <Metal/Metal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct umbra_basic_block BB;

struct umbra_metal {
    void *device;
    void *pipeline;
    void *queue;
    void *n_buf;
    void *bufs[6];
    int   buf_cap[6];
    int   elem_sz[6];
    char *src;
    int   max_ptr;
    int   tg_size;
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
        int n = vsnprintf(b->buf + b->len, (size_t)(b->cap - b->len), fmt, ap);
        va_end(ap);
        if (b->len + n < b->cap) {
            b->len += n;
            return;
        }
        b->cap = b->cap ? 2*b->cap : 4096;
        b->buf = realloc(b->buf, (size_t)b->cap);
    }
}

static void emit_ops(Buf *b, BB const *bb, _Bool *ptr_16, _Bool *ptr_32,
                     int lo, int hi, char const *pad) {
    for (int i = lo; i < hi; i++) {
        struct bb_inst const *inst = &bb->inst[i];

        if (op_type(inst->op) == OP_HALF) {
            switch (inst->op) {
                case op_imm_half:
                    emit(b, "%shalf v%d = as_type<half>((ushort)%uu);\n", pad, i, (uint16_t)inst->imm);
                    break;
                case op_uni_half: {
                    int p = inst->ptr;
                    _Bool mixed = ptr_32[p] && ptr_16[p];
                    emit(b, mixed ? "%shalf v%d = as_type<half>(p%d_16[%d]);\n"
                                  : "%shalf v%d = as_type<half>(((device const ushort*)p%d)[%d]);\n",
                         pad, i, p, bb->inst[inst->x].imm);
                } break;
                case op_load_half: {
                    int p = inst->ptr;
                    _Bool mixed = ptr_32[p] && ptr_16[p];
                    emit(b, mixed ? "%shalf v%d = as_type<half>(p%d_16[i]);\n"
                                  : "%shalf v%d = as_type<half>(((device ushort*)p%d)[i]);\n", pad, i, p);
                } break;
                case op_gather_half: {
                    int p = inst->ptr;
                    _Bool mixed = ptr_32[p] && ptr_16[p];
                    emit(b, mixed ? "%shalf v%d = as_type<half>(p%d_16[v%d]);\n"
                                  : "%shalf v%d = as_type<half>(((device ushort*)p%d)[v%d]);\n",
                         pad, i, p, inst->x);
                } break;
                case op_store_half: {
                    int p = inst->ptr;
                    _Bool mixed = ptr_32[p] && ptr_16[p];
                    emit(b, mixed ? "%sp%d_16[i] = as_type<ushort>(v%d);\n"
                                  : "%s((device ushort*)p%d)[i] = as_type<ushort>(v%d);\n", pad, p, inst->y);
                } break;
                case op_scatter_half: {
                    int p = inst->ptr;
                    _Bool mixed = ptr_32[p] && ptr_16[p];
                    emit(b, mixed ? "%sp%d_16[v%d] = as_type<ushort>(v%d);\n"
                                  : "%s((device ushort*)p%d)[v%d] = as_type<ushort>(v%d);\n",
                         pad, p, inst->x, inst->y);
                } break;
                case op_add_half:  emit(b, "%shalf v%d = v%d + v%d;\n",       pad, i, inst->x, inst->y); break;
                case op_sub_half:  emit(b, "%shalf v%d = v%d - v%d;\n",       pad, i, inst->x, inst->y); break;
                case op_mul_half:  emit(b, "%shalf v%d = v%d * v%d;\n",       pad, i, inst->x, inst->y); break;
                case op_div_half:  emit(b, "%shalf v%d = v%d / v%d;\n",       pad, i, inst->x, inst->y); break;
                case op_min_half:  emit(b, "%shalf v%d = min(v%d, v%d);\n",   pad, i, inst->x, inst->y); break;
                case op_max_half:  emit(b, "%shalf v%d = max(v%d, v%d);\n",   pad, i, inst->x, inst->y); break;
                case op_sqrt_half: emit(b, "%shalf v%d = sqrt(v%d);\n",       pad, i, inst->x); break;
                case op_fma_half:
                    emit(b, "%shalf v%d = fma(v%d, v%d, v%d);\n", pad, i, inst->x, inst->y, inst->z);
                    break;
                case op_and_half:
                    emit(b, "%shalf v%d = as_type<half>((ushort)(as_type<ushort>(v%d) & as_type<ushort>(v%d)));\n",
                         pad, i, inst->x, inst->y);
                    break;
                case op_or_half:
                    emit(b, "%shalf v%d = as_type<half>((ushort)(as_type<ushort>(v%d) | as_type<ushort>(v%d)));\n",
                         pad, i, inst->x, inst->y);
                    break;
                case op_xor_half:
                    emit(b, "%shalf v%d = as_type<half>((ushort)(as_type<ushort>(v%d) ^ as_type<ushort>(v%d)));\n",
                         pad, i, inst->x, inst->y);
                    break;
                case op_sel_half:
                    emit(b, "%shalf v%d = as_type<half>((ushort)((as_type<ushort>(v%d) & as_type<ushort>(v%d))"
                         " | (~as_type<ushort>(v%d) & as_type<ushort>(v%d))));\n",
                         pad, i, inst->x, inst->y, inst->x, inst->z);
                    break;
                case op_half_from_f32:
                    emit(b, "%shalf v%d = (half)as_type<float>(v%d);\n", pad, i, inst->x);
                    break;
                case op_half_from_i32:
                    emit(b, "%shalf v%d = (half)(int)v%d;\n", pad, i, inst->x);
                    break;
                case op_half_from_i16:
                    emit(b, "%shalf v%d = (half)(short)v%d;\n", pad, i, inst->x);
                    break;
                case op_i16_from_half:
                    emit(b, "%sushort v%d = (ushort)(short)v%d;\n", pad, i, inst->x);
                    break;
                case op_eq_half:
                    emit(b, "%shalf v%d = as_type<half>((ushort)(v%d == v%d ? 0xffff : 0));\n",
                         pad, i, inst->x, inst->y);
                    break;

                case op_lt_half:
                    emit(b, "%shalf v%d = as_type<half>((ushort)(v%d < v%d ? 0xffff : 0));\n",
                         pad, i, inst->x, inst->y);
                    break;
                case op_le_half:
                    emit(b, "%shalf v%d = as_type<half>((ushort)(v%d <= v%d ? 0xffff : 0));\n",
                         pad, i, inst->x, inst->y);
                    break;
                default: break;
            }
            if (is_store(inst->op) && i+1 < hi) { emit(b, "\n"); }
            continue;
        }

        switch (inst->op) {
            case op_lane:
                emit(b, "%suint v%d = i;\n", pad, i);
                break;

            case op_imm_16: emit(b, "%sushort v%d = %uu;\n", pad, i, (uint16_t)inst->imm); break;
            case op_imm_32: emit(b, "%suint v%d = %uu;\n",   pad, i, (uint32_t)inst->imm); break;

            case op_uni_16: {
                int p = inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b, mixed ? "%sushort v%d = p%d_16[%d];\n"
                              : "%sushort v%d = ((device const ushort*)p%d)[%d];\n",
                     pad, i, p, bb->inst[inst->x].imm);
            } break;
            case op_load_16: {
                int p = inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b, mixed ? "%sushort v%d = p%d_16[i];\n"
                              : "%sushort v%d = ((device ushort*)p%d)[i];\n", pad, i, p);
            } break;
            case op_gather_16: {
                int p = inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b, mixed ? "%sushort v%d = p%d_16[v%d];\n"
                              : "%sushort v%d = ((device ushort*)p%d)[v%d];\n", pad, i, p, inst->x);
            } break;
            case op_uni_32: {
                int p = inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b, mixed ? "%suint v%d = p%d_32[%d];\n"
                              : "%suint v%d = ((device const uint*)p%d)[%d];\n",
                     pad, i, p, bb->inst[inst->x].imm);
            } break;
            case op_load_32: {
                int p = inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b, mixed ? "%suint v%d = p%d_32[i];\n"
                              : "%suint v%d = ((device uint*)p%d)[i];\n", pad, i, p);
            } break;
            case op_gather_32: {
                int p = inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b, mixed ? "%suint v%d = p%d_32[v%d];\n"
                              : "%suint v%d = ((device uint*)p%d)[v%d];\n", pad, i, p, inst->x);
            } break;

            case op_store_16: {
                int p = inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b, mixed ? "%sp%d_16[i] = v%d;\n"
                              : "%s((device ushort*)p%d)[i] = v%d;\n", pad, p, inst->y);
            } break;
            case op_scatter_16: {
                int p = inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b, mixed ? "%sp%d_16[v%d] = v%d;\n"
                              : "%s((device ushort*)p%d)[v%d] = v%d;\n", pad, p, inst->x, inst->y);
            } break;
            case op_store_32: {
                int p = inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b, mixed ? "%sp%d_32[i] = v%d;\n"
                              : "%s((device uint*)p%d)[i] = v%d;\n", pad, p, inst->y);
            } break;
            case op_scatter_32: {
                int p = inst->ptr;
                _Bool mixed = ptr_32[p] && ptr_16[p];
                emit(b, mixed ? "%sp%d_32[v%d] = v%d;\n"
                              : "%s((device uint*)p%d)[v%d] = v%d;\n", pad, p, inst->x, inst->y);
            } break;

            case op_add_f32: emit(b, "%suint v%d = as_type<uint>(as_type<float>(v%d) + as_type<float>(v%d));\n", pad, i, inst->x, inst->y); break;
            case op_sub_f32: emit(b, "%suint v%d = as_type<uint>(as_type<float>(v%d) - as_type<float>(v%d));\n", pad, i, inst->x, inst->y); break;
            case op_mul_f32: emit(b, "%suint v%d = as_type<uint>(as_type<float>(v%d) * as_type<float>(v%d));\n", pad, i, inst->x, inst->y); break;
            case op_div_f32: emit(b, "%suint v%d = as_type<uint>(as_type<float>(v%d) / as_type<float>(v%d));\n", pad, i, inst->x, inst->y); break;
            case op_min_f32: emit(b, "%suint v%d = as_type<uint>(min(as_type<float>(v%d), as_type<float>(v%d)));\n", pad, i, inst->x, inst->y); break;
            case op_max_f32: emit(b, "%suint v%d = as_type<uint>(max(as_type<float>(v%d), as_type<float>(v%d)));\n", pad, i, inst->x, inst->y); break;
            case op_sqrt_f32: emit(b, "%suint v%d = as_type<uint>(sqrt(as_type<float>(v%d)));\n", pad, i, inst->x); break;
            case op_fma_f32:
                emit(b, "%suint v%d = as_type<uint>(fma(as_type<float>(v%d), as_type<float>(v%d), as_type<float>(v%d)));\n",
                     pad, i, inst->x, inst->y, inst->z);
                break;

            case op_add_i32: emit(b, "%suint v%d = v%d + v%d;\n", pad, i, inst->x, inst->y); break;
            case op_sub_i32: emit(b, "%suint v%d = v%d - v%d;\n", pad, i, inst->x, inst->y); break;
            case op_mul_i32: emit(b, "%suint v%d = v%d * v%d;\n", pad, i, inst->x, inst->y); break;
            case op_shl_i32: emit(b, "%suint v%d = v%d << v%d;\n", pad, i, inst->x, inst->y); break;
            case op_shr_u32: emit(b, "%suint v%d = v%d >> v%d;\n", pad, i, inst->x, inst->y); break;
            case op_shr_s32: emit(b, "%suint v%d = (uint)((int)v%d >> (int)v%d);\n", pad, i, inst->x, inst->y); break;

            case op_shl_i32_imm: emit(b, "%suint v%d = v%d << %d;\n", pad, i, inst->x, inst->imm); break;
            case op_shr_u32_imm: emit(b, "%suint v%d = v%d >> %d;\n", pad, i, inst->x, inst->imm); break;
            case op_shr_s32_imm: emit(b, "%suint v%d = (uint)((int)v%d >> %d);\n", pad, i, inst->x, inst->imm); break;

            case op_and_32: emit(b, "%suint v%d = v%d & v%d;\n", pad, i, inst->x, inst->y); break;
            case op_or_32:  emit(b, "%suint v%d = v%d | v%d;\n", pad, i, inst->x, inst->y); break;
            case op_xor_32: emit(b, "%suint v%d = v%d ^ v%d;\n", pad, i, inst->x, inst->y); break;

            case op_add_i16: emit(b, "%sushort v%d = v%d + v%d;\n", pad, i, inst->x, inst->y); break;
            case op_sub_i16: emit(b, "%sushort v%d = v%d - v%d;\n", pad, i, inst->x, inst->y); break;
            case op_mul_i16: emit(b, "%sushort v%d = v%d * v%d;\n", pad, i, inst->x, inst->y); break;
            case op_shl_i16: emit(b, "%sushort v%d = v%d << v%d;\n", pad, i, inst->x, inst->y); break;
            case op_shr_u16: emit(b, "%sushort v%d = v%d >> v%d;\n", pad, i, inst->x, inst->y); break;
            case op_shr_s16: emit(b, "%sushort v%d = (ushort)((short)v%d >> (short)v%d);\n", pad, i, inst->x, inst->y); break;

            case op_shl_i16_imm: emit(b, "%sushort v%d = v%d << %d;\n", pad, i, inst->x, inst->imm); break;
            case op_shr_u16_imm: emit(b, "%sushort v%d = v%d >> %d;\n", pad, i, inst->x, inst->imm); break;
            case op_shr_s16_imm: emit(b, "%sushort v%d = (ushort)((short)v%d >> %d);\n", pad, i, inst->x, inst->imm); break;

            case op_and_16: emit(b, "%sushort v%d = v%d & v%d;\n", pad, i, inst->x, inst->y); break;
            case op_or_16:  emit(b, "%sushort v%d = v%d | v%d;\n", pad, i, inst->x, inst->y); break;
            case op_xor_16: emit(b, "%sushort v%d = v%d ^ v%d;\n", pad, i, inst->x, inst->y); break;

            case op_f32_from_i32:
                emit(b, "%suint v%d = as_type<uint>((float)(int)v%d);\n", pad, i, inst->x);
                break;
            case op_i32_from_f32:
                emit(b, "%suint v%d = (uint)(int)as_type<float>(v%d);\n", pad, i, inst->x);
                break;
            case op_f32_from_half:
                emit(b, "%suint v%d = as_type<uint>((float)v%d);\n", pad, i, inst->x);
                break;
            case op_i32_from_half:
                emit(b, "%suint v%d = (uint)(int)v%d;\n", pad, i, inst->x);
                break;
            case op_i16_from_i32:
                emit(b, "%sushort v%d = (ushort)(short)v%d;\n", pad, i, inst->x);
                break;
            case op_i32_from_i16:
                emit(b, "%suint v%d = (uint)(int)(short)v%d;\n", pad, i, inst->x);
                break;

            case op_sel_32:
                emit(b, "%suint v%d = (v%d & v%d) | (~v%d & v%d);\n",
                     pad, i, inst->x, inst->y, inst->x, inst->z);
                break;
            case op_sel_16:
                emit(b, "%sushort v%d = (v%d & v%d) | (~v%d & v%d);\n",
                     pad, i, inst->x, inst->y, inst->x, inst->z);
                break;

            case op_eq_f32: emit(b, "%suint v%d = as_type<float>(v%d) == as_type<float>(v%d) ? 0xffffffffu : 0u;\n", pad, i, inst->x, inst->y); break;

            case op_lt_f32: emit(b, "%suint v%d = as_type<float>(v%d) <  as_type<float>(v%d) ? 0xffffffffu : 0u;\n", pad, i, inst->x, inst->y); break;
            case op_le_f32: emit(b, "%suint v%d = as_type<float>(v%d) <= as_type<float>(v%d) ? 0xffffffffu : 0u;\n", pad, i, inst->x, inst->y); break;

            case op_eq_i32: emit(b, "%suint v%d = (int)v%d == (int)v%d ? 0xffffffffu : 0u;\n", pad, i, inst->x, inst->y); break;

            case op_lt_s32: emit(b, "%suint v%d = (int)v%d <  (int)v%d ? 0xffffffffu : 0u;\n", pad, i, inst->x, inst->y); break;
            case op_le_s32: emit(b, "%suint v%d = (int)v%d <= (int)v%d ? 0xffffffffu : 0u;\n", pad, i, inst->x, inst->y); break;
            case op_lt_u32: emit(b, "%suint v%d = v%d <  v%d ? 0xffffffffu : 0u;\n", pad, i, inst->x, inst->y); break;
            case op_le_u32: emit(b, "%suint v%d = v%d <= v%d ? 0xffffffffu : 0u;\n", pad, i, inst->x, inst->y); break;

            case op_eq_i16: emit(b, "%sushort v%d = (short)v%d == (short)v%d ? 0xffffu : 0u;\n", pad, i, inst->x, inst->y); break;

            case op_lt_s16: emit(b, "%sushort v%d = (short)v%d <  (short)v%d ? 0xffffu : 0u;\n", pad, i, inst->x, inst->y); break;
            case op_le_s16: emit(b, "%sushort v%d = (short)v%d <= (short)v%d ? 0xffffu : 0u;\n", pad, i, inst->x, inst->y); break;
            case op_lt_u16: emit(b, "%sushort v%d = v%d <  v%d ? 0xffffu : 0u;\n", pad, i, inst->x, inst->y); break;
            case op_le_u16: emit(b, "%sushort v%d = v%d <= v%d ? 0xffffu : 0u;\n", pad, i, inst->x, inst->y); break;

            case op_load_8x4: {
                int ch = inst->x ? inst->imm : 0;
                int p  = inst->x ? bb->inst[inst->x].ptr : inst->ptr;
                emit(b, "%sushort v%d = (ushort)((device uchar*)p%d)[i*4+%d];\n", pad, i, p, ch);
            } break;
            case op_store_8x4: {
                int p = inst->ptr;
                emit(b, "%s((device uchar*)p%d)[i*4+0] = (uchar)v%d;\n", pad, p, inst->x);
                emit(b, "%s((device uchar*)p%d)[i*4+1] = (uchar)v%d;\n", pad, p, inst->y);
                emit(b, "%s((device uchar*)p%d)[i*4+2] = (uchar)v%d;\n", pad, p, inst->z);
                emit(b, "%s((device uchar*)p%d)[i*4+3] = (uchar)v%d;\n", pad, p, inst->w);
            } break;

            case op_store_half: case op_scatter_half: break;
            case op_uni_half: case op_load_half: case op_gather_half: break;
            default: break;
        }

        if (is_store(inst->op) && i+1 < hi) { emit(b, "\n"); }
    }
}

static char* build_source(BB const *bb, int *out_max_ptr) {
    int max_ptr = -1;
    for (int i = 0; i < bb->insts; i++) {
        if (has_ptr(bb->inst[i].op)) {
            if (bb->inst[i].ptr > max_ptr) { max_ptr = bb->inst[i].ptr; }
        }
    }
    *out_max_ptr = max_ptr;

    _Bool *ptr_16 = calloc((size_t)(max_ptr + 2), 1);
    _Bool *ptr_32 = calloc((size_t)(max_ptr + 2), 1);
    for (int i = 0; i < bb->insts; i++) {
        enum op op = bb->inst[i].op;
        if (has_ptr(op)) {
            int p = bb->inst[i].ptr;
            if (op == op_load_8x4 || op == op_store_8x4) { /* uses (uchar*) cast */ }
            else if (op_type(op) == OP_32) { ptr_32[p] = 1; }
            else                           { ptr_16[p] = 1; }
        }
    }

    Buf b = {0};

    emit(&b, "#include <metal_stdlib>\nusing namespace metal;\n\n");

    emit(&b, "kernel void umbra_entry(\n");
    emit(&b, "    constant uint &n [[buffer(0)]]");
    for (int p = 0; p <= max_ptr; p++) {
        emit(&b, ",\n    device uchar *p%d [[buffer(%d)]]", p, p + 1);
    }
    emit(&b, ",\n    uint i [[thread_position_in_grid]]\n) {\n");
    emit(&b, "    if (i >= n) return;\n");

    for (int p = 0; p <= max_ptr; p++) {
        if (ptr_32[p] && ptr_16[p]) {
            emit(&b, "    device uint   *p%d_32 = (device uint*)p%d;\n", p, p);
            emit(&b, "    device ushort *p%d_16 = (device ushort*)p%d;\n", p, p);
        }
    }

    emit_ops(&b, bb, ptr_16, ptr_32, 0, bb->insts, "    ");
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
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) return 0;

        int max_ptr = -1;
        char *src = build_source(bb, &max_ptr);

        NSString *source = [NSString stringWithUTF8String:src];
        NSError *error = nil;
        id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
        if (!library) {
            NSLog(@"Metal compile error: %@", error);
            free(src);
            return 0;
        }

        id<MTLFunction> func = [library newFunctionWithName:@"umbra_entry"];
        if (!func) { free(src); return 0; }

        id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:func error:&error];
        if (!pipeline) { free(src); return 0; }

        id<MTLCommandQueue> queue = [device newCommandQueue];
        if (!queue) { free(src); return 0; }

        id<MTLBuffer> n_buf = [device newBufferWithLength:sizeof(uint32_t)
                                                   options:MTLResourceStorageModeShared];

        // Determine element size per pointer from the ops that access it.
        int elem_sz[6] = {0,0,0,0,0,0};
        for (int i = 0; i < bb->insts; i++) {
            enum op op = bb->inst[i].op;
            if (!has_ptr(op)) continue;
            int p = bb->inst[i].ptr;
            if (p >= 6) continue;
            if (op == op_load_8x4 || op == op_store_8x4) { if (elem_sz[p] < 4) elem_sz[p] = 4; }
            else if (op_type(op) == OP_32)                { if (elem_sz[p] < 4) elem_sz[p] = 4; }
            else                                          { if (elem_sz[p] < 2) elem_sz[p] = 2; }
        }

        struct umbra_metal *m = calloc(1, sizeof *m);
        m->device   = (__bridge_retained void*)device;
        m->pipeline = (__bridge_retained void*)pipeline;
        m->queue    = (__bridge_retained void*)queue;
        m->n_buf    = (__bridge_retained void*)n_buf;
        m->src      = src;
        NSUInteger tg = pipeline.maxTotalThreadsPerThreadgroup;
        if (tg > 256) tg = 256;

        m->max_ptr  = max_ptr;
        m->tg_size  = (int)tg;
        for (int p = 0; p < 6; p++) { m->elem_sz[p] = elem_sz[p]; }
        return m;
    }
}

void umbra_metal_run(struct umbra_metal *m, int n,
                     void *p0, void *p1, void *p2, void *p3, void *p4, void *p5) {
    if (!m || n <= 0) return;

    @autoreleasepool {
        id<MTLDevice> device = (__bridge id<MTLDevice>)m->device;
        id<MTLComputePipelineState> pipeline = (__bridge id<MTLComputePipelineState>)m->pipeline;
        id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)m->queue;
        id<MTLBuffer> n_buf = (__bridge id<MTLBuffer>)m->n_buf;

        *(uint32_t*)n_buf.contents = (uint32_t)n;

        void *ptrs[] = {p0, p1, p2, p3, p4, p5};

        for (int p = 0; p <= m->max_ptr && p < 6; p++) {
            if (!ptrs[p]) continue;
            int const sz = n * m->elem_sz[p];
            if (sz > m->buf_cap[p]) {
                if (m->bufs[p]) { (void)(__bridge_transfer id)m->bufs[p]; }
                m->buf_cap[p] = sz;
                id<MTLBuffer> buf = [device newBufferWithLength:(NSUInteger)sz
                                                       options:MTLResourceStorageModeShared];
                m->bufs[p] = (__bridge_retained void*)buf;
            }
            __builtin_memcpy(((__bridge id<MTLBuffer>)m->bufs[p]).contents, ptrs[p], (size_t)sz);
        }

        id<MTLCommandBuffer> cmdbuf = [queue commandBufferWithUnretainedReferences];
        id<MTLComputeCommandEncoder> enc = [cmdbuf computeCommandEncoder];
        [enc setComputePipelineState:pipeline];
        [enc setBuffer:n_buf offset:0 atIndex:0];
        for (int p = 0; p <= m->max_ptr && p < 6; p++) {
            if (m->bufs[p]) [enc setBuffer:(__bridge id<MTLBuffer>)m->bufs[p]
                                    offset:0 atIndex:(NSUInteger)(p + 1)];
        }

        MTLSize grid = MTLSizeMake((NSUInteger)n, 1, 1);
        MTLSize group = MTLSizeMake((NSUInteger)m->tg_size, 1, 1);
        [enc dispatchThreads:grid threadsPerThreadgroup:group];
        [enc endEncoding];
        [cmdbuf commit];
        [cmdbuf waitUntilCompleted];

        for (int p = 0; p <= m->max_ptr && p < 6; p++) {
            if (!ptrs[p] || !m->bufs[p]) continue;
            __builtin_memcpy(ptrs[p], ((__bridge id<MTLBuffer>)m->bufs[p]).contents,
                             (size_t)(n * m->elem_sz[p]));
        }
    }
}

void umbra_metal_free(struct umbra_metal *m) {
    if (!m) return;
    @autoreleasepool {
        if (m->device)   { (void)(__bridge_transfer id)m->device; }
        if (m->pipeline) { (void)(__bridge_transfer id)m->pipeline; }
        if (m->queue)    { (void)(__bridge_transfer id)m->queue; }
        if (m->n_buf)    { (void)(__bridge_transfer id)m->n_buf; }
        for (int p = 0; p < 6; p++) {
            if (m->bufs[p]) { (void)(__bridge_transfer id)m->bufs[p]; }
        }
    }
    free(m->src);
    free(m);
}

void umbra_metal_dump(struct umbra_metal const *m, FILE *f) {
    if (m && m->src) { fputs(m->src, f); }
}

#endif
