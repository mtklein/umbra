#include "../slides/slide.h"
#include "../slides/text.h"
#include "../slides/slug.h"
#include "test.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct umbra_builder builder;

enum { W = 128, H = 96 };

enum { N_BACKS = 3 };
static char const *backend_name[N_BACKS] = {
    "interp", "jit", "metal",
};

enum {
    FMT_8888, FMT_565, FMT_FP16,
    FMT_FP16P, FMT_1010102, FMT_SRGB, NUM_FMTS,
};
static char const *fmt_name[] = {
    "8888", "565", "fp16", "fp16p", "1010102", "sRGB",
};
static umbra_fmt const fmt_enums[] = {
    umbra_fmt_8888, umbra_fmt_565, umbra_fmt_fp16,
    umbra_fmt_fp16_planar, umbra_fmt_1010102, umbra_fmt_srgb,
};

typedef struct {
    struct umbra_program *prog;
    int                   uni_len;
    int                   out_ptr;
} pipe;

static pipe fill_pipes[NUM_FMTS];
static pipe readback_pipes[NUM_FMTS];
static struct umbra_backend *interp_be;

static void build_fill(int fmt) {
    builder *builder = umbra_builder();
    int fi = umbra_reserve(builder, 4);
    umbra_color c = {
        umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi),
        umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi+1),
        umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi+2),
        umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi+3),
    };
    umbra_store_color(builder, (umbra_ptr){1, 0}, c);
    fill_pipes[fmt].uni_len =
        umbra_uni_len(builder);
    struct umbra_basic_block *opt =
        umbra_basic_block(builder);
    umbra_builder_free(builder);
    fill_pipes[fmt].prog =
        interp_be->compile(interp_be, opt);
    umbra_basic_block_free(opt);
}

static void build_readback(int fmt) {
    builder *builder = umbra_builder();
    umbra_color c =
        umbra_load_color(builder, (umbra_ptr){1, 0});
    umbra_store_color(builder, (umbra_ptr){2, 0}, c);
    readback_pipes[fmt].out_ptr = 2;
    readback_pipes[fmt].uni_len =
        umbra_uni_len(builder);
    struct umbra_basic_block *opt =
        umbra_basic_block(builder);
    umbra_builder_free(builder);
    readback_pipes[fmt].prog =
        interp_be->compile(interp_be, opt);
    umbra_basic_block_free(opt);
}

static void build_pipes(void) {
    interp_be = umbra_backend_interp();
    for (int f = 0; f < NUM_FMTS; f++) {
        build_fill(f);
        build_readback(f);
    }
}

static void free_pipes(void) {
    for (int f = 0; f < NUM_FMTS; f++) {
        fill_pipes[f].prog->free_fn(fill_pipes[f].prog->ctx); free(fill_pipes[f].prog);
        readback_pipes[f].prog->free_fn(readback_pipes[f].prog->ctx); free(readback_pipes[f].prog);
    }
    umbra_backend_free(interp_be);
}

static size_t pixbuf_size(int fmt) {
    int   bpp = umbra_pixel_bytes(fmt_enums[fmt]);
    int   planes = (fmt_enums[fmt] == umbra_fmt_fp16_planar) ? 4 : 1;
    return (size_t)(W * H * bpp * planes);
}

static void fill_bg(int fmt, void *dst, uint32_t bg) {
    float hc[4] = {
        (float)( bg        & 0xffu) / 255.0f,
        (float)((bg >>  8) & 0xffu) / 255.0f,
        (float)((bg >> 16) & 0xffu) / 255.0f,
        (float)((bg >> 24) & 0xffu) / 255.0f,
    };
    uint64_t uni_[4] = {0};
    char *uni = (char*)uni_;
    slide_uni_f32(uni, 0, hc, 4);
    int   bpp = umbra_pixel_bytes(fmt_enums[fmt]);
    umbra_buf buf[2] = {
        {.ptr=uni, .sz=(size_t)fill_pipes[fmt].uni_len, .read_only=1},
        {.ptr=dst, .sz=pixbuf_size(fmt), .row_bytes=(size_t)(W * bpp),
         .fmt=fmt_enums[fmt]},
    };
    umbra_program_queue(fill_pipes[fmt].prog, 0, 0, W, H, buf);
}

