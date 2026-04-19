#include "slide.h"
#include <stdlib.h>

static uint8_t const font3x5[10][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
    {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
};

// One cell: either a live-composed per-cell program (if sub exposes effects) or
// a static X-box placeholder.  The live cell's program was built with a
// final_mat = sub.transform * cell_mat, which we update per frame in draw.
struct cell {
    struct slide              *sub;          // NULL = placeholder
    struct umbra_matrix        cell_mat;   int :32;  // cell -> canvas, fixed at prepare
    struct umbra_matrix        final_mat;  int :32;  // mutated per frame, bound into prog
    struct umbra_program      *prog;
    int                        col, row;
};

struct overview_slide {
    struct slide base;

    int                   w, h;
    struct umbra_backend *be;
    struct umbra_fmt      out_fmt;
    struct umbra_buf      out_buf;

    struct cell          *cells;
    int                   n_cells, rows, cols, cw, ch;
    int                   :32;
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

// Compose sub.transform (if any) with cell_mat into final_mat, so
// final_mat.apply(cell_pixel) == sub.transform.apply(cell_mat.apply(cell_pixel)).
static void compose_final(struct umbra_matrix *out,
                          struct slide_effects const *eff,
                          struct umbra_matrix const *cell_mat) {
    if (eff->transform_fn && eff->transform_mat) {
        umbra_matrix_mul(out, eff->transform_mat, cell_mat);
    } else {
        *out = *cell_mat;
    }
}

static void overview_init(struct slide *s, int w, int h) {
    struct overview_slide *st = (struct overview_slide *)s;
    st->w = w;
    st->h = h;
}

static void overview_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct overview_slide *st = (struct overview_slide *)s;
    st->be      = be;
    st->out_fmt = fmt;

    int const n_real = slide_count() - 1;
    if (n_real <= 0) { return; }

    int cols = 1;
    while (cols * cols < n_real) { cols++; }
    int const rows = (n_real + cols - 1) / cols,
              cw   = st->w / cols,
              ch   = st->h / rows;
    st->cols = cols;
    st->rows = rows;
    st->cw   = cw;
    st->ch   = ch;

    if (st->cells) {
        for (int i = 0; i < st->n_cells; i++) { umbra_program_free(st->cells[i].prog); }
        free(st->cells);
    }
    int const total = rows * cols;
    st->cells = calloc((size_t)total, sizeof *st->cells);
    st->n_cells = total;

    for (int idx = 0; idx < total; idx++) {
        struct cell *c = &st->cells[idx];
        c->col = idx % cols;
        c->row = idx / cols;
        compute_cell_matrix(&c->cell_mat, c->col, c->row, cw, ch, st->w, st->h);
        c->final_mat = c->cell_mat;

        if (idx >= n_real) { continue; }  // placeholder cell

        struct slide *sub = slide_get(idx);
        if (sub->prepare) { sub->prepare(sub, be, fmt); }

        struct slide_effects eff = {0};
        if (!sub->get_effects || !sub->get_effects(sub, &eff)) { continue; }

        c->sub = sub;
        compose_final(&c->final_mat, &eff, &c->cell_mat);

        struct umbra_builder *b = umbra_draw_builder(
            umbra_transform_perspective, &c->final_mat,
            eff.coverage_fn, eff.coverage_ctx,
            eff.shader_fn,   eff.shader_ctx,
            eff.blend_fn,    eff.blend_ctx,
            &st->out_buf,    fmt);
        struct umbra_flat_ir *ir = umbra_flat_ir(b);
        umbra_builder_free(b);
        c->prog = be->compile(be, ir);
        umbra_flat_ir_free(ir);
    }
}

static void overview_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct overview_slide *st = (struct overview_slide *)s;
    (void)l; (void)r;
    int const w = st->w, h = st->h;

    st->out_buf = (struct umbra_buf){
        .ptr    = buf,
        .count  = w * h * st->out_fmt.planes,
        .stride = w,
    };

    // Clear each cell region to dark gray via a placeholder X-box first; then
    // overwrite with live cell programs where available.  The X-box + number
    // overlay requires CPU writes to an 8888 buffer, so we render directly
    // into `buf` only if the output is 8888.  For other formats we skip the
    // overlay for now.
    uint32_t *fb = (st->out_fmt.name == umbra_fmt_8888.name) ? (uint32_t*)buf : NULL;

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

        if (c->sub && c->prog) {
            // Refresh animated uniforms, then recompose final_mat so its
            // contents reflect the current frame.
            if (c->sub->animate) { c->sub->animate(c->sub, secs); }
            struct slide_effects eff = {0};
            c->sub->get_effects(c->sub, &eff);
            compose_final(&c->final_mat, &eff, &c->cell_mat);
            c->prog->queue(c->prog, xl, yt, xr, yb);
        } else if (fb) {
            for (int y = yt; y < yb; y++) {
                for (int x = xl; x < xr; x++) { fb[y * w + x] = 0xff181818; }
            }
            draw_xbox(fb, w, x0, yt, st->cw, yb - yt, 0xff404040);
        }

        if (fb) {
            draw_number(fb, w, x0 + 3, y0 + 3, idx + 1, 0x80000000);
            draw_number(fb, w, x0 + 2, y0 + 2, idx + 1, 0xffffffff);
        }
    }
}

static int overview_get_builders(struct slide *s, struct umbra_fmt fmt,
                                 struct umbra_builder **out, int max) {
    struct overview_slide *st = (struct overview_slide *)s;
    if (max < 1 || st->n_cells == 0) { return 0; }
    // Return the first live cell's builder as a representative.
    for (int i = 0; i < st->n_cells; i++) {
        struct cell *c = &st->cells[i];
        if (!c->sub) { continue; }
        struct slide_effects eff = {0};
        if (!c->sub->get_effects(c->sub, &eff)) { continue; }
        out[0] = umbra_draw_builder(
            umbra_transform_perspective, &c->final_mat,
            eff.coverage_fn, eff.coverage_ctx,
            eff.shader_fn,   eff.shader_ctx,
            eff.blend_fn,    eff.blend_ctx,
            &st->out_buf,    fmt);
        return out[0] ? 1 : 0;
    }
    return 0;
}

static void overview_free(struct slide *s) {
    struct overview_slide *st = (struct overview_slide *)s;
    if (st->cells) {
        for (int i = 0; i < st->n_cells; i++) { umbra_program_free(st->cells[i].prog); }
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
        .init = overview_init,
        .prepare = overview_prepare,
        .draw = overview_draw,
        .free = overview_free,
        .get_builders = overview_get_builders,
    };
    return &st->base;
}
