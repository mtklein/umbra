#include "../umbra_draw.h"
#include "test.h"
#include <stdint.h>

typedef struct {
    struct umbra_interpreter *interp;
    struct umbra_jit          *jit;
    struct umbra_metal        *mtl;
} draw_backends;

static void uni_i32(char *u, int off, int32_t v) { __builtin_memcpy(u+off, &v, 4); }
static void uni_h4(char *u, int off, __fp16 const c[4]) { __builtin_memcpy(u+off, c, 8); }
static void uni_h8(char *u, int off, __fp16 const c[8]) { __builtin_memcpy(u+off, c, 16); }
static void uni_f32(char *u, int off, float const *v, int n) { __builtin_memcpy(u+off, v, (unsigned long)n*4); }
static void uni_ptr(char *u, int off, void *p, long sz) {
    __builtin_memcpy(u+off,   &p,  8);
    __builtin_memcpy(u+off+8, &sz, 8);
}

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
    if (B->jit) { umbra_jit_free(B->jit); }
    if (B->mtl) { umbra_metal_free(B->mtl); }
}

static void test_solid_src(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_src, umbra_load_8888, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
        __fp16   color[4] = {1, 0, 0, 1};
        long long uni_[2] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        if (!run_draw(&B, bi, 4, (umbra_buf[]){{dst,4*4},{uni,-16}})) { continue; }
        for (int i = 0; i < 4; i++) {
            (( dst[i]        & 0xFF) == 0xFF) here;
            (((dst[i] >>  8) & 0xFF) == 0x00) here;
            (((dst[i] >> 16) & 0xFF) == 0x00) here;
            (((dst[i] >> 24) & 0xFF) == 0xFF) here;
        }
    }
    cleanup_draw(&B);
}

static void test_solid_src_n1(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_src, umbra_load_8888, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[1] = {0xFFFFFFFF};
        __fp16   color[4] = {0, 0, 1, 1};
        long long uni_[2] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        if (!run_draw(&B, bi, 1, (umbra_buf[]){{dst,4},{uni,-16}})) { continue; }
        (( dst[0]        & 0xFF) == 0x00) here;
        (((dst[0] >>  8) & 0xFF) == 0x00) here;
        (((dst[0] >> 16) & 0xFF) == 0xFF) here;
        (((dst[0] >> 24) & 0xFF) == 0xFF) here;
    }
    cleanup_draw(&B);
}

static void test_solid_src_n9(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_src, umbra_load_8888, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0xFF, sizeof dst);
        __fp16   color[4] = {0, 1, 0, 1};
        long long uni_[2] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        if (!run_draw(&B, bi, 9, (umbra_buf[]){{dst,9*4},{uni,-16}})) { continue; }
        for (int i = 0; i < 9; i++) {
            (((dst[i] >>  8) & 0xFF) == 0xFF) here;
            (((dst[i] >> 24) & 0xFF) == 0xFF) here;
        }
    }
    cleanup_draw(&B);
}

static void test_solid_src_n16(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_src, umbra_load_8888, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[16];
        __builtin_memset(dst, 0, sizeof dst);
        __fp16   color[4] = {1, 1, 1, 1};
        long long uni_[2] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        if (!run_draw(&B, bi, 16, (umbra_buf[]){{dst,16*4},{uni,-16}})) { continue; }
        for (int i = 0; i < 16; i++) {
            (dst[i] == 0xFFFFFFFF) here;
        }
    }
    cleanup_draw(&B);
}

static void test_srcover_8888(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_srcover, umbra_load_8888, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[2] = {0xFFFFFFFF, 0xFFFFFFFF};
        __fp16   color[4] = {0, (__fp16)0.5f, 0, (__fp16)0.5f};
        long long uni_[2] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        if (!run_draw(&B, bi, 2, (umbra_buf[]){{dst,2*4},{uni,-16}})) { continue; }
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

static void test_dstover_8888(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_dstover, umbra_load_8888, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[2] = {0xFFFFFFFF, 0xFFFFFFFF};
        __fp16   color[4] = {1, 0, 0, 1};
        long long uni_[2] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        if (!run_draw(&B, bi, 2, (umbra_buf[]){{dst,2*4},{uni,-16}})) { continue; }
        for (int i = 0; i < 2; i++) {
            (dst[i] == 0xFFFFFFFF) here;
        }
    }
    cleanup_draw(&B);
}

