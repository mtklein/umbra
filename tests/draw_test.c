#include "../include/umbra_draw.h"
#include "test.h"
#include <stdint.h>

typedef struct {
    test_backends     tb;
    umbra_draw_layout lay;
} draw_backends;

static void uni_f32(char *u, int off, float const *v, int n) {
    __builtin_memcpy(u + off, v, (unsigned long)n * 4);
}
static void uni_ptr(char *u, int off, void *p, long sz) {
    __builtin_memcpy(u + off, &p, 8);
    __builtin_memcpy(u + off + 8, &sz, 8);
}

static draw_backends make_draw(struct umbra_builder *builder, umbra_draw_layout lay) {
    struct umbra_basic_block *bb = umbra_basic_block(builder);
    umbra_builder_free(builder);
    draw_backends B = {
        test_backends_make(bb),
        lay,
    };
    umbra_basic_block_free(bb);
    return B;
}
static _Bool run_draw(draw_backends *B, int b, int w, int h, umbra_buf buf[]) {
    return test_backends_run(&B->tb, b, w, h, buf);
}
static void cleanup_draw(draw_backends *B) { test_backends_free(&B->tb); }

static void test_solid_src(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_src,
                                                 umbra_load_8888, umbra_store_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4] = {
            0xFFFFFFFF,
            0xFFFFFFFF,
            0xFFFFFFFF,
            0xFFFFFFFF,
        };
        float     color[4] = {1, 0, 0, 1};
        long long uni_[3] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 4, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, 4 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 4; i++) {
            ((dst[i] & 0xFF) == 0xFF) here;
            (((dst[i] >> 8) & 0xFF) == 0x00) here;
            (((dst[i] >> 16) & 0xFF) == 0x00) here;
            (((dst[i] >> 24) & 0xFF) == 0xFF) here;
        }
    }
    cleanup_draw(&B);
}

static void test_solid_src_n1(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_src,
                                                 umbra_load_8888, umbra_store_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[1] = {0xFFFFFFFF};
        float     color[4] = {0, 0, 1, 1};
        long long uni_[3] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 1, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, 4},
                      })) {
            continue;
        }
        ((dst[0] & 0xFF) == 0x00) here;
        (((dst[0] >> 8) & 0xFF) == 0x00) here;
        (((dst[0] >> 16) & 0xFF) == 0xFF) here;
        (((dst[0] >> 24) & 0xFF) == 0xFF) here;
    }
    cleanup_draw(&B);
}

static void test_solid_src_n9(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_src,
                                                 umbra_load_8888, umbra_store_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0xFF, sizeof dst);
        float     color[4] = {0, 1, 0, 1};
        long long uni_[3] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 9, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, 9 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 9; i++) {
            (((dst[i] >> 8) & 0xFF) == 0xFF) here;
            (((dst[i] >> 24) & 0xFF) == 0xFF) here;
        }
    }
    cleanup_draw(&B);
}

static void test_solid_src_n16(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_src,
                                                 umbra_load_8888, umbra_store_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[16];
        __builtin_memset(dst, 0, sizeof dst);
        float     color[4] = {1, 1, 1, 1};
        long long uni_[3] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 16, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, 16 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 16; i++) { (dst[i] == 0xFFFFFFFF) here; }
    }
    cleanup_draw(&B);
}

static void test_srcover_8888(void) {
    umbra_draw_layout lay;
    draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_srcover,
                                   umbra_load_8888, umbra_store_8888, &lay),
                  lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[2] = {0xFFFFFFFF, 0xFFFFFFFF};
        float     color[4] = {0, 0.5f, 0, 0.5f};
        long long uni_[3] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 2, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, 2 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 2; i++) {
            int r = (int)(dst[i] & 0xFF);
            int g = (int)((dst[i] >> 8) & 0xFF);
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
    draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_dstover,
                                   umbra_load_8888, umbra_store_8888, &lay),
                  lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[2] = {0xFFFFFFFF, 0xFFFFFFFF};
        float     color[4] = {1, 0, 0, 1};
        long long uni_[3] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 2, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, 2 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 2; i++) { (dst[i] == 0xFFFFFFFF) here; }
    }
    cleanup_draw(&B);
}

static void test_dstover_transparent(void) {
    umbra_draw_layout lay;
    draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_dstover,
                                   umbra_load_8888, umbra_store_8888, &lay),
                  lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[2] = {0, 0};
        float     color[4] = {1, 0, 0, 1};
        long long uni_[3] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 2, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, 2 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 2; i++) {
            ((dst[i] & 0xFF) == 0xFF) here;
            (((dst[i] >> 24) & 0xFF) == 0xFF) here;
        }
    }
    cleanup_draw(&B);
}

