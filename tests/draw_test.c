#include "../umbra_draw.h"
#include "test.h"
#include <stdint.h>
#include <string.h>

typedef struct {
    struct umbra_interpreter *interp;
    struct umbra_jit          *jit;
    struct umbra_metal        *mtl;
} draw_backends;

static draw_backends make_draw(struct umbra_basic_block *bb) {
    umbra_basic_block_optimize(bb);
    draw_backends B = {umbra_interpreter(bb), umbra_jit(bb), umbra_metal(bb)};
    umbra_basic_block_free(bb);
    (B.interp != 0) here;
#if defined(__aarch64__) || defined(__AVX2__)
    (B.jit != 0) here;
#endif
#if defined(__APPLE__) && !defined(__wasm__)
    (B.mtl != 0) here;
#endif
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

// --- solid_src: basic shader + store, no blend ---

static void test_solid_src(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_src, umbra_load_8888, umbra_store_8888));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
        int32_t  x0 = 0, y = 0;
        __fp16   color[4] = {1, 0, 0, 1};
        if (!run_draw(&B, bi, 4, (umbra_buf[]){{dst,4*4},{&x0,-4},{&y,-4},{color,-8}})) continue;
        for (int i = 0; i < 4; i++) {
            (( dst[i]        & 0xFF) == 0xFF) here;
            (((dst[i] >>  8) & 0xFF) == 0x00) here;
            (((dst[i] >> 16) & 0xFF) == 0x00) here;
            (((dst[i] >> 24) & 0xFF) == 0xFF) here;
        }
    }
    cleanup_draw(&B);
}

// --- solid_src with various n values to exercise vector/scalar paths ---

static void test_solid_src_n1(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_src, umbra_load_8888, umbra_store_8888));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[1] = {0xFFFFFFFF};
        int32_t  x0 = 0, y = 0;
        __fp16   color[4] = {0, 0, 1, 1};  // blue
        if (!run_draw(&B, bi, 1, (umbra_buf[]){{dst,4},{&x0,-4},{&y,-4},{color,-8}})) continue;
        (( dst[0]        & 0xFF) == 0x00) here;
        (((dst[0] >>  8) & 0xFF) == 0x00) here;
        (((dst[0] >> 16) & 0xFF) == 0xFF) here;
        (((dst[0] >> 24) & 0xFF) == 0xFF) here;
    }
    cleanup_draw(&B);
}

static void test_solid_src_n9(void) {
    // n=9: one vector iteration (8) + one scalar tail element
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_src, umbra_load_8888, umbra_store_8888));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[9];
        memset(dst, 0xFF, sizeof dst);
        int32_t  x0 = 0, y = 0;
        __fp16   color[4] = {0, 1, 0, 1};  // green
        if (!run_draw(&B, bi, 9, (umbra_buf[]){{dst,9*4},{&x0,-4},{&y,-4},{color,-8}})) continue;
        for (int i = 0; i < 9; i++) {
            (((dst[i] >>  8) & 0xFF) == 0xFF) here;  // G=255
            (((dst[i] >> 24) & 0xFF) == 0xFF) here;  // A=255
        }
    }
    cleanup_draw(&B);
}

static void test_solid_src_n16(void) {
    // n=16: exactly two vector iterations, no scalar tail
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_src, umbra_load_8888, umbra_store_8888));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[16];
        memset(dst, 0, sizeof dst);
        int32_t  x0 = 0, y = 0;
        __fp16   color[4] = {1, 1, 1, 1};  // white
        if (!run_draw(&B, bi, 16, (umbra_buf[]){{dst,16*4},{&x0,-4},{&y,-4},{color,-8}})) continue;
        for (int i = 0; i < 16; i++) {
            (dst[i] == 0xFFFFFFFF) here;
        }
    }
    cleanup_draw(&B);
}

// --- srcover blend ---

