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

umbra_val umbra_imm_i32(struct umbra_basic_block*, int bits);

static inline umbra_val umbra_imm_f32(
    struct umbra_basic_block *bb, float v) {
    union { float f; int i; } u = {.f=v};
    return umbra_imm_i32(bb, u.i);
}

umbra_val umbra_load_i32(struct umbra_basic_block*,
                       umbra_ptr src, umbra_val ix);
umbra_val umbra_load_i16(struct umbra_basic_block*,
                       umbra_ptr src, umbra_val ix);
void umbra_store_i32(struct umbra_basic_block*,
                   umbra_ptr dst, umbra_val ix,
                   umbra_val);
void umbra_store_i16(struct umbra_basic_block*,
                   umbra_ptr dst, umbra_val ix,
                   umbra_val);

umbra_val umbra_widen_s16(struct umbra_basic_block*,
                        umbra_val);
umbra_val umbra_widen_u16(struct umbra_basic_block*,
                         umbra_val);
umbra_val umbra_narrow_i16(struct umbra_basic_block*,
                         umbra_val);

umbra_val umbra_widen_f16(struct umbra_basic_block*,
                     umbra_val);
umbra_val umbra_narrow_f32(struct umbra_basic_block*,
                     umbra_val);

static inline umbra_val umbra_load_f16(
    struct umbra_basic_block *bb,
    umbra_ptr src, umbra_val ix) {
    return umbra_widen_f16(bb, umbra_load_i16(bb, src, ix));
}
static inline void umbra_store_f16(
    struct umbra_basic_block *bb,
    umbra_ptr dst, umbra_val ix, umbra_val v) {
    umbra_store_i16(bb, dst, ix, umbra_narrow_f32(bb, v));
}

umbra_val umbra_add_f32(struct umbra_basic_block*,
                     umbra_val, umbra_val);
umbra_val umbra_sub_f32(struct umbra_basic_block*,
                     umbra_val, umbra_val);
umbra_val umbra_mul_f32(struct umbra_basic_block*,
                     umbra_val, umbra_val);
umbra_val umbra_div_f32(struct umbra_basic_block*,
                     umbra_val, umbra_val);
umbra_val umbra_min_f32(struct umbra_basic_block*,
                     umbra_val, umbra_val);
umbra_val umbra_max_f32(struct umbra_basic_block*,
                     umbra_val, umbra_val);

umbra_val umbra_sqrt_f32(struct umbra_basic_block*,
                     umbra_val);

umbra_val umbra_add_i32(struct umbra_basic_block*,
                     umbra_val, umbra_val);
umbra_val umbra_sub_i32(struct umbra_basic_block*,
                     umbra_val, umbra_val);
umbra_val umbra_mul_i32(struct umbra_basic_block*,
                     umbra_val, umbra_val);
umbra_val umbra_shl_i32(struct umbra_basic_block*,
                     umbra_val, umbra_val);
umbra_val umbra_shr_u32(struct umbra_basic_block*,
                     umbra_val, umbra_val);
umbra_val umbra_shr_s32(struct umbra_basic_block*,
                     umbra_val, umbra_val);

umbra_val umbra_and_i32(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_or_i32 (struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_xor_i32(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_sel_i32(struct umbra_basic_block*,
                    umbra_val, umbra_val, umbra_val);


umbra_val umbra_cvt_f32_i32(struct umbra_basic_block*,
                     umbra_val);
umbra_val umbra_cvt_i32_f32(struct umbra_basic_block*,
                     umbra_val);

void umbra_load_u8x4(struct umbra_basic_block*,
                   umbra_ptr src, umbra_val ix,
                   umbra_val out[4]);
void umbra_store_u8x4(struct umbra_basic_block*,
                    umbra_ptr dst, umbra_val ix,
                    umbra_val in[4]);

umbra_val umbra_eq_f32(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_ne_f32(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_lt_f32(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_le_f32(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_gt_f32(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_ge_f32(struct umbra_basic_block*,
                    umbra_val, umbra_val);

umbra_val umbra_eq_i32(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_ne_i32(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_lt_s32(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_le_s32(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_gt_s32(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_ge_s32(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_lt_u32(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_le_u32(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_gt_u32(struct umbra_basic_block*,
                    umbra_val, umbra_val);
umbra_val umbra_ge_u32(struct umbra_basic_block*,
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
