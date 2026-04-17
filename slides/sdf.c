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
    struct orbit     orbit;
    int              circles_off, pad;
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
    struct umbra_draw_layout  lay;
    struct umbra_sdf_draw *disp;
};

static umbra_interval csg_union_build(struct umbra_sdf *s, struct umbra_builder *b,
                                       struct umbra_uniforms_layout *u,
                                       int buf_index,
                                       umbra_interval x, umbra_interval y) {
    struct two_circle_sdf *self = (struct two_circle_sdf *)s;
    self->circles_off = umbra_uniforms_reserve_f32(u, 6);
    umbra_ptr32 const u_ptr = {.ix = buf_index};
    umbra_interval const cx1 = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->circles_off)),
                         cy1 = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->circles_off + 1)),
                         r1  = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->circles_off + 2)),
                         cx2 = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->circles_off + 3)),
                         cy2 = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->circles_off + 4)),
                         r2  = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->circles_off + 5));
    umbra_interval const a = circle_sdf(b, x, y, cx1, cy1, r1),
                         c = circle_sdf(b, x, y, cx2, cy2, r2);
    return umbra_interval_min_f32(b, a, c);
}

static umbra_interval csg_intersect_build(struct umbra_sdf *s, struct umbra_builder *b,
                                           struct umbra_uniforms_layout *u,
                                           int buf_index,
                                           umbra_interval x, umbra_interval y) {
    struct two_circle_sdf *self = (struct two_circle_sdf *)s;
    self->circles_off = umbra_uniforms_reserve_f32(u, 6);
    umbra_ptr32 const u_ptr = {.ix = buf_index};
    umbra_interval const cx1 = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->circles_off)),
                         cy1 = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->circles_off + 1)),
                         r1  = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->circles_off + 2)),
                         cx2 = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->circles_off + 3)),
                         cy2 = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->circles_off + 4)),
                         r2  = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->circles_off + 5));
    umbra_interval const a = circle_sdf(b, x, y, cx1, cy1, r1),
                         c = circle_sdf(b, x, y, cx2, cy2, r2);
    return umbra_interval_max_f32(b, a, c);
}

static umbra_interval csg_difference_build(struct umbra_sdf *s, struct umbra_builder *b,
                                            struct umbra_uniforms_layout *u,
                                            int buf_index,
                                            umbra_interval x, umbra_interval y) {
    struct two_circle_sdf *self = (struct two_circle_sdf *)s;
    self->circles_off = umbra_uniforms_reserve_f32(u, 6);
    umbra_ptr32 const u_ptr = {.ix = buf_index};
    umbra_interval const cx1 = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->circles_off)),
                         cy1 = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->circles_off + 1)),
                         r1  = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->circles_off + 2)),
                         cx2 = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->circles_off + 3)),
                         cy2 = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->circles_off + 4)),
                         r2  = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->circles_off + 5));
    umbra_interval const a = circle_sdf(b, x, y, cx1, cy1, r1),
                         c = circle_sdf(b, x, y, cx2, cy2, r2);
    umbra_interval const neg_c = umbra_interval_sub_f32(b, umbra_interval_exact(umbra_imm_f32(b, 0)), c);
    return umbra_interval_max_f32(b, a, neg_c);
}

static void two_circle_fill(struct umbra_sdf const *s, void *uniforms) {
    struct two_circle_sdf const *self = (struct two_circle_sdf const *)s;
    float const vals[6] = {self->orbit.cx, self->orbit.cy, self->orbit.r_center,
                           self->orbit.ox, self->orbit.oy, self->orbit.r_orbit};
    umbra_uniforms_fill_f32(uniforms, self->circles_off, vals, 6);
}

static void csg_init(struct slide *s, int w, int h) {
    struct csg_slide *st = (struct csg_slide *)s;
    st->w = w;
    st->h = h;
}

