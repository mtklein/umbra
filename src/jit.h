#pragma once
#include "basic_block.h"
#include <stdio.h>

struct code_buf { void *mem; size_t size; };

struct jit_backend {
    struct umbra_backend base;
    struct code_buf     *cache;
    int                  count, cap;
};

struct umbra_jit {
    struct umbra_program base;
    void  *code;
    size_t code_size;
    void (*entry)(int, int, int, int, struct umbra_buf *);
    int loop_start, loop_end;
};

struct code_buf acquire_code_buf(struct jit_backend*, size_t min_size);

struct ra;
void free_chan(struct ra*, val operand, int i);

struct umbra_jit* umbra_jit         (struct jit_backend*, struct umbra_basic_block const*);
void              umbra_jit_run     (struct umbra_jit*, int l, int t, int r, int b,
                                     struct umbra_buf[]);
void              umbra_dump_jit_mca(struct umbra_jit const*, FILE*);
