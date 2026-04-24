#include "../include/umbra_draw.h"
#include "../slides/coverage.h"
#include "../slides/gradient.h"
#include "../src/count.h"
#include "test.h"
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

static struct test_backends make(struct umbra_builder *builder) {
    struct umbra_flat_ir *ir = umbra_flat_ir(builder);
    umbra_builder_free(builder);
    struct test_backends B = test_backends_make(ir);
    umbra_flat_ir_free(ir);
    return B;
}

TEST(test_draw_builder_flat_solid) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {1, 0, 0, 1};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     NULL, NULL,
                     umbra_shader_color, &color,
                     NULL, NULL);

    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[4] = {0};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=4};
        if (test_backends_run(&B, bi, 4, 1)) {
            for (int i = 0; i < 4; i++) {
                dst[i] == 0xFF0000FFu here;
            }
        }
    }

    // Mutate between dispatches -- program reads the new bytes.
    color = (umbra_color){0, 1, 0, 1};
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[4] = {0};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=4};
        if (test_backends_run(&B, bi, 4, 1)) {
            for (int i = 0; i < 4; i++) {
                dst[i] == 0xFF00FF00u here;
            }
        }
    }
    test_backends_free(&B);
}

TEST(test_solid_src) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {1, 0, 0, 1};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     NULL, NULL,
                     umbra_shader_color, &color,
                     umbra_blend_src, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[4] = {
            0xFFFFFFFF,
            0xFFFFFFFF,
            0xFFFFFFFF,
            0xFFFFFFFF,
        };
        dst_slot = (struct umbra_buf){.ptr=dst, .count=4};
        if (test_backends_run(&B, bi, 4, 1)) {
            for (int i = 0; i < 4; i++) {
                (dst[i] & 0xFF) == 0xFF here;
                ((dst[i] >> 8) & 0xFF) == 0x00 here;
                ((dst[i] >> 16) & 0xFF) == 0x00 here;
                ((dst[i] >> 24) & 0xFF) == 0xFF here;
            }
        }
    }
    test_backends_free(&B);
}

TEST(test_solid_src_n1) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {0, 0, 1, 1};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     NULL, NULL,
                     umbra_shader_color, &color,
                     umbra_blend_src, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[1] = {0xFFFFFFFF};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=1};
        if (test_backends_run(&B, bi, 1, 1)) {
            (dst[0] & 0xFF) == 0x00 here;
            ((dst[0] >> 8) & 0xFF) == 0x00 here;
            ((dst[0] >> 16) & 0xFF) == 0xFF here;
            ((dst[0] >> 24) & 0xFF) == 0xFF here;
        }
    }
    test_backends_free(&B);
}

TEST(test_solid_src_n9) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {0, 1, 0, 1};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     NULL, NULL,
                     umbra_shader_color, &color,
                     umbra_blend_src, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0xFF, sizeof dst);
        dst_slot = (struct umbra_buf){.ptr=dst, .count=9};
        if (test_backends_run(&B, bi, 9, 1)) {
            for (int i = 0; i < 9; i++) {
                ((dst[i] >> 8) & 0xFF) == 0xFF here;
                ((dst[i] >> 24) & 0xFF) == 0xFF here;
            }
        }
    }
    test_backends_free(&B);
}

TEST(test_solid_src_n16) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {1, 1, 1, 1};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     NULL, NULL,
                     umbra_shader_color, &color,
                     umbra_blend_src, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[16];
        __builtin_memset(dst, 0, sizeof dst);
        dst_slot = (struct umbra_buf){.ptr=dst, .count=16};
        if (test_backends_run(&B, bi, 16, 1)) {
            for (int i = 0; i < 16; i++) { dst[i] == 0xFFFFFFFF here; }
        }
    }
    test_backends_free(&B);
}

TEST(test_srcover_8888) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {0, 0.5f, 0, 0.5f};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     NULL, NULL,
                     umbra_shader_color, &color,
                     umbra_blend_srcover, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[2] = {0xFFFFFFFF, 0xFFFFFFFF};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=2};
        if (test_backends_run(&B, bi, 2, 1)) {
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
    test_backends_free(&B);
}

TEST(test_dstover_8888) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {1, 0, 0, 1};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     NULL, NULL,
                     umbra_shader_color, &color,
                     umbra_blend_dstover, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[2] = {0xFFFFFFFF, 0xFFFFFFFF};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=2};
        if (test_backends_run(&B, bi, 2, 1)) {
            for (int i = 0; i < 2; i++) { dst[i] == 0xFFFFFFFF here; }
        }
    }
    test_backends_free(&B);
}

TEST(test_dstover_transparent) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {1, 0, 0, 1};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     NULL, NULL,
                     umbra_shader_color, &color,
                     umbra_blend_dstover, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[2] = {0, 0};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=2};
        if (test_backends_run(&B, bi, 2, 1)) {
            for (int i = 0; i < 2; i++) {
                (dst[i] & 0xFF) == 0xFF here;
                ((dst[i] >> 24) & 0xFF) == 0xFF here;
            }
        }
    }
    test_backends_free(&B);
}

TEST(test_multiply_8888) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {1, 1, 1, 1};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     NULL, NULL,
                     umbra_shader_color, &color,
                     umbra_blend_multiply, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[2] = {0xFF804020, 0xFF804020};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=2};
        if (test_backends_run(&B, bi, 2, 1)) {
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
    test_backends_free(&B);
}

TEST(test_solid_src_fp16) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {0.25f, 0.5f, 0.75f, 1};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_fp16, dx, dy,
                     NULL, NULL,
                     umbra_shader_color, &color,
                     umbra_blend_src, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __fp16 dst[4 * 3];
        __builtin_memset(dst, 0, sizeof dst);
        dst_slot = (struct umbra_buf){.ptr=dst, .count=3};
        if (test_backends_run(&B, bi, 3, 1)) {
            for (int i = 0; i < 3; i++) {
                equiv((float)dst[i * 4 + 0], 0.25f) here;
                equiv((float)dst[i * 4 + 1], 0.5f) here;
                equiv((float)dst[i * 4 + 2], 0.75f) here;
                equiv((float)dst[i * 4 + 3], 1.0f) here;
            }
        }
    }
    test_backends_free(&B);
}

TEST(test_srcover_fp16) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {0, 0.5f, 0, 0.5f};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_fp16, dx, dy,
                     NULL, NULL,
                     umbra_shader_color, &color,
                     umbra_blend_srcover, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __fp16 dst[4 * 2];
        for (int i = 0; i < 2; i++) {
            dst[i * 4 + 0] = 1;
            dst[i * 4 + 1] = 1;
            dst[i * 4 + 2] = 1;
            dst[i * 4 + 3] = 1;
        }
        dst_slot = (struct umbra_buf){.ptr=dst, .count=2};
        if (test_backends_run(&B, bi, 2, 1)) {
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
    test_backends_free(&B);
}

TEST(test_coverage_rect) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {1, 0, 0, 1};
    umbra_rect rect = {2.0f, 0.0f, 5.0f, 1.0f};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     umbra_coverage_rect, &rect,
                     umbra_shader_color, &color,
                     umbra_blend_srcover, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        dst_slot = (struct umbra_buf){.ptr=dst, .count=8};
        if (test_backends_run(&B, bi, 8, 1)) {
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
    test_backends_free(&B);
}