static void test_multiply_8888(void) {
    umbra_draw_layout lay;
    draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_multiply,
                                   umbra_load_8888, umbra_store_8888, &lay),
                  lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[2] = {0xFF804020, 0xFF804020};
        float     color[4] = {1, 1, 1, 1};
        long long uni_[3] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 2, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, 2 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 2; i++) {
            int r = (int)(dst[i] & 0xFF);
            int g = (int)((dst[i] >> 8) & 0xFF);
            int b = (int)((dst[i] >> 16) & 0xFF);
            int a = (int)((dst[i] >> 24) & 0xFF);
            (r >= 0x1E && r <= 0x22) here;
            (g >= 0x3E && g <= 0x42) here;
            (b >= 0x7E && b <= 0x82) here;
            (a == 0xFF) here;
        }
    }
    cleanup_draw(&B);
}

static void test_solid_src_fp16(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_src,
                                                 umbra_load_fp16, umbra_store_fp16, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        __fp16 dst[4 * 3];
        __builtin_memset(dst, 0, sizeof dst);
        float     color[4] = {0.25f, 0.5f, 0.75f, 1};
        long long uni_[3] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 3, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, (long)sizeof dst},
                      })) {
            continue;
        }
        for (int i = 0; i < 3; i++) {
            (equiv((float)dst[i * 4 + 0], 0.25f)) here;
            (equiv((float)dst[i * 4 + 1], 0.5f)) here;
            (equiv((float)dst[i * 4 + 2], 0.75f)) here;
            (equiv((float)dst[i * 4 + 3], 1.0f)) here;
        }
    }
    cleanup_draw(&B);
}

static void test_srcover_fp16(void) {
    umbra_draw_layout lay;
    draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_srcover,
                                   umbra_load_fp16, umbra_store_fp16, &lay),
                  lay);

    for (int bi = 0; bi < 3; bi++) {
        __fp16 dst[4 * 2];
        for (int i = 0; i < 2; i++) {
            dst[i * 4 + 0] = 1;
            dst[i * 4 + 1] = 1;
            dst[i * 4 + 2] = 1;
            dst[i * 4 + 3] = 1;
        }
        float     color[4] = {0, 0.5f, 0, 0.5f};
        long long uni_[3] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 2, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, (long)sizeof dst},
                      })) {
            continue;
        }
        for (int i = 0; i < 2; i++) {
            float r = (float)dst[i * 4 + 0];
            float g = (float)dst[i * 4 + 1];
            float b = (float)dst[i * 4 + 2];
            float a = (float)dst[i * 4 + 3];
            (equiv(r, 0.5f)) here;
            (equiv(g, 1.0f)) here;
            (equiv(b, 0.5f)) here;
            (equiv(a, 1.0f)) here;
        }
    }
    cleanup_draw(&B);
}

static void test_coverage_rect(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, umbra_coverage_rect,
                                                 umbra_blend_srcover, umbra_load_8888,
                                                 umbra_store_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        float     color[4] = {1, 0, 0, 1};
        float     rect[4] = {2.0f, 0.0f, 5.0f, 1.0f};
        long long uni_[5] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        uni_f32(uni, B.lay.coverage, rect, 4);
        if (!run_draw(&B, bi, 8, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, (long)sizeof dst},
                      })) {
            continue;
        }
        for (int i = 0; i < 8; i++) {
            if (i >= 2 && i < 5) {
                ((dst[i] & 0xFF) == 0xFF) here;
                (((dst[i] >> 24) & 0xFF) == 0xFF) here;
            } else {
                (dst[i] == 0) here;
            }
        }
    }
    cleanup_draw(&B);
}

static void test_coverage_rect_scalar(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, umbra_coverage_rect,
                                                 umbra_blend_srcover, umbra_load_8888,
                                                 umbra_store_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4];
        __builtin_memset(dst, 0, sizeof dst);
        float     color[4] = {1, 0, 0, 1};
        float     rect[4] = {1.0f, 0.0f, 3.0f, 1.0f};
        long long uni_[5] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        uni_f32(uni, B.lay.coverage, rect, 4);
        if (!run_draw(&B, bi, 4, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, (long)sizeof dst},
                      })) {
            continue;
        }
        (dst[0] == 0) here;
        ((dst[1] & 0xFF) == 0xFF) here;
        (((dst[1] >> 24) & 0xFF) == 0xFF) here;
        ((dst[2] & 0xFF) == 0xFF) here;
        (((dst[2] >> 24) & 0xFF) == 0xFF) here;
        (dst[3] == 0) here;
    }
    cleanup_draw(&B);
}

