#include "umbra_draw.h"
#include "test.h"
#include <stdint.h>

typedef struct {
    struct umbra_interpreter *interp;
    struct umbra_jit          *jit;
    struct umbra_metal        *mtl;
} draw_backends;

static draw_backends make_draw(struct umbra_basic_block *bb) {
    umbra_basic_block_optimize(bb);
    draw_backends B = {umbra_interpreter(bb), umbra_jit(bb), umbra_metal(bb)};
    umbra_basic_block_free(bb);
    return B;
}
static _Bool run_draw(draw_backends *B, int b, int n, umbra_buf buf[]) {
    switch (b) {
    case 0: umbra_interpreter_run(B->interp, n, buf); return 1;
    case 1: if (B->jit) { umbra_jit_run(B->jit, n, buf); return 1; } return 0;
    case 2: if (B->mtl) { umbra_metal_run(B->mtl, n, buf); return 1; } return 0;
    }
    return 0;
}
static void cleanup_draw(draw_backends *B) {
    umbra_interpreter_free(B->interp);
    if (B->jit) umbra_jit_free(B->jit);
    if (B->mtl) umbra_metal_free(B->mtl);
}

static void test_solid_src(void) {
    // Paint solid red (premul: r=1,g=0,b=0,a=1) into 8888 with src blend (overwrite).
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_src, umbra_load_8888, umbra_store_8888));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
        int32_t  x0 = 0, y = 0;
        __fp16   color[4] = {1, 0, 0, 1};
        // p0=dst(rw), p1=x0(ro), p2=y(ro), p3=color(ro)
        if (!run_draw(&B, bi, 4, (umbra_buf[]){{dst,4*4},{&x0,-4},{&y,-4},{color,-8}})) continue;
        for (int i = 0; i < 4; i++) {
            (( dst[i]        & 0xFF) == 0xFF) here;  // R
            (((dst[i] >>  8) & 0xFF) == 0x00) here;  // G
            (((dst[i] >> 16) & 0xFF) == 0x00) here;  // B
            (((dst[i] >> 24) & 0xFF) == 0xFF) here;  // A
        }
    }
    cleanup_draw(&B);
}

static void test_srcover_8888(void) {
    // SrcOver: semi-transparent green over white.
    // src = (0, 0.5, 0, 0.5) premul, dst = (1, 1, 1, 1)
    // result = src + dst*(1-0.5) = (0.5, 1.0, 0.5, 1.0)
    // As 8888: R=128, G=255, B=128, A=255
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_srcover, umbra_load_8888, umbra_store_8888));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[2] = {0xFFFFFFFF, 0xFFFFFFFF};
        int32_t  x0 = 0, y = 0;
        __fp16   color[4] = {0, (__fp16)0.5f, 0, (__fp16)0.5f};
        if (!run_draw(&B, bi, 2, (umbra_buf[]){{dst,2*4},{&x0,-4},{&y,-4},{color,-8}})) continue;
        for (int i = 0; i < 2; i++) {
            int r = (int)( dst[i]        & 0xFF);
            int g = (int)((dst[i] >>  8) & 0xFF);
            int b = (int)((dst[i] >> 16) & 0xFF);
            int a = (int)((dst[i] >> 24) & 0xFF);
            (r >= 127 && r <= 129) here;
            (g >= 254 && g <= 255) here;
            (b >= 127 && b <= 129) here;
            (a >= 254 && a <= 255) here;
        }
    }
    cleanup_draw(&B);
}

int main(void) {
    test_solid_src();
    test_srcover_8888();
    return 0;
}