static void test_dstover_transparent(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_dstover, umbra_load_8888, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[2] = {0, 0};
        __fp16   color[4] = {1, 0, 0, 1};
        long long uni_[2] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        if (!run_draw(&B, bi, 2, (umbra_buf[]){{dst,2*4},{uni,-16}})) { continue; }
        for (int i = 0; i < 2; i++) {
            (( dst[i]        & 0xFF) >= 0xFE) here;
            (((dst[i] >> 24) & 0xFF) >= 0xFE) here;
        }
    }
    cleanup_draw(&B);
}

static void test_multiply_8888(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_multiply, umbra_load_8888, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[2] = {0xFF804020, 0xFF804020};
        __fp16   color[4] = {1, 1, 1, 1};
        long long uni_[2] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        if (!run_draw(&B, bi, 2, (umbra_buf[]){{dst,2*4},{uni,-16}})) { continue; }
        for (int i = 0; i < 2; i++) {
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

static void test_solid_src_fp16(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_src, umbra_load_fp16, umbra_store_fp16, NULL));

    for (int bi = 0; bi < 3; bi++) {
        __fp16 dst[4*3];
        __builtin_memset(dst, 0, sizeof dst);
        __fp16 color[4] = {(__fp16)0.25f, (__fp16)0.5f, (__fp16)0.75f, 1};
        long long uni_[2] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        if (!run_draw(&B, bi, 3, (umbra_buf[]){{dst,(long)sizeof dst},{uni,-16}})) { continue; }
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
        umbra_shader_solid, NULL, umbra_blend_srcover, umbra_load_fp16, umbra_store_fp16, NULL));

    for (int bi = 0; bi < 3; bi++) {
        __fp16 dst[4*2];
        for (int i = 0; i < 2; i++) {
            dst[i*4+0] = 1; dst[i*4+1] = 1; dst[i*4+2] = 1; dst[i*4+3] = 1;
        }
        __fp16 color[4] = {0, (__fp16)0.5f, 0, (__fp16)0.5f};
        long long uni_[2] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        if (!run_draw(&B, bi, 2, (umbra_buf[]){{dst,(long)sizeof dst},{uni,-16}})) { continue; }
        for (int i = 0; i < 2; i++) {
            float r = (float)dst[i*4+0], g = (float)dst[i*4+1];
            float b = (float)dst[i*4+2], a = (float)dst[i*4+3];
            (r >= 0.49f && r <= 0.51f) here;
            (g >= 0.99f && g <= 1.01f) here;
            (b >= 0.49f && b <= 0.51f) here;
            (a >= 0.99f && a <= 1.01f) here;
        }
    }
    cleanup_draw(&B);
}

static void test_coverage_rect(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, umbra_coverage_rect, umbra_blend_srcover,
        umbra_load_8888, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        __fp16 color[4] = {1, 0, 0, 1};
        float rect[4] = {2.0f, 0.0f, 5.0f, 1.0f};
        long long uni_[4] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        uni_f32(uni, 16, rect, 4);
        if (!run_draw(&B, bi, 8, (umbra_buf[]){
            {dst, (long)sizeof dst}, {uni, -32}
        })) { continue; }
        for (int i = 0; i < 8; i++) {
            if (i >= 2 && i < 5) {
                (( dst[i]        & 0xFF) >= 0xFE) here;
                (((dst[i] >> 24) & 0xFF) >= 0xFE) here;
            } else {
                (dst[i] == 0) here;
            }
        }
    }
    cleanup_draw(&B);
}

