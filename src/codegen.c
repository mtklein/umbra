#include "../umbra.h"
#include "bb.h"

#ifdef __wasm__

struct umbra_codegen { int dummy; };
struct umbra_codegen* umbra_codegen(struct umbra_basic_block const *bb) { (void)bb; return 0; }
void umbra_codegen_run (struct umbra_codegen *cg, int n, umbra_buf buf[]) {
    (void)cg; (void)n; (void)buf;
}
void umbra_codegen_free(struct umbra_codegen *cg) { (void)cg; }
void umbra_codegen_dump(struct umbra_codegen const *cg, FILE *f) { (void)cg; (void)f; }

#else

#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct umbra_basic_block BB;

struct umbra_codegen {
    void *dl;
    void (*entry)(int, void**);
    char *src_path, *so_path, *src;
    int   nptr, :32;
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
                     int lo, int hi, char const *pad, _Bool varying) {
    for (int i = lo; i < hi; i++) {
        struct bb_inst const *inst = &bb->inst[i];

        if (output_type(inst->op) == OP_HALF) {
            switch (inst->op) {
                case op_imm_half:
                    emit(b, "%sfloat v%d = h2f(%u);\n", pad, i, (uint16_t)inst->imm);
                    break;
                case op_uni_half: {
                    int p = inst->ptr;
                    if (p < 0) {
                        emit(b, "%sfloat v%d = h2f(((u16*)pd%d)[%d]);\n", pad, i, ~p, inst->imm);
                    } else {
                        _Bool mixed = ptr_32[p] && ptr_16[p];
                        emit(b, mixed ? "%sfloat v%d = h2f(p%d_16[%d]);\n"
                                      : "%sfloat v%d = h2f(p%d[%d]);\n",
                             pad, i, p, inst->imm);
                    }
                } break;
                case op_load_half: {
                    int p = inst->ptr;
                    if (p < 0) {
                        emit(b, "%sfloat v%d = h2f(((u16*)pd%d)[i]);\n", pad, i, ~p);
                    } else {
                        _Bool mixed = ptr_32[p] && ptr_16[p];
                        emit(b, mixed ? "%sfloat v%d = h2f(p%d_16[i]);\n"
                                      : "%sfloat v%d = h2f(p%d[i]);\n", pad, i, p);
                    }
                } break;
                case op_gather_half: {
                    int p = inst->ptr;
                    if (p < 0) {
                        emit(b, "%sfloat v%d = h2f(((u16*)pd%d)[v%d]);\n", pad, i, ~p, inst->x);
                    } else {
                        _Bool mixed = ptr_32[p] && ptr_16[p];
                        emit(b, mixed ? "%sfloat v%d = h2f(p%d_16[v%d]);\n"
                                      : "%sfloat v%d = h2f(p%d[v%d]);\n", pad, i, p, inst->x);
                    }
                } break;
                case op_store_half: {
                    int p = inst->ptr;
                    if (p < 0) {
                        emit(b, "%s((u16*)pd%d)[i] = f2h(v%d);\n", pad, ~p, inst->y);
                    } else {
                        _Bool mixed = ptr_32[p] && ptr_16[p];
                        emit(b, mixed ? "%sp%d_16[i] = f2h(v%d);\n"
                                      : "%sp%d[i] = f2h(v%d);\n", pad, p, inst->y);
                    }
                } break;
                case op_scatter_half: {
                    int p = inst->ptr;
                    if (p < 0) {
                        emit(b, "%s((u16*)pd%d)[v%d] = f2h(v%d);\n", pad, ~p, inst->x, inst->y);
                    } else {
                        _Bool mixed = ptr_32[p] && ptr_16[p];
                        emit(b, mixed ? "%sp%d_16[v%d] = f2h(v%d);\n"
                                      : "%sp%d[v%d] = f2h(v%d);\n", pad, p, inst->x, inst->y);
                    }
                } break;
                case op_add_half:  emit(b, "%sfloat v%d = v%d + v%d;\n",       pad, i, inst->x, inst->y); break;
                case op_sub_half:  emit(b, "%sfloat v%d = v%d - v%d;\n",       pad, i, inst->x, inst->y); break;
                case op_mul_half:  emit(b, "%sfloat v%d = v%d * v%d;\n",       pad, i, inst->x, inst->y); break;
                case op_div_half:  emit(b, "%sfloat v%d = v%d / v%d;\n",       pad, i, inst->x, inst->y); break;
                case op_min_half:  emit(b, "%sfloat v%d = fminf(v%d, v%d);\n", pad, i, inst->x, inst->y); break;
                case op_max_half:  emit(b, "%sfloat v%d = fmaxf(v%d, v%d);\n", pad, i, inst->x, inst->y); break;
                case op_sqrt_half: emit(b, "%sfloat v%d = sqrtf(v%d);\n",      pad, i, inst->x); break;
                case op_fma_half:
                    emit(b, "%sfloat v%d = v%d * v%d + v%d;\n", pad, i, inst->x, inst->y, inst->z);
                    break;
                case op_fms_half:
                    emit(b, "%sfloat v%d = v%d - v%d * v%d;\n", pad, i, inst->z, inst->x, inst->y);
                    break;
                case op_and_half:
                    emit(b, "%sfloat v%d = u2f(f2u(v%d) & f2u(v%d));\n", pad, i, inst->x, inst->y);
                    break;
                case op_or_half:
                    emit(b, "%sfloat v%d = u2f(f2u(v%d) | f2u(v%d));\n", pad, i, inst->x, inst->y);
                    break;
                case op_xor_half:
                    emit(b, "%sfloat v%d = u2f(f2u(v%d) ^ f2u(v%d));\n", pad, i, inst->x, inst->y);
                    break;
                case op_sel_half:
                    emit(b, "%sfloat v%d = u2f((f2u(v%d) & f2u(v%d)) | (~f2u(v%d) & f2u(v%d)));\n",
                         pad, i, inst->x, inst->y, inst->x, inst->z);
                    break;
                case op_half_from_f32: emit(b, "%sfloat v%d = u2f(v%d);\n", pad, i, inst->x); break;
                case op_half_from_i32: emit(b, "%sfloat v%d = (float)(s32)v%d;\n", pad, i, inst->x); break;
                case op_half_from_i16: emit(b, "%sfloat v%d = (float)(s16)v%d;\n", pad, i, inst->x); break;
                case op_i16_from_half: emit(b, "%su16 v%d = (u16)(s16)v%d;\n", pad, i, inst->x); break;
                case op_eq_half: emit(b, "%sfloat v%d = u2f((u32)-(s32)(v%d == v%d));\n", pad, i, inst->x, inst->y); break;

                case op_lt_half: emit(b, "%sfloat v%d = u2f((u32)-(s32)(v%d <  v%d));\n", pad, i, inst->x, inst->y); break;
                case op_le_half: emit(b, "%sfloat v%d = u2f((u32)-(s32)(v%d <= v%d));\n", pad, i, inst->x, inst->y); break;
                case op_lane:
                case op_imm_16: case op_imm_32:
                case op_deref_ptr:
                case op_uni_16: case op_uni_32:
                case op_load_16: case op_load_32:
                case op_gather_16: case op_gather_32:
                case op_store_16: case op_store_32:
                case op_scatter_16: case op_scatter_32:
                case op_load_8x4: case op_store_8x4:
                case op_add_f32: case op_sub_f32: case op_mul_f32: case op_div_f32:
                case op_min_f32: case op_max_f32: case op_sqrt_f32: case op_fma_f32: case op_fms_f32:
                case op_add_i32: case op_sub_i32: case op_mul_i32:
                case op_shl_i32: case op_shr_u32: case op_shr_s32:
                case op_shl_i32_imm: case op_shr_u32_imm: case op_shr_s32_imm:
                case op_and_32: case op_or_32: case op_xor_32: case op_sel_32:
                case op_f32_from_i32: case op_i32_from_f32: case op_f32_from_half:
                case op_i32_from_half: case op_i32_from_i16:
                case op_eq_f32: case op_lt_f32: case op_le_f32:
                case op_eq_i32: case op_lt_s32: case op_le_s32: case op_lt_u32: case op_le_u32:
                case op_add_i16: case op_sub_i16: case op_mul_i16:
                case op_shl_i16: case op_shr_u16: case op_shr_s16:
                case op_shl_i16_imm: case op_shr_u16_imm: case op_shr_s16_imm:
                case op_and_16: case op_or_16: case op_xor_16: case op_sel_16:
                case op_i16_from_i32: case op_shr_narrow_u32:
                case op_eq_i16: case op_lt_s16: case op_le_s16: case op_lt_u16: case op_le_u16:
                    break;
            }
            if (is_store(inst->op) && i+1 < hi) { emit(b, "\n"); }
            continue;
        }

        char const *ty = output_type(inst->op) == OP_16 ? "u16" : "u32";

        switch (inst->op) {
            case op_lane:
                if (varying) { emit(b, "%su32 v%d = (u32)i;\n", pad, i); }
                break;

            case op_imm_16: emit(b, "%s%s v%d = %u;\n", pad, ty, i, (uint16_t)inst->imm); break;
            case op_imm_32: emit(b, "%s%s v%d = %uu;\n", pad, ty, i, (uint32_t)inst->imm); break;

            case op_deref_ptr: {
                int p = inst->ptr;
                emit(b, "%svoid *pd%d = *(void**)((char*)ptrs[%d] + %d);\n", pad, i, p, inst->imm);
            } break;

            case op_uni_16: {
                int p = inst->ptr;
                if (p < 0) {
                    emit(b, "%su16 v%d = ((u16*)pd%d)[%d];\n", pad, i, ~p, inst->imm);
                } else {
                    _Bool mixed = ptr_32[p] && ptr_16[p];
                    emit(b, mixed ? "%su16 v%d = p%d_16[%d];\n"
                                  : "%su16 v%d = p%d[%d];\n", pad, i, p, inst->imm);
                }
            } break;
            case op_load_16: {
                int p = inst->ptr;
                if (p < 0) {
                    emit(b, "%su16 v%d = ((u16*)pd%d)[i];\n", pad, i, ~p);
                } else {
                    _Bool mixed = ptr_32[p] && ptr_16[p];
                    emit(b, mixed ? "%su16 v%d = p%d_16[i];\n"
                                  : "%su16 v%d = p%d[i];\n", pad, i, p);
                }
            } break;
            case op_gather_16: {
                int p = inst->ptr;
                if (p < 0) {
                    emit(b, "%su16 v%d = ((u16*)pd%d)[v%d];\n", pad, i, ~p, inst->x);
                } else {
                    _Bool mixed = ptr_32[p] && ptr_16[p];
                    emit(b, mixed ? "%su16 v%d = p%d_16[v%d];\n"
                                  : "%su16 v%d = p%d[v%d];\n", pad, i, p, inst->x);
                }
            } break;
            case op_uni_32: {
                int p = inst->ptr;
                if (p < 0) {
                    emit(b, "%su32 v%d = ((u32*)pd%d)[%d];\n", pad, i, ~p, inst->imm);
                } else {
                    _Bool mixed = ptr_32[p] && ptr_16[p];
                    emit(b, mixed ? "%su32 v%d = p%d_32[%d];\n"
                                  : "%su32 v%d = p%d[%d];\n", pad, i, p, inst->imm);
                }
            } break;
            case op_load_32: {
                int p = inst->ptr;
                if (p < 0) {
                    emit(b, "%su32 v%d = ((u32*)pd%d)[i];\n", pad, i, ~p);
                } else {
                    _Bool mixed = ptr_32[p] && ptr_16[p];
                    emit(b, mixed ? "%su32 v%d = p%d_32[i];\n"
                                  : "%su32 v%d = p%d[i];\n", pad, i, p);
                }
            } break;
            case op_gather_32: {
                int p = inst->ptr;
                if (p < 0) {
                    emit(b, "%su32 v%d = ((u32*)pd%d)[v%d];\n", pad, i, ~p, inst->x);
                } else {
                    _Bool mixed = ptr_32[p] && ptr_16[p];
                    emit(b, mixed ? "%su32 v%d = p%d_32[v%d];\n"
                                  : "%su32 v%d = p%d[v%d];\n", pad, i, p, inst->x);
                }
            } break;

            case op_store_16: {
                int p = inst->ptr;
                if (p < 0) {
                    emit(b, "%s((u16*)pd%d)[i] = v%d;\n", pad, ~p, inst->y);
                } else {
                    _Bool mixed = ptr_32[p] && ptr_16[p];
                    emit(b, mixed ? "%sp%d_16[i] = v%d;\n" : "%sp%d[i] = v%d;\n", pad, p, inst->y);
                }
            } break;
            case op_scatter_16: {
                int p = inst->ptr;
                if (p < 0) {
                    emit(b, "%s((u16*)pd%d)[v%d] = v%d;\n", pad, ~p, inst->x, inst->y);
                } else {
                    _Bool mixed = ptr_32[p] && ptr_16[p];
                    emit(b, mixed ? "%sp%d_16[v%d] = v%d;\n" : "%sp%d[v%d] = v%d;\n", pad, p, inst->x, inst->y);
                }
            } break;
            case op_store_32: {
                int p = inst->ptr;
                if (p < 0) {
                    emit(b, "%s((u32*)pd%d)[i] = v%d;\n", pad, ~p, inst->y);
                } else {
                    _Bool mixed = ptr_32[p] && ptr_16[p];
                    emit(b, mixed ? "%sp%d_32[i] = v%d;\n"
                                  : "%sp%d[i] = v%d;\n", pad, p, inst->y);
                }
            } break;
            case op_scatter_32: {
                int p = inst->ptr;
                if (p < 0) {
                    emit(b, "%s((u32*)pd%d)[v%d] = v%d;\n", pad, ~p, inst->x, inst->y);
                } else {
                    _Bool mixed = ptr_32[p] && ptr_16[p];
                    emit(b, mixed ? "%sp%d_32[v%d] = v%d;\n"
                                  : "%sp%d[v%d] = v%d;\n", pad, p, inst->x, inst->y);
                }
            } break;

            #define BINOP(OP, EXPR) case OP: emit(b, "%s%s v%d = " EXPR ";\n", pad, ty, i, inst->x, inst->y); break;
            #define UNOP(OP, EXPR)  case OP: emit(b, "%s%s v%d = " EXPR ";\n", pad, ty, i, inst->x); break;

            BINOP(op_add_f32, "f2u(u2f(v%d) + u2f(v%d))")
            BINOP(op_sub_f32, "f2u(u2f(v%d) - u2f(v%d))")
            BINOP(op_mul_f32, "f2u(u2f(v%d) * u2f(v%d))")
            BINOP(op_div_f32, "f2u(u2f(v%d) / u2f(v%d))")
            BINOP(op_min_f32, "f2u(fminf(u2f(v%d), u2f(v%d)))")
            BINOP(op_max_f32, "f2u(fmaxf(u2f(v%d), u2f(v%d)))")
            UNOP (op_sqrt_f32,"f2u(sqrtf(u2f(v%d)))")

            BINOP(op_add_i32, "(u32)(v%d + v%d)")
            BINOP(op_sub_i32, "(u32)(v%d - v%d)")
            BINOP(op_mul_i32, "(u32)(v%d * v%d)")
            BINOP(op_shl_i32, "(u32)(v%d << v%d)")
            BINOP(op_shr_u32, "(u32)(v%d >> v%d)")
            BINOP(op_shr_s32, "(u32)((s32)v%d >> (s32)v%d)")

            case op_shl_i32_imm: emit(b, "%su32 v%d = (u32)(v%d << %d);\n", pad, i, inst->x, inst->imm); break;
            case op_shr_u32_imm: emit(b, "%su32 v%d = (u32)(v%d >> %d);\n", pad, i, inst->x, inst->imm); break;
            case op_shr_s32_imm: emit(b, "%su32 v%d = (u32)((s32)v%d >> %d);\n", pad, i, inst->x, inst->imm); break;
            BINOP(op_and_32,  "(u32)(v%d & v%d)")
            BINOP(op_or_32,   "(u32)(v%d | v%d)")
            BINOP(op_xor_32,  "(u32)(v%d ^ v%d)")

            BINOP(op_add_i16, "(u16)(v%d + v%d)")
            BINOP(op_sub_i16, "(u16)(v%d - v%d)")
            BINOP(op_mul_i16, "(u16)(v%d * v%d)")
            BINOP(op_shl_i16, "(u16)(v%d << v%d)")
            BINOP(op_shr_u16, "(u16)(v%d >> v%d)")
            BINOP(op_shr_s16, "(u16)((s16)v%d >> (s16)v%d)")

            case op_shl_i16_imm: emit(b, "%su16 v%d = (u16)(v%d << %d);\n", pad, i, inst->x, inst->imm); break;
            case op_shr_u16_imm: emit(b, "%su16 v%d = (u16)(v%d >> %d);\n", pad, i, inst->x, inst->imm); break;
            case op_shr_s16_imm: emit(b, "%su16 v%d = (u16)((s16)v%d >> %d);\n", pad, i, inst->x, inst->imm); break;
            BINOP(op_and_16,  "(u16)(v%d & v%d)")
            BINOP(op_or_16,   "(u16)(v%d | v%d)")
            BINOP(op_xor_16,  "(u16)(v%d ^ v%d)")

            UNOP(op_f32_from_i32,  "f2u((float)(s32)v%d)")
            UNOP(op_i32_from_f32,  "(u32)(s32)u2f(v%d)")
            UNOP(op_f32_from_half, "f2u(v%d)")
            UNOP(op_i32_from_half, "(u32)(s32)v%d")
            UNOP(op_i16_from_i32,  "(u16)(s16)v%d")
            case op_shr_narrow_u32: emit(b, "%su16 v%d = (u16)(v%d >> %d);\n", pad, i, inst->x, inst->imm); break;
            UNOP(op_i32_from_i16,  "(u32)(s32)(s16)v%d")

            BINOP(op_eq_f32, "(u32)-(s32)(u2f(v%d) == u2f(v%d))")

            BINOP(op_lt_f32, "(u32)-(s32)(u2f(v%d) <  u2f(v%d))")
            BINOP(op_le_f32, "(u32)-(s32)(u2f(v%d) <= u2f(v%d))")

            BINOP(op_eq_i32, "(u32)-(s32)((s32)v%d == (s32)v%d)")
            BINOP(op_lt_s32, "(u32)-(s32)((s32)v%d <  (s32)v%d)")
            BINOP(op_le_s32, "(u32)-(s32)((s32)v%d <= (s32)v%d)")
            BINOP(op_lt_u32, "(u32)-(s32)(v%d <  v%d)")
            BINOP(op_le_u32, "(u32)-(s32)(v%d <= v%d)")

            BINOP(op_eq_i16, "(u16)-(s16)((s16)v%d == (s16)v%d)")
            BINOP(op_lt_s16, "(u16)-(s16)((s16)v%d <  (s16)v%d)")
            BINOP(op_le_s16, "(u16)-(s16)((s16)v%d <= (s16)v%d)")
            BINOP(op_lt_u16, "(u16)-(s16)(v%d <  v%d)")
            BINOP(op_le_u16, "(u16)-(s16)(v%d <= v%d)")

            #undef BINOP
            #undef UNOP

            case op_sel_32:
                emit(b, "%su32 v%d = (v%d & v%d) | (~v%d & v%d);\n",
                     pad, i, inst->x, inst->y, inst->x, inst->z);
                break;
            case op_sel_16:
                emit(b, "%su16 v%d = (v%d & v%d) | (~v%d & v%d);\n",
                     pad, i, inst->x, inst->y, inst->x, inst->z);
                break;

            case op_fma_f32:
                emit(b, "%su32 v%d = f2u(u2f(v%d) * u2f(v%d) + u2f(v%d));\n",
                     pad, i, inst->x, inst->y, inst->z);
                break;
            case op_fms_f32:
                emit(b, "%su32 v%d = f2u(u2f(v%d) - u2f(v%d) * u2f(v%d));\n",
                     pad, i, inst->z, inst->x, inst->y);
                break;

            case op_load_8x4: {
                int ch = inst->x ? inst->imm : 0;
                int p  = inst->x ? bb->inst[inst->x].ptr : inst->ptr;
                if (p < 0) {
                    emit(b, "%su16 v%d = (u16)((unsigned char*)pd%d)[i*4+%d];\n", pad, i, ~p, ch);
                } else {
                    emit(b, "%su16 v%d = (u16)((unsigned char*)ptrs[%d])[i*4+%d];\n", pad, i, p, ch);
                }
            } break;
            case op_store_8x4: {
                int p = inst->ptr;
                if (p < 0) {
                    emit(b, "%s((unsigned char*)pd%d)[i*4+0] = (unsigned char)v%d;\n", pad, ~p, inst->x);
                    emit(b, "%s((unsigned char*)pd%d)[i*4+1] = (unsigned char)v%d;\n", pad, ~p, inst->y);
                    emit(b, "%s((unsigned char*)pd%d)[i*4+2] = (unsigned char)v%d;\n", pad, ~p, inst->z);
                    emit(b, "%s((unsigned char*)pd%d)[i*4+3] = (unsigned char)v%d;\n", pad, ~p, inst->w);
                } else {
                    emit(b, "%s((unsigned char*)ptrs[%d])[i*4+0] = (unsigned char)v%d;\n", pad, p, inst->x);
                    emit(b, "%s((unsigned char*)ptrs[%d])[i*4+1] = (unsigned char)v%d;\n", pad, p, inst->y);
                    emit(b, "%s((unsigned char*)ptrs[%d])[i*4+2] = (unsigned char)v%d;\n", pad, p, inst->z);
                    emit(b, "%s((unsigned char*)ptrs[%d])[i*4+3] = (unsigned char)v%d;\n", pad, p, inst->w);
                }
            } break;

            case op_store_half: case op_scatter_half: break;
            case op_uni_half: case op_load_half: case op_gather_half: break;
            case op_imm_half:
            case op_add_half: case op_sub_half: case op_mul_half: case op_div_half:
            case op_min_half: case op_max_half: case op_sqrt_half: case op_fma_half: case op_fms_half:
            case op_and_half: case op_or_half: case op_xor_half: case op_sel_half:
            case op_half_from_f32: case op_half_from_i32: case op_half_from_i16: case op_i16_from_half:
            case op_eq_half: case op_lt_half: case op_le_half:
                break;
        }

        if (is_store(inst->op) && i+1 < hi) { emit(b, "\n"); }
    }
}

