// TODO: add a "butt" slide recreating the smiling anthropomorphic butt from
// https://mtklein.github.io/butts/.  The original is an SVG with a bean-shaped
// body (single cubic path), stick-figure arms and legs (polylines), a curved
// smile (quadratic path), and two teardrop eyes.  A good SDF approximation:
// body as union of two offset circles (like the CSG union slide), arms/legs as
// capsule SDFs, mouth as an arc or thin capsule, eyes as small ellipses.
// Animate with orbit_update or a gentle breathing scale, and slowly cycle the
// fill color over time (the original randomizes on click).

#include "slide.h"
#include "slug.h"
#include "../include/umbra_interval.h"
#include <math.h>
#include <stdlib.h>

#define SLOT(field) ((int)(__builtin_offsetof(__typeof__(*self), field) / 4))

static void orbit_compute(float *cx, float *cy, float *ox, float *oy,
                           float *r_center, float *r_orbit, float t, int w, int h) {
    *cx = (float)w * 0.5f;
    *cy = (float)h * 0.5f;
    *r_center = (float)(w < h ? w : h) * 0.18f;
    *r_orbit  = (float)(w < h ? w : h) * 0.12f;
    float const dist = (float)(w < h ? w : h) * 0.22f;
    *ox = *cx + dist * cosf(t);
    *oy = *cy + dist * sinf(t);
}

static umbra_interval circle_sdf(struct umbra_builder *b,
                                 umbra_interval x, umbra_interval y,
                                 umbra_interval cx, umbra_interval cy,
                                 umbra_interval r) {
    umbra_interval const dx = umbra_interval_sub_f32(b, x, cx),
                         dy = umbra_interval_sub_f32(b, y, cy),
                         d2 = umbra_interval_add_f32(b,
                                  umbra_interval_mul_f32(b, dx, dx),
                                  umbra_interval_mul_f32(b, dy, dy)),
                         d  = umbra_interval_sqrt_f32(b, d2);
    return umbra_interval_sub_f32(b, d, r);
}

struct two_circle_sdf {
    float cx1, cy1, r1, cx2, cy2, r2;
};

enum csg_op { CSG_UNION, CSG_INTERSECT, CSG_DIFFERENCE };

struct csg_slide {
    struct slide base;
    int w, h;
    enum csg_op op;
    int pad;

    umbra_color               color;
    struct two_circle_sdf     sdf;
    umbra_sdf                 *sdf_fn;

    struct umbra_fmt          fmt;
    struct umbra_sdf_draw    *disp;
    struct umbra_buf          dst_buf;
};

static void two_circle_gather(struct umbra_builder *b, void *ctx,
                               umbra_interval x, umbra_interval y,
                               umbra_interval *a, umbra_interval *c) {
    struct two_circle_sdf const *self = ctx;
    umbra_ptr32 const u = umbra_bind_uniforms32(b, self, sizeof *self / 4);
    umbra_interval const cx1 = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(cx1))),
                         cy1 = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(cy1))),
                         r1  = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(r1))),
                         cx2 = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(cx2))),
                         cy2 = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(cy2))),
                         r2  = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(r2)));
    *a = circle_sdf(b, x, y, cx1, cy1, r1);
    *c = circle_sdf(b, x, y, cx2, cy2, r2);
}

static umbra_interval csg_union_build(void *ctx, struct umbra_builder *b,
                                       umbra_interval x, umbra_interval y) {
    umbra_interval a, c;
    two_circle_gather(b, ctx, x, y, &a, &c);
    return umbra_interval_min_f32(b, a, c);
}

static umbra_interval csg_intersect_build(void *ctx, struct umbra_builder *b,
                                           umbra_interval x, umbra_interval y) {
    umbra_interval a, c;
    two_circle_gather(b, ctx, x, y, &a, &c);
    return umbra_interval_max_f32(b, a, c);
}

static umbra_interval csg_difference_build(void *ctx, struct umbra_builder *b,
                                            umbra_interval x, umbra_interval y) {
    umbra_interval a, c;
    two_circle_gather(b, ctx, x, y, &a, &c);
    umbra_interval const neg_c = umbra_interval_sub_f32(b,
                                     umbra_interval_exact(umbra_imm_f32(b, 0)), c);
    return umbra_interval_max_f32(b, a, neg_c);
}

static void two_circle_orbit(struct two_circle_sdf *self, float t, int w, int h) {
    float ox, oy, r_orbit;
    orbit_compute(&self->cx1, &self->cy1, &ox, &oy, &self->r1, &r_orbit, t, w, h);
    self->cx2 = ox;
    self->cy2 = oy;
    self->r2  = r_orbit;
}

static void csg_init(struct slide *s, int w, int h) {
    struct csg_slide *st = (struct csg_slide *)s;
    st->w = w;
    st->h = h;
}

static void csg_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct csg_slide *st = (struct csg_slide *)s;
    umbra_sdf_draw_free(st->disp);
    st->fmt  = fmt;
    st->disp = umbra_sdf_draw(be,
                              st->sdf_fn,         &st->sdf,
                              0,
                              umbra_shader_color, &st->color,
                              umbra_blend_srcover, NULL,
                              fmt);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void csg_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct csg_slide *st = (struct csg_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    two_circle_orbit(&st->sdf, (float)secs, st->w, st->h);
    umbra_sdf_draw_queue(st->disp, l, t, r, b, (struct umbra_buf){
        .ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w,
    });
}

