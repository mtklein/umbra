#include "test.h"

#ifndef __wasm__

#include "../slides/slide.h"
#include "../slides/coverage.h"
#include "../slides/text.h"
#include "../src/count.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum { W = 128, H = 96 };

static char const *const backend_name[NUM_BACKENDS] = {
    "interp", "jit", "metal", "vulkan", "wgpu",
};

static struct umbra_backend *bes[NUM_BACKENDS];

static void build_pipes(void) {
    bes[0] = umbra_backend_interp();
    bes[1] = umbra_backend_jit();
    bes[2] = umbra_backend_metal();
    bes[3] = umbra_backend_vulkan();
    bes[4] = umbra_backend_wgpu();
}

static void free_pipes(void) {
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        umbra_backend_free(bes[bi]);
    }
}

static void test_slide_golden(int slide_idx, struct umbra_fmt fmt) {
    struct slide *s = slide_get(slide_idx);

    size_t const rb = (size_t)W * fmt.bpp;
    size_t const pixbuf_sz = rb * H * (size_t)fmt.planes;

    struct slide_runtime *rt[NUM_BACKENDS] = {0};
    struct slide_bg      *bg[NUM_BACKENDS] = {0};
    void *pbuf[NUM_BACKENDS] = {0};
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        if (bes[bi]) {
            pbuf[bi] = calloc(1, pixbuf_sz);

            rt[bi] = slide_runtime(s, W, H, bes[bi], fmt, NULL);
            bg[bi] = slide_bg(bes[bi], fmt);

            struct umbra_buf const dst = {
                .ptr=pbuf[bi], .count=W * H * fmt.planes, .stride=W,
            };
            slide_bg_draw(bg[bi], s->bg, 0, 0, W, H, dst);
            slide_runtime_animate(s, 0);
            slide_runtime_draw(rt[bi], dst, 0, 0, W, H);

            bes[bi]->flush(bes[bi]);
        }
    }

    _Bool ok = 1;
    for (int i = 0; i < NUM_BACKENDS; i++) {
        if (pbuf[i]) {
            for (int j = i + 1; j < NUM_BACKENDS; j++) {
                if (pbuf[j]) {
                    int mismatches = 0;
                    int worst = 0;
                    int worst_off = -1;
                    uint8_t const *a = pbuf[i], *b = pbuf[j];
                    for (size_t k = 0; k < pixbuf_sz; k++) {
                        if (a[k] != b[k]) {
                            mismatches++;
                            int d = (int)a[k] - (int)b[k];
                            if (d < 0) { d = -d; }
                            if (d > worst) {
                                worst = d;
                                worst_off = (int)k;
                            }
                        }
                    }
                    if (worst > 0) {
                        dprintf(2,
                            "slide %d \"%s\" %s vs %s fmt=%s: "
                            "%d/%d bytes differ, worst delta=%d at byte %d\n",
                            slide_idx + 1, s->title,
                            backend_name[i], backend_name[j], fmt.name,
                            mismatches, (int)pixbuf_sz,
                            worst, worst_off);
                        ok = 0;
                    }
                }
            }
        }
    }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        slide_bg_free(bg[bi]);
        slide_runtime_free(rt[bi]);
        free(pbuf[bi]);
    }
    ok here;
}

