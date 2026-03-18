#include "backend.h"
#include <stdlib.h>

typedef struct umbra_backend*
    (*build_fn)(struct umbra_basic_block const*);

struct umbra_backend {
    void    *ctx;
    void   (*queue)   (void*, int, umbra_buf[]);
    void   (*flush)   (void*);
    void   (*dump)    (void const*, FILE*);
    void   (*free_fn) (void*);
    build_fn build;
};

static void run_interp(void *ctx, int n,
                       umbra_buf buf[]) {
    umbra_interpreter_run(ctx, n, buf);
}
static void free_interp(void *ctx) {
    umbra_interpreter_free(ctx);
}

struct umbra_backend* umbra_backend_interp(
        struct umbra_basic_block const *bb) {
    struct umbra_interpreter *p =
        umbra_interpreter(bb);
    if (!p) { return NULL; }
    struct umbra_backend *b = malloc(sizeof *b);
    *b = (struct umbra_backend){
        .ctx     = p,
        .queue   = run_interp,
        .free_fn = free_interp,
        .build   = umbra_backend_interp,
    };
    return b;
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

struct umbra_backend* umbra_backend_jit(
        struct umbra_basic_block const *bb) {
    struct umbra_jit *j = umbra_jit(bb);
    if (!j) { return NULL; }
    struct umbra_backend *b = malloc(sizeof *b);
    *b = (struct umbra_backend){
        .ctx     = j,
        .queue   = run_jit,
        .dump    = dump_jit,
        .free_fn = free_jit,
        .build   = umbra_backend_jit,
    };
    return b;
}

static void run_metal(void *ctx, int n,
                      umbra_buf buf[]) {
    umbra_metal_run(ctx, n, buf);
}
static void flush_metal(void *ctx) {
    umbra_metal_flush(ctx);
}
static void dump_metal(void const *ctx, FILE *f) {
    umbra_dump_metal(ctx, f);
}
static void free_metal(void *ctx) {
    umbra_metal_free(ctx);
}

struct umbra_backend* umbra_backend_metal(
        struct umbra_basic_block const *bb) {
    struct umbra_metal *m = umbra_metal(bb);
    if (!m) { return NULL; }
    struct umbra_backend *b = malloc(sizeof *b);
    *b = (struct umbra_backend){
        .ctx     = m,
        .queue   = run_metal,
        .flush   = flush_metal,
        .dump    = dump_metal,
        .free_fn = free_metal,
        .build   = umbra_backend_metal,
    };
    return b;
}

void umbra_backend_queue(struct umbra_backend *b,
                        int n, umbra_buf buf[]) {
    b->queue(b->ctx, n, buf);
}

void umbra_backend_flush(struct umbra_backend *b) {
    if (b && b->flush) { b->flush(b->ctx); }
}

void umbra_backend_dump(struct umbra_backend *b,
                        FILE *f) {
    if (b && b->dump) { b->dump(b->ctx, f); }
}

typedef struct umbra_backend*
    (*umbra_backend_ctor_fn)(
        struct umbra_basic_block const*);

umbra_backend_ctor_fn umbra_backend_ctor(
        struct umbra_backend const *b) {
    return b->build;
}

void umbra_backend_free(struct umbra_backend *b) {
    if (!b) { return; }
    if (b->free_fn) { b->free_fn(b->ctx); }
    free(b);
}