static int csg_get_builders(struct slide *s, struct umbra_fmt fmt,
                            struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct csg_slide *st = (struct csg_slide *)s;
    struct umbra_coverage_from_sdf cov = {
        .sdf_fn    = st->sdf_fn,
        .sdf_ctx   = &st->sdf,
        .hard_edge = 0,
    };
    out[0] = umbra_draw_builder(umbra_coverage_from_sdf, &cov,
                                umbra_shader_color,      &st->color,
                                umbra_blend_srcover,     NULL,
                                &st->dst_buf,            fmt);
    return out[0] ? 1 : 0;
}

static void csg_free(struct slide *s) {
    struct csg_slide *st = (struct csg_slide *)s;
    umbra_sdf_draw_free(st->disp);
    free(st);
}

static struct slide* make_csg(char const *title, float const bg[4], float const color[4],
                              enum csg_op op) {
    umbra_sdf *build = NULL;
    if (op == CSG_UNION)      { build = csg_union_build; }
    if (op == CSG_INTERSECT)  { build = csg_intersect_build; }
    if (op == CSG_DIFFERENCE) { build = csg_difference_build; }

    struct csg_slide *st = calloc(1, sizeof *st);
    st->op     = op;
    st->color  = (umbra_color){color[0], color[1], color[2], color[3]};
    st->sdf_fn = build;
    st->base = (struct slide){
        .title = title,
        .bg = {bg[0], bg[1], bg[2], bg[3]},
        .init = csg_init,
        .prepare = csg_prepare,
        .draw = csg_draw,
        .free = csg_free,
        .get_builders = csg_get_builders,
    };
    return &st->base;
}

struct circle_sdf {
    float cx, cy, r;
    int   :32;
};

static umbra_interval circle_build(void *ctx, struct umbra_builder *b,
                                    umbra_interval x, umbra_interval y) {
    struct circle_sdf const *self = ctx;
    umbra_ptr32 const u = umbra_bind_uniforms32(b, self, sizeof *self / 4);
    umbra_interval const cx = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(cx))),
                         cy = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(cy))),
                         r  = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(r)));
    return circle_sdf(b, x, y, cx, cy, r);
}

struct circle_slide {
    struct slide base;

    float cx0, cy0, vx, vy, r;
    int   w, h, pad;

    umbra_color               color;
    struct circle_sdf         sdf;

    struct umbra_fmt          fmt;
    struct umbra_sdf_draw    *qt;
    struct umbra_buf          dst_buf;
};

static void circle_init(struct slide *s, int w, int h) {
    struct circle_slide *st = (struct circle_slide *)s;
    st->w  = w;
    st->h  = h;
    st->r  = (float)(w < h ? w : h) * 0.18f;
    st->cx0 = (float)w * 0.3f;
    st->cy0 = (float)h * 0.4f;
    st->vx  = 1.7f;
    st->vy  = 1.3f;
}

static float bounce(float p0, float v, double secs, float range) {
    float p = fmodf(p0 + (float)secs * v, 2.0f * range);
    if (p < 0.0f) { p += 2.0f * range; }
    if (p > range) { p = 2.0f * range - p; }
    return p;
}

static void circle_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct circle_slide *st = (struct circle_slide *)s;
    umbra_sdf_draw_free(st->qt);
    st->fmt = fmt;
    st->qt  = umbra_sdf_draw(be,
                             circle_build,       &st->sdf,
                             0,
                             umbra_shader_color, &st->color,
                             umbra_blend_srcover, NULL,
                             fmt);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void circle_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct circle_slide *st = (struct circle_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);

    double const ticks = secs * 60.0;
    float const pad = st->r + 2.0f;
    st->sdf.cx = pad + bounce(st->cx0 - pad, st->vx, ticks, (float)st->w - 2.0f*pad);
    st->sdf.cy = pad + bounce(st->cy0 - pad, st->vy, ticks, (float)st->h - 2.0f*pad);
    st->sdf.r  = st->r;

    umbra_sdf_draw_queue(st->qt, l, t, r, b, (struct umbra_buf){
        .ptr=buf, .count=st->w * st->h * st->fmt.planes, .stride=st->w,
    });
}

static int circle_get_builders(struct slide *s, struct umbra_fmt fmt,
                               struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct circle_slide *st = (struct circle_slide *)s;
    struct umbra_coverage_from_sdf cov = {
        .sdf_fn    = circle_build,
        .sdf_ctx   = &st->sdf,
        .hard_edge = 0,
    };
    out[0] = umbra_draw_builder(umbra_coverage_from_sdf, &cov,
                                umbra_shader_color,      &st->color,
                                umbra_blend_srcover,     NULL,
                                &st->dst_buf,            fmt);
    return out[0] ? 1 : 0;
}

static void circle_free(struct slide *s) {
    struct circle_slide *st = (struct circle_slide *)s;
    umbra_sdf_draw_free(st->qt);
    free(st);
}

SLIDE(slide_sdf_circle) {
    struct circle_slide *st = calloc(1, sizeof *st);
    st->color = (umbra_color){0.95f, 0.45f, 0.10f, 1.0f};
    st->base = (struct slide){
        .title = "SDF Circle",
        .bg = {0.08f, 0.10f, 0.14f, 1.0f},
        .init = circle_init,
        .prepare = circle_prepare,
        .draw = circle_draw,
        .free = circle_free,
        .get_builders = circle_get_builders,
    };
    return &st->base;
}