static void test_coverage_rect_scalar(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, umbra_coverage_rect, umbra_blend_srcover,
        umbra_load_8888, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4];
        __builtin_memset(dst, 0, sizeof dst);
        __fp16 color[4] = {1, 0, 0, 1};
        float rect[4] = {1.0f, 0.0f, 3.0f, 1.0f};
        long long uni_[4] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        uni_f32(uni, 16, rect, 4);
        if (!run_draw(&B, bi, 4, (umbra_buf[]){
            {dst, (long)sizeof dst}, {uni, -32}
        })) { continue; }
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
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, umbra_coverage_rect, umbra_blend_srcover,
        umbra_load_8888, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0, sizeof dst);
        __fp16 color[4] = {0, 1, 0, 1};
        float rect[4] = {3.0f, 0.0f, 7.0f, 1.0f};
        long long uni_[4] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        uni_f32(uni, 16, rect, 4);
        if (!run_draw(&B, bi, 9, (umbra_buf[]){
            {dst, 9*4}, {uni, -32}
        })) { continue; }
        for (int i = 0; i < 9; i++) {
            if (i >= 3 && i < 7) {
                (((dst[i] >> 8) & 0xFF) >= 0xFE) here;
            } else {
                (dst[i] == 0) here;
            }
        }
    }
    cleanup_draw(&B);
}

static void test_x0_offset(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, umbra_coverage_rect, umbra_blend_src,
        umbra_load_8888, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4];
        __builtin_memset(dst, 0, sizeof dst);
        __fp16 color[4] = {0, 1, 0, 1};
        float rect[4] = {11.0f, 0.0f, 13.0f, 10.0f};
        long long uni_[4] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 10);
        uni_i32(uni, 4, 5);
        uni_h4(uni, 8, color);
        uni_f32(uni, 16, rect, 4);
        if (!run_draw(&B, bi, 4, (umbra_buf[]){
            {dst, (long)sizeof dst}, {uni, -32}
        })) { continue; }
        (dst[0] == 0) here;
        (((dst[1] >> 8) & 0xFF) >= 0xFE) here;
        (((dst[2] >> 8) & 0xFF) >= 0xFE) here;
        (dst[3] == 0) here;
    }
    cleanup_draw(&B);
}

static void test_coverage_rect_outside_y(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, umbra_coverage_rect, umbra_blend_srcover,
        umbra_load_8888, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4] = {0x12345678, 0x12345678, 0x12345678, 0x12345678};
        __fp16 color[4] = {1, 1, 1, 1};
        float rect[4] = {0.0f, 0.0f, 10.0f, 2.0f};
        long long uni_[4] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 5);
        uni_h4(uni, 8, color);
        uni_f32(uni, 16, rect, 4);
        if (!run_draw(&B, bi, 4, (umbra_buf[]){
            {dst, (long)sizeof dst}, {uni, -32}
        })) { continue; }
        for (int i = 0; i < 4; i++) {
            (dst[i] == 0x12345678) here;
        }
    }
    cleanup_draw(&B);
}

static void test_no_shader(void) {
    draw_backends B = make_draw(umbra_draw_build(
        NULL, NULL, umbra_blend_src, umbra_load_8888, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
        long long uni_[1] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        if (!run_draw(&B, bi, 4, (umbra_buf[]){{dst,4*4},{uni,-8}})) { continue; }
        for (int i = 0; i < 4; i++) {
            (dst[i] == 0) here;
        }
    }
    cleanup_draw(&B);
}

static void test_no_blend(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, NULL, umbra_load_8888, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[2] = {0, 0};
        __fp16   color[4] = {1, 0, 1, 1};
        long long uni_[2] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        if (!run_draw(&B, bi, 2, (umbra_buf[]){{dst,2*4},{uni,-16}})) { continue; }
        for (int i = 0; i < 2; i++) {
            (( dst[i]        & 0xFF) >= 0xFE) here;
            (((dst[i] >>  8) & 0xFF) == 0x00) here;
            (((dst[i] >> 16) & 0xFF) >= 0xFE) here;
            (((dst[i] >> 24) & 0xFF) >= 0xFE) here;
        }
    }
    cleanup_draw(&B);
}

static umbra_color gradient_shader(struct umbra_basic_block *bb, umbra_f32 x, umbra_f32 y) {
    (void)y;
    int fi = umbra_reserve_f32(bb, 2);
    umbra_f32 w = umbra_load_f32(bb, (umbra_ptr){1}, umbra_imm_i32(bb, (uint32_t)(fi+0)));
    umbra_f32 a = umbra_load_f32(bb, (umbra_ptr){1}, umbra_imm_i32(bb, (uint32_t)(fi+1)));
    umbra_f32 t = umbra_div_f32(bb, x, w);
    umbra_half r = umbra_half_from_f32(bb, t);
    umbra_half zero = umbra_imm_half(bb, 0);
    umbra_half alpha = umbra_half_from_f32(bb, a);
    return (umbra_color){r, zero, zero, alpha};
}

