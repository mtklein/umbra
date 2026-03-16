#define _POSIX_C_SOURCE 200809L
#include "srcover.h"
#include "umbra_draw.h"
#if !defined(__wasm__)
#include <dlfcn.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static int cmp_double(void const *a, void const *b) {
    double const x = *(double const*)a, y = *(double const*)b;
    return (x > y) - (x < y);
}

typedef void (*run_fn)(void*, int, umbra_buf[]);

static void run_interp(void *ctx, int n, umbra_buf buf[]) {
    umbra_interpreter_run(ctx, n, buf);
}
static void run_cg(void *ctx, int n, umbra_buf buf[]) {
    umbra_codegen_run(ctx, n, buf);
}
static void run_jit(void *ctx, int n, umbra_buf buf[]) {
    umbra_jit_run(ctx, n, buf);
}
static void run_mtl(void *ctx, int n, umbra_buf buf[]) {
    umbra_metal_run(ctx, n, buf);
}

// --- incbin/dlopen mode ---
// Wraps JIT machine code in a .dylib with a named symbol for profiler visibility.
#if !defined(__wasm__)

typedef void (*jit_entry_fn)(int, umbra_buf*);

static jit_entry_fn incbin_load(struct umbra_jit *jit, const char *sym) {
    char bin_path[128], s_path[128], dylib_path[128], cmd[512];
    snprintf(bin_path,   sizeof bin_path,   "/tmp/umbra_%s.bin",   sym);
    snprintf(s_path,     sizeof s_path,     "/tmp/umbra_%s.S",     sym);
    snprintf(dylib_path, sizeof dylib_path, "/tmp/umbra_%s.dylib", sym);

    FILE *f = fopen(bin_path, "wb");
    if (!f) return NULL;
    umbra_jit_dump_bin(jit, f);
    fclose(f);

    f = fopen(s_path, "w");
    if (!f) return NULL;
    fprintf(f, ".text\n.globl _%s\n.p2align 4\n_%s:\n.incbin \"%s\"\n", sym, sym, bin_path);
    fclose(f);

    snprintf(cmd, sizeof cmd, "cc -shared -o %s %s 2>/dev/null", dylib_path, s_path);
    if (system(cmd) != 0) return NULL;

    void *handle = dlopen(dylib_path, RTLD_LAZY);
    if (!handle) return NULL;
    return (jit_entry_fn)dlsym(handle, sym);
}

static jit_entry_fn incbin_fn_ctx;
static void run_incbin(void *ctx, int n, umbra_buf buf[]) {
    (void)ctx;
    incbin_fn_ctx(n, buf);
}

#endif // !__wasm__

// --- bench helpers ---

static double bench_run(run_fn fn, void *ctx, int pixel_n, umbra_buf buf[]) {
    fn(ctx, pixel_n, buf);

    int iters = 1;
    for (;;) {
        double const start = now();
        for (int i = 0; i < iters; i++) {
            fn(ctx, pixel_n, buf);
        }
        double const elapsed = now() - start;
        if (elapsed >= 0.1) {
            return elapsed / ((double)iters * (double)pixel_n) * 1e9;
        }
        iters *= 2;
    }
}

typedef struct umbra_basic_block* (*build_fn)(void);

static double bench_build(build_fn build, void (*compile_and_free)(struct umbra_basic_block*)) {
    // Single-shot: build + optimize + compile + free, timed once.
    double const start = now();
    struct umbra_basic_block *bb = build();
    umbra_basic_block_optimize(bb);
    compile_and_free(bb);
    return (now() - start) * 1e9;
}

static void build_optimize_only(struct umbra_basic_block *bb) {
    umbra_basic_block_free(bb);
}
static void build_interp(struct umbra_basic_block *bb) {
    struct umbra_interpreter *p = umbra_interpreter(bb);
    umbra_basic_block_free(bb);
    umbra_interpreter_free(p);
}
static void build_codegen(struct umbra_basic_block *bb) {
    struct umbra_codegen *cg = umbra_codegen(bb);
    umbra_basic_block_free(bb);
    if (cg) { umbra_codegen_free(cg); }
}
static void build_jit(struct umbra_basic_block *bb) {
    struct umbra_jit *j = umbra_jit(bb);
    umbra_basic_block_free(bb);
    if (j) { umbra_jit_free(j); }
}
static void build_metal(struct umbra_basic_block *bb) {
    struct umbra_metal *m = umbra_metal(bb);
    umbra_basic_block_free(bb);
    if (m) { umbra_metal_free(m); }
}

static void fmt_ns(char buf[16], double ns) {
    if      (ns >= 1e6) sprintf(buf, "%7.1f ms", ns * 1e-6);
    else if (ns >= 1e3) sprintf(buf, "%7.1f us", ns * 1e-3);
    else                sprintf(buf, "%7.0f ns", ns);
}