SLIDE(slide_sdf_union) {
    return make_csg("SDF Union", (float[]){0.08f, 0.10f, 0.14f, 1},
                    (float[]){0.2f, 0.8f, 0.4f, 1}, CSG_UNION);
}

SLIDE(slide_sdf_intersect) {
    return make_csg("SDF Intersection", (float[]){0.08f, 0.10f, 0.14f, 1},
                    (float[]){0.8f, 0.4f, 0.2f, 1}, CSG_INTERSECT);
}

SLIDE(slide_sdf_difference) {
    return make_csg("SDF Difference", (float[]){0.08f, 0.10f, 0.14f, 1},
                    (float[]){0.4f, 0.2f, 0.8f, 1}, CSG_DIFFERENCE);
}

struct ring_sdf {
    float cx, cy, r, w;
};

static umbra_interval ring_build(void *ctx, struct umbra_builder *b,
                                  umbra_interval x, umbra_interval y) {
    struct ring_sdf const *self = ctx;
    umbra_ptr32 const u = umbra_bind_uniforms32(b, self, sizeof *self / 4);
    umbra_interval const cx = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(cx))),
                         cy = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(cy))),
                         r  = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(r))),
                         w  = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(w)));
    return umbra_interval_sub_f32(b, umbra_interval_abs_f32(b, circle_sdf(b, x, y, cx, cy, r)), w);
}

struct ring_slide {
    struct slide base;
    int w, h;

    umbra_color               color;
    struct ring_sdf           sdf;

    struct umbra_fmt          fmt;
    struct umbra_sdf_draw    *disp;
    struct umbra_buf          dst_buf;
};

static void ring_init(struct slide *s, int w, int h) {
    struct ring_slide *st = (struct ring_slide *)s;
    st->w = w;
    st->h = h;
}

static void ring_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct ring_slide *st = (struct ring_slide *)s;
    umbra_sdf_draw_free(st->disp);
    st->fmt  = fmt;
    st->disp = umbra_sdf_draw(be,
                              ring_build,         &st->sdf,
                              0,
                              umbra_shader_color, &st->color,
                              umbra_blend_srcover, NULL,
                              fmt);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void ring_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct ring_slide *st = (struct ring_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    float ox, oy, r_orbit;
    orbit_compute(&st->sdf.cx, &st->sdf.cy, &ox, &oy, &st->sdf.r, &r_orbit,
                   (float)secs, st->w, st->h);
    st->sdf.w = st->sdf.r * 0.15f;
    umbra_sdf_draw_queue(st->disp, l, t, r, b, (struct umbra_buf){
        .ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w,
    });
}

static int ring_get_builders(struct slide *s, struct umbra_fmt fmt,
                             struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct ring_slide *st = (struct ring_slide *)s;
    struct umbra_coverage_from_sdf cov = {
        .sdf_fn    = ring_build,
        .sdf_ctx   = &st->sdf,
        .hard_edge = 0,
    };
    out[0] = umbra_draw_builder(umbra_coverage_from_sdf, &cov,
                                umbra_shader_color,      &st->color,
                                umbra_blend_srcover,     NULL,
                                &st->dst_buf,            fmt);
    return out[0] ? 1 : 0;
}

static void ring_free(struct slide *s) {
    struct ring_slide *st = (struct ring_slide *)s;
    umbra_sdf_draw_free(st->disp);
    free(st);
}

SLIDE(slide_sdf_ring) {
    struct ring_slide *st = calloc(1, sizeof *st);
    st->color = (umbra_color){1.0f, 0.6f, 0.1f, 1};
    st->base = (struct slide){
        .title = "SDF Ring",
        .bg = {0.08f, 0.10f, 0.14f, 1},
        .init = ring_init,
        .prepare = ring_prepare,
        .draw = ring_draw,
        .free = ring_free,
        .get_builders = ring_get_builders,
    };
    return &st->base;
}

struct rounded_rect_sdf {
    float cx, cy, hw, hh, r;
    int   :32;
};

static umbra_interval rounded_rect_build(void *ctx, struct umbra_builder *b,
                                          umbra_interval x, umbra_interval y) {
    struct rounded_rect_sdf const *self = ctx;
    umbra_ptr32 const u = umbra_bind_uniforms32(b, self, sizeof *self / 4);
    umbra_interval const cx = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(cx))),
                         cy = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(cy))),
                         hw = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(hw))),
                         hh = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(hh))),
                         r  = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(r)));
    umbra_interval const zero = umbra_interval_exact(umbra_imm_f32(b, 0));
    umbra_interval const dx = umbra_interval_max_f32(b, zero,
                                  umbra_interval_sub_f32(b,
                                      umbra_interval_abs_f32(b,
                                          umbra_interval_sub_f32(b, x, cx)),
                                      umbra_interval_sub_f32(b, hw, r))),
                         dy = umbra_interval_max_f32(b, zero,
                                  umbra_interval_sub_f32(b,
                                      umbra_interval_abs_f32(b,
                                          umbra_interval_sub_f32(b, y, cy)),
                                      umbra_interval_sub_f32(b, hh, r)));
    umbra_interval const d2 = umbra_interval_add_f32(b,
                                  umbra_interval_mul_f32(b, dx, dx),
                                  umbra_interval_mul_f32(b, dy, dy));
    return umbra_interval_sub_f32(b, umbra_interval_sqrt_f32(b, d2), r);
}

struct rounded_rect_slide {
    struct slide base;
    int w, h;

    umbra_color               color;
    struct rounded_rect_sdf   sdf;

