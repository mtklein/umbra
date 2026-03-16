#include "../umbra_draw.h"
#include "test.h"
#include <stdint.h>

typedef struct {
    struct umbra_interpreter *interp;
    struct umbra_jit          *jit;
    struct umbra_metal        *mtl;
    umbra_draw_layout          lay;
    int                        pad_;
} draw_backends;

static void uni_i32(char *u, int off, int32_t v) {
    __builtin_memcpy(u+off, &v, 4);
}
static void uni_f32(char *u, int off,
                    float const *v, int n) {
    __builtin_memcpy(u+off, v, (unsigned long)n*4);
}
static void uni_ptr(char *u, int off,
                    void *p, long sz) {
    __builtin_memcpy(u+off,   &p,  8);
    __builtin_memcpy(u+off+8, &sz, 8);
}

static draw_backends make_draw(
        struct umbra_basic_block *bb,
        umbra_draw_layout lay) {
    umbra_basic_block_optimize(bb);
    draw_backends B = {
        umbra_interpreter(bb),
        umbra_jit(bb),
        umbra_metal(bb),
        lay, 0,
    };
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
static _Bool run_draw(draw_backends *B, int b,
                      int n, umbra_buf buf[]) {
    switch (b) {
    case 0:
        umbra_interpreter_run(B->interp, n, buf);
        return 1;
    case 1:
        if (B->jit) {
            umbra_jit_run(B->jit, n, buf);
            return 1;
        }
        return 0;
    case 2:
        if (B->mtl) {
            umbra_metal_run(B->mtl, n, buf);
            return 1;
        }
        return 0;
    }
    return 0;
}
static void cleanup_draw(draw_backends *B) {
    umbra_interpreter_free(B->interp);
    if (B->jit) { umbra_jit_free(B->jit); }
    if (B->mtl) { umbra_metal_free(B->mtl); }
}

static void test_solid_src(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid, NULL,
            umbra_blend_src,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4] = {
            0xFFFFFFFF, 0xFFFFFFFF,
            0xFFFFFFFF, 0xFFFFFFFF,
        };
        float color[4] = {1, 0, 0, 1};
        long long uni_[3] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 4, (umbra_buf[]){
            {dst, 4*4},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
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
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid, NULL,
            umbra_blend_src,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[1] = {0xFFFFFFFF};
        float    color[4] = {0, 0, 1, 1};
        long long uni_[3] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 1, (umbra_buf[]){
            {dst, 4},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        (( dst[0]        & 0xFF) == 0x00) here;
        (((dst[0] >>  8) & 0xFF) == 0x00) here;
        (((dst[0] >> 16) & 0xFF) == 0xFF) here;
        (((dst[0] >> 24) & 0xFF) == 0xFF) here;
    }
    cleanup_draw(&B);
}

static void test_solid_src_n9(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid, NULL,
            umbra_blend_src,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0xFF, sizeof dst);
        float    color[4] = {0, 1, 0, 1};
        long long uni_[3] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 9, (umbra_buf[]){
            {dst, 9*4},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        for (int i = 0; i < 9; i++) {
            (((dst[i] >>  8) & 0xFF) == 0xFF) here;
            (((dst[i] >> 24) & 0xFF) == 0xFF) here;
        }
    }
    cleanup_draw(&B);
}

static void test_solid_src_n16(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid, NULL,
            umbra_blend_src,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[16];
        __builtin_memset(dst, 0, sizeof dst);
        float    color[4] = {1, 1, 1, 1};
        long long uni_[3] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 16, (umbra_buf[]){
            {dst, 16*4},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        for (int i = 0; i < 16; i++) {
            (dst[i] == 0xFFFFFFFF) here;
        }
    }
    cleanup_draw(&B);
}

static void test_srcover_8888(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid, NULL,
            umbra_blend_srcover,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[2] = {0xFFFFFFFF, 0xFFFFFFFF};
        float    color[4] = {0, 0.5f, 0, 0.5f};
        long long uni_[3] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 2, (umbra_buf[]){
            {dst, 2*4},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        for (int i = 0; i < 2; i++) {
            int r = (int)( dst[i]        & 0xFF);
            int g = (int)((dst[i] >>  8) & 0xFF);
            int b = (int)((dst[i] >> 16) & 0xFF);
            int a = (int)((dst[i] >> 24) & 0xFF);
            (r == 128) here;
            (g == 255) here;
            (b == 128) here;
            (a == 255) here;
        }
    }
    cleanup_draw(&B);
}

