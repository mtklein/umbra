#include "../include/umbra_draw.h"
#include "../src/count.h"
#include "test.h"
#include <stdint.h>

struct draw_backends {
    struct test_backends     tb;
    struct umbra_draw_layout lay;
};

static struct draw_backends make_draw(struct umbra_builder *builder, struct umbra_draw_layout lay) {
    struct umbra_flat_ir *bb = umbra_flat_ir(builder);
    umbra_builder_free(builder);
    struct draw_backends B = {
        test_backends_make(bb),
        lay,
    };
    umbra_flat_ir_free(bb);
    return B;
}
static _Bool run_draw(struct draw_backends *B, int b, int w, int h, struct umbra_buf buf[]) {
    return test_backends_run(&B->tb, b, w, h, buf);
}
static void cleanup_draw(struct draw_backends *B) {
    test_backends_free(&B->tb);
    free(B->lay.uniforms);
}

TEST(test_solid_src) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){1, 0, 0, 1});
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_builder(&shader.base, NULL, umbra_blend_src,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[4] = {
            0xFFFFFFFF,
            0xFFFFFFFF,
            0xFFFFFFFF,
            0xFFFFFFFF,
        };
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, 4, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=4},
                      })) {
            for (int i = 0; i < 4; i++) {
                (dst[i] & 0xFF) == 0xFF here;
                ((dst[i] >> 8) & 0xFF) == 0x00 here;
                ((dst[i] >> 16) & 0xFF) == 0x00 here;
                ((dst[i] >> 24) & 0xFF) == 0xFF here;
            }
        }
    }
    cleanup_draw(&B);
}

TEST(test_solid_src_n1) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){0, 0, 1, 1});
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_builder(&shader.base, NULL, umbra_blend_src,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[1] = {0xFFFFFFFF};
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, 1, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=1},
                      })) {
            (dst[0] & 0xFF) == 0x00 here;
            ((dst[0] >> 8) & 0xFF) == 0x00 here;
            ((dst[0] >> 16) & 0xFF) == 0xFF here;
            ((dst[0] >> 24) & 0xFF) == 0xFF here;
        }
    }
    cleanup_draw(&B);
}

TEST(test_solid_src_n9) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){0, 1, 0, 1});
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_builder(&shader.base, NULL, umbra_blend_src,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0xFF, sizeof dst);
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, 9, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=9},
                      })) {
            for (int i = 0; i < 9; i++) {
                ((dst[i] >> 8) & 0xFF) == 0xFF here;
                ((dst[i] >> 24) & 0xFF) == 0xFF here;
            }
        }
    }
    cleanup_draw(&B);
}

TEST(test_solid_src_n16) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){1, 1, 1, 1});
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_builder(&shader.base, NULL, umbra_blend_src,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[16];
        __builtin_memset(dst, 0, sizeof dst);
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, 16, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=16},
                      })) {
            for (int i = 0; i < 16; i++) { dst[i] == 0xFFFFFFFF here; }
        }
    }
    cleanup_draw(&B);
}

TEST(test_srcover_8888) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){0, 0.5f, 0, 0.5f});
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_builder(&shader.base, NULL, umbra_blend_srcover,
                                   umbra_fmt_8888, &lay),
                  lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[2] = {0xFFFFFFFF, 0xFFFFFFFF};
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, 2, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=2},
                      })) {
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
    }
    cleanup_draw(&B);
}

TEST(test_dstover_8888) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){1, 0, 0, 1});
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_builder(&shader.base, NULL, umbra_blend_dstover,
                                   umbra_fmt_8888, &lay),
                  lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[2] = {0xFFFFFFFF, 0xFFFFFFFF};
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, 2, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=2},
                      })) {
            for (int i = 0; i < 2; i++) { dst[i] == 0xFFFFFFFF here; }
        }
    }
    cleanup_draw(&B);
}

TEST(test_dstover_transparent) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){1, 0, 0, 1});
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_builder(&shader.base, NULL, umbra_blend_dstover,
                                   umbra_fmt_8888, &lay),
                  lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[2] = {0, 0};
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, 2, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=2},
                      })) {
            for (int i = 0; i < 2; i++) {
                (dst[i] & 0xFF) == 0xFF here;
                ((dst[i] >> 24) & 0xFF) == 0xFF here;
            }
        }
    }
    cleanup_draw(&B);
}

TEST(test_multiply_8888) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){1, 1, 1, 1});
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_builder(&shader.base, NULL, umbra_blend_multiply,
                                   umbra_fmt_8888, &lay),
                  lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[2] = {0xFF804020, 0xFF804020};
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, 2, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=2},
                      })) {
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
    }
    cleanup_draw(&B);
}

TEST(test_solid_src_fp16) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){0.25f, 0.5f, 0.75f, 1});
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_builder(&shader.base, NULL, umbra_blend_src,
                                                 umbra_fmt_fp16, &lay),
                                lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __fp16 dst[4 * 3];
        __builtin_memset(dst, 0, sizeof dst);
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, 3, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=3},
                      })) {
            for (int i = 0; i < 3; i++) {
                equiv((float)dst[i * 4 + 0], 0.25f) here;
                equiv((float)dst[i * 4 + 1], 0.5f) here;
                equiv((float)dst[i * 4 + 2], 0.75f) here;
                equiv((float)dst[i * 4 + 3], 1.0f) here;
            }
        }
    }
    cleanup_draw(&B);
}

TEST(test_srcover_fp16) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){0, 0.5f, 0, 0.5f});
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_builder(&shader.base, NULL, umbra_blend_srcover,
                                   umbra_fmt_fp16, &lay),
                  lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __fp16 dst[4 * 2];
        for (int i = 0; i < 2; i++) {
            dst[i * 4 + 0] = 1;
            dst[i * 4 + 1] = 1;
            dst[i * 4 + 2] = 1;
            dst[i * 4 + 3] = 1;
        }
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, 2, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=2},
                      })) {
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
    }
    cleanup_draw(&B);
}

