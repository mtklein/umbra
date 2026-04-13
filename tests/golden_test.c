#include "test.h"

#ifndef __wasm__

#include "../slides/slide.h"
#include "../slides/text.h"
#include "../slides/slug.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum { W = 128, H = 96 };

static char const *backend_name[NUM_BACKENDS] = {
    "interp", "jit", "metal", "vulkan", "wgpu",
};

static struct umbra_backend *bes[NUM_BACKENDS];

static struct umbra_fmt const *all_fmts[] = {
    &umbra_fmt_8888,
    &umbra_fmt_565,
    &umbra_fmt_1010102,
    &umbra_fmt_fp16,
    &umbra_fmt_fp16_planar,
};
enum { N_FMTS = (int)(sizeof all_fmts / sizeof *all_fmts) };

static void build_pipes(void) {
    bes[0] = umbra_backend_interp();
    bes[1] = umbra_backend_jit();
    bes[2] = umbra_backend_metal();
    bes[3] = umbra_backend_vulkan();
    bes[4] = umbra_backend_wgpu();
}

static void free_pipes(void) {
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        if (bes[bi]) { bes[bi]->free(bes[bi]); }
    }
}

static void render_slide(int slide_idx, struct umbra_backend *be,
                         struct umbra_fmt fmt, void *pixbuf) {
    struct slide *s = slide_get(slide_idx);
    s->prepare(s, be, fmt);
    s->draw(s, 0, 0, 0, W, H, pixbuf);
}

static void test_slide_golden(int slide_idx, struct umbra_fmt fmt) {
    struct slide *s = slide_get(slide_idx);

    size_t const rb = (size_t)W * fmt.bpp;
    size_t const pixbuf_sz = rb * H * (size_t)fmt.planes;

    void *pbuf[NUM_BACKENDS] = {0};
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        if (!bes[bi]) { continue; }
        pbuf[bi] = calloc(1, pixbuf_sz);
        render_slide(slide_idx, bes[bi], fmt, pbuf[bi]);
        bes[bi]->flush(bes[bi]);
    }

    _Bool ok = 1;
    for (int i = 0; i < NUM_BACKENDS; i++) {
        if (!pbuf[i]) { continue; }
        for (int j = i + 1; j < NUM_BACKENDS; j++) {
            if (!pbuf[j]) { continue; }

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

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        free(pbuf[bi]);
    }
    ok here;
}

TEST(test_slug_rect) {
    static float rect[] = {
         5, 5,  5,20,  5,35,
         5,35, 30,35, 55,35,
        55,35, 55,20, 55, 5,
        55, 5, 30, 5,  5, 5,
    };

    struct slug_acc_layout alay;
    struct umbra_builder *ab = slug_build_acc(&alay);
    struct umbra_flat_ir *abb =
        umbra_flat_ir(ab);
    umbra_builder_free(ab);
    struct umbra_backend *be =
        umbra_backend_interp();
    struct umbra_program *acc =
        be->compile(be, abb);
    umbra_flat_ir_free(abb);

    float color[4] = {1,1,1,1};
    float wind_buf[W * H];
    __builtin_memset(wind_buf, 0, sizeof wind_buf);

    struct umbra_shader_solid shader = umbra_shader_solid(color);
    struct umbra_coverage_wind cov = umbra_coverage_wind(
        (struct umbra_buf){.ptr=wind_buf, .count=sizeof wind_buf / 4,
                           .stride=W});

    struct umbra_draw_layout lay;
    struct umbra_builder *bld = umbra_draw_build(
        &shader.base, &cov.base,
        umbra_blend_srcover, umbra_fmt_8888,
        &lay);
    struct umbra_flat_ir *bb =
        umbra_flat_ir(bld);
    umbra_builder_free(bld);
    struct umbra_program *interp =
        be->compile(be, bb);
    umbra_flat_ir_free(bb);

    uint32_t pixels[W * H];
    for (int i = 0; i < W * H; i++) {
        pixels[i] = 0xff000000;
    }

    struct umbra_matrix mat = {1,0,0, 0,1,0, 0,0,1};
    umbra_uniforms_fill_f32(alay.uniforms, alay.mat, &mat.sx, 9);
    float const slug_wh[2] = {60, 40};
    umbra_uniforms_fill_f32(alay.uniforms, alay.wh, slug_wh, 2);
    umbra_uniforms_fill_ptr(alay.uniforms, alay.curves_off,
        (struct umbra_buf){.ptr=rect, .count=sizeof rect / 4});
    struct umbra_buf abuf[] = {
        (struct umbra_buf){.ptr=alay.uniforms, .count=alay.uni.slots},
        {.ptr=wind_buf, .count=sizeof wind_buf / 4, .stride=W},
    };
    for (int j = 0; j < 4; j++) {
        umbra_uniforms_fill_i32(alay.uniforms, alay.loop_off, &j, 1);
        acc->queue(acc, 0, 0, W, H, abuf);
    }
    be->flush(be);

    umbra_draw_fill(&lay, &shader.base, &cov.base);
    struct umbra_buf buf[] = {
        (struct umbra_buf){.ptr=lay.uniforms, .count=lay.uni.slots},
        {.ptr=pixels, .count=W * H, .stride=W},
    };
    interp->queue(interp, 0, 0, W, H, buf);
    be->flush(be);

    uint32_t bg = 0xff000000;
    uint32_t fg = 0xffffffff;
    pixels[20*W + 30] == fg here;
    pixels[20*W +  2] == bg here;
    pixels[20*W + 58] == bg here;
    pixels[ 2*W + 30] == bg here;
    pixels[38*W + 30] == bg here;
    pixels[20*W + 70] == bg here;

    free(lay.uniforms);
    free(alay.uniforms);
    acc->free(acc);
    interp->free(interp);
    be->free(be);
}

