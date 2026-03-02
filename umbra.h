#pragma once

#include <stddef.h>

struct umbra_inst {
    enum {
        umbra_imm_16, umbra_uni_16, umbra_gather_16, umbra_load_16, umbra_store_16,
        umbra_imm_32, umbra_uni_32, umbra_gather_32, umbra_load_32, umbra_store_32,

        umbra_add_f16, umbra_sub_f16, umbra_mul_f16, umbra_div_f16,
        umbra_add_f32, umbra_sub_f32, umbra_mul_f32, umbra_div_f32,

        umbra_add_i16, umbra_sub_i16, umbra_mul_i16,
        umbra_shl_i16, umbra_shr_i16, umbra_sra_i16,
        umbra_and_i16, umbra_or_i16,  umbra_xor_i16, umbra_sel_i16,

        umbra_add_i32, umbra_sub_i32, umbra_mul_i32,
        umbra_shl_i32, umbra_shr_i32, umbra_sra_i32,
        umbra_and_i32, umbra_or_i32,  umbra_xor_i32, umbra_sel_i32,
    } op;
    int    x,y,z;
    union { int immi; float immf; };
    int    ptr;
};

struct umbra_program* umbra_program     (struct umbra_inst const inst[], int insts);
void                  umbra_program_run (struct umbra_program const*, int n, void* ptr[]);
void                  umbra_program_free(struct umbra_program*);