TEST(test_coverage_rect) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){1, 0, 0, 1});
    struct umbra_coverage_rect cov = umbra_coverage_rect((float[]){2.0f, 0.0f, 5.0f, 1.0f});
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_builder(&shader.base, &cov.base,
                                                 umbra_blend_srcover,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        umbra_draw_fill(&B.lay, &shader.base, &cov.base);
        if (run_draw(&B, bi, 8, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=8},
                      })) {
            for (int i = 0; i < 8; i++) {
                if (i >= 2 && i < 5) {
                    (dst[i] & 0xFF) == 0xFF here;
                    ((dst[i] >> 24) & 0xFF) == 0xFF here;
                } else {
                    dst[i] == 0 here;
                }
            }
        }
    }
    cleanup_draw(&B);
}

TEST(test_coverage_rect_scalar) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){1, 0, 0, 1});
    struct umbra_coverage_rect cov = umbra_coverage_rect((float[]){1.0f, 0.0f, 3.0f, 1.0f});
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_builder(&shader.base, &cov.base,
                                                 umbra_blend_srcover,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[4];
        __builtin_memset(dst, 0, sizeof dst);
        umbra_draw_fill(&B.lay, &shader.base, &cov.base);
        if (run_draw(&B, bi, 4, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=4},
                      })) {
            dst[0] == 0 here;
            (dst[1] & 0xFF) == 0xFF here;
            ((dst[1] >> 24) & 0xFF) == 0xFF here;
            (dst[2] & 0xFF) == 0xFF here;
            ((dst[2] >> 24) & 0xFF) == 0xFF here;
            dst[3] == 0 here;
        }
    }
    cleanup_draw(&B);
}

TEST(test_coverage_rect_n9) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){0, 1, 0, 1});
    struct umbra_coverage_rect cov = umbra_coverage_rect((float[]){3.0f, 0.0f, 7.0f, 1.0f});
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_builder(&shader.base, &cov.base,
                                                 umbra_blend_srcover,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0, sizeof dst);
        umbra_draw_fill(&B.lay, &shader.base, &cov.base);
        if (run_draw(&B, bi, 9, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=9},
                      })) {
            for (int i = 0; i < 9; i++) {
                if (i >= 3 && i < 7) {
                    ((dst[i] >> 8) & 0xFF) == 0xFF here;
                } else {
                    dst[i] == 0 here;
                }
            }
        }
    }
    cleanup_draw(&B);
}

TEST(test_coverage_rect_offset) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){0, 1, 0, 1});
    struct umbra_coverage_rect cov = umbra_coverage_rect((float[]){1.0f, 0.0f, 3.0f, 10.0f});
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_builder(&shader.base, &cov.base,
                                                 umbra_blend_src,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[4];
        __builtin_memset(dst, 0, sizeof dst);
        umbra_draw_fill(&B.lay, &shader.base, &cov.base);
        if (run_draw(&B, bi, 4, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=4},
                      })) {
            dst[0] == 0 here;
            ((dst[1] >> 8) & 0xFF) == 0xFF here;
            ((dst[2] >> 8) & 0xFF) == 0xFF here;
            dst[3] == 0 here;
        }
    }
    cleanup_draw(&B);
}

TEST(test_coverage_rect_outside_y) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){1, 1, 1, 1});
    struct umbra_coverage_rect cov = umbra_coverage_rect((float[]){0.0f, 5.0f, 10.0f, 10.0f});
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_builder(&shader.base, &cov.base,
                                                 umbra_blend_srcover,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[4] = {
            0x12345678,
            0x12345678,
            0x12345678,
            0x12345678,
        };
        umbra_draw_fill(&B.lay, &shader.base, &cov.base);
        if (run_draw(&B, bi, 4, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=4},
                      })) {
            for (int i = 0; i < 4; i++) { dst[i] == 0x12345678 here; }
        }
    }
    cleanup_draw(&B);
}

TEST(test_no_shader) {
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_builder(NULL, NULL, umbra_blend_src,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[4] = {
            0xFFFFFFFF,
            0xFFFFFFFF,
            0xFFFFFFFF,
            0xFFFFFFFF,
        };
        umbra_draw_fill(&B.lay, NULL, NULL);
        if (run_draw(&B, bi, 4, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=4},
                      })) {
            for (int i = 0; i < 4; i++) { dst[i] == 0 here; }
        }
    }
    cleanup_draw(&B);
}

TEST(test_no_blend) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){1, 0, 1, 1});
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_builder(&shader.base, NULL, NULL,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[2] = {0, 0};
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, 2, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=2},
                      })) {
            for (int i = 0; i < 2; i++) {
                (dst[i] & 0xFF) == 0xFF here;
                ((dst[i] >> 8) & 0xFF) == 0x00 here;
                ((dst[i] >> 16) & 0xFF) == 0xFF here;
                ((dst[i] >> 24) & 0xFF) == 0xFF here;
            }
        }
    }
    cleanup_draw(&B);
}

struct test_gradient_shader {
    struct umbra_shader base;
    float  params[2];
    int off_, :32;
};

static umbra_color test_gradient_build(struct umbra_shader *s, struct umbra_builder *builder,
                                       struct umbra_uniforms_layout *u,
                                       umbra_val32 x, umbra_val32 y) {
    struct test_gradient_shader *self = (struct test_gradient_shader *)s;
    (void)y;
    self->off_ = umbra_uniforms_reserve_f32(u, 2);
    umbra_val32 w = umbra_uniform_32(builder, (umbra_ptr32){0}, self->off_);
    umbra_val32 a = umbra_uniform_32(builder, (umbra_ptr32){0}, self->off_ + 1);
    umbra_val32 t = umbra_div_f32(builder, x, w);
    umbra_val32 zero = umbra_imm_i32(builder, 0);
    return (umbra_color){t, zero, zero, a};
}

