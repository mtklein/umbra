// TODO: adapt iv2d's SDF text rendering as a follow-up.

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

#define SLOT(field) \
    ((int)((__builtin_offsetof(__typeof__(*self), field) - sizeof(self->base)) / 4))

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
    struct umbra_sdf base;
    float            cx1, cy1, r1, cx2, cy2, r2;
};

enum csg_op { CSG_UNION, CSG_INTERSECT, CSG_DIFFERENCE };

struct csg_slide {
    struct slide base;
    int w, h;
    enum csg_op op;
    int pad;

    struct umbra_shader      *shader;
    struct two_circle_sdf     sdf;

    struct umbra_fmt          fmt;
    struct umbra_sdf_draw    *disp;
};

static void two_circle_gather(struct umbra_builder *b, umbra_ptr32 uniforms,
                               struct two_circle_sdf *self,
                               umbra_interval x, umbra_interval y,
                               umbra_interval *a, umbra_interval *c) {
    umbra_interval const cx1 = umbra_interval_exact(umbra_uniform_32(b, uniforms, SLOT(cx1))),
                         cy1 = umbra_interval_exact(umbra_uniform_32(b, uniforms, SLOT(cy1))),
                         r1  = umbra_interval_exact(umbra_uniform_32(b, uniforms, SLOT(r1))),
                         cx2 = umbra_interval_exact(umbra_uniform_32(b, uniforms, SLOT(cx2))),
                         cy2 = umbra_interval_exact(umbra_uniform_32(b, uniforms, SLOT(cy2))),
                         r2  = umbra_interval_exact(umbra_uniform_32(b, uniforms, SLOT(r2)));
    *a = circle_sdf(b, x, y, cx1, cy1, r1);
    *c = circle_sdf(b, x, y, cx2, cy2, r2);
}

static umbra_interval csg_union_build(struct umbra_sdf *s, struct umbra_builder *b,
                                       umbra_ptr32 uniforms,
                                       umbra_interval x, umbra_interval y) {
    struct two_circle_sdf *self = (struct two_circle_sdf *)s;
    umbra_interval a, c;
    two_circle_gather(b, uniforms, self, x, y, &a, &c);
    return umbra_interval_min_f32(b, a, c);
}

static umbra_interval csg_intersect_build(struct umbra_sdf *s, struct umbra_builder *b,
                                           umbra_ptr32 uniforms,
                                           umbra_interval x, umbra_interval y) {
    struct two_circle_sdf *self = (struct two_circle_sdf *)s;
    umbra_interval a, c;
    two_circle_gather(b, uniforms, self, x, y, &a, &c);
    return umbra_interval_max_f32(b, a, c);
}

