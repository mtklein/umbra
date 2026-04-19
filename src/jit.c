#include "jit.h"

// TODO: JIT subrect-dispatch + coverage bug.  Seen rendering the overview
// slide through the dump tool: the overview.hdr is wrong only on the JIT
// backend (interp, metal, vulkan, wgpu are all correct).  Minimal repro:
//
//   build a draw with fmt=fp16_planar, coverage=umbra_coverage_rect over
//   rect (160,128,480,368), shader_color, blend=srcover (or NULL).  Bind a
//   1024x768 dst buf and dispatch (0,0,170,153).
//
//   expected: 10 cols x 25 rows = 250 pixels changed.
//   interp/metal/vulkan/wgpu: 250 (correct).
//   jit (arm64 and x86): wrong.  srcover path: 50 (only the tail at cols
//   168-169 writes; the SIMD block at XCOL=160 covering cols 160-167 does
//   not land).  noblend path: 1242, with spurious writes on rows where Y
//   coverage is false (the LO lanes of the XCOL=0 SIMD iter and the HI
//   lanes of the XCOL=160 SIMD iter look "stuck" with some earlier mask).
//
// Reproduces only when dispatch width is not a multiple of K=8, i.e. only
// when the tail loop is actually needed.  Width 168 (aligned) is correct
// on JIT; 170 is wrong.  Bug reproduces across arm64 and x86 JITs with
// different symptoms, so the root cause is likely in shared code (flat_ir
// resolve/schedule, ra, or run_jit's binding setup), not backend-specific
// codegen.
//
// Disassembling the arm64 SIMD body looks correct on paper -- fcmgt/fcmge/
// and/bsl for the rect mask, fmla for the lerp, 4+4 lane stores to the
// planar dst.  Pencil-tracing says coverage should be 0 for the "bad"
// lanes, so fmla leaves dst unchanged.  Runtime disagrees.  Either the
// generated code differs from what objdump shows, or a register is
// clobbered between the bsl and the fmla in a way I missed.  Next step is
// lldb breakpoint at j->entry, single-step the SIMD iter at XCOL=0 row 1,
// and watch the coverage-mask register.
//
// Overview.prepare works around this today by rendering every cell program
// through the SIMD-aligned full-frame dispatch only when the demo drives
// it (user confirms demo looks fine); the dump tool and any driver that
// queues cell-sized subrects on JIT will render garbage until this lands.

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

static void run_jit(struct umbra_program *prog, int l, int t, int r, int b) {
    struct jit_program *j = (struct jit_program*)prog;
    struct umbra_buf buf[32];
    for (int i = 0; i < j->bindings; i++) {
        buf[j->binding[i].ix] = j->binding[i].buf ? *j->binding[i].buf : j->binding[i].uniforms;
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
    free(j->binding);
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
