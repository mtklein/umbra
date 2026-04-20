#include "slide.h"
#include "coverage.h"
#include <stdlib.h>

static uint8_t const font3x5[10][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
    {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
};

// One cell: either a live-composed per-cell slide_runtime (if sub exposes
// build_draw or build_sdf_draw) or a static X-box placeholder.  The
// runtime's programs stack the cell_mat (cell -> canvas) with whatever
// transform the sub-slide's effects declare; each transform runs as its
// own umbra_transform_perspective pass in the IR, so matrices compose
// through the built-in uniforms rebind each dispatch rather than through
// any CPU-side composition.
struct cell {
    struct slide         *sub;          // NULL = placeholder
    int                   col, row;
    struct slide_runtime *rt;
    struct umbra_matrix   cell_mat;     // cell -> canvas, fixed at prepare
    int                   :32;
};

struct overview_slide {
    struct slide base;

    struct umbra_buf      out_buf;
    struct slide_bg      *bg;

    struct cell          *cells;
    int                   n_cells, rows, cols, cw, ch;
    int                   :32;

    // Overlay: a uint16_t coverage mask of slide numbers + empty-slot X-boxes,
    // rasterized CPU-side in prepare and sampled through a white-on-srcover
    // draw program queued after the cell programs each frame.
    uint16_t             *overlay;
    struct umbra_buf      overlay_buf;
    umbra_color           overlay_color;
    struct umbra_program *overlay_prog;
};

static void plot(uint16_t *mask, int stride, int x, int y) {
    mask[y * stride + x] = 0xff;
}

static void draw_digit(uint16_t *mask, int stride, int ox, int oy, int digit) {
    for (int dy = 0; dy < 10; dy++) {
        uint8_t bits = font3x5[digit][dy / 2];
        for (int dx = 0; dx < 6; dx++) {
            if (bits & (4 >> (dx / 2))) { plot(mask, stride, ox + dx, oy + dy); }
        }
    }
}

static void draw_number(uint16_t *mask, int stride, int ox, int oy, int num) {
    if (num >= 10) {
        draw_digit(mask, stride, ox, oy, num / 10);
        ox += 7;
    }
    draw_digit(mask, stride, ox, oy, num % 10);
}

static void draw_xbox(uint16_t *mask, int stride, int x0, int y0, int cw, int ch) {
    for (int x = 0; x < cw; x++) {
        plot(mask, stride, x0 + x, y0);
        plot(mask, stride, x0 + x, y0 + ch - 1);
    }
    for (int y = 0; y < ch; y++) {
        plot(mask, stride, x0,          y0 + y);
        plot(mask, stride, x0 + cw - 1, y0 + y);
        int xd = y * (cw - 1) / (ch - 1);
        plot(mask, stride, x0 + xd,          y0 + y);
        plot(mask, stride, x0 + cw - 1 - xd, y0 + y);
    }
}

// Cell -> canvas: maps a pixel at (cell_x, cell_y) landing in the cell's sub-rect
// of the overview framebuffer to the sub-slide's full-canvas coordinates.
//   cell_x in [col*cw, col*cw + cw]  ->  canvas_x in [0, w]
//   cell_y in [row*ch, row*ch + ch]  ->  canvas_y in [0, h]
static void compute_cell_matrix(struct umbra_matrix *out, int col, int row,
                                 int cw, int ch, int w, int h) {
    float const sx = (float)w / (float)cw,
                sy = (float)h / (float)ch;
    *out = (struct umbra_matrix){
        .sx = sx, .kx = 0, .tx = -(float)col * (float)w,
        .ky = 0,  .sy = sy, .ty = -(float)row * (float)h,
        .p0 = 0,  .p1 = 0,  .p2 = 1,
    };
}

static void overview_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct overview_slide *st = (struct overview_slide *)s;

    slide_bg_free(st->bg);
    st->bg = slide_bg(be, fmt);

    if (!st->overlay) {
        st->overlay = calloc((size_t)(s->w * s->h), sizeof *st->overlay);
    } else {
        for (int i = 0; i < s->w * s->h; i++) { st->overlay[i] = 0; }
    }
    st->overlay_buf = (struct umbra_buf){
        .ptr    = st->overlay,
        .count  = s->w * s->h,
        .stride = s->w,
    };
    st->overlay_color = (umbra_color){1, 1, 1, 1};

    int const n_real = slide_count() - 1;
    if (n_real <= 0) { return; }

    int cols = 1;
    while (cols * cols < n_real) {
        cols++;
    }
    int const rows = cols,
              cw   = s->w / cols,
              ch   = s->h / rows;
    st->cols = cols;
    st->rows = rows;
    st->cw   = cw;
    st->ch   = ch;

    if (st->cells) {
        for (int i = 0; i < st->n_cells; i++) {
            slide_runtime_free(st->cells[i].rt);
        }
        free(st->cells);
    }
    int const total = rows * cols;
    st->cells = calloc((size_t)total, sizeof *st->cells);
    st->n_cells = total;

    for (int idx = 0; idx < total; idx++) {
        struct cell *c = &st->cells[idx];
        c->col = idx % cols;
        c->row = idx / cols;
        compute_cell_matrix(&c->cell_mat, c->col, c->row, cw, ch, s->w, s->h);

        int const x0 = c->col * cw,
                  y0 = c->row * ch;
        if (idx < n_real) {
            draw_number(st->overlay, s->w, x0 + 2, y0 + 2, idx + 1);
        } else {
            draw_xbox(st->overlay, s->w, x0, y0, cw, ch);
        }

        if (idx >= n_real) { continue; }  // empty slot (no slide)

        struct slide *sub = slide_get(idx);
        c->sub = sub;
        if (sub->prepare) { sub->prepare(sub, be, fmt); }

        c->rt = slide_runtime(sub, s->w, s->h, be, fmt, &c->cell_mat);
    }

    umbra_program_free(st->overlay_prog);
    struct umbra_builder *ob = umbra_builder();
    umbra_ptr const ob_dst = umbra_bind_buf(ob, &st->out_buf);
    umbra_val32 const ob_x = umbra_f32_from_i32(ob, umbra_x(ob)),
                      ob_y = umbra_f32_from_i32(ob, umbra_y(ob));
    umbra_build_draw(ob, ob_dst, fmt, ob_x, ob_y,
                     coverage_bitmap,     &st->overlay_buf,
                     umbra_shader_color,  &st->overlay_color,
                     umbra_blend_srcover, NULL);
    struct umbra_flat_ir *oir = umbra_flat_ir(ob);
    umbra_builder_free(ob);
    st->overlay_prog = be->compile(be, oir);
    umbra_flat_ir_free(oir);
}

