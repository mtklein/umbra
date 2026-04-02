#include "../include/umbra_draw.h"
#include "test.h"
#include <stdint.h>

struct draw_backends {
    struct test_backends     tb;
    struct umbra_draw_layout lay;
};


static struct draw_backends make_draw(struct umbra_builder *builder, struct umbra_draw_layout lay) {
    struct umbra_basic_block *bb = umbra_basic_block(builder);
    umbra_builder_free(builder);
    struct draw_backends B = {
        test_backends_make(bb),
        lay,
    };
    umbra_basic_block_free(bb);
    return B;
}
static _Bool run_draw(struct draw_backends *B, int b, int w, int h, struct umbra_buf buf[]) {
    return test_backends_run(&B->tb, b, w, h, buf);
}
static void cleanup_draw(struct draw_backends *B) {
    test_backends_free(&B->tb);
    if (B->lay.uni) { free(B->lay.uni->data); free(B->lay.uni); }
}

static void test_solid_src(void) {
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_src,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4] = {
            0xFFFFFFFF,
            0xFFFFFFFF,
            0xFFFFFFFF,
            0xFFFFFFFF,
        };
        float     color[4] = {1, 0, 0, 1};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 4, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=4 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 4; i++) {
            (dst[i] & 0xFF) == 0xFF here;
            ((dst[i] >> 8) & 0xFF) == 0x00 here;
            ((dst[i] >> 16) & 0xFF) == 0x00 here;
            ((dst[i] >> 24) & 0xFF) == 0xFF here;
        }
    }
    cleanup_draw(&B);
}

static void test_solid_src_n1(void) {
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_src,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[1] = {0xFFFFFFFF};
        float     color[4] = {0, 0, 1, 1};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 1, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=4},
                      })) {
            continue;
        }
        (dst[0] & 0xFF) == 0x00 here;
        ((dst[0] >> 8) & 0xFF) == 0x00 here;
        ((dst[0] >> 16) & 0xFF) == 0xFF here;
        ((dst[0] >> 24) & 0xFF) == 0xFF here;
    }
    cleanup_draw(&B);
}

static void test_solid_src_n9(void) {
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_src,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0xFF, sizeof dst);
        float     color[4] = {0, 1, 0, 1};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 9, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=9 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 9; i++) {
            ((dst[i] >> 8) & 0xFF) == 0xFF here;
            ((dst[i] >> 24) & 0xFF) == 0xFF here;
        }
    }
    cleanup_draw(&B);
}

static void test_solid_src_n16(void) {
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_src,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[16];
        __builtin_memset(dst, 0, sizeof dst);
        float     color[4] = {1, 1, 1, 1};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 16, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=16 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 16; i++) { (dst[i] == 0xFFFFFFFF) here; }
    }
    cleanup_draw(&B);
}

static void test_srcover_8888(void) {
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_srcover,
                                   umbra_fmt_8888, &lay),
                  lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[2] = {0xFFFFFFFF, 0xFFFFFFFF};
        float     color[4] = {0, 0.5f, 0, 0.5f};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 2, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=2 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 2; i++) {
            int r = (int)(dst[i] & 0xFF);
            int g = (int)((dst[i] >> 8) & 0xFF);
            int b = (int)((dst[i] >> 16) & 0xFF);
            int a = (int)((dst[i] >> 24) & 0xFF);
            r == 128 here;
            g == 255 here;
            b == 128 here;
            a == 255 here;
        }
    }
    cleanup_draw(&B);
}

static void test_dstover_8888(void) {
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_dstover,
                                   umbra_fmt_8888, &lay),
                  lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[2] = {0xFFFFFFFF, 0xFFFFFFFF};
        float     color[4] = {1, 0, 0, 1};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 2, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=2 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 2; i++) { (dst[i] == 0xFFFFFFFF) here; }
    }
    cleanup_draw(&B);
}

static void test_dstover_transparent(void) {
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_dstover,
                                   umbra_fmt_8888, &lay),
                  lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[2] = {0, 0};
        float     color[4] = {1, 0, 0, 1};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 2, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=2 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 2; i++) {
            (dst[i] & 0xFF) == 0xFF here;
            ((dst[i] >> 24) & 0xFF) == 0xFF here;
        }
    }
    cleanup_draw(&B);
}

