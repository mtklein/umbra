#include "slide.h"
#include "coverage.h"
#include "text.h"
#include <stdio.h>
#include <stdlib.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "../third_party/stb/stb_truetype.h"
#pragma clang diagnostic pop

umbra_val32 coverage_bitmap(void *ctx, struct umbra_builder *b,
                            umbra_val32 x, umbra_val32 y) {
    struct umbra_buf const *self = ctx;
    (void)x; (void)y;
    umbra_ptr const bmp = umbra_bind_sealed_buf(b, self);
    umbra_val32 const val = umbra_i32_from_u16(b, umbra_load_16(b, bmp));
    umbra_val32 const inv255 = umbra_imm_f32(b, 1.0f / 255.0f);
    return umbra_mul_f32(b, umbra_f32_from_i32(b, val), inv255);
}

umbra_val32 coverage_sdf(void *ctx, struct umbra_builder *b,
                         umbra_val32 x, umbra_val32 y) {
    struct umbra_buf const *self = ctx;
    (void)x; (void)y;
    umbra_ptr const bmp = umbra_bind_sealed_buf(b, self);
    umbra_val32 const raw = umbra_i32_from_u16(b, umbra_load_16(b, bmp));
    umbra_val32 const inv255 = umbra_imm_f32(b, 1.0f / 255.0f);
    umbra_val32 const dist = umbra_mul_f32(b, umbra_f32_from_i32(b, raw), inv255);
    umbra_val32 const lo = umbra_imm_f32(b, 0.4375f);
    umbra_val32 const scale = umbra_imm_f32(b, 8.0f);
    umbra_val32 const shifted = umbra_sub_f32(b, dist, lo);
    umbra_val32 const scaled = umbra_mul_f32(b, shifted, scale);
    umbra_val32 const zero = umbra_imm_f32(b, 0.0f);
    umbra_val32 const one = umbra_imm_f32(b, 1.0f);
    return umbra_min_f32(b, umbra_max_f32(b, scaled, zero), one);
}

// Gather-based bitmap sampler for arbitrary (x, y) sample points.  Samples
// outside [0, w) x [0, h) return 0.  w and h are baked at IR-build time
// (read from self-> directly, emitted as imms).
umbra_val32 coverage_bitmap2d(void *ctx, struct umbra_builder *b,
                              umbra_val32 x, umbra_val32 y) {
    struct coverage_bitmap2d const *self = ctx;
    umbra_ptr const pixels = umbra_bind_sealed_buf(b, &self->buf);

    umbra_val32 const zero_f = umbra_imm_f32(b, 0.0f);
    umbra_val32 const bw     = umbra_imm_f32(b, (float)self->w);
    umbra_val32 const bh     = umbra_imm_f32(b, (float)self->h);

    umbra_val32 const in = umbra_and_32(b,
                                        umbra_and_32(b, umbra_le_f32(b, zero_f, x),
                                                        umbra_lt_f32(b, x, bw)),
                                        umbra_and_32(b, umbra_le_f32(b, zero_f, y),
                                                        umbra_lt_f32(b, y, bh)));

    umbra_val32 const one_f = umbra_imm_f32(b, 1.0f);
    umbra_val32 const xc = umbra_min_f32(b, umbra_max_f32(b, x, zero_f),
                                         umbra_sub_f32(b, bw, one_f));
    umbra_val32 const yc = umbra_min_f32(b, umbra_max_f32(b, y, zero_f),
                                         umbra_sub_f32(b, bh, one_f));
    umbra_val32 const xi = umbra_floor_i32(b, xc);
    umbra_val32 const yi = umbra_floor_i32(b, yc);
    umbra_val32 const wi = umbra_imm_i32(b, self->w);
    umbra_val32 const idx = umbra_add_i32(b, umbra_mul_i32(b, yi, wi), xi);

    umbra_val32 const val    = umbra_i32_from_u16(b, umbra_gather_16(b, pixels, idx));
    umbra_val32 const inv255 = umbra_imm_f32(b, 1.0f / 255.0f);
    umbra_val32 const cov    = umbra_mul_f32(b, umbra_f32_from_i32(b, val), inv255);

    return umbra_sel_32(b, in, cov, zero_f);
}