    struct umbra_fmt          fmt;
    struct umbra_sdf_draw    *disp;
    struct umbra_buf          dst_buf;
};

static void rounded_rect_init(struct slide *s, int w, int h) {
    struct rounded_rect_slide *st = (struct rounded_rect_slide *)s;
    st->w = w;
    st->h = h;
}

static void rounded_rect_prepare(struct slide *s, struct umbra_backend *be,
                                 struct umbra_fmt fmt) {
    struct rounded_rect_slide *st = (struct rounded_rect_slide *)s;
    umbra_sdf_draw_free(st->disp);
    st->fmt  = fmt;
    st->disp = umbra_sdf_draw(be,
                              rounded_rect_build, &st->sdf,
                              0,
                              umbra_shader_color, &st->color,
                              umbra_blend_srcover, NULL,
                              fmt);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void rounded_rect_draw(struct slide *s, double secs, int l, int t, int r, int b,
                               void *buf) {
    struct rounded_rect_slide *st = (struct rounded_rect_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);

    float const anim = sinf((float)secs) * 0.5f + 0.5f;
    st->sdf.cx = (float)st->w * 0.5f;
    st->sdf.cy = (float)st->h * 0.5f;
    st->sdf.hw = (float)(st->w < st->h ? st->w : st->h) * (0.15f + 0.1f * anim);
    st->sdf.hh = (float)(st->w < st->h ? st->w : st->h) * (0.10f + 0.08f * anim);
    st->sdf.r  = (float)(st->w < st->h ? st->w : st->h) * 0.04f;

    umbra_sdf_draw_queue(st->disp, l, t, r, b, (struct umbra_buf){
        .ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w,
    });
}

static int rounded_rect_get_builders(struct slide *s, struct umbra_fmt fmt,
                                     struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct rounded_rect_slide *st = (struct rounded_rect_slide *)s;
    struct umbra_coverage_from_sdf cov = {
        .sdf_fn    = rounded_rect_build,
        .sdf_ctx   = &st->sdf,
        .hard_edge = 0,
    };
    out[0] = umbra_draw_builder(umbra_coverage_from_sdf, &cov,
                                umbra_shader_color,      &st->color,
                                umbra_blend_srcover,     NULL,
                                &st->dst_buf,            fmt);
    return out[0] ? 1 : 0;
}

static void rounded_rect_free(struct slide *s) {
    struct rounded_rect_slide *st = (struct rounded_rect_slide *)s;
    umbra_sdf_draw_free(st->disp);
    free(st);
}

SLIDE(slide_sdf_rounded_rect) {
    struct rounded_rect_slide *st = calloc(1, sizeof *st);
    st->color = (umbra_color){0.1f, 0.5f, 0.9f, 1};
    st->base = (struct slide){
        .title = "SDF Rounded Rect",
        .bg = {0.08f, 0.10f, 0.14f, 1},
        .init = rounded_rect_init,
        .prepare = rounded_rect_prepare,
        .draw = rounded_rect_draw,
        .free = rounded_rect_free,
        .get_builders = rounded_rect_get_builders,
    };
    return &st->base;
}

struct capsule_sdf {
    float p0x, p0y, p1x, p1y, rad;
    int   :32;
};

static umbra_interval capsule_build(void *ctx, struct umbra_builder *b,
                                     umbra_interval x, umbra_interval y) {
    struct capsule_sdf const *self = ctx;
    umbra_ptr32 const u = umbra_bind_uniforms32(b, self, sizeof *self / 4);
    umbra_interval const p0x = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(p0x))),
                         p0y = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(p0y))),
                         p1x = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(p1x))),
                         p1y = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(p1y))),
                         rad = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(rad)));

    umbra_interval const dx = umbra_interval_sub_f32(b, p1x, p0x),
                         dy = umbra_interval_sub_f32(b, p1y, p0y);
    umbra_interval const px = umbra_interval_sub_f32(b, x, p0x),
                         py = umbra_interval_sub_f32(b, y, p0y);

    umbra_interval const dot_pd = umbra_interval_add_f32(b,
                                      umbra_interval_mul_f32(b, px, dx),
                                      umbra_interval_mul_f32(b, py, dy));
    umbra_interval const dot_dd = umbra_interval_add_f32(b,
                                      umbra_interval_mul_f32(b, dx, dx),
                                      umbra_interval_mul_f32(b, dy, dy));
    umbra_interval const zero = umbra_interval_exact(umbra_imm_f32(b, 0)),
                         one  = umbra_interval_exact(umbra_imm_f32(b, 1));
    umbra_interval const t = umbra_interval_min_f32(b, one,
                                 umbra_interval_max_f32(b, zero,
                                     umbra_interval_div_f32(b, dot_pd, dot_dd)));

    umbra_interval const cx = umbra_interval_add_f32(b, p0x, umbra_interval_mul_f32(b, t, dx)),
                         cy = umbra_interval_add_f32(b, p0y, umbra_interval_mul_f32(b, t, dy));
    umbra_interval const ex = umbra_interval_sub_f32(b, x, cx),
                         ey = umbra_interval_sub_f32(b, y, cy);
    umbra_interval const d2 = umbra_interval_add_f32(b,
                                  umbra_interval_mul_f32(b, ex, ex),
                                  umbra_interval_mul_f32(b, ey, ey));
    return umbra_interval_sub_f32(b, umbra_interval_sqrt_f32(b, d2), rad);
}

struct capsule_slide {
    struct slide base;
    int w, h;