TEST(test_coverage_rect_scalar) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {1, 0, 0, 1};
    umbra_rect rect = {1.0f, 0.0f, 3.0f, 1.0f};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     umbra_coverage_rect, &rect,
                     umbra_shader_color, &color,
                     umbra_blend_srcover, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[4];
        __builtin_memset(dst, 0, sizeof dst);
        dst_slot = (struct umbra_buf){.ptr=dst, .count=4};
        if (test_backends_run(&B, bi, 4, 1)) {
            dst[0] == 0 here;
            (dst[1] & 0xFF) == 0xFF here;
            ((dst[1] >> 24) & 0xFF) == 0xFF here;
            (dst[2] & 0xFF) == 0xFF here;
            ((dst[2] >> 24) & 0xFF) == 0xFF here;
            dst[3] == 0 here;
        }
    }
    test_backends_free(&B);
}

TEST(test_coverage_rect_n9) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {0, 1, 0, 1};
    umbra_rect rect = {3.0f, 0.0f, 7.0f, 1.0f};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     umbra_coverage_rect, &rect,
                     umbra_shader_color, &color,
                     umbra_blend_srcover, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0, sizeof dst);
        dst_slot = (struct umbra_buf){.ptr=dst, .count=9};
        if (test_backends_run(&B, bi, 9, 1)) {
            for (int i = 0; i < 9; i++) {
                if (i >= 3 && i < 7) {
                    ((dst[i] >> 8) & 0xFF) == 0xFF here;
                } else {
                    dst[i] == 0 here;
                }
            }
        }
    }
    test_backends_free(&B);
}

TEST(test_coverage_rect_offset) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {0, 1, 0, 1};
    umbra_rect rect = {1.0f, 0.0f, 3.0f, 10.0f};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     umbra_coverage_rect, &rect,
                     umbra_shader_color, &color,
                     umbra_blend_src, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[4];
        __builtin_memset(dst, 0, sizeof dst);
        dst_slot = (struct umbra_buf){.ptr=dst, .count=4};
        if (test_backends_run(&B, bi, 4, 1)) {
            dst[0] == 0 here;
            ((dst[1] >> 8) & 0xFF) == 0xFF here;
            ((dst[2] >> 8) & 0xFF) == 0xFF here;
            dst[3] == 0 here;
        }
    }
    test_backends_free(&B);
}

TEST(test_coverage_rect_outside_y) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {1, 1, 1, 1};
    umbra_rect rect = {0.0f, 5.0f, 10.0f, 10.0f};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     umbra_coverage_rect, &rect,
                     umbra_shader_color, &color,
                     umbra_blend_srcover, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[4] = {
            0x12345678,
            0x12345678,
            0x12345678,
            0x12345678,
        };
        dst_slot = (struct umbra_buf){.ptr=dst, .count=4};
        if (test_backends_run(&B, bi, 4, 1)) {
            for (int i = 0; i < 4; i++) { dst[i] == 0x12345678 here; }
        }
    }
    test_backends_free(&B);
}

TEST(test_no_shader) {
    struct umbra_buf dst_slot = {0};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     NULL, NULL,
                     NULL, NULL,
                     umbra_blend_src, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[4] = {
            0xFFFFFFFF,
            0xFFFFFFFF,
            0xFFFFFFFF,
            0xFFFFFFFF,
        };
        dst_slot = (struct umbra_buf){.ptr=dst, .count=4};
        if (test_backends_run(&B, bi, 4, 1)) {
            for (int i = 0; i < 4; i++) { dst[i] == 0 here; }
        }
    }
    test_backends_free(&B);
}

TEST(test_no_blend) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {1, 0, 1, 1};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     NULL, NULL,
                     umbra_shader_color, &color,
                     NULL, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[2] = {0, 0};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=2};
        if (test_backends_run(&B, bi, 2, 1)) {
            for (int i = 0; i < 2; i++) {
                (dst[i] & 0xFF) == 0xFF here;
                ((dst[i] >> 8) & 0xFF) == 0x00 here;
                ((dst[i] >> 16) & 0xFF) == 0xFF here;
                ((dst[i] >> 24) & 0xFF) == 0xFF here;
            }
        }
    }
    test_backends_free(&B);
}

struct test_gradient_state {
    float w, a;
};

static umbra_color_val32 test_gradient_fn(void *ctx, struct umbra_builder *builder,
                                          umbra_val32 x, umbra_val32 y) {
    struct test_gradient_state const *self = ctx;
    (void)y;
    umbra_ptr const u = umbra_bind_uniforms(builder, self, 2);
    umbra_val32 const w    = umbra_uniform_32(builder, u, 0),
                      a    = umbra_uniform_32(builder, u, 1),
                      t    = umbra_div_f32(builder, x, w),
                      zero = umbra_imm_i32(builder, 0);
    return (umbra_color_val32){t, zero, zero, a};
}

TEST(test_gradient_shader) {
    struct umbra_buf dst_slot = {0};
    struct test_gradient_state state = {.w = 4.0f, .a = 1.0f};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     NULL, NULL,
                     test_gradient_fn, &state,
                     umbra_blend_src, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[4] = {0};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=4};
        if (test_backends_run(&B, bi, 4, 1)) {
            int r0 = (int)(dst[0] & 0xFF);
            int r3 = (int)(dst[3] & 0xFF);
            r0 == 0 here;
            r3 == 191 here;
            for (int i = 0; i < 4; i++) { (((dst[i] >> 24) & 0xFF) == 0xFF) here; }
        }
    }
    test_backends_free(&B);
}

TEST(test_multiply_half_alpha) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {1, 0, 0, 0.5f};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     NULL, NULL,
                     umbra_shader_color, &color,
                     umbra_blend_multiply, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[2] = {0x80FF0000, 0x80FF0000};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=2};
        if (test_backends_run(&B, bi, 2, 1)) {
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
    test_backends_free(&B);
}

TEST(test_srcover_8888_n9) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {1, 0, 0, 0.5f};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     NULL, NULL,
                     umbra_shader_color, &color,
                     umbra_blend_srcover, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0, sizeof dst);
        dst_slot = (struct umbra_buf){.ptr=dst, .count=9};
        if (test_backends_run(&B, bi, 9, 1)) {
            for (int i = 0; i < 9; i++) {
                int r = (int)(dst[i] & 0xFF);
                int a = (int)((dst[i] >> 24) & 0xFF);
                r == 0xFF here;
                a == 128 here;
            }
        }
    }
    test_backends_free(&B);
}

TEST(test_full_pipeline) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {1, 0, 0, 1};
    umbra_rect rect = {2.0f, 0.0f, 7.0f, 1.0f};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     umbra_coverage_rect, &rect,
                     umbra_shader_color, &color,
                     umbra_blend_srcover, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[9];
        __builtin_memset(dst, 0, sizeof dst);
        dst_slot = (struct umbra_buf){.ptr=dst, .count=9};
        if (test_backends_run(&B, bi, 9, 1)) {
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
    test_backends_free(&B);
}

TEST(test_solid_src_fp16_n9) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {0.125f, 0.25f, 0.5f, 1};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_fp16, dx, dy,
                     NULL, NULL,
                     umbra_shader_color, &color,
                     umbra_blend_src, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __fp16 dst[4 * 9];
        __builtin_memset(dst, 0, sizeof dst);
        dst_slot = (struct umbra_buf){.ptr=dst, .count=9};
        if (test_backends_run(&B, bi, 9, 1)) {
            for (int i = 0; i < 9; i++) {
                equiv((float)dst[i * 4 + 0], 0.125f) here;
                equiv((float)dst[i * 4 + 1], 0.25f) here;
                equiv((float)dst[i * 4 + 2], 0.5f) here;
                equiv((float)dst[i * 4 + 3], 1.0f) here;
            }
        }
    }
    test_backends_free(&B);
}

