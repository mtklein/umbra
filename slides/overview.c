#include "slide.h"
#include <stdlib.h>

enum { COLS = 4, ROWS = 4 };

static uint8_t const font3x5[10][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
    {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
};

typedef struct {
    int                       w, h, cw, ch;
    int                       n_real, pad_;
    uint32_t                 *fb, *tmp;
    struct umbra_backend     *be;
    struct umbra_program     *progs[ROWS * COLS];
    struct umbra_basic_block *bbs[ROWS * COLS];
    umbra_draw_layout         lays[ROWS * COLS];
} overview_state;

static void draw_digit(uint32_t *fb, int stride, int ox, int oy, int digit, uint32_t color) {
    for (int dy = 0; dy < 10; dy++) {
        uint8_t bits = font3x5[digit][dy / 2];
        for (int dx = 0; dx < 6; dx++) {
            if (bits & (4 >> (dx / 2))) { fb[(oy + dy) * stride + ox + dx] = color; }
        }
    }
}

static void draw_number(uint32_t *fb, int stride, int ox, int oy, int num, uint32_t color) {
    if (num >= 10) {
        draw_digit(fb, stride, ox, oy, num / 10, color);
        ox += 7;
    }
    draw_digit(fb, stride, ox, oy, num % 10, color);
}

static void draw_xbox(uint32_t *fb, int stride, int x0, int y0, int cw, int ch,
                      uint32_t color) {
    for (int x = 0; x < cw; x++) {
        fb[y0 * stride + x0 + x] = color;
        fb[(y0 + ch - 1) * stride + x0 + x] = color;
    }
    for (int y = 0; y < ch; y++) {
        fb[(y0 + y) * stride + x0] = color;
        fb[(y0 + y) * stride + x0 + cw - 1] = color;
        int xd = y * (cw - 1) / (ch - 1);
        fb[(y0 + y) * stride + x0 + xd] = color;
        fb[(y0 + y) * stride + x0 + cw - 1 - xd] = color;
    }
}

static void render_thumbnails(overview_state *st) {
    int w = st->w, h = st->h;
    for (int idx = 0; idx < ROWS * COLS; idx++) {
        int col = idx % COLS;
        int row = idx / COLS;
        int x0 = col * st->cw;
        int y0 = row * st->ch;

        if (idx >= st->n_real) {
            for (int y = 0; y < st->ch; y++) {
                for (int x = 0; x < st->cw; x++) {
                    st->fb[(y0 + y) * w + x0 + x] = 0xff181818;
                }
            }
            draw_xbox(st->fb, w, x0, y0, st->cw, st->ch, 0xff404040);
            continue;
        }

        slide *sub = slide_get(idx);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) { st->tmp[y * w + x] = sub->bg; }
        }
        if (sub->prepare) { sub->prepare(sub, w, h, st->be); }
        sub->draw(sub, w, h, 0, h, st->tmp, &st->lays[idx],
                  st->progs[idx]);
        umbra_backend_flush(st->be);

        for (int cy = 0; cy < st->ch; cy++) {
            for (int cx = 0; cx < st->cw; cx++) {
                int sx = cx * w / st->cw;
                int sy = cy * h / st->ch;
                st->fb[(y0 + cy) * w + x0 + cx] = st->tmp[sy * w + sx];
            }
        }

        draw_number(st->fb, w, x0 + 3, y0 + 3, idx + 1, 0x80000000);
        draw_number(st->fb, w, x0 + 2, y0 + 2, idx + 1, 0xffffffff);
    }
}

static void recompile(overview_state *st, struct umbra_backend *be) {
    if (be == st->be) { return; }
    for (int i = 0; i < st->n_real; i++) { umbra_program_free(st->progs[i]); }
    st->be = be;
    for (int i = 0; i < st->n_real; i++) {
        st->progs[i] = umbra_backend_compile(be, st->bbs[i]);
    }
}

static void overview_init(slide *s, int w, int h) {
    overview_state *st = s->state;
    st->w = w;
    st->h = h;
    st->cw = w / COLS;
    st->ch = h / ROWS;
    st->fb = calloc((size_t)(w * h), 4);
    st->tmp = calloc((size_t)(w * h), 4);
    st->n_real = slide_count() - 1;

    for (int idx = 0; idx < st->n_real; idx++) {
        slide                *sub = slide_get(idx);
        struct umbra_builder *b = umbra_draw_build(sub->shader, sub->coverage, sub->blend,
                                                   sub->format, &st->lays[idx]);
        st->bbs[idx] = umbra_basic_block(b);
        umbra_builder_free(b);
    }
}

static void overview_animate(slide *s, float dt) {
    overview_state *st = s->state;
    for (int i = 0; i < st->n_real; i++) {
        slide *sub = slide_get(i);
        if (sub->animate) { sub->animate(sub, dt); }
    }
}

static void overview_prepare(slide *s, int w, int h, struct umbra_backend *be) {
    (void)w;
    (void)h;
    overview_state *st = s->state;
    recompile(st, be);
    render_thumbnails(st);
}

static void overview_draw(slide *s, int w, int h, int y0, int y1, void *buf,
                             umbra_draw_layout const *lay, struct umbra_program *program) {
    overview_state *st = s->state;
    (void)w;
    (void)h;
    (void)lay;
    (void)program;
    size_t off = (size_t)y0 * (size_t)st->w * 4;
    size_t len = (size_t)(y1 - y0) * (size_t)st->w * 4;
    __builtin_memcpy((char*)buf + off, (char*)st->fb + off, len);
}

static void overview_cleanup(slide *s) {
    overview_state *st = s->state;
    for (int i = 0; i < st->n_real; i++) { umbra_program_free(st->progs[i]); }
    for (int i = 0; i < st->n_real; i++) { umbra_basic_block_free(st->bbs[i]); }
    free(st->fb);
    free(st->tmp);
    free(st);
    s->state = NULL;
}

slide slide_overview(void);

slide slide_overview(void) {
    overview_state *st = calloc(1, sizeof *st);
    return (slide){
        .title = "Overview",
        .shader = umbra_shader_solid,
        .format = umbra_format_8888,
        .bg = 0xff101010,
        .init = overview_init,
        .animate = overview_animate,
        .prepare = overview_prepare,
        .draw = overview_draw,
        .cleanup = overview_cleanup,
        .state = st,
    };
}