    umbra_color               color;
    struct capsule_sdf        sdf;

    struct umbra_fmt          fmt;
    struct umbra_sdf_draw    *disp;
    struct umbra_buf          dst_buf;
};

static void capsule_init(struct slide *s, int w, int h) {
    struct capsule_slide *st = (struct capsule_slide *)s;
    st->w = w;
    st->h = h;
}

static void capsule_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct capsule_slide *st = (struct capsule_slide *)s;
    umbra_sdf_draw_free(st->disp);
    st->fmt  = fmt;
    st->disp = umbra_sdf_draw(be,
                              capsule_build,      &st->sdf,
                              0,
                              umbra_shader_color, &st->color,
                              umbra_blend_srcover, NULL,
                              fmt);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void capsule_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct capsule_slide *st = (struct capsule_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    float cx, cy, ox, oy, r_center, r_orbit;
    orbit_compute(&cx, &cy, &ox, &oy, &r_center, &r_orbit, (float)secs, st->w, st->h);
    st->sdf.p0x = cx; st->sdf.p0y = cy;
    st->sdf.p1x = ox; st->sdf.p1y = oy;
    st->sdf.rad = r_center * 0.15f;
    umbra_sdf_draw_queue(st->disp, l, t, r, b, (struct umbra_buf){
        .ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w,
    });
}

static int capsule_get_builders(struct slide *s, struct umbra_fmt fmt,
                                struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct capsule_slide *st = (struct capsule_slide *)s;
    struct umbra_coverage_from_sdf cov = {
        .sdf_fn    = capsule_build,
        .sdf_ctx   = &st->sdf,
        .hard_edge = 0,
    };
    out[0] = umbra_draw_builder(umbra_coverage_from_sdf, &cov,
                                umbra_shader_color,      &st->color,
                                umbra_blend_srcover,     NULL,
                                &st->dst_buf,            fmt);
    return out[0] ? 1 : 0;
}

static void capsule_free(struct slide *s) {
    struct capsule_slide *st = (struct capsule_slide *)s;
    umbra_sdf_draw_free(st->disp);
    free(st);
}

SLIDE(slide_sdf_capsule) {
    struct capsule_slide *st = calloc(1, sizeof *st);
    st->color = (umbra_color){0.9f, 0.3f, 0.6f, 1};
    st->base = (struct slide){
        .title = "SDF Capsule",
        .bg = {0.08f, 0.10f, 0.14f, 1},
        .init = capsule_init,
        .prepare = capsule_prepare,
        .draw = capsule_draw,
        .free = capsule_free,
        .get_builders = capsule_get_builders,
    };
    return &st->base;
}

struct halfplane_sdf {
    float nx, ny, d;
    int   :32;
};

static umbra_interval halfplane_build(void *ctx, struct umbra_builder *b,
                                       umbra_interval x, umbra_interval y) {
    struct halfplane_sdf const *self = ctx;
    umbra_ptr32 const u = umbra_bind_uniforms32(b, self, sizeof *self / 4);
    umbra_interval const nx = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(nx))),
                         ny = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(ny))),
                         d  = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(d)));
    return umbra_interval_sub_f32(b,
               umbra_interval_add_f32(b,
                   umbra_interval_mul_f32(b, nx, x),
                   umbra_interval_mul_f32(b, ny, y)),
               d);
}

struct halfplane_slide {
    struct slide base;
    int w, h;

    umbra_color               color;
    struct halfplane_sdf      sdf;

    struct umbra_fmt          fmt;
    struct umbra_sdf_draw    *disp;
    struct umbra_buf          dst_buf;
};

static void halfplane_init(struct slide *s, int w, int h) {
    struct halfplane_slide *st = (struct halfplane_slide *)s;
    st->w = w;
    st->h = h;
}

static void halfplane_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct halfplane_slide *st = (struct halfplane_slide *)s;
    umbra_sdf_draw_free(st->disp);
    st->fmt  = fmt;
    st->disp = umbra_sdf_draw(be,
                              halfplane_build,    &st->sdf,
                              0,
                              umbra_shader_color, &st->color,
                              umbra_blend_srcover, NULL,
                              fmt);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void halfplane_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct halfplane_slide *st = (struct halfplane_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    float cx, cy, ox, oy, r_center, r_orbit;
    orbit_compute(&cx, &cy, &ox, &oy, &r_center, &r_orbit, (float)secs, st->w, st->h);
    float const dx = ox - cx, dy = oy - cy;
    float const len = sqrtf(dx * dx + dy * dy);
    st->sdf.nx = len > 0 ? dy / len : 0;
    st->sdf.ny = len > 0 ? -dx / len : 1;
    st->sdf.d  = st->sdf.nx * cx + st->sdf.ny * cy;
    umbra_sdf_draw_queue(st->disp, l, t, r, b, (struct umbra_buf){
        .ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w,
    });
}

static int halfplane_get_builders(struct slide *s, struct umbra_fmt fmt,
                                  struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct halfplane_slide *st = (struct halfplane_slide *)s;
    struct umbra_coverage_from_sdf cov = {
        .sdf_fn    = halfplane_build,
        .sdf_ctx   = &st->sdf,
        .hard_edge = 0,
    };
    out[0] = umbra_draw_builder(umbra_coverage_from_sdf, &cov,
                                umbra_shader_color,      &st->color,
                                umbra_blend_srcover,     NULL,
                                &st->dst_buf,            fmt);
    return out[0] ? 1 : 0;
}

