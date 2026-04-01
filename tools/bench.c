#define _POSIX_C_SOURCE 200809L
#include "../slides/slide.h"
#include "../slides/slug.h"
#include "../include/umbra.h"
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
                    void *buf, struct umbra_backend *be,
                    struct umbra_program *prog) {
    if (s->prepare) { s->prepare(s, w, h, be); }
    s->draw(s, w, h, 0, h, buf, lay, prog);
    be->flush(be);
    int iters = 1;
    for (;;) {
        if (s->prepare) { s->prepare(s, w, h, be); }
        double const start = now();
        for (int it = 0; it < iters; it++) {
            s->draw(s, w, h, 0, h, buf, lay, prog);
        }
        be->flush(be);
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
    printf("%-40s %12s %12s %12s %12s\n", "", "interp", "jit", "metal", "vulkan");

    for (int si = 0; si < ns; si++) {
        slide *s = slide_get(si);
        if (!s->draw) { continue; }

        umbra_draw_layout         lay;
        struct umbra_builder     *bld = umbra_draw_build(s->shader, s->coverage, s->blend, s->fmt,
                                                         &lay);
        struct umbra_basic_block *bb = umbra_basic_block(bld);
        umbra_builder_free(bld);

        struct umbra_backend *bes[] = {
            umbra_backend_interp(), umbra_backend_jit(),
            umbra_backend_metal(),  umbra_backend_vulkan(),
        };
        struct umbra_program *progs[4];
        for (int bi = 0; bi < 4; bi++) {
            progs[bi] = bes[bi] ? bes[bi]->compile(bes[bi], bb) : NULL;
        }
        umbra_basic_block_free(bb);

        size_t bpp = umbra_fmt_size(s->fmt);
        void *buf = calloc((size_t)(W * H), bpp);

        printf("%-40s", s->title);
        for (int bi = 0; bi < 4; bi++) {
            if (progs[bi]) {
                char tmp[32];
                sprintf(tmp, "%5.2f ns/px",
                        bench(s, W, H, &lay, buf, bes[bi], progs[bi]));
                printf(" %12s", tmp);
            } else {
                printf(" %12s", "-");
            }
        }
        printf("\n");

        for (int bi = 0; bi < 4; bi++) {
            if (progs[bi]) { progs[bi]->free(progs[bi]); }
            if (bes[bi])   { bes[bi]->free(bes[bi]); }
        }
        free(buf);
        umbra_uniforms_free(lay.uni);
    }

    {
        slug_curves               sc = slug_extract("Slug", (float)H * 0.3125f);
        slug_acc_layout           al;
        struct umbra_builder     *bld = slug_build_acc(&al);
        struct umbra_basic_block *bb = umbra_basic_block(bld);
        umbra_builder_free(bld);

        struct umbra_backend *bes[] = {
            umbra_backend_interp(), umbra_backend_jit(),
            umbra_backend_metal(),  umbra_backend_vulkan(),
        };
        struct umbra_program *progs[4];
        for (int bi = 0; bi < 4; bi++) {
            progs[bi] = bes[bi] ? bes[bi]->compile(bes[bi], bb) : NULL;
        }
        umbra_basic_block_free(bb);

        float *wind = calloc((size_t)(W * H), 4);
        float  mat[11] = {0};
        slide_perspective_matrix(mat, 0.0f, W, H, (int)sc.w, (int)sc.h);
        mat[9] = sc.w;
        mat[10] = sc.h;
        umbra_set_f32(al.uni, (umbra_uniform){al.mat}, mat, 11);
        umbra_set_ptr(al.uni, (umbra_uniform){al.curves_off},
                      sc.data, (size_t)(sc.count * 6 * 4), 0, 0);
        float j0;
        { int32_t z = 0; __builtin_memcpy(&j0, &z, 4); }
        umbra_set_f32(al.uni, (umbra_uniform){al.loop_off}, &j0, 1);
        umbra_buf abuf[] = {
            (umbra_buf){.ptr=umbra_uniforms_data(al.uni), .sz=umbra_uniforms_size(al.uni), .read_only=1},
            {.ptr=wind, .sz=(size_t)(W * H * 4)},
        };

        printf("\n%-40s %12s %12s %12s %12s\n", "slug accumulator (1 curve)", "interp",
               "jit", "metal", "vulkan");
        printf("%-40s", "");

        for (int bi = 0; bi < 4; bi++) {
            if (!progs[bi]) {
                printf(" %12s", "-");
                continue;
            }
            progs[bi]->queue(progs[bi], 0, 0, W, H, abuf);
            bes[bi]->flush(bes[bi]);
            int iters = 1;
            for (;;) {
                double const start = now();
                for (int it = 0; it < iters; it++) {
                    progs[bi]->queue(progs[bi], 0, 0, W, H, abuf);
                }
                bes[bi]->flush(bes[bi]);
                double const elapsed = now() - start;
                if (elapsed >= 0.1) {
                    char tmp[32];
                    sprintf(tmp, "%5.2f ns/px",
                            elapsed / ((double)iters * (double)(W * H)) * 1e9);
                    printf(" %12s", tmp);
                    break;
                }
                iters *= 2;
            }
        }
        printf("\n");

        for (int bi = 0; bi < 4; bi++) {
            if (progs[bi]) { progs[bi]->free(progs[bi]); }
            if (bes[bi])   { bes[bi]->free(bes[bi]); }
        }
        free(wind);
        umbra_uniforms_free(al.uni);
        slug_free(&sc);
    }

    slides_cleanup();
    return 0;
}