TEST(test_perspective_text) {
    enum { BW = 16, BH = 8 };
    uint16_t bmp[BW * BH];
    __builtin_memset(bmp, 0, sizeof bmp);
    bmp[0 * BW + 8] = 255;

    struct umbra_backend *be =
        umbra_backend_interp();

    umbra_color color = {1,1,1,1};

    struct coverage_bitmap2d sampler = {
        .buf = {.ptr=bmp, .count=BW * BH}, .w=BW, .h=BH,
    };
    struct umbra_matrix mat = {1,0,0, 0,1,0, 0,0,1};

    struct umbra_buf dst_slot = {0};
    struct umbra_builder *b = umbra_builder();
    umbra_ptr const dst_ptr = umbra_early_bind_buf(b, &dst_slot);
    umbra_val32 const x = umbra_f32_from_i32(b, umbra_x(b)),
                      y = umbra_f32_from_i32(b, umbra_y(b));
    umbra_point_val32 const p = umbra_transform_perspective(&mat, b, x, y);
    umbra_build_draw(b, dst_ptr, umbra_fmt_8888, p.x, p.y,
                     coverage_bitmap2d,   &sampler,
                     umbra_shader_color,  &color,
                     umbra_blend_srcover, NULL);
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    struct umbra_program *interp = be->compile(be, ir);
    umbra_flat_ir_free(ir);

    uint32_t pixels[BW];
    for (int i = 0; i < BW; i++) {
        pixels[i] = 0xff000000;
    }

    dst_slot = (struct umbra_buf){.ptr=pixels, .count=BW};
    interp->queue(interp, 0, 0, BW, 1, 0, NULL);
    be->flush(be);

    pixels[8] == 0xffffffff here;
    pixels[0] == 0xff000000 here;

    umbra_program_free(interp);

    struct text_cov tc = text_rasterize(W, H, 24.0f, 0);

    struct umbra_matrix mat2;
    slide_perspective_matrix(&mat2, 1.0f, W, H, tc.w, tc.h);
    umbra_color hc2 = {1,0.8f,0.2f,1};

    struct coverage_bitmap2d sampler2 = {
        .buf = {.ptr=tc.data, .count=tc.w * tc.h},
        .w   = tc.w,
        .h   = tc.h,
    };

    b = umbra_builder();
    umbra_ptr const dst_ptr2 = umbra_early_bind_buf(b, &dst_slot);
    umbra_val32 const x2 = umbra_f32_from_i32(b, umbra_x(b)),
                      y2 = umbra_f32_from_i32(b, umbra_y(b));
    umbra_point_val32 const p2 = umbra_transform_perspective(&mat2, b, x2, y2);
    umbra_build_draw(b, dst_ptr2, umbra_fmt_8888, p2.x, p2.y,
                     coverage_bitmap2d,   &sampler2,
                     umbra_shader_color,  &hc2,
                     umbra_blend_srcover, NULL);
    ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    interp = be->compile(be, ir);
    umbra_flat_ir_free(ir);

    uint32_t px2[W * H];
    for (int i = 0; i < W * H; i++) {
        px2[i] = 0xff0a0a1e;
    }
    {
        dst_slot = (struct umbra_buf){.ptr=px2, .count=W * H, .stride=W};
        interp->queue(interp, 0, 0, W, H, 0, NULL);
        be->flush(be);
    }
    int changed = 0;
    for (int i = 0; i < W * H; i++) {
        if (px2[i] != 0xff0a0a1e) { changed++; }
    }
    changed > 0 here;

    umbra_program_free(interp);
    umbra_backend_free(be);
    text_cov_free(&tc);
}

static void golden_slides_for_fmt(struct umbra_fmt fmt) {
    build_pipes();
    slides_init(W, H);
    for (int si = 0; si < slide_count() - 1; si++) {
        test_slide_golden(si, fmt);
    }
    slides_cleanup();
    free_pipes();
}

TEST(test_golden_slides_8888)        { golden_slides_for_fmt(umbra_fmt_8888); }
TEST(test_golden_slides_565)         { golden_slides_for_fmt(umbra_fmt_565); }
TEST(test_golden_slides_1010102)     { golden_slides_for_fmt(umbra_fmt_1010102); }
TEST(test_golden_slides_fp16)        { golden_slides_for_fmt(umbra_fmt_fp16); }
TEST(test_golden_slides_fp16_planar) { golden_slides_for_fmt(umbra_fmt_fp16_planar); }

static void run_long_batch_no_oom(struct umbra_backend *be) {
    if (be) {
        umbra_color color = {0, 0, 0, 1};
        uint32_t    pixel = 0;
        struct umbra_buf pixel_buf = {.ptr=&pixel, .count=1, .stride=1};

        struct umbra_builder *b = umbra_builder();
        umbra_ptr const cu = umbra_early_bind_uniforms(b, &color, (int)(sizeof color / 4)),
                        pp = umbra_early_bind_buf(b, &pixel_buf);
        umbra_color_val32 c = {
            umbra_uniform_32(b, cu, 0),
            umbra_uniform_32(b, cu, 1),
            umbra_uniform_32(b, cu, 2),
            umbra_uniform_32(b, cu, 3),
        };
        umbra_store_8888(b, pp, c);
        struct umbra_flat_ir *ir = umbra_flat_ir(b);
        umbra_builder_free(b);

        struct umbra_program *p = be->compile(be, ir);
        umbra_flat_ir_free(ir);
        p != 0 here;

        int rotations_before = be->stats(be).uniform_ring_rotations;

        int const N = 12000;
        for (int i = 0; i < N; i++) {
            color.r = (float)((i & 0xff) / 255.0f);
            p->queue(p, 0, 0, 1, 1, 0, NULL);
        }
        be->flush(be);

        uint32_t expected_r = (uint32_t)((N - 1) & 0xff);
        (pixel & 0xff)   == expected_r here;
        (pixel >> 24)    == 0xff here;

        struct umbra_backend_stats st = be->stats(be);
        st.uniform_ring_rotations > rotations_before here;
        st.gpu_sec > 0.0 here;

        umbra_program_free(p);
        umbra_backend_free(be);
    }
}