struct umbra_codegen* umbra_codegen(BB const *bb) {
    int max_ptr = -1;
    for (int i = 0; i < bb->insts; i++) {
        if (has_ptr(bb->inst[i].op) && bb->inst[i].ptr >= 0) {
            if (bb->inst[i].ptr > max_ptr) { max_ptr = bb->inst[i].ptr; }
        }
    }

    _Bool *ptr_16 = calloc((size_t)(max_ptr + 2), 1);
    _Bool *ptr_32 = calloc((size_t)(max_ptr + 2), 1);
    for (int i = 0; i < bb->insts; i++) {
        enum op op = bb->inst[i].op;
        if (has_ptr(op) && bb->inst[i].ptr >= 0) {
            int p = bb->inst[i].ptr;
            if (op == op_deref_ptr || op == op_load_8x4 || op == op_store_8x4) { /* uses cast */ }
            else if (output_type(op) == OP_32) { ptr_32[p] = 1; }
            else                               { ptr_16[p] = 1; }
        }
    }

    Buf b = {0};

    emit(&b, "#include <stdint.h>\n");
    emit(&b, "#include <math.h>\n\n");

    emit(&b, "typedef uint32_t u32;\ntypedef uint16_t u16;\ntypedef  int32_t s32;\ntypedef  int16_t s16;\n\n");