static void test_dstover_8888(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid, NULL,
            umbra_blend_dstover,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[2] = {0xFFFFFFFF, 0xFFFFFFFF};
        float    color[4] = {1, 0, 0, 1};
        long long uni_[3] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 2, (umbra_buf[]){
            {dst, 2*4},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        for (int i = 0; i < 2; i++) {
            (dst[i] == 0xFFFFFFFF) here;
        }
    }
    cleanup_draw(&B);
}

static void test_dstover_transparent(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid, NULL,
            umbra_blend_dstover,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[2] = {0, 0};
        float    color[4] = {1, 0, 0, 1};
        long long uni_[3] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 2, (umbra_buf[]){
            {dst, 2*4},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        for (int i = 0; i < 2; i++) {
            (( dst[i]        & 0xFF) == 0xFF) here;
            (((dst[i] >> 24) & 0xFF) == 0xFF) here;
        }
    }
    cleanup_draw(&B);
}

static void test_multiply_8888(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid, NULL,
            umbra_blend_multiply,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[2] = {0xFF804020, 0xFF804020};
        float    color[4] = {1, 1, 1, 1};
        long long uni_[3] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 2, (umbra_buf[]){
            {dst, 2*4},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        for (int i = 0; i < 2; i++) {
            int r = (int)( dst[i]        & 0xFF);
            int g = (int)((dst[i] >>  8) & 0xFF);
            int b = (int)((dst[i] >> 16) & 0xFF);
            int a = (int)((dst[i] >> 24) & 0xFF);
            (r >= 0x1E && r <= 0x22) here;
            (g >= 0x3E && g <= 0x42) here;
            (b >= 0x7E && b <= 0x82) here;
            (a == 0xFF)              here;
        }
    }
    cleanup_draw(&B);
}

static void test_solid_src_fp16(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid, NULL,
            umbra_blend_src,
            umbra_load_fp16,
            umbra_store_fp16, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        __fp16 dst[4*3];
        __builtin_memset(dst, 0, sizeof dst);
        float color[4] = {0.25f, 0.5f, 0.75f, 1};
        long long uni_[3] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 3, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
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
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid, NULL,
            umbra_blend_srcover,
            umbra_load_fp16,
            umbra_store_fp16, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        __fp16 dst[4*2];
        for (int i = 0; i < 2; i++) {
            dst[i*4+0] = 1; dst[i*4+1] = 1;
            dst[i*4+2] = 1; dst[i*4+3] = 1;
        }
        float color[4] = {0, 0.5f, 0, 0.5f};
        long long uni_[3] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 2, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        for (int i = 0; i < 2; i++) {
            float r = (float)dst[i*4+0];
            float g = (float)dst[i*4+1];
            float b = (float)dst[i*4+2];
            float a = (float)dst[i*4+3];
            (equiv(r, 0.5f))  here;
            (equiv(g, 1.0f))  here;
            (equiv(b, 0.5f))  here;
            (equiv(a, 1.0f))  here;
        }
    }
    cleanup_draw(&B);
}

static void test_coverage_rect(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid,
            umbra_coverage_rect,
            umbra_blend_srcover,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        float color[4] = {1, 0, 0, 1};
        float rect[4] = {2.0f, 0.0f, 5.0f, 1.0f};
        long long uni_[5] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color, 4);
        uni_f32(uni, B.lay.coverage, rect, 4);
        if (!run_draw(&B, bi, 8, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        for (int i = 0; i < 8; i++) {
            if (i >= 2 && i < 5) {
                (( dst[i]      & 0xFF) == 0xFF) here;
                (((dst[i]>>24) & 0xFF) == 0xFF) here;
            } else {
                (dst[i] == 0) here;
            }
        }
    }
    cleanup_draw(&B);
}

static void test_coverage_rect_scalar(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid,
            umbra_coverage_rect,
            umbra_blend_srcover,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4];
        __builtin_memset(dst, 0, sizeof dst);
        float color[4] = {1, 0, 0, 1};
        float rect[4] = {1.0f, 0.0f, 3.0f, 1.0f};
        long long uni_[5] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color, 4);
        uni_f32(uni, B.lay.coverage, rect, 4);
        if (!run_draw(&B, bi, 4, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        (dst[0] == 0) here;
        (( dst[1]        & 0xFF) == 0xFF) here;
        (((dst[1] >> 24) & 0xFF) == 0xFF) here;
        (( dst[2]        & 0xFF) == 0xFF) here;
        (((dst[2] >> 24) & 0xFF) == 0xFF) here;
        (dst[3] == 0) here;
    }
    cleanup_draw(&B);
}