static void test_gradient_shader(void) {
    draw_backends B = make_draw(umbra_draw_build(
        gradient_shader, NULL, umbra_blend_src, umbra_load_8888, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4] = {0, 0, 0, 0};
        float    params[2] = {4.0f, 1.0f};
        long long uni_[2] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_f32(uni, 8, params, 2);
        if (!run_draw(&B, bi, 4, (umbra_buf[]){
            {dst, (long)sizeof dst}, {uni, -16}
        })) { continue; }
        int r0 = (int)( dst[0]       & 0xFF);
        int r3 = (int)( dst[3]       & 0xFF);
        (r0 <= 2)                  here;
        (r3 >= 189 && r3 <= 193)   here;
        for (int i = 0; i < 4; i++) {
            (((dst[i] >> 24) & 0xFF) >= 0xFE) here;
        }
    }
    cleanup_draw(&B);
}

static void test_multiply_half_alpha(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_multiply, umbra_load_8888, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[2] = {0x80FF0000, 0x80FF0000};
        __fp16   color[4] = {1, 0, 0, (__fp16)0.5f};
        long long uni_[2] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        if (!run_draw(&B, bi, 2, (umbra_buf[]){{dst,2*4},{uni,-16}})) { continue; }
        for (int i = 0; i < 2; i++) {
            int r = (int)( dst[i]        & 0xFF);
            int g = (int)((dst[i] >>  8) & 0xFF);
            int b = (int)((dst[i] >> 16) & 0xFF);
            int a = (int)((dst[i] >> 24) & 0xFF);
            (r >= 125 && r <= 131) here;
            (g <= 2) here;
            (b >= 125 && b <= 131) here;
            (a >= 189 && a <= 195) here;
        }
    }
    cleanup_draw(&B);
}

static void test_srcover_8888_n9(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_srcover, umbra_load_8888, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0, sizeof dst);
        __fp16   color[4] = {1, 0, 0, (__fp16)0.5f};
        long long uni_[2] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        if (!run_draw(&B, bi, 9, (umbra_buf[]){{dst,9*4},{uni,-16}})) { continue; }
        for (int i = 0; i < 9; i++) {
            int r = (int)( dst[i]        & 0xFF);
            int a = (int)((dst[i] >> 24) & 0xFF);
            (r >= 0xFE)              here;
            (a >= 126 && a <= 130)   here;
        }
    }
    cleanup_draw(&B);
}

static void test_full_pipeline(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, umbra_coverage_rect, umbra_blend_srcover,
        umbra_load_8888, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0, sizeof dst);
        __fp16   color[4] = {1, 0, 0, 1};
        float    rect[4] = {2.0f, 0.0f, 7.0f, 1.0f};
        long long uni_[4] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        uni_f32(uni, 16, rect, 4);
        if (!run_draw(&B, bi, 9, (umbra_buf[]){
            {dst, 9*4}, {uni, -32}
        })) { continue; }
        for (int i = 0; i < 9; i++) {
            if (i >= 2 && i < 7) {
                int r = (int)( dst[i]        & 0xFF);
                int g = (int)((dst[i] >>  8) & 0xFF);
                int b = (int)((dst[i] >> 16) & 0xFF);
                int a = (int)((dst[i] >> 24) & 0xFF);
                (r >= 0xFE) here;
                (g <= 1)    here;
                (b <= 1)    here;
                (a >= 0xFE) here;
            } else {
                (dst[i] == 0) here;
            }
        }
    }
    cleanup_draw(&B);
}

static void test_solid_src_fp16_n9(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, NULL, umbra_blend_src, umbra_load_fp16, umbra_store_fp16, NULL));

    for (int bi = 0; bi < 3; bi++) {
        if (bi == 1) { continue; }
        __fp16 dst[4*9];
        __builtin_memset(dst, 0, sizeof dst);
        __fp16 color[4] = {(__fp16)0.125f, (__fp16)0.25f, (__fp16)0.5f, 1};
        long long uni_[2] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        if (!run_draw(&B, bi, 9, (umbra_buf[]){{dst,(long)sizeof dst},{uni,-16}})) { continue; }
        for (int i = 0; i < 9; i++) {
            (equiv((float)dst[i*4+0], 0.125f)) here;
            (equiv((float)dst[i*4+1], 0.25f))  here;
            (equiv((float)dst[i*4+2], 0.5f))   here;
            (equiv((float)dst[i*4+3], 1.0f))   here;
        }
    }
    cleanup_draw(&B);
}

