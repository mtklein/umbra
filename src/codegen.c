#include "../umbra.h"
#include "bb.h"

#ifdef __wasm__

struct umbra_codegen { int dummy; };
struct umbra_codegen* umbra_codegen(struct umbra_basic_block const *bb) { (void)bb; return 0; }
void umbra_codegen_run (struct umbra_codegen *cg, int n, void *ptr[]) { (void)cg; (void)n; (void)ptr; }
void umbra_codegen_free(struct umbra_codegen *cg) { (void)cg; }

#else

#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct umbra_codegen {
    void *dl;
    void (*entry)(int n, void* ptr[]);
    char *src_path, *so_path;
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

struct umbra_codegen* umbra_codegen(struct umbra_basic_block const *bb) {
    // Liveness
    _Bool *live    = calloc((size_t)bb->insts, 1);
    _Bool *varying = calloc((size_t)bb->insts, 1);

    for (int i = bb->insts; i --> 0;) {
        if (is_store(bb->inst[i].op)) { live[i] = 1; }
        if (live[i]) {
            live[bb->inst[i].x] = 1;
            live[bb->inst[i].y] = 1;
            live[bb->inst[i].z] = 1;
        }
    }
    for (int i = 0; i < bb->insts; i++) {
        varying[i] = (bb->inst[i].op == op_lane)
                    | varying[bb->inst[i].x]
                    | varying[bb->inst[i].y]
                    | varying[bb->inst[i].z];
    }

    // Find max ptr index used
    int max_ptr = -1;
    for (int i = 0; i < bb->insts; i++) {
        if (!live[i]) { continue; }
        enum op op = bb->inst[i].op;
        if (op == op_load_16 || op == op_load_32 ||
            op == op_store_16 || op == op_store_32) {
            if (bb->inst[i].ptr > max_ptr) { max_ptr = bb->inst[i].ptr; }
        }
    }

    Buf b = {0};

    // Preamble
    emit(&b, "#include <stdint.h>\n");
    emit(&b, "#include <string.h>\n");
    emit(&b, "#include <math.h>\n\n");

    emit(&b, "typedef uint32_t u32;\ntypedef uint16_t u16;\ntypedef  int32_t s32;\ntypedef  int16_t s16;\n\n");

    emit(&b, "static inline u32 f2u(float  f) { u32 u; memcpy(&u,&f,4); return u; }\n");
    emit(&b, "static inline float u2f(u32  u) { float f; memcpy(&f,&u,4); return f; }\n\n");

    // Scalar f16 helpers
    emit(&b, "static inline float h2f(u16 h) {\n");
    emit(&b, "    u32 sign=((u32)h>>15)<<31, exp=((u32)h>>10)&0x1f, mant=(u32)h&0x3ff;\n");
    emit(&b, "    u32 bits;\n");
    emit(&b, "    if(exp==0) bits=sign;\n");
    emit(&b, "    else if(exp==31) bits=sign|(0xffu<<23)|(mant<<13);\n");
    emit(&b, "    else bits=sign|((exp+112)<<23)|(mant<<13);\n");
    emit(&b, "    return u2f(bits);\n}\n\n");

    emit(&b, "static inline u16 f2h(float f) {\n");
    emit(&b, "    u32 bits=f2u(f), sign=(bits>>31)<<15;\n");
    emit(&b, "    int exp=(int)((bits>>23)&0xff)-127+15;\n");
    emit(&b, "    u32 mant=(bits>>13)&0x3ff, rb=(bits>>12)&1, st=(bits&0xfff)!=0?1:0;\n");
    emit(&b, "    mant+=rb&(st|(mant&1)); if(mant>>10){exp++;mant&=0x3ff;}\n");
    emit(&b, "    u32 se=(bits>>23)&0xff;\n");
    emit(&b, "    if(se==0xff) return (u16)(sign|0x7c00|mant);\n");
    emit(&b, "    if(exp>=31)  return (u16)(sign|0x7c00);\n");
    emit(&b, "    if(exp<=0)   return (u16)sign;\n");
    emit(&b, "    return (u16)(sign|((u32)exp<<10)|mant);\n}\n\n");

    // Function signature
    emit(&b, "void umbra_entry(int n, void* ptr[]) {\n");

