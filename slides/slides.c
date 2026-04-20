#include "slide.h"
#include "text.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

void slide_perspective_matrix(struct umbra_matrix *out, float t,
                              int sw, int sh, int bw, int bh) {
    float cx = (float)sw * 0.5f;
    float cy = (float)sh * 0.5f;
    float bx = (float)bw * 0.5f;
    float by = (float)bh * 0.5f;
    float angle = t * 0.3f;
    float tilt  = sinf(t * 0.7f) * 0.0008f;
    float sc    = 1.0f + 0.2f * sinf(t * 0.5f);
    float ca = cosf(angle), sa = sinf(angle);
    float w0 = 1.0f - tilt * cx;
    *out = (struct umbra_matrix){
        .sx = ca * sc,  .kx = sa * sc,
        .tx = -cx*ca*sc - cy*sa*sc + bx*w0,
        .ky = -sa * sc, .sy = ca * sc,
        .ty = cx*sa*sc - cy*ca*sc + by*w0,
        .p0 = tilt, .p1 = 0.0f, .p2 = w0,
    };
}

enum { MAX_SLIDES = 32 };

struct slide* make_overview(void);

static slide_factory_fn registry[MAX_SLIDES];
static int              registry_count;

static struct slide *all[MAX_SLIDES];
static int           count;

void slide_register(slide_factory_fn factory) {
    if (registry_count < MAX_SLIDES) {
        registry[registry_count++] = factory;
    }
}

int           slide_count(void) { return count; }
struct slide* slide_get(int i)  { return all[i]; }

static void add_slide(struct slide *s, int w, int h) {
    all[count] = s;
    s->w = w;
    s->h = h;
    if (s->init) { s->init(s); }
    count++;
}

void slides_init(int w, int h) {
    text_shared_init(w, h, (float)h * 0.15f);

    count = 0;
    for (int i = 0; i < registry_count; i++) {
        add_slide(registry[i](), w, h);
    }
    add_slide(make_overview(), w, h);
}

void slides_cleanup(void) {
    for (int i = 0; i < count; i++) {
        if (all[i]->free) { all[i]->free(all[i]); }
    }
    text_shared_cleanup();
    slide_bg_cleanup();
    count = 0;
}

static struct umbra_flat_ir    *bg_ir;
static struct umbra_program    *bg_prog;
static umbra_color              bg_color;
static struct umbra_fmt         bg_fmt;
static int                      bg_w, bg_h;
static struct umbra_buf         bg_dst_buf;

void slide_bg_prepare(struct umbra_backend *be, struct umbra_fmt fmt, int w, int h) {
    if (bg_fmt.name != fmt.name || !bg_ir || bg_w != w || bg_h != h) {
        umbra_flat_ir_free(bg_ir);
        bg_fmt = fmt;
        bg_w   = w;
        bg_h   = h;
        struct umbra_builder *b = umbra_builder();
        umbra_ptr const dst = umbra_bind_buf(b, &bg_dst_buf);
        umbra_val32 const x = umbra_f32_from_i32(b, umbra_x(b)),
                          y = umbra_f32_from_i32(b, umbra_y(b));
        umbra_build_draw(b, dst, fmt, x, y,
                         NULL,               NULL,
                         umbra_shader_color, &bg_color,
                         NULL,               NULL);
        bg_ir = umbra_flat_ir(b);
        umbra_builder_free(b);
    }
    umbra_program_free(bg_prog);
    bg_prog = be->compile(be, bg_ir);
}

void slide_bg_draw(umbra_color bg, int l, int t, int r, int b, void *buf) {
    bg_color = bg;
    bg_dst_buf = (struct umbra_buf){
        .ptr=buf, .count=bg_w * bg_h * bg_fmt.planes, .stride=bg_w,
    };
    bg_prog->queue(bg_prog, l, t, r, b);
}

void slide_bg_cleanup(void) {
    if (bg_prog) { umbra_program_free(bg_prog); bg_prog = NULL; }
    umbra_flat_ir_free(bg_ir); bg_ir = NULL;
    bg_fmt = (struct umbra_fmt){0};
    bg_w = bg_h = 0;
}

// TODO: query per-backend dispatch_granularity instead of this global
// compromise (same note as src/draw.c's QUEUE_MIN_TILE).
enum { SLIDE_TILE = 512 };

static struct umbra_builder* runtime_draw_builder(
        struct slide *s, struct umbra_buf *dst,
        struct umbra_fmt fmt,
        struct umbra_matrix const *pre) {
    struct umbra_builder *b = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(b, dst);
    umbra_val32 x = umbra_f32_from_i32(b, umbra_x(b)),
                y = umbra_f32_from_i32(b, umbra_y(b));
    if (pre) {
        umbra_point_val32 const p = umbra_transform_perspective(pre, b, x, y);
        x = p.x;
        y = p.y;
    }
    s->build_draw(s, b, dst_ptr, fmt, x, y);
    return b;
}

