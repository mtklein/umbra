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

    struct umbra_fmt      fmt;
    struct umbra_program *prog;
    struct umbra_buf      dst_buf;
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

static struct umbra_builder* blend_builder(struct slide *s, struct umbra_fmt fmt) {
    struct blend_slide *st = (struct blend_slide *)s;
    struct umbra_builder *b = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(b, &st->dst_buf);
    umbra_val32 const x = umbra_f32_from_i32(b, umbra_x(b)),
                      y = umbra_f32_from_i32(b, umbra_y(b));
    blend_build_draw(s, b, dst_ptr, fmt, x, y);
    return b;
}

static void blend_prepare(struct slide *s, struct umbra_backend *be,
                          struct umbra_fmt fmt) {
    struct blend_slide *st = (struct blend_slide *)s;
    umbra_program_free(st->prog);
    st->fmt = fmt;
    struct umbra_builder *b = blend_builder(s, fmt);
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    st->prog = be->compile(be, ir);
    umbra_flat_ir_free(ir);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void blend_animate(struct slide *s, double secs) {
    struct blend_slide *st = (struct blend_slide *)s;
    double const ticks = secs * 60.0;
    float const rx = bounce(st->rx, st->vx, ticks, (float)st->w - st->rect_w),
                ry = bounce(st->ry, st->vy, ticks, (float)st->h - st->rect_h);
    st->rect = (umbra_rect){rx, ry, rx + st->rect_w, ry + st->rect_h};
}

static void blend_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct blend_slide *st = (struct blend_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    blend_animate(s, secs);
    st->dst_buf = (struct umbra_buf){
        .ptr=buf, .count=st->w * st->h * st->fmt.planes, .stride=st->w,
    };
    st->prog->queue(st->prog, l, t, r, b);
}

static int blend_get_builders(struct slide *s, struct umbra_fmt fmt,
                              struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    out[0] = blend_builder(s, fmt);
    return out[0] ? 1 : 0;
}

static void blend_free(struct slide *s) {
    struct blend_slide *st = (struct blend_slide *)s;
    umbra_program_free(st->prog);
    free(st);
}

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
        .prepare = blend_prepare,
        .draw = blend_draw,
        .free = blend_free,
        .get_builders = blend_get_builders,
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
