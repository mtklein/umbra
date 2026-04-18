#include "slide.h"
#include "text.h"
#include <stdio.h>
#include <stdlib.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "../third_party/stb/stb_truetype.h"
#pragma clang diagnostic pop

static unsigned char* text_load_font(char const *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = malloc((size_t)sz);
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    return buf;
}

struct text_cov text_rasterize(int W, int H, float font_size, _Bool sdf) {
    struct text_cov tc = { .data = calloc((size_t)(W * H), sizeof(uint16_t)), .w = W, .h = H };

    unsigned char *font_data = text_load_font("/System/Library/Fonts/Supplemental/Arial.ttf");
    if (!font_data) { return tc; }

    stbtt_fontinfo font;
    stbtt_InitFont(&font, font_data, stbtt_GetFontOffsetForIndex(font_data, 0));
    float scale = stbtt_ScaleForPixelHeight(&font, font_size);

    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &line_gap);

    char const *text = "hamburgefons";

    // Measure total width for centering.
    float total_w = 0;
    for (int i = 0; text[i]; i++) {
        int adv, lsb;
        stbtt_GetCodepointHMetrics(&font, text[i], &adv, &lsb);
        total_w += (float)adv * scale;
        if (text[i+1]) {
            int kern = stbtt_GetCodepointKernAdvance(&font, text[i], text[i+1]);
            total_w += (float)kern * scale;
        }
    }

    float cx = ((float)W - total_w) / 2;
    int baseline = (H - (int)font_size) / 2 + (int)((float)ascent * scale);

    for (int i = 0; text[i]; i++) {
        int gw, gh, gx, gy;
        unsigned char *bmp;

        if (sdf) {
            bmp = stbtt_GetCodepointSDF(&font, scale, text[i],
                                        5, 128, 128.0f / 5.0f,
                                        &gw, &gh, &gx, &gy);
        } else {
            bmp = stbtt_GetCodepointBitmap(&font, scale, scale, text[i],
                                           &gw, &gh, &gx, &gy);
        }

        if (bmp) {
            int dx = (int)cx + gx;
            int dy = baseline + gy;
            for (int row = 0; row < gh; row++) {
                int py = dy + row;
                if (py < 0 || py >= H) { continue; }
                for (int col = 0; col < gw; col++) {
                    int px = dx + col;
                    if (px < 0 || px >= W) { continue; }
                    uint8_t val = bmp[row * gw + col];
                    uint16_t cur = tc.data[py * W + px];
                    if (val > cur) { tc.data[py * W + px] = val; }
                }
            }
            if (sdf) { stbtt_FreeSDF(bmp, NULL); }
            else     { stbtt_FreeBitmap(bmp, NULL); }
        }

        int adv, lsb;
        stbtt_GetCodepointHMetrics(&font, text[i], &adv, &lsb);
        cx += (float)adv * scale;
        if (text[i+1]) {
            int kern = stbtt_GetCodepointKernAdvance(&font, text[i], text[i+1]);
            cx += (float)kern * scale;
        }
    }

    free(font_data);
    return tc;
}

void text_cov_free(struct text_cov *tc) {
    free(tc->data);
    tc->data = NULL;
}

static struct text_cov shared_bitmap, shared_sdf;

void text_shared_init(int w, int h, float font_size) {
    shared_bitmap = text_rasterize(w, h, font_size, 0);
    shared_sdf    = text_rasterize(w, h, font_size, 1);
}
void text_shared_cleanup(void) {
    text_cov_free(&shared_bitmap);
    text_cov_free(&shared_sdf);
}
struct text_cov* text_shared_bitmap(void) { return &shared_bitmap; }
struct text_cov* text_shared_sdf   (void) { return &shared_sdf; }

// Coverage (8-bit bitmap) and Coverage (SDF bitmap): sample a glyph-mask
// through umbra_coverage_{bitmap,sdf}.
struct text_slide {
    struct slide base;

    struct text_cov *tc;
    int              w, h;
    int              is_sdf, :32;

    struct umbra_shader       *shader;
    struct umbra_coverage     *cov;

    struct umbra_fmt            fmt;
    struct umbra_program      *prog;
};

static void text_init(struct slide *s, int w, int h) {
    struct text_slide *st = (struct text_slide *)s;
    st->w = w;
    st->h = h;
}

