#define _POSIX_C_SOURCE 199309L
#include "umbra.h"
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

static uint32_t bits(float f) {
    return (union{float f; uint32_t bits;}){f}.bits;
}

static struct umbra_basic_block* build_srcover_bb(void) {
    struct umbra_basic_block *bb = umbra_basic_block();

    umbra_v32 const ix = umbra_lane(bb),
                   src = umbra_load_32(bb, (umbra_ptr){0}, ix),
                 mask8 = umbra_imm_32(bb, 0xff),
                inv255 = umbra_imm_32(bb, bits(1/255.0f)),
                    ri = umbra_and_32(bb, src, mask8),
                    rf = umbra_mul_f32(bb, umbra_f32_from_i32(bb, ri), inv255);
    umbra_half const sr = umbra_half_from_f32(bb, rf);
    umbra_v32 const sh8 = umbra_imm_32(bb, 8),
                     gi = umbra_and_32(bb, umbra_shr_u32(bb, src, sh8), mask8),
                     gf = umbra_mul_f32(bb, umbra_f32_from_i32(bb, gi), inv255);
    umbra_half const sg = umbra_half_from_f32(bb, gf);

    umbra_v32 const sh16 = umbra_imm_32(bb, 16),
                      bi = umbra_and_32(bb, umbra_shr_u32(bb, src, sh16), mask8),
                      bf = umbra_mul_f32(bb, umbra_f32_from_i32(bb, bi), inv255);
    umbra_half const sb = umbra_half_from_f32(bb, bf);

    umbra_v32 const sh24 = umbra_imm_32(bb, 24),
                      ai = umbra_and_32(bb, umbra_shr_u32(bb, src, sh24), mask8),
                      af = umbra_mul_f32(bb, umbra_f32_from_i32(bb, ai), inv255);
    umbra_half const sa = umbra_half_from_f32(bb, af),
                    dr = umbra_load_half(bb, (umbra_ptr){1}, ix),
                    dg = umbra_load_half(bb, (umbra_ptr){2}, ix),
                    db = umbra_load_half(bb, (umbra_ptr){3}, ix),
                    da = umbra_load_half(bb, (umbra_ptr){4}, ix),
                   one = umbra_imm_half(bb, 0x3c00),
                 inv_a = umbra_sub_half(bb, one, sa),
                  rout = umbra_add_half(bb, sr, umbra_mul_half(bb, dr, inv_a)),
                  gout = umbra_add_half(bb, sg, umbra_mul_half(bb, dg, inv_a)),
                  bout = umbra_add_half(bb, sb, umbra_mul_half(bb, db, inv_a)),
                  aout = umbra_add_half(bb, sa, umbra_mul_half(bb, da, inv_a));

    umbra_store_half(bb, (umbra_ptr){1}, ix, rout);
    umbra_store_half(bb, (umbra_ptr){2}, ix, gout);
    umbra_store_half(bb, (umbra_ptr){3}, ix, bout);
    umbra_store_half(bb, (umbra_ptr){4}, ix, aout);

    return bb;
}

typedef void (*run_fn)(void*, int, void*, void*, void*, void*, void*, void*);

static void run_interp(void *ctx, int n, void *p0, void *p1, void *p2, void *p3, void *p4, void *p5) {
    umbra_interpreter_run(ctx, n, p0,p1,p2,p3,p4,p5);
}
static void run_cg(void *ctx, int n, void *p0, void *p1, void *p2, void *p3, void *p4, void *p5) {
    umbra_codegen_run(ctx, n, p0,p1,p2,p3,p4,p5);
}
static void run_jit(void *ctx, int n, void *p0, void *p1, void *p2, void *p3, void *p4, void *p5) {
    umbra_jit_run(ctx, n, p0,p1,p2,p3,p4,p5);
}

static double bench_run(run_fn fn, void *ctx, int pixel_n,
                        void *p0, void *p1, void *p2, void *p3, void *p4, void *p5) {
    fn(ctx, pixel_n, p0,p1,p2,p3,p4,p5);

    int iters = 1;
    for (;;) {
        double const start = now();
        for (int i = 0; i < iters; i++) {
            fn(ctx, pixel_n, p0,p1,p2,p3,p4,p5);
        }
        double const elapsed = now() - start;
        if (elapsed >= 0.5) {
            return elapsed / ((double)iters * (double)pixel_n) * 1e9;
        }
        iters *= 2;
    }
}

static double bench_build_interp(void) {
    struct umbra_basic_block *bb = build_srcover_bb();
    umbra_basic_block_optimize(bb);
    struct umbra_interpreter *p = umbra_interpreter(bb);
    umbra_basic_block_free(bb);
    umbra_interpreter_free(p);

    int iters = 1;
    for (;;) {
        double const start = now();
        for (int i = 0; i < iters; i++) {
            bb = build_srcover_bb();
            umbra_basic_block_optimize(bb);
            p = umbra_interpreter(bb);
            umbra_basic_block_free(bb);
            umbra_interpreter_free(p);
        }
        double const elapsed = now() - start;
        if (elapsed >= 0.5) {
            return elapsed / (double)iters * 1e9;
        }
        iters *= 2;
    }
}

