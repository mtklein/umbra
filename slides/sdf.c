#include "slide.h"
#include "slug.h"
#include "../include/umbra_interval.h"
#include <math.h>
#include <stdint.h>
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

// Shared state for every SDF slide in this file.  Each slide embeds this as
// its first field so (struct slide *) casts hit sdf_common.base.  Slides
// fill base.sdf_fn/base.sdf_ctx/color in their ctor; the shared
// build_sdf_draw hook below feeds those to umbra_build_sdf_draw.
struct sdf_common {
    struct slide base;
    umbra_color  color;
};

static void sdf_common_build_draw(struct slide *s,
                                  struct umbra_builder *b,
                                  umbra_ptr dst_ptr, struct umbra_fmt fmt,
                                  umbra_val32 x, umbra_val32 y) {
    struct sdf_common *c = (struct sdf_common *)s;
    umbra_build_sdf_draw(b, dst_ptr, fmt, x, y,
                         s->sdf_fn, s->sdf_ctx,
                         umbra_shader_color,  &c->color,
                         umbra_blend_srcover, NULL);
}

static void sdf_common_build_draw_full(struct slide *s,
                                       struct umbra_builder *b,
                                       umbra_ptr dst_ptr, struct umbra_fmt fmt,
                                       umbra_val32 x, umbra_val32 y) {
    struct sdf_common *c = (struct sdf_common *)s;
    umbra_build_draw(b, dst_ptr, fmt, x, y,
                     NULL,                NULL,
                     umbra_shader_color,  &c->color,
                     umbra_blend_srcover, NULL);
}

static void sdf_common_free(struct slide *s) { free(s); }

#define SDF_COMMON_HOOKS                                     \
    .build_sdf_draw  = sdf_common_build_draw,                \
    .build_draw_full = sdf_common_build_draw_full

// CSG: union / intersection / difference of two circles.

struct two_circle_sdf {
    float cx1, cy1, r1, cx2, cy2, r2;
};

enum csg_op { CSG_UNION, CSG_INTERSECT, CSG_DIFFERENCE };

static void two_circle_gather(struct umbra_builder *b, void *ctx,
                               umbra_interval x, umbra_interval y,
                               umbra_interval *a, umbra_interval *c) {
    struct two_circle_sdf const *self = ctx;
    umbra_ptr const u = umbra_early_bind_uniforms(b, self, sizeof *self / 4);
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

struct csg_slide {
    struct sdf_common       common;
    enum csg_op             op;
    int                     :32;
    struct two_circle_sdf   sdf;
};

static void csg_animate(struct slide *s, double secs) {
    struct csg_slide *st = (struct csg_slide *)s;
    float ox, oy, r_orbit;
    orbit_compute(&st->sdf.cx1, &st->sdf.cy1, &ox, &oy,
                   &st->sdf.r1, &r_orbit, (float)secs, s->w, s->h);
    st->sdf.cx2 = ox;
    st->sdf.cy2 = oy;
    st->sdf.r2  = r_orbit;
}

static struct slide* make_csg(char const *title, umbra_color color, enum csg_op op) {
    umbra_sdf *build = NULL;
    if (op == CSG_UNION)      { build = csg_union_build; }
    if (op == CSG_INTERSECT)  { build = csg_intersect_build; }
    if (op == CSG_DIFFERENCE) { build = csg_difference_build; }

    struct csg_slide *st = calloc(1, sizeof *st);
    st->op               = op;
    st->common.color     = color;
    st->common.base      = (struct slide){
        .title   = title,
        .bg      = {0.08f, 0.10f, 0.14f, 1},
        .free    = sdf_common_free,
        .animate = csg_animate,
        SDF_COMMON_HOOKS,
    };
    st->common.base.sdf_fn    = build;
    st->common.base.sdf_ctx   = &st->sdf;
    return &st->common.base;
}

SLIDE(slide_sdf_union) {
    return make_csg("SDF Union",        (umbra_color){0.2f, 0.8f, 0.4f, 1}, CSG_UNION);
}
SLIDE(slide_sdf_intersect) {
    return make_csg("SDF Intersection", (umbra_color){0.8f, 0.4f, 0.2f, 1}, CSG_INTERSECT);
}
SLIDE(slide_sdf_difference) {
    return make_csg("SDF Difference",   (umbra_color){0.4f, 0.2f, 0.8f, 1}, CSG_DIFFERENCE);
}

// Circle.

struct circle_sdf_ctx {
    float cx, cy, r;
    int   :32;
};

static umbra_interval circle_build(void *ctx, struct umbra_builder *b,
                                    umbra_interval x, umbra_interval y) {
    struct circle_sdf_ctx const *self = ctx;
    umbra_ptr const u = umbra_early_bind_uniforms(b, self, sizeof *self / 4);
    umbra_interval const cx = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(cx))),
                         cy = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(cy))),
                         r  = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(r)));
    return circle_sdf(b, x, y, cx, cy, r);
}

