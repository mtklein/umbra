#include "slide.h"
#include <math.h>
#include <stdlib.h>

struct solid_slide {
    struct slide base;

    float rx, ry, vx, vy;
    float rect_w, rect_h;
    int   w, h;
    int   has_cov, :32;

    struct umbra_shader_solid    shader;
    struct umbra_coverage_rect   cov;
    umbra_blend_fn               blend;

    struct umbra_fmt            fmt;
    struct umbra_draw_layout   lay;
    struct umbra_flat_ir      *bb;
    struct umbra_program          *prog;
};

static void solid_init(struct slide *s, int w, int h) {
    struct solid_slide *st = (struct solid_slide *)s;
    st->w = w;
    st->h = h;
    st->rect_w = (float)w * 5.0f / 16.0f;
    st->rect_h = (float)h * 5.0f / 16.0f;
    st->rx = (float)w * 5.0f / 32.0f;
    st->ry = (float)h / 6.0f;
    st->vx = 1.5f;
    st->vy = 1.1f;
}

// Triangle-wave bounce: position oscillates in [0, range] starting at the
// init-time offset, advancing |v| pixels per frame. Equivalent to the prior
// stateful integrate-and-bounce but expressed as a closed form of `frame`.
static float bounce(float p0, float v, int frame, float range) {
    float p = fmodf(p0 + (float)frame * v, 2.0f * range);
    if (p < 0.0f) { p += 2.0f * range; }
    if (p > range) { p = 2.0f * range - p; }
    return p;
}

static void solid_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct solid_slide *st = (struct solid_slide *)s;
    if (st->fmt.name != fmt.name || !st->bb) {
        st->fmt = fmt;
        umbra_flat_ir_free(st->bb);
        free(st->lay.uniforms);
        struct umbra_builder *b = umbra_draw_build(&st->shader.base,
                                                    st->has_cov ? &st->cov.base : NULL,
                                                    st->blend, fmt, &st->lay);
        st->bb = umbra_flat_ir(b);
        umbra_builder_free(b);
    }
    if (st->prog) { st->prog->free(st->prog); }
    st->prog = be->compile(be, st->bb);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void solid_draw(struct slide *s, int frame, int l, int t, int r, int b, void *buf) {
    struct solid_slide *st = (struct solid_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    float rx = bounce(st->rx, st->vx, frame, (float)st->w - st->rect_w);
    float ry = bounce(st->ry, st->vy, frame, (float)st->h - st->rect_h);
    if (st->has_cov) {
        st->cov.rect[0] = rx;
        st->cov.rect[1] = ry;
        st->cov.rect[2] = rx + st->rect_w;
        st->cov.rect[3] = ry + st->rect_h;
    }
    umbra_draw_fill(&st->lay, &st->shader.base, st->has_cov ? &st->cov.base : NULL);
    struct umbra_buf ubuf[] = {
        {.ptr=st->lay.uniforms, .count=st->lay.uni.slots},
        {.ptr=buf, .count=st->w * st->h * st->fmt.planes, .stride=st->w},
    };
    st->prog->queue(st->prog, l, t, r, b, ubuf);
}

static struct umbra_builder *solid_get_builder(struct slide *s, struct umbra_fmt fmt) {
    struct solid_slide *st = (struct solid_slide *)s;
    return umbra_draw_build(&st->shader.base, st->has_cov ? &st->cov.base : NULL,
                            st->blend, fmt, NULL);
}

static void solid_free(struct slide *s) {
    struct solid_slide *st = (struct solid_slide *)s;
    if (st->prog) { st->prog->free(st->prog); }
    umbra_flat_ir_free(st->bb);
    free(st->lay.uniforms);
    free(st);
}

static struct slide *make_solid(char const *title, float const bg[4], float const color[4],
                                _Bool has_cov, umbra_blend_fn blend) {
    struct solid_slide *st = calloc(1, sizeof *st);
    st->shader  = umbra_shader_solid(color);
    st->has_cov = has_cov;
    if (has_cov) { st->cov = umbra_coverage_rect((float[]){0, 0, 0, 0}); }
    st->blend   = blend;
    st->base = (struct slide){
        .title = title,
        .bg = {bg[0], bg[1], bg[2], bg[3]},
        .init = solid_init,
        .prepare = solid_prepare,
        .draw = solid_draw,
        .free = solid_free,
        .get_builder = solid_get_builder,
    };
    return &st->base;
}

SLIDE(slide_solid_src) {
    return make_solid("Solid Fill (src)", (float[]){0.125f, 0.125f, 0.125f, 1},
                      (float[]){0.0f, 0.6f, 1.0f, 1.0f},
                      1, umbra_blend_src);
}

SLIDE(slide_solid_srcover) {

    return make_solid("Source Over (srcover)", (float[]){0, 1, 0, 1},
                      (float[]){0.45f, 0.0f, 0.0f, 0.5f},
                      1, umbra_blend_srcover);
}

SLIDE(slide_solid_dstover) {

    return make_solid("Destination Over (dstover)", (float[]){0, 0.5f, 0, 0.75f},
                      (float[]){0.0f, 0.0f, 0.9f, 0.9f},
                      1, umbra_blend_dstover);
}

SLIDE(slide_solid_multiply) {

    return make_solid("Multiply Blend", (float[]){0.125f, 0.25f, 0.5f, 1},
                      (float[]){1.0f, 0.5f, 0.0f, 1.0f},
                      1, umbra_blend_multiply);
}

SLIDE(slide_solid_full_cov) {

    return make_solid("Full Coverage (no rect clip)", (float[]){1, 1, 1, 1},
                      (float[]){0.15f, 0.0f, 0.3f, 0.3f},
                      0, umbra_blend_srcover);
}

SLIDE(slide_solid_no_blend) {

    return make_solid("No Blend (direct paint)", (float[]){0, 0, 0, 1},
                      (float[]){0.9f, 0.4f, 0.1f, 1.0f},
                      1, NULL);
}