static void test_multiply_8888(void) {
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_multiply,
                                   umbra_fmt_8888, &lay),
                  lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[2] = {0xFF804020, 0xFF804020};
        float     color[4] = {1, 1, 1, 1};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 2, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=2 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 2; i++) {
            int r = (int)(dst[i] & 0xFF);
            int g = (int)((dst[i] >> 8) & 0xFF);
            int b = (int)((dst[i] >> 16) & 0xFF);
            int a = (int)((dst[i] >> 24) & 0xFF);
            r == 0x20 here;
            g == 0x40 here;
            b == 0x80 here;
            a == 0xFF here;
        }
    }
    cleanup_draw(&B);
}

static void test_solid_src_fp16(void) {
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_src,
                                                 umbra_fmt_fp16, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        __fp16 dst[4 * 3];
        __builtin_memset(dst, 0, sizeof dst);
        float     color[4] = {0.25f, 0.5f, 0.75f, 1};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 3, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=sizeof dst},
                      })) {
            continue;
        }
        for (int i = 0; i < 3; i++) {
            equiv((float)dst[i * 4 + 0], 0.25f) here;
            equiv((float)dst[i * 4 + 1], 0.5f) here;
            equiv((float)dst[i * 4 + 2], 0.75f) here;
            equiv((float)dst[i * 4 + 3], 1.0f) here;
        }
    }
    cleanup_draw(&B);
}

static void test_srcover_fp16(void) {
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_srcover,
                                   umbra_fmt_fp16, &lay),
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
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 2, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=sizeof dst},
                      })) {
            continue;
        }
        for (int i = 0; i < 2; i++) {
            float r = (float)dst[i * 4 + 0];
            float g = (float)dst[i * 4 + 1];
            float b = (float)dst[i * 4 + 2];
            float a = (float)dst[i * 4 + 3];
            equiv(r, 0.5f) here;
            equiv(g, 1.0f) here;
            equiv(b, 0.5f) here;
            equiv(a, 1.0f) here;
        }
    }
    cleanup_draw(&B);
}

static void test_coverage_rect(void) {
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, umbra_coverage_rect,
                                                 umbra_blend_srcover,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        float     color[4] = {1, 0, 0, 1};
        float     rect[4] = {2.0f, 0.0f, 5.0f, 1.0f};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.coverage, rect, 4);
        if (!run_draw(&B, bi, 8, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=sizeof dst},
                      })) {
            continue;
        }
        for (int i = 0; i < 8; i++) {
            if (i >= 2 && i < 5) {
                (dst[i] & 0xFF) == 0xFF here;
                ((dst[i] >> 24) & 0xFF) == 0xFF here;
            } else {
                dst[i] == 0 here;
            }
        }
    }
    cleanup_draw(&B);
}

static void test_coverage_rect_scalar(void) {
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, umbra_coverage_rect,
                                                 umbra_blend_srcover,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4];
        __builtin_memset(dst, 0, sizeof dst);
        float     color[4] = {1, 0, 0, 1};
        float     rect[4] = {1.0f, 0.0f, 3.0f, 1.0f};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.coverage, rect, 4);
        if (!run_draw(&B, bi, 4, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=sizeof dst},
                      })) {
            continue;
        }
        dst[0] == 0 here;
        (dst[1] & 0xFF) == 0xFF here;
        ((dst[1] >> 24) & 0xFF) == 0xFF here;
        (dst[2] & 0xFF) == 0xFF here;
        ((dst[2] >> 24) & 0xFF) == 0xFF here;
        dst[3] == 0 here;
    }
    cleanup_draw(&B);
}

static void test_coverage_rect_n9(void) {
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, umbra_coverage_rect,
                                                 umbra_blend_srcover,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0, sizeof dst);
        float     color[4] = {0, 1, 0, 1};
        float     rect[4] = {3.0f, 0.0f, 7.0f, 1.0f};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.coverage, rect, 4);
        if (!run_draw(&B, bi, 9, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=9 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 9; i++) {
            if (i >= 3 && i < 7) {
                ((dst[i] >> 8) & 0xFF) == 0xFF here;
            } else {
                dst[i] == 0 here;
            }
        }
    }
    cleanup_draw(&B);
}