TEST(test_coverage_rect_white_dst) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {1, 0, 0, 1};
    umbra_rect rect = {0, 0, 0, 0};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     umbra_coverage_rect, &rect,
                     umbra_shader_color, &color,
                     umbra_blend_srcover, NULL);
    struct test_backends B = make(builder);

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
            rect = (umbra_rect){rc.x0, 0.0f, rc.x1, 1.0f};
            dst_slot = (struct umbra_buf){.ptr=dst, .count=rc.n};
            if (test_backends_run(&B, bi, rc.n, 1)) {
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
    test_backends_free(&B);
}

TEST(test_coverage_bitmap) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {1, 1, 1, 1};
    uint8_t  cov_data[8] = {0, 128, 255, 0, 0, 0, 0, 0};
    struct umbra_buf buf = {.ptr=cov_data, .count=8};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     coverage_bitmap, &buf,
                     umbra_shader_color, &color,
                     umbra_blend_srcover, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        dst_slot = (struct umbra_buf){.ptr=dst, .count=8};
        if (test_backends_run(&B, bi, 8, 1)) {
            dst[0] == 0 here;
            (dst[1] & 0xff) == 128 here;
            (dst[2] & 0xff) == 0xff here;
            dst[3] == 0 here;
        }
    }
    test_backends_free(&B);
}

TEST(test_coverage_bitmap_565) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {1, 0, 0, 1};
    uint8_t  cov_data[16];
    for (int i = 0; i < 16; i++) { cov_data[i] = 255; }
    struct umbra_buf buf = {.ptr=cov_data, .count=16};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_565, dx, dy,
                     coverage_bitmap, &buf,
                     umbra_shader_color, &color,
                     umbra_blend_srcover, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint16_t dst[16];
        __builtin_memset(dst, 0, sizeof dst);
        dst_slot = (struct umbra_buf){.ptr=dst, .count=16, .stride=16};
        if (test_backends_run(&B, bi, 16, 1)) {
            for (int i = 0; i < 16; i++) {
                dst[i] == 0xF800u here;
            }
        }
    }
    test_backends_free(&B);
}

TEST(test_coverage_sdf) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {1, 1, 1, 1};
    uint8_t  cov_data[8] = {0, 100, 128, 200, 255, 0, 0, 0};
    struct umbra_buf buf = {.ptr=cov_data, .count=8};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     coverage_sdf, &buf,
                     umbra_shader_color, &color,
                     umbra_blend_srcover, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        dst_slot = (struct umbra_buf){.ptr=dst, .count=8};
        if (test_backends_run(&B, bi, 8, 1)) {
            dst[0] == 0 here;
            (dst[4] & 0xff) == 0xff here;
        }
    }
    test_backends_free(&B);
}

TEST(test_coverage_bitmap_matrix) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {1, 1, 1, 1};
    uint8_t  bmp[8] = {0, 0, 255, 0, 0, 0, 0, 0};
    struct coverage_bitmap2d sampler = {
        .buf = {.ptr=bmp, .count=8}, .w=8, .h=1,
    };
    struct umbra_matrix mat = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_point_val32 const p = umbra_transform_perspective(&mat, builder, dx, dy);
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, p.x, p.y,
                     coverage_bitmap2d, &sampler,
                     umbra_shader_color, &color,
                     umbra_blend_srcover, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        dst_slot = (struct umbra_buf){.ptr=dst, .count=8};
        if (test_backends_run(&B, bi, 8, 1)) {
            dst[0] == 0 here;
            (dst[2] & 0xff) == 0xff here;
            dst[3] == 0 here;
        }
    }
    test_backends_free(&B);
}

TEST(test_coverage_bitmap_matrix_565) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {1, 0, 0, 1};
    uint8_t  bmp[16];
    for (int i = 0; i < 16; i++) { bmp[i] = 255; }
    struct coverage_bitmap2d sampler = {
        .buf = {.ptr=bmp, .count=16}, .w=16, .h=1,
    };
    struct umbra_matrix mat = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_point_val32 const p = umbra_transform_perspective(&mat, builder, dx, dy);
    umbra_build_draw(builder, dst_ptr, umbra_fmt_565, p.x, p.y,
                     coverage_bitmap2d, &sampler,
                     umbra_shader_color, &color,
                     umbra_blend_srcover, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint16_t dst[16];
        __builtin_memset(dst, 0, sizeof dst);
        dst_slot = (struct umbra_buf){.ptr=dst, .count=16, .stride=16};
        if (test_backends_run(&B, bi, 16, 1)) {
            for (int i = 0; i < 16; i++) {
                dst[i] == 0xF800u here;
            }
        }
    }
    test_backends_free(&B);
}

TEST(test_coverage_bitmap_matrix_oob) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {1, 1, 1, 1};
    uint8_t  bmp[4] = {255, 0, 0, 0};
    struct coverage_bitmap2d sampler = {
        .buf = {.ptr=bmp, .count=4}, .w=2, .h=2,
    };
    struct umbra_matrix mat = {1, 0, 0, 0, 1, 0, 0.001f, 0, 1};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_point_val32 const p = umbra_transform_perspective(&mat, builder, dx, dy);
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, p.x, p.y,
                     coverage_bitmap2d, &sampler,
                     umbra_shader_color, &color,
                     umbra_blend_srcover, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[8];
        __builtin_memset(dst, 0, sizeof dst);
        dst_slot = (struct umbra_buf){.ptr=dst, .count=8};
        if (test_backends_run(&B, bi, 8, 1)) {
            (dst[0] & 0xff) == 0xFF here;
        }
    }
    test_backends_free(&B);
}

TEST(test_linear_2) {
    struct umbra_buf dst_slot = {0};
    struct gradient_linear coords =
        gradient_linear_from((umbra_point){0, 0}, (umbra_point){4, 0});
    struct shader_gradient_two_stops state = {
        .coords_fn  = gradient_linear,
        .coords_ctx = &coords,
        .c0         = (umbra_color){1, 0, 0, 1},
        .c1         = (umbra_color){0, 0, 1, 1},
    };
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     NULL, NULL,
                     shader_gradient_two_stops, &state,
                     umbra_blend_src, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[4] = {0};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=4};
        if (test_backends_run(&B, bi, 4, 1)) {
            (dst[0] & 0xff) == 0xFF here;
            ((dst[0] >> 16) & 0xff) == 0 here;
            (dst[3] & 0xff) == 64 here;
            ((dst[3] >> 16) & 0xff) == 191 here;
            for (int i = 0; i < 4; i++) { (((dst[i] >> 24) & 0xff) == 0xFF) here; }
        }
    }
    test_backends_free(&B);
}

TEST(test_radial_2) {
    struct umbra_buf dst_slot = {0};
    struct gradient_radial coords =
        gradient_radial_from((umbra_point){0, 0}, 10.0f);
    struct shader_gradient_two_stops state = {
        .coords_fn  = gradient_radial,
        .coords_ctx = &coords,
        .c0         = (umbra_color){1, 1, 1, 1},
        .c1         = (umbra_color){0, 0, 0, 1},
    };
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     NULL, NULL,
                     shader_gradient_two_stops, &state,
                     umbra_blend_src, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[1] = {0};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=1};
        if (test_backends_run(&B, bi, 1, 1)) {
            (dst[0] & 0xff) == 0xFF here;
            ((dst[0] >> 24) & 0xff) == 0xFF here;
        }
    }
    test_backends_free(&B);
}

