#include "../include/umbra_draw.h"
#include "../slides/coverage.h"
#include "../slides/gradient.h"
#include "../src/count.h"
#include "test.h"
#include <stdint.h>
#include <stdlib.h>

enum { sdf_shim_tile = 512 };

struct sdf_shim {
    struct umbra_program *draw;
    struct umbra_program *bounds;
    struct umbra_backend *bounds_be;
    struct umbra_sdf_grid grid;
    struct umbra_buf      cov_buf;
    struct umbra_buf      dst_slot;
    uint16_t             *cov;
    int                   cov_cap, :32;
};

static struct sdf_shim* sdf_shim_build(struct umbra_backend *draw_be,
                                       umbra_sdf     sdf_fn, void *sdf_ctx,
                                       umbra_shader  shader_fn, void *shader_ctx,
                                       umbra_blend   blend_fn,  void *blend_ctx,
                                       struct umbra_fmt fmt) {
    struct sdf_shim *w = calloc(1, sizeof *w);

    struct umbra_builder *db = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(db, &w->dst_slot);
    umbra_val32 const x = umbra_f32_from_i32(db, umbra_x(db)),
                      y = umbra_f32_from_i32(db, umbra_y(db));
    umbra_build_sdf_draw(db, dst_ptr, fmt, x, y,
                         sdf_fn, sdf_ctx, 0,
                         shader_fn, shader_ctx,
                         blend_fn,  blend_ctx);
    struct umbra_flat_ir *dir = umbra_flat_ir(db);
    umbra_builder_free(db);
    w->draw = draw_be->compile(draw_be, dir);
    umbra_flat_ir_free(dir);
    if (!w->draw) {
        free(w);
        return NULL;
    }

    struct umbra_builder *bb = umbra_builder();
    umbra_ptr const cov_ptr = umbra_bind_buf(bb, &w->cov_buf);
    umbra_interval ix, iy;
    umbra_sdf_tile_intervals(bb, &w->grid, NULL, &ix, &iy);
    umbra_build_sdf_bounds(bb, cov_ptr, ix, iy, sdf_fn, sdf_ctx);
    struct umbra_flat_ir *bir = umbra_flat_ir(bb);
    umbra_builder_free(bb);
    struct umbra_backend *bounds_be = umbra_backend_jit();
    if (!bounds_be) { bounds_be = umbra_backend_interp(); }
    w->bounds    = bounds_be->compile(bounds_be, bir);
    w->bounds_be = bounds_be;
    umbra_flat_ir_free(bir);
    return w;
}

static void sdf_shim_queue(struct sdf_shim *w,
                           int l, int t, int r, int b, struct umbra_buf dst) {
    w->dst_slot = dst;
    int const T  = sdf_shim_tile,
              xt = (r - l + T - 1) / T,
              yt = (b - t + T - 1) / T,
              tiles = xt * yt;
    if (tiles > w->cov_cap) {
        w->cov     = realloc(w->cov, (size_t)tiles * sizeof *w->cov);
        w->cov_cap = tiles;
    }
    w->cov_buf = (struct umbra_buf){.ptr = w->cov, .count = tiles, .stride = xt};
    umbra_sdf_dispatch(w->bounds, w->draw, &w->grid, &w->cov_buf, T, l, t, r, b);
}