static void csg_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct csg_slide *st = (struct csg_slide *)s;
    umbra_sdf_draw_free(st->disp);
    free(st->lay.uniforms);
    st->fmt  = fmt;
    st->disp = umbra_sdf_draw(be, &st->sdf.base,
                                  (struct umbra_sdf_draw_config){.hard_edge = 0},
                                  st->shader, umbra_blend_srcover, fmt, &st->lay);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void csg_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct csg_slide *st = (struct csg_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    orbit_update(&st->sdf.orbit, (float)secs, st->w, st->h);
    umbra_sdf_draw_fill(&st->lay, &st->sdf.base, st->shader);
    struct umbra_buf ubuf[] = {
        {.ptr = st->lay.uniforms, .count = st->lay.uni.slots},
        {.ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w},
    };
    umbra_sdf_draw_queue(st->disp, l, t, r, b, ubuf);
}

static int csg_get_builders(struct slide *s, struct umbra_fmt fmt,
                            struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct csg_slide *st = (struct csg_slide *)s;
    struct umbra_coverage *adapter = umbra_sdf_coverage(&st->sdf.base, 0);
    out[0] = umbra_draw_builder(adapter, st->shader,
                                umbra_blend_srcover, fmt, NULL);
    umbra_coverage_free(adapter);
    return out[0] ? 1 : 0;
}

static void csg_free(struct slide *s) {
    struct csg_slide *st = (struct csg_slide *)s;
    umbra_sdf_draw_free(st->disp);
    free(st->lay.uniforms);
    umbra_shader_free(st->shader);
    free(st);
}

static struct slide* make_csg(char const *title, float const bg[4], float const color[4],
                              enum csg_op op) {
    umbra_interval (*build)(struct umbra_sdf*, struct umbra_builder*,
                            struct umbra_uniforms_layout*, int,
                            umbra_interval, umbra_interval) = NULL;
    if (op == CSG_UNION)      { build = csg_union_build; }
    if (op == CSG_INTERSECT)  { build = csg_intersect_build; }
    if (op == CSG_DIFFERENCE) { build = csg_difference_build; }

    struct csg_slide *st = calloc(1, sizeof *st);
    st->op      = op;
    umbra_color const c = {color[0], color[1], color[2], color[3]};
    st->shader  = umbra_shader_solid(c);
    st->sdf.base = (struct umbra_sdf){.build = build, .fill = two_circle_fill};
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
    struct orbit     orbit;
    int              ring_off, pad;
};

static umbra_interval ring_build(struct umbra_sdf *s, struct umbra_builder *b,
                                  struct umbra_uniforms_layout *u,
                                  int buf_index,
                                  umbra_interval x, umbra_interval y) {
    struct ring_sdf *self = (struct ring_sdf *)s;
    self->ring_off = umbra_uniforms_reserve_f32(u, 4);
    umbra_ptr32 const u_ptr = {.ix = buf_index};
    umbra_interval const cx = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->ring_off)),
                         cy = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->ring_off + 1)),
                         r  = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->ring_off + 2)),
                         w  = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->ring_off + 3));
    return umbra_interval_sub_f32(b, umbra_interval_abs_f32(b, circle_sdf(b, x, y, cx, cy, r)), w);
}

static void ring_fill(struct umbra_sdf const *s, void *uniforms) {
    struct ring_sdf const *self = (struct ring_sdf const *)s;
    float const vals[4] = {self->orbit.cx, self->orbit.cy,
                           self->orbit.r_center, self->orbit.r_center * 0.15f};
    umbra_uniforms_fill_f32(uniforms, self->ring_off, vals, 4);
}

SLIDE(slide_sdf_ring) {
    struct csg_slide *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid((umbra_color){1.0f, 0.6f, 0.1f, 1});
    struct ring_sdf *ring = (struct ring_sdf *)&st->sdf;
    ring->base = (struct umbra_sdf){.build = ring_build, .fill = ring_fill};
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

struct rounded_rect_sdf {
    struct umbra_sdf base;
    float cx, cy, hw, hh, r;
    int   rrect_off;
};

static umbra_interval rounded_rect_build(struct umbra_sdf *s, struct umbra_builder *b,
                                          struct umbra_uniforms_layout *u,
                                          int buf_index,
                                          umbra_interval x, umbra_interval y) {
    struct rounded_rect_sdf *self = (struct rounded_rect_sdf *)s;
    self->rrect_off = umbra_uniforms_reserve_f32(u, 5);
    umbra_ptr32 const u_ptr = {.ix = buf_index};
    umbra_interval const cx = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->rrect_off)),
                         cy = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->rrect_off + 1)),
                         hw = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->rrect_off + 2)),
                         hh = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->rrect_off + 3)),
                         r  = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->rrect_off + 4));
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