static void test_srcover_8888(void) {
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

// --- dstover blend ---

static void test_dstover_8888(void) {
    // dst_over: dst + src*(1-dst.a)
    // White dst (a=1): result = dst (src contributes nothing because 1-dst.a=0)
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_dstover, umbra_load_8888, umbra_store_8888));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[2] = {0xFFFFFFFF, 0xFFFFFFFF};
        int32_t  x0 = 0, y = 0;
        __fp16   color[4] = {1, 0, 0, 1};  // red src
        if (!run_draw(&B, bi, 2, (umbra_buf[]){{dst,2*4},{&x0,-4},{&y,-4},{color,-8}})) continue;
        for (int i = 0; i < 2; i++) {
            // Dst was fully opaque white, so dstover keeps it white.
            (dst[i] == 0xFFFFFFFF) here;
        }
    }
    cleanup_draw(&B);
}

static void test_dstover_transparent(void) {
    // Transparent dst (a=0): result = dst + src*1 = src
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_dstover, umbra_load_8888, umbra_store_8888));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[2] = {0, 0};
        int32_t  x0 = 0, y = 0;
        __fp16   color[4] = {1, 0, 0, 1};  // red src
        if (!run_draw(&B, bi, 2, (umbra_buf[]){{dst,2*4},{&x0,-4},{&y,-4},{color,-8}})) continue;
        for (int i = 0; i < 2; i++) {
            (( dst[i]        & 0xFF) >= 0xFE) here;  // R=255
            (((dst[i] >> 24) & 0xFF) >= 0xFE) here;  // A=255
        }
    }
    cleanup_draw(&B);
}

// --- multiply blend ---

static void test_multiply_8888(void) {
    // White src (1,1,1,1) * any dst = dst (since multiply with opaque white is identity)
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_multiply, umbra_load_8888, umbra_store_8888));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[2] = {0xFF804020, 0xFF804020};  // RGBA=(32,64,128,255) in ABGR8888
        int32_t  x0 = 0, y = 0;
        __fp16   color[4] = {1, 1, 1, 1};  // white
        if (!run_draw(&B, bi, 2, (umbra_buf[]){{dst,2*4},{&x0,-4},{&y,-4},{color,-8}})) continue;
        for (int i = 0; i < 2; i++) {
            // multiply(white, dst) = white*dst + white*(1-da) + dst*(1-sa)
            //                      = dst + 0 + 0 = dst  (since da=sa=1)
            int r = (int)( dst[i]        & 0xFF);
            int g = (int)((dst[i] >>  8) & 0xFF);
            int b = (int)((dst[i] >> 16) & 0xFF);
            int a = (int)((dst[i] >> 24) & 0xFF);
            (r >= 0x1E && r <= 0x22) here;
            (g >= 0x3E && g <= 0x42) here;
            (b >= 0x7E && b <= 0x82) here;
            (a >= 0xFE)              here;
        }
    }
    cleanup_draw(&B);
}

// --- fp16 pixel format ---

static void test_solid_src_fp16(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_src, umbra_load_fp16, umbra_store_fp16));

    for (int bi = 0; bi < 3; bi++) {
        __fp16 dst[4*3];
        memset(dst, 0, sizeof dst);
        int32_t x0 = 0, y = 0;
        __fp16 color[4] = {(__fp16)0.25f, (__fp16)0.5f, (__fp16)0.75f, 1};
        if (!run_draw(&B, bi, 3, (umbra_buf[]){{dst,(long)sizeof dst},{&x0,-4},{&y,-4},{color,-8}})) continue;
        for (int i = 0; i < 3; i++) {
            (equiv((float)dst[i*4+0], 0.25f)) here;
            (equiv((float)dst[i*4+1], 0.5f))  here;
            (equiv((float)dst[i*4+2], 0.75f)) here;
            (equiv((float)dst[i*4+3], 1.0f))  here;
        }
    }
    cleanup_draw(&B);
}

