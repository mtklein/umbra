#include "slide.h"
#include <stdlib.h>

enum { SWATCH_COLS = 5, SWATCH_ROWS = 2, SWATCH_N = SWATCH_COLS * SWATCH_ROWS };

static umbra_color swatch_colors[SWATCH_N] = {
    {1.0f, 0.0f, 0.0f, 1.0f},
    {0.0f, 1.0f, 0.0f, 1.0f},
    {0.0f, 0.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f},
    {0.5f, 0.5f, 0.5f, 1.0f},
    {1.2f, 0.0f, 0.0f, 1.0f},
    {0.0f, 1.2f, 0.0f, 1.0f},
    {0.0f, 0.0f, 1.2f, 1.0f},
    {1.0f, 1.0f, 0.0f, 1.0f},
    {1.0f, 0.0f, 1.0f, 1.0f},
};

struct swatch_ctx {
    struct umbra_buf const *colors;
    float                   cw, ch;
};

static umbra_color_val32 shader_swatch(void *vctx, struct umbra_builder *b,
                                        umbra_val32 x, umbra_val32 y) {
    struct swatch_ctx const *ctx = vctx;
    umbra_ptr const colors = umbra_bind_buf     (b, ctx->colors);
    umbra_ptr const u      = umbra_bind_uniforms(b, &ctx->cw, 2);

    umbra_val32 const cw = umbra_uniform_32(b, u, 0),
                      ch = umbra_uniform_32(b, u, 1);

    umbra_val32 const max_col = umbra_imm_f32(b, (float)(SWATCH_COLS - 1)),
                      max_row = umbra_imm_f32(b, (float)(SWATCH_ROWS - 1));
    umbra_val32 const col_f = umbra_min_f32(b,
        umbra_floor_f32(b, umbra_div_f32(b, x, cw)), max_col);
    umbra_val32 const row_f = umbra_min_f32(b,
        umbra_floor_f32(b, umbra_div_f32(b, y, ch)), max_row);

    umbra_val32 const col_i = umbra_i32_from_f32(b, col_f),
                      row_i = umbra_i32_from_f32(b, row_f);

    umbra_val32 const base = umbra_mul_i32(b,
        umbra_add_i32(b, umbra_mul_i32(b, row_i, umbra_imm_i32(b, SWATCH_COLS)), col_i),
        umbra_imm_i32(b, 4));

    return (umbra_color_val32){
        umbra_gather_32(b, colors, base),
        umbra_gather_32(b, colors, umbra_add_i32(b, base, umbra_imm_i32(b, 1))),
        umbra_gather_32(b, colors, umbra_add_i32(b, base, umbra_imm_i32(b, 2))),
        umbra_gather_32(b, colors, umbra_add_i32(b, base, umbra_imm_i32(b, 3))),
    };
}

struct swatch_slide {
    struct slide base;

    int w, h;

    struct umbra_buf      colors_buf;
    struct swatch_ctx     ctx;

    struct umbra_buf      dst_buf;
};

static void swatch_init(struct slide *s, int w, int h) {
    struct swatch_slide *st = (struct swatch_slide *)s;
    st->w = w;
    st->h = h;
    st->colors_buf = (struct umbra_buf){
        .ptr   = swatch_colors,
        .count = SWATCH_N * 4,
    };
    st->ctx = (struct swatch_ctx){
        .colors = &st->colors_buf,
        .cw     = (float)(w / SWATCH_COLS),
        .ch     = (float)(h / SWATCH_ROWS),
    };
}

static void swatch_build_draw(struct slide *s, struct umbra_builder *b,
                              umbra_ptr dst_ptr, struct umbra_fmt fmt,
                              umbra_val32 x, umbra_val32 y) {
    struct swatch_slide *st = (struct swatch_slide *)s;
    umbra_build_draw(b, dst_ptr, fmt, x, y,
                     NULL,           NULL,
                     shader_swatch,  &st->ctx,
                     NULL,           NULL);
}

static struct umbra_builder* swatch_builder(struct slide *s, struct umbra_fmt fmt) {
    struct swatch_slide *st = (struct swatch_slide *)s;
    struct umbra_builder *b = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(b, &st->dst_buf);
    umbra_val32 const x = umbra_f32_from_i32(b, umbra_x(b)),
                      y = umbra_f32_from_i32(b, umbra_y(b));
    swatch_build_draw(s, b, dst_ptr, fmt, x, y);
    return b;
}

static int swatch_get_builders(struct slide *s, struct umbra_fmt fmt,
                               struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    out[0] = swatch_builder(s, fmt);
    return 1;
}

static void swatch_free(struct slide *s) { free(s); }

SLIDE(slide_swatch) {
    struct swatch_slide *st = calloc(1, sizeof *st);
    st->base = (struct slide){
        .title        = "Color Swatches",
        .bg           = {0, 0, 0, 1},
        .init         = swatch_init,
        .free         = swatch_free,
        .get_builders = swatch_get_builders,
        .build_draw   = swatch_build_draw,
    };
    return &st->base;
}