static void test_coverage_rect_n9(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid,
            umbra_coverage_rect,
            umbra_blend_srcover,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0, sizeof dst);
        float color[4] = {0, 1, 0, 1};
        float rect[4] = {3.0f, 0.0f, 7.0f, 1.0f};
        long long uni_[5] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color, 4);
        uni_f32(uni, B.lay.coverage, rect, 4);
        if (!run_draw(&B, bi, 9, (umbra_buf[]){
            {dst, 9*4},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        for (int i = 0; i < 9; i++) {
            if (i >= 3 && i < 7) {
                (((dst[i]>>8) & 0xFF) == 0xFF) here;
            } else {
                (dst[i] == 0) here;
            }
        }
    }
    cleanup_draw(&B);
}

static void test_x0_offset(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid,
            umbra_coverage_rect,
            umbra_blend_src,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4];
        __builtin_memset(dst, 0, sizeof dst);
        float color[4] = {0, 1, 0, 1};
        float rect[4] = {
            11.0f, 0.0f, 13.0f, 10.0f,
        };
        long long uni_[5] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 10);
        uni_i32(uni, B.lay.y, 5);
        uni_f32(uni, B.lay.shader, color, 4);
        uni_f32(uni, B.lay.coverage, rect, 4);
        if (!run_draw(&B, bi, 4, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        (dst[0] == 0) here;
        (((dst[1] >> 8) & 0xFF) == 0xFF) here;
        (((dst[2] >> 8) & 0xFF) == 0xFF) here;
        (dst[3] == 0) here;
    }
    cleanup_draw(&B);
}

static void test_coverage_rect_outside_y(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid,
            umbra_coverage_rect,
            umbra_blend_srcover,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4] = {
            0x12345678, 0x12345678,
            0x12345678, 0x12345678,
        };
        float color[4] = {1, 1, 1, 1};
        float rect[4] = {0.0f, 0.0f, 10.0f, 2.0f};
        long long uni_[5] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 5);
        uni_f32(uni, B.lay.shader, color, 4);
        uni_f32(uni, B.lay.coverage, rect, 4);
        if (!run_draw(&B, bi, 4, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        for (int i = 0; i < 4; i++) {
            (dst[i] == 0x12345678) here;
        }
    }
    cleanup_draw(&B);
}

static void test_no_shader(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            NULL, NULL, umbra_blend_src,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4] = {
            0xFFFFFFFF, 0xFFFFFFFF,
            0xFFFFFFFF, 0xFFFFFFFF,
        };
        long long uni_[1] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        if (!run_draw(&B, bi, 4, (umbra_buf[]){
            {dst, 4*4},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        for (int i = 0; i < 4; i++) {
            (dst[i] == 0) here;
        }
    }
    cleanup_draw(&B);
}

static void test_no_blend(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid, NULL, NULL,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[2] = {0, 0};
        float    color[4] = {1, 0, 1, 1};
        long long uni_[3] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 2, (umbra_buf[]){
            {dst, 2*4},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        for (int i = 0; i < 2; i++) {
            (( dst[i]        & 0xFF) == 0xFF) here;
            (((dst[i] >>  8) & 0xFF) == 0x00) here;
            (((dst[i] >> 16) & 0xFF) == 0xFF) here;
            (((dst[i] >> 24) & 0xFF) == 0xFF) here;
        }
    }
    cleanup_draw(&B);
}

static umbra_color gradient_shader(
        struct umbra_basic_block *bb,
        umbra_val x, umbra_val y) {
    (void)y;
    int fi = umbra_reserve(bb, 2);
    umbra_val w = umbra_load_i32(bb,
        (umbra_ptr){1}, umbra_imm_i32(bb, fi));
    umbra_val a = umbra_load_i32(bb,
        (umbra_ptr){1}, umbra_imm_i32(bb, fi+1));
    umbra_val t = umbra_div_f32(bb, x, w);
    umbra_val zero = umbra_imm_f32(bb, 0.0f);
    return (umbra_color){t, zero, zero, a};
}

static void test_gradient_shader(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            gradient_shader, NULL,
            umbra_blend_src,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4] = {0, 0, 0, 0};
        float    params[2] = {4.0f, 1.0f};
        long long uni_[2] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, params, 2);
        if (!run_draw(&B, bi, 4, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        int r0 = (int)( dst[0]       & 0xFF);
        int r3 = (int)( dst[3]       & 0xFF);
        (r0 == 0)                  here;
        (r3 == 191)                here;
        for (int i = 0; i < 4; i++) {
            (((dst[i]>>24) & 0xFF) == 0xFF) here;
        }
    }
    cleanup_draw(&B);
}