static void test_coverage_rect_n9(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, umbra_coverage_rect,
                                                 umbra_blend_srcover, umbra_load_8888,
                                                 umbra_store_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0, sizeof dst);
        float     color[4] = {0, 1, 0, 1};
        float     rect[4] = {3.0f, 0.0f, 7.0f, 1.0f};
        long long uni_[5] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        uni_f32(uni, B.lay.coverage, rect, 4);
        if (!run_draw(&B, bi, 9, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, 9 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 9; i++) {
            if (i >= 3 && i < 7) {
                (((dst[i] >> 8) & 0xFF) == 0xFF) here;
            } else {
                (dst[i] == 0) here;
            }
        }
    }
    cleanup_draw(&B);
}

static void test_coverage_rect_offset(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, umbra_coverage_rect,
                                                 umbra_blend_src, umbra_load_8888,
                                                 umbra_store_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4];
        __builtin_memset(dst, 0, sizeof dst);
        float color[4] = {0, 1, 0, 1};
        float rect[4] = {1.0f, 0.0f, 3.0f, 10.0f};
        long long uni_[5] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        uni_f32(uni, B.lay.coverage, rect, 4);
        if (!run_draw(&B, bi, 4, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, (long)sizeof dst},
                      })) {
            continue;
        }
        (dst[0] == 0) here;
        (((dst[1] >> 8) & 0xFF) == 0xFF) here;
        (((dst[2] >> 8) & 0xFF) == 0xFF) here;
        (dst[3] == 0) here;
    }
    cleanup_draw(&B);
}

static void test_coverage_rect_outside_y(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, umbra_coverage_rect,
                                                 umbra_blend_srcover, umbra_load_8888,
                                                 umbra_store_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4] = {
            0x12345678,
            0x12345678,
            0x12345678,
            0x12345678,
        };
        float     color[4] = {1, 1, 1, 1};
        float     rect[4] = {0.0f, 5.0f, 10.0f, 10.0f};
        long long uni_[5] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        uni_f32(uni, B.lay.coverage, rect, 4);
        if (!run_draw(&B, bi, 4, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, (long)sizeof dst},
                      })) {
            continue;
        }
        for (int i = 0; i < 4; i++) { (dst[i] == 0x12345678) here; }
    }
    cleanup_draw(&B);
}

static void test_no_shader(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(umbra_draw_build(NULL, NULL, umbra_blend_src,
                                                 umbra_load_8888, umbra_store_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4] = {
            0xFFFFFFFF,
            0xFFFFFFFF,
            0xFFFFFFFF,
            0xFFFFFFFF,
        };
        long long uni_[1] = {0};
        char     *uni = (char *)uni_;
        if (!run_draw(&B, bi, 4, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, 4 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 4; i++) { (dst[i] == 0) here; }
    }
    cleanup_draw(&B);
}

static void test_no_blend(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, NULL, NULL,
                                                 umbra_load_8888, umbra_store_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[2] = {0, 0};
        float     color[4] = {1, 0, 1, 1};
        long long uni_[3] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 2, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, 2 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 2; i++) {
            ((dst[i] & 0xFF) == 0xFF) here;
            (((dst[i] >> 8) & 0xFF) == 0x00) here;
            (((dst[i] >> 16) & 0xFF) == 0xFF) here;
            (((dst[i] >> 24) & 0xFF) == 0xFF) here;
        }
    }
    cleanup_draw(&B);
}

static umbra_color gradient_shader(struct umbra_builder *builder, umbra_val x, umbra_val y) {
    (void)y;
    int       fi = umbra_reserve(builder, 2);
    umbra_val w = umbra_uniform_32(builder, (umbra_ptr){0}, fi);
    umbra_val a = umbra_uniform_32(builder, (umbra_ptr){0}, fi + 1);
    umbra_val t = umbra_div_f32(builder, x, w);
    umbra_val zero = umbra_imm_i32(builder, 0);
    return (umbra_color){t, zero, zero, a};
}