static void test_coverage_rect_offset(void) {
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, umbra_coverage_rect,
                                                 umbra_blend_src,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4];
        __builtin_memset(dst, 0, sizeof dst);
        float color[4] = {0, 1, 0, 1};
        float rect[4] = {1.0f, 0.0f, 3.0f, 10.0f};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.coverage, rect, 4);
        if (!run_draw(&B, bi, 4, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=sizeof dst},
                      })) {
            continue;
        }
        dst[0] == 0 here;
        ((dst[1] >> 8) & 0xFF) == 0xFF here;
        ((dst[2] >> 8) & 0xFF) == 0xFF here;
        dst[3] == 0 here;
    }
    cleanup_draw(&B);
}

static void test_coverage_rect_outside_y(void) {
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, umbra_coverage_rect,
                                                 umbra_blend_srcover,
                                                 umbra_fmt_8888, &lay),
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
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.coverage, rect, 4);
        if (!run_draw(&B, bi, 4, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=sizeof dst},
                      })) {
            continue;
        }
        for (int i = 0; i < 4; i++) { (dst[i] == 0x12345678) here; }
    }
    cleanup_draw(&B);
}

static void test_no_shader(void) {
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_build(NULL, NULL, umbra_blend_src,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[4] = {
            0xFFFFFFFF,
            0xFFFFFFFF,
            0xFFFFFFFF,
            0xFFFFFFFF,
        };
        if (!run_draw(&B, bi, 4, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=4 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 4; i++) { (dst[i] == 0) here; }
    }
    cleanup_draw(&B);
}

static void test_no_blend(void) {
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, NULL, NULL,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[2] = {0, 0};
        float     color[4] = {1, 0, 1, 1};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 2, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=2 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 2; i++) {
            (dst[i] & 0xFF) == 0xFF here;
            ((dst[i] >> 8) & 0xFF) == 0x00 here;
            ((dst[i] >> 16) & 0xFF) == 0xFF here;
            ((dst[i] >> 24) & 0xFF) == 0xFF here;
        }
    }
    cleanup_draw(&B);
}

static umbra_color gradient_shader(struct umbra_builder *builder, struct umbra_uniforms *u, umbra_val x, umbra_val y) {
    (void)y;
    size_t fi = umbra_uniforms_reserve_f32(u, 2);
    umbra_val w = umbra_uniform_32(builder, (umbra_ptr){0}, fi);
    umbra_val a = umbra_uniform_32(builder, (umbra_ptr){0}, fi + 4);
    umbra_val t = umbra_div_f32(builder, x, w);
    umbra_val zero = umbra_imm_i32(builder, 0);
    return (umbra_color){t, zero, zero, a};
}

static void test_gradient_shader(void) {
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_build(gradient_shader, NULL, umbra_blend_src,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[4] = {0};
        float     params[2] = {4.0f, 1.0f};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, params, 2);
        if (!run_draw(&B, bi, 4, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=sizeof dst},
                      })) {
            continue;
        }
        int r0 = (int)(dst[0] & 0xFF);
        int r3 = (int)(dst[3] & 0xFF);
        r0 == 0 here;
        r3 == 191 here;
        for (int i = 0; i < 4; i++) { (((dst[i] >> 24) & 0xFF) == 0xFF) here; }
    }
    cleanup_draw(&B);
}

static void test_multiply_half_alpha(void) {
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_multiply,
                                   umbra_fmt_8888, &lay),
                  lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[2] = {0x80FF0000, 0x80FF0000};
        float     color[4] = {1, 0, 0, 0.5f};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 2, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=2 * 4},
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
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_srcover,
                                   umbra_fmt_8888, &lay),
                  lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0, sizeof dst);
        float     color[4] = {1, 0, 0, 0.5f};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 9, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=9 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 9; i++) {
            int r = (int)(dst[i] & 0xFF);
            int a = (int)((dst[i] >> 24) & 0xFF);
            r == 0xFF here;
            a == 128 here;
        }
    }
    cleanup_draw(&B);
}

static void test_full_pipeline(void) {
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, umbra_coverage_rect,
                                                 umbra_blend_srcover,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0, sizeof dst);
        float     color[4] = {1, 0, 0, 1};
        float     rect[4] = {2.0f, 0.0f, 7.0f, 1.0f};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.coverage, rect, 4);
        if (!run_draw(&B, bi, 9, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=9 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 9; i++) {
            if (i >= 2 && i < 7) {
                int r = (int)(dst[i] & 0xFF);
                int g = (int)((dst[i] >> 8) & 0xFF);
                int b = (int)((dst[i] >> 16) & 0xFF);
                int a = (int)((dst[i] >> 24) & 0xFF);
                r == 0xFF here;
                g == 0 here;
                b == 0 here;
                a == 0xFF here;
            } else {
                dst[i] == 0 here;
            }
        }
    }
    cleanup_draw(&B);
}