static void test_multiply_half_alpha(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid, NULL,
            umbra_blend_multiply,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[2] = {0x80FF0000, 0x80FF0000};
        float    color[4] = {1, 0, 0, 0.5f};
        long long uni_[3] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 2, (umbra_buf[]){
            {dst, 2*4},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        for (int i = 0; i < 2; i++) {
            int r = (int)( dst[i]        & 0xFF);
            int g = (int)((dst[i] >>  8) & 0xFF);
            int b = (int)((dst[i] >> 16) & 0xFF);
            int a = (int)((dst[i] >> 24) & 0xFF);
            r == 127 here;
            g == 0 here;
            b == 128 here;
            a == 192 here;
        }
    }
    cleanup_draw(&B);
}

static void test_srcover_8888_n9(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid, NULL,
            umbra_blend_srcover,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0, sizeof dst);
        float    color[4] = {1, 0, 0, 0.5f};
        long long uni_[3] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 9, (umbra_buf[]){
            {dst, 9*4},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        for (int i = 0; i < 9; i++) {
            int r = (int)( dst[i]        & 0xFF);
            int a = (int)((dst[i] >> 24) & 0xFF);
            (r == 0xFF)              here;
            (a == 128)               here;
        }
    }
    cleanup_draw(&B);
}

static void test_full_pipeline(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid,
            umbra_coverage_rect,
            umbra_blend_srcover,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0, sizeof dst);
        float color[4] = {1, 0, 0, 1};
        float rect[4] = {2.0f, 0.0f, 7.0f, 1.0f};
        long long uni_[5] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color, 4);
        uni_f32(uni, B.lay.coverage, rect, 4);
        if (!run_draw(&B, bi, 9, (umbra_buf[]){
            {dst, 9*4},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        for (int i = 0; i < 9; i++) {
            if (i >= 2 && i < 7) {
                int r = (int)( dst[i]      & 0xFF);
                int g = (int)((dst[i]>> 8) & 0xFF);
                int b = (int)((dst[i]>>16) & 0xFF);
                int a = (int)((dst[i]>>24) & 0xFF);
                (r == 0xFF) here;
                (g == 0)    here;
                (b == 0)    here;
                (a == 0xFF) here;
            } else {
                (dst[i] == 0) here;
            }
        }
    }
    cleanup_draw(&B);
}

static void test_solid_src_fp16_n9(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid, NULL,
            umbra_blend_src,
            umbra_load_fp16,
            umbra_store_fp16, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        if (bi == 1) { continue; }
        __fp16 dst[4*9];
        __builtin_memset(dst, 0, sizeof dst);
        float color[4] = {0.125f, 0.25f, 0.5f, 1};
        long long uni_[3] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 9, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
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
    umbra_draw_layout lay;
    struct umbra_basic_block *bb =
        umbra_draw_build(
            umbra_shader_solid,
            umbra_coverage_rect,
            umbra_blend_srcover,
            umbra_load_8888,
            umbra_store_8888, &lay);
    umbra_basic_block_optimize(bb);
    struct umbra_interpreter *interp =
        umbra_interpreter(bb);
    struct umbra_jit *jit = umbra_jit(bb);
    struct umbra_metal *mtl = umbra_metal(bb);
    umbra_basic_block_free(bb);

    {
        uint32_t dst[16];
        __builtin_memset(dst, 0xFF, sizeof dst);
        float color[4] = {1, 0, 0, 1};
        float rect[4] = {
            4.0f, 0.0f, 12.0f, 1.0f,
        };
        long long uni_[5] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, lay.x0, 0);
        uni_i32(uni, lay.y, 0);
        uni_f32(uni, lay.shader, color, 4);
        uni_f32(uni, lay.coverage, rect, 4);
        umbra_interpreter_run(interp, 16,
            (umbra_buf[]){
                {dst, (long)sizeof dst},
                {uni, -(long)lay.uni_len},
            });
        for (int i = 0; i < 16; i++) {
            if (i >= 4 && i < 12) {
                (( dst[i]      & 0xFF) == 0xFF) here;
                (((dst[i]>>24) & 0xFF) == 0xFF) here;
            } else {
                (dst[i] == 0xFFFFFFFF) here;
            }
        }
    }

    if (jit) {
        uint32_t dst[16];
        __builtin_memset(dst, 0xFF, sizeof dst);
        float color[4] = {1, 0, 0, 1};
        float rect[4] = {
            4.0f, 0.0f, 12.0f, 1.0f,
        };
        long long uni_[5] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, lay.x0, 0);
        uni_i32(uni, lay.y, 0);
        uni_f32(uni, lay.shader, color, 4);
        uni_f32(uni, lay.coverage, rect, 4);
        umbra_jit_run(jit, 16, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {uni, -(long)lay.uni_len},
        });
        for (int i = 0; i < 16; i++) {
            if (i >= 4 && i < 12) {
                (( dst[i]      & 0xFF) == 0xFF) here;
                (((dst[i]>>24) & 0xFF) == 0xFF) here;
            } else {
                (dst[i] == 0xFFFFFFFF) here;
            }
        }
    }

    if (jit) {
        uint32_t dst[24];
        __builtin_memset(dst, 0xFF, sizeof dst);
        float color[4] = {1, 0, 0, 1};
        float rect[4] = {
            4.0f, 0.0f, 20.0f, 1.0f,
        };
        long long uni_[5] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, lay.x0, 0);
        uni_i32(uni, lay.y, 0);
        uni_f32(uni, lay.shader, color, 4);
        uni_f32(uni, lay.coverage, rect, 4);
        umbra_jit_run(jit, 24, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {uni, -(long)lay.uni_len},
        });
        for (int i = 0; i < 24; i++) {
            if (i >= 4 && i < 20) {
                (( dst[i]      & 0xFF) == 0xFF) here;
                (((dst[i]>>24) & 0xFF) == 0xFF) here;
            } else {
                (dst[i] == 0xFFFFFFFF) here;
            }
        }
    }

    if (mtl) {
        uint32_t dst[16];
        __builtin_memset(dst, 0xFF, sizeof dst);
        float color[4] = {1, 0, 0, 1};
        float rect[4] = {
            4.0f, 0.0f, 12.0f, 1.0f,
        };
        long long uni_[5] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, lay.x0, 0);
        uni_i32(uni, lay.y, 0);
        uni_f32(uni, lay.shader, color, 4);
        uni_f32(uni, lay.coverage, rect, 4);
        umbra_metal_run(mtl, 16, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {uni, -(long)lay.uni_len},
        });
        for (int i = 0; i < 16; i++) {
            if (i >= 4 && i < 12) {
                (( dst[i]      & 0xFF) == 0xFF) here;
                (((dst[i]>>24) & 0xFF) == 0xFF) here;
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
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid,
            umbra_coverage_bitmap,
            umbra_blend_srcover,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        float    color[4] = {1, 1, 1, 1};
        uint16_t cov[8] = {0,128,255,0,0,0,0,0};
        long long uni_[5] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color, 4);
        uni_ptr(uni, B.lay.coverage,
                cov, (long)sizeof cov);
        if (!run_draw(&B, bi, 8, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        (dst[0] == 0) here;
        ((dst[1] & 0xff) >= 120
            && (dst[1] & 0xff) <= 136) here;
        ((dst[2] & 0xff) >= 0xfe) here;
        (dst[3] == 0) here;
    }
    cleanup_draw(&B);
}

static void test_coverage_sdf(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid,
            umbra_coverage_sdf,
            umbra_blend_srcover,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        float    color[4] = {1, 1, 1, 1};
        uint16_t cov[8] = {0,100,128,200,255,0,0,0};
        long long uni_[5] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color, 4);
        uni_ptr(uni, B.lay.coverage,
                cov, (long)sizeof cov);
        if (!run_draw(&B, bi, 8, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        (dst[0] == 0) here;
        ((dst[4] & 0xff) >= 0xfe) here;
    }
    cleanup_draw(&B);
}

static void test_coverage_bitmap_matrix(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid,
            umbra_coverage_bitmap_matrix,
            umbra_blend_srcover,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    int ptr_off =
        (B.lay.coverage + 11*4 + 7) & ~7;
    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        float    color[4] = {1, 1, 1, 1};
        uint16_t bmp[8] = {0,0,255,0,0,0,0,0};
        float mat[11] = {
            1, 0, 0,  0, 1, 0,  0, 0, 1,  8, 1,
        };
        long long uni_[11] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color, 4);
        uni_f32(uni, B.lay.coverage, mat, 11);
        uni_ptr(uni, ptr_off,
                bmp, (long)sizeof bmp);
        if (!run_draw(&B, bi, 8, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        (dst[0] == 0) here;
        ((dst[2] & 0xff) >= 0xfe) here;
        (dst[3] == 0) here;
    }
    cleanup_draw(&B);
}

static void test_coverage_bitmap_matrix_oob(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_solid,
            umbra_coverage_bitmap_matrix,
            umbra_blend_srcover,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    int ptr_off =
        (B.lay.coverage + 11*4 + 7) & ~7;
    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        float    color[4] = {1, 1, 1, 1};
        uint16_t bmp[4] = {255, 0, 0, 0};
        float mat[11] = {
            1, 0, 0,  0, 1, 0,
            0.001f, 0, 1,  2, 2,
        };
        long long uni_[11] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color, 4);
        uni_f32(uni, B.lay.coverage, mat, 11);
        uni_ptr(uni, ptr_off,
                bmp, (long)sizeof bmp);
        if (!run_draw(&B, bi, 8, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        ((dst[0] & 0xff) == 0xFF) here;
    }
    cleanup_draw(&B);
}

static void test_linear_2(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_linear_2, NULL,
            umbra_blend_src, NULL,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4] = {0};
        float colors[8] = {1,0,0,1, 0,0,1,1};
        float params[3] = {0.25f, 0, 0};
        long long uni_[7] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, params, 3);
        uni_f32(uni, B.lay.shader+12, colors, 8);
        if (!run_draw(&B, bi, 4, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        ((dst[0] & 0xff) == 0xFF) here;
        (((dst[0]>>16) & 0xff) == 0) here;
        ((dst[3] & 0xff) <= 66) here;
        (((dst[3]>>16) & 0xff) >= 189) here;
        for (int i = 0; i < 4; i++) {
            (((dst[i]>>24) & 0xff) == 0xFF) here;
        }
    }
    cleanup_draw(&B);
}