static void test_gradient_shader(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(umbra_draw_build(gradient_shader, NULL, umbra_blend_src,
                                                 umbra_load_8888, umbra_store_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[4] = {0, 0, 0, 0};
        float     params[2] = {4.0f, 1.0f};
        long long uni_[2] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, params, 2);
        if (!run_draw(&B, bi, 4, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, (long)sizeof dst},
                      })) {
            continue;
        }
        int r0 = (int)(dst[0] & 0xFF);
        int r3 = (int)(dst[3] & 0xFF);
        (r0 == 0) here;
        (r3 == 191) here;
        for (int i = 0; i < 4; i++) { (((dst[i] >> 24) & 0xFF) == 0xFF) here; }
    }
    cleanup_draw(&B);
}

static void test_multiply_half_alpha(void) {
    umbra_draw_layout lay;
    draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_multiply,
                                   umbra_load_8888, umbra_store_8888, &lay),
                  lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[2] = {0x80FF0000, 0x80FF0000};
        float     color[4] = {1, 0, 0, 0.5f};
        long long uni_[3] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 2, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, 2 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 2; i++) {
            int r = (int)(dst[i] & 0xFF);
            int g = (int)((dst[i] >> 8) & 0xFF);
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
    draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_srcover,
                                   umbra_load_8888, umbra_store_8888, &lay),
                  lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0, sizeof dst);
        float     color[4] = {1, 0, 0, 0.5f};
        long long uni_[3] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 9, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, 9 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 9; i++) {
            int r = (int)(dst[i] & 0xFF);
            int a = (int)((dst[i] >> 24) & 0xFF);
            (r == 0xFF) here;
            (a == 128) here;
        }
    }
    cleanup_draw(&B);
}

static void test_full_pipeline(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, umbra_coverage_rect,
                                                 umbra_blend_srcover, umbra_load_8888,
                                                 umbra_store_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0, sizeof dst);
        float     color[4] = {1, 0, 0, 1};
        float     rect[4] = {2.0f, 0.0f, 7.0f, 1.0f};
        long long uni_[5] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        uni_f32(uni, B.lay.coverage, rect, 4);
        if (!run_draw(&B, bi, 9, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, 9 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 9; i++) {
            if (i >= 2 && i < 7) {
                int r = (int)(dst[i] & 0xFF);
                int g = (int)((dst[i] >> 8) & 0xFF);
                int b = (int)((dst[i] >> 16) & 0xFF);
                int a = (int)((dst[i] >> 24) & 0xFF);
                (r == 0xFF) here;
                (g == 0) here;
                (b == 0) here;
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
    draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_src,
                                                 umbra_load_fp16, umbra_store_fp16, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        if (bi == 1) { continue; }
        __fp16 dst[4 * 9];
        __builtin_memset(dst, 0, sizeof dst);
        float     color[4] = {0.125f, 0.25f, 0.5f, 1};
        long long uni_[3] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 9, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, (long)sizeof dst},
                      })) {
            continue;
        }
        for (int i = 0; i < 9; i++) {
            (equiv((float)dst[i * 4 + 0], 0.125f)) here;
            (equiv((float)dst[i * 4 + 1], 0.25f)) here;
            (equiv((float)dst[i * 4 + 2], 0.5f)) here;
            (equiv((float)dst[i * 4 + 3], 1.0f)) here;
        }
    }
    cleanup_draw(&B);
}

static void test_coverage_rect_white_dst(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, umbra_coverage_rect,
                                                 umbra_blend_srcover, umbra_load_8888,
                                                 umbra_store_8888, &lay),
                                lay);

    typedef struct {
        int   n;
        float x0;
        float x1;
    } rect_case;
    rect_case cases[] = {
        {16, 4.0f, 12.0f},
        {24, 4.0f, 20.0f},
    };
    int ncases = (int)(sizeof cases / sizeof cases[0]);

    for (int ci = 0; ci < ncases; ci++) {
        rect_case rc = cases[ci];
        for (int bi = 0; bi < 3; bi++) {
            uint32_t dst[24];
            __builtin_memset(dst, 0xFF, (size_t)rc.n * 4);
            float color[4] = {1, 0, 0, 1};
            float rect[4] = {
                rc.x0,
                0.0f,
                rc.x1,
                1.0f,
            };
            long long uni_[5] = {0};
            char     *uni = (char *)uni_;
            uni_f32(uni, B.lay.shader, color, 4);
            uni_f32(uni, B.lay.coverage, rect, 4);
            if (!run_draw(&B, bi, rc.n, 1,
                          (umbra_buf[]){
                              {uni, -(long)B.lay.uni_len},
                              {dst, (long)(rc.n * 4)},
                          })) {
                continue;
            }
            for (int i = 0; i < rc.n; i++) {
                if (i >= 4 && i < (int)rc.x1) {
                    ((dst[i] & 0xFF) == 0xFF) here;
                    (((dst[i] >> 24) & 0xFF) == 0xFF) here;
                } else {
                    (dst[i] == 0xFFFFFFFF) here;
                }
            }
        }
    }
    cleanup_draw(&B);
}