static void test_solid_src_fp16_n9(void) {
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_src,
                                                 umbra_fmt_fp16, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        __fp16 dst[4 * 9];
        __builtin_memset(dst, 0, sizeof dst);
        float     color[4] = {0.125f, 0.25f, 0.5f, 1};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 9, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=sizeof dst},
                      })) {
            continue;
        }
        for (int i = 0; i < 9; i++) {
            equiv((float)dst[i * 4 + 0], 0.125f) here;
            equiv((float)dst[i * 4 + 1], 0.25f) here;
            equiv((float)dst[i * 4 + 2], 0.5f) here;
            equiv((float)dst[i * 4 + 3], 1.0f) here;
        }
    }
    cleanup_draw(&B);
}

static void test_coverage_rect_white_dst(void) {
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, umbra_coverage_rect,
                                                 umbra_blend_srcover,
                                                 umbra_fmt_8888, &lay),
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
            umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
            umbra_uniforms_fill_f32(B.lay.uni, B.lay.coverage, rect, 4);
            if (!run_draw(&B, bi, rc.n, 1,
                          (struct umbra_buf[]){
                              (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                              {.ptr=dst, .sz=(size_t)(rc.n * 4)},
                          })) {
                continue;
            }
            for (int i = 0; i < rc.n; i++) {
                if (i >= 4 && i < (int)rc.x1) {
                    (dst[i] & 0xFF) == 0xFF here;
                    ((dst[i] >> 24) & 0xFF) == 0xFF here;
                } else {
                    dst[i] == 0xFFFFFFFF here;
                }
            }
        }
    }
    cleanup_draw(&B);
}

static void test_coverage_bitmap(void) {
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, umbra_coverage_bitmap,
                                                 umbra_blend_srcover,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        float     color[4] = {1, 1, 1, 1};
        uint16_t  cov[8] = {0, 128, 255, 0, 0, 0, 0, 0};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        umbra_uniforms_fill_ptr(B.lay.uni, B.lay.coverage, (struct umbra_buf){.ptr=cov, .sz=sizeof cov});
        if (!run_draw(&B, bi, 8, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=sizeof dst},
                      })) {
            continue;
        }
        dst[0] == 0 here;
        (dst[1] & 0xff) == 128 here;
        (dst[2] & 0xff) == 0xff here;
        dst[3] == 0 here;
    }
    cleanup_draw(&B);
}

static void test_coverage_sdf(void) {
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, umbra_coverage_sdf,
                                                 umbra_blend_srcover,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        float     color[4] = {1, 1, 1, 1};
        uint16_t  cov[8] = {0, 100, 128, 200, 255, 0, 0, 0};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        umbra_uniforms_fill_ptr(B.lay.uni, B.lay.coverage, (struct umbra_buf){.ptr=cov, .sz=sizeof cov});
        if (!run_draw(&B, bi, 8, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=sizeof dst},
                      })) {
            continue;
        }
        dst[0] == 0 here;
        (dst[4] & 0xff) == 0xff here;
    }
    cleanup_draw(&B);
}

static void test_coverage_bitmap_matrix(void) {
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_solid, umbra_coverage_bitmap_matrix,
                                   umbra_blend_srcover, umbra_fmt_8888, &lay),
                  lay);

    size_t ptr_off = (B.lay.coverage + 11 * 4 + 7) & ~(size_t)7;
    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        float    color[4] = {1, 1, 1, 1};
        uint16_t bmp[8] = {0, 0, 255, 0, 0, 0, 0, 0};
        float    mat[11] = {
            1, 0, 0, 0, 1, 0, 0, 0, 1, 8, 1,
        };
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.coverage, mat, 11);
        umbra_uniforms_fill_ptr(B.lay.uni, ptr_off, (struct umbra_buf){.ptr=bmp, .sz=sizeof bmp});
        if (!run_draw(&B, bi, 8, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=sizeof dst},
                      })) {
            continue;
        }
        dst[0] == 0 here;
        (dst[2] & 0xff) == 0xff here;
        dst[3] == 0 here;
    }
    cleanup_draw(&B);
}