TEST(test_linear_grad_evenly_spaced) {
    struct umbra_buf dst_slot = {0};
    float planar[3 * 4] = {
        1, 0, 0,
        0, 1, 0,
        0, 0, 1,
        1, 1, 1,
    };

    struct gradient_linear coords =
        gradient_linear_from((umbra_point){0, 0}, (umbra_point){8, 0});
    struct shader_gradient_evenly_spaced_stops state = {
        .coords_fn  = gradient_linear,
        .coords_ctx = &coords,
        .N          = 3.0f,
        .colors     = {.ptr=planar, .count=3 * 4},
    };
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     NULL, NULL,
                     shader_gradient_evenly_spaced_stops, &state,
                     umbra_blend_src, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[8] = {0};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=8};
        if (test_backends_run(&B, bi, 8, 1)) {
            (dst[0] & 0xff) == 0xFF here;
            ((dst[0] >>  8) & 0xff) ==    0 here;
            ((dst[0] >> 16) & 0xff) ==    0 here;
            ((dst[7] >> 16) & 0xff) ==  191 here;
            ((dst[7] >>  8) & 0xff) ==   64 here;
        }
    }
    test_backends_free(&B);
}

TEST(test_radial_grad_evenly_spaced) {
    struct umbra_buf dst_slot = {0};
    float planar[4 * 4] = {
        1, 1, 0, 0,
        0, 1, 1, 0,
        0, 0, 0, 1,
        1, 1, 1, 1,
    };

    struct gradient_radial coords =
        gradient_radial_from((umbra_point){0, 0}, 10.0f);
    struct shader_gradient_evenly_spaced_stops state = {
        .coords_fn  = gradient_radial,
        .coords_ctx = &coords,
        .N          = 4.0f,
        .colors     = {.ptr=planar, .count=4 * 4},
    };
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     NULL, NULL,
                     shader_gradient_evenly_spaced_stops, &state,
                     umbra_blend_src, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[1] = {0};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=1};
        if (test_backends_run(&B, bi, 1, 1)) {
            (dst[0] & 0xff) == 0xFF here;
            ((dst[0] >> 16) & 0xff) ==    0 here;
        }
    }
    test_backends_free(&B);
}

TEST(test_linear_stops) {
    struct umbra_buf dst_slot = {0};
    float colors_planar[3 * 4] = {1,0,0, 0,1,0, 0,0,1, 1,1,1};
    float pos[3] = {0.0f, 0.5f, 1.0f};

    struct gradient_linear coords =
        gradient_linear_from((umbra_point){0, 0}, (umbra_point){4, 0});
    struct shader_gradient state = {
        .coords_fn  = gradient_linear,
        .coords_ctx = &coords,
        .N          = 3.0f,
        .colors     = {.ptr=colors_planar, .count=12},
        .pos        = {.ptr=pos, .count=3},
    };
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     NULL, NULL,
                     shader_gradient, &state,
                     umbra_blend_src, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[5] = {0};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=5, .stride=5};
        if (test_backends_run(&B, bi, 5, 1)) {
            dst[0] == 0xff0000ffu here;
            dst[1] == 0xff008080u here;
            dst[2] == 0xff00ff00u here;
            dst[3] == 0xff808000u here;
            dst[4] == 0xffff0000u here;
        }
    }
    test_backends_free(&B);
}

TEST(test_linear_stops_fp16_planar) {
    struct umbra_buf dst_slot = {0};
    float colors_planar[3 * 4] = {1,0,0, 0,1,0, 0,0,1, 1,1,1};
    float pos[3] = {0.0f, 0.5f, 1.0f};

    struct gradient_linear coords =
        gradient_linear_from((umbra_point){0, 0}, (umbra_point){8, 0});
    struct shader_gradient state = {
        .coords_fn  = gradient_linear,
        .coords_ctx = &coords,
        .N          = 3.0f,
        .colors     = {.ptr=colors_planar, .count=12},
        .pos        = {.ptr=pos, .count=3},
    };
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_fp16_planar, dx, dy,
                     NULL, NULL,
                     shader_gradient, &state,
                     NULL, NULL);
    struct test_backends B = make(builder);

    enum { W = 8, HP = 1 };
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint16_t dst[W * HP * 4];
        __builtin_memset(dst, 0, sizeof dst);
        dst_slot = (struct umbra_buf){.ptr=dst, .count=W * HP * 4, .stride=W};
        if (test_backends_run(&B, bi, W, HP)) {
            dst[0*W+0] == 0x3c00 here;
            dst[0*W+2] == 0x3800 here;
            dst[0*W+4] == 0x0000 here;
            dst[1*W+4] == 0x3c00 here;
        }
    }
    test_backends_free(&B);
}

TEST(test_supersample) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {1, 0, 0, 1};
    struct umbra_supersample ss = {
        .inner_fn  = umbra_shader_color,
        .inner_ctx = &color,
        .samples   = 4,
    };
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     NULL, NULL,
                     umbra_shader_supersample, &ss,
                     umbra_blend_src, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[4] = {0};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=4};
        if (test_backends_run(&B, bi, 4, 1)) {
            for (int i = 0; i < 4; i++) {
                (dst[i] & 0xFF) == 0xFF here;
                ((dst[i] >> 24) & 0xFF) == 0xFF here;
            }
        }
    }
    test_backends_free(&B);
}

#if !defined(__wasm__)
#include <sys/mman.h>
#include <unistd.h>
#endif

TEST(test_page_aligned_buffer) {
#if defined(__wasm__)
    return;
#else
    struct umbra_buf dst_slot = {0};
    umbra_color color = {0, 1, 0, 1};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                     NULL, NULL,
                     umbra_shader_color, &color,
                     umbra_blend_src, NULL);
    struct test_backends B = make(builder);

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
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(aligned, 0, N * 4);
        dst_slot = (struct umbra_buf){.ptr=aligned, .count=N};
        if (test_backends_run(&B, bi, N, 1)) {
            for (int i = 0; i < N; i++) { aligned[i] == 0xFF00FF00u here; }

            __builtin_memset(offset, 0, N * 4);
            dst_slot = (struct umbra_buf){.ptr=offset, .count=N};
            if (test_backends_run(&B, bi, N, 1)) {
                for (int i = 0; i < N; i++) { offset[i] == 0xFF00FF00u here; }
            }
        }
    }
    munmap(page, alloc + (size_t)pgsz);
    test_backends_free(&B);
#endif
}

TEST(test_565_round_trip) {
    enum { W565 = 35 };
    uint16_t src[W565], dst[W565];
    for (int i = 0; i < W565; i++) { src[i] = (uint16_t)(0x1234u + (unsigned)i * 0x1111u); }
    struct umbra_buf src_buf = {.ptr=src, .count=W565},
                     dst_buf = {.ptr=dst, .count=W565};

    struct umbra_builder *b = umbra_builder();
    umbra_ptr const sp = umbra_bind_buf(b, &src_buf),
                      dp = umbra_bind_buf(b, &dst_buf);
    umbra_color_val32 c = umbra_load_565(b, sp);
    umbra_store_565(b, dp, c);
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    struct test_backends B = test_backends_make(ir);
    umbra_flat_ir_free(ir);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (test_backends_run(&B, bi, W565, 1)) {
            for (int i = 0; i < W565; i++) { dst[i] == src[i] here; }
        }
    }
    test_backends_free(&B);
}

TEST(test_1010102_round_trip) {
    uint32_t src[7], dst[7];
    for (int i = 0; i < 7; i++) {
        src[i] = ((unsigned)i * 73u) | ((unsigned)i * 37u << 10)
               | ((unsigned)i * 19u << 20) | (2u << 30);
    }
    struct umbra_buf src_buf = {.ptr=src, .count=7},
                     dst_buf = {.ptr=dst, .count=7};

    struct umbra_builder *b = umbra_builder();
    umbra_ptr const sp = umbra_bind_buf(b, &src_buf),
                      dp = umbra_bind_buf(b, &dst_buf);
    umbra_color_val32 c = umbra_load_1010102(b, sp);
    umbra_store_1010102(b, dp, c);
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    struct test_backends B = test_backends_make(ir);
    umbra_flat_ir_free(ir);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (test_backends_run(&B, bi, 7, 1)) {
            for (int i = 0; i < 7; i++) { dst[i] == src[i] here; }
        }
    }
    test_backends_free(&B);
}