TEST(test_metal_long_batch_no_oom) {
    run_long_batch_no_oom(umbra_backend_metal());
}

TEST(test_vulkan_long_batch_no_oom) {
    run_long_batch_no_oom(umbra_backend_vulkan());
}

TEST(test_wgpu_long_batch_no_oom) {
    run_long_batch_no_oom(umbra_backend_wgpu());
}

static void run_tiled_writable_sync(struct umbra_backend *be) {
    if (be) {
        enum { BW = 128, BH = 128 };
        size_t buf_sz  = BW * BH * sizeof(float),
               half_sz = buf_sz / 2;
        float *data = NULL;
        posix_memalign((void **)&data, (size_t)getpagesize(), buf_sz);
        data != NULL here;
        struct umbra_buf data_buf = {.ptr=data, .count=BW * BH, .stride=BW};

        struct umbra_builder *b = umbra_builder();
        umbra_ptr const dp = umbra_early_bind_buf(b, &data_buf);
        umbra_val32 v   = umbra_load_32(b, dp);
        umbra_val32 one = umbra_imm_f32(b, 1.0f);
        umbra_store_32(b, dp, umbra_add_f32(b, v, one));
        struct umbra_flat_ir *ir = umbra_flat_ir(b);
        umbra_builder_free(b);

        struct umbra_program *p = be->compile(be, ir);
        umbra_flat_ir_free(ir);
        p != 0 here;

        for (int frame = 0; frame < 3; frame++) {
            float sentinel = (float)(frame + 2) * 10.0f;
            for (int i = 0; i < BW * BH; i++) { data[i] = sentinel; }

            __builtin_memset(data, 0, half_sz);
            p->queue(p, 0, 0, BW, BH / 2, 0, NULL);
            be->flush(be);

            __builtin_memset((char *)data + half_sz, 0, half_sz);
            p->queue(p, 0, BH / 2, BW, BH, 0, NULL);
            be->flush(be);

            for (int i = 0; i < BW * BH; i++) {
                data[i] == 1.0f here;
            }
        }

        free(data);
        umbra_program_free(p);
        umbra_backend_free(be);
    }
}

TEST(test_metal_tiled_writable_sync) {
    run_tiled_writable_sync(umbra_backend_metal());
}

TEST(test_vulkan_tiled_writable_sync) {
    run_tiled_writable_sync(umbra_backend_vulkan());
}

TEST(test_wgpu_tiled_writable_sync) {
    run_tiled_writable_sync(umbra_backend_wgpu());
}

TEST(test_wgpu_misc) {
    struct umbra_backend *be = umbra_backend_wgpu();
    if (be) {
        float    uniform_data[2] = {1.0f, 0.0f};
        uint32_t pixel           = 0;
        struct umbra_buf pixel_buf = {.ptr=&pixel, .count=1, .stride=1};

        struct umbra_builder *b = umbra_builder();
        umbra_ptr const cu = umbra_early_bind_uniforms(b, uniform_data, count(uniform_data)),
                        pp = umbra_early_bind_buf(b, &pixel_buf);
        umbra_color_val32 c = {
            umbra_uniform_32(b, cu, 0),
            umbra_imm_f32(b, 0.0f),
            umbra_imm_f32(b, 0.0f),
            umbra_imm_f32(b, 1.0f),
        };
        umbra_store_8888(b, pp, c);
        struct umbra_flat_ir *ir = umbra_flat_ir(b);
        umbra_builder_free(b);

        struct umbra_program *p = be->compile(be, ir);
        umbra_flat_ir_free(ir);
        p != 0 here;

        FILE *devnull = fopen("/dev/null", "w");
        p->dump(p, devnull);
        fclose(devnull);

        p->queue(p, 0, 0, 1, 1, 0, NULL);
        be->flush(be);

        (pixel & 0xff)   == 0xff here;
        (pixel >> 24)    == 0xff here;

        umbra_program_free(p);
        umbra_backend_free(be);
    }
}

#endif /* !__wasm__ */