static void readback_to_8888(int fmt,
                             void *pixbuf,
                             uint32_t *out) {
    uint64_t uni_[2] = {0};
    char *uni = (char*)uni_;
    int   bpp = umbra_pixel_bytes(fmt_enums[fmt]);
    int   op = readback_pipes[fmt].out_ptr;
    umbra_buf buf[3];
    buf[0] = (umbra_buf){.ptr=uni, .sz=(size_t)readback_pipes[fmt].uni_len, .read_only=1};
    buf[1] = (umbra_buf){.ptr=pixbuf, .sz=pixbuf_size(fmt), .row_bytes=(size_t)(W * bpp),
                         .read_only=1, .fmt=fmt_enums[fmt]};
    buf[op] = (umbra_buf){.ptr=out, .sz=(size_t)(W * H * 4), .row_bytes=(size_t)(W * 4),
                          .fmt=umbra_fmt_8888};
    umbra_program_queue(readback_pipes[fmt].prog, 0, 0, W, H, buf);
}

static void render_slide(
        int slide_idx, int fmt,
        struct umbra_backend *be,
        struct umbra_program *program,
        void *pixbuf,
        umbra_draw_layout const *lay) {
    slide *s = slide_get(slide_idx);

    fill_bg(fmt, pixbuf, s->bg);
    if (s->prepare) { s->prepare(s, W, H, be); }
    s->draw(s, W, H, 0, H, pixbuf,
            lay, program);
}