static void sdf_shim_free(struct sdf_shim *w) {
    if (w) {
        umbra_program_free(w->draw);
        umbra_program_free(w->bounds);
        umbra_backend_free(w->bounds_be);
        free(w->cov);
        free(w);
    }
}

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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, umbra_shader_color, &color, NULL, NULL);

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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, umbra_shader_color, &color, umbra_blend_src, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, umbra_shader_color, &color, umbra_blend_src, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, umbra_shader_color, &color, umbra_blend_src, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, umbra_shader_color, &color, umbra_blend_src, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, umbra_shader_color, &color, umbra_blend_srcover, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, umbra_shader_color, &color, umbra_blend_dstover, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, umbra_shader_color, &color, umbra_blend_dstover, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, umbra_shader_color, &color, umbra_blend_multiply, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_fp16, dx, dy, NULL, NULL, umbra_shader_color, &color, umbra_blend_src, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_fp16, dx, dy, NULL, NULL, umbra_shader_color, &color, umbra_blend_srcover, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, umbra_coverage_rect, &rect, umbra_shader_color, &color, umbra_blend_srcover, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, umbra_coverage_rect, &rect, umbra_shader_color, &color, umbra_blend_srcover, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, umbra_coverage_rect, &rect, umbra_shader_color, &color, umbra_blend_srcover, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, umbra_coverage_rect, &rect, umbra_shader_color, &color, umbra_blend_src, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, umbra_coverage_rect, &rect, umbra_shader_color, &color, umbra_blend_srcover, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, NULL, NULL, umbra_blend_src, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, umbra_shader_color, &color, NULL, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, test_gradient_fn, &state, umbra_blend_src, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, umbra_shader_color, &color, umbra_blend_multiply, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, umbra_shader_color, &color, umbra_blend_srcover, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, umbra_coverage_rect, &rect, umbra_shader_color, &color, umbra_blend_srcover, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_fp16, dx, dy, NULL, NULL, umbra_shader_color, &color, umbra_blend_src, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, umbra_coverage_rect, &rect, umbra_shader_color, &color, umbra_blend_srcover, NULL);
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
    uint16_t cov_data[8] = {0, 128, 255, 0, 0, 0, 0, 0};
    struct umbra_buf buf = {.ptr=cov_data, .count=8};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, coverage_bitmap, &buf, umbra_shader_color, &color, umbra_blend_srcover, NULL);
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
    uint16_t cov_data[16];
    for (int i = 0; i < 16; i++) { cov_data[i] = 255; }
    struct umbra_buf buf = {.ptr=cov_data, .count=16};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_565, dx, dy, coverage_bitmap, &buf, umbra_shader_color, &color, umbra_blend_srcover, NULL);
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
    uint16_t cov_data[8] = {0, 100, 128, 200, 255, 0, 0, 0};
    struct umbra_buf buf = {.ptr=cov_data, .count=8};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, coverage_sdf, &buf, umbra_shader_color, &color, umbra_blend_srcover, NULL);
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
    uint16_t bmp[8] = {0, 0, 255, 0, 0, 0, 0, 0};
    struct coverage_bitmap2d sampler = {
        .buf = {.ptr=bmp, .count=8}, .w=8, .h=1,
    };
    struct umbra_matrix mat = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_point_val32 const p = umbra_transform_perspective(&mat, builder, dx, dy);
    dx = p.x;
    dy = p.y;
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, coverage_bitmap2d, &sampler, umbra_shader_color, &color, umbra_blend_srcover, NULL);
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
    uint16_t bmp[16];
    for (int i = 0; i < 16; i++) { bmp[i] = 255; }
    struct coverage_bitmap2d sampler = {
        .buf = {.ptr=bmp, .count=16}, .w=16, .h=1,
    };
    struct umbra_matrix mat = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_point_val32 const p = umbra_transform_perspective(&mat, builder, dx, dy);
    dx = p.x;
    dy = p.y;
    umbra_build_draw(builder, dst_ptr, umbra_fmt_565, dx, dy, coverage_bitmap2d, &sampler, umbra_shader_color, &color, umbra_blend_srcover, NULL);
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
    uint16_t bmp[4] = {255, 0, 0, 0};
    struct coverage_bitmap2d sampler = {
        .buf = {.ptr=bmp, .count=4}, .w=2, .h=2,
    };
    struct umbra_matrix mat = {1, 0, 0, 0, 1, 0, 0.001f, 0, 1};
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_point_val32 const p = umbra_transform_perspective(&mat, builder, dx, dy);
    dx = p.x;
    dy = p.y;
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, coverage_bitmap2d, &sampler, umbra_shader_color, &color, umbra_blend_srcover, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, shader_gradient_two_stops, &state, umbra_blend_src, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, shader_gradient_two_stops, &state, umbra_blend_src, NULL);
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