static void test_coverage_bitmap_matrix_oob(void) {
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_solid, umbra_coverage_bitmap_matrix,
                                   umbra_blend_srcover, umbra_fmt_8888, &lay),
                  lay);

    size_t ptr_off = (B.lay.coverage + 11 * 4 + 7) & ~(size_t)7;
    for (int bi = 0; bi < 3; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        float    color[4] = {1, 1, 1, 1};
        uint16_t bmp[4] = {255, 0, 0, 0};
        float    mat[11] = {
            1, 0, 0, 0, 1, 0, 0.001f, 0, 1, 2, 2,
        };
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.coverage, mat, 11);
        umbra_uniforms_fill_ptr(B.lay.uni, ptr_off, (struct umbra_buf){.ptr=bmp, .sz=sizeof bmp});
        if (!run_draw(&B, bi, 8, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=sizeof dst},
                      })) {
            continue;
        }
        (dst[0] & 0xff) == 0xFF here;
    }
    cleanup_draw(&B);
}

static void test_linear_2(void) {
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_linear_2, NULL, umbra_blend_src,
                                   umbra_fmt_8888, &lay),
                  lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[4] = {0};
        float     colors[8] = {1, 0, 0, 1, 0, 0, 1, 1};
        float     params[3] = {0.25f, 0, 0};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, params, 3);
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader + 12, colors, 8);
        if (!run_draw(&B, bi, 4, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=sizeof dst},
                      })) {
            continue;
        }
        (dst[0] & 0xff) == 0xFF here;
        ((dst[0] >> 16) & 0xff) == 0 here;
        (dst[3] & 0xff) == 64 here;
        ((dst[3] >> 16) & 0xff) == 191 here;
        for (int i = 0; i < 4; i++) { (((dst[i] >> 24) & 0xff) == 0xFF) here; }
    }
    cleanup_draw(&B);
}

static void test_radial_2(void) {
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_radial_2, NULL, umbra_blend_src,
                                   umbra_fmt_8888, &lay),
                  lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[1] = {0};
        float     colors[8] = {1, 1, 1, 1, 0, 0, 0, 1};
        float     params[3] = {0, 0, 0.1f};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, params, 3);
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader + 12, colors, 8);
        if (!run_draw(&B, bi, 1, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=4},
                      })) {
            continue;
        }
        (dst[0] & 0xff) == 0xFF here;
        ((dst[0] >> 24) & 0xff) == 0xFF here;
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

    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_linear_grad, NULL, umbra_blend_src,
                                   umbra_fmt_8888, &lay),
                  lay);

    size_t lut_off = (B.lay.shader + 16 + 7) & ~(size_t)7;
    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[8] = {0};
        float     params[4] = {0.125f, 0, 0, 256};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, params, 4);
        umbra_uniforms_fill_ptr(B.lay.uni, lut_off, (struct umbra_buf){.ptr=lut, .sz=sizeof lut});
        if (!run_draw(&B, bi, 8, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=sizeof dst},
                      })) {
            continue;
        }
        (dst[0] & 0xff) == 0xFF here;
        ((dst[0] >> 8) & 0xff) == 0 here;
        ((dst[7] >> 16) & 0xff) == 191 here;
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

    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_radial_grad, NULL, umbra_blend_src,
                                   umbra_fmt_8888, &lay),
                  lay);

    size_t lut_off = (B.lay.shader + 16 + 7) & ~(size_t)7;
    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[1] = {0};
        float     params[4] = {0, 0, 0.1f, 64};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, params, 4);
        umbra_uniforms_fill_ptr(B.lay.uni, lut_off, (struct umbra_buf){.ptr=lut, .sz=sizeof lut});
        if (!run_draw(&B, bi, 1, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=4},
                      })) {
            continue;
        }
        (dst[0] & 0xff) == 0xFF here;
        ((dst[0] >> 16) & 0xff) == 0 here;
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

    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_build(umbra_shader_linear_grad, NULL, umbra_blend_src,
                                   umbra_fmt_8888, &lay),
                  lay);

    size_t lut_off = (B.lay.shader + 16 + 7) & ~(size_t)7;
    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[8] = {0};
        float     params[4] = {0.125f, 0, 0, 64};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, params, 4);
        umbra_uniforms_fill_ptr(B.lay.uni, lut_off, (struct umbra_buf){.ptr=lut, .sz=sizeof lut});
        if (!run_draw(&B, bi, 8, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=sizeof dst},
                      })) {
            continue;
        }
        (dst[0] & 0xff) == 0xFF here;
        ((dst[7] >> 16) & 0xff) == 215 here;
    }
    cleanup_draw(&B);
}