static void rounded_rect_fill(struct umbra_sdf const *s, void *uniforms) {
    struct rounded_rect_sdf const *self = (struct rounded_rect_sdf const *)s;
    float const vals[5] = {self->cx, self->cy, self->hw, self->hh, self->r};
    umbra_uniforms_fill_f32(uniforms, self->rrect_off, vals, 5);
}

struct rounded_rect_slide {
    struct slide base;
    int w, h;

    struct umbra_shader      *shader;
    struct rounded_rect_sdf   sdf;

    struct umbra_fmt          fmt;
    struct umbra_draw_layout  lay;
    struct umbra_sdf_draw *disp;
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
    free(st->lay.uniforms);
    st->fmt  = fmt;
    st->disp = umbra_sdf_draw(be, &st->sdf.base,
                                  (struct umbra_sdf_draw_config){.hard_edge = 0},
                                  st->shader, umbra_blend_srcover, fmt, &st->lay);
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

    umbra_sdf_draw_fill(&st->lay, &st->sdf.base, st->shader);
    struct umbra_buf ubuf[] = {
        {.ptr = st->lay.uniforms, .count = st->lay.uni.slots},
        {.ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w},
    };
    umbra_sdf_draw_queue(st->disp, l, t, r, b, ubuf);
}

static int rounded_rect_get_builders(struct slide *s, struct umbra_fmt fmt,
                                     struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct rounded_rect_slide *st = (struct rounded_rect_slide *)s;
    struct umbra_coverage *adapter = umbra_sdf_coverage(&st->sdf.base, 0);
    out[0] = umbra_draw_builder(adapter, st->shader,
                                umbra_blend_srcover, fmt, NULL);
    umbra_coverage_free(adapter);
    return out[0] ? 1 : 0;
}

static void rounded_rect_free(struct slide *s) {
    struct rounded_rect_slide *st = (struct rounded_rect_slide *)s;
    umbra_sdf_draw_free(st->disp);
    free(st->lay.uniforms);
    umbra_shader_free(st->shader);
    free(st);
}

SLIDE(slide_sdf_rounded_rect) {
    struct rounded_rect_slide *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid((umbra_color){0.1f, 0.5f, 0.9f, 1});
    st->sdf.base = (struct umbra_sdf){.build = rounded_rect_build, .fill = rounded_rect_fill};
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
    struct orbit     orbit;
    int              capsule_off, pad;
};

static umbra_interval capsule_build(struct umbra_sdf *s, struct umbra_builder *b,
                                     struct umbra_uniforms_layout *u,
                                     int buf_index,
                                     umbra_interval x, umbra_interval y) {
    struct capsule_sdf *self = (struct capsule_sdf *)s;
    self->capsule_off = umbra_uniforms_reserve_f32(u, 5);
    umbra_ptr32 const u_ptr = {.ix = buf_index};
    umbra_interval const p0x = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->capsule_off)),
                         p0y = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->capsule_off + 1)),
                         p1x = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->capsule_off + 2)),
                         p1y = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->capsule_off + 3)),
                         rad = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->capsule_off + 4));

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

static void capsule_fill(struct umbra_sdf const *s, void *uniforms) {
    struct capsule_sdf const *self = (struct capsule_sdf const *)s;
    float const r = self->orbit.r_center * 0.15f;
    float const vals[5] = {self->orbit.cx, self->orbit.cy,
                           self->orbit.ox, self->orbit.oy, r};
    umbra_uniforms_fill_f32(uniforms, self->capsule_off, vals, 5);
}

SLIDE(slide_sdf_capsule) {
    struct csg_slide *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid((umbra_color){0.9f, 0.3f, 0.6f, 1});
    struct capsule_sdf *cap = (struct capsule_sdf *)&st->sdf;
    cap->base = (struct umbra_sdf){.build = capsule_build, .fill = capsule_fill};
    st->base = (struct slide){
        .title = "SDF Capsule",
        .bg = {0.08f, 0.10f, 0.14f, 1},
        .init = csg_init,
        .prepare = csg_prepare,
        .draw = csg_draw,
        .free = csg_free,
        .get_builders = csg_get_builders,
    };
    return &st->base;
}

