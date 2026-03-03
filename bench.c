#define _POSIX_C_SOURCE 199309L
#include "umbra.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static uint32_t bits(float f) {
    return (union{float f; uint32_t bits;}){f}.bits;
}

static struct umbra_interpreter* build_srcover(void) {
    struct umbra_basic_block *bb = umbra_basic_block();

    umbra_v32 const ix = umbra_lane(bb),
                   src = umbra_load_32(bb, (umbra_ptr){0}, ix),
                 mask8 = umbra_imm_32(bb, 0xff),
                inv255 = umbra_imm_32(bb, bits(1/255.0f)),
                    ri = umbra_and_32(bb, src, mask8),
                    rf = umbra_mul_f32(bb, umbra_f32_from_i32(bb, ri), inv255);
    umbra_v16 const sr = umbra_f16_from_f32(bb, rf);
    umbra_v32 const sh8 = umbra_imm_32(bb, 8),
                     gi = umbra_and_32(bb, umbra_shr_u32(bb, src, sh8), mask8),
                     gf = umbra_mul_f32(bb, umbra_f32_from_i32(bb, gi), inv255);
    umbra_v16 const sg = umbra_f16_from_f32(bb, gf);

    umbra_v32 const sh16 = umbra_imm_32(bb, 16),
                      bi = umbra_and_32(bb, umbra_shr_u32(bb, src, sh16), mask8),
                      bf = umbra_mul_f32(bb, umbra_f32_from_i32(bb, bi), inv255);
    umbra_v16 const sb = umbra_f16_from_f32(bb, bf);

    umbra_v32 const sh24 = umbra_imm_32(bb, 24),
                      ai = umbra_and_32(bb, umbra_shr_u32(bb, src, sh24), mask8),
                      af = umbra_mul_f32(bb, umbra_f32_from_i32(bb, ai), inv255);
    umbra_v16 const sa = umbra_f16_from_f32(bb, af),
                    dr = umbra_load_16(bb, (umbra_ptr){1}, ix),
                    dg = umbra_load_16(bb, (umbra_ptr){2}, ix),
                    db = umbra_load_16(bb, (umbra_ptr){3}, ix),
                    da = umbra_load_16(bb, (umbra_ptr){4}, ix),
                   one = umbra_imm_16(bb, 0x3c00),
                 inv_a = umbra_sub_f16(bb, one, sa),
                  rout = umbra_add_f16(bb, sr, umbra_mul_f16(bb, dr, inv_a)),
                  gout = umbra_add_f16(bb, sg, umbra_mul_f16(bb, dg, inv_a)),
                  bout = umbra_add_f16(bb, sb, umbra_mul_f16(bb, db, inv_a)),
                  aout = umbra_add_f16(bb, sa, umbra_mul_f16(bb, da, inv_a));

    umbra_store_16(bb, (umbra_ptr){1}, ix, rout);
    umbra_store_16(bb, (umbra_ptr){2}, ix, gout);
    umbra_store_16(bb, (umbra_ptr){3}, ix, bout);
    umbra_store_16(bb, (umbra_ptr){4}, ix, aout);

    struct umbra_interpreter *p = umbra_interpreter(bb);
    umbra_basic_block_free(bb);
    return p;
}

static double bench(struct umbra_interpreter *p, int pixel_n, void *ptrs[]) {
    umbra_interpreter_run(p, pixel_n, ptrs);

    int iters = 1;
    for (;;) {
        double const start = now();
        for (int i = 0; i < iters; i++) {
            umbra_interpreter_run(p, pixel_n, ptrs);
        }
        double const elapsed = now() - start;
        if (elapsed >= 0.5) {
            return elapsed / ((double)iters * (double)pixel_n) * 1e9;
        }
        iters *= 2;
    }
}

static double bench_build(void) {
    struct umbra_interpreter *p = build_srcover();
    umbra_interpreter_free(p);

    int iters = 1;
    for (;;) {
        double const start = now();
        for (int i = 0; i < iters; i++) {
            p = build_srcover();
            umbra_interpreter_free(p);
        }
        double const elapsed = now() - start;
        if (elapsed >= 0.5) {
            return elapsed / (double)iters * 1e9;
        }
        iters *= 2;
    }
}

int main(int argc, char *argv[]) {
    int pixel_n = 4096;
    if (argc > 1) {
        pixel_n = atoi(argv[1]);
    }

    struct umbra_interpreter *p = build_srcover();

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

    void *ptrs[] = {src, dr, dg, db, da};
    double const build_ns = bench_build();
    double const ns = bench(p, pixel_n, ptrs);

    printf("SrcOver 8888->fp16: %.0f ns/build, %.2f ns/pixel (%d pixels)\n",
           build_ns, ns, pixel_n);

    umbra_interpreter_free(p);
    free(src); free(dr); free(dg); free(db); free(da);
    return 0;
}
