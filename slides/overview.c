#include "slide.h"
#include <stdlib.h>

static uint8_t const font3x5[10][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
    {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
};

struct overview_slide {
    struct slide base;

    int                   w, h;
    uint32_t             *fb, *tmp;
    struct umbra_backend *be;
    struct umbra_fmt       fmt, out_fmt;
    struct umbra_program *cvt;
};

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

static void render_thumbnails(struct overview_slide *st) {
    int const w      = st->w,
              h      = st->h,
              n_real = slide_count() - 1;
    if (n_real <= 0) { return; }
    int cols = 1;
    while (cols * cols < n_real) { cols++; }
    int const rows = (n_real + cols - 1) / cols,
              cw   = w / cols,
              ch   = h / rows;
    for (int idx = 0; idx < rows * cols; idx++) {
        int col = idx % cols;
        int row = idx / cols;
        int x0 = col * cw;
        int y0 = row * ch;

        if (idx >= n_real) {
            for (int y = 0; y < ch; y++) {
                for (int x = 0; x < cw; x++) {
                    st->fb[(y0 + y) * w + x0 + x] = 0xff181818;
                }
            }
            draw_xbox(st->fb, w, x0, y0, cw, ch, 0xff404040);
            continue;
        }

        struct slide *sub = slide_get(idx);
        if (sub->prepare) { sub->prepare(sub, st->be, st->fmt); }
        sub->draw(sub, 0.0, 0, 0, w, h, st->tmp);
        st->be->flush(st->be);

        for (int cy = 0; cy < ch; cy++) {
            for (int cx = 0; cx < cw; cx++) {
                int sx = cx * w / cw;
                int sy = cy * h / ch;
                st->fb[(y0 + cy) * w + x0 + cx] = st->tmp[sy * w + sx];
            }
        }

        draw_number(st->fb, w, x0 + 3, y0 + 3, idx + 1, 0x80000000);
        draw_number(st->fb, w, x0 + 2, y0 + 2, idx + 1, 0xffffffff);
    }
}

static void overview_init(struct slide *s, int w, int h) {
    struct overview_slide *st = (struct overview_slide *)s;
    st->w = w;
    st->h = h;
    st->fb = calloc((size_t)(w * h), 4);
    st->tmp = calloc((size_t)(w * h), 4);
}

static void overview_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct overview_slide *st = (struct overview_slide *)s;
    st->be      = be;
    st->fmt     = umbra_fmt_8888;
    st->out_fmt = fmt;
    render_thumbnails(st);

    umbra_program_free(st->cvt);
    struct umbra_builder *b = umbra_builder();
    umbra_ptr32 src_ptr = {.ix = 0};
    umbra_ptr32 dst_ptr = {.ix = 1};
    umbra_color_val32 c = umbra_fmt_8888.load(b, &src_ptr);
    fmt.store(b, &dst_ptr, c);
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    st->cvt = be->compile(be, ir);
    umbra_flat_ir_free(ir);
}

static void overview_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct overview_slide *st = (struct overview_slide *)s;
    (void)secs; (void)l; (void)r;
    int const w = st->w,
              h = st->h;
    st->cvt->queue(st->cvt, 0, t, w, b, (struct umbra_buf[]){
        {.ptr = st->fb, .count = w * h,                       .stride = w},
        {.ptr = buf,    .count = w * h * st->out_fmt.planes,  .stride = w},
    });
}

static int overview_get_builders(struct slide *s, struct umbra_fmt fmt,
                                 struct umbra_builder **out, int max) {
    (void)s;
    if (max < 1) { return 0; }
    struct umbra_builder *b = umbra_builder();
    umbra_ptr32 src_ptr = {.ix = 0};
    umbra_ptr32 dst_ptr = {.ix = 1};
    umbra_color_val32 c = umbra_fmt_8888.load(b, &src_ptr);
    fmt.store(b, &dst_ptr, c);
    out[0] = b;
    return 1;
}

static void overview_free(struct slide *s) {
    struct overview_slide *st = (struct overview_slide *)s;
    umbra_program_free(st->cvt);
    free(st->fb);
    free(st->tmp);
    free(st);
}

struct slide* make_overview(void);

struct slide* make_overview(void) {
    struct overview_slide *st = calloc(1, sizeof *st);
    st->fmt = umbra_fmt_8888;
    st->base = (struct slide){
        .title = "Overview",
        .bg = {0.06f, 0.06f, 0.06f, 1},
        .init = overview_init,
        .prepare = overview_prepare,
        .draw = overview_draw,
        .free = overview_free,
        .get_builders = overview_get_builders,
    };
    return &st->base;
}