static void test_coverage_bitmap(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, umbra_coverage_bitmap,
                                                 umbra_blend_srcover, umbra_load_8888,
                                                 umbra_store_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        float     color[4] = {1, 1, 1, 1};
        uint16_t  cov[8] = {0, 128, 255, 0, 0, 0, 0, 0};
        long long uni_[5] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        uni_ptr(uni, B.lay.coverage, cov, (long)sizeof cov);
        if (!run_draw(&B, bi, 8, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, (long)sizeof dst},
                      })) {
            continue;
        }
        (dst[0] == 0) here;
        ((dst[1] & 0xff) >= 120 && (dst[1] & 0xff) <= 136) here;
        ((dst[2] & 0xff) >= 0xfe) here;
        (dst[3] == 0) here;
    }
    cleanup_draw(&B);
}

static void test_coverage_sdf(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, umbra_coverage_sdf,
                                                 umbra_blend_srcover, umbra_load_8888,
                                                 umbra_store_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        float     color[4] = {1, 1, 1, 1};
        uint16_t  cov[8] = {0, 100, 128, 200, 255, 0, 0, 0};
        long long uni_[5] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        uni_ptr(uni, B.lay.coverage, cov, (long)sizeof cov);
        if (!run_draw(&B, bi, 8, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, (long)sizeof dst},
                      })) {
            continue;
        }
        (dst[0] == 0) here;
        ((dst[4] & 0xff) >= 0xfe) here;
    }
    cleanup_draw(&B);
}

static void test_coverage_bitmap_matrix(void) {
    umbra_draw_layout lay;
    draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_solid, umbra_coverage_bitmap_matrix,
                                   umbra_blend_srcover, umbra_load_8888, umbra_store_8888,
                                   &lay),
                  lay);

    int ptr_off = (B.lay.coverage + 11 * 4 + 7) & ~7;
    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        float    color[4] = {1, 1, 1, 1};
        uint16_t bmp[8] = {0, 0, 255, 0, 0, 0, 0, 0};
        float    mat[11] = {
            1, 0, 0, 0, 1, 0, 0, 0, 1, 8, 1,
        };
        long long uni_[11] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        uni_f32(uni, B.lay.coverage, mat, 11);
        uni_ptr(uni, ptr_off, bmp, (long)sizeof bmp);
        if (!run_draw(&B, bi, 8, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, (long)sizeof dst},
                      })) {
            continue;
        }
        (dst[0] == 0) here;
        ((dst[2] & 0xff) >= 0xfe) here;
        (dst[3] == 0) here;
    }
    cleanup_draw(&B);
}

static void test_coverage_bitmap_matrix_oob(void) {
    umbra_draw_layout lay;
    draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_solid, umbra_coverage_bitmap_matrix,
                                   umbra_blend_srcover, umbra_load_8888, umbra_store_8888,
                                   &lay),
                  lay);

    int ptr_off = (B.lay.coverage + 11 * 4 + 7) & ~7;
    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        float    color[4] = {1, 1, 1, 1};
        uint16_t bmp[4] = {255, 0, 0, 0};
        float    mat[11] = {
            1, 0, 0, 0, 1, 0, 0.001f, 0, 1, 2, 2,
        };
        long long uni_[11] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color, 4);
        uni_f32(uni, B.lay.coverage, mat, 11);
        uni_ptr(uni, ptr_off, bmp, (long)sizeof bmp);
        if (!run_draw(&B, bi, 8, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, (long)sizeof dst},
                      })) {
            continue;
        }
        ((dst[0] & 0xff) == 0xFF) here;
    }
    cleanup_draw(&B);
}

static void test_linear_2(void) {
    umbra_draw_layout lay;
    draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_linear_2, NULL, umbra_blend_src, NULL,
                                   umbra_store_8888, &lay),
                  lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[4] = {0};
        float     colors[8] = {1, 0, 0, 1, 0, 0, 1, 1};
        float     params[3] = {0.25f, 0, 0};
        long long uni_[7] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, params, 3);
        uni_f32(uni, B.lay.shader + 12, colors, 8);
        if (!run_draw(&B, bi, 4, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, (long)sizeof dst},
                      })) {
            continue;
        }
        ((dst[0] & 0xff) == 0xFF) here;
        (((dst[0] >> 16) & 0xff) == 0) here;
        ((dst[3] & 0xff) <= 66) here;
        (((dst[3] >> 16) & 0xff) >= 189) here;
        for (int i = 0; i < 4; i++) { (((dst[i] >> 24) & 0xff) == 0xFF) here; }
    }
    cleanup_draw(&B);
}

