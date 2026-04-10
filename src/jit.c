#include "jit.h"

#if !defined(__aarch64__) && !defined(__AVX2__)

struct umbra_backend *umbra_backend_jit(void) { return 0; }

#else

#include "assume.h"
#include "ra.h"
#include <stdlib.h>
#include <sys/mman.h>

void free_chan(struct ra *ra, val operand, int i) {
    int id = operand.id, ch = (int)operand.chan;
    if (ch) {
        if (ra_chan_last_use(ra, id, ch) <= i) {
            int8_t r = ra_chan_reg(ra, id, ch);
            if (r >= 0) { ra_return_reg(ra, r); ra_set_chan_reg(ra, id, ch, -1); }
        }
    } else {
        if (ra_last_use(ra, id) <= i) { ra_free_reg(ra, id); }
    }
}

struct code_buf acquire_code_buf(struct jit_backend *be, size_t min_size) {
    for (int i = be->count; i-- > 0;) {
        if (be->cache[i].size >= min_size) {
            struct code_buf buf = be->cache[i];
            be->cache[i] = be->cache[--be->count];
            mprotect(buf.mem, buf.size, PROT_READ | PROT_WRITE);
            return buf;
        }
    }
    size_t const pg = 16384;
    size_t const alloc = (min_size + pg - 1) & ~(pg - 1);
    void *mem = mmap(NULL, alloc, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANON, -1, 0);
    assume(mem != MAP_FAILED);
    return (struct code_buf){.mem = mem, .size = alloc};
}

static void release_code_buf(struct jit_backend *be, void *mem, size_t size) {
    if (be->count == be->cap) {
        be->cap = be->cap ? 2 * be->cap : 4;
        be->cache = realloc(be->cache, (size_t)be->cap * sizeof *be->cache);
    }
    be->cache[be->count++] = (struct code_buf){.mem = mem, .size = size};
}

static void run_jit(struct umbra_program *prog,
                    int l, int t, int r, int b, struct umbra_buf buf[]) {
    umbra_jit_run((struct umbra_jit*)prog, l, t, r, b, buf);
}
static void dump_jit(struct umbra_program const *prog, FILE *f) {
    umbra_dump_jit_mca((struct umbra_jit const*)prog, f);
}
static void free_jit(struct umbra_program *prog) {
    struct umbra_jit    *j  = (struct umbra_jit*)prog;
    struct jit_backend  *be = (struct jit_backend*)prog->backend;
    release_code_buf(be, j->code, j->code_size);
    free(j);
}
static struct umbra_program *compile_jit(struct umbra_backend           *be,
                                         struct umbra_basic_block const *bb) {
    struct umbra_jit *j = umbra_jit((struct jit_backend*)be, bb);
    j->base = (struct umbra_program){
        .queue      = run_jit,
        .dump       = dump_jit,
        .free       = free_jit,
        .backend    = be,
        .threadsafe = 1,
    };
    return &j->base;
}
static void flush_be_noop(struct umbra_backend *be) { (void)be; }
static void free_be_jit(struct umbra_backend *be) {
    struct jit_backend *jbe = (struct jit_backend*)be;
    for (int i = 0; i < jbe->count; i++) { munmap(jbe->cache[i].mem, jbe->cache[i].size); }
    free(jbe->cache);
    free(jbe);
}
struct umbra_backend *umbra_backend_jit(void) {
    struct jit_backend *be = calloc(1, sizeof *be);
    be->base = (struct umbra_backend){
        .compile = compile_jit,
        .flush   = flush_be_noop,
        .free    = free_be_jit,
    };
    return &be->base;
}

#endif
