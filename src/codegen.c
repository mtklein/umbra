#include "../umbra.h"
#include "bb.h"

#ifdef __wasm__

struct umbra_codegen { int dummy; };
struct umbra_codegen* umbra_codegen(
    struct umbra_basic_block const *bb)
{
    (void)bb; return 0;
}
void umbra_codegen_run(
    struct umbra_codegen *cg,
    int n, umbra_buf buf[])
{
    (void)cg; (void)n; (void)buf;
}
void umbra_codegen_free(struct umbra_codegen *cg) {
    (void)cg;
}
void umbra_dump_codegen(
    struct umbra_codegen const *cg, FILE *f)
{
    (void)cg; (void)f;
}

#else

#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct umbra_basic_block BB;

struct umbra_codegen {
    void *dl;
    void (*entry)(int, void**, long*);
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

static _Bool is_16bit_mem_op(enum op op) {
    return op == op_uni_16
        || op == op_load_16
        || op == op_store_16
        || op == op_gather_16
        || op == op_scatter_16;
}

static _Bool is_32bit_mem_op(enum op op) {
    return op == op_uni_32
        || op == op_load_32
        || op == op_store_32
        || op == op_gather_32
        || op == op_scatter_32;
}

static void emit_ops(Buf *b, BB const *bb,
                     _Bool *ptr_16, _Bool *ptr_32,
                     int lo, int hi,
                     char const *pad,
                     _Bool varying) {
    for (int i = lo; i < hi; i++) {
        struct bb_inst const *inst = &bb->inst[i];

        switch (inst->op) {
            case op_lane:
                if (varying) {
                    emit(b, "%su32 v%d = (u32)i;\n",
                         pad, i);
                }
                break;

            case op_imm_32:
                emit(b, "%su32 v%d = %uu;\n",
                     pad, i, (uint32_t)inst->imm);
                break;

            case op_deref_ptr: {
                int p = inst->ptr;
                emit(b,
                    "%svoid *pd%d = "
                    "*(void**)((char*)ptrs[%d]"
                    " + %d);\n",
                    pad, i, p, inst->imm);
                emit(b,
                    "%slong szd%d = "
                    "*(long*)((char*)ptrs[%d]"
                    " + %d);\n",
                    pad, i, p, inst->imm + 8);
                emit(b,
                    "%sif (szd%d < 0)"
                    " szd%d = -szd%d;\n",
                    pad, i, i, i);
            } break;

            case op_uni_32: {
                int p = inst->ptr;
                if (p < 0) {
                    emit(b,
                        "%su32 v%d ="
                        " ((u32*)pd%d)[%d];\n",
                        pad, i, ~p, inst->imm);
                } else {
                    _Bool mx = ptr_32[p] && ptr_16[p];
                    emit(b,
                        mx ? "%su32 v%d ="
                             " p%d_32[%d];\n"
                           : "%su32 v%d ="
                             " p%d[%d];\n",
                        pad, i, p, inst->imm);
                }
            } break;
            case op_load_32: {
                int p = inst->ptr;
                if (inst->x) {
                    if (p < 0) {
                        emit(b,
                            "%su32 v%d = ((u32*)"
                            "pd%d)[i+(s32)v%d];\n",
                            pad, i, ~p, inst->x);
                    } else {
                        _Bool mx =
                            ptr_32[p] && ptr_16[p];
                        emit(b,
                            mx
                            ? "%su32 v%d ="
                              " p%d_32"
                              "[i+(s32)v%d];\n"
                            : "%su32 v%d ="
                              " p%d"
                              "[i+(s32)v%d];\n",
                            pad, i, p, inst->x);
                    }
                } else {
                    if (p < 0) {
                        emit(b,
                            "%su32 v%d ="
                            " ((u32*)pd%d)[i];\n",
                            pad, i, ~p);
                    } else {
                        _Bool mx =
                            ptr_32[p] && ptr_16[p];
                        emit(b,
                            mx
                            ? "%su32 v%d ="
                              " p%d_32[i];\n"
                            : "%su32 v%d ="
                              " p%d[i];\n",
                            pad, i, p);
                    }
                }
            } break;
            case op_gather_32: {
                int p = inst->ptr;
                if (p < 0) {
                    emit(b,
                        "%su32 v%d = ((u32*)pd%d)"
                        "[clamp_ix((s32)v%d"
                        ",szd%d,4)];\n",
                        pad, i, ~p, inst->x, ~p);
                } else {
                    _Bool mx =
                        ptr_32[p] && ptr_16[p];
                    emit(b,
                        mx
                        ? "%su32 v%d = p%d_32"
                          "[clamp_ix((s32)v%d"
                          ",sz%d,4)];\n"
                        : "%su32 v%d = p%d"
                          "[clamp_ix((s32)v%d"
                          ",sz%d,4)];\n",
                        pad, i, p, inst->x, p);
                }
            } break;
            case op_store_32: {
                int p = inst->ptr;
                if (inst->x) {
                    if (p < 0) {
                        emit(b,
                            "%s((u32*)pd%d)"
                            "[i+(s32)v%d]"
                            " = v%d;\n",
                            pad, ~p, inst->x,
                            inst->y);
                    } else {
                        _Bool mx =
                            ptr_32[p] && ptr_16[p];
                        emit(b,
                            mx
                            ? "%sp%d_32"
                              "[i+(s32)v%d]"
                              " = v%d;\n"
                            : "%sp%d[i+(s32)v%d]"
                              " = v%d;\n",
                            pad, p, inst->x,
                            inst->y);
                    }
                } else {
                    if (p < 0) {
                        emit(b,
                            "%s((u32*)pd%d)[i]"
                            " = v%d;\n",
                            pad, ~p, inst->y);
                    } else {
                        _Bool mx =
                            ptr_32[p] && ptr_16[p];
                        emit(b,
                            mx
                            ? "%sp%d_32[i]"
                              " = v%d;\n"
                            : "%sp%d[i] = v%d;\n",
                            pad, p, inst->y);
                    }
                }
            } break;
            case op_scatter_32: {
                int p = inst->ptr;
                if (p < 0) {
                    emit(b,
                        "%s((u32*)pd%d)"
                        "[clamp_ix((s32)v%d"
                        ",szd%d,4)]"
                        " = v%d;\n",
                        pad, ~p, inst->x,
                        ~p, inst->y);
                } else {
                    _Bool mx =
                        ptr_32[p] && ptr_16[p];
                    emit(b,
                        mx
                        ? "%sp%d_32"
                          "[clamp_ix((s32)v%d"
                          ",sz%d,4)]"
                          " = v%d;\n"
                        : "%sp%d"
                          "[clamp_ix((s32)v%d"
                          ",sz%d,4)]"
                          " = v%d;\n",
                        pad, p, inst->x,
                        p, inst->y);
                }
            } break;

            case op_uni_16: {
                int p = inst->ptr;
                if (p < 0) {
                    emit(b,
                        "%su32 v%d ="
                        " (u32)(u16)"
                        "((u16*)pd%d)[%d];\n",
                        pad, i, ~p, inst->imm);
                } else {
                    _Bool mx =
                        ptr_32[p] && ptr_16[p];
                    emit(b,
                        mx
                        ? "%su32 v%d ="
                          " (u32)(u16)"
                          "p%d_16[%d];\n"
                        : "%su32 v%d ="
                          " (u32)(u16)"
                          "p%d[%d];\n",
                        pad, i, p, inst->imm);
                }
            } break;
            case op_load_16: {
                int p = inst->ptr;
                if (inst->x) {
                    if (p < 0) {
                        emit(b,
                            "%su32 v%d ="
                            " (u32)(u16)"
                            "((u16*)pd%d)"
                            "[i+(s32)v%d];\n",
                            pad, i, ~p, inst->x);
                    } else {
                        _Bool mx =
                            ptr_32[p] && ptr_16[p];
                        emit(b,
                            mx
                            ? "%su32 v%d ="
                              " (u32)(u16)"
                              "p%d_16"
                              "[i+(s32)v%d];\n"
                            : "%su32 v%d ="
                              " (u32)(u16)"
                              "p%d"
                              "[i+(s32)v%d];\n",
                            pad, i, p, inst->x);
                    }
                } else {
                    if (p < 0) {
                        emit(b,
                            "%su32 v%d ="
                            " (u32)(u16)"
                            "((u16*)pd%d)[i];\n",
                            pad, i, ~p);
                    } else {
                        _Bool mx =
                            ptr_32[p] && ptr_16[p];
                        emit(b,
                            mx
                            ? "%su32 v%d ="
                              " (u32)(u16)"
                              "p%d_16[i];\n"
                            : "%su32 v%d ="
                              " (u32)(u16)"
                              "p%d[i];\n",
                            pad, i, p);
                    }
                }
            } break;
            case op_gather_16: {
                int p = inst->ptr;
                if (p < 0) {
                    emit(b,
                        "%su32 v%d ="
                        " (u32)(u16)"
                        "((u16*)pd%d)"
                        "[clamp_ix((s32)v%d"
                        ",szd%d,2)];\n",
                        pad, i, ~p, inst->x, ~p);
                } else {
                    _Bool mx =
                        ptr_32[p] && ptr_16[p];
                    emit(b,
                        mx
                        ? "%su32 v%d ="
                          " (u32)(u16)"
                          "p%d_16"
                          "[clamp_ix((s32)v%d"
                          ",sz%d,2)];\n"
                        : "%su32 v%d ="
                          " (u32)(u16)"
                          "p%d"
                          "[clamp_ix((s32)v%d"
                          ",sz%d,2)];\n",
                        pad, i, p, inst->x, p);
                }
            } break;
            case op_store_16: {
                int p = inst->ptr;
                if (inst->x) {
                    if (p < 0) {
                        emit(b,
                            "%s((u16*)pd%d)"
                            "[i+(s32)v%d]"
                            " = (u16)v%d;\n",
                            pad, ~p, inst->x,
                            inst->y);
                    } else {
                        _Bool mx =
                            ptr_32[p] && ptr_16[p];
                        emit(b,
                            mx
                            ? "%sp%d_16"
                              "[i+(s32)v%d]"
                              " = (u16)v%d;\n"
                            : "%sp%d[i+(s32)v%d]"
                              " = (u16)v%d;\n",
                            pad, p, inst->x,
                            inst->y);
                    }
                } else {
                    if (p < 0) {
                        emit(b,
                            "%s((u16*)pd%d)[i]"
                            " = (u16)v%d;\n",
                            pad, ~p, inst->y);
                    } else {
                        _Bool mx =
                            ptr_32[p] && ptr_16[p];
                        emit(b,
                            mx
                            ? "%sp%d_16[i]"
                              " = (u16)v%d;\n"
                            : "%sp%d[i]"
                              " = (u16)v%d;\n",
                            pad, p, inst->y);
                    }
                }
            } break;
            case op_scatter_16: {
                int p = inst->ptr;
                if (p < 0) {
                    emit(b,
                        "%s((u16*)pd%d)"
                        "[clamp_ix((s32)v%d"
                        ",szd%d,2)]"
                        " = (u16)v%d;\n",
                        pad, ~p, inst->x,
                        ~p, inst->y);
                } else {
                    _Bool mx =
                        ptr_32[p] && ptr_16[p];
                    emit(b,
                        mx
                        ? "%sp%d_16"
                          "[clamp_ix((s32)v%d"
                          ",sz%d,2)]"
                          " = (u16)v%d;\n"
                        : "%sp%d"
                          "[clamp_ix((s32)v%d"
                          ",sz%d,2)]"
                          " = (u16)v%d;\n",
                        pad, p, inst->x,
                        p, inst->y);
                }
            } break;

            case op_widen_f16:
                emit(b,
                    "%su32 v%d ="
                    " f2u(h2f((u16)v%d));\n",
                    pad, i, inst->x);
                break;
            case op_narrow_f32:
                emit(b,
                    "%su32 v%d ="
                    " (u32)f2h(u2f(v%d));\n",
                    pad, i, inst->x);
                break;

            case op_widen_s16:
                emit(b,
                    "%su32 v%d ="
                    " (u32)(s32)(s16)(u16)v%d;\n",
                    pad, i, inst->x);
                break;
            case op_widen_u16:
                emit(b,
                    "%su32 v%d ="
                    " (u32)(u16)v%d;\n",
                    pad, i, inst->x);
                break;
            case op_narrow_16:
                emit(b,
                    "%su32 v%d ="
                    " (u32)(u16)v%d;\n",
                    pad, i, inst->x);
                break;

            #define BINOP(OP, EXPR) \
                case OP: \
                    emit(b, "%su32 v%d = " \
                         EXPR ";\n", \
                         pad, i, \
                         inst->x, inst->y); \
                    break;
            #define UNOP(OP, EXPR) \
                case OP: \
                    emit(b, "%su32 v%d = " \
                         EXPR ";\n", \
                         pad, i, inst->x); \
                    break;

            BINOP(op_add_f32,
                  "f2u(u2f(v%d) + u2f(v%d))")
            BINOP(op_sub_f32,
                  "f2u(u2f(v%d) - u2f(v%d))")
            BINOP(op_mul_f32,
                  "f2u(u2f(v%d) * u2f(v%d))")
            BINOP(op_div_f32,
                  "f2u(u2f(v%d) / u2f(v%d))")
            BINOP(op_min_f32,
                  "f2u(fminf(u2f(v%d), u2f(v%d)))")
            BINOP(op_max_f32,
                  "f2u(fmaxf(u2f(v%d), u2f(v%d)))")
            UNOP(op_sqrt_f32,
                 "f2u(sqrtf(u2f(v%d)))")

            BINOP(op_add_i32, "(u32)(v%d + v%d)")
            BINOP(op_sub_i32, "(u32)(v%d - v%d)")
            BINOP(op_mul_i32, "(u32)(v%d * v%d)")
            BINOP(op_shl_i32, "(u32)(v%d << v%d)")
            BINOP(op_shr_u32, "(u32)(v%d >> v%d)")
            BINOP(op_shr_s32,
                  "(u32)((s32)v%d >> (s32)v%d)")

            BINOP(op_and_32, "(u32)(v%d & v%d)")
            BINOP(op_or_32,  "(u32)(v%d | v%d)")
            BINOP(op_xor_32, "(u32)(v%d ^ v%d)")

            #undef BINOP
            #undef UNOP

            case op_sel_32:
                emit(b,
                    "%su32 v%d ="
                    " (v%d & v%d)"
                    " | (~v%d & v%d);\n",
                    pad, i,
                    inst->x, inst->y,
                    inst->x, inst->z);
                break;

            case op_f32_from_i32:
                emit(b,
                    "%su32 v%d ="
                    " f2u((float)(s32)v%d);\n",
                    pad, i, inst->x);
                break;
            case op_i32_from_f32:
                emit(b,
                    "%su32 v%d ="
                    " (u32)(s32)u2f(v%d);\n",
                    pad, i, inst->x);
                break;

            case op_fma_f32:
                emit(b,
                    "%su32 v%d = f2u("
                    "fmaf(u2f(v%d), u2f(v%d),"
                    " u2f(v%d)));\n",
                    pad, i,
                    inst->x, inst->y, inst->z);
                break;
            case op_fms_f32:
                emit(b,
                    "%su32 v%d = f2u("
                    "fmaf(-u2f(v%d), u2f(v%d),"
                    " u2f(v%d)));\n",
                    pad, i,
                    inst->x, inst->y, inst->z);
                break;

            case op_eq_f32:
                emit(b,
                    "%su32 v%d ="
                    " (u32)-(s32)"
                    "(u2f(v%d) == u2f(v%d));\n",
                    pad, i, inst->x, inst->y);
                break;
            case op_lt_f32:
                emit(b,
                    "%su32 v%d ="
                    " (u32)-(s32)"
                    "(u2f(v%d) <  u2f(v%d));\n",
                    pad, i, inst->x, inst->y);
                break;
            case op_le_f32:
                emit(b,
                    "%su32 v%d ="
                    " (u32)-(s32)"
                    "(u2f(v%d) <= u2f(v%d));\n",
                    pad, i, inst->x, inst->y);
                break;
            case op_eq_i32:
                emit(b,
                    "%su32 v%d ="
                    " (u32)-(s32)"
                    "((s32)v%d == (s32)v%d);\n",
                    pad, i, inst->x, inst->y);
                break;
            case op_lt_s32:
                emit(b,
                    "%su32 v%d ="
                    " (u32)-(s32)"
                    "((s32)v%d <  (s32)v%d);\n",
                    pad, i, inst->x, inst->y);
                break;
            case op_le_s32:
                emit(b,
                    "%su32 v%d ="
                    " (u32)-(s32)"
                    "((s32)v%d <= (s32)v%d);\n",
                    pad, i, inst->x, inst->y);
                break;
            case op_lt_u32:
                emit(b,
                    "%su32 v%d ="
                    " (u32)-(s32)"
                    "(v%d <  v%d);\n",
                    pad, i, inst->x, inst->y);
                break;
            case op_le_u32:
                emit(b,
                    "%su32 v%d ="
                    " (u32)-(s32)"
                    "(v%d <= v%d);\n",
                    pad, i, inst->x, inst->y);
                break;

        }

        if (is_store(inst->op) && i+1 < hi) {
            emit(b, "\n");
        }
    }
}

struct umbra_codegen* umbra_codegen(
    BB const *bb)
{
    int max_ptr = -1;
    for (int i = 0; i < bb->insts; i++) {
        if (has_ptr(bb->inst[i].op)
            && bb->inst[i].ptr >= 0) {
            if (bb->inst[i].ptr > max_ptr) {
                max_ptr = bb->inst[i].ptr;
            }
        }
    }

    _Bool *ptr_16 = calloc((size_t)(max_ptr+2), 1);
    _Bool *ptr_32 = calloc((size_t)(max_ptr+2), 1);
    for (int i = 0; i < bb->insts; i++) {
        enum op op = bb->inst[i].op;
        if (has_ptr(op) && bb->inst[i].ptr >= 0) {
            int p = bb->inst[i].ptr;
            if (op == op_deref_ptr) {
            } else if (is_32bit_mem_op(op)) {
                ptr_32[p] = 1;
            } else if (is_16bit_mem_op(op)) {
                ptr_16[p] = 1;
            }
        }
    }

    Buf b = {0};

    emit(&b, "#include <stdint.h>\n");
    emit(&b, "#include <math.h>\n\n");

    emit(&b,
        "typedef uint32_t u32;\n"
        "typedef uint16_t u16;\n"
        "typedef  int32_t s32;\n"
        "typedef  int16_t s16;\n\n");

    emit(&b,
        "static inline u32 f2u(float f)"
        " { union{float f;u32 u;} x;"
        " x.f=f; return x.u; }\n");
    emit(&b,
        "static inline float u2f(u32 u)"
        " { union{float f;u32 u;} x;"
        " x.u=u; return x.f; }\n");
    emit(&b,
        "static inline float h2f(u16 h) {\n");
    emit(&b,
        "    u32 sign=((u32)h>>15)<<31,"
        " exp=((u32)h>>10)&0x1f,"
        " mant=(u32)h&0x3ff;\n");
    emit(&b,
        "    u32 normal = sign"
        " | ((exp+112)<<23)"
        " | (mant<<13);\n");
    emit(&b, "    u32 zero   = sign;\n");
    emit(&b,
        "    u32 infnan = sign"
        " | (0xffu<<23)"
        " | (mant<<13);\n");
    emit(&b,
        "    u32 is_zero   ="
        " -(u32)(exp==0);\n");
    emit(&b,
        "    u32 is_infnan ="
        " -(u32)(exp==31);\n");
    emit(&b,
        "    u32 bits ="
        " (is_zero&zero)"
        " | (is_infnan&infnan)"
        " | (~is_zero&~is_infnan&normal);\n");
    emit(&b, "    return u2f(bits);\n}\n\n");

    emit(&b,
        "static inline u16 f2h(float f) {\n");
    emit(&b,
        "    u32 bits=f2u(f),"
        " sign=(bits>>31)<<15;\n");
    emit(&b,
        "    s32 exp=(s32)((bits>>23)&0xff)"
        "-127+15;\n");
    emit(&b,
        "    u32 mant=(bits>>13)&0x3ff,"
        " rb=(bits>>12)&1,"
        " st=-(u32)((bits&0xfff)!=0);\n");
    emit(&b, "    mant+=rb&(st|(mant&1));\n");
    emit(&b,
        "    u32 mo=mant>>10;"
        " exp+=(s32)mo; mant&=0x3ff;\n");
    emit(&b,
        "    u32 normal ="
        " sign|((u32)exp<<10)|mant;\n");
    emit(&b, "    u32 inf    = sign|0x7c00;\n");
    emit(&b,
        "    u32 infnan ="
        " sign|0x7c00|mant;\n");
    emit(&b,
        "    u32 is_of ="
        " -(u32)(exp>=31);\n");
    emit(&b,
        "    u32 is_uf ="
        " -(u32)(exp<=0);\n");
    emit(&b,
        "    u32 se=(bits>>23)&0xff;\n");
    emit(&b,
        "    u32 is_in ="
        " -(u32)(se==0xff);\n");
    emit(&b,
        "    u32 r = (is_uf&sign)"
        " | (is_of&~is_in&inf)"
        " | (is_in&infnan)"
        " | (~is_uf&~is_of&~is_in&normal);\n");
    emit(&b, "    return (u16)r;\n}\n\n");

    emit(&b,
        "static inline s32 clamp_ix("
        "s32 ix, long bytes, int elem) {\n");
    emit(&b,
        "    s32 hi ="
        " (s32)(bytes / elem) - 1;\n");
    emit(&b,
        "    if (hi < 0) hi = 0;\n");
    emit(&b,
        "    if (ix < 0) ix = 0;\n");
    emit(&b,
        "    if (ix > hi) ix = hi;\n");
    emit(&b, "    return ix;\n}\n\n");

    emit(&b,
        "void umbra_entry("
        "int n, void **ptrs, long *szs) {\n");

    for (int p = 0; p <= max_ptr; p++) {
        if (ptr_32[p] && ptr_16[p]) {
            emit(&b,
                "    u32* p%d_32 ="
                " (u32*)ptrs[%d];\n", p, p);
            emit(&b,
                "    u16* p%d_16 ="
                " (u16*)ptrs[%d];\n", p, p);
        } else if (ptr_32[p]) {
            emit(&b,
                "    u32* restrict p%d ="
                " (u32*)ptrs[%d];\n", p, p);
        } else if (ptr_16[p]) {
            emit(&b,
                "    u16* restrict p%d ="
                " (u16*)ptrs[%d];\n", p, p);
        }
    }
    for (int p = 0; p <= max_ptr; p++) {
        emit(&b,
            "    long sz%d = szs[%d];\n", p, p);
    }

    emit_ops(&b, bb, ptr_16, ptr_32,
             0, bb->preamble, "    ", 0);

    if (bb->preamble) { emit(&b, "\n"); }
    emit(&b,
        "    for (int i = 0; i < n; i++) {\n");
    emit_ops(&b, bb, ptr_16, ptr_32,
             bb->preamble, bb->insts,
             "        ", 1);
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
    if (!fp) {
        free(b.buf); remove(c_path); return 0;
    }
    fwrite(b.buf, 1, (size_t)b.len, fp);
    fclose(fp);

    char *src_copy = malloc((size_t)b.len + 1);
    __builtin_memcpy(src_copy, b.buf,
                     (size_t)b.len);
    src_copy[b.len] = '\0';
    free(b.buf);

    size_t slen = __builtin_strlen(c_path);
    char *so_path = malloc(slen + 8);
    __builtin_memcpy(so_path, c_path, slen - 2);
    #ifdef __APPLE__
    __builtin_memcpy(
        so_path + slen - 2, ".dylib", 7);
    #else
    __builtin_memcpy(
        so_path + slen - 2, ".so", 4);
    #endif

    char cmd[512];
    #ifdef __APPLE__
    snprintf(cmd, sizeof cmd,
        "cc -O2 -g -std=c99 -shared"
        " -Wl,-install_name,umbra_codegen"
        " -o %s %s 2>&1",
        so_path, c_path);
    #else
    snprintf(cmd, sizeof cmd,
        "cc -O2 -g -std=c99 -shared"
        " -o %s %s 2>&1",
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

    void (*entry)(int, void**, long*) =
        (void (*)(int, void**, long*))
        dlsym(dl, "umbra_entry");
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
    __builtin_memcpy(
        cg->src_path, c_path, slen + 1);
    cg->so_path  = so_path;
    cg->src      = src_copy;
    cg->nptr     = max_ptr + 1;
    return cg;
}

void umbra_codegen_run(
    struct umbra_codegen *cg,
    int n, umbra_buf buf[])
{
    if (!cg) { return; }
    void *ptrs[16] = {0};
    long  szs[16]  = {0};
    for (int i = 0; i < cg->nptr && i < 16; i++) {
        ptrs[i] = buf[i].ptr;
        szs[i]  = buf[i].sz < 0
                 ? -buf[i].sz : buf[i].sz;
    }
    cg->entry(n, ptrs, szs);
}

void umbra_codegen_free(struct umbra_codegen *cg) {
    if (!cg) { return; }
    if (cg->dl) { dlclose(cg->dl); }
    if (cg->src_path) {
        remove(cg->src_path);
        free(cg->src_path);
    }
    if (cg->so_path) {
        remove(cg->so_path);
        free(cg->so_path);
    }
    free(cg->src);
    free(cg);
}

void umbra_dump_codegen(
    struct umbra_codegen const *cg, FILE *f)
{
    if (cg && cg->src) { fputs(cg->src, f); }
}

#endif