static void test_gradient_fill(struct umbra_shader const *s, void *uniforms) {
    struct test_gradient_shader const *self = (struct test_gradient_shader const *)s;
    umbra_uniforms_fill_f32(uniforms, self->off_, self->params, 2);
}

static struct test_gradient_shader make_test_gradient(float const params[2]) {
    struct test_gradient_shader s = {
        .base = {.build = test_gradient_build, .fill = test_gradient_fill},
    };
    __builtin_memcpy(s.params, params, 8);
    return s;
}

TEST(test_gradient_shader) {
    struct test_gradient_shader shader = make_test_gradient((float[]){4.0f, 1.0f});
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_builder(&shader.base, NULL, umbra_blend_src,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[4] = {0};
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, 4, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=4},
                      })) {
            int r0 = (int)(dst[0] & 0xFF);
            int r3 = (int)(dst[3] & 0xFF);
            r0 == 0 here;
            r3 == 191 here;
            for (int i = 0; i < 4; i++) { (((dst[i] >> 24) & 0xFF) == 0xFF) here; }
        }
    }
    cleanup_draw(&B);
}

TEST(test_multiply_half_alpha) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){1, 0, 0, 0.5f});
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_builder(&shader.base, NULL, umbra_blend_multiply,
                                   umbra_fmt_8888, &lay),
                  lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[2] = {0x80FF0000, 0x80FF0000};
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, 2, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=2},
                      })) {
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
    }
    cleanup_draw(&B);
}

TEST(test_srcover_8888_n9) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){1, 0, 0, 0.5f});
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_builder(&shader.base, NULL, umbra_blend_srcover,
                                   umbra_fmt_8888, &lay),
                  lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0, sizeof dst);
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, 9, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=9},
                      })) {
            for (int i = 0; i < 9; i++) {
                int r = (int)(dst[i] & 0xFF);
                int a = (int)((dst[i] >> 24) & 0xFF);
                r == 0xFF here;
                a == 128 here;
            }
        }
    }
    cleanup_draw(&B);
}

TEST(test_full_pipeline) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){1, 0, 0, 1});
    struct umbra_coverage_rect cov = umbra_coverage_rect((float[]){2.0f, 0.0f, 7.0f, 1.0f});
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_builder(&shader.base, &cov.base,
                                                 umbra_blend_srcover,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0, sizeof dst);
        umbra_draw_fill(&B.lay, &shader.base, &cov.base);
        if (run_draw(&B, bi, 9, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=9},
                      })) {
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
    }
    cleanup_draw(&B);
}

TEST(test_solid_src_fp16_n9) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){0.125f, 0.25f, 0.5f, 1});
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_builder(&shader.base, NULL, umbra_blend_src,
                                                 umbra_fmt_fp16, &lay),
                                lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __fp16 dst[4 * 9];
        __builtin_memset(dst, 0, sizeof dst);
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, 9, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=9},
                      })) {
            for (int i = 0; i < 9; i++) {
                equiv((float)dst[i * 4 + 0], 0.125f) here;
                equiv((float)dst[i * 4 + 1], 0.25f) here;
                equiv((float)dst[i * 4 + 2], 0.5f) here;
                equiv((float)dst[i * 4 + 3], 1.0f) here;
            }
        }
    }
    cleanup_draw(&B);
}

TEST(test_coverage_rect_white_dst) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){1, 0, 0, 1});
    struct umbra_coverage_rect cov = umbra_coverage_rect((float[]){0, 0, 0, 0});
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_builder(&shader.base, &cov.base,
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
    int ncases = count(cases);

    for (int ci = 0; ci < ncases; ci++) {
        rect_case rc = cases[ci];
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            uint32_t dst[24];
            __builtin_memset(dst, 0xFF, (size_t)rc.n * 4);
            __builtin_memcpy(shader.color, (float[]){1, 0, 0, 1}, 16);
            __builtin_memcpy(cov.rect, (float[]){rc.x0, 0.0f, rc.x1, 1.0f}, 16);
            umbra_draw_fill(&B.lay, &shader.base, &cov.base);
            if (run_draw(&B, bi, rc.n, 1,
                          (struct umbra_buf[]){
                              (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                              {.ptr=dst, .count=rc.n},
                          })) {
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
    }
    cleanup_draw(&B);
}

TEST(test_coverage_bitmap) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){1, 1, 1, 1});
    uint16_t cov_data[8] = {0, 128, 255, 0, 0, 0, 0, 0};
    struct umbra_coverage_bitmap cov =
        umbra_coverage_bitmap((struct umbra_buf){.ptr=cov_data, .count=8});
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_builder(&shader.base, &cov.base,
                                                 umbra_blend_srcover,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        umbra_draw_fill(&B.lay, &shader.base, &cov.base);
        if (run_draw(&B, bi, 8, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=8},
                      })) {
            dst[0] == 0 here;
            (dst[1] & 0xff) == 128 here;
            (dst[2] & 0xff) == 0xff here;
            dst[3] == 0 here;
        }
    }
    cleanup_draw(&B);
}

TEST(test_coverage_bitmap_565) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){1, 0, 0, 1});
    uint16_t cov_data[16];
    for (int i = 0; i < 16; i++) { cov_data[i] = 255; }
    struct umbra_coverage_bitmap cov =
        umbra_coverage_bitmap((struct umbra_buf){.ptr=cov_data, .count=16});
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_builder(&shader.base, &cov.base,
                                                 umbra_blend_srcover,
                                                 umbra_fmt_565, &lay),
                                lay);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint16_t dst[16];
        __builtin_memset(dst, 0, sizeof dst);
        umbra_draw_fill(&B.lay, &shader.base, &cov.base);
        if (run_draw(&B, bi, 16, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=16, .stride=16},
                      })) {
            for (int i = 0; i < 16; i++) {
                dst[i] == 0xF800u here;
            }
        }
    }
    cleanup_draw(&B);
}