static void test_coverage_rect_white_dst(void) {
    struct umbra_basic_block *bb = umbra_draw_build(
        umbra_shader_solid, umbra_coverage_rect, umbra_blend_srcover,
        umbra_load_8888, umbra_store_8888, NULL);
    umbra_basic_block_optimize(bb);
    struct umbra_interpreter *interp = umbra_interpreter(bb);
    struct umbra_jit         *jit    = umbra_jit(bb);
    struct umbra_metal       *mtl    = umbra_metal(bb);
    umbra_basic_block_free(bb);

    {
        uint32_t dst[16];
        __builtin_memset(dst, 0xFF, sizeof dst);
        __fp16   color[4] = {1, 0, 0, 1};
        float    rect[4] = {4.0f, 0.0f, 12.0f, 1.0f};
        long long uni_[4] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        uni_f32(uni, 16, rect, 4);
        umbra_interpreter_run(interp, 16, (umbra_buf[]){
            {dst, (long)sizeof dst}, {uni, -32}});
        for (int i = 0; i < 16; i++) {
            if (i >= 4 && i < 12) {
                (( dst[i]        & 0xFF) >= 0xFE) here;
                (((dst[i] >> 24) & 0xFF) >= 0xFE) here;
            } else {
                (dst[i] == 0xFFFFFFFF) here;
            }
        }
    }

    if (jit) {
        uint32_t dst[16];
        __builtin_memset(dst, 0xFF, sizeof dst);
        __fp16   color[4] = {1, 0, 0, 1};
        float    rect[4] = {4.0f, 0.0f, 12.0f, 1.0f};
        long long uni_[4] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        uni_f32(uni, 16, rect, 4);
        umbra_jit_run(jit, 16, (umbra_buf[]){
            {dst, (long)sizeof dst}, {uni, -32}});
        for (int i = 0; i < 16; i++) {
            if (i >= 4 && i < 12) {
                (( dst[i]        & 0xFF) >= 0xFE) here;
                (((dst[i] >> 24) & 0xFF) >= 0xFE) here;
            } else {
                (dst[i] == 0xFFFFFFFF) here;
            }
        }
    }

    if (jit) {
        uint32_t dst[24];
        __builtin_memset(dst, 0xFF, sizeof dst);
        __fp16   color[4] = {1, 0, 0, 1};
        float    rect[4] = {4.0f, 0.0f, 20.0f, 1.0f};
        long long uni_[4] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        uni_f32(uni, 16, rect, 4);
        umbra_jit_run(jit, 24, (umbra_buf[]){
            {dst, (long)sizeof dst}, {uni, -32}});
        for (int i = 0; i < 24; i++) {
            if (i >= 4 && i < 20) {
                (( dst[i]        & 0xFF) >= 0xFE) here;
                (((dst[i] >> 24) & 0xFF) >= 0xFE) here;
            } else {
                (dst[i] == 0xFFFFFFFF) here;
            }
        }
    }

    if (mtl) {
        uint32_t dst[16];
        __builtin_memset(dst, 0xFF, sizeof dst);
        __fp16   color[4] = {1, 0, 0, 1};
        float    rect[4] = {4.0f, 0.0f, 12.0f, 1.0f};
        long long uni_[4] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        uni_f32(uni, 16, rect, 4);
        umbra_metal_run(mtl, 16, (umbra_buf[]){
            {dst, (long)sizeof dst}, {uni, -32}});
        for (int i = 0; i < 16; i++) {
            if (i >= 4 && i < 12) {
                (( dst[i]        & 0xFF) >= 0xFE) here;
                (((dst[i] >> 24) & 0xFF) >= 0xFE) here;
            } else {
                (dst[i] == 0xFFFFFFFF) here;
            }
        }
    }

    umbra_interpreter_free(interp);
    if (jit) { umbra_jit_free(jit); }
    if (mtl) { umbra_metal_free(mtl); }
}