    emit(&b, "static inline u32 f2u(float f) { union{float f;u32 u;} x; x.f=f; return x.u; }\n");
    emit(&b, "static inline float u2f(u32 u) { union{float f;u32 u;} x; x.u=u; return x.f; }\n");
    emit(&b, "static inline float h2f(u16 h) {\n");
    emit(&b, "    u32 sign=((u32)h>>15)<<31, exp=((u32)h>>10)&0x1f, mant=(u32)h&0x3ff;\n");
    emit(&b, "    u32 normal = sign | ((exp+112)<<23) | (mant<<13);\n");
    emit(&b, "    u32 zero   = sign;\n");
    emit(&b, "    u32 infnan = sign | (0xffu<<23) | (mant<<13);\n");
    emit(&b, "    u32 is_zero   = -(u32)(exp==0);\n");
    emit(&b, "    u32 is_infnan = -(u32)(exp==31);\n");
    emit(&b, "    u32 bits = (is_zero&zero) | (is_infnan&infnan) | (~is_zero&~is_infnan&normal);\n");
    emit(&b, "    return u2f(bits);\n}\n\n");

    emit(&b, "static inline u16 f2h(float f) {\n");
    emit(&b, "    u32 bits=f2u(f), sign=(bits>>31)<<15;\n");
    emit(&b, "    s32 exp=(s32)((bits>>23)&0xff)-127+15;\n");
    emit(&b, "    u32 mant=(bits>>13)&0x3ff, rb=(bits>>12)&1, st=-(u32)((bits&0xfff)!=0);\n");
    emit(&b, "    mant+=rb&(st|(mant&1));\n");
    emit(&b, "    u32 mo=mant>>10; exp+=(s32)mo; mant&=0x3ff;\n");
    emit(&b, "    u32 normal = sign|((u32)exp<<10)|mant;\n");
    emit(&b, "    u32 inf    = sign|0x7c00;\n");
    emit(&b, "    u32 infnan = sign|0x7c00|mant;\n");
    emit(&b, "    u32 is_of = -(u32)(exp>=31);\n");
    emit(&b, "    u32 is_uf = -(u32)(exp<=0);\n");
    emit(&b, "    u32 se=(bits>>23)&0xff;\n");
    emit(&b, "    u32 is_in = -(u32)(se==0xff);\n");
    emit(&b, "    u32 r = (is_uf&sign) | (is_of&~is_in&inf) | (is_in&infnan) | (~is_uf&~is_of&~is_in&normal);\n");
    emit(&b, "    return (u16)r;\n}\n\n");

