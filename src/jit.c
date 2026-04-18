#include "jit.h"

#if !defined(__aarch64__) && !defined(__AVX2__)

struct umbra_backend* umbra_backend_jit(void) {
    return 0;
}

#else

#include "assume.h"
#include "ra.h"
#include <stdlib.h>
#include <sys/mman.h>

struct cache_entry { void *mem; size_t size; };

struct jit_backend {
    struct umbra_backend base;
    struct cache_entry  *cache;
    int                  count, cap;
};

void acquire_code_buf(struct jit_backend *be, void **mem, size_t *size, size_t min_size) {
    for (int i = be->count; i-- > 0;) {
        if (be->cache[i].size >= min_size) {
            *mem  = be->cache[i].mem;
            *size = be->cache[i].size;
            be->cache[i] = be->cache[--be->count];
            mprotect(*mem, *size, PROT_READ | PROT_WRITE);
            return;
        }
    }
    size_t const pg = 16384;
    *size = (min_size + pg - 1) & ~(pg - 1);
    *mem  = mmap(NULL, *size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANON, -1, 0);
    assume(*mem != MAP_FAILED);
}

void release_code_buf(struct jit_backend *be, void *mem, size_t size) {
    if (be->count == be->cap) {
        be->cap = be->cap ? 2 * be->cap : 4;
        be->cache = realloc(be->cache, (size_t)be->cap * sizeof *be->cache);
    }
    be->cache[be->count++] = (struct cache_entry){.mem = mem, .size = size};
}

static void run_jit(struct umbra_program *prog,
                    int l, int t, int r, int b, struct umbra_buf caller_buf[]) {
    struct jit_program *j = (struct jit_program*)prog;
    if (j->n_reg == 0) {
        jit_program_run(j, l, t, r, b, caller_buf);
        return;
    }
    // Thread-local scratch: caller-provided prefix [0, caller_nptr), then
    // registered uniform slots overlaid at their assigned ixes.
    struct umbra_buf buf[32];
    for (int i = 0; i < j->caller_nptr; i++) { buf[i] = caller_buf[i]; }
    for (int i = 0; i < j->n_reg; i++) {
        buf[j->reg[i].ix] = (struct umbra_buf){
            .ptr   = (void*)(uintptr_t)j->reg[i].ptr,
            .count = j->reg[i].slots,
        };
    }
    jit_program_run(j, l, t, r, b, buf);
}

static void dump_jit(struct umbra_program const *prog, FILE *f) {
    jit_program_dump((struct jit_program const*)prog, f);
}

static void free_jit(struct umbra_program *prog) {
    struct jit_program *j = (struct jit_program*)prog;
    struct jit_backend *be = (struct jit_backend*)prog->backend;
    release_code_buf(be, j->code, j->code_size);
    free(j->reg);
    free(j);
}

static struct umbra_program* compile_jit(struct umbra_backend           *be,
                                         struct umbra_flat_ir const *ir) {
    struct jit_program *j = jit_program((struct jit_backend*)be, ir);
    j->base = (struct umbra_program){
        .queue      = run_jit,
        .dump       = dump_jit,
        .free       = free_jit,
        .backend    = be,
        .queue_is_threadsafe = 1,
    };
    return &j->base;
}

static void flush_be_noop(struct umbra_backend *be) {
    (void)be;
}

static void free_be_jit(struct umbra_backend *be) {
    struct jit_backend *jbe = (struct jit_backend*)be;
    for (int i = 0; i < jbe->count; i++) { munmap(jbe->cache[i].mem, jbe->cache[i].size); }
    free(jbe->cache);
    free(jbe);
}

static struct umbra_backend_stats stats_zero(struct umbra_backend const *b) {
    (void)b;
    return (struct umbra_backend_stats){0};
}
struct umbra_backend* umbra_backend_jit(void) {
    struct jit_backend *be = calloc(1, sizeof *be);
    be->base = (struct umbra_backend){
        .compile = compile_jit,
        .flush   = flush_be_noop,
        .free    = free_be_jit,
        .stats   = stats_zero,
    };
    return &be->base;
}

#endif