static void test_coverage_bitmap(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, umbra_coverage_bitmap, umbra_blend_srcover,
        umbra_load_8888, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        __fp16   color[4] = {1, 1, 1, 1};
        uint16_t cov[8] = {0, 128, 255, 0, 0, 0, 0, 0};
        long long uni_[4] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        uni_ptr(uni, 16, cov, (long)sizeof cov);
        if (!run_draw(&B, bi, 8, (umbra_buf[]){
            {dst, (long)sizeof dst}, {uni, -32}
        })) { continue; }
        (dst[0] == 0) here;
        ((dst[1] & 0xff) >= 120 && (dst[1] & 0xff) <= 136) here;
        ((dst[2] & 0xff) >= 0xfe) here;
        (dst[3] == 0) here;
    }
    cleanup_draw(&B);
}

static void test_coverage_sdf(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, umbra_coverage_sdf, umbra_blend_srcover,
        umbra_load_8888, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        __fp16   color[4] = {1, 1, 1, 1};
        uint16_t cov[8] = {0, 100, 128, 200, 255, 0, 0, 0};
        long long uni_[4] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        uni_ptr(uni, 16, cov, (long)sizeof cov);
        if (!run_draw(&B, bi, 8, (umbra_buf[]){
            {dst, (long)sizeof dst}, {uni, -32}
        })) { continue; }
        (dst[0] == 0) here;
        ((dst[4] & 0xff) >= 0xfe) here;
    }
    cleanup_draw(&B);
}

static void test_coverage_bitmap_matrix(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, umbra_coverage_bitmap_matrix, umbra_blend_srcover,
        umbra_load_8888, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        __fp16   color[4] = {1, 1, 1, 1};
        uint16_t bmp[8] = {0, 0, 255, 0, 0, 0, 0, 0};
        float mat[11] = {1, 0, 0,  0, 1, 0,  0, 0, 1,  8, 1};
        long long uni_[10] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        uni_f32(uni, 16, mat, 11);
        uni_ptr(uni, 64, bmp, (long)sizeof bmp);
        if (!run_draw(&B, bi, 8, (umbra_buf[]){
            {dst, (long)sizeof dst}, {uni, -80}
        })) { continue; }
        (dst[0] == 0) here;
        ((dst[2] & 0xff) >= 0xfe) here;
        (dst[3] == 0) here;
    }
    cleanup_draw(&B);
}

static void test_coverage_bitmap_matrix_oob(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_solid, umbra_coverage_bitmap_matrix, umbra_blend_srcover,
        umbra_load_8888, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        __fp16   color[4] = {1, 1, 1, 1};
        uint16_t bmp[4] = {255, 0, 0, 0};
        float mat[11] = {1, 0, 0,  0, 1, 0,  0.001f, 0, 1,  2, 2};
        long long uni_[10] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_h4(uni, 8, color);
        uni_f32(uni, 16, mat, 11);
        uni_ptr(uni, 64, bmp, (long)sizeof bmp);
        if (!run_draw(&B, bi, 8, (umbra_buf[]){
            {dst, (long)sizeof dst}, {uni, -80}
        })) { continue; }
        ((dst[0] & 0xff) >= 0xfc) here;
    }
    cleanup_draw(&B);
}

static void test_linear_2(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_linear_2, NULL, umbra_blend_src, NULL, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4] = {0};
        __fp16 colors[8] = {1,0,0,1, 0,0,1,1};
        float params[3] = {0.25f, 0, 0};
        long long uni_[5] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_f32(uni, 8, params, 3);
        uni_h8(uni, 20, colors);
        if (!run_draw(&B, bi, 4, (umbra_buf[]){
            {dst, (long)sizeof dst}, {uni, -36}
        })) { continue; }
        ((dst[0] & 0xff) >= 0xfc) here;
        (((dst[0] >> 16) & 0xff) <= 2) here;
        ((dst[3] & 0xff) <= 66) here;
        (((dst[3] >> 16) & 0xff) >= 189) here;
        for (int i = 0; i < 4; i++) { (((dst[i] >> 24) & 0xff) >= 0xfc) here; }
    }
    cleanup_draw(&B);
}

