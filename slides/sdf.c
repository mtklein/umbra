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
                         d2 = umbra_interval_fma_f32(b, dx, dx,
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
    umbra_sdf *fn;
    void      *ctx;
    slide_effective_sdf(s, &fn, &ctx);
    umbra_build_sdf_draw(b, dst_ptr, fmt, x, y,
                         fn, ctx,
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
    umbra_interval const d2 = umbra_interval_fma_f32(b, dx, dx,
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

    umbra_interval const dot_pd = umbra_interval_fma_f32(b, px, dx,
                                      umbra_interval_mul_f32(b, py, dy));
    umbra_interval const dot_dd = umbra_interval_fma_f32(b, dx, dx,
                                      umbra_interval_mul_f32(b, dy, dy));
    umbra_interval const zero = umbra_interval_exact(umbra_imm_f32(b, 0)),
                         one  = umbra_interval_exact(umbra_imm_f32(b, 1));
    umbra_interval const t = umbra_interval_min_f32(b, one,
                                 umbra_interval_max_f32(b, zero,
                                     umbra_interval_div_f32(b, dot_pd, dot_dd)));

    umbra_interval const cx = umbra_interval_fma_f32(b, t, dx, p0x),
                         cy = umbra_interval_fma_f32(b, t, dy, p0y);
    umbra_interval const ex = umbra_interval_sub_f32(b, x, cx),
                         ey = umbra_interval_sub_f32(b, y, cy);
    umbra_interval const d2 = umbra_interval_fma_f32(b, ex, ex,
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
    umbra_ptr const u = umbra_bind_uniforms(b, self, sizeof *self / 4);
    umbra_interval const nx = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(nx))),
                         ny = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(ny))),
                         d  = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(d)));
    return umbra_interval_sub_f32(b,
               umbra_interval_fma_f32(b, nx, x,
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
    float const mag = sqrtf(dx * dx + dy * dy);
    st->sdf.nx = mag > 0 ?  dy / mag : 0;
    st->sdf.ny = mag > 0 ? -dx / mag : 1;
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
    // hp_data is recomputed every frame by ngon_animate; not host-readonly.
    umbra_ptr const data = umbra_bind_buf(b, &self->hp);
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
                                      umbra_interval_fma_f32(b, nx, x,
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
        float const mag = sqrtf(ex * ex + ey * ey);
        float const nx = mag > 0 ?  ey / mag : 0,
                    ny = mag > 0 ? -ex / mag : 1;
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

// SDF Text (outline approximation).
//
// Unsigned distance to each curve's P0→P2 chord — the control point P1 is
// ignored.  Renders as a stroke-like outline; kept alongside the polyline
// and analytic slides as the simplest baseline in the comparison.

struct sdf_text_outline_sdf {
    struct umbra_affine mat;
    int              n_curves, :32;
    struct umbra_buf curves;
};

static umbra_interval sdf_text_outline_build(void *ctx, struct umbra_builder *b,
                                             umbra_interval x, umbra_interval y) {
    struct sdf_text_outline_sdf const *self = ctx;
    umbra_ptr const u    = umbra_bind_uniforms(b, self, sizeof *self / 4);
    umbra_ptr const data = umbra_bind_host_readonly_buf(b, &self->curves);
    umbra_val32 const n    = umbra_uniform_32(b, u, SLOT(n_curves));

    umbra_interval const sx = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(mat.sx))),
                         kx = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(mat.kx))),
                         tx = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(mat.tx))),
                         ky = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(mat.ky))),
                         sy = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(mat.sy))),
                         ty = umbra_interval_exact(umbra_uniform_32(b, u, SLOT(mat.ty)));

    umbra_interval const gx = umbra_interval_fma_f32(b, sx, x,
                                  umbra_interval_fma_f32(b, kx, y, tx)),
                         gy = umbra_interval_fma_f32(b, ky, x,
                                  umbra_interval_fma_f32(b, sy, y, ty));

    umbra_imm_f32(b, 0);
    umbra_imm_f32(b, 1);
    umbra_imm_f32(b, 1.5f);
    umbra_imm_i32(b, 1);
    umbra_imm_i32(b, 4);
    umbra_imm_i32(b, 5);
    umbra_imm_i32(b, 6);

    // Track squared distance bounds; sqrt them once after the loop.  See
    // sdf_text_polyline_build for the same trick.
    umbra_var32 lo2_var = umbra_declare_var32(b, umbra_imm_f32(b, 1e18f));
    umbra_var32 hi2_var = umbra_declare_var32(b, umbra_imm_f32(b, 1e18f));

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
        umbra_interval const dot_pd = umbra_interval_fma_f32(b, px, dx,
                                          umbra_interval_mul_f32(b, py, dy));
        umbra_interval const zero = umbra_interval_exact(umbra_imm_f32(b, 0)),
                             one  = umbra_interval_exact(umbra_imm_f32(b, 1));
        umbra_interval const t = umbra_interval_min_f32(b, one,
                                     umbra_interval_max_f32(b, zero,
                                         umbra_interval_div_f32(b, dot_pd, dd)));

        umbra_interval const cx = umbra_interval_fma_f32(b, t, dx, p0x),
                             cy = umbra_interval_fma_f32(b, t, dy, p0y);
        umbra_interval const ex = umbra_interval_sub_f32(b, gx, cx),
                             ey = umbra_interval_sub_f32(b, gy, cy);
        umbra_interval const d2 = umbra_interval_fma_f32(b, ex, ex,
                                      umbra_interval_mul_f32(b, ey, ey));

        umbra_store_var32(b, lo2_var, umbra_min_f32(b, umbra_load_var32(b, lo2_var), d2.lo));
        umbra_store_var32(b, hi2_var, umbra_min_f32(b, umbra_load_var32(b, hi2_var), d2.hi));
    } umbra_end_loop(b);

    umbra_interval const min_dist = {
        umbra_sqrt_f32(b, umbra_load_var32(b, lo2_var)),
        umbra_sqrt_f32(b, umbra_load_var32(b, hi2_var)),
    };
    return umbra_interval_sub_f32(b, min_dist, umbra_interval_exact(umbra_imm_f32(b, 1.5f)));
}