static void test_srcover_fp16(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_srcover, umbra_load_fp16, umbra_store_fp16));

    for (int bi = 0; bi < 3; bi++) {
        // White destination.
        __fp16 dst[4*2];
        for (int i = 0; i < 2; i++) {
            dst[i*4+0] = 1; dst[i*4+1] = 1; dst[i*4+2] = 1; dst[i*4+3] = 1;
        }
        int32_t x0 = 0, y = 0;
        __fp16 color[4] = {0, (__fp16)0.5f, 0, (__fp16)0.5f};
        if (!run_draw(&B, bi, 2, (umbra_buf[]){{dst,(long)sizeof dst},{&x0,-4},{&y,-4},{color,-8}})) continue;
        for (int i = 0; i < 2; i++) {
            float r = (float)dst[i*4+0], g = (float)dst[i*4+1];
            float b = (float)dst[i*4+2], a = (float)dst[i*4+3];
            // src=(0,0.5,0,0.5) over (1,1,1,1): out = src + dst*(1-0.5)
            (r >= 0.49f && r <= 0.51f) here;
            (g >= 0.99f && g <= 1.01f) here;
            (b >= 0.49f && b <= 0.51f) here;
            (a >= 0.99f && a <= 1.01f) here;
        }
    }
    cleanup_draw(&B);
}

// --- coverage_rect tests ---

static void test_coverage_rect(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, umbra_coverage_rect, umbra_blend_srcover,
        umbra_load_8888, umbra_store_8888));

    for (int bi = 0; bi < 3; bi++) {
        // 8 pixels, rect covers x=[2,5), y=[0,1).
        uint32_t dst[8];
        memset(dst, 0, sizeof dst);
        int32_t x0 = 0, y = 0;
        __fp16 color[4] = {1, 0, 0, 1};  // solid red
        float rect[4] = {2.0f, 0.0f, 5.0f, 1.0f};  // l,t,r,b
        if (!run_draw(&B, bi, 8, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {&x0, -4}, {&y, -4},
            {color, -8}, {rect, -16}
        })) continue;
        for (int i = 0; i < 8; i++) {
            if (i >= 2 && i < 5) {
                (( dst[i]        & 0xFF) >= 0xFE) here;  // R=255
                (((dst[i] >> 24) & 0xFF) >= 0xFE) here;  // A=255
            } else {
                (dst[i] == 0) here;  // untouched (black)
            }
        }
    }
    cleanup_draw(&B);
}

static void test_coverage_rect_scalar(void) {
    // n=4: scalar-only path for JIT (tests the preamble re-emit fix).
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, umbra_coverage_rect, umbra_blend_srcover,
        umbra_load_8888, umbra_store_8888));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4];
        memset(dst, 0, sizeof dst);
        int32_t x0 = 0, y = 0;
        __fp16 color[4] = {1, 0, 0, 1};
        float rect[4] = {1.0f, 0.0f, 3.0f, 1.0f};
        if (!run_draw(&B, bi, 4, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {&x0, -4}, {&y, -4},
            {color, -8}, {rect, -16}
        })) continue;
        (dst[0] == 0) here;
        (( dst[1]        & 0xFF) >= 0xFE) here;
        (((dst[1] >> 24) & 0xFF) >= 0xFE) here;
        (( dst[2]        & 0xFF) >= 0xFE) here;
        (((dst[2] >> 24) & 0xFF) >= 0xFE) here;
        (dst[3] == 0) here;
    }
    cleanup_draw(&B);
}

static void test_coverage_rect_n9(void) {
    // n=9: vector(8) + scalar tail(1). Tests vector-to-scalar transition.
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, umbra_coverage_rect, umbra_blend_srcover,
        umbra_load_8888, umbra_store_8888));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[9];
        memset(dst, 0, sizeof dst);
        int32_t x0 = 0, y = 0;
        __fp16 color[4] = {0, 1, 0, 1};  // green
        float rect[4] = {3.0f, 0.0f, 7.0f, 1.0f};
        if (!run_draw(&B, bi, 9, (umbra_buf[]){
            {dst, 9*4},
            {&x0, -4}, {&y, -4},
            {color, -8}, {rect, -16}
        })) continue;
        for (int i = 0; i < 9; i++) {
            if (i >= 3 && i < 7) {
                (((dst[i] >> 8) & 0xFF) >= 0xFE) here;  // G=255
            } else {
                (dst[i] == 0) here;
            }
        }
    }
    cleanup_draw(&B);
}

