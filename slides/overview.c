#include "slide.h"
#include "coverage.h"
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

// Compose sub.transform (if any) with cell_mat into final_mat, so
// final_mat.apply(cell_pixel) == sub.transform.apply(cell_mat.apply(cell_pixel)).
static void compose_final(struct umbra_matrix *out,
                          struct slide_effects const *eff,
                          struct umbra_matrix const *cell_mat) {
    if (eff->transform_mat) {
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

    slide_bg_prepare(be, fmt, st->w, st->h);

    if (!st->overlay) {
        st->overlay = calloc((size_t)(st->w * st->h), sizeof *st->overlay);
    } else {
        for (int i = 0; i < st->w * st->h; i++) { st->overlay[i] = 0; }
    }
    st->overlay_buf = (struct umbra_buf){
        .ptr    = st->overlay,
        .count  = st->w * st->h,
        .stride = st->w,
    };
    st->overlay_color = (umbra_color){1, 1, 1, 1};

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

        int const x0 = c->col * cw,
                  y0 = c->row * ch;
        if (idx < n_real) {
            draw_number(st->overlay, st->w, x0 + 2, y0 + 2, idx + 1);
        } else {
            draw_xbox(st->overlay, st->w, x0, y0, cw, ch);
        }

        if (idx >= n_real) { continue; }  // empty slot (no slide)

        struct slide *sub = slide_get(idx);
        c->sub = sub;
        if (sub->prepare) { sub->prepare(sub, be, fmt); }

        // TODO: SDF-based slides (all of slides/sdf.c, and the blend variants
        // in slides/blend.c that wrap an SDF) currently render as placeholders
        // here because they have no get_effects hook.  Exposing them the same
        // way we do persp_slide -- i.e. umbra_coverage_from_sdf as the coverage
        // slot of slide_effects -- looks right but collapses perf at our
        // workloads: coverage_from_sdf evaluates the SDF at every pixel with
        // no tile culling, so sdf_text with ~100 curves/pixel x 30 cells
        // blew past the `ninja` 30s dump timeout in one experiment and would
        // tank demo FPS.  The standalone slides dodge this by going through
        // umbra_sdf_draw, whose bounds program tile-culls most pixels before
        // they reach sdf evaluation.
        //
        // Right fix: grow slide_effects (or a sibling get_sdf_effects hook)
        // to carry an umbra_sdf *sdf_fn + void *sdf_ctx + _Bool hard_edge,
        // and branch this cell builder to build a per-cell umbra_sdf_draw
        // (with &c->final_mat as the transform) instead of a draw_builder +
        // coverage_from_sdf.  The cell transform is affine (scale+translate)
        // so umbra_sdf_draw's affine-gate accepts it and keeps tile culling
        // -- see the TODO near umbra_sdf_draw in src/draw.c and commit
        // f63cec88 for that gate.  Then each cell dispatches via
        // umbra_sdf_draw_queue in overview_draw instead of c->prog->queue.
        //
        // Same treatment applies to blend slides, which also build an
        // umbra_sdf_draw internally.  If a slide exposes both effects and
        // sdf hooks, prefer the tile-culled sdf path.
        struct slide_effects eff = {0};
        if (!sub->get_effects || !sub->get_effects(sub, &eff)) { continue; }

        compose_final(&c->final_mat, &eff, &c->cell_mat);

        struct umbra_builder *b = umbra_draw_builder(
            &c->final_mat,
            eff.coverage_fn, eff.coverage_ctx,
            eff.shader_fn,   eff.shader_ctx,
            eff.blend_fn,    eff.blend_ctx,
            &st->out_buf,    fmt);
        struct umbra_flat_ir *ir = umbra_flat_ir(b);
        umbra_builder_free(b);
        c->prog = be->compile(be, ir);
        umbra_flat_ir_free(ir);
    }

    umbra_program_free(st->overlay_prog);
    struct umbra_builder *ob = umbra_draw_builder(
        NULL,
        coverage_bitmap,     &st->overlay_buf,
        umbra_shader_color,  &st->overlay_color,
        umbra_blend_srcover, NULL,
        &st->out_buf,        fmt);
    struct umbra_flat_ir *oir = umbra_flat_ir(ob);
    umbra_builder_free(ob);
    st->overlay_prog = be->compile(be, oir);
    umbra_flat_ir_free(oir);
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

    float const placeholder_bg[4] = {0.094f, 0.094f, 0.094f, 1};

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
        float const *bg = c->sub ? c->sub->bg : placeholder_bg;
        slide_bg_draw(bg, xl, yt, xr, yb, buf);

        if (c->sub && c->prog) {
            // Refresh animated uniforms, then recompose final_mat so its
            // contents reflect the current frame.
            if (c->sub->animate) { c->sub->animate(c->sub, secs); }
            struct slide_effects eff = {0};
            c->sub->get_effects(c->sub, &eff);
            compose_final(&c->final_mat, &eff, &c->cell_mat);
            c->prog->queue(c->prog, xl, yt, xr, yb);
        }
    }

    // Overlay: slide numbers + empty-slot X-boxes, white srcover.
    if (st->overlay_prog) { st->overlay_prog->queue(st->overlay_prog, 0, t, w, b); }
}

static int overview_get_builders(struct slide *s, struct umbra_fmt fmt,
                                 struct umbra_builder **out, int max) {
    struct overview_slide *st = (struct overview_slide *)s;
    if (max < 1 || st->n_cells == 0) { return 0; }
    // Return the first live cell's builder as a representative.
    for (int i = 0; i < st->n_cells; i++) {
        struct cell *c = &st->cells[i];
        if (!c->sub || !c->prog) { continue; }
        struct slide_effects eff = {0};
        if (!c->sub->get_effects(c->sub, &eff)) { continue; }
        out[0] = umbra_draw_builder(
            &c->final_mat,
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
    umbra_program_free(st->overlay_prog);
    free(st->overlay);
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
