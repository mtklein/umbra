#pragma once

struct umbra_basic_block* umbra_basic_block(void);
void umbra_basic_block_free(struct umbra_basic_block*);
void umbra_basic_block_optimize(struct umbra_basic_block*);

typedef struct {int id;} umbra_i32;
typedef struct {int id;} umbra_f32;
typedef struct {int ix;} umbra_ptr;

umbra_i32 umbra_lane(struct umbra_basic_block*);

int       umbra_reserve_i32 (struct umbra_basic_block*, int n);
int       umbra_reserve_f32 (struct umbra_basic_block*, int n);
int       umbra_reserve_ptr (struct umbra_basic_block*);
umbra_ptr umbra_deref_ptr   (struct umbra_basic_block*, umbra_ptr buf, int byte_off);
int       umbra_uni_len    (struct umbra_basic_block const*);
void      umbra_set_uni_len(struct umbra_basic_block*, int);

umbra_i32 umbra_iimm(struct umbra_basic_block*, unsigned int   bits);
umbra_f32 umbra_fimm(struct umbra_basic_block*, float);

umbra_i32 umbra_iload (struct umbra_basic_block*, umbra_ptr src, umbra_i32 ix);
umbra_f32 umbra_fload (struct umbra_basic_block*, umbra_ptr src, umbra_i32 ix);
umbra_i32 umbra_i16load (struct umbra_basic_block*, umbra_ptr src, umbra_i32 ix);
umbra_f32 umbra_load_f16(struct umbra_basic_block*, umbra_ptr src, umbra_i32 ix);

void umbra_istore (struct umbra_basic_block*, umbra_ptr dst, umbra_i32 ix, umbra_i32);
void umbra_fstore (struct umbra_basic_block*, umbra_ptr dst, umbra_i32 ix, umbra_f32);
void umbra_i16store (struct umbra_basic_block*, umbra_ptr dst, umbra_i32 ix, umbra_i32);
void umbra_store_f16(struct umbra_basic_block*, umbra_ptr dst, umbra_i32 ix, umbra_f32);

umbra_f32 umbra_fadd(struct umbra_basic_block*, umbra_f32, umbra_f32);
umbra_f32 umbra_fsub(struct umbra_basic_block*, umbra_f32, umbra_f32);
umbra_f32 umbra_fmul(struct umbra_basic_block*, umbra_f32, umbra_f32);
umbra_f32 umbra_fdiv(struct umbra_basic_block*, umbra_f32, umbra_f32);
umbra_f32 umbra_fmin(struct umbra_basic_block*, umbra_f32, umbra_f32);
umbra_f32 umbra_fmax(struct umbra_basic_block*, umbra_f32, umbra_f32);

umbra_f32 umbra_fsqrt(struct umbra_basic_block*, umbra_f32);

umbra_i32 umbra_iadd(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_isub(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_imul(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_ishl(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_ushr(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_sshr(struct umbra_basic_block*, umbra_i32, umbra_i32);

umbra_i32 umbra_and(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_or (struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_xor(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_isel(struct umbra_basic_block*, umbra_i32, umbra_i32, umbra_i32);
umbra_f32 umbra_fsel(struct umbra_basic_block*, umbra_i32, umbra_f32, umbra_f32);

umbra_f32 umbra_itof(struct umbra_basic_block*, umbra_i32);
umbra_i32 umbra_ftoi(struct umbra_basic_block*, umbra_f32);

void umbra_load8x4 (struct umbra_basic_block*, umbra_ptr src, umbra_i32 ix, umbra_i32 out[4]);
void umbra_store8x4(struct umbra_basic_block*, umbra_ptr dst, umbra_i32 ix, umbra_i32 in[4]);

umbra_i32 umbra_feq(struct umbra_basic_block*, umbra_f32, umbra_f32);
umbra_i32 umbra_fne(struct umbra_basic_block*, umbra_f32, umbra_f32);
umbra_i32 umbra_flt(struct umbra_basic_block*, umbra_f32, umbra_f32);
umbra_i32 umbra_fle(struct umbra_basic_block*, umbra_f32, umbra_f32);
umbra_i32 umbra_fgt(struct umbra_basic_block*, umbra_f32, umbra_f32);
umbra_i32 umbra_fge(struct umbra_basic_block*, umbra_f32, umbra_f32);

umbra_i32 umbra_ieq(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_ine(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_slt(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_sle(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_sgt(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_sge(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_ult(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_ule(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_ugt(struct umbra_basic_block*, umbra_i32, umbra_i32);
umbra_i32 umbra_uge(struct umbra_basic_block*, umbra_i32, umbra_i32);

typedef struct { void *ptr; long sz; } umbra_buf;

struct umbra_interpreter* umbra_interpreter(struct umbra_basic_block const*);
void umbra_interpreter_run (struct umbra_interpreter*, int n, umbra_buf[]);
void umbra_interpreter_free(struct umbra_interpreter*);

struct umbra_codegen* umbra_codegen(struct umbra_basic_block const*);
void umbra_codegen_run (struct umbra_codegen*, int n, umbra_buf[]);
void umbra_codegen_free(struct umbra_codegen*);

struct umbra_jit* umbra_jit(struct umbra_basic_block const*);
void umbra_jit_run (struct umbra_jit*, int n, umbra_buf[]);
void umbra_jit_free(struct umbra_jit*);

struct umbra_metal* umbra_metal(struct umbra_basic_block const*);
void umbra_metal_run        (struct umbra_metal*, int n, umbra_buf[]);
void umbra_metal_begin_batch(struct umbra_metal*);
void umbra_metal_flush      (struct umbra_metal*);
void umbra_metal_free       (struct umbra_metal*);

#include <stdio.h>
void umbra_basic_block_dump(struct umbra_basic_block const*, FILE*);
void umbra_codegen_dump    (struct umbra_codegen const*, FILE*);
void umbra_jit_dump        (struct umbra_jit const*, FILE*);
void umbra_jit_dump_bin    (struct umbra_jit const*, FILE*);
void umbra_jit_mca         (struct umbra_jit const*, FILE*);
void umbra_metal_dump      (struct umbra_metal const*, FILE*);