static void test_radial_2(void) {
    umbra_draw_layout lay;
    draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_radial_2, NULL, umbra_blend_src, NULL,
                                   umbra_store_8888, &lay),
                  lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[1] = {0};
        float     colors[8] = {1, 1, 1, 1, 0, 0, 0, 1};
        float     params[3] = {0, 0, 0.1f};
        long long uni_[7] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, params, 3);
        uni_f32(uni, B.lay.shader + 12, colors, 8);
        if (!run_draw(&B, bi, 1, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, 4},
                      })) {
            continue;
        }
        ((dst[0] & 0xff) == 0xFF) here;
        (((dst[0] >> 24) & 0xff) == 0xFF) here;
    }
    cleanup_draw(&B);
}

static void test_linear_grad(void) {
    float const stop_colors[][4] = {
        {1, 0, 0, 1},
        {0, 1, 0, 1},
        {0, 0, 1, 1},
    };
    float lut[256 * 4];
    umbra_gradient_lut_even(lut, 256, 3, stop_colors);

    umbra_draw_layout lay;
    draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_linear_grad, NULL, umbra_blend_src, NULL,
                                   umbra_store_8888, &lay),
                  lay);

    int lut_off = (B.lay.shader + 16 + 7) & ~7;
    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[8] = {0};
        float     params[4] = {0.125f, 0, 0, 256};
        long long uni_[5] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, params, 4);
        uni_ptr(uni, lut_off, lut, (long)sizeof lut);
        if (!run_draw(&B, bi, 8, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, (long)sizeof dst},
                      })) {
            continue;
        }
        ((dst[0] & 0xff) == 0xFF) here;
        (((dst[0] >> 8) & 0xff) == 0) here;
        (((dst[7] >> 16) & 0xff) >= 180) here;
    }
    cleanup_draw(&B);
}

static void test_radial_grad(void) {
    float const stop_colors[][4] = {
        {1, 0, 0, 1},
        {1, 1, 0, 1},
        {0, 1, 0, 1},
        {0, 0, 1, 1},
    };
    float lut[64 * 4];
    umbra_gradient_lut_even(lut, 64, 4, stop_colors);

    umbra_draw_layout lay;
    draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_radial_grad, NULL, umbra_blend_src, NULL,
                                   umbra_store_8888, &lay),
                  lay);

    int lut_off = (B.lay.shader + 16 + 7) & ~7;
    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[1] = {0};
        float     params[4] = {0, 0, 0.1f, 64};
        long long uni_[5] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, params, 4);
        uni_ptr(uni, lut_off, lut, (long)sizeof lut);
        if (!run_draw(&B, bi, 1, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, 4},
                      })) {
            continue;
        }
        ((dst[0] & 0xff) == 0xFF) here;
        (((dst[0] >> 16) & 0xff) == 0) here;
    }
    cleanup_draw(&B);
}

static void test_gradient_lut_nonuniform(void) {
    float const stop_colors[][4] = {
        {1, 0, 0, 1},
        {0, 1, 0, 1},
        {0, 0, 1, 1},
    };
    float positions[] = {0, 0.2f, 1.0f};
    float lut[64 * 4];
    umbra_gradient_lut(lut, 64, 3, positions, stop_colors);

    umbra_draw_layout lay;
    draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_linear_grad, NULL, umbra_blend_src, NULL,
                                   umbra_store_8888, &lay),
                  lay);

    int lut_off = (B.lay.shader + 16 + 7) & ~7;
    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[8] = {0};
        float     params[4] = {0.125f, 0, 0, 64};
        long long uni_[5] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, params, 4);
        uni_ptr(uni, lut_off, lut, (long)sizeof lut);
        if (!run_draw(&B, bi, 8, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, (long)sizeof dst},
                      })) {
            continue;
        }
        ((dst[0] & 0xff) == 0xFF) here;
        (((dst[7] >> 16) & 0xff) >= 180) here;
    }
    cleanup_draw(&B);
}