static umbra_shader_fn ss_inner_;
static int             ss_n_;
static umbra_color ss_shader_(struct umbra_builder *builder, struct umbra_uniforms *u, umbra_val x, umbra_val y) {
    (void)u;
    return umbra_supersample(builder, u, x, y, ss_inner_, ss_n_);
}

static void test_supersample(void) {
    ss_inner_ = umbra_shader_solid;
    ss_n_ = 4;
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_build(ss_shader_, NULL, umbra_blend_src,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < 3; bi++) {
        uint32_t  dst[4] = {0};
        float     color[4] = {1, 0, 0, 1};
        umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);
        if (!run_draw(&B, bi, 4, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
                          {.ptr=dst, .sz=4 * 4},
                      })) {
            continue;
        }
        for (int i = 0; i < 4; i++) {
            (dst[i] & 0xFF) == 0xFF here;
            ((dst[i] >> 24) & 0xFF) == 0xFF here;
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
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_src,
                                                 umbra_fmt_8888, &lay),
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
    umbra_uniforms_fill_f32(B.lay.uni, B.lay.shader, color, 4);

    for (int bi = 0; bi < 3; bi++) {
        __builtin_memset(aligned, 0, N * 4);
        if (!run_draw(&B, bi, N, 1, (struct umbra_buf[]){
            (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
            {.ptr=aligned, .sz=N * 4},
        })) { continue; }
        for (int i = 0; i < N; i++) { (aligned[i] == 0xFF00FF00u) here; }

        __builtin_memset(offset, 0, N * 4);
        if (!run_draw(&B, bi, N, 1, (struct umbra_buf[]){
            (struct umbra_buf){.ptr=B.lay.uni->data, .sz=B.lay.uni->size, .read_only=1},
            {.ptr=offset, .sz=N * 4},
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
    test_page_aligned_buffer();

    // load_color / store_color round-trip tests for formats without dedicated x4 ops.
    {
        struct umbra_builder *b = umbra_builder();
        umbra_color c = umbra_load_color(b, (umbra_ptr){0}, umbra_fmt_565);
        umbra_store_color(b, (umbra_ptr){1}, c, umbra_fmt_565);
        struct umbra_basic_block *bb = umbra_basic_block(b);
        umbra_builder_free(b);
        struct test_backends B = test_backends_make(bb);
        umbra_basic_block_free(bb);
        uint16_t src[7], dst[7];
        for (int i = 0; i < 7; i++) { src[i] = (uint16_t)(0x1234u + (unsigned)i * 0x1111u); }
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            __builtin_memset(dst, 0, sizeof dst);
            if (!test_backends_run(&B, bi, 7, 1, (struct umbra_buf[]){
                {.ptr=src, .sz=sizeof src}, {.ptr=dst, .sz=sizeof dst},
            })) { continue; }
            for (int i = 0; i < 7; i++) { (dst[i] == src[i]) here; }
        }
        test_backends_free(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_color c = umbra_load_color(b, (umbra_ptr){0}, umbra_fmt_1010102);
        umbra_store_color(b, (umbra_ptr){1}, c, umbra_fmt_1010102);
        struct umbra_basic_block *bb = umbra_basic_block(b);
        umbra_builder_free(b);
        struct test_backends B = test_backends_make(bb);
        umbra_basic_block_free(bb);
        uint32_t src[7], dst[7];
        for (int i = 0; i < 7; i++) {
            src[i] = ((unsigned)i * 73u) | ((unsigned)i * 37u << 10)
                   | ((unsigned)i * 19u << 20) | (2u << 30);
        }
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            __builtin_memset(dst, 0, sizeof dst);
            if (!test_backends_run(&B, bi, 7, 1, (struct umbra_buf[]){
                {.ptr=src, .sz=sizeof src}, {.ptr=dst, .sz=sizeof dst},
            })) { continue; }
            for (int i = 0; i < 7; i++) { (dst[i] == src[i]) here; }
        }
        test_backends_free(&B);
    }

    return 0;
}
