#define _POSIX_C_SOURCE 200809L
#include "../slides/slide.h"
#include "../src/backend.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec
         + (double)ts.tv_nsec * 1e-9;
}

static double bench(slide *s, int w, int h,
                    umbra_draw_layout const *lay,
                    int ps, int32_t stride,
                    void *row, long row_sz,
                    struct umbra_backend *be) {
    s->render_row(s, h/2, w, row, row_sz,
                  lay, ps, stride, be);
    int iters = 1;
    for (;;) {
        double const start = now();
        for (int it = 0; it < iters; it++) {
            s->render_row(s, h/2, w, row, row_sz,
                          lay, ps, stride, be);
        }
        double const elapsed = now() - start;
        if (elapsed >= 0.1) {
            return elapsed
                 / ((double)iters * (double)w)
                 * 1e9;
        }
        iters *= 2;
    }
}

int main(int argc, char *argv[]) {
    int W = 4096;
    for (int i = 1; i < argc; i++) {
        W = atoi(argv[i]);
    }
    int const H = 480;
    slides_init(W, H);

    int ns = slide_count() - 1;
    printf("%-40s %12s %12s %12s\n",
           "", "interp", "jit", "metal");

    for (int si = 0; si < ns; si++) {
        slide *s = slide_get(si);
        if (!s->render_row) { continue; }

        umbra_draw_layout lay;
        struct umbra_builder *bld =
            umbra_draw_build(
                s->shader, s->coverage,
                s->blend, s->load, s->store,
                &lay);
        struct umbra_basic_block *bb =
            umbra_basic_block(bld);
        umbra_builder_free(bld);

        struct umbra_backend *interp =
            umbra_backend_interp(bb);
        struct umbra_backend *jit =
            umbra_backend_jit(bb);
        struct umbra_backend *mtl =
            umbra_backend_metal(bb);
        umbra_basic_block_free(bb);

        _Bool planar =
            s->store == umbra_store_fp16_planar;
        int px = planar ? 2 : 4;
        int ps = planar ? (s->load ? 2 : 1) : 0;
        void *row = calloc((size_t)W, (size_t)px);
        long row_sz = W * px;
        int32_t stride = (int32_t)W;

        char tmp[32];
        printf("%-40s", s->title);

        sprintf(tmp, "%5.2f ns/px",
                bench(s, W, H, &lay, ps, stride,
                      row, row_sz, interp));
        printf(" %12s", tmp);

        if (jit) {
            sprintf(tmp, "%5.2f ns/px",
                    bench(s, W, H, &lay, ps, stride,
                          row, row_sz, jit));
            printf(" %12s", tmp);
        } else {
            printf(" %12s", "-");
        }

        if (mtl) {
            sprintf(tmp, "%5.2f ns/px",
                    bench(s, W, H, &lay, ps, stride,
                          row, row_sz, mtl));
            printf(" %12s", tmp);
        } else {
            printf(" %12s", "-");
        }

        printf("\n");

        umbra_backend_free(interp);
        umbra_backend_free(jit);
        umbra_backend_free(mtl);
        free(row);
    }

    slides_cleanup();
    return 0;
}