struct circle_slide {
    struct sdf_common     common;
    float                 cx0, cy0, vx, vy, r;
    int                   :32;
    struct circle_sdf_ctx sdf;
};

static void circle_init(struct slide *s) {
    struct circle_slide *st = (struct circle_slide *)s;
    st->r  = (float)(s->w < s->h ? s->w : s->h) * 0.18f;
    st->cx0 = (float)s->w * 0.3f;
    st->cy0 = (float)s->h * 0.4f;
    st->vx  = 1.7f;
    st->vy  = 1.3f;
}

static float bounce(float p0, float v, double secs, float range) {
    float p = fmodf(p0 + (float)secs * v, 2.0f * range);
    if (p < 0.0f) { p += 2.0f * range; }
    if (p > range) { p = 2.0f * range - p; }
    return p;
}

static void circle_animate(struct slide *s, double secs) {
    struct circle_slide *st = (struct circle_slide *)s;
    double const ticks = secs * 60.0;
    float const pad = st->r + 2.0f;
    st->sdf.cx = pad + bounce(st->cx0 - pad, st->vx, ticks, (float)s->w - 2.0f*pad);
    st->sdf.cy = pad + bounce(st->cy0 - pad, st->vy, ticks, (float)s->h - 2.0f*pad);
    st->sdf.r  = st->r;
}

SLIDE(slide_sdf_circle) {
    struct circle_slide *st = calloc(1, sizeof *st);
    st->common.color     = (umbra_color){0.95f, 0.45f, 0.10f, 1.0f};
    st->common.base      = (struct slide){
        .title   = "SDF Circle",
        .bg      = {0.08f, 0.10f, 0.14f, 1.0f},
        .init    = circle_init,
        .free    = sdf_common_free,
        .animate = circle_animate,
        SDF_COMMON_HOOKS,
    };
    st->common.base.sdf_fn    = circle_build;
    st->common.base.sdf_ctx   = &st->sdf;
    return &st->common.base;
}

// Ring.

struct ring_sdf_ctx {
    float cx, cy, r, w;
};

static umbra_interval ring_build(void *ctx, struct umbra_builder *b,
                                  umbra_interval x, umbra_interval y) {
    struct ring_sdf_ctx const *self = ctx;
    umbra_ptr const u = umbra_early_bind_uniforms(b, self, sizeof *self / 4);
    umbra_interval const cx = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(cx))),
                         cy = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(cy))),
                         r  = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(r))),
                         w  = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(w)));
    return umbra_interval_sub_f32(b, umbra_interval_abs_f32(b, circle_sdf(b, x, y, cx, cy, r)), w);
}

struct ring_slide {
    struct sdf_common   common;
    struct ring_sdf_ctx sdf;
};

static void ring_animate(struct slide *s, double secs) {
    struct ring_slide *st = (struct ring_slide *)s;
    float ox, oy, r_orbit;
    orbit_compute(&st->sdf.cx, &st->sdf.cy, &ox, &oy, &st->sdf.r, &r_orbit,
                   (float)secs, s->w, s->h);
    st->sdf.w = st->sdf.r * 0.15f;
}

SLIDE(slide_sdf_ring) {
    struct ring_slide *st = calloc(1, sizeof *st);
    st->common.color   = (umbra_color){1.0f, 0.6f, 0.1f, 1};
    st->common.base    = (struct slide){
        .title   = "SDF Ring",
        .bg      = {0.08f, 0.10f, 0.14f, 1},
        .free    = sdf_common_free,
        .animate = ring_animate,
        SDF_COMMON_HOOKS,
    };
    st->common.base.sdf_fn  = ring_build;
    st->common.base.sdf_ctx = &st->sdf;
    return &st->common.base;
}

// Rounded rect.