static void test_radial_2(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_radial_2, NULL,
            umbra_blend_src, NULL,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[1] = {0};
        float colors[8] = {1,1,1,1, 0,0,0,1};
        float params[3] = {0, 0, 0.1f};
        long long uni_[7] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, params, 3);
        uni_f32(uni, B.lay.shader+12, colors, 8);
        if (!run_draw(&B, bi, 1, (umbra_buf[]){
            {dst, 4},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        ((dst[0] & 0xff) == 0xFF) here;
        (((dst[0]>>24) & 0xff) == 0xFF) here;
    }
    cleanup_draw(&B);
}

static void test_linear_grad(void) {
    float stop_colors[][4] = {
        {1,0,0,1}, {0,1,0,1}, {0,0,1,1},
    };
    float lut[256*4];
    umbra_gradient_lut_even(lut, 256,
                            3, stop_colors);

    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_linear_grad, NULL,
            umbra_blend_src, NULL,
            umbra_store_8888, &lay), lay);

    int lut_off = (B.lay.shader + 16 + 7) & ~7;
    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8] = {0};
        float params[4] = {0.125f, 0, 0, 256};
        long long uni_[5] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, params, 4);
        uni_ptr(uni, lut_off,
                lut, (long)sizeof lut);
        if (!run_draw(&B, bi, 8, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        ((dst[0] & 0xff) == 0xFF) here;
        (((dst[0]>>8) & 0xff) == 0) here;
        (((dst[7]>>16) & 0xff) >= 180) here;
    }
    cleanup_draw(&B);
}