static void test_radial_2(void) {
    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_radial_2, NULL, umbra_blend_src, NULL, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[1] = {0};
        __fp16 colors[8] = {1,1,1,1, 0,0,0,1};
        float params[3] = {0, 0, 0.1f};
        long long uni_[5] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_f32(uni, 8, params, 3);
        uni_h8(uni, 20, colors);
        if (!run_draw(&B, bi, 1, (umbra_buf[]){
            {dst, 4}, {uni, -36}
        })) { continue; }
        ((dst[0] & 0xff) >= 0xfc) here;
        (((dst[0] >> 24) & 0xff) >= 0xfc) here;
    }
    cleanup_draw(&B);
}

static void test_linear_grad(void) {
    __fp16 stop_colors[][4] = {{1,0,0,1}, {0,1,0,1}, {0,0,1,1}};
    __fp16 lut[256*4];
    umbra_gradient_lut_even(lut, 256, 3, stop_colors);

    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_linear_grad, NULL, umbra_blend_src, NULL, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8] = {0};
        float params[4] = {0.125f, 0, 0, 256};
        long long uni_[5] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_f32(uni, 8, params, 4);
        uni_ptr(uni, 24, lut, (long)sizeof lut);
        if (!run_draw(&B, bi, 8, (umbra_buf[]){
            {dst, (long)sizeof dst}, {uni, -40}
        })) { continue; }
        ((dst[0] & 0xff) >= 0xfc) here;
        (((dst[0] >> 8) & 0xff) <= 2) here;
        (((dst[7] >> 16) & 0xff) >= 180) here;
    }
    cleanup_draw(&B);
}

static void test_radial_grad(void) {
    __fp16 stop_colors[][4] = {{1,0,0,1}, {1,1,0,1}, {0,1,0,1}, {0,0,1,1}};
    __fp16 lut[64*4];
    umbra_gradient_lut_even(lut, 64, 4, stop_colors);

    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_radial_grad, NULL, umbra_blend_src, NULL, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[1] = {0};
        float params[4] = {5, 5, 0.1f, 64};
        long long uni_[5] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 5);
        uni_i32(uni, 4, 5);
        uni_f32(uni, 8, params, 4);
        uni_ptr(uni, 24, lut, (long)sizeof lut);
        if (!run_draw(&B, bi, 1, (umbra_buf[]){
            {dst, 4}, {uni, -40}
        })) { continue; }
        ((dst[0] & 0xff) >= 0xfc) here;
        (((dst[0] >> 16) & 0xff) <= 2) here;
    }
    cleanup_draw(&B);
}

static void test_gradient_lut_nonuniform(void) {
    __fp16 stop_colors[][4] = {{1,0,0,1}, {0,1,0,1}, {0,0,1,1}};
    float  positions[] = {0, 0.2f, 1.0f};
    __fp16 lut[64*4];
    umbra_gradient_lut(lut, 64, 3, positions, stop_colors);

    draw_backends B = make_draw(umbra_draw_build(
        umbra_shader_linear_grad, NULL, umbra_blend_src, NULL, umbra_store_8888, NULL));

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8] = {0};
        float params[4] = {0.125f, 0, 0, 64};
        long long uni_[5] = {0}; char *uni = (char*)uni_;
        uni_i32(uni, 0, 0);
        uni_i32(uni, 4, 0);
        uni_f32(uni, 8, params, 4);
        uni_ptr(uni, 24, lut, (long)sizeof lut);
        if (!run_draw(&B, bi, 8, (umbra_buf[]){
            {dst, (long)sizeof dst}, {uni, -40}
        })) { continue; }
        ((dst[0] & 0xff) >= 0xfc) here;
        (((dst[7] >> 16) & 0xff) >= 180) here;
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
    test_gradient_shader();
    test_multiply_half_alpha();
    test_srcover_8888_n9();
    test_full_pipeline();
    test_solid_src_fp16_n9();
    test_coverage_rect_white_dst();
    test_coverage_bitmap();
    test_coverage_sdf();
    test_coverage_bitmap_matrix();
    test_coverage_bitmap_matrix_oob();
    test_linear_2();
    test_radial_2();
    test_linear_grad();
    test_radial_grad();
    test_gradient_lut_nonuniform();
    return 0;
}