struct sdf_text_outline_slide {
    struct sdf_common           common;
    struct slug_curves          slug;
    struct sdf_text_outline_sdf sdf;
};

static void sdf_text_outline_init(struct slide *s) {
    struct sdf_text_outline_slide *st = (struct sdf_text_outline_slide *)s;
    st->slug = slug_extract("Hamburgefons", (float)s->h * 0.4f);
    st->sdf.n_curves = st->slug.count;
    st->sdf.curves   = (struct umbra_buf){.ptr = st->slug.data,
                                           .count = st->slug.count * 6};
}

static void sdf_text_outline_animate(struct slide *s, double secs) {
    struct sdf_text_outline_slide *st = (struct sdf_text_outline_slide *)s;
    slide_affine_matrix(&st->sdf.mat, (float)secs, s->w, s->h,
                        (int)st->slug.w, (int)st->slug.h);
}

static void sdf_text_outline_free(struct slide *s) {
    struct sdf_text_outline_slide *st = (struct sdf_text_outline_slide *)s;
    slug_free(&st->slug);
    free(s);
}

SLIDE(slide_sdf_text_outline) {
    struct sdf_text_outline_slide *st = calloc(1, sizeof *st);
    st->common.color   = (umbra_color){0.95f, 0.9f, 0.8f, 1};
    st->common.base    = (struct slide){
        .title   = "SDF Text Outline",
        .bg      = {0.08f, 0.10f, 0.14f, 1},
        .init    = sdf_text_outline_init,
        .animate = sdf_text_outline_animate,
        .free    = sdf_text_outline_free,
        SDF_COMMON_HOOKS,
    };
    st->common.base.sdf_fn  = sdf_text_outline_build;
    st->common.base.sdf_ctx = &st->sdf;
    return &st->common.base;
}

// SDF Text Polyline.
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

struct sdf_text_polyline_sdf {
    struct umbra_affine mat;
    int              n_curves;
    int              lg_n;    // log2(samples_per_curve); mask_n and rcp_n derive from this
    struct umbra_buf curves;
};