static void test_radial_grad(void) {
    float stop_colors[][4] = {
        {1,0,0,1}, {1,1,0,1},
        {0,1,0,1}, {0,0,1,1},
    };
    float lut[64*4];
    umbra_gradient_lut_even(lut, 64,
                            4, stop_colors);

    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_radial_grad, NULL,
            umbra_blend_src, NULL,
            umbra_store_8888, &lay), lay);

    int lut_off = (B.lay.shader + 16 + 7) & ~7;
    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[1] = {0};
        float params[4] = {5, 5, 0.1f, 64};
        long long uni_[5] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 5);
        uni_i32(uni, B.lay.y, 5);
        uni_f32(uni, B.lay.shader, params, 4);
        uni_ptr(uni, lut_off,
                lut, (long)sizeof lut);
        if (!run_draw(&B, bi, 1, (umbra_buf[]){
            {dst, 4},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        ((dst[0] & 0xff) == 0xFF) here;
        (((dst[0]>>16) & 0xff) == 0) here;
    }
    cleanup_draw(&B);
}

static void test_gradient_lut_nonuniform(void) {
    float stop_colors[][4] = {
        {1,0,0,1}, {0,1,0,1}, {0,0,1,1},
    };
    float positions[] = {0, 0.2f, 1.0f};
    float lut[64*4];
    umbra_gradient_lut(lut, 64,
        3, positions, stop_colors);

    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            umbra_shader_linear_grad, NULL,
            umbra_blend_src, NULL,
            umbra_store_8888, &lay), lay);

    int lut_off = (B.lay.shader + 16 + 7) & ~7;
    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8] = {0};
        float params[4] = {0.125f, 0, 0, 64};
        long long uni_[5] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, params, 4);
        uni_ptr(uni, lut_off,
                lut, (long)sizeof lut);
        if (!run_draw(&B, bi, 8, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        ((dst[0] & 0xff) == 0xFF) here;
        (((dst[7]>>16) & 0xff) >= 180) here;
    }
    cleanup_draw(&B);
}