// --- x0 offset ---

static void test_x0_offset(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, umbra_coverage_rect, umbra_blend_src,
        umbra_load_8888, umbra_store_8888));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4];
        memset(dst, 0, sizeof dst);
        int32_t x0 = 10, y = 5;
        __fp16 color[4] = {0, 1, 0, 1};
        float rect[4] = {11.0f, 0.0f, 13.0f, 10.0f};
        if (!run_draw(&B, bi, 4, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {&x0, -4}, {&y, -4},
            {color, -8}, {rect, -16}
        })) continue;
        (dst[0] == 0) here;
        (((dst[1] >> 8) & 0xFF) >= 0xFE) here;
        (((dst[2] >> 8) & 0xFF) >= 0xFE) here;
        (dst[3] == 0) here;
    }
    cleanup_draw(&B);
}

// --- coverage_rect outside y range ---

static void test_coverage_rect_outside_y(void) {
    // y=5 but rect covers y=[0,2), so all pixels should be untouched.
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, umbra_coverage_rect, umbra_blend_srcover,
        umbra_load_8888, umbra_store_8888));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4] = {0x12345678, 0x12345678, 0x12345678, 0x12345678};
        int32_t x0 = 0, y = 5;
        __fp16 color[4] = {1, 1, 1, 1};
        float rect[4] = {0.0f, 0.0f, 10.0f, 2.0f};
        if (!run_draw(&B, bi, 4, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {&x0, -4}, {&y, -4},
            {color, -8}, {rect, -16}
        })) continue;
        // y=5 is outside [0,2), so coverage=0. dst + (out - dst) * 0 = dst.
        for (int i = 0; i < 4; i++) {
            (dst[i] == 0x12345678) here;
        }
    }
    cleanup_draw(&B);
}

// --- no shader (src = black) ---

static void test_no_shader(void) {
    // shader=NULL → src = {0,0,0,0}. blend_src → write black.
    draw_backends B = make_draw(umbra_draw_build(
        NULL, NULL, umbra_blend_src, umbra_load_8888, umbra_store_8888));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
        int32_t  x0 = 0, y = 0;
        if (!run_draw(&B, bi, 4, (umbra_buf[]){{dst,4*4},{&x0,-4},{&y,-4}})) continue;
        for (int i = 0; i < 4; i++) {
            (dst[i] == 0) here;
        }
    }
    cleanup_draw(&B);
}

// --- no blend (just paint source) ---

static void test_no_blend(void) {
    // blend=NULL → out = src.
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, NULL, umbra_load_8888, umbra_store_8888));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[2] = {0, 0};
        int32_t  x0 = 0, y = 0;
        __fp16   color[4] = {1, 0, 1, 1};  // magenta
        if (!run_draw(&B, bi, 2, (umbra_buf[]){{dst,2*4},{&x0,-4},{&y,-4},{color,-8}})) continue;
        for (int i = 0; i < 2; i++) {
            (( dst[i]        & 0xFF) >= 0xFE) here;  // R
            (((dst[i] >>  8) & 0xFF) == 0x00) here;  // G=0
            (((dst[i] >> 16) & 0xFF) >= 0xFE) here;  // B
            (((dst[i] >> 24) & 0xFF) >= 0xFE) here;  // A
        }
    }
    cleanup_draw(&B);
}

int main(void) {
    test_solid_src();
    test_solid_src_n1();
    test_solid_src_n9();
    test_solid_src_n16();
    test_srcover_8888();
    test_dstover_8888();
    test_dstover_transparent();
    test_multiply_8888();
    test_solid_src_fp16();
    test_srcover_fp16();
    test_coverage_rect();
    test_coverage_rect_scalar();
    test_coverage_rect_n9();
    test_x0_offset();
    test_coverage_rect_outside_y();
    test_no_shader();
    test_no_blend();
    return 0;
}