static void test_slide_golden(
        int slide_idx, int fmt) {
    slide *s = slide_get(slide_idx);


    umbra_fmt saved_fmt = s->fmt;
    s->fmt = fmt_enums[fmt];

    umbra_draw_layout lay;
    struct umbra_builder *bld =
        umbra_draw_build(s->shader, s->coverage,
                         s->blend,
                         &lay);
    struct umbra_basic_block *bb =
        umbra_basic_block(bld);
    umbra_builder_free(bld);

    struct umbra_backend *bes[N_BACKS] = {
        umbra_backend_interp(),
        umbra_backend_jit(),
        umbra_backend_metal(),
    };
    struct umbra_program *progs[N_BACKS];
    for (int bi = 0; bi < N_BACKS; bi++) {
        progs[bi] = bes[bi]
            ? bes[bi]->compile(bes[bi], bb)
            : NULL;
    }
    umbra_basic_block_free(bb);

    size_t pixbuf_sz = pixbuf_size(fmt);
    void *pbuf_ref = calloc(1, pixbuf_sz);
    void *pbuf_tst = calloc(1, pixbuf_sz);
    uint32_t *ref = malloc((size_t)(W * H) * 4);
    uint32_t *tst = malloc((size_t)(W * H) * 4);

    render_slide(slide_idx, fmt,
                 bes[0], progs[0], pbuf_ref, &lay);
    readback_to_8888(fmt, pbuf_ref, ref);

    for (int bi = 1; bi < N_BACKS; bi++) {
        if (!progs[bi]) { continue; }
        render_slide(slide_idx, fmt,
                     bes[bi], progs[bi], pbuf_tst, &lay);
        umbra_backend_flush(bes[bi]);
        readback_to_8888(fmt, pbuf_tst, tst);

        int mismatches = 0;
        int worst = 0;
        for (int i = 0; i < W * H; i++) {
            if (ref[i] == tst[i]) { continue; }
            for (int ch = 0; ch < 4; ch++) {
                int a = (int)(
                    (ref[i] >> (ch*8)) & 0xff);
                int b = (int)(
                    (tst[i] >> (ch*8)) & 0xff);
                int d = a - b;
                if (d < 0) { d = -d; }
                if (d > worst) { worst = d; }
            }
            mismatches++;
        }
        if (worst > 0) {
            dprintf(2,
                "slide %d \"%s\" %s/%s: "
                "%d/%d pixels differ, "
                "worst channel delta = %d\n",
                slide_idx + 1, s->title,
                backend_name[bi], fmt_name[fmt],
                mismatches, W * H, worst);
        }
        worst == 0 here;
    }

    free(ref);
    free(tst);
    free(pbuf_ref);
    free(pbuf_tst);
    for (int bi = 0; bi < N_BACKS; bi++) {
        if (progs[bi]) { progs[bi]->free_fn(progs[bi]->ctx); free(progs[bi]); }
        umbra_backend_free(bes[bi]);
    }
    s->fmt = saved_fmt;
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
        umbra_blend_srcover,
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
    uint64_t au_[12] = {0};
    char *au = (char*)au_;
    slide_uni_f32(au, alay.mat, mat, 11);
    slide_uni_ptr(au, alay.curves_off,
        rect, sizeof rect, 0, 0);
    umbra_buf abuf[] = {
        {.ptr=au, .sz=(size_t)alay.uni_len, .read_only=1},
        {.ptr=wind_buf, .sz=sizeof wind_buf , .row_bytes=W * sizeof(float)},
    };
    for (int j = 0; j < 4; j++) {
        int32_t j32 = j;
        __builtin_memcpy(
            au + alay.loop_off, &j32, 4);
        umbra_program_queue(acc, 0, 0, W, H, abuf);
    }
    umbra_backend_flush(be);

    uint64_t uni_[12] = {0};
    char *uni = (char*)uni_;
    slide_uni_f32(uni, lay.shader, color, 4);
    slide_uni_ptr(uni, lay.coverage,
        wind_buf, sizeof wind_buf, 1, (size_t)W * sizeof(float));
    umbra_buf buf[] = {
        {.ptr=uni, .sz=(size_t)lay.uni_len, .read_only=1},
        {.ptr=pixels, .sz=sizeof pixels, .row_bytes=W * 4, .fmt=umbra_fmt_8888},
    };
    umbra_program_queue(interp, 0, 0, W, H, buf);
    umbra_backend_flush(be);

    uint32_t bg = 0xff000000;
    uint32_t fg = 0xffffffff;
    pixels[20*W + 30] == fg here;
    pixels[20*W +  2] == bg here;
    pixels[20*W + 58] == bg here;
    pixels[ 2*W + 30] == bg here;
    pixels[38*W + 30] == bg here;
    pixels[20*W + 70] == bg here;

    acc->free_fn(acc->ctx); free(acc);
    interp->free_fn(interp->ctx); free(interp);
    umbra_backend_free(be);
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
        umbra_blend_srcover,
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

    uint64_t uni_[12] = {0};
    char *uni = (char*)uni_;
    slide_uni_f32(uni, lay.shader, color, 4);
    slide_uni_f32(uni, lay.coverage, mat, 11);
    slide_uni_ptr(uni,
        (lay.coverage + 11*4 + 7) & ~7,
        bmp, sizeof bmp, 0, 0);
    umbra_buf buf[] = {
        {.ptr=uni, .sz=(size_t)lay.uni_len, .read_only=1},
        {.ptr=pixels, .sz=sizeof pixels, .fmt=umbra_fmt_8888},
    };
    umbra_program_queue(interp, 0, 0, BW, 1, buf);
    umbra_backend_flush(be);

    pixels[8] == 0xffffffff here;
    pixels[0] == 0xff000000 here;

    interp->free_fn(interp->ctx); free(interp);

    text_cov tc = text_rasterize(W, H, 24.0f, 0);

    umbra_draw_layout lay2;
    bld = umbra_draw_build(
        umbra_shader_solid,
        umbra_coverage_bitmap_matrix,
        umbra_blend_srcover,
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
        uint64_t u2_[12] = {0};
        char *u2 = (char*)u2_;
        slide_uni_f32(u2, lay2.shader, hc2, 4);
        slide_uni_f32(u2, lay2.coverage, mat2, 11);
        slide_uni_ptr(u2,
            (lay2.coverage + 11*4 + 7) & ~7,
            tc.data,
            (size_t)(W * H * 2), 0, 0);
        umbra_buf b2[] = {
            {.ptr=u2, .sz=(size_t)lay2.uni_len, .read_only=1},
            {.ptr=px2, .sz=(size_t)(W * H * 4), .row_bytes=W * 4, .fmt=umbra_fmt_8888},
        };
        umbra_program_queue(interp, 0, 0, W, H, b2);
        umbra_backend_flush(be);
    }
    int changed = 0;
    for (int i = 0; i < W * H; i++) {
        if (px2[i] != 0xff0a0a1e) { changed++; }
    }
    changed > 0 here;

    interp->free_fn(interp->ctx); free(interp);
    umbra_backend_free(be);
    text_cov_free(&tc);
}

int main(void) {
    slides_init(W, H);
    build_pipes();

    test_perspective_text();
    test_slug_rect();

    for (int fi = 0; fi < NUM_FMTS; fi++) {
        for (int si = 0; si < slide_count() - 1; si++) {
            test_slide_golden(si, fi);
        }
    }

    free_pipes();
    slides_cleanup();
    return 0;
}