static void test_transfer_lut(void) {
    float inv[256], fwd[256];
    umbra_transfer_lut_invert(inv, &umbra_transfer_srgb);
    umbra_transfer_lut_apply(fwd, &umbra_transfer_srgb);

    (equiv(inv[0], 0.0f)) here;
    (inv[255] >= 0.999f && inv[255] <= 1.001f) here;
    (inv[128] > inv[64]) here;

    (equiv(fwd[0], 0.0f)) here;
    (fwd[255] >= 0.999f && fwd[255] <= 1.001f) here;
    (fwd[128] > fwd[64]) here;

    for (int i = 0; i < 256; i++) {
        (inv[i] >= 0.0f && inv[i] <= 1.0f) here;
        (fwd[i] >= 0.0f && fwd[i] <= 1.0f) here;
    }
}

static umbra_color srgb_invert_shader(struct umbra_builder *builder, umbra_val x,
                                      umbra_val y) {
    (void)x;
    (void)y;
    int       fi = umbra_reserve(builder, 4);
    umbra_val r = umbra_uniform_32(builder, (umbra_ptr){0}, fi);
    umbra_val g = umbra_uniform_32(builder, (umbra_ptr){0}, fi + 1);
    umbra_val b = umbra_uniform_32(builder, (umbra_ptr){0}, fi + 2);
    umbra_val a = umbra_uniform_32(builder, (umbra_ptr){0}, fi + 3);
    umbra_color c = {r, g, b, a};
    return umbra_transfer_invert(builder, c, &umbra_transfer_srgb);
}

static void test_transfer_invert(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(umbra_draw_build(srgb_invert_shader, NULL, umbra_blend_src,
                                                 NULL, umbra_store_fp16, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        __fp16 dst[4 * 2];
        __builtin_memset(dst, 0, sizeof dst);
        float     color0[4] = {0.5f, 0.5f, 0.5f, 1};
        long long uni_[3] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color0, 4);
        if (!run_draw(&B, bi, 2, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, (long)sizeof dst},
                      })) {
            continue;
        }
        for (int i = 0; i < 2; i++) {
            float r = (float)dst[i * 4 + 0];
            float a = (float)dst[i * 4 + 3];
            equiv(r, 0.2141113281f) here;
            (equiv(a, 1.0f)) here;
        }
    }
    cleanup_draw(&B);
}

static umbra_color srgb_apply_shader(struct umbra_builder *builder, umbra_val x,
                                     umbra_val y) {
    (void)x;
    (void)y;
    int       fi = umbra_reserve(builder, 4);
    umbra_val r = umbra_uniform_32(builder, (umbra_ptr){0}, fi);
    umbra_val g = umbra_uniform_32(builder, (umbra_ptr){0}, fi + 1);
    umbra_val b = umbra_uniform_32(builder, (umbra_ptr){0}, fi + 2);
    umbra_val a = umbra_uniform_32(builder, (umbra_ptr){0}, fi + 3);
    umbra_color c = {r, g, b, a};
    return umbra_transfer_apply(builder, c, &umbra_transfer_srgb);
}

static void test_transfer_apply(void) {
    umbra_draw_layout lay;
    draw_backends B = make_draw(umbra_draw_build(srgb_apply_shader, NULL, umbra_blend_src,
                                                 NULL, umbra_store_fp16, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        __fp16 dst[4 * 2];
        __builtin_memset(dst, 0, sizeof dst);
        float     color0[4] = {0.5f, 0.5f, 0.5f, 1};
        long long uni_[3] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color0, 4);
        if (!run_draw(&B, bi, 2, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, (long)sizeof dst},
                      })) {
            continue;
        }
        for (int i = 0; i < 2; i++) {
            float r = (float)dst[i * 4 + 0];
            float a = (float)dst[i * 4 + 3];
            (r >= 0.72f && r <= 0.75f) here;
            (a >= 0.99f && a <= 1.01f) here;
        }
    }
    cleanup_draw(&B);
}

static umbra_color srgb_roundtrip_shader(struct umbra_builder *builder, umbra_val x,
                                         umbra_val y) {
    (void)x;
    (void)y;
    int       fi = umbra_reserve(builder, 4);
    umbra_val r = umbra_uniform_32(builder, (umbra_ptr){0}, fi);
    umbra_val g = umbra_uniform_32(builder, (umbra_ptr){0}, fi + 1);
    umbra_val b = umbra_uniform_32(builder, (umbra_ptr){0}, fi + 2);
    umbra_val a = umbra_uniform_32(builder, (umbra_ptr){0}, fi + 3);
    umbra_color c = {r, g, b, a};
    c = umbra_transfer_apply(builder, c, &umbra_transfer_srgb);
    c = umbra_transfer_invert(builder, c, &umbra_transfer_srgb);
    return c;
}