TEST(test_coverage_sdf) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){1, 1, 1, 1});
    uint16_t cov_data[8] = {0, 100, 128, 200, 255, 0, 0, 0};
    struct umbra_coverage_sdf cov =
        umbra_coverage_sdf((struct umbra_buf){.ptr=cov_data, .count=8});
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_builder(&shader.base, &cov.base,
                                                 umbra_blend_srcover,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        umbra_draw_fill(&B.lay, &shader.base, &cov.base);
        if (run_draw(&B, bi, 8, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=8},
                      })) {
            dst[0] == 0 here;
            (dst[4] & 0xff) == 0xff here;
        }
    }
    cleanup_draw(&B);
}

TEST(test_coverage_bitmap_matrix) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){1, 1, 1, 1});
    uint16_t bmp[8] = {0, 0, 255, 0, 0, 0, 0, 0};
    struct umbra_coverage_bitmap_matrix cov = umbra_coverage_bitmap_matrix(
        (struct umbra_matrix){1, 0, 0, 0, 1, 0, 0, 0, 1},
        (struct umbra_bitmap){.buf={.ptr=bmp, .count=8}, .w=8, .h=1});
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_builder(&shader.base, &cov.base,
                                   umbra_blend_srcover, umbra_fmt_8888, &lay),
                  lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        umbra_draw_fill(&B.lay, &shader.base, &cov.base);
        if (run_draw(&B, bi, 8, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=8},
                      })) {
            dst[0] == 0 here;
            (dst[2] & 0xff) == 0xff here;
            dst[3] == 0 here;
        }
    }
    cleanup_draw(&B);
}

TEST(test_coverage_bitmap_matrix_565) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){1, 0, 0, 1});
    uint16_t bmp[16];
    for (int i = 0; i < 16; i++) { bmp[i] = 255; }
    struct umbra_coverage_bitmap_matrix cov = umbra_coverage_bitmap_matrix(
        (struct umbra_matrix){1, 0, 0, 0, 1, 0, 0, 0, 1},
        (struct umbra_bitmap){.buf={.ptr=bmp, .count=16}, .w=16, .h=1});
    struct umbra_draw_layout lay;
    struct draw_backends B =
        make_draw(umbra_draw_builder(&shader.base, &cov.base,
                                   umbra_blend_srcover, umbra_fmt_565, &lay), lay);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint16_t dst[16];
        __builtin_memset(dst, 0, sizeof dst);
        umbra_draw_fill(&B.lay, &shader.base, &cov.base);
        if (run_draw(&B, bi, 16, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=16, .stride=16},
                      })) {
            for (int i = 0; i < 16; i++) {
                dst[i] == 0xF800u here;
            }
        }
    }
    cleanup_draw(&B);
}

TEST(test_coverage_bitmap_matrix_oob) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){1, 1, 1, 1});
    uint16_t bmp[4] = {255, 0, 0, 0};
    struct umbra_coverage_bitmap_matrix cov = umbra_coverage_bitmap_matrix(
        (struct umbra_matrix){1, 0, 0, 0, 1, 0, 0.001f, 0, 1},
        (struct umbra_bitmap){.buf={.ptr=bmp, .count=4}, .w=2, .h=2});
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_builder(&shader.base, &cov.base,
                                   umbra_blend_srcover, umbra_fmt_8888, &lay),
                  lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        umbra_draw_fill(&B.lay, &shader.base, &cov.base);
        if (run_draw(&B, bi, 8, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=8},
                      })) {
            (dst[0] & 0xff) == 0xFF here;
        }
    }
    cleanup_draw(&B);
}

TEST(test_linear_2) {
    struct umbra_shader_linear_2 shader =
        umbra_shader_linear_2((float[]){0.25f, 0, 0},
                              (float[]){1, 0, 0, 1, 0, 0, 1, 1});
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_builder(&shader.base, NULL, umbra_blend_src,
                                   umbra_fmt_8888, &lay),
                  lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[4] = {0};
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, 4, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=4},
                      })) {
            (dst[0] & 0xff) == 0xFF here;
            ((dst[0] >> 16) & 0xff) == 0 here;
            (dst[3] & 0xff) == 64 here;
            ((dst[3] >> 16) & 0xff) == 191 here;
            for (int i = 0; i < 4; i++) { (((dst[i] >> 24) & 0xff) == 0xFF) here; }
        }
    }
    cleanup_draw(&B);
}

TEST(test_radial_2) {
    struct umbra_shader_radial_2 shader =
        umbra_shader_radial_2((float[]){0, 0, 0.1f},
                              (float[]){1, 1, 1, 1, 0, 0, 0, 1});
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_builder(&shader.base, NULL, umbra_blend_src,
                                   umbra_fmt_8888, &lay),
                  lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[1] = {0};
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, 1, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=1},
                      })) {
            (dst[0] & 0xff) == 0xFF here;
            ((dst[0] >> 24) & 0xff) == 0xFF here;
        }
    }
    cleanup_draw(&B);
}

TEST(test_linear_grad) {
    float const stop_colors[][4] = {
        {1, 0, 0, 1},
        {0, 1, 0, 1},
        {0, 0, 1, 1},
    };
    float lut[256 * 4];
    umbra_gradient_lut_even(lut, 256, 3, stop_colors);

    struct umbra_shader_linear_grad shader =
        umbra_shader_linear_grad((float[]){0.125f, 0, 0, 256},
                                 (struct umbra_buf){.ptr=lut, .count=256 * 4});
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_builder(&shader.base, NULL, umbra_blend_src,
                                   umbra_fmt_8888, &lay),
                  lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[8] = {0};
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, 8, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=8},
                      })) {
            (dst[0] & 0xff) == 0xFF here;
            ((dst[0] >> 8) & 0xff) == 0 here;
            ((dst[7] >> 16) & 0xff) == 191 here;
        }
    }
    cleanup_draw(&B);
}

