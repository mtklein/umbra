#define _POSIX_C_SOURCE 200809L
#include "../slides/slide.h"
#include "../src/backend.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec
         + (double)ts.tv_nsec * 1e-9;
}

int main(int argc, char *argv[]) {
    int W = 4096;
    for (int i = 1; i < argc; i++) {
        W = atoi(argv[i]);
    }
    int const H = 480;
    slides_init(W, H);

    int ns = slide_count() - 1;
    printf("%-40s %12s %12s\n",
           "", "interp", "jit");

    for (int si = 0; si < ns; si++) {
        slide *s = slide_get(si);

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
        umbra_basic_block_free(bb);

        _Bool planar =
            s->store == umbra_store_fp16_planar;
        int px = planar ? 2 : 4;
        int ps = planar ? (s->load ? 2 : 1) : 0;
        void *row = calloc((size_t)W, (size_t)px);
        long row_sz = W * px;
        int32_t stride = (int32_t)W;

        if (!s->render_row) {
            printf("%-40s   (no render)\n",
                   s->title);
            umbra_backend_free(interp);
            umbra_backend_free(jit);
            free(row);
            continue;
        }
        s->render_row(s, H/2, W, row, row_sz,
                      &lay, ps, stride, interp);

        int iters = 1;
        for (;;) {
            double const start = now();
            for (int it = 0; it < iters; it++) {
                s->render_row(s, H/2, W,
                    row, row_sz,
                    &lay, ps, stride, interp);
            }
            double const elapsed = now() - start;
            if (elapsed >= 0.1) {
                double ns_px = elapsed
                    / ((double)iters * (double)W)
                    * 1e9;
                printf("%-40s %9.2f ns", s->title,
                       ns_px);
                break;
            }
            iters *= 2;
        }

        if (jit) {
            iters = 1;
            for (;;) {
                double const start = now();
                for (int it = 0; it < iters; it++) {
                    s->render_row(s, H/2, W,
                        row, row_sz,
                        &lay, ps, stride, jit);
                }
                double const elapsed = now()-start;
                if (elapsed >= 0.1) {
                    double ns_px = elapsed
                        / ((double)iters*(double)W)
                        * 1e9;
                    printf(" %9.2f ns", ns_px);
                    break;
                }
                iters *= 2;
            }
        }
        printf("\n");

        umbra_backend_free(interp);
        umbra_backend_free(jit);
        free(row);
    }

    slides_cleanup();
    return 0;
}
