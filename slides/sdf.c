// SDF shape and CSG slides.  Each demonstrates an SDF primitive or combinator
// using umbra_interval_* helpers and umbra_sdf_dispatch.
//
// Animation follows iv2d's convention: an orbit point rotates around a center.
//
// TODO: adapt iv2d's SDF text rendering as a follow-up.

#include "slide.h"
#include "../include/umbra_interval.h"
#include <math.h>
#include <stdlib.h>

// --- Shared animation: orbit a point around a center ---

struct orbit {
    float cx, cy, ox, oy, r_center, r_orbit;
};

static void orbit_update(struct orbit *o, float t, int w, int h) {
    o->cx = (float)w * 0.5f;
    o->cy = (float)h * 0.5f;
    o->r_center = (float)(w < h ? w : h) * 0.18f;
    o->r_orbit  = (float)(w < h ? w : h) * 0.12f;
    float const dist = (float)(w < h ? w : h) * 0.22f;
    o->ox = o->cx + dist * cosf(t);
    o->oy = o->cy + dist * sinf(t);
}

// --- Circle SDF (reusable, separate from the circle slide) ---

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

// --- Two-circle SDF base (shared by union/intersection/difference) ---

struct two_circle_sdf {
    struct umbra_sdf base;
    struct orbit     orbit;
    int              off_, pad_;
};

enum csg_op { CSG_UNION, CSG_INTERSECT, CSG_DIFFERENCE };

struct csg_slide {
    struct slide base;
    int w, h;
    enum csg_op op;
    int pad_;

    struct umbra_shader_solid shader;
    struct two_circle_sdf     sdf;

    struct umbra_fmt          fmt;
    struct umbra_draw_layout  lay;
    struct umbra_sdf_dispatch *disp;
};

static umbra_interval csg_union_build_(struct umbra_sdf *s, struct umbra_builder *b,
                                       struct umbra_uniforms_layout *u,
                                       umbra_interval x, umbra_interval y) {
    struct two_circle_sdf *self = (struct two_circle_sdf *)s;
    self->off_ = umbra_uniforms_reserve_f32(u, 6);
    umbra_interval const cx1 = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_),
                         cy1 = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_ + 1),
                         r1  = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_ + 2),
                         cx2 = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_ + 3),
                         cy2 = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_ + 4),
                         r2  = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_ + 5);
    umbra_interval const a = circle_sdf(b, x, y, cx1, cy1, r1),
                         c = circle_sdf(b, x, y, cx2, cy2, r2);
    return umbra_interval_min_f32(b, a, c);
}

static umbra_interval csg_intersect_build_(struct umbra_sdf *s, struct umbra_builder *b,
                                           struct umbra_uniforms_layout *u,
                                           umbra_interval x, umbra_interval y) {
    struct two_circle_sdf *self = (struct two_circle_sdf *)s;
    self->off_ = umbra_uniforms_reserve_f32(u, 6);
    umbra_interval const cx1 = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_),
                         cy1 = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_ + 1),
                         r1  = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_ + 2),
                         cx2 = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_ + 3),
                         cy2 = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_ + 4),
                         r2  = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_ + 5);
    umbra_interval const a = circle_sdf(b, x, y, cx1, cy1, r1),
                         c = circle_sdf(b, x, y, cx2, cy2, r2);
    return umbra_interval_max_f32(b, a, c);
}

static umbra_interval csg_difference_build_(struct umbra_sdf *s, struct umbra_builder *b,
                                            struct umbra_uniforms_layout *u,
                                            umbra_interval x, umbra_interval y) {
    struct two_circle_sdf *self = (struct two_circle_sdf *)s;
    self->off_ = umbra_uniforms_reserve_f32(u, 6);
    umbra_interval const cx1 = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_),
                         cy1 = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_ + 1),
                         r1  = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_ + 2),
                         cx2 = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_ + 3),
                         cy2 = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_ + 4),
                         r2  = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_ + 5);
    umbra_interval const a = circle_sdf(b, x, y, cx1, cy1, r1),
                         c = circle_sdf(b, x, y, cx2, cy2, r2);
    umbra_interval const neg_c = umbra_interval_sub_f32(b, umbra_interval_exact(b, 0), c);
    return umbra_interval_max_f32(b, a, neg_c);
}

static void two_circle_fill_(struct umbra_sdf const *s, void *uniforms) {
    struct two_circle_sdf const *self = (struct two_circle_sdf const *)s;
    float const vals[6] = {self->orbit.cx, self->orbit.cy, self->orbit.r_center,
                           self->orbit.ox, self->orbit.oy, self->orbit.r_orbit};
    umbra_uniforms_fill_f32(uniforms, self->off_, vals, 6);
}