int main(int argc, char *argv[]) {
    int pixel_n = 4096;
    int samples = 1;
    int incbin  = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 's') {
            samples = atoi(argv[i]+2);
            if (samples < 1) samples = 1;
#if !defined(__wasm__)
        } else if (strcmp(argv[i], "-incbin") == 0) {
            incbin = 1;
#endif
        } else {
            pixel_n = atoi(argv[i]);
        }
    }

    struct umbra_basic_block *bb = build_srcover();
    umbra_basic_block_optimize(bb);

    struct umbra_interpreter *p   = umbra_interpreter(bb);
    struct umbra_codegen     *cg  = umbra_codegen(bb);
    struct umbra_jit         *jit = umbra_jit(bb);
    struct umbra_metal       *mtl = umbra_metal(bb);
    umbra_basic_block_free(bb);

#if !defined(__wasm__)
    jit_entry_fn srcover_incbin = NULL;
    if (incbin && jit) {
        srcover_incbin = incbin_load(jit, "srcover");
        if (!srcover_incbin) fprintf(stderr, "warning: incbin load failed for srcover\n");
    }
#endif

    uint32_t *src = malloc((size_t)pixel_n * sizeof *src);
    __fp16   *dr  = malloc((size_t)pixel_n * sizeof *dr);
    __fp16   *dg  = malloc((size_t)pixel_n * sizeof *dg);
    __fp16   *db  = malloc((size_t)pixel_n * sizeof *db);
    __fp16   *da  = malloc((size_t)pixel_n * sizeof *da);
    for (int i = 0; i < pixel_n; i++) {
        src[i] = 0x80402010u;
        dr[i] = (__fp16)0.5f;
        dg[i] = (__fp16)0.5f;
        db[i] = (__fp16)0.5f;
        da[i] = (__fp16)0.5f;
    }

    umbra_buf buf[] = {
        {src, pixel_n*4},
        {dr,  pixel_n*2},
        {dg,  pixel_n*2},
        {db,  pixel_n*2},
        {da,  pixel_n*2},
    };

    printf("SrcOver 8888->fp16, %d pixels", pixel_n);
    if (samples > 1) printf(", %d samples", samples);
    if (incbin) printf(", incbin mode");
    printf(":\n");

    if (samples <= 1) {
        char build_buf[16], run_buf[16];
        printf("             build        run\n");

        fmt_ns(build_buf, bench_build(build_srcover, build_optimize_only));
        printf("  optimize%s\n", build_buf);

        fmt_ns(build_buf, bench_build(build_srcover, build_interp));
        sprintf(run_buf, "%5.2f ns/px", bench_run(run_interp, p, pixel_n, buf));
        printf("  interp  %s  %s\n", build_buf, run_buf);

        if (cg) {
            fmt_ns(build_buf, bench_build(build_srcover, build_codegen));
            sprintf(run_buf, "%5.2f ns/px", bench_run(run_cg, cg, pixel_n, buf));
            printf("  codegen %s  %s\n", build_buf, run_buf);
        }

        if (jit) {
            fmt_ns(build_buf, bench_build(build_srcover, build_jit));
            sprintf(run_buf, "%5.2f ns/px", bench_run(run_jit, jit, pixel_n, buf));
            printf("  jit     %s  %s\n", build_buf, run_buf);
        }

#if !defined(__wasm__)
        if (srcover_incbin) {
            incbin_fn_ctx = srcover_incbin;
            sprintf(run_buf, "%5.2f ns/px", bench_run(run_incbin, NULL, pixel_n, buf));
            printf("  incbin             %s\n", run_buf);
        }
#endif

        if (mtl) {
            fmt_ns(build_buf, bench_build(build_srcover, build_metal));
            sprintf(run_buf, "%5.2f ns/px", bench_run(run_mtl, mtl, pixel_n, buf));
            printf("  metal   %s  %s\n", build_buf, run_buf);
        }
    } else {
        double *s = malloc((size_t)samples * sizeof *s);

        printf("             min     median     max\n");

        for (int i = 0; i < samples; i++) s[i] = bench_run(run_interp, p, pixel_n, buf);
        qsort(s, (size_t)samples, sizeof *s, cmp_double);
        printf("  interp    %5.2f     %5.2f    %5.2f  ns/px\n",
               s[0], s[samples/2], s[samples-1]);

        if (cg) {
            for (int i = 0; i < samples; i++) s[i] = bench_run(run_cg, cg, pixel_n, buf);
            qsort(s, (size_t)samples, sizeof *s, cmp_double);
            printf("  codegen   %5.2f     %5.2f    %5.2f  ns/px\n",
                   s[0], s[samples/2], s[samples-1]);
        }

        if (jit) {
            for (int i = 0; i < samples; i++) s[i] = bench_run(run_jit, jit, pixel_n, buf);
            qsort(s, (size_t)samples, sizeof *s, cmp_double);
            printf("  jit       %5.2f     %5.2f    %5.2f  ns/px\n",
                   s[0], s[samples/2], s[samples-1]);
        }

#if !defined(__wasm__)
        if (srcover_incbin) {
            incbin_fn_ctx = srcover_incbin;
            for (int i = 0; i < samples; i++) s[i] = bench_run(run_incbin, NULL, pixel_n, buf);
            qsort(s, (size_t)samples, sizeof *s, cmp_double);
            printf("  incbin    %5.2f     %5.2f    %5.2f  ns/px\n",
                   s[0], s[samples/2], s[samples-1]);
        }
#endif

        if (mtl) {
            for (int i = 0; i < samples; i++) s[i] = bench_run(run_mtl, mtl, pixel_n, buf);
            qsort(s, (size_t)samples, sizeof *s, cmp_double);
            printf("  metal     %5.2f     %5.2f    %5.2f  ns/px\n",
                   s[0], s[samples/2], s[samples-1]);
        }

        free(s);
    }

    umbra_interpreter_free(p);
    if (cg)  { umbra_codegen_free(cg); }
    if (jit) { umbra_jit_free(jit); }
    if (mtl) { umbra_metal_free(mtl); }
    free(src); free(dr); free(dg); free(db); free(da);

    // --- Draw API benchmarks ---
    typedef struct {
        const char              *name;
        const char              *sym;
        umbra_shader_fn          shader;
        umbra_coverage_fn        coverage;
        umbra_blend_fn           blend;
        umbra_load_fn            load;
        umbra_store_fn           store;
    } draw_config;
    draw_config draws[] = {
        {"solid src 8888",     "solid_src_8888",     umbra_shader_solid, NULL, umbra_blend_src,     umbra_load_8888, umbra_store_8888},
        {"solid srcover 8888", "solid_srcover_8888", umbra_shader_solid, NULL, umbra_blend_srcover, umbra_load_8888, umbra_store_8888},
        {"solid src fp16",     "solid_src_fp16",     umbra_shader_solid, NULL, umbra_blend_src,     umbra_load_fp16, umbra_store_fp16},
        {"solid srcover fp16", "solid_srcover_fp16", umbra_shader_solid, NULL, umbra_blend_srcover, umbra_load_fp16, umbra_store_fp16},
    };

    for (int di = 0; di < (int)(sizeof draws / sizeof draws[0]); di++) {
        draw_config *d = &draws[di];
        umbra_draw_layout dlay;
        struct umbra_basic_block *dbb = umbra_draw_build(
            d->shader, d->coverage, d->blend, d->load, d->store, &dlay);
        umbra_basic_block_optimize(dbb);

        struct umbra_interpreter *dp   = umbra_interpreter(dbb);
        struct umbra_codegen     *dcg  = umbra_codegen(dbb);
        struct umbra_jit         *djit = umbra_jit(dbb);
        struct umbra_metal       *dmtl = umbra_metal(dbb);
        umbra_basic_block_free(dbb);

#if !defined(__wasm__)
        jit_entry_fn draw_incbin_fn = NULL;
        if (incbin && djit) {
            draw_incbin_fn = incbin_load(djit, d->sym);
            if (!draw_incbin_fn) fprintf(stderr, "warning: incbin load failed for %s\n", d->sym);
        }
#endif

        // Allocate pixel buffers -- 8888 needs 4 bytes/px, fp16 needs 8 bytes/px.
        int px_bytes = (d->load == umbra_load_fp16) ? 8 : 4;
        void *dst_px = malloc((size_t)pixel_n * (size_t)px_bytes);
        memset(dst_px, 0x80, (size_t)pixel_n * (size_t)px_bytes);

        float color[4] = {0.25f, 0.5f, 0.125f, 0.5f};
        long long uni_[3] = {0};
        char *uni = (char*)uni_;
        __builtin_memcpy(uni + dlay.shader, color, 16);

        umbra_buf dbuf[] = {
            {dst_px, pixel_n * px_bytes},
            {uni,   -(long)dlay.uni_len},
        };

        printf("\nDraw %s, %d pixels:\n", d->name, pixel_n);
        printf("             run\n");

        char rbuf[16];
        sprintf(rbuf, "%5.2f ns/px", bench_run(run_interp, dp, pixel_n, dbuf));
        printf("  interp    %s\n", rbuf);

        if (dcg) {
            sprintf(rbuf, "%5.2f ns/px", bench_run(run_cg, dcg, pixel_n, dbuf));
            printf("  codegen   %s\n", rbuf);
        }
        if (djit) {
            sprintf(rbuf, "%5.2f ns/px", bench_run(run_jit, djit, pixel_n, dbuf));
            printf("  jit       %s\n", rbuf);
        }
#if !defined(__wasm__)
        if (draw_incbin_fn) {
            incbin_fn_ctx = draw_incbin_fn;
            sprintf(rbuf, "%5.2f ns/px", bench_run(run_incbin, NULL, pixel_n, dbuf));
            printf("  incbin    %s\n", rbuf);
        }
#endif
        if (dmtl) {
            sprintf(rbuf, "%5.2f ns/px", bench_run(run_mtl, dmtl, pixel_n, dbuf));
            printf("  metal     %s\n", rbuf);
        }

        umbra_interpreter_free(dp);
        if (dcg)  { umbra_codegen_free(dcg); }
        if (djit) { umbra_jit_free(djit); }
        if (dmtl) { umbra_metal_free(dmtl); }
        free(dst_px);
    }

    return 0;
}
