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
    umbra_ptr16 const bmp = umbra_bind_buf16(b, self);
    umbra_val32 const val = umbra_i32_from_s16(b, umbra_load_16(b, bmp));
    umbra_val32 const inv255 = umbra_imm_f32(b, 1.0f / 255.0f);
    return umbra_mul_f32(b, umbra_f32_from_i32(b, val), inv255);
}

umbra_val32 coverage_sdf(void *ctx, struct umbra_builder *b,
                         umbra_val32 x, umbra_val32 y) {
    struct umbra_buf const *self = ctx;
    (void)x; (void)y;
    umbra_ptr16 const bmp = umbra_bind_buf16(b, self);
    umbra_val32 const raw = umbra_i32_from_s16(b, umbra_load_16(b, bmp));
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
    umbra_ptr16 const pixels = umbra_bind_buf16(b, &self->buf);

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

    umbra_val32 const val    = umbra_i32_from_s16(b, umbra_gather_16(b, pixels, idx));
    umbra_val32 const inv255 = umbra_imm_f32(b, 1.0f / 255.0f);
    umbra_val32 const cov    = umbra_mul_f32(b, umbra_f32_from_i32(b, val), inv255);

    return umbra_sel_32(b, in, cov, zero_f);
}

// Matrix combinator: apply self->mat to (x, y) and forward to self->inner_fn.
umbra_val32 coverage_matrix(void *ctx, struct umbra_builder *b,
                            umbra_val32 x, umbra_val32 y) {
    struct coverage_matrix const *self = ctx;
    umbra_ptr32 const u = umbra_bind_uniforms32(b, &self->mat, sizeof self->mat / 4);

    enum {
        M_SX = (int)(__builtin_offsetof(struct umbra_matrix, sx) / 4),
        M_KX = (int)(__builtin_offsetof(struct umbra_matrix, kx) / 4),
        M_TX = (int)(__builtin_offsetof(struct umbra_matrix, tx) / 4),
        M_KY = (int)(__builtin_offsetof(struct umbra_matrix, ky) / 4),
        M_SY = (int)(__builtin_offsetof(struct umbra_matrix, sy) / 4),
        M_TY = (int)(__builtin_offsetof(struct umbra_matrix, ty) / 4),
        M_P0 = (int)(__builtin_offsetof(struct umbra_matrix, p0) / 4),
        M_P1 = (int)(__builtin_offsetof(struct umbra_matrix, p1) / 4),
        M_P2 = (int)(__builtin_offsetof(struct umbra_matrix, p2) / 4),
    };

    umbra_val32 const sx = umbra_uniform_32(b, u, M_SX),
                      kx = umbra_uniform_32(b, u, M_KX),
                      tx = umbra_uniform_32(b, u, M_TX),
                      ky = umbra_uniform_32(b, u, M_KY),
                      sy = umbra_uniform_32(b, u, M_SY),
                      ty = umbra_uniform_32(b, u, M_TY),
                      p0 = umbra_uniform_32(b, u, M_P0),
                      p1 = umbra_uniform_32(b, u, M_P1),
                      p2 = umbra_uniform_32(b, u, M_P2);

    umbra_val32 const w = umbra_add_f32(b,
                                        umbra_add_f32(b, umbra_mul_f32(b, p0, x),
                                                         umbra_mul_f32(b, p1, y)),
                                        p2);
    umbra_val32 const xp =
        umbra_div_f32(b,
                      umbra_add_f32(b,
                                    umbra_add_f32(b, umbra_mul_f32(b, sx, x),
                                                     umbra_mul_f32(b, kx, y)),
                                    tx),
                      w);
    umbra_val32 const yp =
        umbra_div_f32(b,
                      umbra_add_f32(b,
                                    umbra_add_f32(b, umbra_mul_f32(b, ky, x),
                                                     umbra_mul_f32(b, sy, y)),
                                    ty),
                      w);

    return self->inner_fn(self->inner_ctx, b, xp, yp);
}

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

    umbra_color       color;
    struct umbra_buf  buf;
    umbra_coverage    *coverage_fn;

    struct umbra_fmt      fmt;
    struct umbra_program *prog;
    struct umbra_buf      dst_buf;
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
    st->buf = (struct umbra_buf){
        .ptr    = st->tc->data,
        .count  = st->w * st->h,
        .stride = st->w,
    };
    st->fmt = fmt;
    struct umbra_builder *b = umbra_draw_builder(
        st->coverage_fn,    &st->buf,
        umbra_shader_color, &st->color,
        umbra_blend_srcover, NULL,
        &st->dst_buf,        fmt);
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
    st->dst_buf = (struct umbra_buf){
        .ptr=buf, .count=st->w * st->h * st->fmt.planes, .stride=st->w,
    };
    st->prog->queue(st->prog, l, t, r, b);
}

static int text_get_builders(struct slide *s, struct umbra_fmt fmt,
                             struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct text_slide *st = (struct text_slide *)s;
    out[0] = umbra_draw_builder(
        st->coverage_fn,    &st->buf,
        umbra_shader_color, &st->color,
        umbra_blend_srcover, NULL,
        &st->dst_buf,        fmt);
    return out[0] ? 1 : 0;
}