TEST(test_linear_grad) {
    struct umbra_buf dst_slot = {0};
    umbra_color const stop_colors[] = {
        {1, 0, 0, 1},
        {0, 1, 0, 1},
        {0, 0, 1, 1},
    };
    float lut[256 * 4];
    float pos[3];
    for (int i = 0; i < 3; i++) { pos[i] = (float)i / 2.0f; }
    gradient_lut(lut, 256, 3, pos, stop_colors);

    struct gradient_linear coords =
        gradient_linear_from((umbra_point){0, 0}, (umbra_point){8, 0});
    struct shader_gradient_lut state = {
        .coords_fn  = gradient_linear,
        .coords_ctx = &coords,
        .N          = 256.0f,
        .lut        = {.ptr=lut, .count=256 * 4},
    };
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, shader_gradient_lut, &state, umbra_blend_src, NULL);
    struct test_backends B = make(builder);


    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[8] = {0};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=8};
        if (test_backends_run(&B, bi, 8, 1)) {
            (dst[0] & 0xff) == 0xFF here;
            ((dst[0] >> 8) & 0xff) == 0 here;
            ((dst[7] >> 16) & 0xff) == 191 here;
        }
    }
    test_backends_free(&B);
}

TEST(test_radial_grad) {
    struct umbra_buf dst_slot = {0};
    umbra_color const stop_colors[] = {
        {1, 0, 0, 1},
        {1, 1, 0, 1},
        {0, 1, 0, 1},
        {0, 0, 1, 1},
    };
    float lut[64 * 4];
    float pos[4];
    for (int i = 0; i < 4; i++) { pos[i] = (float)i / 3.0f; }
    gradient_lut(lut, 64, 4, pos, stop_colors);

    struct gradient_radial coords =
        gradient_radial_from((umbra_point){0, 0}, 10.0f);
    struct shader_gradient_lut state = {
        .coords_fn  = gradient_radial,
        .coords_ctx = &coords,
        .N          = 64.0f,
        .lut        = {.ptr=lut, .count=64 * 4},
    };
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, shader_gradient_lut, &state, umbra_blend_src, NULL);
    struct test_backends B = make(builder);


    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[1] = {0};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=1};
        if (test_backends_run(&B, bi, 1, 1)) {
            (dst[0] & 0xff) == 0xFF here;
            ((dst[0] >> 16) & 0xff) == 0 here;
        }
    }
    test_backends_free(&B);
}

TEST(test_lut_grad_last_pixel) {
    struct umbra_buf dst_slot = {0};
    float lut[3 * 4] = {
        1, 0, 0,
        0, 1, 0,
        0, 0, 1,
        1, 1, 1,
    };

    struct gradient_linear coords =
        gradient_linear_from((umbra_point){0, 0}, (umbra_point){7, 0});
    struct shader_gradient_lut state = {
        .coords_fn  = gradient_linear,
        .coords_ctx = &coords,
        .N          = 3.0f,
        .lut        = {.ptr=lut, .count=3 * 4},
    };
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, shader_gradient_lut, &state, umbra_blend_src, NULL);
    struct test_backends B = make(builder);


    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[8] = {0};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=8};
        if (test_backends_run(&B, bi, 8, 1)) {
            (dst[0] & 0xff) == 0xFF here;
            ((dst[7] >> 16) & 0xff) == 0xFF here;
            ((dst[7] >>  8) & 0xff) ==    0 here;
            ( dst[7]        & 0xff) ==    0 here;
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, shader_gradient_evenly_spaced_stops, &state, umbra_blend_src, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, shader_gradient_evenly_spaced_stops, &state, umbra_blend_src, NULL);
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

