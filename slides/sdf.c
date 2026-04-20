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
// its first field so (struct slide *) casts hit sdf_common.base and
// (struct sdf_common *) casts work too.  Slides fill sdf_fn/sdf_ctx/color
// in their ctor and are otherwise driven by sdf_common_{prepare,draw,...}
// and the shared slide_runtime embedded here.
struct sdf_common {
    struct slide          base;
    int                   w, h;
    umbra_color           color;
    umbra_sdf            *sdf_fn;
    void                 *sdf_ctx;
    struct slide_runtime  rt;
};

static void sdf_common_build_draw(struct slide *s,
                                  struct umbra_builder *b_draw,
                                  umbra_ptr dst_ptr, struct umbra_fmt fmt,
                                  umbra_val32 x, umbra_val32 y,
                                  struct umbra_builder *b_bounds,
                                  umbra_ptr cov_ptr,
                                  umbra_interval ix, umbra_interval iy) {
    struct sdf_common *c = (struct sdf_common *)s;
    umbra_build_sdf_draw(b_draw, dst_ptr, fmt, x, y,
                         c->sdf_fn, c->sdf_ctx, 1,
                         umbra_shader_color,  &c->color,
                         umbra_blend_srcover, NULL);
    umbra_build_sdf_bounds(b_bounds, cov_ptr, ix, iy, c->sdf_fn, c->sdf_ctx);
}

static void sdf_common_prepare(struct slide *s,
                               struct umbra_backend *be, struct umbra_fmt fmt) {
    struct sdf_common *c = (struct sdf_common *)s;
    slide_runtime_cleanup(&c->rt);
    slide_runtime_compile(&c->rt, s, c->w, c->h, be, fmt, NULL);
    slide_bg_prepare(be, fmt, c->w, c->h);
}