struct halfplane_sdf {
    struct umbra_sdf base;
    struct orbit     orbit;
    int              halfplane_off, pad;
};

static umbra_interval halfplane_build(struct umbra_sdf *s, struct umbra_builder *b,
                                       struct umbra_uniforms_layout *u,
                                       int buf_index,
                                       umbra_interval x, umbra_interval y) {
    struct halfplane_sdf *self = (struct halfplane_sdf *)s;
    self->halfplane_off = umbra_uniforms_reserve_f32(u, 3);
    umbra_ptr32 const u_ptr = {.ix = buf_index};
    umbra_interval const nx = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->halfplane_off)),
                         ny = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->halfplane_off + 1)),
                         d  = umbra_interval_exact(umbra_uniform_32(b, u_ptr, self->halfplane_off + 2));
    return umbra_interval_sub_f32(b,
               umbra_interval_add_f32(b,
                   umbra_interval_mul_f32(b, nx, x),
                   umbra_interval_mul_f32(b, ny, y)),
               d);
}

static void halfplane_fill(struct umbra_sdf const *s, void *uniforms) {
    struct halfplane_sdf const *self = (struct halfplane_sdf const *)s;
    float const dx = self->orbit.ox - self->orbit.cx,
                dy = self->orbit.oy - self->orbit.cy;
    float const len = sqrtf(dx * dx + dy * dy);
    float const nx = len > 0 ? dy / len : 0,
                ny = len > 0 ? -dx / len : 1;
    float const d = nx * self->orbit.cx + ny * self->orbit.cy;
    float const vals[3] = {nx, ny, d};
    umbra_uniforms_fill_f32(uniforms, self->halfplane_off, vals, 3);
}