TEST(test_radial_grad) {
    float const stop_colors[][4] = {
        {1, 0, 0, 1},
        {1, 1, 0, 1},
        {0, 1, 0, 1},
        {0, 0, 1, 1},
    };
    float lut[64 * 4];
    umbra_gradient_lut_even(lut, 64, 4, stop_colors);

    struct umbra_shader_radial_grad shader =
        umbra_shader_radial_grad((float[]){0, 0, 0.1f, 64},
                                 (struct umbra_buf){.ptr=lut, .count=64 * 4});
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_builder(&shader.base, NULL, umbra_blend_src,
                                   umbra_fmt_8888, &lay),
                  lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[1] = {0};
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, 1, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=1},
                      })) {
            (dst[0] & 0xff) == 0xFF here;
            ((dst[0] >> 16) & 0xff) == 0 here;
        }
    }
    cleanup_draw(&B);
}

TEST(test_gradient_lut_nonuniform) {
    float const stop_colors[][4] = {
        {1, 0, 0, 1},
        {0, 1, 0, 1},
        {0, 0, 1, 1},
    };
    float positions[] = {0, 0.2f, 1.0f};
    float lut[64 * 4];
    umbra_gradient_lut(lut, 64, 3, positions, stop_colors);

    struct umbra_shader_linear_grad shader =
        umbra_shader_linear_grad((float[]){0.125f, 0, 0, 64},
                                 (struct umbra_buf){.ptr=lut, .count=64 * 4});
    struct umbra_draw_layout lay;
    struct draw_backends     B =
        make_draw(umbra_draw_builder(&shader.base, NULL, umbra_blend_src,
                                   umbra_fmt_8888, &lay),
                  lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[8] = {0};
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, 8, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=8},
                      })) {
            (dst[0] & 0xff) == 0xFF here;
            ((dst[7] >> 16) & 0xff) == 215 here;
        }
    }
    cleanup_draw(&B);
}

TEST(test_linear_stops) {
    float colors_planar[3 * 4] = {1,0,0, 0,1,0, 0,0,1, 1,1,1};
    float pos[3] = {0.0f, 0.5f, 1.0f};

    struct umbra_shader_linear_stops shader =
        umbra_shader_linear_stops((float[]){0.25f, 0, 0, 3},
                                  (struct umbra_buf){.ptr=colors_planar, .count=12},
                                  (struct umbra_buf){.ptr=pos, .count=3});
    struct umbra_draw_layout lay;
    struct draw_backends B =
        make_draw(umbra_draw_builder(&shader.base, NULL,
                                   umbra_blend_src, umbra_fmt_8888, &lay), lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[5] = {0};
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, 5, 1, (struct umbra_buf[]){
                {.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                {.ptr=dst, .count=5, .stride=5},
            })) {
            dst[0] == 0xff0000ffu here;
            dst[1] == 0xff008080u here;
            dst[2] == 0xff00ff00u here;
            dst[3] == 0xff808000u here;
            dst[4] == 0xffff0000u here;
        }
    }
    cleanup_draw(&B);
}

TEST(test_linear_stops_fp16_planar) {
    float colors_planar[3 * 4] = {1,0,0, 0,1,0, 0,0,1, 1,1,1};
    float pos[3] = {0.0f, 0.5f, 1.0f};

    struct umbra_shader_linear_stops shader =
        umbra_shader_linear_stops((float[]){0.125f, 0, 0, 3},
                                  (struct umbra_buf){.ptr=colors_planar, .count=12},
                                  (struct umbra_buf){.ptr=pos, .count=3});
    struct umbra_draw_layout lay;
    struct draw_backends B =
        make_draw(umbra_draw_builder(&shader.base, NULL,
                                   NULL, umbra_fmt_fp16_planar, &lay), lay);

    enum { W = 8, HP = 1 };
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint16_t dst[W * HP * 4];
        __builtin_memset(dst, 0, sizeof dst);
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, W, HP, (struct umbra_buf[]){
                {.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                {.ptr=dst, .count=W * HP * 4, .stride=W},
            })) {
            dst[0*W+0] == 0x3c00 here;
            dst[0*W+2] == 0x3800 here;
            dst[0*W+4] == 0x0000 here;
            dst[1*W+4] == 0x3c00 here;
        }
    }
    cleanup_draw(&B);
}

TEST(test_supersample) {
    struct umbra_shader_solid inner = umbra_shader_solid((float[]){1, 0, 0, 1});
    struct umbra_shader_supersample shader = umbra_shader_supersample(&inner.base, 4);
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_builder(&shader.base, NULL, umbra_blend_src,
                                                 umbra_fmt_8888, &lay),
                                lay);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[4] = {0};
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, 4, 1,
                      (struct umbra_buf[]){
                          (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                          {.ptr=dst, .count=4},
                      })) {
            for (int i = 0; i < 4; i++) {
                (dst[i] & 0xFF) == 0xFF here;
                ((dst[i] >> 24) & 0xFF) == 0xFF here;
            }
        }
    }
    cleanup_draw(&B);
}

#if !defined(__wasm__)
#include <sys/mman.h>
#include <unistd.h>
#endif

