#include "slide.h"
#include <stdlib.h>

enum { SWATCH_COLS = 5, SWATCH_ROWS = 2, SWATCH_N = SWATCH_COLS * SWATCH_ROWS };

static umbra_color const swatch_colors[SWATCH_N] = {
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

struct swatch_slide {
    struct slide base;

    int w, h;

    struct umbra_fmt      fmt;
    struct umbra_program *progs[SWATCH_N];
    struct umbra_buf      dst_buf;

    umbra_rect            rects [SWATCH_N];
    umbra_color           colors[SWATCH_N];
};

static void swatch_init(struct slide *s, int w, int h) {
    struct swatch_slide *st = (struct swatch_slide *)s;
    st->w = w;
    st->h = h;
    int const cw = w / SWATCH_COLS,
              ch = h / SWATCH_ROWS;
    for (int i = 0; i < SWATCH_N; i++) {
        int const col = i % SWATCH_COLS,
                  row = i / SWATCH_COLS;
        int const x0 = col * cw,
                  y0 = row * ch,
                  x1 = (col + 1 == SWATCH_COLS) ? w : x0 + cw,
                  y1 = (row + 1 == SWATCH_ROWS) ? h : y0 + ch;
        st->rects [i] = (umbra_rect){(float)x0, (float)y0, (float)x1, (float)y1};
        st->colors[i] = swatch_colors[i];
    }
}

static _Bool swatch_build_draw(struct slide *s, int i, struct umbra_builder *b,
                               umbra_ptr dst_ptr, struct umbra_fmt fmt,
                               umbra_val32 x, umbra_val32 y) {
    if (i < 0 || i >= SWATCH_N) { return 0; }
    struct swatch_slide *st = (struct swatch_slide *)s;
    umbra_build_draw(b, dst_ptr, fmt, x, y,
                     umbra_coverage_rect, &st->rects[i],
                     umbra_shader_color,  &st->colors[i],
                     NULL,                NULL);
    return 1;
}

static struct umbra_builder* swatch_builder(struct slide *s, int i, struct umbra_fmt fmt) {
    struct swatch_slide *st = (struct swatch_slide *)s;
    struct umbra_builder *b = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(b, &st->dst_buf);
    umbra_val32 const x = umbra_f32_from_i32(b, umbra_x(b)),
                      y = umbra_f32_from_i32(b, umbra_y(b));
    if (swatch_build_draw(s, i, b, dst_ptr, fmt, x, y)) {
        return b;
    }
    umbra_builder_free(b);
    return NULL;
}

static void swatch_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct swatch_slide *st = (struct swatch_slide *)s;
    st->fmt = fmt;
    for (int i = 0; i < SWATCH_N; i++) {
        umbra_program_free(st->progs[i]);
        struct umbra_builder *b = swatch_builder(s, i, fmt);
        struct umbra_flat_ir *ir = umbra_flat_ir(b);
        umbra_builder_free(b);
        st->progs[i] = be->compile(be, ir);
        umbra_flat_ir_free(ir);
    }
}

static void swatch_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct swatch_slide *st = (struct swatch_slide *)s;
    (void)secs;
    st->dst_buf = (struct umbra_buf){
        .ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w,
    };
    for (int i = 0; i < SWATCH_N; i++) {
        st->progs[i]->queue(st->progs[i], l, t, r, b);
    }
}

static int swatch_get_builders(struct slide *s, struct umbra_fmt fmt,
                               struct umbra_builder **out, int max) {
    int n = 0;
    for (int i = 0; i < SWATCH_N && n < max; i++) {
        struct umbra_builder *b = swatch_builder(s, i, fmt);
        if (!b) { break; }
        out[n++] = b;
    }
    return n;
}

static void swatch_free(struct slide *s) {
    struct swatch_slide *st = (struct swatch_slide *)s;
    for (int i = 0; i < SWATCH_N; i++) { umbra_program_free(st->progs[i]); }
    free(st);
}

SLIDE(slide_swatch) {
    struct swatch_slide *st = calloc(1, sizeof *st);
    st->base = (struct slide){
        .title        = "Color Swatches",
        .bg           = {0, 0, 0, 1},
        .init         = swatch_init,
        .prepare      = swatch_prepare,
        .draw         = swatch_draw,
        .free         = swatch_free,
        .get_builders = swatch_get_builders,
        .build_draw   = swatch_build_draw,
    };
    return &st->base;
}
