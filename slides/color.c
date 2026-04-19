#include "slide.h"
#include "../src/count.h"
#include <stdlib.h>

struct swatch_slide {
    struct slide base;

    int w, h;

    umbra_color                color;
    struct umbra_fmt           fmt;
    struct umbra_flat_ir      *ir;
    struct umbra_program      *prog;
    struct umbra_buf           dst_buf;
};

static void swatch_init(struct slide *s, int w, int h) {
    struct swatch_slide *st = (struct swatch_slide *)s;
    st->w = w;
    st->h = h;
}

static void swatch_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct swatch_slide *st = (struct swatch_slide *)s;
    if (st->fmt.name != fmt.name || !st->ir) {
        st->fmt = fmt;
        umbra_flat_ir_free(st->ir);
        struct umbra_builder *b = umbra_draw_builder(
        NULL,            NULL,               NULL,
            umbra_shader_color, &st->color,
            NULL,               NULL,
            &st->dst_buf,       fmt);
        st->ir = umbra_flat_ir(b);
        umbra_builder_free(b);
    }
    umbra_program_free(st->prog);
    st->prog = be->compile(be, st->ir);
}

static void swatch_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct swatch_slide *st = (struct swatch_slide *)s;
    (void)secs;

    static umbra_color const swatches[] = {
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
    enum { N = count(swatches), COLS = 5, ROWS = 2 };
    int const cw = st->w / COLS,
              ch = st->h / ROWS;

    for (int i = 0; i < N; i++) {
        int const col = i % COLS,
                  row = i / COLS;
        int const x0 = col * cw,
                  y0 = row * ch;
        int const x1 = (col + 1 == COLS) ? st->w : x0 + cw;
        int const y1 = (row + 1 == ROWS) ? st->h : y0 + ch;
        if (y1 <= t || y0 >= b) { continue; }
        int const yt = y0 > t ? y0 : t;
        int const yb = y1 < b ? y1 : b;
        int const xl = x0 > l ? x0 : l;
        int const xr = x1 < r ? x1 : r;
        if (xr <= xl) { continue; }

        st->color = swatches[i];
        st->dst_buf = (struct umbra_buf){
            .ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w,
        };
        st->prog->queue(st->prog, xl, yt, xr, yb);
    }
}

static int swatch_get_builders(struct slide *s, struct umbra_fmt fmt,
                               struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct swatch_slide *st = (struct swatch_slide *)s;
    out[0] = umbra_draw_builder(
        NULL,        NULL,               NULL,
        umbra_shader_color, &st->color,
        NULL,               NULL,
        &st->dst_buf,       fmt);
    return out[0] ? 1 : 0;
}

static void swatch_free(struct slide *s) {
    struct swatch_slide *st = (struct swatch_slide *)s;
    umbra_program_free(st->prog);
    umbra_flat_ir_free(st->ir);
    free(st);
}

SLIDE(slide_swatch) {
    struct swatch_slide *st = calloc(1, sizeof *st);
    st->color = (umbra_color){0, 0, 0, 1};
    st->base = (struct slide){
        .title   = "Color Swatches",
        .bg      = {0, 0, 0, 1},
        .init    = swatch_init,
        .prepare = swatch_prepare,
        .draw    = swatch_draw,
        .free = swatch_free,
        .get_builders = swatch_get_builders,
    };
    return &st->base;
}