struct rounded_rect_sdf_ctx {
    float cx, cy, hw, hh, r;
    int   :32;
};

static umbra_interval rounded_rect_build(void *ctx, struct umbra_builder *b,
                                          umbra_interval x, umbra_interval y) {
    struct rounded_rect_sdf_ctx const *self = ctx;
    umbra_ptr const u = umbra_early_bind_uniforms(b, self, sizeof *self / 4);
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
    struct sdf_common           common;
    struct rounded_rect_sdf_ctx sdf;
};

static void rounded_rect_animate(struct slide *s, double secs) {
    struct rounded_rect_slide *st = (struct rounded_rect_slide *)s;
    int const w = s->w, h = s->h;
    float const anim = sinf((float)secs) * 0.5f + 0.5f;
    st->sdf.cx = (float)w * 0.5f;
    st->sdf.cy = (float)h * 0.5f;
    st->sdf.hw = (float)(w < h ? w : h) * (0.15f + 0.1f * anim);
    st->sdf.hh = (float)(w < h ? w : h) * (0.10f + 0.08f * anim);
    st->sdf.r  = (float)(w < h ? w : h) * 0.04f;
}

SLIDE(slide_sdf_rounded_rect) {
    struct rounded_rect_slide *st = calloc(1, sizeof *st);
    st->common.color   = (umbra_color){0.1f, 0.5f, 0.9f, 1};
    st->common.base    = (struct slide){
        .title   = "SDF Rounded Rect",
        .bg      = {0.08f, 0.10f, 0.14f, 1},
        .free    = sdf_common_free,
        .animate = rounded_rect_animate,
        SDF_COMMON_HOOKS,
    };
    st->common.base.sdf_fn  = rounded_rect_build;
    st->common.base.sdf_ctx = &st->sdf;
    return &st->common.base;
}

// Capsule.

struct capsule_sdf_ctx {
    float p0x, p0y, p1x, p1y, rad;
    int   :32;
};

static umbra_interval capsule_build(void *ctx, struct umbra_builder *b,
                                     umbra_interval x, umbra_interval y) {
    struct capsule_sdf_ctx const *self = ctx;
    umbra_ptr const u = umbra_early_bind_uniforms(b, self, sizeof *self / 4);
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
    struct sdf_common      common;
    struct capsule_sdf_ctx sdf;
};

static void capsule_animate(struct slide *s, double secs) {
    struct capsule_slide *st = (struct capsule_slide *)s;
    float cx, cy, ox, oy, r_center, r_orbit;
    orbit_compute(&cx, &cy, &ox, &oy, &r_center, &r_orbit,
                   (float)secs, s->w, s->h);
    st->sdf.p0x = cx; st->sdf.p0y = cy;
    st->sdf.p1x = ox; st->sdf.p1y = oy;
    st->sdf.rad = r_center * 0.15f;
}

SLIDE(slide_sdf_capsule) {
    struct capsule_slide *st = calloc(1, sizeof *st);
    st->common.color   = (umbra_color){0.9f, 0.3f, 0.6f, 1};
    st->common.base    = (struct slide){
        .title   = "SDF Capsule",
        .bg      = {0.08f, 0.10f, 0.14f, 1},
        .free    = sdf_common_free,
        .animate = capsule_animate,
        SDF_COMMON_HOOKS,
    };
    st->common.base.sdf_fn  = capsule_build;
    st->common.base.sdf_ctx = &st->sdf;
    return &st->common.base;
}

// Halfplane.

struct halfplane_sdf_ctx {
    float nx, ny, d;
    int   :32;
};

static umbra_interval halfplane_build(void *ctx, struct umbra_builder *b,
                                       umbra_interval x, umbra_interval y) {
    struct halfplane_sdf_ctx const *self = ctx;
    umbra_ptr const u = umbra_early_bind_uniforms(b, self, sizeof *self / 4);
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
    struct sdf_common        common;
    struct halfplane_sdf_ctx sdf;
};

static void halfplane_animate(struct slide *s, double secs) {
    struct halfplane_slide *st = (struct halfplane_slide *)s;
    float cx, cy, ox, oy, r_center, r_orbit;
    orbit_compute(&cx, &cy, &ox, &oy, &r_center, &r_orbit,
                   (float)secs, s->w, s->h);
    float const dx = ox - cx, dy = oy - cy;
    float const len = sqrtf(dx * dx + dy * dy);
    st->sdf.nx = len > 0 ?  dy / len : 0;
    st->sdf.ny = len > 0 ? -dx / len : 1;
    st->sdf.d  = st->sdf.nx * cx + st->sdf.ny * cy;
}