TEST(test_gradient_lut_nonuniform) {
    struct umbra_buf dst_slot = {0};
    umbra_color const stop_colors[] = {
        {1, 0, 0, 1},
        {0, 1, 0, 1},
        {0, 0, 1, 1},
    };
    float positions[] = {0, 0.2f, 1.0f};
    float lut[64 * 4];
    gradient_lut(lut, 64, 3, positions, stop_colors);

    struct gradient_linear coords =
        gradient_linear_from((umbra_point){0, 0}, (umbra_point){8, 0});
    struct shader_gradient_lut state = {
        .coords_fn  = gradient_linear,
        .coords_ctx = &coords,
        .N          = 64.0f,
        .lut        = {.ptr=lut, .count=64 * 4},
    };
    struct umbra_builder *builder = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, shader_gradient_lut, &state, umbra_blend_src, NULL);
    struct test_backends B = make(builder);


    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t  dst[8] = {0};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=8};
        if (test_backends_run(&B, bi, 8, 1)) {
            (dst[0] & 0xff) == 0xFF here;
            ((dst[7] >> 16) & 0xff) == 215 here;
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, shader_gradient, &state, umbra_blend_src, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_fp16_planar, dx, dy, NULL, NULL, shader_gradient, &state, NULL, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, umbra_shader_supersample, &ss, umbra_blend_src, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, NULL, NULL, umbra_shader_color, &color, umbra_blend_src, NULL);
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
        if (!test_backends_run(&B, bi, W565, 1)) { continue; }
        for (int i = 0; i < W565; i++) { dst[i] == src[i] here; }
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
        if (!test_backends_run(&B, bi, 7, 1)) { continue; }
        for (int i = 0; i < 7; i++) { dst[i] == src[i] here; }
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
        if (!test_backends_run(&B, bi, WP, 1)) { continue; }
        for (int i = 0; i < WP * 4; i++) { equiv((float)dst[i], (float)src[i]) here; }
    }
    test_backends_free(&B);
}

TEST(test_solid_src_565) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {1, 0, 0, 1};
    struct umbra_builder *b = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(b, &dst_slot);
    umbra_val32 dx = umbra_f32_from_i32(b, umbra_x(b)),
                dy = umbra_f32_from_i32(b, umbra_y(b));
    umbra_build_draw(b, dst_ptr, umbra_fmt_565, dx, dy, NULL, NULL, umbra_shader_color, &color, umbra_blend_src, NULL);

    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    struct test_backends B = test_backends_make(ir);
    umbra_flat_ir_free(ir);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint16_t dst[4] = {0};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=4};
        if (!test_backends_run(&B, bi, 4, 1)) { continue; }
        for (int i = 0; i < 4; i++) { dst[i] == 0xF800 here; }
    }
    test_backends_free(&B);
}

TEST(test_solid_src_1010102) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {0, 1, 0, 1};
    struct umbra_builder *b = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(b, &dst_slot);
    umbra_val32 dx = umbra_f32_from_i32(b, umbra_x(b)),
                dy = umbra_f32_from_i32(b, umbra_y(b));
    umbra_build_draw(b, dst_ptr, umbra_fmt_1010102, dx, dy, NULL, NULL, umbra_shader_color, &color, umbra_blend_src, NULL);

    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    struct test_backends B = test_backends_make(ir);
    umbra_flat_ir_free(ir);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[4] = {0};
        dst_slot = (struct umbra_buf){.ptr=dst, .count=4};
        if (!test_backends_run(&B, bi, 4, 1)) { continue; }
        uint32_t expect = (1023u << 10) | (3u << 30);
        for (int i = 0; i < 4; i++) { dst[i] == expect here; }
    }
    test_backends_free(&B);
}