SLIDE(slide_sdf_halfplane) {
    struct csg_slide *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid((umbra_color){0.3f, 0.7f, 0.9f, 1});
    struct halfplane_sdf *hp = (struct halfplane_sdf *)&st->sdf;
    hp->base = (struct umbra_sdf){.build = halfplane_build, .fill = halfplane_fill};
    st->base = (struct slide){
        .title = "SDF Halfplane",
        .bg = {0.08f, 0.10f, 0.14f, 1},
        .init = csg_init,
        .prepare = csg_prepare,
        .draw = csg_draw,
        .free = csg_free,
        .get_builders = csg_get_builders,
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
    struct umbra_sdf    base;
    struct slug_curves  curves;
    float               scale_x, scale_y, off_x, off_y;
    int                 curves_off, params_off;
    int                 w, h;
};

static umbra_interval sdf_text_build(struct umbra_sdf *s, struct umbra_builder *b,
                                      struct umbra_uniforms_layout *u,
                                      int buf_index,
                                      umbra_interval x, umbra_interval y) {
    struct sdf_text_sdf *self = (struct sdf_text_sdf *)s;
    self->curves_off = umbra_uniforms_reserve_ptr(u);
    self->params_off = umbra_uniforms_reserve_f32(u, 5);
    umbra_ptr32 const u_ptr = {.ix = buf_index};
    umbra_ptr32 const data = umbra_deref_ptr32(b, u_ptr, self->curves_off);
    umbra_val32 const n    = umbra_uniform_32(b, u_ptr, self->params_off);

    umbra_interval const sx = umbra_interval_exact(
                                  umbra_uniform_32(b, u_ptr, self->params_off + 1)),
                         sy = umbra_interval_exact(
                                  umbra_uniform_32(b, u_ptr, self->params_off + 2)),
                         ox = umbra_interval_exact(
                                  umbra_uniform_32(b, u_ptr, self->params_off + 3)),
                         oy = umbra_interval_exact(
                                  umbra_uniform_32(b, u_ptr, self->params_off + 4));

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

static void sdf_text_fill(struct umbra_sdf const *s, void *uniforms) {
    struct sdf_text_sdf const *self = (struct sdf_text_sdf const *)s;
    umbra_uniforms_fill_ptr(uniforms, self->curves_off,
                            (struct umbra_buf){.ptr = self->curves.data,
                                               .count = self->curves.count * 6});
    umbra_uniforms_fill_i32(uniforms, self->params_off, &self->curves.count, 1);
    float const vals[4] = {self->scale_x, self->scale_y, self->off_x, self->off_y};
    umbra_uniforms_fill_f32(uniforms, self->params_off + 1, vals, 4);
}

struct sdf_text_slide {
    struct slide base;
    int w, h;

    struct umbra_shader      *shader;
    struct sdf_text_sdf       sdf;

    struct umbra_fmt          fmt;
    struct umbra_draw_layout  lay;
    struct umbra_sdf_draw *disp;
};

static void sdf_text_init(struct slide *s, int w, int h) {
    struct sdf_text_slide *st = (struct sdf_text_slide *)s;
    st->w = w;
    st->h = h;
    st->sdf.curves = slug_extract("Hamburgefons", (float)h * 0.4f);

    float const gw = st->sdf.curves.w,
                gh = st->sdf.curves.h;
    float const sx = gw / (float)w,
                sy = gh / (float)h;
    float const scale = sx > sy ? sx : sy;
    float const pad_x = ((float)w * scale - gw) * 0.5f,
                pad_y = ((float)h * scale - gh) * 0.5f;
    st->sdf.scale_x = scale;
    st->sdf.scale_y = scale;
    st->sdf.off_x   = -pad_x;
    st->sdf.off_y   = -pad_y;
    st->sdf.w = w;
    st->sdf.h = h;
}

static void sdf_text_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct sdf_text_slide *st = (struct sdf_text_slide *)s;
    umbra_sdf_draw_free(st->disp);
    free(st->lay.uniforms);
    st->fmt  = fmt;
    st->disp = umbra_sdf_draw(be, &st->sdf.base,
                                  (struct umbra_sdf_draw_config){.hard_edge = 0},
                                  st->shader, umbra_blend_srcover, fmt, &st->lay);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void sdf_text_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    (void)secs;
    struct sdf_text_slide *st = (struct sdf_text_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    umbra_sdf_draw_fill(&st->lay, &st->sdf.base, st->shader);
    struct umbra_buf ubuf[] = {
        {.ptr = st->lay.uniforms, .count = st->lay.uni.slots},
        {.ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w},
    };
    umbra_sdf_draw_queue(st->disp, l, t, r, b, ubuf);
}

static int sdf_text_get_builders(struct slide *s, struct umbra_fmt fmt,
                                 struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct sdf_text_slide *st = (struct sdf_text_slide *)s;
    struct umbra_coverage *adapter = umbra_sdf_coverage(&st->sdf.base, 0);
    out[0] = umbra_draw_builder(adapter, st->shader,
                                umbra_blend_srcover, fmt, NULL);
    umbra_coverage_free(adapter);
    return out[0] ? 1 : 0;
}

static void sdf_text_free(struct slide *s) {
    struct sdf_text_slide *st = (struct sdf_text_slide *)s;
    umbra_sdf_draw_free(st->disp);
    free(st->lay.uniforms);
    slug_free(&st->sdf.curves);
    umbra_shader_free(st->shader);
    free(st);
}

SLIDE(slide_sdf_text) {
    struct sdf_text_slide *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid((umbra_color){0.95f, 0.9f, 0.8f, 1});
    st->sdf.base = (struct umbra_sdf){.build = sdf_text_build, .fill = sdf_text_fill};
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
    float cx, cy, r, angle;
    float *hp_data;
    int   hp_off, count_off;
};

static umbra_interval ngon_build(struct umbra_sdf *s, struct umbra_builder *b,
                                  struct umbra_uniforms_layout *u,
                                  int buf_index,
                                  umbra_interval x, umbra_interval y) {
    struct ngon_sdf *self = (struct ngon_sdf *)s;
    self->hp_off    = umbra_uniforms_reserve_ptr(u);
    self->count_off = umbra_uniforms_reserve_f32(u, 1);
    umbra_ptr32 const u_ptr = {.ix = buf_index};
    umbra_ptr32 const data = umbra_deref_ptr32(b, u_ptr, self->hp_off);
    umbra_val32 const n    = umbra_uniform_32(b, u_ptr, self->count_off);

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

static void ngon_fill(struct umbra_sdf const *s, void *uniforms) {
    struct ngon_sdf const *self = (struct ngon_sdf const *)s;
    for (int i = 0; i < NGON_SIDES; i++) {
        float const a = self->angle + (float)i * (2.0f * 3.14159265f / NGON_SIDES);
        float const vx = self->cx + self->r * cosf(a),
                    vy = self->cy + self->r * sinf(a);
        float const a2 = self->angle + (float)(i + 1) * (2.0f * 3.14159265f / NGON_SIDES);
        float const wx = self->cx + self->r * cosf(a2),
                    wy = self->cy + self->r * sinf(a2);
        float const ex = wx - vx,
                    ey = wy - vy;
        float const len = sqrtf(ex * ex + ey * ey);
        float const nx = len > 0 ? ey / len : 0,
                    ny = len > 0 ? -ex / len : 1;
        self->hp_data[i * 3]     = nx;
        self->hp_data[i * 3 + 1] = ny;
        self->hp_data[i * 3 + 2] = nx * vx + ny * vy;
    }
    int const n = NGON_SIDES;
    umbra_uniforms_fill_ptr(uniforms, self->hp_off,
                            (struct umbra_buf){.ptr = self->hp_data, .count = 3 * NGON_SIDES});
    umbra_uniforms_fill_i32(uniforms, self->count_off, &n, 1);
}

struct ngon_slide {
    struct slide base;
    int w, h;

    struct umbra_shader      *shader;
    struct ngon_sdf           sdf;

    struct umbra_fmt          fmt;
    struct umbra_draw_layout  lay;
    struct umbra_sdf_draw *disp;
};

static void ngon_init(struct slide *s, int w, int h) {
    struct ngon_slide *st = (struct ngon_slide *)s;
    st->w = w;
    st->h = h;
}

static void ngon_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct ngon_slide *st = (struct ngon_slide *)s;
    umbra_sdf_draw_free(st->disp);
    free(st->lay.uniforms);
    st->fmt  = fmt;
    st->disp = umbra_sdf_draw(be, &st->sdf.base,
                                  (struct umbra_sdf_draw_config){.hard_edge = 0},
                                  st->shader, umbra_blend_srcover, fmt, &st->lay);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void ngon_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct ngon_slide *st = (struct ngon_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    st->sdf.cx    = (float)st->w * 0.5f;
    st->sdf.cy    = (float)st->h * 0.5f;
    st->sdf.r     = (float)(st->w < st->h ? st->w : st->h) * 0.3f;
    st->sdf.angle = (float)secs;
    umbra_sdf_draw_fill(&st->lay, &st->sdf.base, st->shader);
    struct umbra_buf ubuf[] = {
        {.ptr = st->lay.uniforms, .count = st->lay.uni.slots},
        {.ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w},
    };
    umbra_sdf_draw_queue(st->disp, l, t, r, b, ubuf);
}

static int ngon_get_builders(struct slide *s, struct umbra_fmt fmt,
                             struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct ngon_slide *st = (struct ngon_slide *)s;
    struct umbra_coverage *adapter = umbra_sdf_coverage(&st->sdf.base, 0);
    out[0] = umbra_draw_builder(adapter, st->shader,
                                umbra_blend_srcover, fmt, NULL);
    umbra_coverage_free(adapter);
    return out[0] ? 1 : 0;
}

static void ngon_free(struct slide *s) {
    struct ngon_slide *st = (struct ngon_slide *)s;
    umbra_sdf_draw_free(st->disp);
    free(st->lay.uniforms);
    free(st->sdf.hp_data);
    umbra_shader_free(st->shader);
    free(st);
}

SLIDE(slide_sdf_ngon) {
    struct ngon_slide *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid((umbra_color){0.8f, 0.8f, 0.2f, 1});
    st->sdf.hp_data = malloc(3 * NGON_SIDES * sizeof(float));
    st->sdf.base = (struct umbra_sdf){.build = ngon_build, .fill = ngon_fill};
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