TEST(test_fp16_planar_round_trip) {
    enum { WP = 8 };
    __fp16 src[WP * 4], dst[WP * 4];
    for (int i = 0; i < WP * 4; i++) { src[i] = (__fp16)(1.0f + (float)i * 0.1f); }
    struct umbra_buf src_buf = {.ptr=src, .count=WP * 4, .stride=WP},
                     dst_buf = {.ptr=dst, .count=WP * 4, .stride=WP};

    struct umbra_builder *b = umbra_builder();
    umbra_ptr const sp = umbra_bind_buf(b, &src_buf),
                      dp = umbra_bind_buf(b, &dst_buf);
    umbra_color_val32 c = umbra_load_fp16_planar(b, sp);
    umbra_store_fp16_planar(b, dp, c);
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    struct test_backends B = test_backends_make(ir);
    umbra_flat_ir_free(ir);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (test_backends_run(&B, bi, WP, 1)) {
            for (int i = 0; i < WP * 4; i++) { equiv((float)dst[i], (float)src[i]) here; }
        }
    }
    test_backends_free(&B);
}

TEST(test_solid_src_565) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {1, 0, 0, 1};
    struct umbra_builder *b = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(b, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(b, umbra_x(b)),
                      dy = umbra_f32_from_i32(b, umbra_y(b));
    umbra_build_draw(b, dst_ptr, umbra_fmt_565, dx, dy,
                     NULL, NULL,
                     umbra_shader_color, &color,
                     umbra_blend_src, NULL);

    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    struct test_backends B = test_backends_make(ir);
    umbra_flat_ir_free(ir);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint16_t dst[4] = {0};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=4};
        if (test_backends_run(&B, bi, 4, 1)) {
            for (int i = 0; i < 4; i++) { dst[i] == 0xF800 here; }
        }
    }
    test_backends_free(&B);
}

TEST(test_solid_src_1010102) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {0, 1, 0, 1};
    struct umbra_builder *b = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(b, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(b, umbra_x(b)),
                      dy = umbra_f32_from_i32(b, umbra_y(b));
    umbra_build_draw(b, dst_ptr, umbra_fmt_1010102, dx, dy,
                     NULL, NULL,
                     umbra_shader_color, &color,
                     umbra_blend_src, NULL);

    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    struct test_backends B = test_backends_make(ir);
    umbra_flat_ir_free(ir);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[4] = {0};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=4};
        if (test_backends_run(&B, bi, 4, 1)) {
            uint32_t expect = (1023u << 10) | (3u << 30);
            for (int i = 0; i < 4; i++) { dst[i] == expect here; }
        }
    }
    test_backends_free(&B);
}

TEST(test_solid_src_fp16_planar) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {0, 0, 1, 1};
    struct umbra_builder *b = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(b, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(b, umbra_x(b)),
                      dy = umbra_f32_from_i32(b, umbra_y(b));
    umbra_build_draw(b, dst_ptr, umbra_fmt_fp16_planar, dx, dy,
                     NULL, NULL,
                     umbra_shader_color, &color,
                     umbra_blend_src, NULL);

    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    struct test_backends B = test_backends_make(ir);
    umbra_flat_ir_free(ir);
    enum { WFP = 4 };
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __fp16 dst[WFP * 4];
        __builtin_memset(dst, 0, sizeof dst);
        dst_slot = (struct umbra_buf){.ptr=dst, .count=WFP * 4, .stride=WFP};
        if (test_backends_run(&B, bi, WFP, 1)) {
            for (int i = 0; i < WFP; i++) {
                equiv((float)dst[i + WFP*0], 0.0f) here;
                equiv((float)dst[i + WFP*1], 0.0f) here;
                equiv((float)dst[i + WFP*2], 1.0f) here;
                equiv((float)dst[i + WFP*3], 1.0f) here;
            }
        }
    }
    test_backends_free(&B);
}

// Property-style sweep over the shape that produced the original bug:
// dispatch widths that straddle the K=8 SIMD/tail split, heights > 1 so
// the row loop is exercised, and rect boundaries that land at each
// meaningful position (before SIMD, mid-SIMD, on the SIMD/tail seam,
// mid-tail).  For every combination, every backend must match the
// interpreter pixel for pixel.
static void coverage_rect_tail_matrix_case(int W, int H) {
    struct umbra_buf dst_slot = {0};
    float const rect_ls[] = {0.0f, 4.0f, 8.0f, 9.0f};
    float const rect_rs[] = {8.0f, 10.0f, 16.0f, 17.0f};

    for (int li = 0; li < count(rect_ls); li++) {
    for (int ri = 0; ri < count(rect_rs); ri++) {
        float const l = rect_ls[li], r = rect_rs[ri];
        if (l < r) {
            umbra_color color = {1, 0, 0, 1};
            umbra_rect  rect  = {l, 0.0f, r, (float)H};
            struct umbra_builder *builder = umbra_builder();
            umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
            umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                              dy = umbra_f32_from_i32(builder, umbra_y(builder));
            umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                             umbra_coverage_rect, &rect,
                             umbra_shader_color,  &color,
                             umbra_blend_srcover, NULL);
            struct test_backends B = make(builder);

            // Interp (bi=0) is always available; use it as the oracle and check
            // every other backend that compiled against its output.
            uint32_t ref[32 * 4] = {0};
            dst_slot = (struct umbra_buf){.ptr=ref, .count=W*H, .stride=W};
            test_backends_run(&B, 0, W, H) here;
            for (int bi = 1; bi < NUM_BACKENDS; bi++) {
                uint32_t dst[32 * 4] = {0};
                dst_slot = (struct umbra_buf){.ptr=dst, .count=W*H, .stride=W};
                if (test_backends_run(&B, bi, W, H)) {
                    for (int i = 0; i < W*H; i++) { dst[i] == ref[i] here; }
                }
            }
            test_backends_free(&B);
        }
    }}
}

#define RECT_TAIL_MATRIX_CASE(W, H)                            \
    TEST(test_coverage_rect_tail_matrix_w##W##_h##H) {         \
        coverage_rect_tail_matrix_case(W, H);                  \
    }
RECT_TAIL_MATRIX_CASE( 8, 2) RECT_TAIL_MATRIX_CASE( 8, 3)
RECT_TAIL_MATRIX_CASE( 9, 2) RECT_TAIL_MATRIX_CASE( 9, 3)
RECT_TAIL_MATRIX_CASE(10, 2) RECT_TAIL_MATRIX_CASE(10, 3)
RECT_TAIL_MATRIX_CASE(15, 2) RECT_TAIL_MATRIX_CASE(15, 3)
RECT_TAIL_MATRIX_CASE(16, 2) RECT_TAIL_MATRIX_CASE(16, 3)
RECT_TAIL_MATRIX_CASE(17, 2) RECT_TAIL_MATRIX_CASE(17, 3)
#undef RECT_TAIL_MATRIX_CASE