    // restrict pointer aliases
    for (int p = 0; p <= max_ptr; p++) {
        // Check if this ptr is used for 16-bit or 32-bit (check stores and loads)
        _Bool used16 = 0, used32 = 0;
        for (int i = 0; i < bb->insts; i++) {
            if (!live[i]) { continue; }
            if (bb->inst[i].ptr != p) { continue; }
            enum op op = bb->inst[i].op;
            if (op == op_load_16 || op == op_store_16) { used16 = 1; }
            if (op == op_load_32 || op == op_store_32) { used32 = 1; }
        }
        if (used32) {
            emit(&b, "    char* restrict p%d = ptr[%d];\n", p, p);
        } else if (used16) {
            emit(&b, "    char* restrict p%d = ptr[%d];\n", p, p);
        }
    }

    // Emit uniform (non-varying) values before the loop
    for (int i = 0; i < bb->insts; i++) {
        if (!live[i] || varying[i] || is_store(bb->inst[i].op)) { continue; }
        struct bb_inst const *inst = &bb->inst[i];
        _Bool is16 = is_16bit_result(inst->op);
        char const *ty = is16 ? "u16" : "u32";

        switch (inst->op) {
            case op_imm_16: emit(&b, "    %s v%d = %u;\n", ty, i, (uint16_t)inst->imm); break;
            case op_imm_32: emit(&b, "    %s v%d = %uu;\n", ty, i, (uint32_t)inst->imm); break;

            case op_load_16:
                if (bb->inst[inst->x].op == op_imm_32) {
                    emit(&b, "    u16 v%d; memcpy(&v%d, p%d+2*%d, 2);\n",
                         i, i, inst->ptr, bb->inst[inst->x].imm);
                }
                break;
            case op_load_32:
                if (bb->inst[inst->x].op == op_imm_32) {
                    emit(&b, "    u32 v%d; memcpy(&v%d, p%d+4*%d, 4);\n",
                         i, i, inst->ptr, bb->inst[inst->x].imm);
                }
                break;

            #define BINOP(OP, EXPR) case OP: emit(&b, "    %s v%d = " EXPR ";\n", ty, i, inst->x, inst->y); break;
            #define UNOP(OP, EXPR)  case OP: emit(&b, "    %s v%d = " EXPR ";\n", ty, i, inst->x); break;

            BINOP(op_add_f32, "f2u(u2f(v%d) + u2f(v%d))")
            BINOP(op_sub_f32, "f2u(u2f(v%d) - u2f(v%d))")
            BINOP(op_mul_f32, "f2u(u2f(v%d) * u2f(v%d))")
            BINOP(op_div_f32, "f2u(u2f(v%d) / u2f(v%d))")
            BINOP(op_min_f32, "f2u(fminf(u2f(v%d), u2f(v%d)))")
            BINOP(op_max_f32, "f2u(fmaxf(u2f(v%d), u2f(v%d)))")
            UNOP (op_sqrt_f32,"f2u(sqrtf(u2f(v%d)))")

            BINOP(op_add_f16, "f2h(h2f(v%d) + h2f(v%d))")
            BINOP(op_sub_f16, "f2h(h2f(v%d) - h2f(v%d))")
            BINOP(op_mul_f16, "f2h(h2f(v%d) * h2f(v%d))")
            BINOP(op_div_f16, "f2h(h2f(v%d) / h2f(v%d))")
            BINOP(op_min_f16, "f2h(fminf(h2f(v%d), h2f(v%d)))")
            BINOP(op_max_f16, "f2h(fmaxf(h2f(v%d), h2f(v%d)))")
            UNOP (op_sqrt_f16,"f2h(sqrtf(h2f(v%d)))")

            BINOP(op_add_i32, "(u32)(v%d + v%d)")
            BINOP(op_sub_i32, "(u32)(v%d - v%d)")
            BINOP(op_mul_i32, "(u32)(v%d * v%d)")
            BINOP(op_shl_i32, "(u32)(v%d << v%d)")
            BINOP(op_shr_u32, "(u32)(v%d >> v%d)")
            BINOP(op_shr_s32, "(u32)((s32)v%d >> (s32)v%d)")
            BINOP(op_and_32,  "(u32)(v%d & v%d)")
            BINOP(op_or_32,   "(u32)(v%d | v%d)")
            BINOP(op_xor_32,  "(u32)(v%d ^ v%d)")

            BINOP(op_add_i16, "(u16)(v%d + v%d)")
            BINOP(op_sub_i16, "(u16)(v%d - v%d)")
            BINOP(op_mul_i16, "(u16)(v%d * v%d)")
            BINOP(op_shl_i16, "(u16)(v%d << v%d)")
            BINOP(op_shr_u16, "(u16)(v%d >> v%d)")
            BINOP(op_shr_s16, "(u16)((s16)v%d >> (s16)v%d)")
            BINOP(op_and_16,  "(u16)(v%d & v%d)")
            BINOP(op_or_16,   "(u16)(v%d | v%d)")
            BINOP(op_xor_16,  "(u16)(v%d ^ v%d)")

            UNOP(op_f16_from_f32, "f2h(u2f(v%d))")
            UNOP(op_f32_from_f16, "f2u(h2f(v%d))")
            UNOP(op_f32_from_i32, "f2u((float)(s32)v%d)")
            UNOP(op_i32_from_f32, "(u32)(s32)u2f(v%d)")

            BINOP(op_eq_f32, "(u32)-(s32)(u2f(v%d) == u2f(v%d))")
            BINOP(op_ne_f32, "(u32)-(s32)(u2f(v%d) != u2f(v%d))")
            BINOP(op_lt_f32, "(u32)-(s32)(u2f(v%d) <  u2f(v%d))")
            BINOP(op_le_f32, "(u32)-(s32)(u2f(v%d) <= u2f(v%d))")
            BINOP(op_gt_f32, "(u32)-(s32)(u2f(v%d) >  u2f(v%d))")
            BINOP(op_ge_f32, "(u32)-(s32)(u2f(v%d) >= u2f(v%d))")

            BINOP(op_eq_f16, "(u16)-(s16)(h2f(v%d) == h2f(v%d))")
            BINOP(op_ne_f16, "(u16)-(s16)(h2f(v%d) != h2f(v%d))")
            BINOP(op_lt_f16, "(u16)-(s16)(h2f(v%d) <  h2f(v%d))")
            BINOP(op_le_f16, "(u16)-(s16)(h2f(v%d) <= h2f(v%d))")
            BINOP(op_gt_f16, "(u16)-(s16)(h2f(v%d) >  h2f(v%d))")
            BINOP(op_ge_f16, "(u16)-(s16)(h2f(v%d) >= h2f(v%d))")

            BINOP(op_eq_i32, "(u32)-(s32)((s32)v%d == (s32)v%d)")
            BINOP(op_ne_i32, "(u32)-(s32)((s32)v%d != (s32)v%d)")
            BINOP(op_lt_s32, "(u32)-(s32)((s32)v%d <  (s32)v%d)")
            BINOP(op_le_s32, "(u32)-(s32)((s32)v%d <= (s32)v%d)")
            BINOP(op_gt_s32, "(u32)-(s32)((s32)v%d >  (s32)v%d)")
            BINOP(op_ge_s32, "(u32)-(s32)((s32)v%d >= (s32)v%d)")
            BINOP(op_lt_u32, "(u32)-(s32)(v%d <  v%d)")
            BINOP(op_le_u32, "(u32)-(s32)(v%d <= v%d)")
            BINOP(op_gt_u32, "(u32)-(s32)(v%d >  v%d)")
            BINOP(op_ge_u32, "(u32)-(s32)(v%d >= v%d)")

            BINOP(op_eq_i16, "(u16)-(s16)((s16)v%d == (s16)v%d)")
            BINOP(op_ne_i16, "(u16)-(s16)((s16)v%d != (s16)v%d)")
            BINOP(op_lt_s16, "(u16)-(s16)((s16)v%d <  (s16)v%d)")
            BINOP(op_le_s16, "(u16)-(s16)((s16)v%d <= (s16)v%d)")
            BINOP(op_gt_s16, "(u16)-(s16)((s16)v%d >  (s16)v%d)")
            BINOP(op_ge_s16, "(u16)-(s16)((s16)v%d >= (s16)v%d)")
            BINOP(op_lt_u16, "(u16)-(s16)(v%d <  v%d)")
            BINOP(op_le_u16, "(u16)-(s16)(v%d <= v%d)")
            BINOP(op_gt_u16, "(u16)-(s16)(v%d >  v%d)")
            BINOP(op_ge_u16, "(u16)-(s16)(v%d >= v%d)")

            #undef BINOP
            #undef UNOP

            case op_sel_32:
                emit(&b, "    %s v%d = (v%d & v%d) | (~v%d & v%d);\n",
                     ty, i, inst->x, inst->y, inst->x, inst->z);
                break;
            case op_sel_16:
                emit(&b, "    %s v%d = (v%d & v%d) | (~v%d & v%d);\n",
                     ty, i, inst->x, inst->y, inst->x, inst->z);
                break;

            case op_fma_f32:
                emit(&b, "    %s v%d = f2u(u2f(v%d) * u2f(v%d) + u2f(v%d));\n",
                     ty, i, inst->x, inst->y, inst->z);
                break;
            case op_fma_f16:
                emit(&b, "    %s v%d = f2h(h2f(v%d) * h2f(v%d) + h2f(v%d));\n",
                     ty, i, inst->x, inst->y, inst->z);
                break;

            case op_lane: case op_store_16: case op_store_32: break;
        }
    }