static umbra_interval sdf_text_polyline_build(void *ctx, struct umbra_builder *b,
                                               umbra_interval x, umbra_interval y) {
    struct sdf_text_polyline_sdf const *self = ctx;
    umbra_ptr const u    = umbra_bind_uniforms(b, self, sizeof *self / 4);
    umbra_ptr const data = umbra_bind_host_readonly_buf(b, &self->curves);

    umbra_val32 const n    = umbra_uniform_32(b, u, SLOT(n_curves)),
                      lgN  = umbra_uniform_32(b, u, SLOT(lg_n));

    umbra_val32 const msx = umbra_uniform_32(b, u, SLOT(mat.sx)),
                      mkx = umbra_uniform_32(b, u, SLOT(mat.kx)),
                      mtx = umbra_uniform_32(b, u, SLOT(mat.tx)),
                      mky = umbra_uniform_32(b, u, SLOT(mat.ky)),
                      msy = umbra_uniform_32(b, u, SLOT(mat.sy)),
                      mty = umbra_uniform_32(b, u, SLOT(mat.ty));

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
    umbra_val32 const hwx = umbra_mul_f32(b, umbra_sub_f32(b, x.hi, x.lo), half),
                      hwy = umbra_mul_f32(b, umbra_sub_f32(b, y.hi, y.lo), half);
    umbra_val32 const rx  = umbra_fma_f32(b, umbra_abs_f32(b, msx), hwx,
                                             umbra_mul_f32(b, umbra_abs_f32(b, mkx), hwy)),
                      ry  = umbra_fma_f32(b, umbra_abs_f32(b, mky), hwx,
                                             umbra_mul_f32(b, umbra_abs_f32(b, msy), hwy));
    umbra_val32 const r   = umbra_sqrt_f32(b,
                                umbra_fma_f32(b, rx, rx, umbra_mul_f32(b, ry, ry)));

    umbra_val32 const gx = umbra_fma_f32(b, msx, bcx, umbra_fma_f32(b, mkx, bcy, mtx)),
                      gy = umbra_fma_f32(b, mky, bcx, umbra_fma_f32(b, msy, bcy, mty));

    umbra_val32 const n_iter = umbra_shl_i32(b, n, lgN);

    // Track squared distance in the inner loop and sqrt it once at the end
    // (min preserves order on non-negative values), to amortize sqrt cost
    // across the curves*N iterations.
    umbra_var32 const dist2 = umbra_declare_var32(b, umbra_imm_f32(b, 1e18f)),
                      par   = umbra_declare_var32(b, umbra_imm_i32(b, 0));

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

        umbra_val32 const seg_f = umbra_f32_from_i32(b, seg_id);
        umbra_val32 const ts   = umbra_mul_f32(b, seg_f, rcpN),
                          te   = umbra_fma_f32(b, seg_f, rcpN, rcpN),
                          mts  = umbra_fms_f32(b, seg_f, rcpN, o),
                          mte  = umbra_sub_f32(b, o, te);
        umbra_val32 const w0s  = umbra_mul_f32(b, mts, mts),
                          w1s  = umbra_mul_f32(b, tw,  umbra_mul_f32(b, mts, ts)),
                          w2s  = umbra_mul_f32(b, ts,  ts),
                          w0e  = umbra_mul_f32(b, mte, mte),
                          w1e  = umbra_mul_f32(b, tw,  umbra_mul_f32(b, mte, te)),
                          w2e  = umbra_mul_f32(b, te,  te);

        umbra_val32 const ax = umbra_fma_f32(b, w0s, p0x,
                                 umbra_fma_f32(b, w1s, p1x,
                                                  umbra_mul_f32(b, w2s, p2x))),
                          ay = umbra_fma_f32(b, w0s, p0y,
                                 umbra_fma_f32(b, w1s, p1y,
                                                  umbra_mul_f32(b, w2s, p2y))),
                          cx = umbra_fma_f32(b, w0e, p0x,
                                 umbra_fma_f32(b, w1e, p1x,
                                                  umbra_mul_f32(b, w2e, p2x))),
                          cy = umbra_fma_f32(b, w0e, p0y,
                                 umbra_fma_f32(b, w1e, p1y,
                                                  umbra_mul_f32(b, w2e, p2y)));

        umbra_val32 const dx = umbra_sub_f32(b, cx, ax),
                          dy = umbra_sub_f32(b, cy, ay),
                          ex = umbra_sub_f32(b, gx, ax),
                          ey = umbra_sub_f32(b, gy, ay);
        umbra_val32 const dd = umbra_fma_f32(b, dx, dx, umbra_mul_f32(b, dy, dy));
        umbra_val32 const ed = umbra_fma_f32(b, ex, dx, umbra_mul_f32(b, ey, dy));
        umbra_val32 const t  = umbra_min_f32(b, o,
                                  umbra_max_f32(b, z,
                                      umbra_div_f32(b, ed, dd)));
        umbra_val32 const qx = umbra_sub_f32(b, gx, umbra_fma_f32(b, t, dx, ax)),
                          qy = umbra_sub_f32(b, gy, umbra_fma_f32(b, t, dy, ay));
        umbra_val32 const seg_d2 = umbra_fma_f32(b, qx, qx, umbra_mul_f32(b, qy, qy));
        umbra_store_var32(b, dist2, umbra_min_f32(b, umbra_load_var32(b, dist2), seg_d2));

        umbra_val32 const below0 = umbra_lt_f32(b, ay, gy),
                          below1 = umbra_lt_f32(b, cy, gy),
                          strad  = umbra_xor_32(b, below0, below1);
        umbra_val32 const ty   = umbra_div_f32(b, umbra_sub_f32(b, gy, ay), dy),
                          xat  = umbra_fma_f32(b, ty, dx, ax),
                          right = umbra_lt_f32(b, gx, xat);
        umbra_val32 const cross = umbra_and_32(b, strad, right);
        umbra_store_var32(b, par, umbra_xor_32(b, umbra_load_var32(b, par), cross));
    } umbra_end_loop(b);

    umbra_val32 const d        = umbra_sqrt_f32(b, umbra_load_var32(b, dist2)),
                      parity   = umbra_load_var32(b, par);
    umbra_val32 const signed_d = umbra_sel_32(b, parity,
                                     umbra_sub_f32(b, z, d), d);

    return (umbra_interval){
        umbra_sub_f32(b, signed_d, r),
        umbra_add_f32(b, signed_d, r),
    };
}