    emit(&b, "void umbra_entry(int n, void **ptrs) {\n");

    for (int p = 0; p <= max_ptr; p++) {
        if (ptr_32[p] && ptr_16[p]) {
            emit(&b, "    u32* p%d_32 = (u32*)ptrs[%d];\n", p, p);
            emit(&b, "    u16* p%d_16 = (u16*)ptrs[%d];\n", p, p);
        } else if (ptr_32[p]) {
            emit(&b, "    u32* restrict p%d = (u32*)ptrs[%d];\n", p, p);
        } else if (ptr_16[p]) {
            emit(&b, "    u16* restrict p%d = (u16*)ptrs[%d];\n", p, p);
        }
    }

    emit_ops(&b, bb, ptr_16, ptr_32, 0, bb->preamble, "    ", 0);

    if (bb->preamble) { emit(&b, "\n"); }
    emit(&b, "    for (int i = 0; i < n; i++) {\n");
    emit_ops(&b, bb, ptr_16, ptr_32, bb->preamble, bb->insts, "        ", 1);
    emit(&b, "    }\n}\n");

    free(ptr_16);
    free(ptr_32);

    char tpl[] = "/tmp/umbra_XXXXXX";
    int fd = mkstemp(tpl);
    if (fd < 0) { free(b.buf); return 0; }