static void halfplane_free(struct slide *s) {
    struct halfplane_slide *st = (struct halfplane_slide *)s;
    umbra_sdf_draw_free(st->disp);
    free(st);
}

SLIDE(slide_sdf_halfplane) {
    struct halfplane_slide *st = calloc(1, sizeof *st);
    st->color = (umbra_color){0.3f, 0.7f, 0.9f, 1};
    st->base = (struct slide){
        .title = "SDF Halfplane",
        .bg = {0.08f, 0.10f, 0.14f, 1},
        .init = halfplane_init,
        .prepare = halfplane_prepare,
        .draw = halfplane_draw,
        .free = halfplane_free,
        .get_builders = halfplane_get_builders,
    };
    return &st->base;
}

// TODO: use the actual quadratic Bezier distance (P0, P1, P2) instead of
// the line-segment approximation (P0, P2).  The control point P1 is already
// in the curve data at offsets k+2, k+3 — just not used yet.
//
// Distance to a quadratic Bezier requires solving a cubic for the nearest
// parameter t.  Since umbra has no cbrt (and no trig), the closed-form
// Cardano path is out; use 3–5 Newton iterations on
// f(t) = (B(t) - p) · B'(t) seeded from the best of t ∈ {0, 0.5, 1} and
// clamped to [0,1] after each step.  Sub-pixel accuracy in ~3 iters.
//
// For proper filled rendering add winding-number sign alongside the
// distance: for each curve solve B(t).y = gy (quadratic in t, one sqrt),
// check each root in [0,1], XOR a parity var when B(root).x > gx.  Final
// signed distance = sel(parity, -|dist|, +|dist|).  Bounds path returns
// the conservative {-|dist|.hi, +|dist|.hi} when inputs are non-exact.
//
// TODO: add an SDF stroke wrapper (abs(sdf) - width) that can be toggled
// on any SDF slide, like iv2d's 's' key.  This converts fills to outlines
// and outlines to double-stroked outlines.

struct sdf_text_sdf {
    float            scale_x, scale_y, off_x, off_y;
    int              n_curves, :32;
    struct umbra_buf curves;
};

static umbra_interval sdf_text_build(void *ctx, struct umbra_builder *b,
                                      umbra_interval x, umbra_interval y) {
    struct sdf_text_sdf const *self = ctx;
    umbra_ptr32 const u    = umbra_bind_uniforms32(b, self, sizeof *self / 4);
    umbra_ptr32 const data = umbra_bind_buf32(b, &self->curves);
    umbra_val32 const n    = umbra_uniform_32(b, u, SLOT(n_curves));

    umbra_interval const sx = umbra_interval_exact(
                                  umbra_uniform_32(b, u, SLOT(scale_x))),
                         sy = umbra_interval_exact(
                                  umbra_uniform_32(b, u, SLOT(scale_y))),
                         ox = umbra_interval_exact(
                                  umbra_uniform_32(b, u, SLOT(off_x))),
                         oy = umbra_interval_exact(
                                  umbra_uniform_32(b, u, SLOT(off_y)));

    umbra_interval const gx = umbra_interval_add_f32(b, umbra_interval_mul_f32(b, x, sx), ox),
                         gy = umbra_interval_add_f32(b, umbra_interval_mul_f32(b, y, sy), oy);

    umbra_imm_f32(b, 0);
    umbra_imm_f32(b, 1);
    umbra_imm_f32(b, 1.5f);
    umbra_imm_i32(b, 1);
    umbra_imm_i32(b, 4);
    umbra_imm_i32(b, 5);
    umbra_imm_i32(b, 6);

    umbra_var32 lo_var = umbra_declare_var32(b);
    umbra_var32 hi_var = umbra_declare_var32(b);
    umbra_store_var32(b, lo_var, umbra_imm_f32(b, 1e9f));
    umbra_store_var32(b, hi_var, umbra_imm_f32(b, 1e9f));

    umbra_val32 const j = umbra_loop(b, n); {
        umbra_val32 const k = umbra_mul_i32(b, j, umbra_imm_i32(b, 6));
        umbra_val32 const p0x_v = umbra_gather_32(b, data, k),
                          p0y_v = umbra_gather_32(b, data,
                                      umbra_add_i32(b, k, umbra_imm_i32(b, 1))),
                          p2x_v = umbra_gather_32(b, data,
                                      umbra_add_i32(b, k, umbra_imm_i32(b, 4))),
                          p2y_v = umbra_gather_32(b, data,
                                      umbra_add_i32(b, k, umbra_imm_i32(b, 5)));

        umbra_val32 const dx_v = umbra_sub_f32(b, p2x_v, p0x_v),
                          dy_v = umbra_sub_f32(b, p2y_v, p0y_v);

        umbra_val32 const dot_dd_v = umbra_add_f32(b,
                                         umbra_mul_f32(b, dx_v, dx_v),
                                         umbra_mul_f32(b, dy_v, dy_v));

        umbra_interval const p0x = umbra_interval_exact(p0x_v),
                             p0y = umbra_interval_exact(p0y_v),
                             dx  = umbra_interval_exact(dx_v),
                             dy  = umbra_interval_exact(dy_v),
                             dd  = umbra_interval_exact(dot_dd_v);

        umbra_interval const px = umbra_interval_sub_f32(b, gx, p0x),
                             py = umbra_interval_sub_f32(b, gy, p0y);
        umbra_interval const dot_pd = umbra_interval_add_f32(b,
                                          umbra_interval_mul_f32(b, px, dx),
                                          umbra_interval_mul_f32(b, py, dy));
        umbra_interval const zero = umbra_interval_exact(umbra_imm_f32(b, 0)),
                             one  = umbra_interval_exact(umbra_imm_f32(b, 1));
        umbra_interval const t = umbra_interval_min_f32(b, one,
                                     umbra_interval_max_f32(b, zero,
                                         umbra_interval_div_f32(b, dot_pd, dd)));

        umbra_interval const cx = umbra_interval_add_f32(b, p0x,
                                      umbra_interval_mul_f32(b, t, dx)),
                             cy = umbra_interval_add_f32(b, p0y,
                                      umbra_interval_mul_f32(b, t, dy));
        umbra_interval const ex = umbra_interval_sub_f32(b, gx, cx),
                             ey = umbra_interval_sub_f32(b, gy, cy);
        umbra_interval const d2 = umbra_interval_add_f32(b,
                                      umbra_interval_mul_f32(b, ex, ex),
                                      umbra_interval_mul_f32(b, ey, ey));
        umbra_interval const dist = umbra_interval_sqrt_f32(b, d2);

        umbra_store_var32(b, lo_var, umbra_min_f32(b, umbra_load_var32(b, lo_var), dist.lo));
        umbra_store_var32(b, hi_var, umbra_min_f32(b, umbra_load_var32(b, hi_var), dist.hi));
    } umbra_end_loop(b);

    umbra_interval const min_dist = {umbra_load_var32(b, lo_var), umbra_load_var32(b, hi_var)};
    return umbra_interval_sub_f32(b, min_dist, umbra_interval_exact(umbra_imm_f32(b, 1.5f)));
}