struct sdf_text_polyline_slide {
    struct sdf_common            common;
    struct slug_curves           slug;
    struct sdf_text_polyline_sdf sdf;
};

static void sdf_text_polyline_init(struct slide *s) {
    struct sdf_text_polyline_slide *st = (struct sdf_text_polyline_slide *)s;
    st->slug = slug_extract("Hamburgefons", (float)s->h * 0.4f);
    st->sdf.n_curves = st->slug.count;
    st->sdf.lg_n     = __builtin_ctz(SDF_POLYLINE_SAMPLES);
    st->sdf.curves   = (struct umbra_buf){.ptr = st->slug.data,
                                           .count = st->slug.count * 6};
}

static void sdf_text_polyline_animate(struct slide *s, double secs) {
    struct sdf_text_polyline_slide *st = (struct sdf_text_polyline_slide *)s;
    slide_affine_matrix(&st->sdf.mat, (float)secs, s->w, s->h,
                        (int)st->slug.w, (int)st->slug.h);
}

static void sdf_text_polyline_free(struct slide *s) {
    struct sdf_text_polyline_slide *st = (struct sdf_text_polyline_slide *)s;
    slug_free(&st->slug);
    free(s);
}

// SDF Text Analytic.
//
// Signed distance field where per-curve distance is computed analytically
// via Inigo Quilez's Cardano-form solution of the quadratic-Bezier distance
// cubic.  See https://iquilezles.org/articles/distfunctions2d/.
//
// The expensive trig branch (h<0 three real roots via acos/cos/sin) lives
// under umbra_if so GPU backends skip it on tiles where no lane hits the
// three-roots regime.  The linear fallback and the h≥0 cbrt branch are
// always computed and combined with sel() — they're cheap enough that
// branching isn't worth it.
//
// Straight-line segments arrive from slug.c as degenerate quadratics with
// P1 = midpoint(P0, P2), i.e. bb = P0 − 2·P1 + P2 = 0.  We clamp bb_bb with
// lin_eps so Cardano stays finite, and sel out its (wrong-for-linear)
// contribution via is_linear.  sqrt and acos arguments are clamped into
// their valid range for the same reason.
//
// Sign via horizontal-ray winding parity: linear crossing test always
// evaluated, curved two-root test gated by umbra_if(is_curved) so GPU
// backends skip the quadratic root solve on tiles where every curve nearby
// is linear.
//
// Bounds returned as a Lipschitz-1 ball around the tile center, matching
// the polyline slide.

struct sdf_text_analytic_sdf {
    struct umbra_affine mat;
    int              n_curves, :32;
    struct umbra_buf curves;
};