    char c_path[sizeof tpl + 2];
    snprintf(c_path, sizeof c_path, "%s.c", tpl);
    rename(tpl, c_path);

    FILE *fp = fdopen(fd, "w");
    if (!fp) { free(b.buf); remove(c_path); return 0; }
    fwrite(b.buf, 1, (size_t)b.len, fp);
    fclose(fp);

    char *src_copy = malloc((size_t)b.len + 1);
    __builtin_memcpy(src_copy, b.buf, (size_t)b.len);
    src_copy[b.len] = '\0';
    free(b.buf);

    size_t slen = __builtin_strlen(c_path);
    char *so_path = malloc(slen + 8);
    __builtin_memcpy(so_path, c_path, slen - 2);
    #ifdef __APPLE__
    __builtin_memcpy(so_path + slen - 2, ".dylib", 7);
    #else
    __builtin_memcpy(so_path + slen - 2, ".so", 4);
    #endif

    char cmd[512];
    #ifdef __APPLE__
    snprintf(cmd, sizeof cmd,
             "cc -O2 -g -std=c99 -shared -Wl,-install_name,umbra_codegen -o %s %s 2>&1",
             so_path, c_path);
    #else
    snprintf(cmd, sizeof cmd,
             "cc -O2 -g -std=c99 -shared -o %s %s 2>&1",
             so_path, c_path);
    #endif
    int rc = system(cmd);
    if (rc != 0) {
        remove(c_path);
        free(so_path);
        free(src_copy);
        return 0;
    }