TEST(test_coverage_rect_tail_srcover_fp16_planar) {
    struct umbra_buf dst_slot = {0};
    // Regression: after a tail iteration runs (dispatch width not a multiple
    // of K=8), the JIT was leaving preamble registers (uniforms and the 0.0
    // imm constant) clobbered.  The next row's first SIMD iter then compared
    // x against garbage, producing wrong coverage on every row past the
    // first.  Rect x in [8,18), y in [1,3), dispatched 18x3 so each row has
    // both SIMD iters and a 2-col tail.
    umbra_color color = {1, 0, 0, 1};
    umbra_rect rect = {8.0f, 1.0f, 18.0f, 3.0f};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_fp16_planar, dx, dy,
                     umbra_coverage_rect, &rect,
                     umbra_shader_color, &color,
                     umbra_blend_srcover, NULL);
    struct test_backends B = make(builder);

    enum { W = 18, H = 3 };
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __fp16 dst[W * H * 4];
        for (int i = 0; i < W * H * 4; i++) { dst[i] = (__fp16)0.0f; }
        dst_slot = (struct umbra_buf){.ptr=dst, .count=W * H * 4, .stride=W};
        if (test_backends_run(&B, bi, W, H)) {
            for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                    _Bool const inside = x >= 8 && x < 18 && y >= 1 && y < 3;
                    float const r = (float)dst[y*W + x + W*H*0],
                                g = (float)dst[y*W + x + W*H*1],
                                b = (float)dst[y*W + x + W*H*2],
                                a = (float)dst[y*W + x + W*H*3];
                    equiv(r, inside ? 1.0f : 0.0f) here;
                    equiv(g, 0.0f) here;
                    equiv(b, 0.0f) here;
                    equiv(a, inside ? 1.0f : 0.0f) here;
                }
            }
        }
    }
    test_backends_free(&B);
}

TEST(test_srcover_fp16_planar) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {0, 0.5f, 0, 0.5f};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_fp16_planar, dx, dy,
                     NULL, NULL,
                     umbra_shader_color, &color,
                     umbra_blend_srcover, NULL);
    struct test_backends B = make(builder);

    enum { W = 16 };
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __fp16 dst[W * 4];
        for (int i = 0; i < W * 4; i++) {
            dst[i] = (__fp16)1.0f;
        }
        dst_slot = (struct umbra_buf){.ptr=dst, .count=W * 4, .stride=W};
        if (test_backends_run(&B, bi, W, 1)) {
            for (int i = 0; i < W; i++) {
                equiv((float)dst[i + W*0], 0.5f)  here;
                equiv((float)dst[i + W*1], 1.0f)  here;
                equiv((float)dst[i + W*2], 0.5f)  here;
                equiv((float)dst[i + W*3], 1.0f)  here;
            }
        }
    }
    test_backends_free(&B);
}

static umbra_interval test_rect_fn(void *ctx, struct umbra_builder *b,
                                    umbra_interval x, umbra_interval y) {
    umbra_rect const *self = ctx;
    umbra_ptr const u = umbra_bind_uniforms(b, self, sizeof *self / 4);
    umbra_interval const l  = umbra_interval_exact(umbra_uniform_32(b, u, 0)),
                         t  = umbra_interval_exact(umbra_uniform_32(b, u, 1)),
                         r  = umbra_interval_exact(umbra_uniform_32(b, u, 2)),
                         bo = umbra_interval_exact(umbra_uniform_32(b, u, 3));
    return umbra_interval_max_f32(b, umbra_interval_max_f32(b, umbra_interval_sub_f32(b, l, x),
                                                               umbra_interval_sub_f32(b, x, r)),
                                    umbra_interval_max_f32(b, umbra_interval_sub_f32(b, t, y),
                                                              umbra_interval_sub_f32(b, y, bo)));
}

TEST(test_sdf_dispatch_rect) {
    enum { W = 8, H = 8 };
    umbra_color color = {1, 0, 0, 1};
    // Rect edges sit at pixel centers (x.5), so pixel boxes at x in {2,5} or
    // y in {2,5} straddle a rect edge and have coverage == 0.5.
    umbra_rect  rect  = {2.5f, 2.5f, 5.5f, 5.5f};

    struct umbra_backend *bes[NUM_BACKENDS] = {
        umbra_backend_interp(), umbra_backend_jit(),
        umbra_backend_metal(),  umbra_backend_vulkan(),
        umbra_backend_wgpu(),
    };

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        if (bes[bi]) {
            struct umbra_buf dst_slot = {0};

            // Draw program on the target backend.
            struct umbra_builder *db = umbra_builder();
            umbra_ptr   const dst_ptr = umbra_bind_buf(db, &dst_slot);
            umbra_val32 const dx = umbra_f32_from_i32(db, umbra_x(db)),
                              dy = umbra_f32_from_i32(db, umbra_y(db));
            umbra_build_sdf_draw(db, dst_ptr, umbra_fmt_8888, dx, dy,
                                 test_rect_fn,        &rect,
                                 umbra_shader_color,  &color,
                                 umbra_blend_srcover, NULL);
            struct umbra_flat_ir *dir = umbra_flat_ir(db);
            umbra_builder_free(db);
            struct umbra_program *draw = bes[bi]->compile(bes[bi], dir);
            umbra_flat_ir_free(dir);
            draw != NULL here;

            struct umbra_builder *bb = umbra_builder();
            struct umbra_sdf_bounds_program *bounds =
                umbra_sdf_bounds_program(bb, NULL, test_rect_fn, &rect);
            umbra_builder_free(bb);

            uint32_t dst[W * H];
            __builtin_memset(dst, 0, sizeof dst);
            dst_slot = (struct umbra_buf){.ptr = dst, .count = W * H, .stride = W};
            umbra_sdf_dispatch(bounds, draw, draw, 0, 0, W, H, NULL, 0);
            bes[bi]->flush(bes[bi]);

            for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                    uint32_t const px = dst[y * W + x];
                    _Bool const inner = x >= 3 && x <= 4 && y >= 3 && y <= 4,
                                band  = x >= 2 && x <= 5 && y >= 2 && y <= 5,
                                edge  = band && !inner;
                    if (inner) {
                        (px & 0xFF)         == 0xFF here;
                        ((px >> 24) & 0xFF) == 0xFF here;
                    }
                    if (edge) {
                        (px & 0xFF)         == 0x80 here;
                        ((px >> 24) & 0xFF) == 0x80 here;
                    }
                    if (!band) {
                        px == 0 here;
                    }
                }
            }

            umbra_program_free(draw);
            umbra_sdf_bounds_program_free(bounds);
        }
    }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        umbra_backend_free(bes[bi]);
    }
}

struct test_circle_state {
    float cx, cy, r;
};
static umbra_interval test_circle_fn(void *ctx, struct umbra_builder *b,
                                      umbra_interval x, umbra_interval y) {
    struct test_circle_state const *self = ctx;
    umbra_ptr const u = umbra_bind_uniforms(b, self, 3);
    umbra_interval const cx = umbra_interval_exact(umbra_uniform_32(b, u, 0)),
                         cy = umbra_interval_exact(umbra_uniform_32(b, u, 1)),
                         r  = umbra_interval_exact(umbra_uniform_32(b, u, 2));
    umbra_interval const dx = umbra_interval_sub_f32(b, x, cx),
                         dy = umbra_interval_sub_f32(b, y, cy),
                         d2 = umbra_interval_fma_f32(b, dx, dx,
                                  umbra_interval_mul_f32(b, dy, dy)),
                         d  = umbra_interval_sqrt_f32(b, d2);
    return umbra_interval_sub_f32(b, d, r);
}