TEST(test_solid_src_fp16_planar) {
    struct umbra_buf dst_slot = {0};
    umbra_color color = {0, 0, 1, 1};
    struct umbra_builder *b = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(b, &dst_slot);
    umbra_val32 dx = umbra_f32_from_i32(b, umbra_x(b)),
                dy = umbra_f32_from_i32(b, umbra_y(b));
    umbra_build_draw(b, dst_ptr, umbra_fmt_fp16_planar, dx, dy, NULL, NULL, umbra_shader_color, &color, umbra_blend_src, NULL);

    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    struct test_backends B = test_backends_make(ir);
    umbra_flat_ir_free(ir);
    enum { WFP = 4 };
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __fp16 dst[WFP * 4];
        __builtin_memset(dst, 0, sizeof dst);
        dst_slot = (struct umbra_buf){.ptr=dst, .count=WFP * 4, .stride=WFP};
        if (!test_backends_run(&B, bi, WFP, 1)) { continue; }
        for (int i = 0; i < WFP; i++) {
            equiv((float)dst[i + WFP*0], 0.0f) here;
            equiv((float)dst[i + WFP*1], 0.0f) here;
            equiv((float)dst[i + WFP*2], 1.0f) here;
            equiv((float)dst[i + WFP*3], 1.0f) here;
        }
    }
    test_backends_free(&B);
}

TEST(test_coverage_rect_tail_matrix) {
    struct umbra_buf dst_slot = {0};
    // Property-style sweep over the shape that produced the original bug:
    // dispatch widths that straddle the K=8 SIMD/tail split, heights > 1 so
    // the row loop is exercised, and rect boundaries that land at each
    // meaningful position (before SIMD, mid-SIMD, on the SIMD/tail seam,
    // mid-tail).  For every combination, every backend must match the
    // interpreter pixel for pixel.
    int   const widths[]  = {8, 9, 10, 15, 16, 17};
    int   const heights[] = {2, 3};
    float const rect_ls[] = {0.0f, 4.0f, 8.0f, 9.0f};
    float const rect_rs[] = {8.0f, 10.0f, 16.0f, 17.0f};

    for (int wi = 0; wi < count(widths);  wi++) {
    for (int hi = 0; hi < count(heights); hi++) {
    for (int li = 0; li < count(rect_ls); li++) {
    for (int ri = 0; ri < count(rect_rs); ri++) {
        int   const W = widths[wi], H = heights[hi];
        float const l = rect_ls[li], r = rect_rs[ri];
        if (l >= r) { continue; }

        umbra_color color = {1, 0, 0, 1};
        umbra_rect  rect  = {l, 0.0f, r, (float)H};
        struct umbra_builder *builder = umbra_builder();
        umbra_ptr const dst_ptr = umbra_bind_buf(builder, &dst_slot);
        umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                    dy = umbra_f32_from_i32(builder, umbra_y(builder));
        umbra_build_draw(builder, dst_ptr, umbra_fmt_8888, dx, dy, umbra_coverage_rect, &rect, umbra_shader_color, &color, umbra_blend_srcover, NULL);
        struct test_backends B = make(builder);


        // Interp (bi=0) is always available; use it as the oracle and check
        // every other backend that compiled against its output.
        uint32_t ref[32 * 4] = {0};
        dst_slot = (struct umbra_buf){.ptr=ref, .count=W*H, .stride=W};
        test_backends_run(&B, 0, W, H) here;
        for (int bi = 1; bi < NUM_BACKENDS; bi++) {
            uint32_t dst[32 * 4] = {0};
            dst_slot = (struct umbra_buf){.ptr=dst, .count=W*H, .stride=W};
            if (!test_backends_run(&B, bi, W, H)) {
                continue;
            }
            for (int i = 0; i < W*H; i++) { dst[i] == ref[i] here; }
        }
        test_backends_free(&B);
    }}}}
}

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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_fp16_planar, dx, dy, umbra_coverage_rect, &rect, umbra_shader_color, &color, umbra_blend_srcover, NULL);
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
    umbra_val32 dx = umbra_f32_from_i32(builder, umbra_x(builder)),
                dy = umbra_f32_from_i32(builder, umbra_y(builder));
    umbra_build_draw(builder, dst_ptr, umbra_fmt_fp16_planar, dx, dy, NULL, NULL, umbra_shader_color, &color, umbra_blend_srcover, NULL);
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
    umbra_color color = {1, 0, 0, 1};
    umbra_rect rect = {1.0f, 1.0f, 7.0f, 3.0f};

    struct umbra_backend *bes[NUM_BACKENDS] = {
        umbra_backend_interp(), umbra_backend_jit(),
        umbra_backend_metal(),  umbra_backend_vulkan(),
        umbra_backend_wgpu(),
    };

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        if (!bes[bi]) { continue; }

        struct sdf_shim *qt = sdf_shim_build(bes[bi],
                                             test_rect_fn,        &rect,
                                             umbra_shader_color,  &color,
                                             umbra_blend_srcover, NULL,
                                             umbra_fmt_8888);
        qt != NULL here;
        uint32_t dst[8 * 4];
        __builtin_memset(dst, 0, sizeof dst);
        sdf_shim_queue(qt, 0, 0, 8, 4, (struct umbra_buf){
            .ptr = dst, .count = 8 * 4, .stride = 8,
        });
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

        sdf_shim_free(qt);
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
                         d2 = umbra_interval_add_f32(b,
                                  umbra_interval_mul_f32(b, dx, dx),
                                  umbra_interval_mul_f32(b, dy, dy)),
                         d  = umbra_interval_sqrt_f32(b, d2);
    return umbra_interval_sub_f32(b, d, r);
}