static void csg_init(struct slide *s, int w, int h) {
    struct csg_slide *st = (struct csg_slide *)s;
    st->w = w;
    st->h = h;
}

static void csg_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct csg_slide *st = (struct csg_slide *)s;
    umbra_sdf_dispatch_free(st->disp);
    free(st->lay.uniforms);
    st->fmt  = fmt;
    st->disp = umbra_sdf_dispatch(be, &st->sdf.base,
                                  (struct umbra_sdf_dispatch_config){.hard_edge = 0},
                                  &st->shader.base, umbra_blend_srcover, fmt, &st->lay);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void csg_draw(struct slide *s, int frame, int l, int t, int r, int b, void *buf) {
    struct csg_slide *st = (struct csg_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    orbit_update(&st->sdf.orbit, (float)frame * 0.02f, st->w, st->h);
    umbra_sdf_dispatch_fill(&st->lay, &st->sdf.base, &st->shader.base);
    struct umbra_buf ubuf[] = {
        {.ptr = st->lay.uniforms, .count = st->lay.uni.slots},
        {.ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w},
    };
    umbra_sdf_dispatch_queue(st->disp, l, t, r, b, ubuf);
}

static int csg_get_builders(struct slide *s, struct umbra_fmt fmt,
                            struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct csg_slide *st = (struct csg_slide *)s;
    struct umbra_sdf_coverage adapter = umbra_sdf_coverage(&st->sdf.base, 0);
    out[0] = umbra_draw_builder(&st->shader.base, &adapter.base,
                                umbra_blend_srcover, fmt, NULL);
    return out[0] ? 1 : 0;
}

static void csg_free(struct slide *s) {
    struct csg_slide *st = (struct csg_slide *)s;
    umbra_sdf_dispatch_free(st->disp);
    free(st->lay.uniforms);
    free(st);
}

static struct slide* make_csg(char const *title, float const bg[4], float const color[4],
                              enum csg_op op) {
    umbra_interval (*build)(struct umbra_sdf*, struct umbra_builder*,
                            struct umbra_uniforms_layout*, umbra_interval, umbra_interval) = NULL;
    if (op == CSG_UNION)      { build = csg_union_build_; }
    if (op == CSG_INTERSECT)  { build = csg_intersect_build_; }
    if (op == CSG_DIFFERENCE) { build = csg_difference_build_; }

    struct csg_slide *st = calloc(1, sizeof *st);
    st->op      = op;
    st->shader  = umbra_shader_solid(color);
    st->sdf.base = (struct umbra_sdf){.build = build, .fill = two_circle_fill_};
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

// --- Ring (shell of a circle): abs(circle) - width ---

struct ring_sdf {
    struct umbra_sdf base;
    struct orbit     orbit;
    int              off_, pad_;
};

static umbra_interval ring_build_(struct umbra_sdf *s, struct umbra_builder *b,
                                  struct umbra_uniforms_layout *u,
                                  umbra_interval x, umbra_interval y) {
    struct ring_sdf *self = (struct ring_sdf *)s;
    self->off_ = umbra_uniforms_reserve_f32(u, 4);
    umbra_interval const cx = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_),
                         cy = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_ + 1),
                         r  = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_ + 2),
                         w  = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_ + 3);
    return umbra_interval_sub_f32(b, umbra_interval_abs_f32(b, circle_sdf(b, x, y, cx, cy, r)), w);
}

static void ring_fill_(struct umbra_sdf const *s, void *uniforms) {
    struct ring_sdf const *self = (struct ring_sdf const *)s;
    float const vals[4] = {self->orbit.cx, self->orbit.cy,
                           self->orbit.r_center, self->orbit.r_center * 0.15f};
    umbra_uniforms_fill_f32(uniforms, self->off_, vals, 4);
}

SLIDE(slide_sdf_ring) {
    struct csg_slide *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid((float[]){1.0f, 0.6f, 0.1f, 1});
    struct ring_sdf *ring = (struct ring_sdf *)&st->sdf;
    ring->base = (struct umbra_sdf){.build = ring_build_, .fill = ring_fill_};
    st->base = (struct slide){
        .title = "SDF Ring (shell)",
        .bg = {0.08f, 0.10f, 0.14f, 1},
        .init = csg_init,
        .prepare = csg_prepare,
        .draw = csg_draw,
        .free = csg_free,
        .get_builders = csg_get_builders,
    };
    return &st->base;
}