static void text_prepare(struct slide *s, struct umbra_backend *be,
                         struct umbra_fmt fmt) {
    struct text_slide *st = (struct text_slide *)s;
    umbra_program_free(st->prog);
    umbra_coverage_free(st->cov);
    struct umbra_buf bmp = {
        .ptr    = st->tc->data,
        .count  = st->w * st->h,
        .stride = st->w,
    };
    st->cov = st->is_sdf ? umbra_coverage_sdf(bmp) : umbra_coverage_bitmap(bmp);
    st->fmt = fmt;
    struct umbra_builder *b = umbra_draw_builder(st->cov, st->shader,
                                                 umbra_blend_srcover, fmt);
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    st->prog = be->compile(be, ir);
    umbra_flat_ir_free(ir);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void text_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct text_slide *st = (struct text_slide *)s;
    (void)secs;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    struct umbra_buf ubuf[] = {
        {.ptr=buf, .count=st->w * st->h * st->fmt.planes, .stride=st->w},
        st->cov->uniforms,
        st->shader->uniforms,
    };
    st->prog->queue(st->prog, l, t, r, b, ubuf);
}

static int text_get_builders(struct slide *s, struct umbra_fmt fmt,
                             struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct text_slide *st = (struct text_slide *)s;
    out[0] = umbra_draw_builder(st->cov, st->shader,
                                umbra_blend_srcover, fmt);
    return out[0] ? 1 : 0;
}

static void text_free(struct slide *s) {
    struct text_slide *st = (struct text_slide *)s;
    umbra_program_free(st->prog);
    umbra_shader_free  (st->shader);
    umbra_coverage_free(st->cov);
    free(st);
}

SLIDE(slide_coverage_bitmap) {
    struct text_slide *st = calloc(1, sizeof *st);
    st->tc     = text_shared_bitmap();
    st->is_sdf = 0;
    st->shader = umbra_shader_solid((umbra_color){1.0f, 1.0f, 1.0f, 1.0f});
    st->cov    = umbra_coverage_bitmap((struct umbra_buf){0});
    st->base = (struct slide){
        .title = "Coverage (8-bit bitmap)",
        .bg = {0.18f, 0.1f, 0.1f, 1},
        .init = text_init,
        .prepare = text_prepare,
        .draw = text_draw,
        .free = text_free,
        .get_builders = text_get_builders,
    };
    return &st->base;
}

SLIDE(slide_coverage_sdf_bitmap) {
    struct text_slide *st = calloc(1, sizeof *st);
    st->tc     = text_shared_sdf();
    st->is_sdf = 1;
    st->shader = umbra_shader_solid((umbra_color){0.2f, 0.8f, 1.0f, 1.0f});
    st->cov    = umbra_coverage_sdf((struct umbra_buf){0});
    st->base = (struct slide){
        .title = "Coverage (SDF bitmap)",
        .bg = {0.18f, 0.1f, 0.1f, 1},
        .init = text_init,
        .prepare = text_prepare,
        .draw = text_draw,
        .free = text_free,
        .get_builders = text_get_builders,
    };
    return &st->base;
}

// Coverage (8-bit bitmap + matrix): sample the glyph-mask through
// umbra_coverage_bitmap_matrix with an animated perspective transform.
struct persp_slide {
    struct slide base;

    struct text_cov *bitmap;
    int              w, h;

    struct umbra_shader        *shader;
    struct umbra_coverage      *cov;

    struct umbra_fmt            fmt;
    struct umbra_program       *prog;
};

static void persp_init(struct slide *s, int w, int h) {
    struct persp_slide *st = (struct persp_slide *)s;
    st->w = w;
    st->h = h;
}