TEST(test_page_aligned_buffer) {
#if defined(__wasm__)
    return;
#else
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){0, 1, 0, 1});
    struct umbra_draw_layout lay;
    struct draw_backends B = make_draw(umbra_draw_builder(&shader.base, NULL, umbra_blend_src,
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

    umbra_draw_fill(&B.lay, &shader.base, NULL);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(aligned, 0, N * 4);
        if (run_draw(&B, bi, N, 1, (struct umbra_buf[]){
            (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
            {.ptr=aligned, .count=N},
        })) {
            for (int i = 0; i < N; i++) { aligned[i] == 0xFF00FF00u here; }

            __builtin_memset(offset, 0, N * 4);
            if (run_draw(&B, bi, N, 1, (struct umbra_buf[]){
                (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                {.ptr=offset, .count=N},
            })) {
                for (int i = 0; i < N; i++) { offset[i] == 0xFF00FF00u here; }
            }
        }
    }
    munmap(page, alloc + (size_t)pgsz);
    cleanup_draw(&B);
#endif
}

TEST(test_565_round_trip) {
    struct umbra_builder *b = umbra_builder();
    umbra_color c = umbra_load_565(b, (umbra_ptr16){0});
    umbra_store_565(b, (umbra_ptr16){.ix=1}, c);
    struct umbra_flat_ir *bb = umbra_flat_ir(b);
    umbra_builder_free(b);
    struct test_backends B = test_backends_make(bb);
    umbra_flat_ir_free(bb);
    enum { W565 = 35 };
    uint16_t src[W565], dst[W565];
    for (int i = 0; i < W565; i++) { src[i] = (uint16_t)(0x1234u + (unsigned)i * 0x1111u); }
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!test_backends_run(&B, bi, W565, 1, (struct umbra_buf[]){
            {.ptr=src, .count=W565}, {.ptr=dst, .count=W565},
        })) { continue; }
        for (int i = 0; i < W565; i++) { dst[i] == src[i] here; }
    }
    test_backends_free(&B);
}

TEST(test_1010102_round_trip) {
    struct umbra_builder *b = umbra_builder();
    umbra_color c = umbra_load_1010102(b, (umbra_ptr32){0});
    umbra_store_1010102(b, (umbra_ptr32){.ix=1}, c);
    struct umbra_flat_ir *bb = umbra_flat_ir(b);
    umbra_builder_free(b);
    struct test_backends B = test_backends_make(bb);
    umbra_flat_ir_free(bb);
    uint32_t src[7], dst[7];
    for (int i = 0; i < 7; i++) {
        src[i] = ((unsigned)i * 73u) | ((unsigned)i * 37u << 10)
               | ((unsigned)i * 19u << 20) | (2u << 30);
    }
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!test_backends_run(&B, bi, 7, 1, (struct umbra_buf[]){
            {.ptr=src, .count=7}, {.ptr=dst, .count=7},
        })) { continue; }
        for (int i = 0; i < 7; i++) { dst[i] == src[i] here; }
    }
    test_backends_free(&B);
}

TEST(test_fp16_planar_round_trip) {
    struct umbra_builder *b = umbra_builder();
    umbra_color c = umbra_load_fp16_planar(b, (umbra_ptr16){0});
    umbra_store_fp16_planar(b, (umbra_ptr16){.ix=1}, c);
    struct umbra_flat_ir *bb = umbra_flat_ir(b);
    umbra_builder_free(b);
    struct test_backends B = test_backends_make(bb);
    umbra_flat_ir_free(bb);
    enum { WP = 8 };
    __fp16 src[WP * 4], dst[WP * 4];
    for (int i = 0; i < WP * 4; i++) { src[i] = (__fp16)(1.0f + (float)i * 0.1f); }
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!test_backends_run(&B, bi, WP, 1, (struct umbra_buf[]){
            {.ptr=src, .count=WP * 4, .stride=WP},
            {.ptr=dst, .count=WP * 4, .stride=WP},
        })) { continue; }
        for (int i = 0; i < WP * 4; i++) { equiv((float)dst[i], (float)src[i]) here; }
    }
    test_backends_free(&B);
}

TEST(test_solid_src_565) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){1, 0, 0, 1});
    struct umbra_draw_layout lay;
    struct umbra_builder *b = umbra_draw_builder(&shader.base, NULL,
                                                umbra_blend_src, umbra_fmt_565, &lay);
    struct umbra_flat_ir *bb = umbra_flat_ir(b);
    umbra_builder_free(b);
    struct test_backends B = test_backends_make(bb);
    umbra_flat_ir_free(bb);
    umbra_draw_fill(&lay, &shader.base, NULL);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint16_t dst[4] = {0};
        if (!test_backends_run(&B, bi, 4, 1, (struct umbra_buf[]){
            {.ptr=lay.uniforms, .count=lay.uni.slots},
            {.ptr=dst, .count=4},
        })) { continue; }
        for (int i = 0; i < 4; i++) { dst[i] == 0xF800 here; }
    }
    test_backends_free(&B);
    free(lay.uniforms);
}

TEST(test_solid_src_1010102) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){0, 1, 0, 1});
    struct umbra_draw_layout lay;
    struct umbra_builder *b = umbra_draw_builder(&shader.base, NULL,
                                                umbra_blend_src, umbra_fmt_1010102, &lay);
    struct umbra_flat_ir *bb = umbra_flat_ir(b);
    umbra_builder_free(b);
    struct test_backends B = test_backends_make(bb);
    umbra_flat_ir_free(bb);
    umbra_draw_fill(&lay, &shader.base, NULL);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[4] = {0};
        if (!test_backends_run(&B, bi, 4, 1, (struct umbra_buf[]){
            {.ptr=lay.uniforms, .count=lay.uni.slots},
            {.ptr=dst, .count=4},
        })) { continue; }
        uint32_t expect = (1023u << 10) | (3u << 30);
        for (int i = 0; i < 4; i++) { dst[i] == expect here; }
    }
    test_backends_free(&B);
    free(lay.uniforms);
}