static void text_free(struct slide *s) {
    struct text_slide *st = (struct text_slide *)s;
    umbra_program_free(st->prog);
    free(st);
}

SLIDE(slide_coverage_bitmap) {
    struct text_slide *st = calloc(1, sizeof *st);
    st->tc          = text_shared_bitmap();
    st->color       = (umbra_color){1.0f, 1.0f, 1.0f, 1.0f};
    st->coverage_fn = coverage_bitmap;
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
    st->tc          = text_shared_sdf();
    st->color       = (umbra_color){0.2f, 0.8f, 1.0f, 1.0f};
    st->coverage_fn = coverage_sdf;
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

// Coverage (8-bit bitmap + matrix): sample the glyph-mask through a
// coverage_matrix combinator wrapping coverage_bitmap2d.
struct persp_slide {
    struct slide base;

    struct text_cov *bitmap;
    int              w, h;

    umbra_color               color;
    struct coverage_bitmap2d  bmp;
    struct coverage_matrix    mat;

    struct umbra_fmt          fmt;
    struct umbra_program     *prog;
    struct umbra_buf          dst_buf;
};

static void persp_init(struct slide *s, int w, int h) {
    struct persp_slide *st = (struct persp_slide *)s;
    st->w = w;
    st->h = h;
    st->bmp = (struct coverage_bitmap2d){
        .buf = {.ptr = st->bitmap->data, .count = st->bitmap->w * st->bitmap->h},
        .w   = st->bitmap->w,
        .h   = st->bitmap->h,
    };
    st->mat = (struct coverage_matrix){
        .inner_fn  = coverage_bitmap2d,
        .inner_ctx = &st->bmp,
    };
}

static void persp_prepare(struct slide *s, struct umbra_backend *be,
                          struct umbra_fmt fmt) {
    struct persp_slide *st = (struct persp_slide *)s;
    umbra_program_free(st->prog);
    st->fmt = fmt;
    struct umbra_builder *b = umbra_draw_builder(
        coverage_matrix,     &st->mat,
        umbra_shader_color,  &st->color,
        umbra_blend_srcover, NULL,
        &st->dst_buf,        fmt);
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    st->prog = be->compile(be, ir);
    umbra_flat_ir_free(ir);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void persp_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct persp_slide *st = (struct persp_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    slide_perspective_matrix(&st->mat.mat, (float)secs, st->w, st->h,
                             st->bitmap->w, st->bitmap->h);
    st->dst_buf = (struct umbra_buf){
        .ptr=buf, .count=st->w * st->h * st->fmt.planes, .stride=st->w,
    };
    st->prog->queue(st->prog, l, t, r, b);
}

static int persp_get_builders(struct slide *s, struct umbra_fmt fmt,
                              struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct persp_slide *st = (struct persp_slide *)s;
    out[0] = umbra_draw_builder(
        coverage_matrix,     &st->mat,
        umbra_shader_color,  &st->color,
        umbra_blend_srcover, NULL,
        &st->dst_buf,        fmt);
    return out[0] ? 1 : 0;
}

static void persp_free(struct slide *s) {
    struct persp_slide *st = (struct persp_slide *)s;
    umbra_program_free(st->prog);
    free(st);
}

SLIDE(slide_coverage_bitmap_matrix) {
    struct persp_slide *st = calloc(1, sizeof *st);
    st->bitmap = text_shared_bitmap();
    st->color  = (umbra_color){1.0f, 0.8f, 0.2f, 1.0f};
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

    umbra_color           color;
    struct umbra_fmt      fmt;
    struct umbra_flat_ir *ir;
    struct umbra_program *prog;
    struct umbra_buf      dst_buf;
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
        struct umbra_builder *b = umbra_draw_builder(
            NULL,                NULL,
            umbra_shader_color,  &st->color,
            umbra_blend_srcover, NULL,
            &st->dst_buf,        fmt);
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
    st->dst_buf = (struct umbra_buf){
        .ptr=buf, .count=st->w * st->h * st->fmt.planes, .stride=st->w,
    };
    st->prog->queue(st->prog, l, t, r, b);
}

static int cov_null_get_builders(struct slide *s, struct umbra_fmt fmt,
                                 struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct cov_null_slide *st = (struct cov_null_slide *)s;
    out[0] = umbra_draw_builder(
        NULL,                NULL,
        umbra_shader_color,  &st->color,
        umbra_blend_srcover, NULL,
        &st->dst_buf,        fmt);
    return out[0] ? 1 : 0;
}

static void cov_null_free(struct slide *s) {
    struct cov_null_slide *st = (struct cov_null_slide *)s;
    umbra_program_free(st->prog);
    umbra_flat_ir_free(st->ir);
    free(st);
}

SLIDE(slide_coverage_null) {
    struct cov_null_slide *st = calloc(1, sizeof *st);
    st->color = (umbra_color){0.15f, 0.0f, 0.3f, 0.3f};
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