umbra_val32 coverage_sdf2d(void *ctx, struct umbra_builder *b,
                           umbra_val32 x, umbra_val32 y) {
    umbra_val32 const dist = coverage_bitmap2d(ctx, b, x, y);
    umbra_val32 const lo      = umbra_imm_f32(b, 0.4375f);
    umbra_val32 const scale   = umbra_imm_f32(b, 8.0f);
    umbra_val32 const shifted = umbra_sub_f32(b, dist, lo);
    umbra_val32 const scaled  = umbra_mul_f32(b, shifted, scale);
    umbra_val32 const zero    = umbra_imm_f32(b, 0.0f);
    umbra_val32 const one     = umbra_imm_f32(b, 1.0f);
    return umbra_min_f32(b, umbra_max_f32(b, scaled, zero), one);
}

static unsigned char* text_load_font(char const *path) {
    unsigned char *buf = NULL;
    FILE *f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        buf = malloc((size_t)sz);
        if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
            free(buf);
            buf = NULL;
        }
        fclose(f);
    }
    return buf;
}

struct text_cov text_rasterize(int W, int H, float font_size, _Bool sdf) {
    struct text_cov tc = { .data = calloc((size_t)(W * H), sizeof(uint16_t)), .w = W, .h = H };

    unsigned char *font_data = text_load_font("/System/Library/Fonts/Supplemental/Arial.ttf");
    if (font_data) {
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
                    if (py >= 0 && py < H) {
                        for (int col = 0; col < gw; col++) {
                            int px = dx + col;
                            if (px >= 0 && px < W) {
                                uint16_t const val = bmp[row * gw + col];
                                uint16_t const cur = tc.data[py * W + px];
                                if (val > cur) { tc.data[py * W + px] = val; }
                            }
                        }
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
    }
    return tc;
}

void text_cov_free(struct text_cov *tc) {
    free(tc->data);
    tc->data = NULL;
}

// Coverage (8-bit bitmap) and Coverage (SDF bitmap): sample a glyph-mask
// through coverage_bitmap2d / coverage_sdf2d so the slide composes under an
// outer transform (e.g. the overview's cell matrix).  When the slide stands
// alone the dispatch (x, y) equals the bitmap (x, y), so the gather-based
// samplers reproduce the linear-load coverage_bitmap / coverage_sdf output.
struct text_slide {
    struct slide base;

    struct text_cov          tc;
    _Bool                    sdf; int :24, :32;