static double bench_build_codegen(void) {
    struct umbra_basic_block *bb = build_srcover_bb();
    umbra_basic_block_optimize(bb);
    struct umbra_codegen *cg = umbra_codegen(bb);
    umbra_basic_block_free(bb);
    if (cg) { umbra_codegen_free(cg); }

    int iters = 1;
    for (;;) {
        double const start = now();
        for (int i = 0; i < iters; i++) {
            bb = build_srcover_bb();
            umbra_basic_block_optimize(bb);
            cg = umbra_codegen(bb);
            umbra_basic_block_free(bb);
            if (cg) { umbra_codegen_free(cg); }
        }
        double const elapsed = now() - start;
        if (elapsed >= 0.5) {
            return elapsed / (double)iters * 1e9;
        }
        iters *= 2;
    }
}

static double bench_build_jit(void) {
    struct umbra_basic_block *bb = build_srcover_bb();
    umbra_basic_block_optimize(bb);
    struct umbra_jit *j = umbra_jit(bb);
    umbra_basic_block_free(bb);
    if (j) { umbra_jit_free(j); }

    int iters = 1;
    for (;;) {
        double const start = now();
        for (int i = 0; i < iters; i++) {
            bb = build_srcover_bb();
            umbra_basic_block_optimize(bb);
            j = umbra_jit(bb);
            umbra_basic_block_free(bb);
            if (j) { umbra_jit_free(j); }
        }
        double const elapsed = now() - start;
        if (elapsed >= 0.5) {
            return elapsed / (double)iters * 1e9;
        }
        iters *= 2;
    }
}

static double bench_optimize(void) {
    struct umbra_basic_block *bb = build_srcover_bb();
    umbra_basic_block_optimize(bb);
    umbra_basic_block_free(bb);

    int iters = 1;
    for (;;) {
        double const start = now();
        for (int i = 0; i < iters; i++) {
            bb = build_srcover_bb();
            umbra_basic_block_optimize(bb);
            umbra_basic_block_free(bb);
        }
        double const elapsed = now() - start;
        if (elapsed >= 0.5) {
            return elapsed / (double)iters * 1e9;
        }
        iters *= 2;
    }
}

static void fmt_ns(char buf[16], double ns) {
    if      (ns >= 1e6) sprintf(buf, "%7.1f ms", ns * 1e-6);
    else if (ns >= 1e3) sprintf(buf, "%7.1f us", ns * 1e-3);
    else                sprintf(buf, "%7.0f ns", ns);
}

int main(int argc, char *argv[]) {
    int pixel_n = 4096;
    for (int i = 1; i < argc; i++) {
        pixel_n = atoi(argv[i]);
    }

    struct umbra_basic_block *bb = build_srcover_bb();
    { FILE *f = fopen("dumps/srcover.bb", "w"); umbra_basic_block_dump(bb, f); fclose(f); }

    umbra_basic_block_optimize(bb);
    { FILE *f = fopen("dumps/srcover.opt", "w"); umbra_basic_block_dump(bb, f); fclose(f); }

    struct umbra_interpreter *p   = umbra_interpreter(bb);
    struct umbra_codegen     *cg  = umbra_codegen(bb);
    struct umbra_jit         *jit = umbra_jit(bb);
    umbra_basic_block_free(bb);

    if (cg)  { FILE *f = fopen("dumps/srcover.c",     "w"); umbra_codegen_dump(cg,  f); fclose(f); }
    if (jit) { FILE *f = fopen("dumps/srcover.arm64", "w"); umbra_jit_dump    (jit, f); fclose(f); }

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
    char build_buf[16], run_buf[16];

    printf("SrcOver 8888->fp16, %d pixels:\n", pixel_n);
    printf("             build        run\n");

    fmt_ns(build_buf, bench_optimize());
    printf("  optimize%s\n", build_buf);

    fmt_ns(build_buf, bench_build_interp());
    sprintf(run_buf, "%5.2f ns/px", bench_run(run_interp, p, pixel_n, src, dr, dg, db, da, 0));
    printf("  interp  %s  %s\n", build_buf, run_buf);

    if (cg) {
        fmt_ns(build_buf, bench_build_codegen());
        sprintf(run_buf, "%5.2f ns/px", bench_run(run_cg, cg, pixel_n, src, dr, dg, db, da, 0));
        printf("  codegen %s  %s\n", build_buf, run_buf);
    }

    if (jit) {
        fmt_ns(build_buf, bench_build_jit());
        sprintf(run_buf, "%5.2f ns/px", bench_run(run_jit, jit, pixel_n, src, dr, dg, db, da, 0));
        printf("  jit     %s  %s\n", build_buf, run_buf);
    }

    umbra_interpreter_free(p);
    if (cg) { umbra_codegen_free(cg); }
    if (jit) { umbra_jit_free(jit); }
    free(src); free(dr); free(dg); free(db); free(da);
    return 0;
}
