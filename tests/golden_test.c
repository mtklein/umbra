#include "test.h"

#ifndef __wasm__

#include "../slides/slide.h"
#include "../slides/text.h"
#include "../slides/slug.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>



enum { W = 128, H = 96 };

static char const *backend_name[NUM_BACKENDS] = {
    "interp", "jit", "metal", "vulkan",
};

static struct umbra_backend *bes[NUM_BACKENDS];

struct pipe {
    struct umbra_program         *prog;
    struct umbra_uniforms_layout  uni;
    void                         *uniforms;
};

static struct pipe fill_pipe;

static void build_fill(void) {
    struct umbra_builder   *builder = umbra_builder();
    struct umbra_uniforms_layout u = {0};
    size_t fi = umbra_uniforms_reserve_f32(&u, 4);
    umbra_color c = {
        umbra_uniform_32(builder, (umbra_ptr32){0}, fi),
        umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 4),
        umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 8),
        umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 12),
    };
    umbra_store_8888(builder, (umbra_ptr32){.ix=1}, c);
    fill_pipe.uni = u;
    fill_pipe.uniforms = umbra_uniforms_alloc(&u);
    struct umbra_basic_block *opt =
        umbra_basic_block(builder);
    umbra_builder_free(builder);
    fill_pipe.prog =
        bes[0]->compile(bes[0], opt);
    umbra_basic_block_free(opt);
}

static void build_pipes(void) {
    bes[0] = umbra_backend_interp();
    bes[1] = umbra_backend_jit();
    bes[2] = umbra_backend_metal();
    bes[3] = umbra_backend_vulkan();
    build_fill();
}

static void free_pipes(void) {
    fill_pipe.prog->free(fill_pipe.prog);
    free(fill_pipe.uniforms);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        if (bes[bi]) { bes[bi]->free(bes[bi]); }
    }
}

static void fill_bg(void *dst, uint32_t bg) {
    float hc[4] = {
        (float)( bg        & 0xffu) / 255.0f,
        (float)((bg >>  8) & 0xffu) / 255.0f,
        (float)((bg >> 16) & 0xffu) / 255.0f,
        (float)((bg >> 24) & 0xffu) / 255.0f,
    };
    umbra_uniforms_fill_f32(fill_pipe.uniforms, 0, hc, 4);
    struct umbra_buf buf[2] = {
        (struct umbra_buf){.ptr=fill_pipe.uniforms, .sz=fill_pipe.uni.size, .read_only=1},
        {.ptr=dst, .sz=(size_t)(W * H * 4), .row_bytes=(size_t)W * 4},
    };
    fill_pipe.prog->queue(fill_pipe.prog, 0, 0, W, H, buf);
}


static void render_slide(
        int slide_idx,
        struct umbra_backend *be,
        void *pixbuf) {
    struct slide *s = slide_get(slide_idx);

    fill_bg(pixbuf, s->bg);
    s->prepare(s, be, umbra_fmt_8888);
    s->draw(s, 0, 0, 0, W, H, pixbuf);
}