    umbra_color              color;
    struct coverage_bitmap2d bmp;
    umbra_coverage          *coverage_fn;
};

static void text_init(struct slide *s) {
    struct text_slide *st = (struct text_slide *)s;
    st->tc = text_rasterize(s->w, s->h, (float)s->h * 0.15f, st->sdf);
    st->bmp = (struct coverage_bitmap2d){
        .buf = {.ptr = st->tc.data, .count = st->tc.w * st->tc.h},
        .w   = st->tc.w,
        .h   = st->tc.h,
    };
}

static void text_build_draw(struct slide *s, struct umbra_builder *b,
                            umbra_ptr dst_ptr, struct umbra_fmt fmt,
                            umbra_val32 x, umbra_val32 y) {
    struct text_slide *st = (struct text_slide *)s;
    umbra_build_draw(b, dst_ptr, fmt, x, y,
                     st->coverage_fn,     &st->bmp,
                     umbra_shader_color,  &st->color,
                     umbra_blend_srcover, NULL);
}

static void text_free(struct slide *s) {
    struct text_slide *st = (struct text_slide *)s;
    text_cov_free(&st->tc);
    free(s);
}

// Coverage NULL: draw_builder with coverage=NULL, testing the no-coverage
// fast path.
struct cov_null_slide {
    struct slide base;
    umbra_color  color;
};

static void cov_null_build_draw(struct slide *s, struct umbra_builder *b,
                                umbra_ptr dst_ptr, struct umbra_fmt fmt,
                                umbra_val32 x, umbra_val32 y) {
    struct cov_null_slide *st = (struct cov_null_slide *)s;
    umbra_build_draw(b, dst_ptr, fmt, x, y,
                     NULL,                NULL,
                     umbra_shader_color,  &st->color,
                     umbra_blend_srcover, NULL);
}

static void cov_null_free(struct slide *s) { free(s); }

SLIDE(slide_coverage_null) {
    struct cov_null_slide *st = calloc(1, sizeof *st);
    st->color = (umbra_color){0.15f, 0.0f, 0.3f, 0.3f};
    st->base = (struct slide){
        .title = "Coverage NULL",
        .bg = {1, 1, 1, 1},
        .free = cov_null_free,
        .build_draw   = cov_null_build_draw,
    };
    return &st->base;
}

SLIDE(slide_coverage_bitmap) {
    struct text_slide *st = calloc(1, sizeof *st);
    st->sdf         = 0;
    st->color       = (umbra_color){1.0f, 1.0f, 1.0f, 1.0f};
    st->coverage_fn = coverage_bitmap2d;
    st->base = (struct slide){
        .title = "Coverage (8-bit bitmap)",
        .bg = {0.18f, 0.1f, 0.1f, 1},
        .init = text_init,
        .free = text_free,
        .build_draw   = text_build_draw,
    };
    return &st->base;
}

SLIDE(slide_coverage_sdf_bitmap) {
    struct text_slide *st = calloc(1, sizeof *st);
    st->sdf         = 1;
    st->color       = (umbra_color){0.2f, 0.8f, 1.0f, 1.0f};
    st->coverage_fn = coverage_sdf2d;
    st->base = (struct slide){
        .title = "Coverage (SDF bitmap)",
        .bg = {0.18f, 0.1f, 0.1f, 1},
        .init = text_init,
        .free = text_free,
        .build_draw   = text_build_draw,
    };
    return &st->base;
}

// Coverage (8-bit bitmap + matrix): sample the glyph-mask through a
// coverage_matrix combinator wrapping coverage_bitmap2d.
struct persp_slide {
    struct slide base;

    struct text_cov           bitmap;

    umbra_color               color;
    struct coverage_bitmap2d  bmp;
    struct umbra_matrix       mat; int :32;
};

static void persp_init(struct slide *s) {
    struct persp_slide *st = (struct persp_slide *)s;
    st->bitmap = text_rasterize(s->w, s->h, (float)s->h * 0.15f, 0);
    st->bmp = (struct coverage_bitmap2d){
        .buf = {.ptr = st->bitmap.data, .count = st->bitmap.w * st->bitmap.h},
        .w   = st->bitmap.w,
        .h   = st->bitmap.h,
    };
}

static void persp_build_draw(struct slide *s, struct umbra_builder *b,
                             umbra_ptr dst_ptr, struct umbra_fmt fmt,
                             umbra_val32 x, umbra_val32 y) {
    struct persp_slide *st = (struct persp_slide *)s;
    umbra_point_val32 const p = umbra_transform_perspective(&st->mat, b, x, y);
    umbra_build_draw(b, dst_ptr, fmt, p.x, p.y,
                     coverage_bitmap2d,   &st->bmp,
                     umbra_shader_color,  &st->color,
                     umbra_blend_srcover, NULL);
}

static void persp_animate(struct slide *s, double secs) {
    struct persp_slide *st = (struct persp_slide *)s;
    slide_perspective_matrix(&st->mat, (float)secs, s->w, s->h,
                             st->bitmap.w, st->bitmap.h);
}

static void persp_free(struct slide *s) {
    struct persp_slide *st = (struct persp_slide *)s;
    text_cov_free(&st->bitmap);
    free(s);
}

SLIDE(slide_coverage_bitmap_matrix) {
    struct persp_slide *st = calloc(1, sizeof *st);
    st->color  = (umbra_color){1.0f, 0.8f, 0.2f, 1.0f};
    st->base = (struct slide){
        .title = "Coverage (8-bit bitmap + matrix)",
        .bg = {0.12f, 0.04f, 0.04f, 1},
        .init = persp_init,
        .free = persp_free,
        .build_draw   = persp_build_draw,
        .animate      = persp_animate,
    };
    return &st->base;
}