    void *dl = dlopen(so_path, RTLD_NOW);
    if (!dl) {
        remove(c_path);
        remove(so_path);
        free(so_path);
        free(src_copy);
        return 0;
    }

    void (*entry)(int, void**) =
        (void (*)(int, void**))dlsym(dl, "umbra_entry");
    if (!entry) {
        dlclose(dl);
        remove(c_path);
        remove(so_path);
        free(so_path);
        free(src_copy);
        return 0;
    }

    struct umbra_codegen *cg = malloc(sizeof *cg);
    cg->dl       = dl;
    cg->entry    = entry;
    cg->src_path = malloc(slen + 1);
    __builtin_memcpy(cg->src_path, c_path, slen + 1);
    cg->so_path  = so_path;
    cg->src      = src_copy;
    cg->nptr     = max_ptr + 1;
    return cg;
}

void umbra_codegen_run(struct umbra_codegen *cg, int n, umbra_buf buf[]) {
    if (!cg) { return; }
    void *ptrs[16] = {0};
    for (int i = 0; i < cg->nptr && i < 16; i++) { ptrs[i] = buf[i].ptr; }
    cg->entry(n, ptrs);
}

void umbra_codegen_free(struct umbra_codegen *cg) {
    if (!cg) { return; }
    if (cg->dl) { dlclose(cg->dl); }
    if (cg->src_path) { remove(cg->src_path); free(cg->src_path); }
    if (cg->so_path)  { remove(cg->so_path);  free(cg->so_path);  }
    free(cg->src);
    free(cg);
}

void umbra_codegen_dump(struct umbra_codegen const *cg, FILE *f) {
    if (cg && cg->src) { fputs(cg->src, f); }
}

#endif