SLIDE(slide_sdf_halfplane) {
    struct halfplane_slide *st = calloc(1, sizeof *st);
    st->common.color   = (umbra_color){0.3f, 0.7f, 0.9f, 1};
    st->common.base    = (struct slide){
        .title   = "SDF Halfplane",
        .bg      = {0.08f, 0.10f, 0.14f, 1},
        .free    = sdf_common_free,
        .animate = halfplane_animate,
        SDF_COMMON_HOOKS,
    };
    st->common.base.sdf_fn  = halfplane_build;
    st->common.base.sdf_ctx = &st->sdf;
    return &st->common.base;
}

// SDF Text.

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
    umbra_ptr const u    = umbra_early_bind_uniforms(b, self, sizeof *self / 4);
    umbra_ptr const data = umbra_early_bind_buf(b, &self->curves);
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

    umbra_var32 lo_var = umbra_declare_var32(b, umbra_imm_f32(b, 1e9f));
    umbra_var32 hi_var = umbra_declare_var32(b, umbra_imm_f32(b, 1e9f));

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
    struct sdf_common   common;
    struct slug_curves  slug;
    struct sdf_text_sdf sdf;
};

static void sdf_text_init(struct slide *s) {
    struct sdf_text_slide *st = (struct sdf_text_slide *)s;
    st->slug = slug_extract("Hamburgefons", (float)s->h * 0.4f);

    float const gw = st->slug.w,
                gh = st->slug.h;
    float const sx = gw / (float)s->w,
                sy = gh / (float)s->h;
    float const scale = sx > sy ? sx : sy;
    float const pad_x = ((float)s->w * scale - gw) * 0.5f,
                pad_y = ((float)s->h * scale - gh) * 0.5f;
    st->sdf.scale_x = scale;
    st->sdf.scale_y = scale;
    st->sdf.off_x   = -pad_x;
    st->sdf.off_y   = -pad_y;
    st->sdf.n_curves = st->slug.count;
    st->sdf.curves   = (struct umbra_buf){.ptr = st->slug.data,
                                           .count = st->slug.count * 6};
}

static void sdf_text_free(struct slide *s) {
    struct sdf_text_slide *st = (struct sdf_text_slide *)s;
    slug_free(&st->slug);
    free(s);
}

SLIDE(slide_sdf_text) {
    struct sdf_text_slide *st = calloc(1, sizeof *st);
    st->common.color   = (umbra_color){0.95f, 0.9f, 0.8f, 1};
    st->common.base    = (struct slide){
        .title = "SDF Text",
        .bg    = {0.08f, 0.10f, 0.14f, 1},
        .init  = sdf_text_init,
        .free  = sdf_text_free,
        SDF_COMMON_HOOKS,
    };
    st->common.base.sdf_fn  = sdf_text_build;
    st->common.base.sdf_ctx = &st->sdf;
    return &st->common.base;
}

// SDF Polyline Text.
//
// Text rendered as a SIGNED distance field.  The curve buffer holds quadratic
// Beziers in (P0, P1, P2) form -- the same data slug.c emits -- and the
// shader flattens each curve into N=8 chord segments on the fly.  A single
// uniform-count loop of curves*N iterations handles this: index j splits as
// curve_id = j >> 3 and seg_id = j & 7, so each iteration pulls the current
// curve and evaluates B(t) at t = seg_id/8 and t = (seg_id+1)/8 to get a
// chord.  Requires N to be a power of two so the shift/mask is cheap.
//
// Interior/exterior sign comes from a horizontal-ray winding-parity test at
// the tile / pixel center.  The returned SDF interval is a Lipschitz-1
// conservative ball around the center's signed distance, so it's valid for
// both the 512-px tile bounds pass and the per-pixel draw pass.

enum { SDF_POLYLINE_SAMPLES = 8 };

struct sdf_polyline_text_sdf {
    float            scale_x, scale_y, off_x, off_y;
    int              n_curves;
    int              lg_n;    // log2(samples_per_curve); mask_n and rcp_n derive from this
    struct umbra_buf curves;
};

