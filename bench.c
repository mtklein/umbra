#define _POSIX_C_SOURCE 199309L
#include "len.h"
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

static int build_srcover(struct umbra_inst inst[]) {
    int n = 0;

    int lane_  = n; inst[n++] = (struct umbra_inst){.op=umbra_lane};
    int src_   = n; inst[n++] = (struct umbra_inst){umbra_load_32, .ptr=0, .x=lane_};

    // Extract R (bits 0..7)
    int maskr  = n; inst[n++] = (struct umbra_inst){umbra_imm_32, .immi=0xFF};
    int ri32   = n; inst[n++] = (struct umbra_inst){umbra_and_32, .x=src_, .y=maskr};
    int rf32   = n; inst[n++] = (struct umbra_inst){umbra_f32_from_i32, .x=ri32};
    int inv255r= n; inst[n++] = (struct umbra_inst){umbra_imm_32, .immf=1.0f/255.0f};
    int srf32  = n; inst[n++] = (struct umbra_inst){umbra_mul_f32, .x=rf32, .y=inv255r};
    int sr16   = n; inst[n++] = (struct umbra_inst){umbra_f16_from_f32, .x=srf32};

    // Extract G (bits 8..15)
    int sh8    = n; inst[n++] = (struct umbra_inst){umbra_imm_32, .immi=8};
    int gshr   = n; inst[n++] = (struct umbra_inst){umbra_shr_u32, .x=src_, .y=sh8};
    int maskg  = n; inst[n++] = (struct umbra_inst){umbra_imm_32, .immi=0xFF};
    int gi32   = n; inst[n++] = (struct umbra_inst){umbra_and_32, .x=gshr, .y=maskg};
    int gf32   = n; inst[n++] = (struct umbra_inst){umbra_f32_from_i32, .x=gi32};
    int inv255g= n; inst[n++] = (struct umbra_inst){umbra_imm_32, .immf=1.0f/255.0f};
    int sgf32  = n; inst[n++] = (struct umbra_inst){umbra_mul_f32, .x=gf32, .y=inv255g};
    int sg16   = n; inst[n++] = (struct umbra_inst){umbra_f16_from_f32, .x=sgf32};

    // Extract B (bits 16..23)
    int sh16   = n; inst[n++] = (struct umbra_inst){umbra_imm_32, .immi=16};
    int bshr   = n; inst[n++] = (struct umbra_inst){umbra_shr_u32, .x=src_, .y=sh16};
    int maskb  = n; inst[n++] = (struct umbra_inst){umbra_imm_32, .immi=0xFF};
    int bi32   = n; inst[n++] = (struct umbra_inst){umbra_and_32, .x=bshr, .y=maskb};
    int bf32   = n; inst[n++] = (struct umbra_inst){umbra_f32_from_i32, .x=bi32};
    int inv255b= n; inst[n++] = (struct umbra_inst){umbra_imm_32, .immf=1.0f/255.0f};
    int sbf32  = n; inst[n++] = (struct umbra_inst){umbra_mul_f32, .x=bf32, .y=inv255b};
    int sb16   = n; inst[n++] = (struct umbra_inst){umbra_f16_from_f32, .x=sbf32};

    // Extract A (bits 24..31)
    int sh24   = n; inst[n++] = (struct umbra_inst){umbra_imm_32, .immi=24};
    int ashr_  = n; inst[n++] = (struct umbra_inst){umbra_shr_u32, .x=src_, .y=sh24};
    int maska  = n; inst[n++] = (struct umbra_inst){umbra_imm_32, .immi=0xFF};
    int ai32   = n; inst[n++] = (struct umbra_inst){umbra_and_32, .x=ashr_, .y=maska};
    int af32   = n; inst[n++] = (struct umbra_inst){umbra_f32_from_i32, .x=ai32};
    int inv255a= n; inst[n++] = (struct umbra_inst){umbra_imm_32, .immf=1.0f/255.0f};
    int saf32  = n; inst[n++] = (struct umbra_inst){umbra_mul_f32, .x=af32, .y=inv255a};
    int sa16   = n; inst[n++] = (struct umbra_inst){umbra_f16_from_f32, .x=saf32};

    // Load dst fp16 channels
    int dr16   = n; inst[n++] = (struct umbra_inst){umbra_load_16, .ptr=1, .x=lane_};
    int dg16   = n; inst[n++] = (struct umbra_inst){umbra_load_16, .ptr=2, .x=lane_};
    int db16   = n; inst[n++] = (struct umbra_inst){umbra_load_16, .ptr=3, .x=lane_};
    int da16   = n; inst[n++] = (struct umbra_inst){umbra_load_16, .ptr=4, .x=lane_};

    // Blend: out = src + dst * (1 - src_alpha), all in fp16
    // inv_alpha = 1 - sa
    int one_r  = n; inst[n++] = (struct umbra_inst){umbra_imm_16, .immi=0x3C00};  // 1.0 in fp16
    int inva_r = n; inst[n++] = (struct umbra_inst){umbra_sub_f16, .x=one_r, .y=sa16};
    int drmul  = n; inst[n++] = (struct umbra_inst){umbra_mul_f16, .x=dr16, .y=inva_r};
    int rout   = n; inst[n++] = (struct umbra_inst){umbra_add_f16, .x=sr16, .y=drmul};

    int one_g  = n; inst[n++] = (struct umbra_inst){umbra_imm_16, .immi=0x3C00};
    int inva_g = n; inst[n++] = (struct umbra_inst){umbra_sub_f16, .x=one_g, .y=sa16};
    int dgmul  = n; inst[n++] = (struct umbra_inst){umbra_mul_f16, .x=dg16, .y=inva_g};
    int gout   = n; inst[n++] = (struct umbra_inst){umbra_add_f16, .x=sg16, .y=dgmul};

    int one_b  = n; inst[n++] = (struct umbra_inst){umbra_imm_16, .immi=0x3C00};
    int inva_b = n; inst[n++] = (struct umbra_inst){umbra_sub_f16, .x=one_b, .y=sa16};
    int dbmul  = n; inst[n++] = (struct umbra_inst){umbra_mul_f16, .x=db16, .y=inva_b};
    int bout   = n; inst[n++] = (struct umbra_inst){umbra_add_f16, .x=sb16, .y=dbmul};

    int one_a  = n; inst[n++] = (struct umbra_inst){umbra_imm_16, .immi=0x3C00};
    int inva_a = n; inst[n++] = (struct umbra_inst){umbra_sub_f16, .x=one_a, .y=sa16};
    int damul  = n; inst[n++] = (struct umbra_inst){umbra_mul_f16, .x=da16, .y=inva_a};
    int aout   = n; inst[n++] = (struct umbra_inst){umbra_add_f16, .x=sa16, .y=damul};

    // Store results
    inst[n++] = (struct umbra_inst){umbra_store_16, .ptr=1, .x=lane_, .y=rout};
    inst[n++] = (struct umbra_inst){umbra_store_16, .ptr=2, .x=lane_, .y=gout};
    inst[n++] = (struct umbra_inst){umbra_store_16, .ptr=3, .x=lane_, .y=bout};
    inst[n++] = (struct umbra_inst){umbra_store_16, .ptr=4, .x=lane_, .y=aout};

    (void)sr16; (void)sg16; (void)sb16; (void)sa16;
    (void)srf32; (void)sgf32; (void)sbf32; (void)saf32;

    return n;
}

