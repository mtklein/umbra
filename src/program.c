#include "program.h"
#include <assert.h>
#include <stdlib.h>

static void nop_flush(struct umbra_backend *be) { (void)be; }
static void nop_dump(void const *ctx, FILE *f) { (void)ctx; (void)f; }

struct umbra_backend {
    struct umbra_program* (*compile)(struct umbra_backend*,
                                     struct umbra_basic_block const*);
    void  (*flush)(struct umbra_backend*);
    void  (*free_fn)(struct umbra_backend*);
    void  *ctx;
    int     threadsafe, :32;
};

struct umbra_program {
    void *ctx;
    void (*queue)(void*, int, int, int, int, umbra_buf[]);
    void (*dump)(void const*, FILE*);
    void (*free_fn)(void*);
    struct umbra_backend *backend;
};

static void run_interp(void *ctx, int l, int t, int r, int b, umbra_buf buf[]) {
    umbra_interpreter_run(ctx, l, t, r, b, buf);
}
static void                  free_interp(void *ctx) { umbra_interpreter_free(ctx); }
static struct umbra_program *compile_interp(struct umbra_backend           *be,
                                            struct umbra_basic_block const *bb) {
    struct umbra_interpreter *const p = umbra_interpreter(bb);
    assert(p);
    struct umbra_program *const prog = malloc(sizeof *prog);
    *prog = (struct umbra_program){
        .ctx = p,
        .queue = run_interp,
        .dump = nop_dump,
        .free_fn = free_interp,
        .backend = be,
    };
    return prog;
}
static void           free_be_interp(struct umbra_backend *be) { free(be); }

static void run_switch(void *ctx, int l, int t, int r, int b, umbra_buf buf[]) {
    umbra_switch_interp_run(ctx, l, t, r, b, buf);
}
static void                  free_switch(void *ctx) { umbra_switch_interp_free(ctx); }
static struct umbra_program *compile_switch(struct umbra_backend           *be,
                                            struct umbra_basic_block const *bb) {
    struct umbra_switch_interp *const p = umbra_switch_interp(bb);
    assert(p);
    struct umbra_program *const prog = malloc(sizeof *prog);
    *prog = (struct umbra_program){
        .ctx = p,
        .queue = run_switch,
        .dump = nop_dump,
        .free_fn = free_switch,
        .backend = be,
    };
    return prog;
}
static void           free_be_switch(struct umbra_backend *be) { free(be); }
struct umbra_backend *umbra_backend_switch(void) {
    struct umbra_backend *const be = malloc(sizeof *be);
    *be = (struct umbra_backend){
        .compile = compile_switch,
        .flush = nop_flush,
        .free_fn = free_be_switch,
        .threadsafe = 1,
    };
    return be;
}

struct umbra_backend *umbra_backend_interp(void) {
    struct umbra_backend *const be = malloc(sizeof *be);
    *be = (struct umbra_backend){
        .compile = compile_interp,
        .flush = nop_flush,
        .free_fn = free_be_interp,
        .threadsafe = 1,
    };
    return be;
}

static void run_jit(void *ctx, int l, int t, int r, int b, umbra_buf buf[]) {
    umbra_jit_run(ctx, l, t, r, b, buf);
}
static void dump_jit(void const *ctx, FILE *f) { umbra_dump_jit_mca(ctx, f); }
static void free_jit(void *ctx) { umbra_jit_free(ctx); }
static struct umbra_program *compile_jit(struct umbra_backend           *be,
                                         struct umbra_basic_block const *bb) {
    struct umbra_jit *const j = umbra_jit(bb);
#if defined(__aarch64__) || defined(__AVX2__)
    assert(j);
#endif
    if (j) {
        struct umbra_program *const prog = malloc(sizeof *prog);
        *prog = (struct umbra_program){
            .ctx = j,
            .queue = run_jit,
            .dump = dump_jit,
            .free_fn = free_jit,
            .backend = be,
        };
        return prog;
    }
    return NULL;
}
static void           free_be_jit(struct umbra_backend *be) { free(be); }
struct umbra_backend *umbra_backend_jit(void) {
    struct umbra_backend *const be = malloc(sizeof *be);
    *be = (struct umbra_backend){
        .compile = compile_jit,
        .flush = nop_flush,
        .free_fn = free_be_jit,
        .threadsafe = 1,
    };
    return be;
}

static void run_metal(void *ctx, int l, int t, int r, int b, umbra_buf buf[]) {
    umbra_metal_run(ctx, l, t, r, b, buf);
}
static void dump_metal(void const *ctx, FILE *f) { umbra_dump_metal(ctx, f); }
static void free_metal(void *ctx) { umbra_metal_free(ctx); }
static struct umbra_program *compile_metal(struct umbra_backend           *be,
                                           struct umbra_basic_block const *bb) {
    struct umbra_metal *const m = umbra_metal(be->ctx, bb);
#if defined(__APPLE__) && !defined(__wasm__)
    assert(m);
#endif
    if (m) {
        struct umbra_program *const prog = malloc(sizeof *prog);
        *prog = (struct umbra_program){
            .ctx = m,
            .queue = run_metal,
            .dump = dump_metal,
            .free_fn = free_metal,
            .backend = be,
        };
        return prog;
    }
    return NULL;
}
static void flush_be_metal(struct umbra_backend *be) { umbra_metal_flush(be->ctx); }
static void free_be_metal(struct umbra_backend *be) {
    umbra_metal_flush(be->ctx);
    umbra_metal_backend_free(be->ctx);
    free(be);
}
struct umbra_backend *umbra_backend_metal(void) {
    void *const ctx = umbra_metal_backend_create();
    if (ctx) {
        struct umbra_backend *const be = malloc(sizeof *be);
        *be = (struct umbra_backend){
            .compile = compile_metal,
            .flush = flush_be_metal,
            .free_fn = free_be_metal,
            .ctx = ctx,
        };
        return be;
    }
    return NULL;
}

struct umbra_program *umbra_program(struct umbra_backend           *be,
                                            struct umbra_basic_block const *bb) {
    return be->compile(be, bb);
}

void umbra_backend_flush(struct umbra_backend *be) {
    if (be) {
        be->flush(be);
    }
}

_Bool umbra_backend_threadsafe(struct umbra_backend const *be) {
    return be && be->threadsafe;
}

void umbra_backend_free(struct umbra_backend *be) {
    if (be) {
        be->free_fn(be);
    }
}

struct umbra_backend* umbra_program_backend(struct umbra_program const *p) {
    return p->backend;
}

void umbra_program_queue(struct umbra_program *p,
                         int l, int t, int r, int b, umbra_buf buf[]) {
    if (r > l && b > t) {
        p->queue(p->ctx, l, t, r, b, buf);
    }
}

void umbra_program_dump(struct umbra_program *p, FILE *f) {
    if (p) {
        p->dump(p->ctx, f);
    }
}

void umbra_program_free(struct umbra_program *p) {
    if (p) {
        p->free_fn(p->ctx);
        free(p);
    }
}