static umbra_interval sdf_polyline_text_build(void *ctx, struct umbra_builder *b,
                                               umbra_interval x, umbra_interval y) {
    struct sdf_polyline_text_sdf const *self = ctx;
    umbra_ptr const u    = umbra_early_bind_uniforms(b, self, sizeof *self / 4);
    umbra_ptr const data = umbra_early_bind_buf(b, &self->curves);

    umbra_val32 const n    = umbra_uniform_32(b, u, SLOT(n_curves)),
                      lgN  = umbra_uniform_32(b, u, SLOT(lg_n));

    umbra_val32 const sx = umbra_uniform_32(b, u, SLOT(scale_x)),
                      sy = umbra_uniform_32(b, u, SLOT(scale_y)),
                      ox = umbra_uniform_32(b, u, SLOT(off_x)),
                      oy = umbra_uniform_32(b, u, SLOT(off_y));

    umbra_val32 const z     = umbra_imm_f32(b, 0.0f),
                      o     = umbra_imm_f32(b, 1.0f),
                      tw    = umbra_imm_f32(b, 2.0f),
                      half  = umbra_imm_f32(b, 0.5f),
                      one_i = umbra_imm_i32(b, 1);

    // samples_n = 1 << lg_n; mask_n = samples_n - 1; rcp_n = 1 / samples_n.
    // All uniforms, so these compile to a single pre-dispatch scalar block.
    umbra_val32 const samplesN = umbra_shl_i32(b, one_i, lgN),
                      mskN     = umbra_sub_i32(b, samplesN, one_i),
                      rcpN     = umbra_div_f32(b, o, umbra_f32_from_i32(b, samplesN));

    umbra_val32 const six  = umbra_imm_i32(b, 6);

    umbra_val32 const bcx = umbra_mul_f32(b, umbra_add_f32(b, x.lo, x.hi), half),
                      bcy = umbra_mul_f32(b, umbra_add_f32(b, y.lo, y.hi), half);
    umbra_val32 const hw  = umbra_mul_f32(b, umbra_mul_f32(b, umbra_sub_f32(b, x.hi, x.lo),
                                                              half), sx),
                      hh  = umbra_mul_f32(b, umbra_mul_f32(b, umbra_sub_f32(b, y.hi, y.lo),
                                                              half), sy);
    umbra_val32 const r   = umbra_sqrt_f32(b,
                                umbra_add_f32(b, umbra_mul_f32(b, hw, hw),
                                                 umbra_mul_f32(b, hh, hh)));

    umbra_val32 const gx = umbra_add_f32(b, umbra_mul_f32(b, bcx, sx), ox),
                      gy = umbra_add_f32(b, umbra_mul_f32(b, bcy, sy), oy);

    umbra_val32 const n_iter = umbra_shl_i32(b, n, lgN);

    umbra_var32 const dist = umbra_declare_var32(b, umbra_imm_f32(b, 1e9f)),
                      par  = umbra_declare_var32(b, umbra_imm_i32(b, 0));

    umbra_val32 const j = umbra_loop(b, n_iter); {
        umbra_val32 const curve_id = umbra_shr_u32(b, j, lgN),
                          seg_id   = umbra_and_32(b, j, mskN);
        umbra_val32 const base = umbra_mul_i32(b, curve_id, six);
        umbra_val32 const p0x = umbra_gather_32(b, data, base),
                          p0y = umbra_gather_32(b, data,
                                    umbra_add_i32(b, base, umbra_imm_i32(b, 1))),
                          p1x = umbra_gather_32(b, data,
                                    umbra_add_i32(b, base, umbra_imm_i32(b, 2))),
                          p1y = umbra_gather_32(b, data,
                                    umbra_add_i32(b, base, umbra_imm_i32(b, 3))),
                          p2x = umbra_gather_32(b, data,
                                    umbra_add_i32(b, base, umbra_imm_i32(b, 4))),
                          p2y = umbra_gather_32(b, data,
                                    umbra_add_i32(b, base, umbra_imm_i32(b, 5)));

        umbra_val32 const ts   = umbra_mul_f32(b, umbra_f32_from_i32(b, seg_id), rcpN),
                          te   = umbra_add_f32(b, ts, rcpN),
                          mts  = umbra_sub_f32(b, o, ts),
                          mte  = umbra_sub_f32(b, o, te);
        umbra_val32 const w0s  = umbra_mul_f32(b, mts, mts),
                          w1s  = umbra_mul_f32(b, tw,  umbra_mul_f32(b, mts, ts)),
                          w2s  = umbra_mul_f32(b, ts,  ts),
                          w0e  = umbra_mul_f32(b, mte, mte),
                          w1e  = umbra_mul_f32(b, tw,  umbra_mul_f32(b, mte, te)),
                          w2e  = umbra_mul_f32(b, te,  te);

        umbra_val32 const ax = umbra_add_f32(b, umbra_mul_f32(b, w0s, p0x),
                                 umbra_add_f32(b, umbra_mul_f32(b, w1s, p1x),
                                                  umbra_mul_f32(b, w2s, p2x))),
                          ay = umbra_add_f32(b, umbra_mul_f32(b, w0s, p0y),
                                 umbra_add_f32(b, umbra_mul_f32(b, w1s, p1y),
                                                  umbra_mul_f32(b, w2s, p2y))),
                          cx = umbra_add_f32(b, umbra_mul_f32(b, w0e, p0x),
                                 umbra_add_f32(b, umbra_mul_f32(b, w1e, p1x),
                                                  umbra_mul_f32(b, w2e, p2x))),
                          cy = umbra_add_f32(b, umbra_mul_f32(b, w0e, p0y),
                                 umbra_add_f32(b, umbra_mul_f32(b, w1e, p1y),
                                                  umbra_mul_f32(b, w2e, p2y)));

        umbra_val32 const dx = umbra_sub_f32(b, cx, ax),
                          dy = umbra_sub_f32(b, cy, ay),
                          ex = umbra_sub_f32(b, gx, ax),
                          ey = umbra_sub_f32(b, gy, ay);
        umbra_val32 const dd = umbra_add_f32(b, umbra_mul_f32(b, dx, dx),
                                                umbra_mul_f32(b, dy, dy));
        umbra_val32 const ed = umbra_add_f32(b, umbra_mul_f32(b, ex, dx),
                                                umbra_mul_f32(b, ey, dy));
        umbra_val32 const t  = umbra_min_f32(b, o,
                                  umbra_max_f32(b, z,
                                      umbra_div_f32(b, ed, dd)));
        umbra_val32 const qx = umbra_sub_f32(b, gx,
                                  umbra_add_f32(b, ax, umbra_mul_f32(b, t, dx))),
                          qy = umbra_sub_f32(b, gy,
                                  umbra_add_f32(b, ay, umbra_mul_f32(b, t, dy)));
        umbra_val32 const seg_d = umbra_sqrt_f32(b,
                                      umbra_add_f32(b, umbra_mul_f32(b, qx, qx),
                                                       umbra_mul_f32(b, qy, qy)));
        umbra_store_var32(b, dist, umbra_min_f32(b, umbra_load_var32(b, dist), seg_d));

        umbra_val32 const below0 = umbra_lt_f32(b, ay, gy),
                          below1 = umbra_lt_f32(b, cy, gy),
                          strad  = umbra_xor_32(b, below0, below1);
        umbra_val32 const ty   = umbra_div_f32(b, umbra_sub_f32(b, gy, ay), dy),
                          xat  = umbra_add_f32(b, ax, umbra_mul_f32(b, ty, dx)),
                          right = umbra_lt_f32(b, gx, xat);
        umbra_val32 const cross = umbra_and_32(b, strad, right);
        umbra_store_var32(b, par, umbra_xor_32(b, umbra_load_var32(b, par), cross));
    } umbra_end_loop(b);

    umbra_val32 const d        = umbra_load_var32(b, dist),
                      parity   = umbra_load_var32(b, par);
    umbra_val32 const signed_d = umbra_sel_32(b, parity,
                                     umbra_sub_f32(b, z, d), d);

    return (umbra_interval){
        umbra_sub_f32(b, signed_d, r),
        umbra_add_f32(b, signed_d, r),
    };
}

