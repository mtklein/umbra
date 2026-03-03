#pragma once

#include <stdint.h>

// Let's start replacing the umbra_inst oriented user interface with a more
// programatic basic block builder API.  By the end of this project, umbra_inst
// should be an internal implementation detail of how umbra_basic_block and its
// backends like the interpreter interoperate.

struct umbra_basic_block* umbra_basic_block(void);
void umbra_basic_block_free(struct umbra_basic_block*);

typedef struct {int id;} umbra_v16;
typedef struct {int id;} umbra_v32;
typedef struct {int ix;} umbra_ptr;

umbra_v32 umbra_lane(struct umbra_basic_block*);

umbra_v16 umbra_imm_16(struct umbra_basic_block*, uint16_t bits);
umbra_v32 umbra_imm_32(struct umbra_basic_block*, uint32_t bits);

umbra_v16 umbra_load_16(struct umbra_basic_block*, umbra_ptr src, umbra_v32 ix);
umbra_v32 umbra_load_32(struct umbra_basic_block*, umbra_ptr src, umbra_v32 ix);

void umbra_store_16(struct umbra_basic_block*, umbra_ptr dst, umbra_v32 ix, umbra_v16);
void umbra_store_32(struct umbra_basic_block*, umbra_ptr dst, umbra_v32 ix, umbra_v32);

umbra_v16 umbra_add_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_sub_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_mul_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_div_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_min_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_max_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);

umbra_v32 umbra_add_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_sub_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_mul_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_div_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_min_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_max_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);

umbra_v16 umbra_sqrt_f16(struct umbra_basic_block*, umbra_v16);
umbra_v32 umbra_sqrt_f32(struct umbra_basic_block*, umbra_v32);

umbra_v16 umbra_add_i16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_sub_i16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_mul_i16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_shl_i16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_shr_u16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_shr_s16(struct umbra_basic_block*, umbra_v16, umbra_v16);

umbra_v32 umbra_add_i32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_sub_i32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_mul_i32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_shl_i32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_shr_u32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_shr_s32(struct umbra_basic_block*, umbra_v32, umbra_v32);

umbra_v16 umbra_and_16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_or_16 (struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_xor_16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_sel_16(struct umbra_basic_block*, umbra_v16, umbra_v16, umbra_v16);

umbra_v32 umbra_and_32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_or_32 (struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_xor_32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_sel_32(struct umbra_basic_block*, umbra_v32, umbra_v32, umbra_v32);

umbra_v16 umbra_f16_from_f32(struct umbra_basic_block*, umbra_v32);
umbra_v32 umbra_f32_from_f16(struct umbra_basic_block*, umbra_v16);
umbra_v32 umbra_f32_from_i32(struct umbra_basic_block*, umbra_v32);
umbra_v32 umbra_i32_from_f32(struct umbra_basic_block*, umbra_v32);

umbra_v16 umbra_eq_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_ne_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_lt_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_le_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_gt_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_ge_f16(struct umbra_basic_block*, umbra_v16, umbra_v16);

umbra_v32 umbra_eq_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_ne_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_lt_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_le_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_gt_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_ge_f32(struct umbra_basic_block*, umbra_v32, umbra_v32);

umbra_v16 umbra_eq_i16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_ne_i16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_lt_s16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_le_s16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_gt_s16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_ge_s16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_lt_u16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_le_u16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_gt_u16(struct umbra_basic_block*, umbra_v16, umbra_v16);
umbra_v16 umbra_ge_u16(struct umbra_basic_block*, umbra_v16, umbra_v16);

umbra_v32 umbra_eq_i32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_ne_i32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_lt_s32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_le_s32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_gt_s32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_ge_s32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_lt_u32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_le_u32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_gt_u32(struct umbra_basic_block*, umbra_v32, umbra_v32);
umbra_v32 umbra_ge_u32(struct umbra_basic_block*, umbra_v32, umbra_v32);

void umbra_basic_block_exec(struct umbra_basic_block*, int n, void* ptr[]);

// Many optimizations now move into these umbra_basic_block methods:
//    1) each operation knows how to canonicalize itself
//    2) each operation knows its own arity, how to test itself for constant prop
//    3) each operation knows how to do its own strength reductions
//    4) deduplication happens in these methods,
//       so users never see un-deduplicated umbra_v16/umbra_v32
// FMA doesn't need its own entry point, that will be handled by peepholes.
//
// Dead code elimination and loop invariant hoisting will still have to happen
// at the end once we have the full program I think.

enum umbra_op {
    op_lane,

    op_imm_16, op_load_16, op_store_16,
    op_imm_32, op_load_32, op_store_32,

    op_add_f16, op_sub_f16, op_mul_f16, op_div_f16,
    op_add_f32, op_sub_f32, op_mul_f32, op_div_f32,

    op_min_f16, op_max_f16, op_sqrt_f16, op_fma_f16,
    op_min_f32, op_max_f32, op_sqrt_f32, op_fma_f32,

    op_add_i16, op_sub_i16, op_mul_i16,
    op_add_i32, op_sub_i32, op_mul_i32,

    op_shl_i16, op_shr_u16, op_shr_s16,
    op_shl_i32, op_shr_u32, op_shr_s32,

    op_and_16, op_or_16,  op_xor_16, op_sel_16,
    op_and_32, op_or_32,  op_xor_32, op_sel_32,

    op_f16_from_f32, op_f32_from_f16,
    op_f32_from_i32, op_i32_from_f32,

    op_eq_f16, op_ne_f16, op_lt_f16, op_le_f16, op_gt_f16, op_ge_f16,
    op_eq_f32, op_ne_f32, op_lt_f32, op_le_f32, op_gt_f32, op_ge_f32,

    op_eq_i16, op_ne_i16,
    op_eq_i32, op_ne_i32,
    op_lt_s16, op_le_s16, op_gt_s16, op_ge_s16,
    op_lt_s32, op_le_s32, op_gt_s32, op_ge_s32,
    op_lt_u16, op_le_u16, op_gt_u16, op_ge_u16,
    op_lt_u32, op_le_u32, op_gt_u32, op_ge_u32,
};



struct umbra_inst {
    enum umbra_op op;
    int           x,y,z;
    union   { int ptr; int immi; float immf; };
};
int umbra_optimize(struct umbra_inst[], int insts);

struct umbra_interpreter* umbra_interpreter     (struct umbra_inst const[], int insts);
void                      umbra_interpreter_free(struct umbra_interpreter*);
void umbra_interpreter_run(struct umbra_interpreter const*, int n, void* ptr[]);