    // Loop over elements
    emit(&b, "    for (int i = 0; i < n; i++) {\n");

    for (int i = 0; i < bb->insts; i++) {
        if (!live[i] || !varying[i]) { continue; }
        struct bb_inst const *inst = &bb->inst[i];
        _Bool is16 = is_16bit_result(inst->op);
        char const *ty = is16 ? "u16" : "u32";

        switch (inst->op) {
            case op_lane: emit(&b, "        u32 v%d = (u32)i;\n", i); break;

            case op_load_16:
                if (bb->inst[inst->x].op == op_lane) {
                    emit(&b, "        u16 v%d; memcpy(&v%d, p%d+2*i, 2);\n", i, i, inst->ptr);
                } else {
                    emit(&b, "        u16 v%d; memcpy(&v%d, p%d+2*v%d, 2);\n",
                         i, i, inst->ptr, inst->x);
                }
                break;
            case op_load_32:
                if (bb->inst[inst->x].op == op_lane) {
                    emit(&b, "        u32 v%d; memcpy(&v%d, p%d+4*i, 4);\n", i, i, inst->ptr);
                } else {
                    emit(&b, "        u32 v%d; memcpy(&v%d, p%d+4*v%d, 4);\n",
                         i, i, inst->ptr, inst->x);
                }
                break;

            case op_store_16:
                if (bb->inst[inst->x].op == op_lane) {
                    emit(&b, "        memcpy(p%d+2*i, &v%d, 2);\n", inst->ptr, inst->y);
                } else {
                    emit(&b, "        memcpy(p%d+2*v%d, &v%d, 2);\n",
                         inst->ptr, inst->x, inst->y);
                }
                break;
            case op_store_32:
                if (bb->inst[inst->x].op == op_lane) {
                    emit(&b, "        memcpy(p%d+4*i, &v%d, 4);\n", inst->ptr, inst->y);
                } else {
                    emit(&b, "        memcpy(p%d+4*v%d, &v%d, 4);\n",
                         inst->ptr, inst->x, inst->y);
                }
                break;

            #define BINOP(OP, EXPR) case OP: emit(&b, "        %s v%d = " EXPR ";\n", ty, i, inst->x, inst->y); break;
            #define UNOP(OP, EXPR)  case OP: emit(&b, "        %s v%d = " EXPR ";\n", ty, i, inst->x); break;

            BINOP(op_add_f32, "f2u(u2f(v%d) + u2f(v%d))")
            BINOP(op_sub_f32, "f2u(u2f(v%d) - u2f(v%d))")
            BINOP(op_mul_f32, "f2u(u2f(v%d) * u2f(v%d))")
            BINOP(op_div_f32, "f2u(u2f(v%d) / u2f(v%d))")
            BINOP(op_min_f32, "f2u(fminf(u2f(v%d), u2f(v%d)))")
            BINOP(op_max_f32, "f2u(fmaxf(u2f(v%d), u2f(v%d)))")
            UNOP (op_sqrt_f32,"f2u(sqrtf(u2f(v%d)))")

            BINOP(op_add_f16, "f2h(h2f(v%d) + h2f(v%d))")
            BINOP(op_sub_f16, "f2h(h2f(v%d) - h2f(v%d))")
            BINOP(op_mul_f16, "f2h(h2f(v%d) * h2f(v%d))")
            BINOP(op_div_f16, "f2h(h2f(v%d) / h2f(v%d))")
            BINOP(op_min_f16, "f2h(fminf(h2f(v%d), h2f(v%d)))")
            BINOP(op_max_f16, "f2h(fmaxf(h2f(v%d), h2f(v%d)))")
            UNOP (op_sqrt_f16,"f2h(sqrtf(h2f(v%d)))")

            BINOP(op_add_i32, "(u32)(v%d + v%d)")
            BINOP(op_sub_i32, "(u32)(v%d - v%d)")
            BINOP(op_mul_i32, "(u32)(v%d * v%d)")
            BINOP(op_shl_i32, "(u32)(v%d << v%d)")
            BINOP(op_shr_u32, "(u32)(v%d >> v%d)")
            BINOP(op_shr_s32, "(u32)((s32)v%d >> (s32)v%d)")
            BINOP(op_and_32,  "(u32)(v%d & v%d)")
            BINOP(op_or_32,   "(u32)(v%d | v%d)")
            BINOP(op_xor_32,  "(u32)(v%d ^ v%d)")

            BINOP(op_add_i16, "(u16)(v%d + v%d)")
            BINOP(op_sub_i16, "(u16)(v%d - v%d)")
            BINOP(op_mul_i16, "(u16)(v%d * v%d)")
            BINOP(op_shl_i16, "(u16)(v%d << v%d)")
            BINOP(op_shr_u16, "(u16)(v%d >> v%d)")
            BINOP(op_shr_s16, "(u16)((s16)v%d >> (s16)v%d)")
            BINOP(op_and_16,  "(u16)(v%d & v%d)")
            BINOP(op_or_16,   "(u16)(v%d | v%d)")
            BINOP(op_xor_16,  "(u16)(v%d ^ v%d)")

            UNOP(op_f16_from_f32, "f2h(u2f(v%d))")
            UNOP(op_f32_from_f16, "f2u(h2f(v%d))")
            UNOP(op_f32_from_i32, "f2u((float)(s32)v%d)")
            UNOP(op_i32_from_f32, "(u32)(s32)u2f(v%d)")

            BINOP(op_eq_f32, "(u32)-(s32)(u2f(v%d) == u2f(v%d))")
            BINOP(op_ne_f32, "(u32)-(s32)(u2f(v%d) != u2f(v%d))")
            BINOP(op_lt_f32, "(u32)-(s32)(u2f(v%d) <  u2f(v%d))")
            BINOP(op_le_f32, "(u32)-(s32)(u2f(v%d) <= u2f(v%d))")
            BINOP(op_gt_f32, "(u32)-(s32)(u2f(v%d) >  u2f(v%d))")
            BINOP(op_ge_f32, "(u32)-(s32)(u2f(v%d) >= u2f(v%d))")

            BINOP(op_eq_f16, "(u16)-(s16)(h2f(v%d) == h2f(v%d))")
            BINOP(op_ne_f16, "(u16)-(s16)(h2f(v%d) != h2f(v%d))")
            BINOP(op_lt_f16, "(u16)-(s16)(h2f(v%d) <  h2f(v%d))")
            BINOP(op_le_f16, "(u16)-(s16)(h2f(v%d) <= h2f(v%d))")
            BINOP(op_gt_f16, "(u16)-(s16)(h2f(v%d) >  h2f(v%d))")
            BINOP(op_ge_f16, "(u16)-(s16)(h2f(v%d) >= h2f(v%d))")

            BINOP(op_eq_i32, "(u32)-(s32)((s32)v%d == (s32)v%d)")
            BINOP(op_ne_i32, "(u32)-(s32)((s32)v%d != (s32)v%d)")
            BINOP(op_lt_s32, "(u32)-(s32)((s32)v%d <  (s32)v%d)")
            BINOP(op_le_s32, "(u32)-(s32)((s32)v%d <= (s32)v%d)")
            BINOP(op_gt_s32, "(u32)-(s32)((s32)v%d >  (s32)v%d)")
            BINOP(op_ge_s32, "(u32)-(s32)((s32)v%d >= (s32)v%d)")
            BINOP(op_lt_u32, "(u32)-(s32)(v%d <  v%d)")
            BINOP(op_le_u32, "(u32)-(s32)(v%d <= v%d)")
            BINOP(op_gt_u32, "(u32)-(s32)(v%d >  v%d)")
            BINOP(op_ge_u32, "(u32)-(s32)(v%d >= v%d)")

            BINOP(op_eq_i16, "(u16)-(s16)((s16)v%d == (s16)v%d)")
            BINOP(op_ne_i16, "(u16)-(s16)((s16)v%d != (s16)v%d)")
            BINOP(op_lt_s16, "(u16)-(s16)((s16)v%d <  (s16)v%d)")
            BINOP(op_le_s16, "(u16)-(s16)((s16)v%d <= (s16)v%d)")
            BINOP(op_gt_s16, "(u16)-(s16)((s16)v%d >  (s16)v%d)")
            BINOP(op_ge_s16, "(u16)-(s16)((s16)v%d >= (s16)v%d)")
            BINOP(op_lt_u16, "(u16)-(s16)(v%d <  v%d)")
            BINOP(op_le_u16, "(u16)-(s16)(v%d <= v%d)")
            BINOP(op_gt_u16, "(u16)-(s16)(v%d >  v%d)")
            BINOP(op_ge_u16, "(u16)-(s16)(v%d >= v%d)")

            #undef BINOP
            #undef UNOP

            case op_sel_32:
                emit(&b, "        %s v%d = (v%d & v%d) | (~v%d & v%d);\n",
                     ty, i, inst->x, inst->y, inst->x, inst->z);
                break;
            case op_sel_16:
                emit(&b, "        %s v%d = (v%d & v%d) | (~v%d & v%d);\n",
                     ty, i, inst->x, inst->y, inst->x, inst->z);
                break;

            case op_fma_f32:
                emit(&b, "        %s v%d = f2u(u2f(v%d) * u2f(v%d) + u2f(v%d));\n",
                     ty, i, inst->x, inst->y, inst->z);
                break;
            case op_fma_f16:
                emit(&b, "        %s v%d = f2h(h2f(v%d) * h2f(v%d) + h2f(v%d));\n",
                     ty, i, inst->x, inst->y, inst->z);
                break;

            case op_imm_16:
                emit(&b, "        u16 v%d = %u;\n", i, (uint16_t)inst->imm);
                break;
            case op_imm_32:
                emit(&b, "        u32 v%d = %uu;\n", i, (uint32_t)inst->imm);
                break;
        }
    }
    emit(&b, "    }\n}\n");