static umbra_interval sdf_text_analytic_build(void *ctx, struct umbra_builder *b,
                                               umbra_interval x, umbra_interval y) {
    struct sdf_text_analytic_sdf const *self = ctx;
    umbra_ptr const u    = umbra_bind_uniforms(b, self, sizeof *self / 4);
    umbra_ptr const data = umbra_bind_host_readonly_buf(b, &self->curves);
    umbra_val32 const n  = umbra_uniform_32(b, u, SLOT(n_curves));

    umbra_val32 const msx = umbra_uniform_32(b, u, SLOT(mat.sx)),
                      mkx = umbra_uniform_32(b, u, SLOT(mat.kx)),
                      mtx = umbra_uniform_32(b, u, SLOT(mat.tx)),
                      mky = umbra_uniform_32(b, u, SLOT(mat.ky)),
                      msy = umbra_uniform_32(b, u, SLOT(mat.sy)),
                      mty = umbra_uniform_32(b, u, SLOT(mat.ty));

    umbra_val32 const zero  = umbra_imm_f32(b, 0.0f),
                      one   = umbra_imm_f32(b, 1.0f),
                      two   = umbra_imm_f32(b, 2.0f),
                      three = umbra_imm_f32(b, 3.0f),
                      four  = umbra_imm_f32(b, 4.0f),
                      half  = umbra_imm_f32(b, 0.5f),
                      third = umbra_imm_f32(b, 1.0f/3.0f),
                      sqrt3 = umbra_imm_f32(b, 1.732050808f),
                      lin_eps = umbra_imm_f32(b, 1e-4f);

    umbra_val32 const bcx = umbra_mul_f32(b, umbra_add_f32(b, x.lo, x.hi), half),
                      bcy = umbra_mul_f32(b, umbra_add_f32(b, y.lo, y.hi), half);
    umbra_val32 const hwx = umbra_mul_f32(b, umbra_sub_f32(b, x.hi, x.lo), half),
                      hwy = umbra_mul_f32(b, umbra_sub_f32(b, y.hi, y.lo), half);
    umbra_val32 const rx  = umbra_fma_f32(b, umbra_abs_f32(b, msx), hwx,
                                             umbra_mul_f32(b, umbra_abs_f32(b, mkx), hwy)),
                      ry  = umbra_fma_f32(b, umbra_abs_f32(b, mky), hwx,
                                             umbra_mul_f32(b, umbra_abs_f32(b, msy), hwy));
    umbra_val32 const r   = umbra_sqrt_f32(b,
                                umbra_fma_f32(b, rx, rx, umbra_mul_f32(b, ry, ry)));

    umbra_val32 const gx = umbra_fma_f32(b, msx, bcx, umbra_fma_f32(b, mkx, bcy, mtx)),
                      gy = umbra_fma_f32(b, mky, bcx, umbra_fma_f32(b, msy, bcy, mty));

    umbra_var32 const dist2        = umbra_declare_var32(b, umbra_imm_f32(b, 1e18f)),
                      par          = umbra_declare_var32(b, umbra_imm_i32(b, 0)),
                      d2_trig_slot = umbra_declare_var32(b, umbra_imm_f32(b, 0.0f)),
                      cross_slot   = umbra_declare_var32(b, umbra_imm_i32(b, 0));

    umbra_val32 const six = umbra_imm_i32(b, 6);

    umbra_val32 const j = umbra_loop(b, n); {
        umbra_val32 const base = umbra_mul_i32(b, j, six);
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

        umbra_val32 const ax = umbra_sub_f32(b, p1x, p0x),
                          ay = umbra_sub_f32(b, p1y, p0y);
        umbra_val32 const bbx = umbra_add_f32(b, umbra_fms_f32(b, two, p1x, p0x), p2x),
                          bby = umbra_add_f32(b, umbra_fms_f32(b, two, p1y, p0y), p2y);
        umbra_val32 const dx = umbra_sub_f32(b, p0x, gx),
                          dy = umbra_sub_f32(b, p0y, gy);

        umbra_val32 const bb_bb = umbra_fma_f32(b, bbx, bbx, umbra_mul_f32(b, bby, bby));
        umbra_val32 const is_linear = umbra_lt_f32(b, bb_bb, lin_eps);

        // Linear path: closest-point-on-segment formula and straight-line
        // winding.  Produces finite results as long as seg_len2 > 0.
        umbra_val32 const seg_x = umbra_sub_f32(b, p2x, p0x),
                          seg_y = umbra_sub_f32(b, p2y, p0y);
        umbra_val32 const seg_len2 = umbra_fma_f32(b, seg_x, seg_x,
                                                       umbra_mul_f32(b, seg_y, seg_y));
        umbra_val32 const g_p0_x = umbra_sub_f32(b, gx, p0x),
                          g_p0_y = umbra_sub_f32(b, gy, p0y);
        umbra_val32 const dot_gs = umbra_fma_f32(b, g_p0_x, seg_x,
                                                    umbra_mul_f32(b, g_p0_y, seg_y));
        umbra_val32 const t_lin = umbra_min_f32(b, one,
                                      umbra_max_f32(b, zero,
                                          umbra_div_f32(b, dot_gs, seg_len2)));
        umbra_val32 const qx_lin = umbra_sub_f32(b, gx, umbra_fma_f32(b, t_lin, seg_x, p0x)),
                          qy_lin = umbra_sub_f32(b, gy, umbra_fma_f32(b, t_lin, seg_y, p0y));
        umbra_val32 const d2_linear = umbra_fma_f32(b, qx_lin, qx_lin,
                                                        umbra_mul_f32(b, qy_lin, qy_lin));

        umbra_val32 const below0 = umbra_lt_f32(b, p0y, gy),
                          below1 = umbra_lt_f32(b, p2y, gy),
                          strad  = umbra_xor_32(b, below0, below1);
        umbra_val32 const ty_lin = umbra_div_f32(b,
                                       umbra_sub_f32(b, gy, p0y), seg_y),
                          xat_lin = umbra_fma_f32(b, ty_lin, seg_x, p0x),
                          right_lin = umbra_lt_f32(b, gx, xat_lin);
        umbra_val32 const linear_cross = umbra_and_32(b, strad, right_lin);

        // Cardano path, using bb_bb_safe so linear inputs don't produce NaN.
        // is_linear picks which path wins at the end.
        umbra_val32 const bb_bb_safe = umbra_max_f32(b, bb_bb, lin_eps);
        umbra_val32 const kk = umbra_div_f32(b, one, bb_bb_safe);
        umbra_val32 const a_bb = umbra_fma_f32(b, ax, bbx, umbra_mul_f32(b, ay, bby));
        umbra_val32 const kx = umbra_mul_f32(b, kk, a_bb);
        umbra_val32 const a_a = umbra_fma_f32(b, ax, ax, umbra_mul_f32(b, ay, ay));
        umbra_val32 const d_bb = umbra_fma_f32(b, dx, bbx, umbra_mul_f32(b, dy, bby));
        umbra_val32 const ky = umbra_mul_f32(b, kk,
                                   umbra_mul_f32(b, third,
                                       umbra_fma_f32(b, two, a_a, d_bb)));
        umbra_val32 const d_a = umbra_fma_f32(b, dx, ax, umbra_mul_f32(b, dy, ay));

        umbra_val32 const kx2 = umbra_mul_f32(b, kx, kx);
        umbra_val32 const p   = umbra_sub_f32(b, ky, kx2);
        umbra_val32 const q   = umbra_fma_f32(b, kk, d_a,
                                    umbra_mul_f32(b, kx,
                                        umbra_fms_f32(b, three, ky,
                                            umbra_mul_f32(b, two, kx2))));
        umbra_val32 const p3 = umbra_mul_f32(b, p, umbra_mul_f32(b, p, p));
        umbra_val32 const h  = umbra_fma_f32(b, four, p3, umbra_mul_f32(b, q, q));

        // h ≥ 0 branch (always computed; max(h,0) keeps sqrt finite when h<0):
        umbra_val32 const hs = umbra_sqrt_f32(b, umbra_max_f32(b, h, zero));
        umbra_val32 const x1 = umbra_mul_f32(b, half, umbra_sub_f32(b, hs, q)),
                          x2 = umbra_mul_f32(b, half,
                                   umbra_sub_f32(b, umbra_sub_f32(b, zero, hs), q));
        umbra_val32 const u1 = umbra_cbrt_f32(b, x1),
                          u2 = umbra_cbrt_f32(b, x2);
        umbra_val32 const t_pos_raw = umbra_sub_f32(b, umbra_add_f32(b, u1, u2), kx);
        umbra_val32 const t_pos = umbra_min_f32(b, one,
                                      umbra_max_f32(b, zero, t_pos_raw));
        umbra_val32 const dpx = umbra_fma_f32(b, t_pos,
                                    umbra_fma_f32(b, two, ax, umbra_mul_f32(b, bbx, t_pos)),
                                    dx),
                          dpy = umbra_fma_f32(b, t_pos,
                                    umbra_fma_f32(b, two, ay, umbra_mul_f32(b, bby, t_pos)),
                                    dy);
        umbra_val32 const d2_pos = umbra_fma_f32(b, dpx, dpx, umbra_mul_f32(b, dpy, dpy));

        // h < 0 branch gated so GPU backends skip acos/cos/sin on tiles with
        // no h<0 lanes.  d2_trig_slot only needs updating in h_neg lanes; the
        // sel below ignores its value everywhere else, so it does not need a
        // per-iteration reset.
        umbra_val32 const h_neg = umbra_lt_f32(b, h, zero);
        umbra_if(b, h_neg); {
            umbra_val32 const neg_p_safe = umbra_max_f32(b,
                                               umbra_sub_f32(b, zero, p), zero);
            umbra_val32 const z_v = umbra_sqrt_f32(b, neg_p_safe);
            umbra_val32 const denom = umbra_mul_f32(b, two, umbra_mul_f32(b, p, z_v));
            umbra_val32 const arg_raw = umbra_div_f32(b, q, denom);
            umbra_val32 const neg_one = umbra_sub_f32(b, zero, one);
            umbra_val32 const arg_safe = umbra_min_f32(b, one,
                                             umbra_max_f32(b, neg_one, arg_raw));
            umbra_val32 const v  = umbra_mul_f32(b, third, umbra_acos_f32(b, arg_safe));
            umbra_val32 const mv = umbra_cos_f32(b, v),
                              nv = umbra_mul_f32(b, sqrt3, umbra_sin_f32(b, v));
            umbra_val32 const t0_raw = umbra_sub_f32(b,
                                           umbra_mul_f32(b, umbra_add_f32(b, mv, mv), z_v),
                                           kx),
                              t1_raw = umbra_sub_f32(b,
                                           umbra_mul_f32(b,
                                               umbra_sub_f32(b, umbra_sub_f32(b, zero, nv), mv),
                                               z_v),
                                           kx);
            umbra_val32 const t0 = umbra_min_f32(b, one, umbra_max_f32(b, zero, t0_raw)),
                              t1 = umbra_min_f32(b, one, umbra_max_f32(b, zero, t1_raw));
            umbra_val32 const d0x = umbra_fma_f32(b, t0,
                                        umbra_fma_f32(b, two, ax, umbra_mul_f32(b, bbx, t0)),
                                        dx),
                              d0y = umbra_fma_f32(b, t0,
                                        umbra_fma_f32(b, two, ay, umbra_mul_f32(b, bby, t0)),
                                        dy),
                              d1x = umbra_fma_f32(b, t1,
                                        umbra_fma_f32(b, two, ax, umbra_mul_f32(b, bbx, t1)),
                                        dx),
                              d1y = umbra_fma_f32(b, t1,
                                        umbra_fma_f32(b, two, ay, umbra_mul_f32(b, bby, t1)),
                                        dy);
            umbra_val32 const d2_t0 = umbra_fma_f32(b, d0x, d0x, umbra_mul_f32(b, d0y, d0y)),
                              d2_t1 = umbra_fma_f32(b, d1x, d1x, umbra_mul_f32(b, d1y, d1y));
            umbra_store_var32(b, d2_trig_slot, umbra_min_f32(b, d2_t0, d2_t1));
        } umbra_end_if(b);

        umbra_val32 const d2_card  = umbra_sel_32(b, h_neg,
                                         umbra_load_var32(b, d2_trig_slot), d2_pos);
        umbra_val32 const d2_curve = umbra_sel_32(b, is_linear, d2_linear, d2_card);
        umbra_store_var32(b, dist2,
            umbra_min_f32(b, umbra_load_var32(b, dist2), d2_curve));

        // Curved winding: B(t).y − gy = bb.y·t² + 2·a.y·t + (P0.y − gy) = 0.
        // Both roots are evaluated; out-of-range or complex roots produce
        // cross=0 naturally via the inside/disc_ok AND-mask chain.
        // Curved winding: nested umbra_if gates the quadratic root solve so
        // GPU skips it on linear-only tiles, with disc>=0 nested inside so
        // the sqrt+div+cross computation is skipped when the ray misses.
        // Reset cross_slot per iteration; inner branch overwrites when both
        // conditions hold.
        umbra_val32 const is_curved = umbra_le_f32(b, lin_eps, bb_bb);
        umbra_store_var32(b, cross_slot, umbra_imm_i32(b, 0));
        umbra_if(b, is_curved); {
            umbra_val32 const qA = bby,
                              qB = umbra_mul_f32(b, two, ay),
                              qC = umbra_sub_f32(b, p0y, gy);
            umbra_val32 const disc = umbra_fms_f32(b, four,
                                         umbra_mul_f32(b, qA, qC),
                                         umbra_mul_f32(b, qB, qB));
            umbra_if(b, umbra_le_f32(b, zero, disc)); {
                umbra_val32 const sd     = umbra_sqrt_f32(b, disc);
                umbra_val32 const inv2A  = umbra_div_f32(b, half, qA);
                umbra_val32 const neg_qB = umbra_sub_f32(b, zero, qB);
                umbra_val32 const r0 = umbra_mul_f32(b, inv2A, umbra_add_f32(b, neg_qB, sd)),
                                  r1 = umbra_mul_f32(b, inv2A, umbra_sub_f32(b, neg_qB, sd));
                umbra_val32 const inside0 = umbra_and_32(b,
                                                umbra_le_f32(b, zero, r0),
                                                umbra_le_f32(b, r0, one)),
                                  inside1 = umbra_and_32(b,
                                                umbra_le_f32(b, zero, r1),
                                                umbra_le_f32(b, r1, one));
                umbra_val32 const bxr0 = umbra_fma_f32(b, r0,
                                             umbra_fma_f32(b, two, ax,
                                                 umbra_mul_f32(b, bbx, r0)),
                                             p0x),
                                  bxr1 = umbra_fma_f32(b, r1,
                                             umbra_fma_f32(b, two, ax,
                                                 umbra_mul_f32(b, bbx, r1)),
                                             p0x);
                umbra_val32 const right0 = umbra_lt_f32(b, gx, bxr0),
                                  right1 = umbra_lt_f32(b, gx, bxr1);
                umbra_val32 const cross0 = umbra_and_32(b, inside0, right0),
                                  cross1 = umbra_and_32(b, inside1, right1);
                umbra_store_var32(b, cross_slot, umbra_xor_32(b, cross0, cross1));
            } umbra_end_if(b);
        } umbra_end_if(b);

        umbra_val32 const cross = umbra_sel_32(b, is_linear, linear_cross,
                                       umbra_load_var32(b, cross_slot));
        umbra_store_var32(b, par, umbra_xor_32(b, umbra_load_var32(b, par), cross));
    } umbra_end_loop(b);

    umbra_val32 const d        = umbra_sqrt_f32(b, umbra_load_var32(b, dist2)),
                      parity   = umbra_load_var32(b, par);
    umbra_val32 const signed_d = umbra_sel_32(b, parity,
                                     umbra_sub_f32(b, zero, d), d);

    return (umbra_interval){
        umbra_sub_f32(b, signed_d, r),
        umbra_add_f32(b, signed_d, r),
    };
}