TEST(test_solid_src_fp16_planar) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){0, 0, 1, 1});
    struct umbra_draw_layout lay;
    struct umbra_builder *b = umbra_draw_builder(&shader.base, NULL,
                                                umbra_blend_src, umbra_fmt_fp16_planar, &lay);
    struct umbra_flat_ir *bb = umbra_flat_ir(b);
    umbra_builder_free(b);
    struct test_backends B = test_backends_make(bb);
    umbra_flat_ir_free(bb);
    umbra_draw_fill(&lay, &shader.base, NULL);
    enum { WFP = 4 };
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __fp16 dst[WFP * 4];
        __builtin_memset(dst, 0, sizeof dst);
        if (!test_backends_run(&B, bi, WFP, 1, (struct umbra_buf[]){
            {.ptr=lay.uniforms, .count=lay.uni.slots},
            {.ptr=dst, .count=WFP * 4, .stride=WFP},
        })) { continue; }
        for (int i = 0; i < WFP; i++) {
            equiv((float)dst[i + WFP*0], 0.0f) here;
            equiv((float)dst[i + WFP*1], 0.0f) here;
            equiv((float)dst[i + WFP*2], 1.0f) here;
            equiv((float)dst[i + WFP*3], 1.0f) here;
        }
    }
    test_backends_free(&B);
    free(lay.uniforms);
}

TEST(test_srcover_fp16_planar) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){0, 0.5f, 0, 0.5f});
    struct umbra_draw_layout lay;
    struct draw_backends B =
        make_draw(umbra_draw_builder(&shader.base, NULL, umbra_blend_srcover,
                                   umbra_fmt_fp16_planar, &lay), lay);
    enum { W = 16 };
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __fp16 dst[W * 4];
        for (int i = 0; i < W * 4; i++) {
            dst[i] = (__fp16)1.0f;
        }
        umbra_draw_fill(&B.lay, &shader.base, NULL);
        if (run_draw(&B, bi, W, 1, (struct umbra_buf[]){
                (struct umbra_buf){.ptr=B.lay.uniforms, .count=B.lay.uni.slots},
                {.ptr=dst, .count=W * 4, .stride=W},
            })) {
            for (int i = 0; i < W; i++) {
                equiv((float)dst[i + W*0], 0.5f)  here;
                equiv((float)dst[i + W*1], 1.0f)  here;
                equiv((float)dst[i + W*2], 0.5f)  here;
                equiv((float)dst[i + W*3], 1.0f)  here;
            }
        }
    }
    cleanup_draw(&B);
}

TEST(test_sdf_dispatch_rect) {
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){1, 0, 0, 1});
    struct umbra_sdf_rect     sdf   = umbra_sdf_rect((float[]){1.0f, 1.0f, 7.0f, 3.0f});

    struct umbra_backend *bes[NUM_BACKENDS] = {
        umbra_backend_interp(), umbra_backend_jit(),
        umbra_backend_metal(),  umbra_backend_vulkan(),
        umbra_backend_wgpu(),
    };

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        if (!bes[bi]) { continue; }

        struct umbra_draw_layout lay;
        struct umbra_sdf_dispatch *qt = umbra_sdf_dispatch(bes[bi], &sdf.base,
                                                   (struct umbra_sdf_dispatch_config){.hard_edge = 1},
                                                   &shader.base, umbra_blend_srcover,
                                                   umbra_fmt_8888, &lay);
        qt != NULL here;
        umbra_sdf_dispatch_fill(&lay, &sdf.base, &shader.base);
        uint32_t dst[8 * 4];
        __builtin_memset(dst, 0, sizeof dst);
        struct umbra_buf buf[] = {
            {.ptr = lay.uniforms, .count = lay.uni.slots},
            {.ptr = dst,          .count = 8 * 4, .stride = 8},
        };
        umbra_sdf_dispatch_queue(qt, 0, 0, 8, 4, buf);
        bes[bi]->flush(bes[bi]);

        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 8; x++) {
                uint32_t const px = dst[y * 8 + x];
                if (x >= 3 && x <= 5 && y == 2) {
                    (px & 0xFF)         == 0xFF here;
                    ((px >> 24) & 0xFF) == 0xFF here;
                }
                if (x == 0 || x == 7 || y == 0 || y == 3) {
                    px == 0 here;
                }
            }
        }

        umbra_sdf_dispatch_free(qt);
        free(lay.uniforms);
    }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        if (bes[bi]) { bes[bi]->free(bes[bi]); }
    }
}

struct test_circle_sdf {
    struct umbra_sdf base;
    float cx, cy, r, pad_;
    int   off_, pad__;
};
static umbra_interval test_circle_build_(struct umbra_sdf *s, struct umbra_builder *b,
                                         struct umbra_uniforms_layout *u,
                                         umbra_interval x, umbra_interval y) {
    struct test_circle_sdf *self = (struct test_circle_sdf *)s;
    self->off_ = umbra_uniforms_reserve_f32(u, 3);
    umbra_interval const cx = umbra_interval_exact(umbra_uniform_32(b, (umbra_ptr32){0}, self->off_)),
                         cy = umbra_interval_exact(umbra_uniform_32(b, (umbra_ptr32){0}, self->off_ + 1)),
                         r  = umbra_interval_exact(umbra_uniform_32(b, (umbra_ptr32){0}, self->off_ + 2));
    umbra_interval const dx = umbra_interval_sub_f32(b, x, cx),
                         dy = umbra_interval_sub_f32(b, y, cy),
                         d2 = umbra_interval_add_f32(b,
                                  umbra_interval_mul_f32(b, dx, dx),
                                  umbra_interval_mul_f32(b, dy, dy)),
                         d  = umbra_interval_sqrt_f32(b, d2);
    return umbra_interval_sub_f32(b, d, r);
}
static void test_circle_fill_(struct umbra_sdf const *s, void *uniforms) {
    struct test_circle_sdf const *self = (struct test_circle_sdf const *)s;
    float const vals[3] = {self->cx, self->cy, self->r};
    umbra_uniforms_fill_f32(uniforms, self->off_, vals, 3);
}

