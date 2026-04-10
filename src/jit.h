#pragma once
#include "basic_block.h"
#include <stdio.h>

struct jit_backend;
void acquire_code_buf(struct jit_backend*, void **mem, size_t *size, size_t min_size);
void release_code_buf(struct jit_backend*, void  *mem, size_t  size);

struct umbra_jit_program {
    struct umbra_program base;
    void  *code;
    size_t code_size;
    void (*entry)(int, int, int, int, struct umbra_buf*);
    int loop_start, loop_end;
};
struct umbra_jit_program* umbra_jit_program(struct jit_backend*, struct umbra_basic_block const*);
void   umbra_jit_program_run (struct umbra_jit_program*, int,int,int,int, struct umbra_buf[]);
void   umbra_jit_program_dump(struct umbra_jit_program const*, FILE*);