    free(live);
    free(varying);

    // Write temp .c file via mkstemp
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
    free(b.buf);

    // Derive .so path
    size_t slen = strlen(c_path);
    char *so_path = malloc(slen + 8);
    memcpy(so_path, c_path, slen - 2);
    #ifdef __APPLE__
    memcpy(so_path + slen - 2, ".dylib", 7);
    #else
    memcpy(so_path + slen - 2, ".so", 4);
    #endif

    // Compile
    char cmd[512];
    snprintf(cmd, sizeof cmd, "cc -O2 -std=c99 -shared -o %s %s 2>&1", so_path, c_path);
    int rc = system(cmd);
    if (rc != 0) {
        remove(c_path);
        free(so_path);
        return 0;
    }

    // dlopen
    void *dl = dlopen(so_path, RTLD_NOW);
    if (!dl) {
        remove(c_path);
        remove(so_path);
        free(so_path);
        return 0;
    }

    void (*entry)(int, void*[]) = (void (*)(int, void*[]))dlsym(dl, "umbra_entry");
    if (!entry) {
        dlclose(dl);
        remove(c_path);
        remove(so_path);
        free(so_path);
        return 0;
    }

    struct umbra_codegen *cg = malloc(sizeof *cg);
    cg->dl       = dl;
    cg->entry    = entry;
    cg->src_path = malloc(slen + 1);
    memcpy(cg->src_path, c_path, slen + 1);
    cg->so_path  = so_path;
    return cg;
}

void umbra_codegen_run(struct umbra_codegen *cg, int n, void *ptr[]) {
    if (cg) { cg->entry(n, ptr); }
}

void umbra_codegen_free(struct umbra_codegen *cg) {
    if (!cg) { return; }
    if (cg->dl) { dlclose(cg->dl); }
    if (cg->src_path) { remove(cg->src_path); free(cg->src_path); }
    if (cg->so_path)  { remove(cg->so_path);  free(cg->so_path);  }
    free(cg);
}

#endif
