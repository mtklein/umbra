#pragma once

#include <stddef.h>

struct umbra_inst {
    enum {
        umbra_imm_32, umbra_uni_32, umbra_gather_32, umbra_load_32, umbra_store_32,
        umbra_add_f32, umbra_sub_f32, umbra_mul_f32, umbra_div_f32,
    } op;
    int    x,y,z;
    union { int immi; float immf; };
    int    ptr;
    size_t offset;
};

struct umbra_program* umbra_program     (struct umbra_inst const inst[], int insts);
void                  umbra_program_run (struct umbra_program const*, int n, void* ptr[]);
void                  umbra_program_free(struct umbra_program*);