static void test_transfer_roundtrip(void) {
    umbra_draw_layout lay;
    draw_backends     B =
        make_draw(umbra_draw_build(srgb_roundtrip_shader, NULL, umbra_blend_src, NULL,
                                   umbra_store_fp16, &lay),
                  lay);

    for (int bi = 0; bi < 3; bi++) {
        __fp16 dst[4 * 4];
        __builtin_memset(dst, 0, sizeof dst);
        float     color0[4] = {0.25f, 0.5f, 0.75f, 1};
        long long uni_[3] = {0};
        char     *uni = (char *)uni_;
        uni_f32(uni, B.lay.shader, color0, 4);
        if (!run_draw(&B, bi, 4, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, (long)sizeof dst},
                      })) {
            continue;
        }
        for (int i = 0; i < 4; i++) {
            float r = (float)dst[i * 4 + 0];
            float g = (float)dst[i * 4 + 1];
            float b = (float)dst[i * 4 + 2];
            float a = (float)dst[i * 4 + 3];
            equiv(r, 0.2498779297f) here;
            equiv(g, 0.5f) here;
            equiv(b, 0.75f) here;
            equiv(a, 1.0f) here;
        }
    }
    cleanup_draw(&B);
}

static umbra_shader_fn ss_inner_;
static int             ss_n_;
static umbra_color ss_shader_(struct umbra_builder *builder, umbra_val x, umbra_val y) {
    return umbra_supersample(builder, x, y, ss_inner_, ss_n_);
}

static void test_supersample(void) {
    ss_inner_ = umbra_shader_solid;
    ss_n_ = 4;
    umbra_draw_layout lay;
    draw_backends B = make_draw(umbra_draw_build(ss_shader_, NULL, umbra_blend_src,
                                                 umbra_load_8888, umbra_store_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[4] = {0};
        long long uni_[3] = {0};
        char     *uni = (char *)uni_;
        float     color[4] = {1, 0, 0, 1};
        uni_f32(uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 4, 1,
                      (umbra_buf[]){
                          {uni, -(long)B.lay.uni_len},
                          {dst, 4 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 4; i++) {
            ((dst[i] & 0xFF) == 0xFF) here;
            (((dst[i] >> 24) & 0xFF) == 0xFF) here;
        }
    }
    cleanup_draw(&B);
}

#if !defined(__wasm__)
#include <sys/mman.h>
#include <unistd.h>
#endif

static void test_page_aligned_buffer(void) {
#if defined(__wasm__)
    return;
#else
    // Test Metal zero-copy path (page-aligned) and copy path (offset by 4).
    umbra_draw_layout lay;
    draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_src,
                                                 umbra_load_8888, umbra_store_8888, &lay),
                                lay);
    enum { N = 64 };
    long      pgsz = sysconf(_SC_PAGESIZE);
    size_t    alloc = (N * 4 + (size_t)pgsz - 1) & ~((size_t)pgsz - 1);
    void     *page = mmap(NULL, alloc + (size_t)pgsz, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANON, -1, 0);
    uint32_t *aligned = page;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-align"
    uint32_t *offset = (uint32_t *)((char *)page + 4);
#pragma clang diagnostic pop

    float     color[4] = {0, 1, 0, 1};
    long long uni_[3] = {0};
    char     *uni = (char *)uni_;
    uni_f32(uni, B.lay.shader, color, 4);

    for (int bi = 0; bi < 3; bi++) {
        __builtin_memset(aligned, 0, N * 4);
        if (!run_draw(&B, bi, N, 1, (umbra_buf[]){
            {uni, -(long)B.lay.uni_len},
            {aligned, N * 4},
        })) { continue; }
        for (int i = 0; i < N; i++) { (aligned[i] == 0xFF00FF00u) here; }

        __builtin_memset(offset, 0, N * 4);
        if (!run_draw(&B, bi, N, 1, (umbra_buf[]){
            {uni, -(long)B.lay.uni_len},
            {offset, N * 4},
        })) { continue; }
        for (int i = 0; i < N; i++) { (offset[i] == 0xFF00FF00u) here; }
    }
    munmap(page, alloc + (size_t)pgsz);
    cleanup_draw(&B);
#endif
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
    test_coverage_rect_offset();
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
    test_page_aligned_buffer();

    return 0;
}