static void runtime_sdf_builders(
        struct slide *s, struct slide_runtime *rt,
        struct umbra_fmt fmt,
        struct umbra_matrix const *pre,
        struct umbra_builder **out_draw,
        struct umbra_builder **out_bounds) {
    struct umbra_builder *db = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(db, &rt->dst_buf);
    umbra_val32 x = umbra_f32_from_i32(db, umbra_x(db)),
                y = umbra_f32_from_i32(db, umbra_y(db));
    if (pre) {
        umbra_point_val32 const p = umbra_transform_perspective(pre, db, x, y);
        x = p.x;
        y = p.y;
    }

    struct umbra_builder *bb = umbra_builder();
    umbra_ptr const cov_ptr = umbra_bind_buf(bb, &rt->cov_buf);
    umbra_interval ix, iy;
    umbra_sdf_tile_intervals(bb, &rt->grid, pre, &ix, &iy);

    s->build_sdf_draw(s, db, dst_ptr, fmt, x, y, bb, cov_ptr, ix, iy);

    *out_draw   = db;
    *out_bounds = bb;
}

struct slide_runtime* slide_runtime(struct slide *s,
                                    int w, int h,
                                    struct umbra_backend *be, struct umbra_fmt fmt,
                                    struct umbra_matrix const *pre) {
    struct slide_runtime *rt = calloc(1, sizeof *rt);
    rt->fmt = fmt;
    rt->w   = w;
    rt->h   = h;

    if (s->build_sdf_draw) {
        struct umbra_builder *db, *bb;
        runtime_sdf_builders(s, rt, fmt, pre, &db, &bb);

        struct umbra_flat_ir *dir = umbra_flat_ir(db);
        umbra_builder_free(db);
        rt->draw = be->compile(be, dir);
        umbra_flat_ir_free(dir);

        struct umbra_flat_ir *bir = umbra_flat_ir(bb);
        umbra_builder_free(bb);
        struct umbra_backend *bounds_be = umbra_backend_jit();
        if (!bounds_be) { bounds_be = umbra_backend_interp(); }
        rt->bounds = bounds_be->compile(bounds_be, bir);
        umbra_flat_ir_free(bir);
    } else if (s->build_draw) {
        struct umbra_builder *b = runtime_draw_builder(s, &rt->dst_buf, fmt, pre);
        struct umbra_flat_ir *ir = umbra_flat_ir(b);
        umbra_builder_free(b);
        rt->draw = be->compile(be, ir);
        umbra_flat_ir_free(ir);
    }
    return rt;
}

void slide_runtime_draw(struct slide_runtime *rt, struct slide *s,
                        double secs, int l, int t, int r, int b) {
    if (!rt->draw) { return; }
    if (s->animate) { s->animate(s, secs); }

    if (rt->bounds) {
        int const T  = SLIDE_TILE,
                  xt = (r - l + T - 1) / T,
                  yt = (b - t + T - 1) / T,
                  tiles = xt * yt;
        if (tiles > rt->cov_cap) {
            rt->cov     = realloc(rt->cov, (size_t)tiles * sizeof *rt->cov);
            rt->cov_cap = tiles;
        }
        rt->cov_buf = (struct umbra_buf){.ptr = rt->cov, .count = tiles, .stride = xt};
        umbra_sdf_dispatch(rt->bounds, rt->draw, &rt->grid, &rt->cov_buf,
                           T, l, t, r, b);
    } else {
        rt->draw->queue(rt->draw, l, t, r, b);
    }
}

void slide_runtime_free(struct slide_runtime *rt) {
    if (!rt) { return; }
    struct umbra_backend *bounds_be = rt->bounds ? rt->bounds->backend : NULL;
    umbra_program_free(rt->draw);
    umbra_program_free(rt->bounds);
    umbra_backend_free(bounds_be);
    free(rt->cov);
    free(rt);
}

struct slide_builders slide_builders(struct slide_runtime *rt, struct slide *s,
                                     struct umbra_fmt fmt,
                                     struct umbra_matrix const *pre) {
    struct slide_builders out = {0};
    if (s->build_sdf_draw) {
        runtime_sdf_builders(s, rt, fmt, pre, &out.draw, &out.bounds);
    } else if (s->build_draw) {
        out.draw = runtime_draw_builder(s, &rt->dst_buf, fmt, pre);
    }
    return out;
}