static void overview_draw(struct slide *s, double secs, int l, int t, int r, int b,
                          struct umbra_buf dst) {
    struct overview_slide *st = (struct overview_slide *)s;
    (void)l; (void)r;
    int const w = s->w, h = s->h;

    st->out_buf = dst;

    umbra_color const placeholder_bg = {0.094f, 0.094f, 0.094f, 1};

    for (int idx = 0; idx < st->n_cells; idx++) {
        struct cell *c = &st->cells[idx];
        int const x0 = c->col * st->cw,
                  y0 = c->row * st->ch,
                  x1 = (c->col + 1 == st->cols) ? w : x0 + st->cw,
                  y1 = (c->row + 1 == st->rows) ? h : y0 + st->ch;

        if (y1 <= t || y0 >= b) { continue; }
        int const yt = y0 > t ? y0 : t,
                  yb = y1 < b ? y1 : b;
        int const xl = x0,
                  xr = x1;

        // Paint the cell's background first -- live cells use srcover that
        // would accumulate without a fresh bg each frame; placeholders just
        // need something visible.  Empty slots (idx >= n_real, c->sub NULL)
        // get a neutral dark gray.
        umbra_color const bg = c->sub ? c->sub->bg : placeholder_bg;
        slide_bg_draw(st->bg, bg, xl, yt, xr, yb, dst);

        if (c->sub && c->rt && c->rt->draw) {
            c->rt->dst_buf = st->out_buf;
            slide_runtime_draw(c->rt, c->sub, secs, xl, yt, xr, yb);
        }
    }

    // Overlay: slide numbers + empty-slot X-boxes, white srcover.
    if (st->overlay_prog) { st->overlay_prog->queue(st->overlay_prog, 0, t, w, b); }
}

static void overview_free(struct slide *s) {
    struct overview_slide *st = (struct overview_slide *)s;
    umbra_program_free(st->overlay_prog);
    slide_bg_free(st->bg);
    free(st->overlay);
    if (st->cells) {
        for (int i = 0; i < st->n_cells; i++) {
            slide_runtime_free(st->cells[i].rt);
        }
        free(st->cells);
    }
    free(st);
}

struct slide* make_overview(void);

struct slide* make_overview(void) {
    struct overview_slide *st = calloc(1, sizeof *st);
    st->base = (struct slide){
        .title = "Overview",
        .bg = {0.06f, 0.06f, 0.06f, 1},
        .prepare = overview_prepare,
        .draw = overview_draw,
        .free = overview_free,
    };
    return &st->base;
}