struct sdf_text_slide {
    struct slide base;
    int w, h;

    struct slug_curves        slug;
    umbra_color               color;
    struct sdf_text_sdf       sdf;

    struct umbra_fmt          fmt;
    struct umbra_sdf_draw    *disp;
    struct umbra_buf          dst_buf;
};

static void sdf_text_init(struct slide *s, int w, int h) {
    struct sdf_text_slide *st = (struct sdf_text_slide *)s;
    st->w = w;
    st->h = h;
    st->slug = slug_extract("Hamburgefons", (float)h * 0.4f);

    float const gw = st->slug.w,
                gh = st->slug.h;
    float const sx = gw / (float)w,
                sy = gh / (float)h;
    float const scale = sx > sy ? sx : sy;
    float const pad_x = ((float)w * scale - gw) * 0.5f,
                pad_y = ((float)h * scale - gh) * 0.5f;
    st->sdf.scale_x = scale;
    st->sdf.scale_y = scale;
    st->sdf.off_x   = -pad_x;
    st->sdf.off_y   = -pad_y;
    st->sdf.n_curves = st->slug.count;
    st->sdf.curves   = (struct umbra_buf){.ptr = st->slug.data,
                                           .count = st->slug.count * 6};
}

static void sdf_text_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct sdf_text_slide *st = (struct sdf_text_slide *)s;
    umbra_sdf_draw_free(st->disp);
    st->fmt  = fmt;
    st->disp = umbra_sdf_draw(be,
                              sdf_text_build,     &st->sdf,
                              0,
                              umbra_shader_color, &st->color,
                              umbra_blend_srcover, NULL,
                              fmt);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void sdf_text_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    (void)secs;
    struct sdf_text_slide *st = (struct sdf_text_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    umbra_sdf_draw_queue(st->disp, l, t, r, b, (struct umbra_buf){
        .ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w,
    });
}

static int sdf_text_get_builders(struct slide *s, struct umbra_fmt fmt,
                                 struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct sdf_text_slide *st = (struct sdf_text_slide *)s;
    struct umbra_coverage_from_sdf cov = {
        .sdf_fn    = sdf_text_build,
        .sdf_ctx   = &st->sdf,
        .hard_edge = 0,
    };
    out[0] = umbra_draw_builder(umbra_coverage_from_sdf, &cov,
                                umbra_shader_color,      &st->color,
                                umbra_blend_srcover,     NULL,
                                &st->dst_buf,            fmt);
    return out[0] ? 1 : 0;
}

static void sdf_text_free(struct slide *s) {
    struct sdf_text_slide *st = (struct sdf_text_slide *)s;
    umbra_sdf_draw_free(st->disp);
    slug_free(&st->slug);
    free(st);
}

SLIDE(slide_sdf_text) {
    struct sdf_text_slide *st = calloc(1, sizeof *st);
    st->color = (umbra_color){0.95f, 0.9f, 0.8f, 1};
    st->base = (struct slide){
        .title = "SDF Text",
        .bg = {0.08f, 0.10f, 0.14f, 1},
        .init = sdf_text_init,
        .prepare = sdf_text_prepare,
        .draw = sdf_text_draw,
        .free = sdf_text_free,
        .get_builders = sdf_text_get_builders,
    };
    return &st->base;
}

#define NGON_SIDES 6

struct ngon_sdf {
    int              n_sides, :32;
    struct umbra_buf hp;
};