static double bench(struct umbra_program const *p, int pixel_n, void *ptrs[]) {
    umbra_program_run(p, pixel_n, ptrs);

    int iters = 1;
    for (;;) {
        double start = now();
        for (int i = 0; i < iters; i++) {
            umbra_program_run(p, pixel_n, ptrs);
        }
        double elapsed = now() - start;
        if (elapsed >= 0.5) {
            return elapsed / ((double)iters * (double)pixel_n) * 1e9;
        }
        iters *= 2;
    }
}

int main(int argc, char *argv[]) {
    int pixel_n = 4096;
    if (argc > 1) {
        pixel_n = atoi(argv[1]);
    }

    struct umbra_inst inst[128];
    int insts = build_srcover(inst);

    // Unoptimized
    struct umbra_inst unopt[128];
    memcpy(unopt, inst, sizeof inst);
    struct umbra_program *p_unopt = umbra_program(unopt, insts);

    // Optimized
    int opt_n = umbra_optimize(inst, insts);
    struct umbra_program *p_opt = umbra_program(inst, opt_n);

    // Allocate pixel buffers
    uint32_t *src = malloc((size_t)pixel_n * sizeof *src);
    __fp16   *dr  = malloc((size_t)pixel_n * sizeof *dr);
    __fp16   *dg  = malloc((size_t)pixel_n * sizeof *dg);
    __fp16   *db  = malloc((size_t)pixel_n * sizeof *db);
    __fp16   *da  = malloc((size_t)pixel_n * sizeof *da);
    for (int i = 0; i < pixel_n; i++) {
        src[i] = 0x80402010u;  // RGBA(16,32,64,128) ~ 50% alpha
        dr[i] = (__fp16)0.5f;
        dg[i] = (__fp16)0.5f;
        db[i] = (__fp16)0.5f;
        da[i] = (__fp16)0.5f;
    }

    void *ptrs[] = {src, dr, dg, db, da};

    double unopt_ns = bench(p_unopt, pixel_n, ptrs);

    // Reset dst
    for (int i = 0; i < pixel_n; i++) {
        dr[i] = (__fp16)0.5f;
        dg[i] = (__fp16)0.5f;
        db[i] = (__fp16)0.5f;
        da[i] = (__fp16)0.5f;
    }

    double opt_ns = bench(p_opt, pixel_n, ptrs);

    printf("SrcOver 8888→fp16, %d pixels\n", pixel_n);
    printf("  unoptimized: %d insts, %.2f ns/pixel\n", insts, unopt_ns);
    printf("  optimized:   %d insts, %.2f ns/pixel\n", opt_n, opt_ns);
    printf("  speedup:     %.2fx\n", unopt_ns / opt_ns);

    umbra_program_free(p_unopt);
    umbra_program_free(p_opt);
    free(src); free(dr); free(dg); free(db); free(da);
    return 0;
}
