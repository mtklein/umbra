#pragma once

struct umbra_basic_block* umbra_basic_block(void);
void umbra_basic_block_free(struct umbra_basic_block*);
void umbra_basic_block_optimize(struct umbra_basic_block*);

typedef struct {int id;} umbra_val;
typedef struct {int ix;} umbra_ptr;

umbra_val umbra_lane(struct umbra_basic_block*);

int       umbra_reserve    (struct umbra_basic_block*, int n);
int       umbra_reserve_ptr(struct umbra_basic_block*);
umbra_ptr umbra_deref_ptr(struct umbra_basic_block*,
                          umbra_ptr buf,
                          int byte_off);
int       umbra_uni_len    (struct umbra_basic_block const*);
void      umbra_set_uni_len(struct umbra_basic_block*, int);

umbra_val umbra_imm(struct umbra_basic_block*, int bits);

static inline umbra_val umbra_float(
    struct umbra_basic_block *bb, float v) {
    union { float f; int i; } u = {.f=v};
    return umbra_imm(bb, u.i);
}

umbra_val umbra_load32(struct umbra_basic_block*,
                       umbra_ptr src, umbra_val ix);
umbra_val umbra_load16(struct umbra_basic_block*,
                       umbra_ptr src, umbra_val ix);
umbra_val umbra_load_f16(struct umbra_basic_block*,
                         umbra_ptr src, umbra_val ix);

void umbra_store32(struct umbra_basic_block*,
                   umbra_ptr dst, umbra_val ix,
                   umbra_val);
void umbra_store16(struct umbra_basic_block*,
                   umbra_ptr dst, umbra_val ix,
                   umbra_val);
void umbra_store_f16(struct umbra_basic_block*,
                     umbra_ptr dst, umbra_val ix,
                     umbra_val);

umbra_val umbra_swiden16(struct umbra_basic_block*,
                        umbra_val);
umbra_val umbra_uwiden16(struct umbra_basic_block*,
                         umbra_val);
umbra_val umbra_narrow16(struct umbra_basic_block*,
                         umbra_val);

umbra_val umbra_htof(struct umbra_basic_block*,
                     umbra_val);
umbra_val umbra_ftoh(struct umbra_basic_block*,
                     umbra_val);

umbra_val umbra_fadd(struct umbra_basic_block*,
                     umbra_val, umbra_val);
umbra_val umbra_fsub(struct umbra_basic_block*,
                     umbra_val, umbra_val);
umbra_val umbra_fmul(struct umbra_basic_block*,
                     umbra_val, umbra_val);
umbra_val umbra_fdiv(struct umbra_basic_block*,
                     umbra_val, umbra_val);
umbra_val umbra_fmin(struct umbra_basic_block*,
                     umbra_val, umbra_val);
umbra_val umbra_fmax(struct umbra_basic_block*,
                     umbra_val, umbra_val);

umbra_val umbra_sqrt(struct umbra_basic_block*,
                     umbra_val);

umbra_val umbra_iadd(struct umbra_basic_block*,
                     umbra_val, umbra_val);
umbra_val umbra_isub(struct umbra_basic_block*,
                     umbra_val, umbra_val);
umbra_val umbra_imul(struct umbra_basic_block*,
                     umbra_val, umbra_val);
umbra_val umbra_ishl(struct umbra_basic_block*,
                     umbra_val, umbra_val);
umbra_val umbra_ushr(struct umbra_basic_block*,
                     umbra_val, umbra_val);
umbra_val umbra_sshr(struct umbra_basic_block*,
                     umbra_val, umbra_val);

umbra_val umbra_and(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_or (struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_xor(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_sel(struct umbra_basic_block*,
                    umbra_val, umbra_val, umbra_val);


umbra_val umbra_itof(struct umbra_basic_block*,
                     umbra_val);
umbra_val umbra_ftoi(struct umbra_basic_block*,
                     umbra_val);

void umbra_load8x4(struct umbra_basic_block*,
                   umbra_ptr src, umbra_val ix,
                   umbra_val out[4]);
void umbra_store8x4(struct umbra_basic_block*,
                    umbra_ptr dst, umbra_val ix,
                    umbra_val in[4]);

umbra_val umbra_feq(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_fne(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_flt(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_fle(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_fgt(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_fge(struct umbra_basic_block*,
                    umbra_val, umbra_val);

umbra_val umbra_ieq(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_ine(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_slt(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_sle(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_sgt(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_sge(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_ult(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_ule(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_ugt(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_uge(struct umbra_basic_block*,
                    umbra_val, umbra_val);

typedef struct { void *ptr; long sz; } umbra_buf;

struct umbra_interpreter*
umbra_interpreter(struct umbra_basic_block const*);
void umbra_interpreter_run(struct umbra_interpreter*,
                           int n, umbra_buf[]);
void umbra_interpreter_free(struct umbra_interpreter*);

struct umbra_codegen*
umbra_codegen(struct umbra_basic_block const*);
void umbra_codegen_run(struct umbra_codegen*,
                       int n, umbra_buf[]);
void umbra_codegen_free(struct umbra_codegen*);

struct umbra_jit*
umbra_jit(struct umbra_basic_block const*);
void umbra_jit_run (struct umbra_jit*, int n, umbra_buf[]);
void umbra_jit_free(struct umbra_jit*);

struct umbra_metal*
umbra_metal(struct umbra_basic_block const*);
void umbra_metal_run        (struct umbra_metal*,
                             int n, umbra_buf[]);
void umbra_metal_begin_batch(struct umbra_metal*);
void umbra_metal_flush      (struct umbra_metal*);
void umbra_metal_free       (struct umbra_metal*);

#include <stdio.h>
void umbra_basic_block_dump(struct umbra_basic_block const*,
                            FILE*);
void umbra_codegen_dump(struct umbra_codegen const*, FILE*);
void umbra_jit_dump    (struct umbra_jit const*, FILE*);
void umbra_jit_dump_bin(struct umbra_jit const*, FILE*);
void umbra_jit_mca     (struct umbra_jit const*, FILE*);
void umbra_metal_dump  (struct umbra_metal const*, FILE*);