static void test_transfer_lut(void) {
    float inv[256], fwd[256];
    umbra_transfer_lut_invert(inv,
                              &umbra_transfer_srgb);
    umbra_transfer_lut_apply(fwd,
                             &umbra_transfer_srgb);

    (equiv(inv[0], 0.0f)) here;
    (inv[255] >= 0.999f
        && inv[255] <= 1.001f) here;
    (inv[128] > inv[64]) here;

    (equiv(fwd[0], 0.0f)) here;
    (fwd[255] >= 0.999f
        && fwd[255] <= 1.001f) here;
    (fwd[128] > fwd[64]) here;

    for (int i = 0; i < 256; i++) {
        (inv[i] >= 0.0f && inv[i] <= 1.0f) here;
        (fwd[i] >= 0.0f && fwd[i] <= 1.0f) here;
    }
}

static umbra_color srgb_invert_shader(
        struct umbra_basic_block *bb,
        umbra_val x, umbra_val y) {
    (void)x; (void)y;
    int fi = umbra_reserve(bb, 4);
    umbra_val r = umbra_load_i32(bb,
        (umbra_ptr){1}, umbra_imm_i32(bb, fi));
    umbra_val g = umbra_load_i32(bb,
        (umbra_ptr){1}, umbra_imm_i32(bb, fi+1));
    umbra_val b = umbra_load_i32(bb,
        (umbra_ptr){1}, umbra_imm_i32(bb, fi+2));
    umbra_val a = umbra_load_i32(bb,
        (umbra_ptr){1}, umbra_imm_i32(bb, fi+3));
    umbra_color c = {r, g, b, a};
    return umbra_transfer_invert(bb, c,
                                 &umbra_transfer_srgb);
}

static void test_transfer_invert(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            srgb_invert_shader, NULL,
            umbra_blend_src, NULL,
            umbra_store_fp16, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        __fp16 dst[4*2];
        __builtin_memset(dst, 0, sizeof dst);
        float color0[4] = {0.5f, 0.5f, 0.5f, 1};
        long long uni_[3] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color0, 4);
        if (!run_draw(&B, bi, 2, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        for (int i = 0; i < 2; i++) {
            float r = (float)dst[i*4+0];
            float a = (float)dst[i*4+3];
            equiv(r, 0.2141113281f) here;
            (equiv(a, 1.0f))   here;
        }
    }
    cleanup_draw(&B);
}

static umbra_color srgb_apply_shader(
        struct umbra_basic_block *bb,
        umbra_val x, umbra_val y) {
    (void)x; (void)y;
    int fi = umbra_reserve(bb, 4);
    umbra_val r = umbra_load_i32(bb,
        (umbra_ptr){1}, umbra_imm_i32(bb, fi));
    umbra_val g = umbra_load_i32(bb,
        (umbra_ptr){1}, umbra_imm_i32(bb, fi+1));
    umbra_val b = umbra_load_i32(bb,
        (umbra_ptr){1}, umbra_imm_i32(bb, fi+2));
    umbra_val a = umbra_load_i32(bb,
        (umbra_ptr){1}, umbra_imm_i32(bb, fi+3));
    umbra_color c = {r, g, b, a};
    return umbra_transfer_apply(bb, c,
                                &umbra_transfer_srgb);
}