TEST(test_perspective_text) {
    enum { BW = 16, BH = 8 };
    uint16_t bmp[BW * BH];
    __builtin_memset(bmp, 0, sizeof bmp);
    bmp[0 * BW + 8] = 255;

    struct umbra_backend *be =
        umbra_backend_interp();

    float color[4] = {1,1,1,1};

    struct umbra_shader_solid shader = umbra_shader_solid(color);
    struct umbra_coverage_bitmap_matrix cov = umbra_coverage_bitmap_matrix(
        (struct umbra_matrix){1,0,0, 0,1,0, 0,0,1},
        (struct umbra_bitmap){.buf={.ptr=bmp, .count=BW * BH}, .w=BW, .h=BH});

    struct umbra_draw_layout lay;
    struct umbra_builder *bld = umbra_draw_build(
        &shader.base, &cov.base,
        umbra_blend_srcover, umbra_fmt_8888,
        &lay);
    struct umbra_flat_ir *bb =
        umbra_flat_ir(bld);
    umbra_builder_free(bld);
    struct umbra_program *interp =
        be->compile(be, bb);
    umbra_flat_ir_free(bb);

    uint32_t pixels[BW];
    for (int i = 0; i < BW; i++) {
        pixels[i] = 0xff000000;
    }

    umbra_draw_fill(&lay, &shader.base, &cov.base);
    struct umbra_buf buf[] = {
        (struct umbra_buf){.ptr=lay.uniforms, .count=lay.uni.slots},
        {.ptr=pixels, .count=BW},
    };
    interp->queue(interp, 0, 0, BW, 1, buf);
    be->flush(be);

    pixels[8] == 0xffffffff here;
    pixels[0] == 0xff000000 here;

    interp->free(interp);

    struct text_cov tc = text_rasterize(W, H, 24.0f, 0);

    struct umbra_matrix mat2;
    slide_perspective_matrix(&mat2, 1.0f, W, H, tc.w, tc.h);
    float hc2[4] = {1,0.8f,0.2f,1};

    struct umbra_shader_solid shader2 = umbra_shader_solid(hc2);
    struct umbra_coverage_bitmap_matrix cov2 = umbra_coverage_bitmap_matrix(mat2,
        (struct umbra_bitmap){
            .buf = {.ptr=tc.data, .count=tc.w * tc.h},
            .w   = tc.w,
            .h   = tc.h,
        });

    struct umbra_draw_layout lay2;
    bld = umbra_draw_build(
        &shader2.base, &cov2.base,
        umbra_blend_srcover, umbra_fmt_8888,
        &lay2);
    bb = umbra_flat_ir(bld);
    umbra_builder_free(bld);
    interp = be->compile(be, bb);
    umbra_flat_ir_free(bb);

    uint32_t px2[W * H];
    for (int i = 0; i < W * H; i++) {
        px2[i] = 0xff0a0a1e;
    }
    {
        umbra_draw_fill(&lay2, &shader2.base, &cov2.base);
        struct umbra_buf b2[] = {
            (struct umbra_buf){.ptr=lay2.uniforms, .count=lay2.uni.slots},
            {.ptr=px2, .count=W * H, .stride=W},
        };
        interp->queue(interp, 0, 0, W, H, b2);
        be->flush(be);
    }
    int changed = 0;
    for (int i = 0; i < W * H; i++) {
        if (px2[i] != 0xff0a0a1e) { changed++; }
    }
    changed > 0 here;

    free(lay.uniforms);
    free(lay2.uniforms);
    interp->free(interp);
    be->free(be);
    text_cov_free(&tc);
}

TEST(test_golden_slides) {
    build_pipes();
    slides_init(W, H);
    for (int fi = 0; fi < N_FMTS; fi++) {
        for (int si = 0; si < slide_count() - 1; si++) {
            test_slide_golden(si, *all_fmts[fi]);
        }
    }
    slides_cleanup();
    free_pipes();
}