TEST(test_sdf_dispatch_tiling) {
    // Canvas larger than the SDF tile size so the grid has multiple tiles.
    enum { W = 1024, H = 768 };

    struct test_circle_state sdf   = {.cx = 512, .cy = 384, .r = 180};
    umbra_color              color = {1, 0, 0, 1};

    struct umbra_backend *be = umbra_backend_jit();
    if (!be) { be = umbra_backend_interp(); }

    struct umbra_buf tiled_dst_slot = {0};
    struct umbra_buf flat_dst_slot  = {0};

    // Tiled draw program.
    struct umbra_builder *db = umbra_builder();
    umbra_ptr   const td_dst = umbra_bind_buf(db, &tiled_dst_slot);
    umbra_val32 const tx     = umbra_f32_from_i32(db, umbra_x(db)),
                      ty     = umbra_f32_from_i32(db, umbra_y(db));
    umbra_build_sdf_draw(db, td_dst, umbra_fmt_8888, tx, ty,
                         test_circle_fn,      &sdf,
                         umbra_shader_color,  &color,
                         umbra_blend_srcover, NULL);
    struct umbra_flat_ir *dir = umbra_flat_ir(db);
    umbra_builder_free(db);
    struct umbra_program *tiled = be->compile(be, dir);
    umbra_flat_ir_free(dir);

    struct umbra_builder *bb = umbra_builder();
    struct umbra_sdf_bounds_program *bounds =
        umbra_sdf_bounds_program(bb, NULL, test_circle_fn, &sdf);
    umbra_builder_free(bb);

    // Flat reference: same shader+coverage, no tiling.
    struct umbra_builder *fb = umbra_builder();
    umbra_ptr   const fdst = umbra_bind_buf(fb, &flat_dst_slot);
    umbra_val32 const fx   = umbra_f32_from_i32(fb, umbra_x(fb)),
                      fy   = umbra_f32_from_i32(fb, umbra_y(fb));
    umbra_build_sdf_draw(fb, fdst, umbra_fmt_8888, fx, fy,
                         test_circle_fn,      &sdf,
                         umbra_shader_color,  &color,
                         umbra_blend_srcover, NULL);
    struct umbra_flat_ir *fir = umbra_flat_ir(fb);
    umbra_builder_free(fb);
    struct umbra_program *flat = be->compile(be, fir);
    umbra_flat_ir_free(fir);

    uint32_t *tiled_buf = calloc(W * H, sizeof *tiled_buf);
    uint32_t *flat_buf  = calloc(W * H, sizeof *flat_buf);

    tiled_dst_slot = (struct umbra_buf){.ptr = tiled_buf, .count = W * H, .stride = W};
    umbra_sdf_dispatch(bounds, tiled, tiled, 0, 0, W, H, NULL, 0);

    flat_dst_slot = (struct umbra_buf){.ptr = flat_buf, .count = W * H, .stride = W};
    flat->queue(flat, 0, 0, W, H, NULL, 0);

    be->flush(be);

    // Every pixel must match.
    int mismatches = 0;
    for (int i = 0; i < W * H; i++) {
        if (tiled_buf[i] != flat_buf[i]) { mismatches++; }
    }
    mismatches == 0 here;

    free(flat_buf);
    free(tiled_buf);
    umbra_program_free(flat);
    umbra_program_free(tiled);
    umbra_sdf_bounds_program_free(bounds);
    umbra_backend_free(be);
}

TEST(test_sdf_stroke_ring) {
    // Stroke a filled disk: the result is a ring, so pixels deep inside the
    // disk and pixels well outside it are both empty, while pixels on the
    // original circle boundary are fully covered.
    enum { W = 64, H = 64 };
    struct test_circle_state inner = {.cx = 32, .cy = 32, .r = 24};
    struct umbra_sdf_stroke  sdf   = {.inner_fn = test_circle_fn,
                                       .inner_ctx = &inner,
                                       .width = 6};
    umbra_color color = {1, 0, 0, 1};

    struct umbra_buf dst_slot = {0};

    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                      dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_sdf_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy,
                         umbra_sdf_stroke,    &sdf,
                         umbra_shader_color,  &color,
                         umbra_blend_srcover, NULL);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t *dst = calloc(W * H, sizeof *dst);
        dst_slot = (struct umbra_buf){.ptr = dst, .count = W * H, .stride = W};
        if (test_backends_run(&B, bi, W, H)) {
            dst[32 * W + 32] == 0        here;  // disk center: outside the ring
            dst[32 * W +  8] == 0xFF0000FFu here;  // on boundary at left (R=24)
            dst[32 * W + 56] == 0xFF0000FFu here;  // on boundary at right (R=24)
            dst[ 4 * W +  4] == 0        here;  // far outside
        }
        free(dst);
    }
    test_backends_free(&B);
}

// Concurrent-draw regression tests.  The driving concern is that CPU-backend
// programs (interp, jit) advertise queue_is_threadsafe=1; TSAN should see a
// clean run even with N threads dispatching against shared programs.  We pair
// each run with an equivalent serial baseline and assert every pixel matches.

struct race_ctx {
    void (*fire)(void *);  // dispatch in the per-thread worker
    void *ctx;
    uint32_t const *baseline;
    uint32_t       *dst;
    int             pixels, mismatches, iters, :32;
};

static void* race_worker(void *v) {
    struct race_ctx *c = v;
    for (int it = 0; it < c->iters; it++) {
        __builtin_memset(c->dst, 0, (size_t)c->pixels * sizeof *c->dst);
        c->fire(c->ctx);
        for (int p = 0; p < c->pixels; p++) {
            if (c->dst[p] != c->baseline[p]) { c->mismatches++; }
        }
    }
    return NULL;
}

struct sdf_fire_ctx {
    struct umbra_sdf_bounds_program *bounds;
    struct umbra_program            *draw;
    struct umbra_buf                *dst_slot;
    uint32_t                        *dst;
    int                              W, H;
};
static void sdf_fire(void *v) {
    struct sdf_fire_ctx *c = v;
    *c->dst_slot = (struct umbra_buf){.ptr = c->dst, .count = c->W * c->H, .stride = c->W};
    umbra_sdf_dispatch(c->bounds, c->draw, c->draw, 0, 0, c->W, c->H, NULL, 0);
}

static void sdf_thread_safety_for(struct umbra_backend *be) {
    if (!be) { return; }
    enum { W = 256, H = 256, N_THREADS = 4, ITERS = 8 };

    struct test_circle_state sdf   = {.cx = 128, .cy = 128, .r = 64};
    umbra_color              color = {1, 0, 0, 1};

    struct umbra_builder *bb = umbra_builder();
    struct umbra_sdf_bounds_program *bounds =
        umbra_sdf_bounds_program(bb, NULL, test_circle_fn, &sdf);
    umbra_builder_free(bb);

    // Per-thread draw program + dst slot so the draw side isn't also racing.
    struct umbra_program *draw[N_THREADS];
    struct umbra_buf      dst_slot[N_THREADS] = {{0}};
    uint32_t             *dst[N_THREADS];
    for (int i = 0; i < N_THREADS; i++) {
        struct umbra_builder *db = umbra_builder();
        umbra_ptr   const dp = umbra_bind_buf(db, &dst_slot[i]);
        umbra_val32 const dx = umbra_f32_from_i32(db, umbra_x(db)),
                          dy = umbra_f32_from_i32(db, umbra_y(db));
        umbra_build_sdf_draw(db, dp, umbra_fmt_8888, dx, dy,
                             test_circle_fn,      &sdf,
                             umbra_shader_color,  &color,
                             umbra_blend_srcover, NULL);
        struct umbra_flat_ir *ir = umbra_flat_ir(db);
        umbra_builder_free(db);
        draw[i] = be->compile(be, ir);
        umbra_flat_ir_free(ir);
        dst[i] = calloc(W * H, sizeof *dst[i]);
    }

    uint32_t *baseline = calloc(W * H, sizeof *baseline);
    struct sdf_fire_ctx fire0 = {
        .bounds = bounds, .draw = draw[0],
        .dst_slot = &dst_slot[0], .dst = baseline, .W = W, .H = H,
    };
    sdf_fire(&fire0);
    be->flush(be);

    pthread_t           th  [N_THREADS];
    struct race_ctx     ctx [N_THREADS];
    struct sdf_fire_ctx fire[N_THREADS];
    for (int i = 0; i < N_THREADS; i++) {
        fire[i] = (struct sdf_fire_ctx){
            .bounds = bounds, .draw = draw[i],
            .dst_slot = &dst_slot[i], .dst = dst[i], .W = W, .H = H,
        };
        ctx[i] = (struct race_ctx){
            .fire = sdf_fire, .ctx = &fire[i],
            .baseline = baseline, .dst = dst[i],
            .pixels = W * H, .iters = ITERS,
        };
        pthread_create(&th[i], NULL, race_worker, &ctx[i]);
    }
    for (int i = 0; i < N_THREADS; i++) { pthread_join(th[i], NULL); }
    be->flush(be);

    for (int i = 0; i < N_THREADS; i++) { ctx[i].mismatches == 0 here; }

    free(baseline);
    for (int i = 0; i < N_THREADS; i++) {
        free(dst[i]);
        umbra_program_free(draw[i]);
    }
    umbra_sdf_bounds_program_free(bounds);
    umbra_backend_free(be);
}

