#define _POSIX_C_SOURCE 200809L
#include "../slides/slide.h"
#include "../slides/slug.h"
#include "../src/program.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static double bench(slide *s, int w, int h, umbra_draw_layout const *lay,
                    void *buf, long buf_sz, int rs, struct umbra_backend *be,
                    struct umbra_program *prog) {
    s->render(s, w, h, buf, buf_sz, rs, lay, prog);
    umbra_backend_flush(be);
    int iters = 1;
    for (;;) {
        double const start = now();
        for (int it = 0; it < iters; it++) {
            s->render(s, w, h, buf, buf_sz, rs, lay, prog);
        }
        umbra_backend_flush(be);
        double const elapsed = now() - start;
        if (elapsed >= 0.1) { return elapsed / ((double)iters * (double)w * (double)h) * 1e9; }
        iters *= 2;
    }
}

int main(int argc, char *argv[]) {
    int W = 4096;
    for (int i = 1; i < argc; i++) { W = atoi(argv[i]); }
    int const H = 480;
    slides_init(W, H);

    int ns = slide_count() - 1;
    printf("%-40s %12s %12s %12s\n", "", "interp", "jit", "metal");

    for (int si = 0; si < ns; si++) {
        slide *s = slide_get(si);
        if (!s->render) { continue; }

        umbra_draw_layout         lay;
        struct umbra_builder     *bld = umbra_draw_build(s->shader, s->coverage, s->blend,
                                                         s->load, s->store, &lay);
        struct umbra_basic_block *bb = umbra_basic_block(bld);
        umbra_builder_free(bld);

        struct umbra_backend *be_i = umbra_backend_interp();
        struct umbra_backend *be_j = umbra_backend_jit();
        struct umbra_backend *be_m = umbra_backend_metal();
        struct umbra_program *interp = umbra_backend_compile(be_i, bb);
        struct umbra_program *jit = be_j ? umbra_backend_compile(be_j, bb) : NULL;
        struct umbra_program *mtl = be_m ? umbra_backend_compile(be_m, bb) : NULL;
        umbra_basic_block_free(bb);

        _Bool planar = s->store == umbra_store_fp16_planar;
        int   bpp = planar ? 2 : 4;
        int   rs = W;
        long  buf_sz = (long)W * H * bpp;
        void *buf = calloc((size_t)(W * H), (size_t)bpp);

        char tmp[32];
        printf("%-40s", s->title);

        sprintf(tmp, "%5.2f ns/px",
                bench(s, W, H, &lay, buf, buf_sz, rs, be_i, interp));
        printf(" %12s", tmp);

        if (jit) {
            sprintf(tmp, "%5.2f ns/px",
                    bench(s, W, H, &lay, buf, buf_sz, rs, be_j, jit));
            printf(" %12s", tmp);
        } else {
            printf(" %12s", "-");
        }

        if (mtl) {
            sprintf(tmp, "%5.2f ns/px",
                    bench(s, W, H, &lay, buf, buf_sz, rs, be_m, mtl));
            printf(" %12s", tmp);
        } else {
            printf(" %12s", "-");
        }

        printf("\n");

        umbra_program_free(interp);
        umbra_program_free(jit);
        umbra_program_free(mtl);
        umbra_backend_free(be_i);
        umbra_backend_free(be_j);
        umbra_backend_free(be_m);
        free(buf);
    }

    {
        slug_curves               sc = slug_extract("Slug", (float)H * 0.3125f);
        slug_acc_layout           al;
        struct umbra_builder     *bld = slug_build_acc(&al);
        struct umbra_basic_block *bb = umbra_basic_block(bld);
        umbra_builder_free(bld);

        struct umbra_backend *be_i = umbra_backend_interp();
        struct umbra_backend *be_j = umbra_backend_jit();
        struct umbra_backend *be_m = umbra_backend_metal();
        struct umbra_program *interp = umbra_backend_compile(be_i, bb);
        struct umbra_program *jit = be_j ? umbra_backend_compile(be_j, bb) : NULL;
        struct umbra_program *mtl = be_m ? umbra_backend_compile(be_m, bb) : NULL;
        umbra_basic_block_free(bb);

        float *wind = calloc((size_t)W, 4);
        float  mat[11] = {0};
        slide_perspective_matrix(mat, 0.0f, W, H, (int)sc.w, (int)sc.h);
        mat[9] = sc.w;
        mat[10] = sc.h;
        long long au_[12] = {0};
        char     *au = (char *)au_;
        slide_uni_i32(au, al.x0, 0);
        slide_uni_i32(au, al.y, H / 2);
        slide_uni_f32(au, al.mat, mat, 11);
        slide_uni_ptr(au, al.curves_off, sc.data, (long)(sc.count * 6 * 4));
        int32_t j0 = 0;
        __builtin_memcpy(au + al.loop_off, &j0, 4);
        umbra_buf abuf[] = {
            {wind, (long)(W * 4)},
            {au, -(long)al.uni_len},
        };

        printf("\n%-40s %12s %12s %12s\n", "slug accumulator (1 curve)", "interp", "jit",
               "metal");
        printf("%-40s", "");

        struct umbra_backend *backs[] = {be_i, be_j, be_m};
        struct umbra_program *progs[] = {interp, jit, mtl};
        for (int bi = 0; bi < 3; bi++) {
            if (!progs[bi]) {
                printf(" %12s", "-");
                continue;
            }
            umbra_program_queue(progs[bi], W, 1, abuf);
            umbra_backend_flush(backs[bi]);
            int iters = 1;
            for (;;) {
                double const start = now();
                for (int it = 0; it < iters; it++) {
                    umbra_program_queue(progs[bi], W, 1, abuf);
                }
                umbra_backend_flush(backs[bi]);
                double const elapsed = now() - start;
                if (elapsed >= 0.1) {
                    char tmp[32];
                    sprintf(tmp, "%5.2f ns/px",
                            elapsed / ((double)iters * (double)W) * 1e9);
                    printf(" %12s", tmp);
                    break;
                }
                iters *= 2;
            }
        }
        printf("\n");

        umbra_program_free(interp);
        umbra_program_free(jit);
        umbra_program_free(mtl);
        umbra_backend_free(be_i);
        umbra_backend_free(be_j);
        umbra_backend_free(be_m);
        free(wind);
        slug_free(&sc);
    }

    slides_cleanup();
    return 0;
}