static void run_long_batch_no_oom(struct umbra_backend *be) {
    if (be) {
        struct umbra_builder *bld = umbra_builder();
        umbra_color c = {
            umbra_uniform_32(bld, (umbra_ptr32){0}, 0),
            umbra_uniform_32(bld, (umbra_ptr32){0}, 1),
            umbra_uniform_32(bld, (umbra_ptr32){0}, 2),
            umbra_uniform_32(bld, (umbra_ptr32){0}, 3),
        };
        umbra_store_8888(bld, (umbra_ptr32){.ix=1}, c);
        struct umbra_flat_ir *bb = umbra_flat_ir(bld);
        umbra_builder_free(bld);

        struct umbra_program *p = be->compile(be, bb);
        umbra_flat_ir_free(bb);
        p != 0 here;

        int rotations_before = be->stats(be).uniform_ring_rotations;

        float    color[4] = {0, 0, 0, 1};
        uint32_t pixel    = 0;
        struct umbra_buf bufs[] = {
            {.ptr=color, .count=sizeof color / 4},
            {.ptr=&pixel, .count=1, .stride=1},
        };
        int const N = 12000;
        for (int i = 0; i < N; i++) {
            color[0] = (float)((i & 0xff) / 255.0f);
            p->queue(p, 0, 0, 1, 1, bufs);
        }
        be->flush(be);

        uint32_t expected_r = (uint32_t)((N - 1) & 0xff);
        (pixel & 0xff)   == expected_r here;
        (pixel >> 24)    == 0xff here;

        struct umbra_backend_stats st = be->stats(be);
        st.uniform_ring_rotations > rotations_before here;
        st.gpu_sec > 0.0 here;

        p->free(p);
        be->free(be);
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
    if (!be) { return; }

    struct umbra_builder *b = umbra_builder();
    umbra_val32 v   = umbra_load_32(b, (umbra_ptr32){.ix=0});
    umbra_val32 one = umbra_imm_f32(b, 1.0f);
    umbra_store_32(b, (umbra_ptr32){.ix=0}, umbra_add_f32(b, v, one));
    struct umbra_flat_ir *bb = umbra_flat_ir(b);
    umbra_builder_free(b);

    struct umbra_program *p = be->compile(be, bb);
    umbra_flat_ir_free(bb);
    p != 0 here;

    enum { BW = 128, BH = 128 };
    size_t buf_sz  = BW * BH * sizeof(float),
           half_sz = buf_sz / 2;
    float *data = NULL;
    posix_memalign((void **)&data, (size_t)getpagesize(), buf_sz);
    data != NULL here;

    struct umbra_buf bufs[] = {
        {.ptr=data, .count=BW * BH, .stride=BW},
    };

    for (int frame = 0; frame < 3; frame++) {
        float sentinel = (float)(frame + 2) * 10.0f;
        for (int i = 0; i < BW * BH; i++) { data[i] = sentinel; }

        __builtin_memset(data, 0, half_sz);
        p->queue(p, 0, 0, BW, BH / 2, bufs);
        be->flush(be);

        __builtin_memset((char *)data + half_sz, 0, half_sz);
        p->queue(p, 0, BH / 2, BW, BH, bufs);
        be->flush(be);

        for (int i = 0; i < BW * BH; i++) {
            data[i] == 1.0f here;
        }
    }

    free(data);
    p->free(p);
    be->free(be);
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
    if (!be) { return; }

    struct umbra_builder *bld = umbra_builder();
    umbra_color c = {
        umbra_uniform_32(bld, (umbra_ptr32){0}, 0),
        umbra_imm_f32(bld, 0.0f),
        umbra_imm_f32(bld, 0.0f),
        umbra_imm_f32(bld, 1.0f),
    };
    umbra_store_8888(bld, (umbra_ptr32){.ix=1}, c);
    struct umbra_flat_ir *bb = umbra_flat_ir(bld);
    umbra_builder_free(bld);

    struct umbra_program *p = be->compile(be, bb);
    umbra_flat_ir_free(bb);
    p != 0 here;

    FILE *devnull = fopen("/dev/null", "w");
    p->dump(p, devnull);
    fclose(devnull);

    float uniform_data[2] = {1.0f, 0.0f};
    uint32_t pixel = 0;
    struct umbra_buf bufs[] = {
        {.ptr=uniform_data, .count=7 / 4},
        {.ptr=&pixel, .count=1, .stride=1},
    };
    p->queue(p, 0, 0, 1, 1, bufs);
    be->flush(be);

    (pixel & 0xff)   == 0xff here;
    (pixel >> 24)    == 0xff here;

    p->free(p);
    be->free(be);
}

#endif /* !__wasm__ */
