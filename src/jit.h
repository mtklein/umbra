#pragma once
#include "flat_ir.h"
#include <stdio.h>

struct jit_backend;
void acquire_code_buf(struct jit_backend*, void **mem, size_t *size, size_t min_size);
void release_code_buf(struct jit_backend*, void  *mem, size_t  size);

struct jit_program {
    struct umbra_program base;
    void  *code;
    size_t code_size;
    void (*entry)(int, int, int, int, struct umbra_buf*);
    int loop_start, loop_end;
    int n_reg, pad;
    struct umbra_uniform_reg *reg;
};
struct jit_program* jit_program(struct jit_backend*, struct umbra_flat_ir const*);
void   jit_program_run (struct jit_program*, int,int,int,int, struct umbra_buf[]);
void   jit_program_dump(struct jit_program const*, FILE*);