TEST(test_sdf_dispatch_tiling) {
    struct umbra_buf dst_slot = {0};
    // Canvas larger than QUEUE_MIN_TILE (512) so the grid has multiple tiles.
    // Circle at (512, 384) r=180 covers ~4 of ~6 tiles on 1024x768 with 512px tiles.
    enum { W = 1024, H = 768 };

    struct test_circle_state sdf = {.cx = 512, .cy = 384, .r = 180};
    umbra_color color = {1, 0, 0, 1};

    struct umbra_backend *be = umbra_backend_jit();
    if (!be) { be = umbra_backend_interp(); }

    // Tiled dispatch.
    struct sdf_shim *disp = sdf_shim_build(be,
                                           test_circle_fn,      &sdf,
                                           umbra_shader_color,  &color,
                                           umbra_blend_srcover, NULL,
                                           umbra_fmt_8888);
    disp != NULL here;

    // Flat reference: same shader+coverage, no tiling.
    struct umbra_builder *fb = umbra_builder();
    umbra_ptr const fdst = umbra_bind_buf(fb, &dst_slot);
    umbra_val32 const fx = umbra_f32_from_i32(fb, umbra_x(fb)),
                      fy = umbra_f32_from_i32(fb, umbra_y(fb));
    umbra_build_sdf_draw(fb, fdst, umbra_fmt_8888, fx, fy,
                         test_circle_fn,      &sdf, 0,
                         umbra_shader_color,  &color,
                         umbra_blend_srcover, NULL);
    struct umbra_flat_ir *fir = umbra_flat_ir(fb);
    umbra_builder_free(fb);
    struct umbra_program *flat = be->compile(be, fir);
    umbra_flat_ir_free(fir);

    uint32_t *tiled_buf = calloc(W * H, sizeof(uint32_t));
    uint32_t *flat_buf  = calloc(W * H, sizeof(uint32_t));

    sdf_shim_queue(disp, 0, 0, W, H, (struct umbra_buf){
        .ptr = tiled_buf, .count = W * H, .stride = W,
    });

    dst_slot = (struct umbra_buf){.ptr = flat_buf, .count = W * H, .stride = W};
    flat->queue(flat, 0, 0, W, H);

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
    sdf_shim_free(disp);
    umbra_backend_free(be);
}

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

    umbra_var32 sum = umbra_declare_var32(b);
    umbra_store_var32(b, sum, umbra_imm_f32(b, 0.0f));

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
        if (!bes[bi]) { continue; }
        struct umbra_program *prog = bes[bi]->compile(bes[bi], ir);
        out = 0;
        prog->queue(prog, 0, 0, 1, 1);
        bes[bi]->flush(bes[bi]);
        equiv(out, 60.0f) here;
        umbra_program_free(prog);
    }

    umbra_flat_ir_free(ir);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        umbra_backend_free(bes[bi]);
    }
}