static void test_transfer_apply(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            srgb_apply_shader, NULL,
            umbra_blend_src, NULL,
            umbra_store_fp16, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        __fp16 dst[4*2];
        __builtin_memset(dst, 0, sizeof dst);
        float color0[4] = {0.5f, 0.5f, 0.5f, 1};
        long long uni_[3] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color0, 4);
        if (!run_draw(&B, bi, 2, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        for (int i = 0; i < 2; i++) {
            float r = (float)dst[i*4+0];
            float a = (float)dst[i*4+3];
            (r >= 0.72f && r <= 0.75f) here;
            (a >= 0.99f && a <= 1.01f) here;
        }
    }
    cleanup_draw(&B);
}

static umbra_color srgb_roundtrip_shader(
        struct umbra_basic_block *bb,
        umbra_val x, umbra_val y) {
    (void)x; (void)y;
    int fi = umbra_reserve(bb, 4);
    umbra_val r = umbra_load_i32(bb,
        (umbra_ptr){1}, umbra_imm_i32(bb, fi));
    umbra_val g = umbra_load_i32(bb,
        (umbra_ptr){1}, umbra_imm_i32(bb, fi+1));
    umbra_val b = umbra_load_i32(bb,
        (umbra_ptr){1}, umbra_imm_i32(bb, fi+2));
    umbra_val a = umbra_load_i32(bb,
        (umbra_ptr){1}, umbra_imm_i32(bb, fi+3));
    umbra_color c = {r, g, b, a};
    c = umbra_transfer_apply(bb, c,
                             &umbra_transfer_srgb);
    c = umbra_transfer_invert(bb, c,
                              &umbra_transfer_srgb);
    return c;
}

static void test_transfer_roundtrip(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            srgb_roundtrip_shader, NULL,
            umbra_blend_src, NULL,
            umbra_store_fp16, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        __fp16 dst[4*4];
        __builtin_memset(dst, 0, sizeof dst);
        float color0[4] = {0.25f, 0.5f, 0.75f, 1};
        long long uni_[3] = {0};
        char *uni = (char*)uni_;
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color0, 4);
        if (!run_draw(&B, bi, 4, (umbra_buf[]){
            {dst, (long)sizeof dst},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        for (int i = 0; i < 4; i++) {
            float r = (float)dst[i*4+0];
            float g = (float)dst[i*4+1];
            float b = (float)dst[i*4+2];
            float a = (float)dst[i*4+3];
            equiv(r, 0.2498779297f) here;
            equiv(g, 0.5f)         here;
            equiv(b, 0.75f)        here;
            equiv(a, 1.0f)         here;
        }
    }
    cleanup_draw(&B);
}

static umbra_shader_fn ss_inner_;
static int ss_n_;
static umbra_color ss_shader_(
        struct umbra_basic_block *bb,
        umbra_val x, umbra_val y) {
    return umbra_supersample(bb, x, y,
                             ss_inner_, ss_n_);
}

static void test_supersample(void) {
    ss_inner_ = umbra_shader_solid;
    ss_n_ = 4;
    umbra_draw_layout lay;
    draw_backends B = make_draw(
        umbra_draw_build(
            ss_shader_, NULL,
            umbra_blend_src,
            umbra_load_8888,
            umbra_store_8888, &lay), lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4] = {0};
        long long uni_[3] = {0};
        char *uni = (char*)uni_;
        float color[4] = {1, 0, 0, 1};
        uni_i32(uni, B.lay.x0, 0);
        uni_i32(uni, B.lay.y, 0);
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 4, (umbra_buf[]){
            {dst, 4*4},
            {uni, -(long)B.lay.uni_len},
        })) { continue; }
        for (int i = 0; i < 4; i++) {
            (( dst[i]        & 0xFF) == 0xFF) here;
            (((dst[i] >> 24) & 0xFF) == 0xFF) here;
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
    test_supersample();
    test_transfer_lut();
    test_transfer_invert();
    test_transfer_apply();
    test_transfer_roundtrip();
    return 0;
}
