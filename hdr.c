#if defined(__wasm__)
int main(void) { return 0; }
#else

#include "umbra_draw.h"
#include <stdint.h>
#include <stdio.h>

#define W 256
#define H 256

typedef void (*run_fn)(void*, int, umbra_buf[]);

static void run_interp(void *ctx, int n, umbra_buf buf[]) { umbra_interpreter_run(ctx, n, buf); }
static void run_jit   (void *ctx, int n, umbra_buf buf[]) { umbra_jit_run(ctx, n, buf); }

int main(void) {
    __fp16 pixels[H * W * 4];

    struct umbra_basic_block *clear_bb = umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_src, umbra_load_fp16, umbra_store_fp16);
    umbra_basic_block_optimize(clear_bb);

    struct umbra_jit         *clear_jit    = umbra_jit(clear_bb);
    struct umbra_interpreter *clear_interp = umbra_interpreter(clear_bb);
    umbra_basic_block_free(clear_bb);

    run_fn clear_run = clear_jit ? run_jit    : run_interp;
    void  *clear_ctx = clear_jit ? (void*)clear_jit : (void*)clear_interp;

    struct umbra_basic_block *rect_bb = umbra_draw_build(
        umbra_shader_solid, umbra_coverage_rect, umbra_blend_srcover,
        umbra_load_fp16, umbra_store_fp16);
    umbra_basic_block_optimize(rect_bb);

    struct umbra_jit         *rect_jit    = umbra_jit(rect_bb);
    struct umbra_interpreter *rect_interp = umbra_interpreter(rect_bb);
    umbra_basic_block_free(rect_bb);

    run_fn rect_run = rect_jit ? run_jit    : run_interp;
    void  *rect_ctx = rect_jit ? (void*)rect_jit : (void*)rect_interp;

    {
        __fp16 color[4] = {(__fp16)0.1f, (__fp16)0.1f, (__fp16)0.1f, (__fp16)1.0f};
        for (int y = 0; y < H; y++) {
            int32_t x0 = 0, yy = y;
            umbra_buf buf[] = {
                {pixels + y * W * 4,  W * 4 * 2},
                {&x0, -4}, {&yy, -4}, {color, -8},
            };
            clear_run(clear_ctx, W, buf);
        }
    }

    struct { float l, t, r, b; __fp16 color[4]; } rects[] = {
        { 20,  20, 180, 180, {(__fp16)0.7f, (__fp16)0.0f, (__fp16)0.0f, (__fp16)0.7f}},
        { 60,  60, 220, 220, {(__fp16)0.0f, (__fp16)0.7f, (__fp16)0.0f, (__fp16)0.7f}},
        {100, 100, 240, 240, {(__fp16)0.0f, (__fp16)0.0f, (__fp16)0.7f, (__fp16)0.7f}},
    };

    for (int ri = 0; ri < 3; ri++) {
        float bounds[4] = {rects[ri].l, rects[ri].t, rects[ri].r, rects[ri].b};
        for (int y = 0; y < H; y++) {
            int32_t x0 = 0, yy = y;
            umbra_buf buf[] = {
                {pixels + y * W * 4,  W * 4 * 2},
                {&x0, -4}, {&yy, -4}, {rects[ri].color, -8}, {bounds, -16},
            };
            rect_run(rect_ctx, W, buf);
        }
    }

    printf("ok\n");

    umbra_interpreter_free(clear_interp);
    umbra_interpreter_free(rect_interp);
    if (clear_jit) { umbra_jit_free(clear_jit); }
    if (rect_jit)  { umbra_jit_free(rect_jit); }

    return 0;
}

#endif