struct sdf_polyline_text_slide {
    struct sdf_common            common;
    struct slug_curves           slug;
    struct sdf_polyline_text_sdf sdf;
};

static void sdf_polyline_text_init(struct slide *s) {
    struct sdf_polyline_text_slide *st = (struct sdf_polyline_text_slide *)s;
    st->slug = slug_extract("Hamburgefons", (float)s->h * 0.4f);

    float const gw = st->slug.w,
                gh = st->slug.h;
    float const sx = gw / (float)s->w,
                sy = gh / (float)s->h;
    float const scale = sx > sy ? sx : sy;
    float const pad_x = ((float)s->w * scale - gw) * 0.5f,
                pad_y = ((float)s->h * scale - gh) * 0.5f;
    st->sdf.scale_x  = scale;
    st->sdf.scale_y  = scale;
    st->sdf.off_x    = -pad_x;
    st->sdf.off_y    = -pad_y;
    st->sdf.n_curves = st->slug.count;
    st->sdf.lg_n     = __builtin_ctz(SDF_POLYLINE_SAMPLES);
    st->sdf.curves   = (struct umbra_buf){.ptr = st->slug.data,
                                           .count = st->slug.count * 6};
}

static void sdf_polyline_text_free(struct slide *s) {
    struct sdf_polyline_text_slide *st = (struct sdf_polyline_text_slide *)s;
    slug_free(&st->slug);
    free(s);
}

