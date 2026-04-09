#include "slide.h"
#include "text.h"
#include <stdio.h>
#include <stdlib.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "../third_party/stb/stb_truetype.h"
#pragma clang diagnostic pop

static unsigned char* text_load_font(const char *path) {
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

struct text_state {
    struct slide base;

    struct text_cov *tc;
    float            color[4];
    int              w, h;

    umbra_shader_fn    shader;
    umbra_coverage_fn  coverage;
    umbra_blend_fn     blend;

    struct umbra_fmt            fmt;
    struct umbra_draw_layout   lay;
    struct umbra_basic_block  *bb;
    struct umbra_program      *prog;
};

static void text_init(struct slide *s, int w, int h) {
    struct text_state *st = (struct text_state *)s;
    st->w = w;
    st->h = h;
}

static void text_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct text_state *st = (struct text_state *)s;
    if (st->fmt.name != fmt.name || !st->bb) {
        st->fmt = fmt;
        umbra_basic_block_free(st->bb);
        free(st->lay.uniforms);
        struct umbra_builder *b = umbra_draw_build(st->shader, st->coverage, st->blend, fmt,
                                                    &st->lay);
        st->bb = umbra_basic_block(b);
        umbra_builder_free(b);
    }
    if (st->prog) { st->prog->free(st->prog); }
    st->prog = be->compile(be, st->bb);
}

static void text_draw(struct slide *s, int frame, int l, int t, int r, int b, void *buf) {
    struct text_state *st = (struct text_state *)s;
    (void)frame;
    umbra_uniforms_fill_f32(st->lay.uniforms, st->lay.shader, st->color, 4);
    umbra_uniforms_fill_ptr(st->lay.uniforms, st->lay.coverage,
                  (struct umbra_buf){.ptr=st->tc->data, .sz=(size_t)(st->w * st->h * 2), .row_bytes=(size_t)st->w * 2});
    size_t    pb = st->fmt.bpp;
    size_t plane_sz = (size_t)st->w * (size_t)st->h * pb;
    size_t rb = (size_t)st->w * pb;
    struct umbra_buf ubuf[] = {
        {.ptr=st->lay.uniforms, .sz=st->lay.uni.size, .read_only=1},
        {.ptr=buf, .sz=plane_sz * (size_t)st->fmt.planes, .row_bytes=rb},
    };
    st->prog->queue(st->prog, l, t, r, b, ubuf);
}

static struct umbra_builder *text_get_builder(struct slide *s, struct umbra_fmt fmt) {
    struct text_state *st = (struct text_state *)s;
    return umbra_draw_build(st->shader, st->coverage, st->blend, fmt, NULL);
}

static void text_free(struct slide *s) {
    struct text_state *st = (struct text_state *)s;
    if (st->prog) { st->prog->free(st->prog); }
    umbra_basic_block_free(st->bb);
    free(st->lay.uniforms);
    free(st);
}

SLIDE(slide_text_bitmap) {
    struct text_state *st = calloc(1, sizeof *st);
    st->tc = ctx->bitmap_cov;
    st->shader = umbra_shader_solid;
    st->coverage = umbra_coverage_bitmap;
    st->blend = umbra_blend_srcover;
    st->fmt = umbra_fmt_8888;
    st->color[0] = 1.0f; st->color[1] = 1.0f; st->color[2] = 1.0f; st->color[3] = 1.0f;
    st->base = (struct slide){
        .title = "Text (8-bit AA)",
        .bg = 0xff1a1a2e,
        .init = text_init,
        .prepare = text_prepare,
        .draw = text_draw,
        .free = text_free,
        .get_builder = text_get_builder,
    };
    return &st->base;
}

SLIDE(slide_text_sdf) {
    struct text_state *st = calloc(1, sizeof *st);
    st->tc = ctx->sdf_cov;
    st->shader = umbra_shader_solid;
    st->coverage = umbra_coverage_sdf;
    st->blend = umbra_blend_srcover;
    st->fmt = umbra_fmt_8888;
    st->color[0] = 0.2f; st->color[1] = 0.8f; st->color[2] = 1.0f; st->color[3] = 1.0f;
    st->base = (struct slide){
        .title = "Text (SDF)",
        .bg = 0xff1a1a2e,
        .init = text_init,
        .prepare = text_prepare,
        .draw = text_draw,
        .free = text_free,
        .get_builder = text_get_builder,
    };
    return &st->base;
}
