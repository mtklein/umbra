#include "slide.h"
#include "../include/umbra_interval.h"
#include <math.h>
#include <stdlib.h>

struct circle_sdf {
    struct umbra_sdf base;
    float cx, cy, r, pad_;
    int   off_, pad__;
};

static umbra_interval circle_build_(struct umbra_sdf *s, struct umbra_builder *b,
                                    struct umbra_uniforms_layout *u,
                                    umbra_interval x, umbra_interval y) {
    struct circle_sdf *self = (struct circle_sdf *)s;
    self->off_ = umbra_uniforms_reserve_f32(u, 3);

    umbra_interval const cx = umbra_interval_exact(umbra_uniform_32(b, (umbra_ptr32){0}, self->off_)),
                         cy = umbra_interval_exact(umbra_uniform_32(b, (umbra_ptr32){0}, self->off_ + 1)),
                         r  = umbra_interval_exact(umbra_uniform_32(b, (umbra_ptr32){0}, self->off_ + 2));
    umbra_interval const dx = umbra_interval_sub_f32(b, x, cx),
                         dy = umbra_interval_sub_f32(b, y, cy),
                         d2 = umbra_interval_add_f32(b,
                                  umbra_interval_mul_f32(b, dx, dx),
                                  umbra_interval_mul_f32(b, dy, dy)),
                         d  = umbra_interval_sqrt_f32(b, d2);
    return umbra_interval_sub_f32(b, d, r);
}
static void circle_fill_(struct umbra_sdf const *s, void *uniforms) {
    struct circle_sdf const *self = (struct circle_sdf const *)s;
    float const vals[3] = {self->cx, self->cy, self->r};
    umbra_uniforms_fill_f32(uniforms, self->off_, vals, 3);
}

struct circle_slide {
    struct slide base;

    float cx0, cy0, vx, vy, r;
    int   w, h, pad_;

    struct umbra_shader_solid shader;
    struct circle_sdf         sdf;

    struct umbra_fmt          fmt;
    struct umbra_draw_layout  lay;
    struct umbra_sdf_dispatch    *qt;
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
    umbra_sdf_dispatch_free(st->qt);
    free(st->lay.uniforms);
    st->fmt = fmt;
    st->qt  = umbra_sdf_dispatch(be, &st->sdf.base,
                             (struct umbra_sdf_dispatch_config){.hard_edge = 0},
                             &st->shader.base, umbra_blend_srcover, fmt, &st->lay);
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

    umbra_sdf_dispatch_fill(&st->lay, &st->sdf.base, &st->shader.base);
    struct umbra_buf ubuf[] = {
        {.ptr=st->lay.uniforms, .count=st->lay.uni.slots},
        {.ptr=buf, .count=st->w * st->h * st->fmt.planes, .stride=st->w},
    };
    umbra_sdf_dispatch_queue(st->qt, l, t, r, b, ubuf);
}

static int circle_get_builders(struct slide *s, struct umbra_fmt fmt,
                               struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct circle_slide *st = (struct circle_slide *)s;
    struct umbra_sdf_coverage adapter = umbra_sdf_coverage(&st->sdf.base, 0);
    out[0] = umbra_draw_builder(&st->shader.base, &adapter.base,
                                umbra_blend_srcover, fmt, NULL);
    return out[0] ? 1 : 0;
}

static void circle_free(struct slide *s) {
    struct circle_slide *st = (struct circle_slide *)s;
    umbra_sdf_dispatch_free(st->qt);
    free(st->lay.uniforms);
    free(st);
}

SLIDE(slide_circle_coverage) {
    struct circle_slide *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid((float[]){0.95f, 0.45f, 0.10f, 1.0f});
    st->sdf = (struct circle_sdf){
        .base = {.build = circle_build_, .fill = circle_fill_},
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