struct sdf_text_analytic_slide {
    struct sdf_common            common;
    struct slug_curves           slug;
    struct sdf_text_analytic_sdf sdf;
};

static void sdf_text_analytic_init(struct slide *s) {
    struct sdf_text_analytic_slide *st = (struct sdf_text_analytic_slide *)s;
    st->slug = slug_extract("Hamburgefons", (float)s->h * 0.4f);
    st->sdf.n_curves = st->slug.count;
    st->sdf.curves   = (struct umbra_buf){.ptr = st->slug.data,
                                           .count = st->slug.count * 6};
}

static void sdf_text_analytic_animate(struct slide *s, double secs) {
    struct sdf_text_analytic_slide *st = (struct sdf_text_analytic_slide *)s;
    slide_affine_matrix(&st->sdf.mat, (float)secs, s->w, s->h,
                        (int)st->slug.w, (int)st->slug.h);
}

static void sdf_text_analytic_free(struct slide *s) {
    struct sdf_text_analytic_slide *st = (struct sdf_text_analytic_slide *)s;
    slug_free(&st->slug);
    free(s);
}

SLIDE(slide_sdf_text_analytic) {
    struct sdf_text_analytic_slide *st = calloc(1, sizeof *st);
    st->common.color   = (umbra_color){0.95f, 0.9f, 0.8f, 1};
    st->common.base    = (struct slide){
        .title   = "SDF Text Analytic",
        .bg      = {0.08f, 0.10f, 0.14f, 1},
        .init    = sdf_text_analytic_init,
        .animate = sdf_text_analytic_animate,
        .free    = sdf_text_analytic_free,
        SDF_COMMON_HOOKS,
    };
    st->common.base.sdf_fn  = sdf_text_analytic_build;
    st->common.base.sdf_ctx = &st->sdf;
    return &st->common.base;
}

SLIDE(slide_sdf_text_polyline) {
    struct sdf_text_polyline_slide *st = calloc(1, sizeof *st);
    st->common.color   = (umbra_color){0.95f, 0.9f, 0.8f, 1};
    st->common.base    = (struct slide){
        .title   = "SDF Text Polyline",
        .bg      = {0.08f, 0.10f, 0.14f, 1},
        .init    = sdf_text_polyline_init,
        .animate = sdf_text_polyline_animate,
        .free    = sdf_text_polyline_free,
        SDF_COMMON_HOOKS,
    };
    st->common.base.sdf_fn  = sdf_text_polyline_build;
    st->common.base.sdf_ctx = &st->sdf;
    return &st->common.base;
}
