#include "slide.h"
#include <math.h>
#include <stdlib.h>

struct blend_slide {
    struct slide base;

    float rx, ry, vx, vy;
    float rect_w, rect_h;
    int   w, h;

    umbra_color   color;
    umbra_rect    rect;
    umbra_blend   *blend_fn;
};

static void blend_init(struct slide *s, int w, int h) {
    struct blend_slide *st = (struct blend_slide *)s;
    st->w = w;
    st->h = h;
    st->rect_w = (float)w * 5.0f / 16.0f;
    st->rect_h = (float)h * 5.0f / 16.0f;
    st->rx = (float)w * 5.0f / 32.0f;
    st->ry = (float)h / 6.0f;
    st->vx = 1.5f;
    st->vy = 1.1f;
}

static float bounce(float p0, float v, double secs, float range) {
    float p = fmodf(p0 + (float)secs * v, 2.0f * range);
    if (p < 0.0f) { p += 2.0f * range; }
    if (p > range) { p = 2.0f * range - p; }
    return p;
}

static void blend_build_draw(struct slide *s, struct umbra_builder *b,
                             umbra_ptr dst_ptr, struct umbra_fmt fmt,
                             umbra_val32 x, umbra_val32 y) {
    struct blend_slide *st = (struct blend_slide *)s;
    umbra_build_draw(b, dst_ptr, fmt, x, y,
                     umbra_coverage_rect, &st->rect,
                     umbra_shader_color,  &st->color,
                     st->blend_fn,        NULL);
}

static void blend_animate(struct slide *s, double secs) {
    struct blend_slide *st = (struct blend_slide *)s;
    double const ticks = secs * 60.0;
    float const rx = bounce(st->rx, st->vx, ticks, (float)st->w - st->rect_w),
                ry = bounce(st->ry, st->vy, ticks, (float)st->h - st->rect_h);
    st->rect = (umbra_rect){rx, ry, rx + st->rect_w, ry + st->rect_h};
}

static void blend_free(struct slide *s) { free(s); }

static struct slide* make_blend(char const *title, float const bg[4], float const color[4],
                                umbra_blend blend) {
    struct blend_slide *st = calloc(1, sizeof *st);
    st->color    = (umbra_color){color[0], color[1], color[2], color[3]};
    st->rect     = (umbra_rect){0, 0, 0, 0};
    st->blend_fn = blend;
    st->base = (struct slide){
        .title = title,
        .bg = {bg[0], bg[1], bg[2], bg[3]},
        .init = blend_init,
        .free = blend_free,
        .build_draw   = blend_build_draw,
        .animate      = blend_animate,
    };
    return &st->base;
}

SLIDE(slide_blend_src) {
    return make_blend("Blend Src", (float[]){0.125f, 0.125f, 0.125f, 1},
                      (float[]){0.0f, 0.6f, 1.0f, 1.0f},
                      umbra_blend_src);
}

SLIDE(slide_blend_srcover) {
    return make_blend("Blend Srcover", (float[]){0, 1, 0, 1},
                      (float[]){0.45f, 0.0f, 0.0f, 0.5f},
                      umbra_blend_srcover);
}

SLIDE(slide_blend_dstover) {
    return make_blend("Blend Dstover", (float[]){0, 0.5f, 0, 0.75f},
                      (float[]){0.0f, 0.0f, 0.9f, 0.9f},
                      umbra_blend_dstover);
}

SLIDE(slide_blend_multiply) {
    return make_blend("Blend Multiply", (float[]){0.125f, 0.25f, 0.5f, 1},
                      (float[]){1.0f, 0.5f, 0.0f, 1.0f},
                      umbra_blend_multiply);
}

SLIDE(slide_blend_null) {
    return make_blend("Blend NULL", (float[]){0, 0, 0, 1},
                      (float[]){0.9f, 0.4f, 0.1f, 1.0f},
                      NULL);
}
