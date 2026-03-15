#include "../slides/slides.h"
#include "test.h"
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

enum { W = 128, H = 96 };

typedef void (*run_fn)(void*, int, umbra_buf[]);

static void run_interp (void *ctx, int n, umbra_buf buf[]) { umbra_interpreter_run(ctx, n, buf); }
static void run_jit    (void *ctx, int n, umbra_buf buf[]) { umbra_jit_run(ctx, n, buf); }
static void run_codegen(void *ctx, int n, umbra_buf buf[]) { umbra_codegen_run(ctx, n, buf); }
static void run_metal  (void *ctx, int n, umbra_buf buf[]) { umbra_metal_run(ctx, n, buf); }

enum { NUM_BACKENDS = 4 };
static char const *backend_name[NUM_BACKENDS] = {"interp", "jit", "codegen", "metal"};
static run_fn const run_fns[NUM_BACKENDS] = {run_interp, run_jit, run_codegen, run_metal};

static void build_perspective_matrix(float out[11], float t, int sw, int sh, int bw, int bh) {
    float cx = (float)sw * 0.5f, cy = (float)sh * 0.5f;
    float bx = (float)bw * 0.5f, by = (float)bh * 0.5f;
    float angle = t * 0.3f;
    float tilt  = sinf(t * 0.7f) * 0.0008f;
    float sc    = 1.0f + 0.2f * sinf(t * 0.5f);
    float ca = cosf(angle), sa = sinf(angle);
    float w0 = 1.0f - tilt * cx;
    out[0] = ca * sc;     out[1] = sa * sc;
    out[2] = -cx*ca*sc - cy*sa*sc + bx*w0;
    out[3] = -sa * sc;    out[4] = ca * sc;
    out[5] = cx*sa*sc - cy*ca*sc + by*w0;
    out[6] = tilt;        out[7] = 0.0f;       out[8] = w0;
    out[9] = (float)bw;   out[10] = (float)bh;
}

static void render_slide(int slide_idx, void *ctx, run_fn run,
                         uint32_t *px, text_cov *bitmap_cov, text_cov *sdf_cov) {
    slide const *s = &slides[slide_idx];
    __fp16 color[4] = {
        (__fp16)s->color[0], (__fp16)s->color[1],
        (__fp16)s->color[2], (__fp16)s->color[3],
    };

    // Fill background.
    for (int i = 0; i < W * H; i++) { px[i] = s->bg; }

    int32_t x0 = 0;

    if (s->coverage == umbra_coverage_bitmap_matrix) {
        float mat[11];
        build_perspective_matrix(mat, 1.0f, W, H, bitmap_cov->w, bitmap_cov->h);
        for (int y = 0; y < H; y++) {
            int32_t yval = y;
            umbra_buf buf[] = {
                { px + y * W,       W * 4                },
                { &x0,             -4                    },
                { &yval,           -4                    },
                { color,           -8                    },
                { bitmap_cov->data, -(W * H * 2)         },
                { mat,             -(int)(sizeof mat)     },
            };
            run(ctx, W, buf);
        }
    } else if (s->coverage == umbra_coverage_bitmap || s->coverage == umbra_coverage_sdf) {
        text_cov *tc = (s->coverage == umbra_coverage_bitmap) ? bitmap_cov : sdf_cov;
        for (int y = 0; y < H; y++) {
            int32_t yval = y;
            umbra_buf buf[] = {
                { px + y * W,        W * 4    },
                { &x0,              -4        },
                { &yval,            -4        },
                { color,            -8        },
                { tc->data + y * W, -(W * 2)  },
            };
            run(ctx, W, buf);
        }
    } else {
        // Rect slides: fixed rect position.
        float rect[4] = { 30.0f, 20.0f, 90.0f, 60.0f };
        for (int y = 0; y < H; y++) {
            int32_t yval = y;
            umbra_buf buf[] = {
                { px + y * W,  W * 4 },
                { &x0,        -4     },
                { &yval,      -4     },
                { color,      -8     },
                { rect,       -16    },
            };
            run(ctx, W, buf);
        }
    }
}

static void test_slide_golden(int slide_idx, text_cov *bitmap_cov, text_cov *sdf_cov) {
    slide const *s = &slides[slide_idx];

    struct umbra_basic_block *bb = umbra_draw_build(
        s->shader, s->coverage, s->blend, s->load, s->store);
    umbra_basic_block_optimize(bb);

    struct umbra_interpreter *interp = umbra_interpreter(bb);
    struct umbra_jit         *jit    = umbra_jit(bb);
    struct umbra_codegen     *cg     = umbra_codegen(bb);
    struct umbra_metal       *mtl    = umbra_metal(bb);
    umbra_basic_block_free(bb);

    void *backends[NUM_BACKENDS] = { interp, jit, cg, mtl };

    uint32_t *ref = malloc((size_t)(W * H) * 4);
    uint32_t *tst = malloc((size_t)(W * H) * 4);

    // Render reference with interpreter.
    render_slide(slide_idx, interp, run_interp, ref, bitmap_cov, sdf_cov);

    // Compare each other backend against interpreter.
    for (int bi = 1; bi < NUM_BACKENDS; bi++) {
        if (!backends[bi]) { continue; }
        render_slide(slide_idx, backends[bi], run_fns[bi], tst, bitmap_cov, sdf_cov);

        int mismatches = 0;
        int worst = 0;
        for (int i = 0; i < W * H; i++) {
            if (ref[i] == tst[i]) { continue; }
            for (int ch = 0; ch < 4; ch++) {
                int a = (int)((ref[i] >> (ch * 8)) & 0xFF);
                int b = (int)((tst[i] >> (ch * 8)) & 0xFF);
                int d = a - b;
                if (d < 0) { d = -d; }
                if (d > worst) { worst = d; }
            }
            mismatches++;
        }
        if (worst > 1) {
            dprintf(2, "slide %d \"%s\" %s: %d/%d pixels differ, "
                    "worst channel delta = %d\n",
                    slide_idx + 1, s->title, backend_name[bi],
                    mismatches, W * H, worst);
        }
        (worst <= 1) here;
    }

    free(ref);
    free(tst);
    umbra_interpreter_free(interp);
    if (jit) { umbra_jit_free(jit); }
    if (cg)  { umbra_codegen_free(cg); }
    if (mtl) { umbra_metal_free(mtl); }
}

int main(void) {
    text_cov bitmap_cov = text_rasterize(W, H, 24.0f, 0);
    text_cov sdf_cov    = text_rasterize(W, H, 24.0f, 1);

    for (int i = 0; i < SLIDE_COUNT; i++) {
        test_slide_golden(i, &bitmap_cov, &sdf_cov);
    }

    text_cov_free(&bitmap_cov);
    text_cov_free(&sdf_cov);
    return 0;
}