static umbra_interval ngon_build(void *ctx, struct umbra_builder *b,
                                  umbra_interval x, umbra_interval y) {
    struct ngon_sdf const *self = ctx;
    umbra_ptr32 const u    = umbra_bind_uniforms32(b, self, sizeof *self / 4);
    umbra_ptr32 const data = umbra_bind_buf32(b, &self->hp);
    umbra_val32 const n    = umbra_uniform_32(b, u, SLOT(n_sides));

    umbra_var32 lo_var = umbra_declare_var32(b);
    umbra_var32 hi_var = umbra_declare_var32(b);
    umbra_store_var32(b, lo_var, umbra_imm_f32(b, -1e9f));
    umbra_store_var32(b, hi_var, umbra_imm_f32(b, -1e9f));

    umbra_val32 const j = umbra_loop(b, n); {
        umbra_val32 const idx = umbra_mul_i32(b, j, umbra_imm_i32(b, 3));
        umbra_interval const nx = umbra_interval_exact(umbra_gather_32(b, data, idx)),
                             ny = umbra_interval_exact(
                                 umbra_gather_32(b, data,
                                     umbra_add_i32(b, idx, umbra_imm_i32(b, 1)))),
                             d  = umbra_interval_exact(
                                 umbra_gather_32(b, data,
                                     umbra_add_i32(b, idx, umbra_imm_i32(b, 2))));
        umbra_interval const hp = umbra_interval_sub_f32(b,
                                      umbra_interval_add_f32(b,
                                          umbra_interval_mul_f32(b, nx, x),
                                          umbra_interval_mul_f32(b, ny, y)),
                                      d);
        umbra_store_var32(b, lo_var, umbra_max_f32(b, umbra_load_var32(b, lo_var), hp.lo));
        umbra_store_var32(b, hi_var, umbra_max_f32(b, umbra_load_var32(b, hi_var), hp.hi));
    } umbra_end_loop(b);

    return (umbra_interval){umbra_load_var32(b, lo_var), umbra_load_var32(b, hi_var)};
}

struct ngon_slide {
    struct slide base;
    int w, h;
    float cx, cy, r, angle;
    float *hp_data;

    umbra_color               color;
    struct ngon_sdf           sdf;

    struct umbra_fmt          fmt;
    struct umbra_sdf_draw    *disp;
    struct umbra_buf          dst_buf;
};

static void ngon_init(struct slide *s, int w, int h) {
    struct ngon_slide *st = (struct ngon_slide *)s;
    st->w = w;
    st->h = h;
}

static void ngon_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct ngon_slide *st = (struct ngon_slide *)s;
    umbra_sdf_draw_free(st->disp);
    st->fmt  = fmt;
    st->disp = umbra_sdf_draw(be,
                              ngon_build,         &st->sdf,
                              0,
                              umbra_shader_color, &st->color,
                              umbra_blend_srcover, NULL,
                              fmt);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void ngon_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct ngon_slide *st = (struct ngon_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    st->cx    = (float)st->w * 0.5f;
    st->cy    = (float)st->h * 0.5f;
    st->r     = (float)(st->w < st->h ? st->w : st->h) * 0.3f;
    st->angle = (float)secs;
    for (int i = 0; i < NGON_SIDES; i++) {
        float const a = st->angle + (float)i * (2.0f * 3.14159265f / NGON_SIDES);
        float const vx = st->cx + st->r * cosf(a),
                    vy = st->cy + st->r * sinf(a);
        float const a2 = st->angle + (float)(i + 1) * (2.0f * 3.14159265f / NGON_SIDES);
        float const wx = st->cx + st->r * cosf(a2),
                    wy = st->cy + st->r * sinf(a2);
        float const ex = wx - vx,
                    ey = wy - vy;
        float const len = sqrtf(ex * ex + ey * ey);
        float const nx = len > 0 ? ey / len : 0,
                    ny = len > 0 ? -ex / len : 1;
        st->hp_data[i * 3]     = nx;
        st->hp_data[i * 3 + 1] = ny;
        st->hp_data[i * 3 + 2] = nx * vx + ny * vy;
    }
    st->sdf.n_sides = NGON_SIDES;
    st->sdf.hp      = (struct umbra_buf){.ptr = st->hp_data, .count = 3 * NGON_SIDES};
    umbra_sdf_draw_queue(st->disp, l, t, r, b, (struct umbra_buf){
        .ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w,
    });
}

static int ngon_get_builders(struct slide *s, struct umbra_fmt fmt,
                             struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct ngon_slide *st = (struct ngon_slide *)s;
    struct umbra_coverage_from_sdf cov = {
        .sdf_fn    = ngon_build,
        .sdf_ctx   = &st->sdf,
        .hard_edge = 0,
    };
    out[0] = umbra_draw_builder(umbra_coverage_from_sdf, &cov,
                                umbra_shader_color,      &st->color,
                                umbra_blend_srcover,     NULL,
                                &st->dst_buf,            fmt);
    return out[0] ? 1 : 0;
}

static void ngon_free(struct slide *s) {
    struct ngon_slide *st = (struct ngon_slide *)s;
    umbra_sdf_draw_free(st->disp);
    free(st->hp_data);
    free(st);
}

SLIDE(slide_sdf_ngon) {
    struct ngon_slide *st = calloc(1, sizeof *st);
    st->color   = (umbra_color){0.8f, 0.8f, 0.2f, 1};
    st->hp_data = malloc(3 * NGON_SIDES * sizeof(float));
    st->base = (struct slide){
        .title = "SDF N-Gon",
        .bg = {0.08f, 0.10f, 0.14f, 1},
        .init = ngon_init,
        .prepare = ngon_prepare,
        .draw = ngon_draw,
        .free = ngon_free,
        .get_builders = ngon_get_builders,
    };
    return &st->base;
}