static void test_slide_golden(int slide_idx) {
    struct slide *s = slide_get(slide_idx);

    size_t pixbuf_sz = (size_t)(W * H * 4);
    void *pbuf_ref = malloc(pixbuf_sz);
    void *pbuf_tst = malloc(pixbuf_sz);

    render_slide(slide_idx, bes[0], pbuf_ref);
    bes[0]->flush(bes[0]);

    for (int bi = 1; bi < NUM_BACKENDS; bi++) {
        if (!bes[bi]) { continue; }
        __builtin_memset(pbuf_tst, 0, pixbuf_sz);
        render_slide(slide_idx, bes[bi], pbuf_tst);
        bes[bi]->flush(bes[bi]);

        int mismatches = 0;
        int worst = 0;
        int worst_px = -1;
        uint8_t const *r = pbuf_ref, *t = pbuf_tst;
        for (int i = 0; i < W * H; i++) {
            uint32_t rp, tp;
            __builtin_memcpy(&rp, r+i*4, 4);
            __builtin_memcpy(&tp, t+i*4, 4);
            if (rp != tp) {
                mismatches++;
                for (int ch = 0; ch < 4; ch++) {
                    int d = (int)((rp>>(ch*8))&0xFF) - (int)((tp>>(ch*8))&0xFF);
                    if (d<0) d=-d;
                    if (d>worst) { worst=d; worst_px=i; }
                }
            }
        }
        int tol = 0;
        if (worst > tol) {
            uint32_t rp, tp;
            __builtin_memcpy(&rp, r+worst_px*4, 4);
            __builtin_memcpy(&tp, t+worst_px*4, 4);
            dprintf(2,
                "slide %d \"%s\" %s: "
                "%d/%d pixels differ, "
                "worst delta=%d at (%d,%d) "
                "ref=%08x tst=%08x\n",
                slide_idx + 1, s->title,
                backend_name[bi],
                mismatches, W * H,
                worst, worst_px % W, worst_px / W,
                rp, tp);
        }
        (worst <= tol) here;
    }

    free(pbuf_ref);
    free(pbuf_tst);
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
    struct umbra_basic_block *abb =
        umbra_basic_block(ab);
    umbra_builder_free(ab);
    struct umbra_backend *be =
        umbra_backend_interp();
    struct umbra_program *acc =
        be->compile(be, abb);
    umbra_basic_block_free(abb);

    struct umbra_draw_layout lay;
    struct umbra_builder *bld = umbra_draw_build(
        umbra_shader_solid, umbra_coverage_wind,
        umbra_blend_srcover, umbra_fmt_8888,
        &lay);
    struct umbra_basic_block *bb =
        umbra_basic_block(bld);
    umbra_builder_free(bld);
    struct umbra_program *interp =
        be->compile(be, bb);
    umbra_basic_block_free(bb);

    uint32_t pixels[W * H];
    for (int i = 0; i < W * H; i++) {
        pixels[i] = 0xff000000;
    }

    float mat[11] = {
        1,0,0, 0,1,0, 0,0,1, 60,40,
    };
    float color[4] = {1,1,1,1};

    float wind_buf[W * H];
    __builtin_memset(wind_buf, 0, sizeof wind_buf);
    umbra_uniforms_fill_f32(alay.uniforms, alay.mat, mat, 11);
    umbra_uniforms_fill_ptr(alay.uniforms, alay.curves_off,
        (struct umbra_buf){.ptr=rect, .sz=sizeof rect});
    struct umbra_buf abuf[] = {
        (struct umbra_buf){.ptr=alay.uniforms, .sz=alay.uni.size, .read_only=1},
        {.ptr=wind_buf, .sz=sizeof wind_buf, .row_bytes=W * sizeof(float)},
    };
    for (int j = 0; j < 4; j++) {
        float jf;
        int32_t j32 = j;
        __builtin_memcpy(&jf, &j32, 4);
        umbra_uniforms_fill_f32(alay.uniforms, alay.loop_off, &jf, 1);
        acc->queue(acc, 0, 0, W, H, abuf);
    }
    be->flush(be);

    umbra_uniforms_fill_f32(lay.uniforms, lay.shader, color, 4);
    umbra_uniforms_fill_ptr(lay.uniforms, lay.coverage,
        (struct umbra_buf){.ptr=wind_buf, .sz=sizeof wind_buf, .read_only=1, .row_bytes=(size_t)W * sizeof(float)});
    struct umbra_buf buf[] = {
        (struct umbra_buf){.ptr=lay.uniforms, .sz=lay.uni.size, .read_only=1},
        {.ptr=pixels, .sz=sizeof pixels, .row_bytes=W * 4},
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

    struct umbra_draw_layout lay;
    struct umbra_builder *bld = umbra_draw_build(
        umbra_shader_solid,
        umbra_coverage_bitmap_matrix,
        umbra_blend_srcover, umbra_fmt_8888,
        &lay);
    struct umbra_basic_block *bb =
        umbra_basic_block(bld);
    umbra_builder_free(bld);
    struct umbra_program *interp =
        be->compile(be, bb);
    umbra_basic_block_free(bb);

    uint32_t pixels[BW];
    for (int i = 0; i < BW; i++) {
        pixels[i] = 0xff000000;
    }

    float mat[11] = {
        1,0,0, 0,1,0, 0,0,1,
        (float)BW, (float)BH,
    };
    float color[4] = {1,1,1,1};

    umbra_uniforms_fill_f32(lay.uniforms, lay.shader, color, 4);
    umbra_uniforms_fill_f32(lay.uniforms, lay.coverage, mat, 11);
    umbra_uniforms_fill_ptr(lay.uniforms, (lay.coverage + 44 + 7) & ~(size_t)7,
        (struct umbra_buf){.ptr=bmp, .sz=sizeof bmp});
    struct umbra_buf buf[] = {
        (struct umbra_buf){.ptr=lay.uniforms, .sz=lay.uni.size, .read_only=1},
        {.ptr=pixels, .sz=sizeof pixels},
    };
    interp->queue(interp, 0, 0, BW, 1, buf);
    be->flush(be);

    pixels[8] == 0xffffffff here;
    pixels[0] == 0xff000000 here;

    interp->free(interp);

    struct text_cov tc = text_rasterize(W, H, 24.0f, 0);

    struct umbra_draw_layout lay2;
    bld = umbra_draw_build(
        umbra_shader_solid,
        umbra_coverage_bitmap_matrix,
        umbra_blend_srcover, umbra_fmt_8888,
        &lay2);
    bb = umbra_basic_block(bld);
    umbra_builder_free(bld);
    interp = be->compile(be, bb);
    umbra_basic_block_free(bb);

    uint32_t px2[W * H];
    for (int i = 0; i < W * H; i++) {
        px2[i] = 0xff0a0a1e;
    }
    float mat2[11];
    slide_perspective_matrix(mat2, 1.0f,
        W, H, tc.w, tc.h);
    float hc2[4] = {1,0.8f,0.2f,1};
    {
        umbra_uniforms_fill_f32(lay2.uniforms, lay2.shader, hc2, 4);
        umbra_uniforms_fill_f32(lay2.uniforms, lay2.coverage, mat2, 11);
        umbra_uniforms_fill_ptr(lay2.uniforms, (lay2.coverage + 44 + 7) & ~(size_t)7,
            (struct umbra_buf){.ptr=tc.data, .sz=(size_t)(W * H * 2)});
        struct umbra_buf b2[] = {
            (struct umbra_buf){.ptr=lay2.uniforms, .sz=lay2.uni.size, .read_only=1},
            {.ptr=px2, .sz=(size_t)(W * H * 4), .row_bytes=W * 4},
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
    for (int si = 0; si < slide_count() - 1; si++) {
        test_slide_golden(si);
    }
    slides_cleanup();
    free_pipes();
}

// Smoke test for the GPU backends' uniform_ring backpressure path.
// Queues enough byte-distinct uniforms to push ring usage past the
// 64 KiB high water mark a few times within one batch, exercising the
// rotate / drain / reset cycle, and asserts both that (a) the last
// dispatch's color landed in the destination pixel and (b) the
// backend's rotation counter actually advanced — proving the rotate
// path fired and wasn't silently disabled. The bench is still the
// canonical regression for the original silent-cmdbuf-corruption
// failure mode (that requires very large N which becomes flaky under
// parallel ninja test execution).
// STYLE: prefer positive nesting — wrap the body in `if (be) { ... }` rather
// STYLE: than `if (!be) return;`.
static void run_long_batch_no_oom(struct umbra_backend *be) {
    if (!be) { return; }

    struct umbra_builder *bld = umbra_builder();
    umbra_color c = {
        umbra_uniform_32(bld, (umbra_ptr32){0}, 0),
        umbra_uniform_32(bld, (umbra_ptr32){0}, 4),
        umbra_uniform_32(bld, (umbra_ptr32){0}, 8),
        umbra_uniform_32(bld, (umbra_ptr32){0}, 12),
    };
    umbra_store_8888(bld, (umbra_ptr32){.ix=1}, c);
    struct umbra_basic_block *bb = umbra_basic_block(bld);
    umbra_builder_free(bld);

    struct umbra_program *p = be->compile(be, bb);
    umbra_basic_block_free(bb);
    p != 0 here;

    int rotations_before = be->ring_rotations ? be->ring_rotations(be) : 0;

    float    color[4] = {0, 0, 0, 1};
    uint32_t pixel    = 0;
    struct umbra_buf bufs[] = {
        {.ptr=color, .sz=sizeof color, .read_only=1},
        {.ptr=&pixel, .sz=sizeof pixel, .row_bytes=sizeof pixel},
    };
    // 12 000 × 16-byte uniforms = ~192 KiB of ring traffic, ~3 backpressure
    // events at the 64 KiB high water mark.
    int const N = 12000;
    for (int i = 0; i < N; i++) {
        color[0] = (float)((i & 0xff) / 255.0f);
        p->queue(p, 0, 0, 1, 1, bufs);
    }
    be->flush(be);

    uint32_t expected_r = (uint32_t)((N - 1) & 0xff);
    (pixel & 0xff)   == expected_r here;
    (pixel >> 24)    == 0xff here;

    be->ring_rotations != 0 here;
    int rotations_after = be->ring_rotations(be);
    (rotations_after > rotations_before) here;

    p->free(p);
    be->free(be);
}

TEST(test_metal_long_batch_no_oom) {
    run_long_batch_no_oom(umbra_backend_metal());
}

TEST(test_vulkan_long_batch_no_oom) {
    run_long_batch_no_oom(umbra_backend_vulkan());
}

#endif /* !__wasm__ */
