#pragma once
#include "../include/umbra.h"

struct umbra_interpreter *umbra_interpreter(struct umbra_basic_block const *);
void umbra_interpreter_run(struct umbra_interpreter *, int n, int w, int y0, umbra_buf[]);
void umbra_interpreter_free(struct umbra_interpreter *);

struct umbra_jit *umbra_jit(struct umbra_basic_block const *);
void              umbra_jit_run(struct umbra_jit *, int n, int w, int y0, umbra_buf[]);
void              umbra_jit_free(struct umbra_jit *);

void *umbra_metal_backend_create(void);
void  umbra_metal_backend_free(void *);

struct umbra_metal *umbra_metal(void *backend_ctx, struct umbra_basic_block const *);
void                umbra_metal_run(struct umbra_metal *, int n, int w, int y0, umbra_buf[]);
void                umbra_metal_begin_batch(void *);
void                umbra_metal_flush(void *);
void                umbra_metal_free(struct umbra_metal *);

#include <stdio.h>
void umbra_dump_jit_mca(struct umbra_jit const *, FILE *);
void umbra_dump_metal(struct umbra_metal const *, FILE *);