static umbra_interval csg_difference_build(struct umbra_sdf *s, struct umbra_builder *b,
                                            umbra_ptr32 uniforms,
                                            umbra_interval x, umbra_interval y) {
    struct two_circle_sdf *self = (struct two_circle_sdf *)s;
    umbra_interval a, c;
    two_circle_gather(b, uniforms, self, x, y, &a, &c);
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
    st->disp = umbra_sdf_draw(be, &st->sdf.base,
                                  (struct umbra_sdf_draw_config){.hard_edge = 0},
                                  st->shader, umbra_blend_srcover, fmt);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void csg_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct csg_slide *st = (struct csg_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    two_circle_orbit(&st->sdf, (float)secs, st->w, st->h);
    struct umbra_buf ubuf[] = {
        {.ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w},
        umbra_sdf_uniforms(&st->sdf.base),
        umbra_shader_uniforms(st->shader),
    };
    umbra_sdf_draw_queue(st->disp, l, t, r, b, ubuf);
}

static int csg_get_builders(struct slide *s, struct umbra_fmt fmt,
                            struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct csg_slide *st = (struct csg_slide *)s;
    struct umbra_coverage *adapter = umbra_sdf_coverage(&st->sdf.base, 0);
    out[0] = umbra_draw_builder(adapter, st->shader,
                                umbra_blend_srcover, fmt);
    umbra_coverage_free(adapter);
    return out[0] ? 1 : 0;
}

static void csg_free(struct slide *s) {
    struct csg_slide *st = (struct csg_slide *)s;
    umbra_sdf_draw_free(st->disp);
    umbra_shader_free(st->shader);
    free(st);
}

static struct slide* make_csg(char const *title, float const bg[4], float const color[4],
                              enum csg_op op) {
    umbra_interval (*build)(struct umbra_sdf*, struct umbra_builder*, umbra_ptr32,
                            umbra_interval, umbra_interval) = NULL;
    if (op == CSG_UNION)      { build = csg_union_build; }
    if (op == CSG_INTERSECT)  { build = csg_intersect_build; }
    if (op == CSG_DIFFERENCE) { build = csg_difference_build; }

    struct csg_slide *st = calloc(1, sizeof *st);
    st->op      = op;
    umbra_color const c = {color[0], color[1], color[2], color[3]};
    st->shader  = umbra_shader_solid(c);
    st->sdf.base = (struct umbra_sdf){
        .build          = build
    };

    st->sdf.base.uniforms = UMBRA_UNIFORMS_OF(&st->sdf);
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
    struct umbra_sdf base;
    float            cx, cy, r, w;
};

static umbra_interval ring_build(struct umbra_sdf *s, struct umbra_builder *b,
                                  umbra_ptr32 uniforms,
                                  umbra_interval x, umbra_interval y) {
    struct ring_sdf *self = (struct ring_sdf *)s;
    umbra_interval const cx = umbra_interval_exact(umbra_uniform_32(b, uniforms, SLOT(cx))),
                         cy = umbra_interval_exact(umbra_uniform_32(b, uniforms, SLOT(cy))),
                         r  = umbra_interval_exact(umbra_uniform_32(b, uniforms, SLOT(r))),
                         w  = umbra_interval_exact(umbra_uniform_32(b, uniforms, SLOT(w)));
    return umbra_interval_sub_f32(b, umbra_interval_abs_f32(b, circle_sdf(b, x, y, cx, cy, r)), w);
}

struct ring_slide {
    struct slide base;
    int w, h;

    struct umbra_shader      *shader;
    struct ring_sdf           sdf;

    struct umbra_fmt          fmt;
    struct umbra_sdf_draw    *disp;
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
    st->disp = umbra_sdf_draw(be, &st->sdf.base,
                                  (struct umbra_sdf_draw_config){.hard_edge = 0},
                                  st->shader, umbra_blend_srcover, fmt);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void ring_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct ring_slide *st = (struct ring_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    float ox, oy, r_orbit;
    orbit_compute(&st->sdf.cx, &st->sdf.cy, &ox, &oy, &st->sdf.r, &r_orbit,
                   (float)secs, st->w, st->h);
    st->sdf.w = st->sdf.r * 0.15f;
    struct umbra_buf ubuf[] = {
        {.ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w},
        umbra_sdf_uniforms(&st->sdf.base),
        umbra_shader_uniforms(st->shader),
    };
    umbra_sdf_draw_queue(st->disp, l, t, r, b, ubuf);
}

static int ring_get_builders(struct slide *s, struct umbra_fmt fmt,
                             struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct ring_slide *st = (struct ring_slide *)s;
    struct umbra_coverage *adapter = umbra_sdf_coverage(&st->sdf.base, 0);
    out[0] = umbra_draw_builder(adapter, st->shader, umbra_blend_srcover, fmt);
    umbra_coverage_free(adapter);
    return out[0] ? 1 : 0;
}

static void ring_free(struct slide *s) {
    struct ring_slide *st = (struct ring_slide *)s;
    umbra_sdf_draw_free(st->disp);
    umbra_shader_free(st->shader);
    free(st);
}

SLIDE(slide_sdf_ring) {
    struct ring_slide *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid((umbra_color){1.0f, 0.6f, 0.1f, 1});
    st->sdf.base = (struct umbra_sdf){
        .build          = ring_build
    };

    st->sdf.base.uniforms = UMBRA_UNIFORMS_OF(&st->sdf);
    st->base = (struct slide){
        .title = "SDF Ring (shell)",
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
    struct umbra_sdf base;
    float            cx, cy, hw, hh, r;
    int              :32;
};

static umbra_interval rounded_rect_build(struct umbra_sdf *s, struct umbra_builder *b,
                                          umbra_ptr32 uniforms,
                                          umbra_interval x, umbra_interval y) {
    struct rounded_rect_sdf *self = (struct rounded_rect_sdf *)s;
    umbra_interval const cx = umbra_interval_exact(umbra_uniform_32(b, uniforms, SLOT(cx))),
                         cy = umbra_interval_exact(umbra_uniform_32(b, uniforms, SLOT(cy))),
                         hw = umbra_interval_exact(umbra_uniform_32(b, uniforms, SLOT(hw))),
                         hh = umbra_interval_exact(umbra_uniform_32(b, uniforms, SLOT(hh))),
                         r  = umbra_interval_exact(umbra_uniform_32(b, uniforms, SLOT(r)));
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

    struct umbra_shader      *shader;
    struct rounded_rect_sdf   sdf;

    struct umbra_fmt          fmt;
    struct umbra_sdf_draw    *disp;
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
    st->disp = umbra_sdf_draw(be, &st->sdf.base,
                                  (struct umbra_sdf_draw_config){.hard_edge = 0},
                                  st->shader, umbra_blend_srcover, fmt);
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

    struct umbra_buf ubuf[] = {
        {.ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w},
        umbra_sdf_uniforms(&st->sdf.base),
        umbra_shader_uniforms(st->shader),
    };
    umbra_sdf_draw_queue(st->disp, l, t, r, b, ubuf);
}

static int rounded_rect_get_builders(struct slide *s, struct umbra_fmt fmt,
                                     struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct rounded_rect_slide *st = (struct rounded_rect_slide *)s;
    struct umbra_coverage *adapter = umbra_sdf_coverage(&st->sdf.base, 0);
    out[0] = umbra_draw_builder(adapter, st->shader,
                                umbra_blend_srcover, fmt);
    umbra_coverage_free(adapter);
    return out[0] ? 1 : 0;
}

static void rounded_rect_free(struct slide *s) {
    struct rounded_rect_slide *st = (struct rounded_rect_slide *)s;
    umbra_sdf_draw_free(st->disp);
    umbra_shader_free(st->shader);
    free(st);
}

SLIDE(slide_sdf_rounded_rect) {
    struct rounded_rect_slide *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid((umbra_color){0.1f, 0.5f, 0.9f, 1});
    st->sdf.base = (struct umbra_sdf){
        .build          = rounded_rect_build
    };

    st->sdf.base.uniforms = UMBRA_UNIFORMS_OF(&st->sdf);
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
    struct umbra_sdf base;
    float            p0x, p0y, p1x, p1y, rad;
    int              :32;
};

static umbra_interval capsule_build(struct umbra_sdf *s, struct umbra_builder *b,
                                     umbra_ptr32 uniforms,
                                     umbra_interval x, umbra_interval y) {
    struct capsule_sdf *self = (struct capsule_sdf *)s;
    umbra_interval const p0x = umbra_interval_exact(umbra_uniform_32(b, uniforms, SLOT(p0x))),
                         p0y = umbra_interval_exact(umbra_uniform_32(b, uniforms, SLOT(p0y))),
                         p1x = umbra_interval_exact(umbra_uniform_32(b, uniforms, SLOT(p1x))),
                         p1y = umbra_interval_exact(umbra_uniform_32(b, uniforms, SLOT(p1y))),
                         rad = umbra_interval_exact(umbra_uniform_32(b, uniforms, SLOT(rad)));

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

    struct umbra_shader      *shader;
    struct capsule_sdf        sdf;

    struct umbra_fmt          fmt;
    struct umbra_sdf_draw    *disp;
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
    st->disp = umbra_sdf_draw(be, &st->sdf.base,
                                  (struct umbra_sdf_draw_config){.hard_edge = 0},
                                  st->shader, umbra_blend_srcover, fmt);
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
    struct umbra_buf ubuf[] = {
        {.ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w},
        umbra_sdf_uniforms(&st->sdf.base),
        umbra_shader_uniforms(st->shader),
    };
    umbra_sdf_draw_queue(st->disp, l, t, r, b, ubuf);
}

static int capsule_get_builders(struct slide *s, struct umbra_fmt fmt,
                                struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct capsule_slide *st = (struct capsule_slide *)s;
    struct umbra_coverage *adapter = umbra_sdf_coverage(&st->sdf.base, 0);
    out[0] = umbra_draw_builder(adapter, st->shader, umbra_blend_srcover, fmt);
    umbra_coverage_free(adapter);
    return out[0] ? 1 : 0;
}

static void capsule_free(struct slide *s) {
    struct capsule_slide *st = (struct capsule_slide *)s;
    umbra_sdf_draw_free(st->disp);
    umbra_shader_free(st->shader);
    free(st);
}

SLIDE(slide_sdf_capsule) {
    struct capsule_slide *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid((umbra_color){0.9f, 0.3f, 0.6f, 1});
    st->sdf.base = (struct umbra_sdf){
        .build          = capsule_build
    };

    st->sdf.base.uniforms = UMBRA_UNIFORMS_OF(&st->sdf);
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
    struct umbra_sdf base;
    float            nx, ny, d;
    int              :32;
};

static umbra_interval halfplane_build(struct umbra_sdf *s, struct umbra_builder *b,
                                       umbra_ptr32 uniforms,
                                       umbra_interval x, umbra_interval y) {
    struct halfplane_sdf *self = (struct halfplane_sdf *)s;
    umbra_interval const nx = umbra_interval_exact(umbra_uniform_32(b, uniforms, SLOT(nx))),
                         ny = umbra_interval_exact(umbra_uniform_32(b, uniforms, SLOT(ny))),
                         d  = umbra_interval_exact(umbra_uniform_32(b, uniforms, SLOT(d)));
    return umbra_interval_sub_f32(b,
               umbra_interval_add_f32(b,
                   umbra_interval_mul_f32(b, nx, x),
                   umbra_interval_mul_f32(b, ny, y)),
               d);
}

struct halfplane_slide {
    struct slide base;
    int w, h;

    struct umbra_shader      *shader;
    struct halfplane_sdf      sdf;

    struct umbra_fmt          fmt;
    struct umbra_sdf_draw    *disp;
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
    st->disp = umbra_sdf_draw(be, &st->sdf.base,
                                  (struct umbra_sdf_draw_config){.hard_edge = 0},
                                  st->shader, umbra_blend_srcover, fmt);
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
    struct umbra_buf ubuf[] = {
        {.ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w},
        umbra_sdf_uniforms(&st->sdf.base),
        umbra_shader_uniforms(st->shader),
    };
    umbra_sdf_draw_queue(st->disp, l, t, r, b, ubuf);
}

static int halfplane_get_builders(struct slide *s, struct umbra_fmt fmt,
                                  struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct halfplane_slide *st = (struct halfplane_slide *)s;
    struct umbra_coverage *adapter = umbra_sdf_coverage(&st->sdf.base, 0);
    out[0] = umbra_draw_builder(adapter, st->shader, umbra_blend_srcover, fmt);
    umbra_coverage_free(adapter);
    return out[0] ? 1 : 0;
}

static void halfplane_free(struct slide *s) {
    struct halfplane_slide *st = (struct halfplane_slide *)s;
    umbra_sdf_draw_free(st->disp);
    umbra_shader_free(st->shader);
    free(st);
}

SLIDE(slide_sdf_halfplane) {
    struct halfplane_slide *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid((umbra_color){0.3f, 0.7f, 0.9f, 1});
    st->sdf.base = (struct umbra_sdf){
        .build          = halfplane_build
    };

    st->sdf.base.uniforms = UMBRA_UNIFORMS_OF(&st->sdf);
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
// in the curve data at offsets k+2, k+3 — just not used yet.  The exact
// distance to a quadratic Bezier requires solving a cubic for the closest
// parameter t, which is more ops per curve but would give smooth outlines
// and enable proper filled rendering with winding-number sign.
//
// TODO: add an SDF stroke wrapper (abs(sdf) - width) that can be toggled
// on any SDF slide, like iv2d's 's' key.  This converts fills to outlines
// and outlines to double-stroked outlines.

struct sdf_text_sdf {
    struct umbra_sdf base;
    float            scale_x, scale_y, off_x, off_y;
    int              n_curves, :32;
    struct umbra_buf curves;
};

static umbra_interval sdf_text_build(struct umbra_sdf *s, struct umbra_builder *b,
                                      umbra_ptr32 uniforms,
                                      umbra_interval x, umbra_interval y) {
    struct sdf_text_sdf *self = (struct sdf_text_sdf *)s;
    umbra_ptr32 const data = umbra_deref_ptr32(b, uniforms, SLOT(curves));
    umbra_val32 const n    = umbra_uniform_32(b, uniforms, SLOT(n_curves));

    umbra_interval const sx = umbra_interval_exact(
                                  umbra_uniform_32(b, uniforms, SLOT(scale_x))),
                         sy = umbra_interval_exact(
                                  umbra_uniform_32(b, uniforms, SLOT(scale_y))),
                         ox = umbra_interval_exact(
                                  umbra_uniform_32(b, uniforms, SLOT(off_x))),
                         oy = umbra_interval_exact(
                                  umbra_uniform_32(b, uniforms, SLOT(off_y)));

    umbra_interval const gx = umbra_interval_add_f32(b, umbra_interval_mul_f32(b, x, sx), ox),
                         gy = umbra_interval_add_f32(b, umbra_interval_mul_f32(b, y, sy), oy);

    umbra_imm_f32(b, 0);
    umbra_imm_f32(b, 1);
    umbra_imm_f32(b, 1.5f);
    umbra_imm_i32(b, 1);
    umbra_imm_i32(b, 4);
    umbra_imm_i32(b, 5);
    umbra_imm_i32(b, 6);

    struct umbra_var32 lo_var = umbra_var32(b);
    struct umbra_var32 hi_var = umbra_var32(b);
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
    struct umbra_shader      *shader;
    struct sdf_text_sdf       sdf;

    struct umbra_fmt          fmt;
    struct umbra_sdf_draw    *disp;
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
    st->disp = umbra_sdf_draw(be, &st->sdf.base,
                                  (struct umbra_sdf_draw_config){.hard_edge = 0},
                                  st->shader, umbra_blend_srcover, fmt);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void sdf_text_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    (void)secs;
    struct sdf_text_slide *st = (struct sdf_text_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    struct umbra_buf ubuf[] = {
        {.ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w},
        umbra_sdf_uniforms(&st->sdf.base),
        umbra_shader_uniforms(st->shader),
    };
    umbra_sdf_draw_queue(st->disp, l, t, r, b, ubuf);
}

static int sdf_text_get_builders(struct slide *s, struct umbra_fmt fmt,
                                 struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct sdf_text_slide *st = (struct sdf_text_slide *)s;
    struct umbra_coverage *adapter = umbra_sdf_coverage(&st->sdf.base, 0);
    out[0] = umbra_draw_builder(adapter, st->shader,
                                umbra_blend_srcover, fmt);
    umbra_coverage_free(adapter);
    return out[0] ? 1 : 0;
}

static void sdf_text_free(struct slide *s) {
    struct sdf_text_slide *st = (struct sdf_text_slide *)s;
    umbra_sdf_draw_free(st->disp);
    slug_free(&st->slug);
    umbra_shader_free(st->shader);
    free(st);
}

SLIDE(slide_sdf_text) {
    struct sdf_text_slide *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid((umbra_color){0.95f, 0.9f, 0.8f, 1});
    st->sdf.base = (struct umbra_sdf){
        .build          = sdf_text_build
    };

    st->sdf.base.uniforms = UMBRA_UNIFORMS_OF(&st->sdf);
    st->base = (struct slide){
        .title = "SDF Text (analytic)",
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
    struct umbra_sdf base;
    int              n_sides, :32;
    struct umbra_buf hp;
};

static umbra_interval ngon_build(struct umbra_sdf *s, struct umbra_builder *b,
                                  umbra_ptr32 uniforms,
                                  umbra_interval x, umbra_interval y) {
    struct ngon_sdf *self = (struct ngon_sdf *)s;
    umbra_ptr32 const data = umbra_deref_ptr32(b, uniforms, SLOT(hp));
    umbra_val32 const n    = umbra_uniform_32(b, uniforms, SLOT(n_sides));

    struct umbra_var32 lo_var = umbra_var32(b);
    struct umbra_var32 hi_var = umbra_var32(b);
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

    struct umbra_shader      *shader;
    struct ngon_sdf           sdf;

    struct umbra_fmt          fmt;
    struct umbra_sdf_draw    *disp;
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
    st->disp = umbra_sdf_draw(be, &st->sdf.base,
                                  (struct umbra_sdf_draw_config){.hard_edge = 0},
                                  st->shader, umbra_blend_srcover, fmt);
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
    struct umbra_buf ubuf[] = {
        {.ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w},
        umbra_sdf_uniforms(&st->sdf.base),
        umbra_shader_uniforms(st->shader),
    };
    umbra_sdf_draw_queue(st->disp, l, t, r, b, ubuf);
}

static int ngon_get_builders(struct slide *s, struct umbra_fmt fmt,
                             struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct ngon_slide *st = (struct ngon_slide *)s;
    struct umbra_coverage *adapter = umbra_sdf_coverage(&st->sdf.base, 0);
    out[0] = umbra_draw_builder(adapter, st->shader,
                                umbra_blend_srcover, fmt);
    umbra_coverage_free(adapter);
    return out[0] ? 1 : 0;
}

static void ngon_free(struct slide *s) {
    struct ngon_slide *st = (struct ngon_slide *)s;
    umbra_sdf_draw_free(st->disp);
    free(st->hp_data);
    umbra_shader_free(st->shader);
    free(st);
}

SLIDE(slide_sdf_ngon) {
    struct ngon_slide *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid((umbra_color){0.8f, 0.8f, 0.2f, 1});
    st->hp_data = malloc(3 * NGON_SIDES * sizeof(float));
    st->sdf.base = (struct umbra_sdf){
        .build          = ngon_build
    };

    st->sdf.base.uniforms = UMBRA_UNIFORMS_OF(&st->sdf);
    st->base = (struct slide){
        .title = "SDF Hexagon (n-gon)",
        .bg = {0.08f, 0.10f, 0.14f, 1},
        .init = ngon_init,
        .prepare = ngon_prepare,
        .draw = ngon_draw,
        .free = ngon_free,
        .get_builders = ngon_get_builders,
    };
    return &st->base;
}
