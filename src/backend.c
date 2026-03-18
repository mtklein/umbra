#include "../umbra.h"
#include <stdlib.h>

struct umbra_backend {
    void  *ctx;
    void (*run)        (void*, int, umbra_buf[]);
    void (*begin_batch)(void*);
    void (*flush)      (void*);
    void (*free_fn)    (void*);
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
        .run     = run_interp,
        .free_fn = free_interp,
    };
    return b;
}

static void run_jit(void *ctx, int n,
                    umbra_buf buf[]) {
    umbra_jit_run(ctx, n, buf);
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
        .run     = run_jit,
        .free_fn = free_jit,
    };
    return b;
}

static void run_metal(void *ctx, int n,
                      umbra_buf buf[]) {
    umbra_metal_run(ctx, n, buf);
}
static void batch_metal(void *ctx) {
    umbra_metal_begin_batch(ctx);
}
static void flush_metal(void *ctx) {
    umbra_metal_flush(ctx);
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
        .ctx         = m,
        .run         = run_metal,
        .begin_batch = batch_metal,
        .flush       = flush_metal,
        .free_fn     = free_metal,
    };
    return b;
}

void umbra_backend_run(struct umbra_backend *b,
                       int n, umbra_buf buf[]) {
    b->run(b->ctx, n, buf);
}

void umbra_backend_begin_batch(
        struct umbra_backend *b) {
    if (b && b->begin_batch) {
        b->begin_batch(b->ctx);
    }
}

void umbra_backend_flush(struct umbra_backend *b) {
    if (b && b->flush) { b->flush(b->ctx); }
}

void umbra_backend_free(struct umbra_backend *b) {
    if (!b) { return; }
    if (b->free_fn) { b->free_fn(b->ctx); }
    free(b);
}
