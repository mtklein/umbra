#include "../slides/slide.h"
#include "../slides/text.h"
#include "../slides/slug.h"
#include "test.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct umbra_builder builder;

enum { W = 128, H = 96 };

enum { N_BACKS = 4 };
static char const *backend_name[N_BACKS] = {
    "interp", "jit", "metal", "vulkan",
};

static struct umbra_backend *bes[N_BACKS];

typedef struct {
    struct umbra_program  *prog;
    struct umbra_uniforms *uni;
} pipe;

static pipe fill_pipe;

static void build_fill(void) {
    builder               *builder = umbra_builder();
    struct umbra_uniforms *u       = calloc(1, sizeof(struct umbra_uniforms));
    size_t fi = umbra_uniforms_reserve_f32(u, 4);
    umbra_color c = {
        umbra_uniform_32(builder, (umbra_ptr){0}, fi),
        umbra_uniform_32(builder, (umbra_ptr){0}, fi + 4),
        umbra_uniform_32(builder, (umbra_ptr){0}, fi + 8),
        umbra_uniform_32(builder, (umbra_ptr){0}, fi + 12),
    };
    umbra_store_color(builder, (umbra_ptr){1}, c, umbra_fmt_8888);
    fill_pipe.uni = u;
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
    if (fill_pipe.uni) { free(fill_pipe.uni->data); free(fill_pipe.uni); }
    for (int bi = 0; bi < N_BACKS; bi++) {
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
    umbra_uniforms_fill_f32(fill_pipe.uni, 0, hc, 4);
    struct umbra_buf buf[2] = {
        (struct umbra_buf){.ptr=fill_pipe.uni->data, .sz=fill_pipe.uni->size, .read_only=1},
        {.ptr=dst, .sz=(size_t)(W * H * 4), .row_bytes=(size_t)W * 4},
    };
    fill_pipe.prog->queue(fill_pipe.prog, 0, 0, W, H, buf);
}


static void render_slide(
        int slide_idx,
        struct umbra_backend *be,
        void *pixbuf) {
    slide *s = slide_get(slide_idx);

    fill_bg(pixbuf, s->bg);
    s->prepare(s, W, H, be);
    s->draw(s, W, H, 0, H, pixbuf);
}

static void test_slide_golden(int slide_idx) {
    slide *s = slide_get(slide_idx);

    size_t pixbuf_sz = (size_t)(W * H * 4);
    void *pbuf_ref = calloc(1, pixbuf_sz);
    void *pbuf_tst = calloc(1, pixbuf_sz);

    render_slide(slide_idx, bes[0], pbuf_ref);

    for (int bi = 1; bi < N_BACKS; bi++) {
        if (!bes[bi]) { continue; }
        render_slide(slide_idx, bes[bi], pbuf_tst);
        bes[bi]->flush(bes[bi]);

        int mismatches = 0;
        int worst = 0;
        uint8_t const *r = pbuf_ref, *t = pbuf_tst;
        for (int i = 0; i < W * H; i++) {
            _Bool differ = 0;
            uint32_t rp, tp;
            __builtin_memcpy(&rp, r+i*4, 4);
            __builtin_memcpy(&tp, t+i*4, 4);
            for (int ch = 0; ch < 4; ch++) {
                int d = (int)((rp>>(ch*8))&0xFF) - (int)((tp>>(ch*8))&0xFF);
                if (d<0) d=-d;
                if (d>worst) worst=d;
                if (d) differ=1;
            }
            if (differ) mismatches++;
        }
        int tol = 0;
        if (worst > tol) {
            dprintf(2,
                "slide %d \"%s\" %s: "
                "%d/%d pixels differ, "
                "worst channel delta = %d\n",
                slide_idx + 1, s->title,
                backend_name[bi],
                mismatches, W * H, worst);
        }
        (worst <= tol) here;
    }

    free(pbuf_ref);
    free(pbuf_tst);
}

static void test_slug_rect(void) {
    static float rect[] = {
         5, 5,  5,20,  5,35,
         5,35, 30,35, 55,35,
        55,35, 55,20, 55, 5,
        55, 5, 30, 5,  5, 5,
    };

    slug_acc_layout alay;
    builder *ab = slug_build_acc(&alay);
    struct umbra_basic_block *abb =
        umbra_basic_block(ab);
    umbra_builder_free(ab);
    struct umbra_backend *be =
        umbra_backend_interp();
    struct umbra_program *acc =
        be->compile(be, abb);
    umbra_basic_block_free(abb);

    umbra_draw_layout lay;
    builder *bld = umbra_draw_build(
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
    umbra_uniforms_fill_f32(alay.uni, alay.mat, mat, 11);
    umbra_uniforms_fill_ptr(alay.uni, alay.curves_off,
        (struct umbra_buf){.ptr=rect, .sz=sizeof rect});
    struct umbra_buf abuf[] = {
        (struct umbra_buf){.ptr=alay.uni->data, .sz=alay.uni->size, .read_only=1},
        {.ptr=wind_buf, .sz=sizeof wind_buf, .row_bytes=W * sizeof(float)},
    };
    for (int j = 0; j < 4; j++) {
        float jf;
        int32_t j32 = j;
        __builtin_memcpy(&jf, &j32, 4);
        umbra_uniforms_fill_f32(alay.uni, alay.loop_off, &jf, 1);
        abuf[0] = (struct umbra_buf){.ptr=alay.uni->data, .sz=alay.uni->size, .read_only=1};
        acc->queue(acc, 0, 0, W, H, abuf);
    }
    be->flush(be);

    umbra_uniforms_fill_f32(lay.uni, lay.shader, color, 4);
    umbra_uniforms_fill_ptr(lay.uni, lay.coverage,
        (struct umbra_buf){.ptr=wind_buf, .sz=sizeof wind_buf, .read_only=1, .row_bytes=(size_t)W * sizeof(float)});
    struct umbra_buf buf[] = {
        (struct umbra_buf){.ptr=lay.uni->data, .sz=lay.uni->size, .read_only=1},
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

    if (lay.uni) { free(lay.uni->data); free(lay.uni); }
    if (alay.uni) { free(alay.uni->data); free(alay.uni); }
    acc->free(acc);
    interp->free(interp);
    be->free(be);
}

static void test_perspective_text(void) {
    enum { BW = 16, BH = 8 };
    uint16_t bmp[BW * BH];
    __builtin_memset(bmp, 0, sizeof bmp);
    bmp[0 * BW + 8] = 255;

    struct umbra_backend *be =
        umbra_backend_interp();

    umbra_draw_layout lay;
    builder *bld = umbra_draw_build(
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

    umbra_uniforms_fill_f32(lay.uni, lay.shader, color, 4);
    umbra_uniforms_fill_f32(lay.uni, lay.coverage, mat, 11);
    umbra_uniforms_fill_ptr(lay.uni, (lay.coverage + 44 + 7) & ~(size_t)7,
        (struct umbra_buf){.ptr=bmp, .sz=sizeof bmp});
    struct umbra_buf buf[] = {
        (struct umbra_buf){.ptr=lay.uni->data, .sz=lay.uni->size, .read_only=1},
        {.ptr=pixels, .sz=sizeof pixels},
    };
    interp->queue(interp, 0, 0, BW, 1, buf);
    be->flush(be);

    pixels[8] == 0xffffffff here;
    pixels[0] == 0xff000000 here;

    interp->free(interp);

    text_cov tc = text_rasterize(W, H, 24.0f, 0);

    umbra_draw_layout lay2;
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
        umbra_uniforms_fill_f32(lay2.uni, lay2.shader, hc2, 4);
        umbra_uniforms_fill_f32(lay2.uni, lay2.coverage, mat2, 11);
        umbra_uniforms_fill_ptr(lay2.uni, (lay2.coverage + 44 + 7) & ~(size_t)7,
            (struct umbra_buf){.ptr=tc.data, .sz=(size_t)(W * H * 2)});
        struct umbra_buf b2[] = {
            (struct umbra_buf){.ptr=lay2.uni->data, .sz=lay2.uni->size, .read_only=1},
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

    if (lay.uni) { free(lay.uni->data); free(lay.uni); }
    if (lay2.uni) { free(lay2.uni->data); free(lay2.uni); }
    interp->free(interp);
    be->free(be);
    text_cov_free(&tc);
}

int main(void) {
    slides_init(W, H);
    build_pipes();

    test_perspective_text();
    test_slug_rect();

    for (int si = 0; si < slide_count() - 1; si++) {
        test_slide_golden(si);
    }

    slides_cleanup();
    free_pipes();
    return 0;
}
