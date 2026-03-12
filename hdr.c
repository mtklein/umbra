#if defined(__wasm__)
int main(void) { return 0; }
#else

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#pragma clang diagnostic pop

#include "umbra_draw.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 256
#define H 256

typedef void (*run_fn)(void*, int, umbra_buf[]);

static void run_interp(void *ctx, int n, umbra_buf buf[]) {
    umbra_interpreter_run(ctx, n, buf);
}
static void run_jit(void *ctx, int n, umbra_buf buf[]) {
    umbra_jit_run(ctx, n, buf);
}

int main(void) {
    __fp16 pixels[H * W * 4];

    // --- Build clear pipeline: solid_src + fp16 (no coverage, no blend needed) ---
    struct umbra_basic_block *clear_bb = umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_src, umbra_load_fp16, umbra_store_fp16);
    umbra_basic_block_optimize(clear_bb);

    struct umbra_jit         *clear_jit    = umbra_jit(clear_bb);
    struct umbra_interpreter *clear_interp = umbra_interpreter(clear_bb);
    umbra_basic_block_free(clear_bb);

    run_fn clear_run;
    void  *clear_ctx;
    if (clear_jit) {
        clear_run = run_jit;
        clear_ctx = clear_jit;
    } else {
        clear_run = run_interp;
        clear_ctx = clear_interp;
    }

    // --- Build rect pipeline: solid + coverage_rect + srcover + fp16 ---
    struct umbra_basic_block *rect_bb = umbra_draw_build(
        umbra_shader_solid, umbra_coverage_rect, umbra_blend_srcover,
        umbra_load_fp16, umbra_store_fp16);
    umbra_basic_block_optimize(rect_bb);

    struct umbra_jit         *rect_jit    = umbra_jit(rect_bb);
    struct umbra_interpreter *rect_interp = umbra_interpreter(rect_bb);
    umbra_basic_block_free(rect_bb);

    run_fn rect_run;
    void  *rect_ctx;
    if (rect_jit) {
        rect_run = run_jit;
        rect_ctx = rect_jit;
    } else {
        rect_run = run_interp;
        rect_ctx = rect_interp;
    }

    // --- Clear to dark gray (0.1, 0.1, 0.1, 1.0) ---
    {
        __fp16 color[4] = {(__fp16)0.1f, (__fp16)0.1f, (__fp16)0.1f, (__fp16)1.0f};
        for (int y = 0; y < H; y++) {
            int32_t x0 = 0;
            int32_t yy = y;
            umbra_buf buf[] = {
                {pixels + y * W * 4,  W * 4 * 2},
                {&x0,                -4},
                {&yy,                -4},
                {color,              -8},
            };
            clear_run(clear_ctx, W, buf);
        }
    }

    // --- Paint colored rectangles ---
    struct {
        float l, t, r, b;
        __fp16 color[4];
    } rects[] = {
        { 20,  20, 180, 180, {(__fp16)0.7f, (__fp16)0.0f, (__fp16)0.0f, (__fp16)0.7f}},  // red
        { 60,  60, 220, 220, {(__fp16)0.0f, (__fp16)0.7f, (__fp16)0.0f, (__fp16)0.7f}},  // green
        {100, 100, 240, 240, {(__fp16)0.0f, (__fp16)0.0f, (__fp16)0.7f, (__fp16)0.7f}},  // blue
    };

    for (int ri = 0; ri < 3; ri++) {
        float bounds[4] = {rects[ri].l, rects[ri].t, rects[ri].r, rects[ri].b};
        for (int y = 0; y < H; y++) {
            int32_t x0 = 0;
            int32_t yy = y;
            umbra_buf buf[] = {
                {pixels + y * W * 4,  W * 4 * 2},
                {&x0,                -4},
                {&yy,                -4},
                {rects[ri].color,    -8},
                {bounds,             -16},
            };
            rect_run(rect_ctx, W, buf);
        }
    }

    // --- Convert fp16 to float for stbi_write_hdr ---
    float *fdata = malloc(W * H * 4 * sizeof(float));
    for (int i = 0; i < W * H * 4; i++) {
        fdata[i] = (float)pixels[i];
    }

    stbi_write_hdr("out.hdr", W, H, 4, fdata);
    printf("out.hdr\n");

    free(fdata);
    umbra_interpreter_free(clear_interp);
    umbra_interpreter_free(rect_interp);
    if (clear_jit) { umbra_jit_free(clear_jit); }
    if (rect_jit)  { umbra_jit_free(rect_jit); }

    return 0;
}

#endif