// --- Rounded rect: max(|x-cx|-hw, |y-cy|-hh) - r ---

struct rounded_rect_sdf {
    struct umbra_sdf base;
    float cx, cy, hw, hh, r;
    int   off_;
};

static umbra_interval rounded_rect_build_(struct umbra_sdf *s, struct umbra_builder *b,
                                          struct umbra_uniforms_layout *u,
                                          umbra_interval x, umbra_interval y) {
    struct rounded_rect_sdf *self = (struct rounded_rect_sdf *)s;
    self->off_ = umbra_uniforms_reserve_f32(u, 5);
    umbra_interval const cx = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_),
                         cy = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_ + 1),
                         hw = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_ + 2),
                         hh = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_ + 3),
                         r  = umbra_interval_uniform(b, (umbra_ptr32){0}, self->off_ + 4);
    umbra_interval const zero = umbra_interval_exact(b, 0);
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

static void rounded_rect_fill_(struct umbra_sdf const *s, void *uniforms) {
    struct rounded_rect_sdf const *self = (struct rounded_rect_sdf const *)s;
    float const vals[5] = {self->cx, self->cy, self->hw, self->hh, self->r};
    umbra_uniforms_fill_f32(uniforms, self->off_, vals, 5);
}

struct rounded_rect_slide {
    struct slide base;
    int w, h;

    struct umbra_shader_solid shader;
    struct rounded_rect_sdf   sdf;

    struct umbra_fmt          fmt;
    struct umbra_draw_layout  lay;
    struct umbra_sdf_dispatch *disp;
};

static void rounded_rect_init(struct slide *s, int w, int h) {
    struct rounded_rect_slide *st = (struct rounded_rect_slide *)s;
    st->w = w;
    st->h = h;
}

static void rounded_rect_prepare(struct slide *s, struct umbra_backend *be,
                                 struct umbra_fmt fmt) {
    struct rounded_rect_slide *st = (struct rounded_rect_slide *)s;
    umbra_sdf_dispatch_free(st->disp);
    free(st->lay.uniforms);
    st->fmt  = fmt;
    st->disp = umbra_sdf_dispatch(be, &st->sdf.base,
                                  (struct umbra_sdf_dispatch_config){.hard_edge = 0},
                                  &st->shader.base, umbra_blend_srcover, fmt, &st->lay);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void rounded_rect_draw(struct slide *s, int frame, int l, int t, int r, int b,
                               void *buf) {
    struct rounded_rect_slide *st = (struct rounded_rect_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);

    float const period = 200.0f;
    float const anim = sinf((float)frame * 0.03f) * 0.5f + 0.5f;
    (void)period;
    st->sdf.cx = (float)st->w * 0.5f;
    st->sdf.cy = (float)st->h * 0.5f;
    st->sdf.hw = (float)(st->w < st->h ? st->w : st->h) * (0.15f + 0.1f * anim);
    st->sdf.hh = (float)(st->w < st->h ? st->w : st->h) * (0.10f + 0.08f * anim);
    st->sdf.r  = (float)(st->w < st->h ? st->w : st->h) * 0.04f;

    umbra_sdf_dispatch_fill(&st->lay, &st->sdf.base, &st->shader.base);
    struct umbra_buf ubuf[] = {
        {.ptr = st->lay.uniforms, .count = st->lay.uni.slots},
        {.ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w},
    };
    umbra_sdf_dispatch_queue(st->disp, l, t, r, b, ubuf);
}

static int rounded_rect_get_builders(struct slide *s, struct umbra_fmt fmt,
                                     struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct rounded_rect_slide *st = (struct rounded_rect_slide *)s;
    struct umbra_sdf_coverage adapter = umbra_sdf_coverage(&st->sdf.base, 0);
    out[0] = umbra_draw_builder(&st->shader.base, &adapter.base,
                                umbra_blend_srcover, fmt, NULL);
    return out[0] ? 1 : 0;
}

static void rounded_rect_free(struct slide *s) {
    struct rounded_rect_slide *st = (struct rounded_rect_slide *)s;
    umbra_sdf_dispatch_free(st->disp);
    free(st->lay.uniforms);
    free(st);
}

SLIDE(slide_sdf_rounded_rect) {
    struct rounded_rect_slide *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid((float[]){0.1f, 0.5f, 0.9f, 1});
    st->sdf.base = (struct umbra_sdf){.build = rounded_rect_build_, .fill = rounded_rect_fill_};
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