static void persp_prepare(struct slide *s, struct umbra_backend *be,
                          struct umbra_fmt fmt) {
    struct persp_slide *st = (struct persp_slide *)s;
    umbra_program_free(st->prog);
    umbra_coverage_free(st->cov);
    struct umbra_bitmap bmp = {
        .buf = {.ptr = st->bitmap->data, .count = st->bitmap->w * st->bitmap->h},
        .w   = st->bitmap->w,
        .h   = st->bitmap->h,
    };
    st->cov = umbra_coverage_bitmap_matrix((struct umbra_matrix){0}, bmp);
    st->fmt = fmt;
    struct umbra_builder *b = umbra_draw_builder(st->cov, st->shader,
                                                 umbra_blend_srcover, fmt);
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    st->prog = be->compile(be, ir);
    umbra_flat_ir_free(ir);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void persp_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct persp_slide *st = (struct persp_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    struct umbra_matrix mat;
    slide_perspective_matrix(&mat, (float)secs, st->w, st->h,
                             st->bitmap->w, st->bitmap->h);
    struct umbra_bitmap bmp = {
        .buf = {.ptr = st->bitmap->data, .count = st->bitmap->w * st->bitmap->h},
        .w   = st->bitmap->w,
        .h   = st->bitmap->h,
    };
    umbra_coverage_free(st->cov);
    st->cov = umbra_coverage_bitmap_matrix(mat, bmp);
    struct umbra_buf ubuf[] = {
        {.ptr=buf, .count=st->w * st->h * st->fmt.planes, .stride=st->w},
        st->cov->uniforms,
        st->shader->uniforms,
    };
    st->prog->queue(st->prog, l, t, r, b, ubuf);
}

static int persp_get_builders(struct slide *s, struct umbra_fmt fmt,
                              struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct persp_slide *st = (struct persp_slide *)s;
    out[0] = umbra_draw_builder(st->cov, st->shader, umbra_blend_srcover, fmt);
    return out[0] ? 1 : 0;
}

static void persp_free(struct slide *s) {
    struct persp_slide *st = (struct persp_slide *)s;
    umbra_program_free(st->prog);
    umbra_shader_free  (st->shader);
    umbra_coverage_free(st->cov);
    free(st);
}

SLIDE(slide_coverage_bitmap_matrix) {
    struct persp_slide *st = calloc(1, sizeof *st);
    st->bitmap = text_shared_bitmap();
    st->shader = umbra_shader_solid((umbra_color){1.0f, 0.8f, 0.2f, 1.0f});
    st->cov    = umbra_coverage_bitmap_matrix((struct umbra_matrix){0}, (struct umbra_bitmap){0});
    st->base = (struct slide){
        .title = "Coverage (8-bit bitmap + matrix)",
        .bg = {0.12f, 0.04f, 0.04f, 1},
        .init = persp_init,
        .prepare = persp_prepare,
        .draw = persp_draw,
        .free = persp_free,
        .get_builders = persp_get_builders,
    };
    return &st->base;
}

// Coverage NULL: draw_builder with coverage=NULL, testing the no-coverage
// fast path.
struct cov_null_slide {
    struct slide base;

    int w, h;

    struct umbra_shader  *shader;
    struct umbra_fmt      fmt;
    struct umbra_flat_ir *ir;
    struct umbra_program *prog;
};

static void cov_null_init(struct slide *s, int w, int h) {
    struct cov_null_slide *st = (struct cov_null_slide *)s;
    st->w = w;
    st->h = h;
}

static void cov_null_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct cov_null_slide *st = (struct cov_null_slide *)s;
    if (st->fmt.name != fmt.name || !st->ir) {
        st->fmt = fmt;
        umbra_flat_ir_free(st->ir);
        struct umbra_builder *b = umbra_draw_builder(NULL, st->shader,
                                                     umbra_blend_srcover, fmt);
        st->ir = umbra_flat_ir(b);
        umbra_builder_free(b);
    }
    umbra_program_free(st->prog);
    st->prog = be->compile(be, st->ir);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void cov_null_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct cov_null_slide *st = (struct cov_null_slide *)s;
    (void)secs;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    struct umbra_buf ubuf[] = {
        {.ptr=buf, .count=st->w * st->h * st->fmt.planes, .stride=st->w},
        {0},
        st->shader->uniforms,
    };
    st->prog->queue(st->prog, l, t, r, b, ubuf);
}

static int cov_null_get_builders(struct slide *s, struct umbra_fmt fmt,
                                 struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct cov_null_slide *st = (struct cov_null_slide *)s;
    out[0] = umbra_draw_builder(NULL, st->shader, umbra_blend_srcover, fmt);
    return out[0] ? 1 : 0;
}

static void cov_null_free(struct slide *s) {
    struct cov_null_slide *st = (struct cov_null_slide *)s;
    umbra_program_free(st->prog);
    umbra_flat_ir_free(st->ir);
    umbra_shader_free (st->shader);
    free(st);
}

SLIDE(slide_coverage_null) {
    struct cov_null_slide *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid((umbra_color){0.15f, 0.0f, 0.3f, 0.3f});
    st->base = (struct slide){
        .title = "Coverage NULL",
        .bg = {1, 1, 1, 1},
        .init = cov_null_init,
        .prepare = cov_null_prepare,
        .draw = cov_null_draw,
        .free = cov_null_free,
        .get_builders = cov_null_get_builders,
    };
    return &st->base;
}
