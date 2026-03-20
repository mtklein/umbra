#include "program.h"
#include <stdlib.h>

struct umbra_backend {
    struct umbra_program* (*compile)(
        struct umbra_backend*,
        struct umbra_basic_block const*);
    void (*flush)  (struct umbra_backend*);
    void (*free_fn)(struct umbra_backend*);
    void *ctx;
};

struct umbra_program {
    void    *ctx;
    void   (*queue)   (void*, int, umbra_buf[]);
    void   (*dump)    (void const*, FILE*);
    void   (*free_fn) (void*);
    struct umbra_backend *backend;
};

static void run_interp(void *ctx, int n,
                       umbra_buf buf[]) {
    umbra_interpreter_run(ctx, n, buf);
}
static void free_interp(void *ctx) {
    umbra_interpreter_free(ctx);
}
static struct umbra_program* compile_interp(
        struct umbra_backend *be,
        struct umbra_basic_block const *bb) {
    struct umbra_interpreter *p =
        umbra_interpreter(bb);
    if (!p) { return NULL; }
    struct umbra_program *prog = malloc(sizeof *prog);
    *prog = (struct umbra_program){
        .ctx     = p,
        .queue   = run_interp,
        .free_fn = free_interp,
        .backend = be,
    };
    return prog;
}
static void free_be_interp(struct umbra_backend *be) {
    free(be);
}
struct umbra_backend* umbra_backend_interp(void) {
    struct umbra_backend *be = malloc(sizeof *be);
    *be = (struct umbra_backend){
        .compile = compile_interp,
        .free_fn = free_be_interp,
    };
    return be;
}

static void run_jit(void *ctx, int n,
                    umbra_buf buf[]) {
    umbra_jit_run(ctx, n, buf);
}
static void dump_jit(void const *ctx, FILE *f) {
    umbra_dump_jit_mca(ctx, f);
}
static void free_jit(void *ctx) {
    umbra_jit_free(ctx);
}
static struct umbra_program* compile_jit(
        struct umbra_backend *be,
        struct umbra_basic_block const *bb) {
    struct umbra_jit *j = umbra_jit(bb);
    if (!j) { return NULL; }
    struct umbra_program *prog = malloc(sizeof *prog);
    *prog = (struct umbra_program){
        .ctx     = j,
        .queue   = run_jit,
        .dump    = dump_jit,
        .free_fn = free_jit,
        .backend = be,
    };
    return prog;
}
static void free_be_jit(struct umbra_backend *be) {
    free(be);
}
struct umbra_backend* umbra_backend_jit(void) {
    struct umbra_backend *be = malloc(sizeof *be);
    *be = (struct umbra_backend){
        .compile = compile_jit,
        .free_fn = free_be_jit,
    };
    return be;
}

static void run_metal(void *ctx, int n,
                      umbra_buf buf[]) {
    umbra_metal_run(ctx, n, buf);
}
static void dump_metal(void const *ctx, FILE *f) {
    umbra_dump_metal(ctx, f);
}
static void free_metal(void *ctx) {
    umbra_metal_free(ctx);
}
static struct umbra_program* compile_metal(
        struct umbra_backend *be,
        struct umbra_basic_block const *bb) {
    struct umbra_metal *m = umbra_metal(be->ctx, bb);
    if (!m) { return NULL; }
    struct umbra_program *prog = malloc(sizeof *prog);
    *prog = (struct umbra_program){
        .ctx     = m,
        .queue   = run_metal,
        .dump    = dump_metal,
        .free_fn = free_metal,
        .backend = be,
    };
    return prog;
}
static void flush_be_metal(struct umbra_backend *be) {
    umbra_metal_flush(be->ctx);
}
static void free_be_metal(struct umbra_backend *be) {
    umbra_metal_flush(be->ctx);
    umbra_metal_backend_free(be->ctx);
    free(be);
}
struct umbra_backend* umbra_backend_metal(void) {
    void *ctx = umbra_metal_backend_create();
    if (!ctx) { return NULL; }
    struct umbra_backend *be = malloc(sizeof *be);
    *be = (struct umbra_backend){
        .compile = compile_metal,
        .flush   = flush_be_metal,
        .free_fn = free_be_metal,
        .ctx     = ctx,
    };
    return be;
}

static void run_mlx(void *ctx, int n,
                     umbra_buf buf[]) {
    umbra_mlx_run(ctx, n, buf);
}
static void free_mlx(void *ctx) {
    umbra_mlx_free(ctx);
}
static struct umbra_program* compile_mlx(
        struct umbra_backend *be,
        struct umbra_basic_block const *bb) {
    struct umbra_mlx *m = umbra_mlx(bb);
    if (!m) { return NULL; }
    struct umbra_program *prog = malloc(sizeof *prog);
    *prog = (struct umbra_program){
        .ctx     = m,
        .queue   = run_mlx,
        .free_fn = free_mlx,
        .backend = be,
    };
    return prog;
}
static void free_be_mlx(struct umbra_backend *be) {
    free(be);
}
struct umbra_backend* umbra_backend_mlx(void) {
    struct umbra_backend *be = malloc(sizeof *be);
    *be = (struct umbra_backend){
        .compile = compile_mlx,
        .free_fn = free_be_mlx,
    };
    return be;
}

struct umbra_program* umbra_backend_compile(
        struct umbra_backend *be,
        struct umbra_basic_block const *bb) {
    return be->compile(be, bb);
}

void umbra_backend_flush(struct umbra_backend *be) {
    if (be && be->flush) { be->flush(be); }
}

void umbra_backend_free(struct umbra_backend *be) {
    if (!be) { return; }
    if (be->free_fn) { be->free_fn(be); }
}

struct umbra_backend* umbra_program_backend(
        struct umbra_program *p) {
    return p->backend;
}

void umbra_program_queue(struct umbra_program *p,
                         int n, umbra_buf buf[]) {
    p->queue(p->ctx, n, buf);
}

void umbra_program_dump(struct umbra_program *p,
                        FILE *f) {
    if (p && p->dump) { p->dump(p->ctx, f); }
}

void umbra_program_free(struct umbra_program *p) {
    if (!p) { return; }
    if (p->free_fn) { p->free_fn(p->ctx); }
    free(p);
}
