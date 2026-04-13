#include "slide.h"
#include <stdlib.h>

struct swatch_state {
    struct slide base;

    int w, h;

    struct umbra_shader_solid  shader;
    struct umbra_fmt           fmt;
    struct umbra_draw_layout   lay;
    struct umbra_basic_block  *bb;
    struct umbra_program      *prog;
};

static void swatch_init(struct slide *s, int w, int h) {
    struct swatch_state *st = (struct swatch_state *)s;
    st->w = w;
    st->h = h;
}

static void swatch_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct swatch_state *st = (struct swatch_state *)s;
    if (st->fmt.name != fmt.name || !st->bb) {
        st->fmt = fmt;
        umbra_basic_block_free(st->bb);
        free(st->lay.uniforms);
        struct umbra_builder *b = umbra_draw_build(&st->shader.base, NULL, NULL, fmt,
                                                    &st->lay);
        st->bb = umbra_basic_block(b);
        umbra_builder_free(b);
    }
    if (st->prog) { st->prog->free(st->prog); }
    st->prog = be->compile(be, st->bb);
}

static void swatch_draw(struct slide *s, int frame, int l, int t, int r, int b, void *buf) {
    struct swatch_state *st = (struct swatch_state *)s;
    (void)frame;

    static float const swatches[][4] = {
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
    enum { N = (int)(sizeof swatches / sizeof *swatches), COLS = 5, ROWS = 2 };
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

        __builtin_memcpy(st->shader.color, swatches[i], sizeof st->shader.color);
        umbra_draw_fill(&st->lay, &st->shader.base, NULL);
        struct umbra_buf ubuf[] = {
            {.ptr = st->lay.uniforms, .count = (int)(st->lay.uni.size / 4)},
            {.ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w},
        };
        st->prog->queue(st->prog, xl, yt, xr, yb, ubuf);
    }
}

static void swatch_free(struct slide *s) {
    struct swatch_state *st = (struct swatch_state *)s;
    if (st->prog) { st->prog->free(st->prog); }
    umbra_basic_block_free(st->bb);
    free(st->lay.uniforms);
    free(st);
}

SLIDE(slide_swatch) {
    struct swatch_state *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid((float[]){0, 0, 0, 1});
    st->base = (struct slide){
        .title   = "Color Swatches",
        .bg      = {0, 0, 0, 1},
        .init    = swatch_init,
        .prepare = swatch_prepare,
        .draw    = swatch_draw,
        .free    = swatch_free,
    };
    return &st->base;
}