static void sdf_common_draw(struct slide *s, double secs,
                            int l, int t, int r, int b, void *buf) {
    struct sdf_common *c = (struct sdf_common *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    c->rt.dst_buf = (struct umbra_buf){
        .ptr = buf, .count = c->w * c->h * c->rt.fmt.planes, .stride = c->w,
    };
    slide_runtime_draw(&c->rt, s, secs, l, t, r, b);
}

static int sdf_common_get_builders(struct slide *s, struct umbra_fmt fmt,
                                   struct umbra_builder **out, int max) {
    if (max < 2) { return 0; }
    struct sdf_common *c = (struct sdf_common *)s;

    struct umbra_builder *db = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(db, &c->rt.dst_buf);
    umbra_val32 const x = umbra_f32_from_i32(db, umbra_x(db)),
                      y = umbra_f32_from_i32(db, umbra_y(db));

    struct umbra_builder *bb = umbra_builder();
    umbra_ptr const cov_ptr = umbra_bind_buf(bb, &c->rt.cov_buf);
    umbra_interval ix, iy;
    umbra_sdf_tile_intervals(bb, &c->rt.grid, NULL, &ix, &iy);

    sdf_common_build_draw(s, db, dst_ptr, fmt, x, y, bb, cov_ptr, ix, iy);

    out[0] = db;
    out[1] = bb;
    return 2;
}

// Release everything sdf_common owns, but leave the outer slide alloc alone
// -- slides with extra state call this from their own free() and then
// free(s).
static void sdf_common_cleanup(struct slide *s) {
    struct sdf_common *c = (struct sdf_common *)s;
    slide_runtime_cleanup(&c->rt);
}

static void sdf_common_free(struct slide *s) {
    sdf_common_cleanup(s);
    free(s);
}

// Convenience to spread the five shared slide-fn pointers across every SLIDE
// macro below.
#define SDF_COMMON_HOOKS                        \
    .prepare        = sdf_common_prepare,       \
    .draw           = sdf_common_draw,          \
    .get_builders   = sdf_common_get_builders,  \
    .build_sdf_draw = sdf_common_build_draw

// =============================================================================
// CSG: union / intersection / difference of two circles.

struct two_circle_sdf {
    float cx1, cy1, r1, cx2, cy2, r2;
};

enum csg_op { CSG_UNION, CSG_INTERSECT, CSG_DIFFERENCE };

static void two_circle_gather(struct umbra_builder *b, void *ctx,
                               umbra_interval x, umbra_interval y,
                               umbra_interval *a, umbra_interval *c) {
    struct two_circle_sdf const *self = ctx;
    umbra_ptr const u = umbra_bind_uniforms(b, self, sizeof *self / 4);
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

static void csg_init(struct slide *s, int w, int h) {
    struct csg_slide *st = (struct csg_slide *)s;
    st->common.w = w;
    st->common.h = h;
}

static void csg_animate(struct slide *s, double secs) {
    struct csg_slide *st = (struct csg_slide *)s;
    float ox, oy, r_orbit;
    orbit_compute(&st->sdf.cx1, &st->sdf.cy1, &ox, &oy,
                   &st->sdf.r1, &r_orbit, (float)secs, st->common.w, st->common.h);
    st->sdf.cx2 = ox;
    st->sdf.cy2 = oy;
    st->sdf.r2  = r_orbit;
}

static struct slide* make_csg(char const *title, float const color[4], enum csg_op op) {
    umbra_sdf *build = NULL;
    if (op == CSG_UNION)      { build = csg_union_build; }
    if (op == CSG_INTERSECT)  { build = csg_intersect_build; }
    if (op == CSG_DIFFERENCE) { build = csg_difference_build; }

    struct csg_slide *st = calloc(1, sizeof *st);
    st->op               = op;
    st->common.color     = (umbra_color){color[0], color[1], color[2], color[3]};
    st->common.sdf_fn    = build;
    st->common.sdf_ctx   = &st->sdf;
    st->common.base      = (struct slide){
        .title   = title,
        .bg      = {0.08f, 0.10f, 0.14f, 1},
        .init    = csg_init,
        .free    = sdf_common_free,
        .animate = csg_animate,
        SDF_COMMON_HOOKS,
    };
    return &st->common.base;
}

SLIDE(slide_sdf_union)     { return make_csg("SDF Union",        (float[]){0.2f, 0.8f, 0.4f, 1}, CSG_UNION); }
SLIDE(slide_sdf_intersect) { return make_csg("SDF Intersection", (float[]){0.8f, 0.4f, 0.2f, 1}, CSG_INTERSECT); }
SLIDE(slide_sdf_difference){ return make_csg("SDF Difference",   (float[]){0.4f, 0.2f, 0.8f, 1}, CSG_DIFFERENCE); }

// =============================================================================
// Circle.

struct circle_sdf_ctx {
    float cx, cy, r;
    int   :32;
};

static umbra_interval circle_build(void *ctx, struct umbra_builder *b,
                                    umbra_interval x, umbra_interval y) {
    struct circle_sdf_ctx const *self = ctx;
    umbra_ptr const u = umbra_bind_uniforms(b, self, sizeof *self / 4);
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

static void circle_init(struct slide *s, int w, int h) {
    struct circle_slide *st = (struct circle_slide *)s;
    st->common.w  = w;
    st->common.h  = h;
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

static void circle_animate(struct slide *s, double secs) {
    struct circle_slide *st = (struct circle_slide *)s;
    double const ticks = secs * 60.0;
    float const pad = st->r + 2.0f;
    st->sdf.cx = pad + bounce(st->cx0 - pad, st->vx, ticks, (float)st->common.w - 2.0f*pad);
    st->sdf.cy = pad + bounce(st->cy0 - pad, st->vy, ticks, (float)st->common.h - 2.0f*pad);
    st->sdf.r  = st->r;
}

SLIDE(slide_sdf_circle) {
    struct circle_slide *st = calloc(1, sizeof *st);
    st->common.color     = (umbra_color){0.95f, 0.45f, 0.10f, 1.0f};
    st->common.sdf_fn    = circle_build;
    st->common.sdf_ctx   = &st->sdf;
    st->common.base      = (struct slide){
        .title   = "SDF Circle",
        .bg      = {0.08f, 0.10f, 0.14f, 1.0f},
        .init    = circle_init,
        .free    = sdf_common_free,
        .animate = circle_animate,
        SDF_COMMON_HOOKS,
    };
    return &st->common.base;
}

// =============================================================================
// Ring.

struct ring_sdf_ctx {
    float cx, cy, r, w;
};

static umbra_interval ring_build(void *ctx, struct umbra_builder *b,
                                  umbra_interval x, umbra_interval y) {
    struct ring_sdf_ctx const *self = ctx;
    umbra_ptr const u = umbra_bind_uniforms(b, self, sizeof *self / 4);
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

static void ring_init(struct slide *s, int w, int h) {
    struct ring_slide *st = (struct ring_slide *)s;
    st->common.w = w;
    st->common.h = h;
}

static void ring_animate(struct slide *s, double secs) {
    struct ring_slide *st = (struct ring_slide *)s;
    float ox, oy, r_orbit;
    orbit_compute(&st->sdf.cx, &st->sdf.cy, &ox, &oy, &st->sdf.r, &r_orbit,
                   (float)secs, st->common.w, st->common.h);
    st->sdf.w = st->sdf.r * 0.15f;
}

SLIDE(slide_sdf_ring) {
    struct ring_slide *st = calloc(1, sizeof *st);
    st->common.color   = (umbra_color){1.0f, 0.6f, 0.1f, 1};
    st->common.sdf_fn  = ring_build;
    st->common.sdf_ctx = &st->sdf;
    st->common.base    = (struct slide){
        .title   = "SDF Ring",
        .bg      = {0.08f, 0.10f, 0.14f, 1},
        .init    = ring_init,
        .free    = sdf_common_free,
        .animate = ring_animate,
        SDF_COMMON_HOOKS,
    };
    return &st->common.base;
}

// =============================================================================
// Rounded rect.

struct rounded_rect_sdf_ctx {
    float cx, cy, hw, hh, r;
    int   :32;
};

static umbra_interval rounded_rect_build(void *ctx, struct umbra_builder *b,
                                          umbra_interval x, umbra_interval y) {
    struct rounded_rect_sdf_ctx const *self = ctx;
    umbra_ptr const u = umbra_bind_uniforms(b, self, sizeof *self / 4);
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

static void rounded_rect_init(struct slide *s, int w, int h) {
    struct rounded_rect_slide *st = (struct rounded_rect_slide *)s;
    st->common.w = w;
    st->common.h = h;
}

static void rounded_rect_animate(struct slide *s, double secs) {
    struct rounded_rect_slide *st = (struct rounded_rect_slide *)s;
    int const w = st->common.w, h = st->common.h;
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
    st->common.sdf_fn  = rounded_rect_build;
    st->common.sdf_ctx = &st->sdf;
    st->common.base    = (struct slide){
        .title   = "SDF Rounded Rect",
        .bg      = {0.08f, 0.10f, 0.14f, 1},
        .init    = rounded_rect_init,
        .free    = sdf_common_free,
        .animate = rounded_rect_animate,
        SDF_COMMON_HOOKS,
    };
    return &st->common.base;
}

// =============================================================================
// Capsule.

struct capsule_sdf_ctx {
    float p0x, p0y, p1x, p1y, rad;
    int   :32;
};

static umbra_interval capsule_build(void *ctx, struct umbra_builder *b,
                                     umbra_interval x, umbra_interval y) {
    struct capsule_sdf_ctx const *self = ctx;
    umbra_ptr const u = umbra_bind_uniforms(b, self, sizeof *self / 4);
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

static void capsule_init(struct slide *s, int w, int h) {
    struct capsule_slide *st = (struct capsule_slide *)s;
    st->common.w = w;
    st->common.h = h;
}

static void capsule_animate(struct slide *s, double secs) {
    struct capsule_slide *st = (struct capsule_slide *)s;
    float cx, cy, ox, oy, r_center, r_orbit;
    orbit_compute(&cx, &cy, &ox, &oy, &r_center, &r_orbit,
                   (float)secs, st->common.w, st->common.h);
    st->sdf.p0x = cx; st->sdf.p0y = cy;
    st->sdf.p1x = ox; st->sdf.p1y = oy;
    st->sdf.rad = r_center * 0.15f;
}

SLIDE(slide_sdf_capsule) {
    struct capsule_slide *st = calloc(1, sizeof *st);
    st->common.color   = (umbra_color){0.9f, 0.3f, 0.6f, 1};
    st->common.sdf_fn  = capsule_build;
    st->common.sdf_ctx = &st->sdf;
    st->common.base    = (struct slide){
        .title   = "SDF Capsule",
        .bg      = {0.08f, 0.10f, 0.14f, 1},
        .init    = capsule_init,
        .free    = sdf_common_free,
        .animate = capsule_animate,
        SDF_COMMON_HOOKS,
    };
    return &st->common.base;
}

// =============================================================================
// Halfplane.

struct halfplane_sdf_ctx {
    float nx, ny, d;
    int   :32;
};

static umbra_interval halfplane_build(void *ctx, struct umbra_builder *b,
                                       umbra_interval x, umbra_interval y) {
    struct halfplane_sdf_ctx const *self = ctx;
    umbra_ptr const u = umbra_bind_uniforms(b, self, sizeof *self / 4);
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

static void halfplane_init(struct slide *s, int w, int h) {
    struct halfplane_slide *st = (struct halfplane_slide *)s;
    st->common.w = w;
    st->common.h = h;
}

static void halfplane_animate(struct slide *s, double secs) {
    struct halfplane_slide *st = (struct halfplane_slide *)s;
    float cx, cy, ox, oy, r_center, r_orbit;
    orbit_compute(&cx, &cy, &ox, &oy, &r_center, &r_orbit,
                   (float)secs, st->common.w, st->common.h);
    float const dx = ox - cx, dy = oy - cy;
    float const len = sqrtf(dx * dx + dy * dy);
    st->sdf.nx = len > 0 ?  dy / len : 0;
    st->sdf.ny = len > 0 ? -dx / len : 1;
    st->sdf.d  = st->sdf.nx * cx + st->sdf.ny * cy;
}

SLIDE(slide_sdf_halfplane) {
    struct halfplane_slide *st = calloc(1, sizeof *st);
    st->common.color   = (umbra_color){0.3f, 0.7f, 0.9f, 1};
    st->common.sdf_fn  = halfplane_build;
    st->common.sdf_ctx = &st->sdf;
    st->common.base    = (struct slide){
        .title   = "SDF Halfplane",
        .bg      = {0.08f, 0.10f, 0.14f, 1},
        .init    = halfplane_init,
        .free    = sdf_common_free,
        .animate = halfplane_animate,
        SDF_COMMON_HOOKS,
    };
    return &st->common.base;
}

// =============================================================================
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
    umbra_ptr const u    = umbra_bind_uniforms(b, self, sizeof *self / 4);
    umbra_ptr const data = umbra_bind_buf(b, &self->curves);
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
    struct sdf_common   common;
    struct slug_curves  slug;
    struct sdf_text_sdf sdf;
};

static void sdf_text_init(struct slide *s, int w, int h) {
    struct sdf_text_slide *st = (struct sdf_text_slide *)s;
    st->common.w = w;
    st->common.h = h;
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

static void sdf_text_free(struct slide *s) {
    struct sdf_text_slide *st = (struct sdf_text_slide *)s;
    slug_free(&st->slug);
    sdf_common_cleanup(s);
    free(s);
}

SLIDE(slide_sdf_text) {
    struct sdf_text_slide *st = calloc(1, sizeof *st);
    st->common.color   = (umbra_color){0.95f, 0.9f, 0.8f, 1};
    st->common.sdf_fn  = sdf_text_build;
    st->common.sdf_ctx = &st->sdf;
    st->common.base    = (struct slide){
        .title = "SDF Text",
        .bg    = {0.08f, 0.10f, 0.14f, 1},
        .init  = sdf_text_init,
        .free  = sdf_text_free,
        SDF_COMMON_HOOKS,
    };
    return &st->common.base;
}

// =============================================================================
// N-gon.

#define NGON_SIDES 6

struct ngon_sdf_ctx {
    int              n_sides, :32;
    struct umbra_buf hp;
};

static umbra_interval ngon_build(void *ctx, struct umbra_builder *b,
                                  umbra_interval x, umbra_interval y) {
    struct ngon_sdf_ctx const *self = ctx;
    umbra_ptr const u    = umbra_bind_uniforms(b, self, sizeof *self / 4);
    umbra_ptr const data = umbra_bind_buf(b, &self->hp);
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
    struct sdf_common   common;
    float               cx, cy, r, angle;
    float              *hp_data;
    struct ngon_sdf_ctx sdf;
};

static void ngon_init(struct slide *s, int w, int h) {
    struct ngon_slide *st = (struct ngon_slide *)s;
    st->common.w = w;
    st->common.h = h;
}

static void ngon_animate(struct slide *s, double secs) {
    struct ngon_slide *st = (struct ngon_slide *)s;
    st->cx    = (float)st->common.w * 0.5f;
    st->cy    = (float)st->common.h * 0.5f;
    st->r     = (float)(st->common.w < st->common.h ? st->common.w : st->common.h) * 0.3f;
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
    sdf_common_cleanup(s);
    free(s);
}

SLIDE(slide_sdf_ngon) {
    struct ngon_slide *st = calloc(1, sizeof *st);
    st->common.color   = (umbra_color){0.8f, 0.8f, 0.2f, 1};
    st->hp_data        = malloc(3 * NGON_SIDES * sizeof(float));
    st->common.sdf_fn  = ngon_build;
    st->common.sdf_ctx = &st->sdf;
    st->common.base    = (struct slide){
        .title   = "SDF N-Gon",
        .bg      = {0.08f, 0.10f, 0.14f, 1},
        .init    = ngon_init,
        .free    = ngon_free,
        .animate = ngon_animate,
        SDF_COMMON_HOOKS,
    };
    return &st->common.base;
}