TEST(test_sdf_dispatch_tiling) {
    // Canvas larger than QUEUE_MIN_TILE (512) so the grid has multiple tiles.
    // Circle at (512, 384) r=180 covers ~4 of ~6 tiles on 1024x768 with 512px tiles.
    enum { W = 1024, H = 768 };

    struct test_circle_sdf sdf = {
        .base = {.build = test_circle_build_, .fill = test_circle_fill_},
        .cx = 512, .cy = 384, .r = 180,
    };
    struct umbra_shader_solid shader = umbra_shader_solid((float[]){1, 0, 0, 1});

    struct umbra_backend *be = umbra_backend_jit();
    if (!be) { be = umbra_backend_interp(); }

    // Tiled dispatch.
    struct umbra_draw_layout lay_tiled;
    struct umbra_sdf_dispatch *disp = umbra_sdf_dispatch(be, &sdf.base,
        (struct umbra_sdf_dispatch_config){.hard_edge = 1},
        &shader.base, umbra_blend_srcover, umbra_fmt_8888, &lay_tiled);
    disp != NULL here;

    // Flat reference: same shader+coverage, no tiling.
    struct umbra_sdf_coverage adapter = umbra_sdf_coverage(&sdf.base, 1);
    struct umbra_draw_layout lay_flat;
    struct umbra_builder *fb = umbra_draw_builder(&shader.base, &adapter.base,
                                                  umbra_blend_srcover, umbra_fmt_8888, &lay_flat);
    struct umbra_flat_ir *fir = umbra_flat_ir(fb);
    umbra_builder_free(fb);
    struct umbra_program *flat = be->compile(be, fir);
    umbra_flat_ir_free(fir);

    uint32_t *tiled_buf = calloc(W * H, sizeof(uint32_t));
    uint32_t *flat_buf  = calloc(W * H, sizeof(uint32_t));

    umbra_sdf_dispatch_fill(&lay_tiled, &sdf.base, &shader.base);
    umbra_draw_fill(&lay_flat, &shader.base, &adapter.base);

    struct umbra_buf tiled_ubuf[] = {
        {.ptr = lay_tiled.uniforms, .count = lay_tiled.uni.slots},
        {.ptr = tiled_buf,          .count = W * H, .stride = W},
    };
    umbra_sdf_dispatch_queue(disp, 0, 0, W, H, tiled_ubuf);

    struct umbra_buf flat_ubuf[] = {
        {.ptr = lay_flat.uniforms, .count = lay_flat.uni.slots},
        {.ptr = flat_buf,          .count = W * H, .stride = W},
    };
    flat->queue(flat, 0, 0, W, H, flat_ubuf);

    be->flush(be);

    // Every pixel must match.
    int mismatches = 0;
    for (int i = 0; i < W * H; i++) {
        if (tiled_buf[i] != flat_buf[i]) { mismatches++; }
    }
    mismatches == 0 here;

    free(flat_buf);
    free(tiled_buf);
    flat->free(flat);
    free(lay_flat.uniforms);
    umbra_sdf_dispatch_free(disp);
    free(lay_tiled.uniforms);
    be->free(be);
}

TEST(test_metal_loop_gather) {
    // Sum of a[i] for i=0..2 via loop + deref_ptr + gather.
    // a = {10, 20, 30}, expected sum = 60.
    struct umbra_uniforms_layout uni = {0};
    int const ptr_off = umbra_uniforms_reserve_ptr(&uni);
    int const n_off   = umbra_uniforms_reserve_f32(&uni, 1);

    struct umbra_builder *b = umbra_builder();
    umbra_ptr32 const u    = {.ix = 0};
    umbra_ptr32 const data = umbra_deref_ptr32(b, u, ptr_off);
    umbra_val32 const n    = umbra_uniform_32(b, u, n_off);

    umbra_var sum = umbra_var_alloc(b);
    umbra_store_var(b, sum, umbra_imm_f32(b, 0.0f));

    umbra_val32 const j = umbra_loop(b, n); {
        umbra_val32 const val = umbra_gather_32(b, data, j);
        umbra_store_var(b, sum, umbra_add_f32(b, umbra_load_var(b, sum), val));
    } umbra_loop_end(b);

    umbra_store_32(b, (umbra_ptr32){.ix = 1}, umbra_load_var(b, sum));

    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);

    float arr[] = {10, 20, 30};
    void *uniforms = umbra_uniforms_alloc(&uni);
    umbra_uniforms_fill_ptr(uniforms, ptr_off, (struct umbra_buf){.ptr = arr, .count = 3});
    int const count = 3;
    umbra_uniforms_fill_i32(uniforms, n_off, &count, 1);

    struct umbra_backend *bes[NUM_BACKENDS] = {
        umbra_backend_interp(), umbra_backend_jit(),
        umbra_backend_metal(),  umbra_backend_vulkan(),
        umbra_backend_wgpu(),
    };

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        if (!bes[bi]) { continue; }
        struct umbra_program *prog = bes[bi]->compile(bes[bi], ir);
        float out = 0;
        struct umbra_buf buf[] = {
            {.ptr = uniforms, .count = uni.slots},
            {.ptr = &out,     .count = 1},
        };
        prog->queue(prog, 0, 0, 1, 1, buf);
        bes[bi]->flush(bes[bi]);
        equiv(out, 60.0f) here;
        prog->free(prog);
    }

    umbra_flat_ir_free(ir);
    free(uniforms);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        if (bes[bi]) { bes[bi]->free(bes[bi]); }
    }
}