SLIDE(slide_sdf_polyline_text) {
    struct sdf_polyline_text_slide *st = calloc(1, sizeof *st);
    st->common.color   = (umbra_color){0.95f, 0.9f, 0.8f, 1};
    st->common.base    = (struct slide){
        .title = "SDF Polyline Text",
        .bg    = {0.08f, 0.10f, 0.14f, 1},
        .init  = sdf_polyline_text_init,
        .free  = sdf_polyline_text_free,
        SDF_COMMON_HOOKS,
    };
    st->common.base.sdf_fn  = sdf_polyline_text_build;
    st->common.base.sdf_ctx = &st->sdf;
    return &st->common.base;
}

// N-gon.

#define NGON_SIDES 6

struct ngon_sdf_ctx {
    int              n_sides, :32;
    struct umbra_buf hp;
};

static umbra_interval ngon_build(void *ctx, struct umbra_builder *b,
                                  umbra_interval x, umbra_interval y) {
    struct ngon_sdf_ctx const *self = ctx;
    umbra_ptr const u    = umbra_early_bind_uniforms(b, self, sizeof *self / 4);
    umbra_ptr const data = umbra_early_bind_buf(b, &self->hp);
    umbra_val32 const n    = umbra_uniform_32(b, u, SLOT(n_sides));

    umbra_var32 lo_var = umbra_declare_var32(b, umbra_imm_f32(b, -1e9f));
    umbra_var32 hi_var = umbra_declare_var32(b, umbra_imm_f32(b, -1e9f));

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
    struct sdf_common   common;
    float               cx, cy, r, angle;
    float              *hp_data;
    struct ngon_sdf_ctx sdf;
};

static void ngon_animate(struct slide *s, double secs) {
    struct ngon_slide *st = (struct ngon_slide *)s;
    st->cx    = (float)s->w * 0.5f;
    st->cy    = (float)s->h * 0.5f;
    st->r     = (float)(s->w < s->h ? s->w : s->h) * 0.3f;
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
        float const nx = len > 0 ?  ey / len : 0,
                    ny = len > 0 ? -ex / len : 1;
        st->hp_data[i * 3]     = nx;
        st->hp_data[i * 3 + 1] = ny;
        st->hp_data[i * 3 + 2] = nx * vx + ny * vy;
    }
    st->sdf.n_sides = NGON_SIDES;
    st->sdf.hp      = (struct umbra_buf){.ptr = st->hp_data, .count = 3 * NGON_SIDES};
}

static void ngon_free(struct slide *s) {
    struct ngon_slide *st = (struct ngon_slide *)s;
    free(st->hp_data);
    free(s);
}

SLIDE(slide_sdf_ngon) {
    struct ngon_slide *st = calloc(1, sizeof *st);
    st->common.color   = (umbra_color){0.8f, 0.8f, 0.2f, 1};
    st->hp_data        = malloc(3 * NGON_SIDES * sizeof(float));
    st->common.base    = (struct slide){
        .title   = "SDF N-Gon",
        .bg      = {0.08f, 0.10f, 0.14f, 1},
        .free    = ngon_free,
        .animate = ngon_animate,
        SDF_COMMON_HOOKS,
    };
    st->common.base.sdf_fn  = ngon_build;
    st->common.base.sdf_ctx = &st->sdf;
    return &st->common.base;
}
