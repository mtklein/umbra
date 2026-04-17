#include "slide.h"
#include "../include/umbra_interval.h"
#include <math.h>
#include <stdlib.h>

struct circle_sdf {
    struct umbra_sdf base;
    float cx, cy, r;
    int   :32;
};

#define SLOT(field) \
    ((int)((__builtin_offsetof(__typeof__(*self), field) - sizeof(self->base)) / 4))

static umbra_interval circle_build(struct umbra_sdf *s, struct umbra_builder *b,
                                    int buf_index,
                                    umbra_interval x, umbra_interval y) {
    struct circle_sdf *self = (struct circle_sdf *)s;
    umbra_ptr32 const u_ptr = {.ix = buf_index};
    umbra_interval const cx = umbra_interval_exact(umbra_uniform_32(b, u_ptr, SLOT(cx))),
                         cy = umbra_interval_exact(umbra_uniform_32(b, u_ptr, SLOT(cy))),
                         r  = umbra_interval_exact(umbra_uniform_32(b, u_ptr, SLOT(r)));
    umbra_interval const dx = umbra_interval_sub_f32(b, x, cx),
                         dy = umbra_interval_sub_f32(b, y, cy),
                         d2 = umbra_interval_add_f32(b,
                                  umbra_interval_mul_f32(b, dx, dx),
                                  umbra_interval_mul_f32(b, dy, dy)),
                         d  = umbra_interval_sqrt_f32(b, d2);
    return umbra_interval_sub_f32(b, d, r);
}

struct circle_slide {
    struct slide base;

    float cx0, cy0, vx, vy, r;
    int   w, h, pad;

    struct umbra_shader      *shader;
    struct circle_sdf         sdf;

    struct umbra_fmt          fmt;
    struct umbra_sdf_draw    *qt;
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
    st->qt  = umbra_sdf_draw(be, &st->sdf.base,
                             (struct umbra_sdf_draw_config){.hard_edge = 0},
                             st->shader, umbra_blend_srcover, fmt);
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

    struct umbra_buf ubuf[] = {
        {.ptr=buf, .count=st->w * st->h * st->fmt.planes, .stride=st->w},
        umbra_sdf_uniforms(&st->sdf.base),
        umbra_shader_uniforms(st->shader),
    };
    umbra_sdf_draw_queue(st->qt, l, t, r, b, ubuf);
}

static int circle_get_builders(struct slide *s, struct umbra_fmt fmt,
                               struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct circle_slide *st = (struct circle_slide *)s;
    struct umbra_coverage *adapter = umbra_sdf_coverage(&st->sdf.base, 0);
    out[0] = umbra_draw_builder(adapter, st->shader,
                                umbra_blend_srcover, fmt);
    umbra_coverage_free(adapter);
    return out[0] ? 1 : 0;
}

static void circle_free(struct slide *s) {
    struct circle_slide *st = (struct circle_slide *)s;
    umbra_sdf_draw_free(st->qt);
    umbra_shader_free(st->shader);
    free(st);
}

SLIDE(slide_circle_coverage) {
    struct circle_slide *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid((umbra_color){0.95f, 0.45f, 0.10f, 1.0f});
    st->sdf = (struct circle_sdf){
        .base = {.build          = circle_build,
                 .uniforms_slots = (int)((sizeof(struct circle_sdf)
                                         - sizeof(struct umbra_sdf)) / 4)},
    };
    st->base = (struct slide){
        .title = "Circle Coverage (interval-ready)",
        .bg = {0.08f, 0.10f, 0.14f, 1.0f},
        .init = circle_init,
        .prepare = circle_prepare,
        .draw = circle_draw,
        .free = circle_free,
        .get_builders = circle_get_builders,
    };
    return &st->base;
}