TEST(test_sdf_dispatch_thread_safety_interp) { sdf_thread_safety_for(umbra_backend_interp()); }
TEST(test_sdf_dispatch_thread_safety_jit)    { sdf_thread_safety_for(umbra_backend_jit()); }

// Non-SDF: one shared program, each thread dispatches its own disjoint
// y-strip of the canvas.  Exercises concurrent access to the program's
// per-dispatch scratch (interp v/var; jit's register allocation uses stack
// so it's safe by construction but worth covering).  Dst writes are
// disjoint between threads, so TSAN only fires on true scratch races.
struct strip_fire_ctx {
    struct umbra_program *p;
    int                   l, t, r, b;
};
static void strip_fire(void *v) {
    struct strip_fire_ctx *c = v;
    c->p->queue(c->p, c->l, c->t, c->r, c->b, NULL, 0);
}

static void draw_thread_safety_for(struct umbra_backend *be) {
    if (!be) { return; }
    enum { W = 128, H = 128, N_THREADS = 4, ITERS = 16 };

    umbra_color color = {0.25f, 0.5f, 0.75f, 1.0f};
    umbra_rect  rect  = {8.5f, 8.5f, 120.5f, 120.5f};

    uint32_t         *full     = calloc(W * H, sizeof *full);
    struct umbra_buf  dst_slot = {.ptr = full, .count = W * H, .stride = W};

    struct umbra_builder *b = umbra_builder();
    umbra_ptr   const dp = umbra_bind_buf(b, &dst_slot);
    umbra_val32 const dx = umbra_f32_from_i32(b, umbra_x(b)),
                      dy = umbra_f32_from_i32(b, umbra_y(b));
    umbra_build_draw(b, dp, umbra_fmt_8888, dx, dy,
                     umbra_coverage_rect, &rect,
                     umbra_shader_color,  &color,
                     umbra_blend_srcover, NULL);
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    struct umbra_program *p = be->compile(be, ir);
    umbra_flat_ir_free(ir);

    uint32_t *baseline = calloc(W * H, sizeof *baseline);
    dst_slot.ptr = baseline;
    p->queue(p, 0, 0, W, H, NULL, 0);
    be->flush(be);

    // N threads, disjoint y-strips, all dispatching against the shared
    // program + shared `full` dst (disjoint pixel ranges, no write overlap).
    dst_slot.ptr = full;
    int const           sh = (H + N_THREADS - 1) / N_THREADS;
    pthread_t             th[N_THREADS];
    struct race_ctx      ctx[N_THREADS];
    struct strip_fire_ctx fire[N_THREADS];
    uint32_t            *tbase[N_THREADS];
    uint32_t            *tdst [N_THREADS];
    for (int i = 0; i < N_THREADS; i++) {
        int const y0 = i * sh,
                  y1 = y0 + sh > H ? H : y0 + sh;
        int const pixels = W * (y1 - y0);
        tbase[i] = baseline + y0 * W;
        tdst [i] = full     + y0 * W;
        fire[i]  = (struct strip_fire_ctx){.p = p, .l = 0, .t = y0, .r = W, .b = y1};
        ctx[i]   = (struct race_ctx){
            .fire = strip_fire, .ctx = &fire[i],
            .baseline = tbase[i], .dst = tdst[i],
            .pixels = pixels, .iters = ITERS,
        };
        pthread_create(&th[i], NULL, race_worker, &ctx[i]);
    }
    for (int i = 0; i < N_THREADS; i++) { pthread_join(th[i], NULL); }
    be->flush(be);

    for (int i = 0; i < N_THREADS; i++) { ctx[i].mismatches == 0 here; }

    free(baseline);
    free(full);
    umbra_program_free(p);
    umbra_backend_free(be);
}

TEST(test_draw_thread_safety_interp) { draw_thread_safety_for(umbra_backend_interp()); }
TEST(test_draw_thread_safety_jit)    { draw_thread_safety_for(umbra_backend_jit()); }

TEST(test_metal_loop_gather) {
    // Sum of a[i] for i=0..2 via loop + bind_buf + gather.
    // a = {10, 20, 30}, expected sum = 60.
    struct uni {
        float n;
        int   :32;
    };
    int const n_slot = (int)(__builtin_offsetof(struct uni, n) / 4);

    float arr[] = {10, 20, 30};
    struct umbra_buf arr_buf = {.ptr = arr, .count = 3};

    struct uni uniforms = {0};
    { int const count = 3; __builtin_memcpy(&uniforms.n, &count, 4); }

    float out = 0;
    struct umbra_buf out_buf = {.ptr = &out, .count = 1};

    struct umbra_builder *b = umbra_builder();
    umbra_ptr const u    = umbra_bind_uniforms(b, &uniforms, (int)(sizeof uniforms / 4));
    umbra_ptr const dst  = umbra_bind_buf(b, &out_buf);
    umbra_ptr const data = umbra_bind_buf(b, &arr_buf);
    umbra_val32 const n    = umbra_uniform_32(b, u, n_slot);

    umbra_var32 sum = umbra_declare_var32(b, umbra_imm_f32(b, 0.0f));

    umbra_val32 const j = umbra_loop(b, n); {
        umbra_val32 const val = umbra_gather_32(b, data, j);
        umbra_store_var32(b, sum, umbra_add_f32(b, umbra_load_var32(b, sum), val));
    } umbra_end_loop(b);

    umbra_store_32(b, dst, umbra_load_var32(b, sum));

    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);

    struct umbra_backend *bes[NUM_BACKENDS] = {
        umbra_backend_interp(), umbra_backend_jit(),
        umbra_backend_metal(),  umbra_backend_vulkan(),
        umbra_backend_wgpu(),
    };

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        if (bes[bi]) {
            struct umbra_program *prog = bes[bi]->compile(bes[bi], ir);
            out = 0;
            prog->queue(prog, 0, 0, 1, 1, NULL, 0);
            bes[bi]->flush(bes[bi]);
            equiv(out, 60.0f) here;
            umbra_program_free(prog);
        }
    }

    umbra_flat_ir_free(ir);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        umbra_backend_free(bes[bi]);
    }
}

