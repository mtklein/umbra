#include "slide.h"
#include <stdlib.h>

struct grad_2stop_state {
    struct slide base;

    float color[8];
    float grad[4];
    int   w, h;

    umbra_shader_fn    shader;
    umbra_coverage_fn  coverage;
    umbra_blend_fn     blend;

    struct umbra_fmt            fmt;
    struct umbra_draw_layout   lay;
    struct umbra_basic_block  *bb;
    struct umbra_program      *prog;
};

struct grad_lut_state {
    struct slide base;

    float *lut;
    int    lut_n;
    int    pad_;
    int    w, h;
    float  grad[4];

    umbra_shader_fn    shader;
    umbra_coverage_fn  coverage;
    umbra_blend_fn     blend;

    struct umbra_fmt            fmt;
    struct umbra_draw_layout   lay;
    struct umbra_basic_block  *bb;
    struct umbra_program      *prog;
};

static void grad_2stop_init(struct slide *s, int w, int h) {
    struct grad_2stop_state *st = (struct grad_2stop_state *)s;
    st->w = w;
    st->h = h;
}

static void grad_2stop_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct grad_2stop_state *st = (struct grad_2stop_state *)s;
    if (st->fmt.name != fmt.name || !st->bb) {
        st->fmt = fmt;
        umbra_basic_block_free(st->bb);
        free(st->lay.uniforms);
        struct umbra_builder *b = umbra_draw_build(st->shader, st->coverage, st->blend, fmt,
                                                    &st->lay);
        st->bb = umbra_basic_block(b);
        umbra_builder_free(b);
    }
    if (st->prog) { st->prog->free(st->prog); }
    st->prog = be->compile(be, st->bb);
}

static void grad_2stop_draw(struct slide *s, int l, int t, int r, int b, void *buf) {
    struct grad_2stop_state *st = (struct grad_2stop_state *)s;
    umbra_uniforms_fill_f32(st->lay.uniforms, st->lay.shader,      st->grad,  3);
    umbra_uniforms_fill_f32(st->lay.uniforms, st->lay.shader + 12, st->color, 8);
    size_t    pb = st->fmt.bpp;
    size_t plane_sz = (size_t)st->w * (size_t)st->h * pb;
    size_t rb = (size_t)st->w * pb;
    struct umbra_buf ubuf[] = {
        {.ptr=st->lay.uniforms, .sz=st->lay.uni.size, .read_only=1},
        {.ptr=buf, .sz=plane_sz * (size_t)st->fmt.planes, .row_bytes=rb},
    };
    st->prog->queue(st->prog, l, t, r, b, ubuf);
}

static struct umbra_builder *grad_2stop_get_builder(struct slide *s, struct umbra_fmt fmt) {
    struct grad_2stop_state *st = (struct grad_2stop_state *)s;
    return umbra_draw_build(st->shader, st->coverage, st->blend, fmt, NULL);
}

static void grad_2stop_free(struct slide *s) {
    struct grad_2stop_state *st = (struct grad_2stop_state *)s;
    if (st->prog) { st->prog->free(st->prog); }
    umbra_basic_block_free(st->bb);
    free(st->lay.uniforms);
    free(st);
}

static void grad_lut_init(struct slide *s, int w, int h) {
    struct grad_lut_state *st = (struct grad_lut_state *)s;
    st->w = w;
    st->h = h;
}

static void grad_lut_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct grad_lut_state *st = (struct grad_lut_state *)s;
    if (st->fmt.name != fmt.name || !st->bb) {
        st->fmt = fmt;
        umbra_basic_block_free(st->bb);
        free(st->lay.uniforms);
        struct umbra_builder *b = umbra_draw_build(st->shader, st->coverage, st->blend, fmt,
                                                    &st->lay);
        st->bb = umbra_basic_block(b);
        umbra_builder_free(b);
    }
    if (st->prog) { st->prog->free(st->prog); }
    st->prog = be->compile(be, st->bb);
}

static void grad_lut_draw(struct slide *s, int l, int t, int r, int b, void *buf) {
    struct grad_lut_state *st = (struct grad_lut_state *)s;
    umbra_uniforms_fill_f32(st->lay.uniforms, st->lay.shader, st->grad, 4);
    umbra_uniforms_fill_ptr(st->lay.uniforms, (st->lay.shader + 16 + 7) & ~(size_t)7,
                  (struct umbra_buf){.ptr=st->lut, .sz=(size_t)(st->lut_n * 4 * 4)});
    size_t    pb = st->fmt.bpp;
    size_t plane_sz = (size_t)st->w * (size_t)st->h * pb;
    size_t rb = (size_t)st->w * pb;
    struct umbra_buf ubuf[] = {
        {.ptr=st->lay.uniforms, .sz=st->lay.uni.size, .read_only=1},
        {.ptr=buf, .sz=plane_sz * (size_t)st->fmt.planes, .row_bytes=rb},
    };
    st->prog->queue(st->prog, l, t, r, b, ubuf);
}

static struct umbra_builder *grad_lut_get_builder(struct slide *s, struct umbra_fmt fmt) {
    struct grad_lut_state *st = (struct grad_lut_state *)s;
    return umbra_draw_build(st->shader, st->coverage, st->blend, fmt, NULL);
}

static void grad_lut_free(struct slide *s) {
    struct grad_lut_state *st = (struct grad_lut_state *)s;
    if (st->prog) { st->prog->free(st->prog); }
    umbra_basic_block_free(st->bb);
    free(st->lay.uniforms);
    free(st);
}

static struct slide *make_grad_2stop(char const *title, uint32_t bg, umbra_shader_fn shader,
                                     struct umbra_fmt fmt, float const color[8],
                                     float const grad[4]) {
    struct grad_2stop_state *st = calloc(1, sizeof *st);
    st->shader = shader;
    st->coverage = NULL;
    st->blend = NULL;
    st->fmt = fmt;
    for (int i = 0; i < 8; i++) { st->color[i] = color[i]; }
    for (int i = 0; i < 4; i++) { st->grad[i] = grad[i]; }
    st->base = (struct slide){
        .title = title,
        .bg = bg,
        .init = grad_2stop_init,
        .prepare = grad_2stop_prepare,
        .draw = grad_2stop_draw,
        .free = grad_2stop_free,
        .get_builder = grad_2stop_get_builder,
    };
    return &st->base;
}

static struct slide *make_grad_lut(char const *title, uint32_t bg, umbra_shader_fn shader,
                                   struct umbra_fmt fmt, float const grad[4],
                                   float *lut, int lut_n) {
    struct grad_lut_state *st = calloc(1, sizeof *st);
    st->lut = lut;
    st->lut_n = lut_n;
    st->shader = shader;
    st->coverage = NULL;
    st->blend = NULL;
    st->fmt = fmt;
    for (int i = 0; i < 4; i++) { st->grad[i] = grad[i]; }
    st->base = (struct slide){
        .title = title,
        .bg = bg,
        .init = grad_lut_init,
        .prepare = grad_lut_prepare,
        .draw = grad_lut_draw,
        .free = grad_lut_free,
        .get_builder = grad_lut_get_builder,
    };
    return &st->base;
}

SLIDE(10, slide_gradient_linear_2) {
    (void)ctx;
    return make_grad_2stop("10. Linear Gradient (2-stop)", 0xff000000,
                           umbra_shader_linear_2, umbra_fmt_8888,
                           (float[]){1.0f, 0.4f, 0.0f, 1.0f, 0.0f, 0.3f, 1.0f, 1.0f},
                           (float[]){1.0f / 640.0f, 0.0f, 0.0f, 0.0f});
}

SLIDE(11, slide_gradient_radial_2) {
    (void)ctx;
    return make_grad_2stop("11. Radial Gradient (2-stop)", 0xff000000,
                           umbra_shader_radial_2, umbra_fmt_8888,
                           (float[]){1.0f, 1.0f, 0.9f, 1.0f, 0.05f, 0.0f, 0.15f, 1.0f},
                           (float[]){320.0f, 240.0f, 1.0f / 300.0f, 0.0f});
}

SLIDE(12, slide_gradient_linear_wide) {
    return make_grad_lut("12. Linear Gradient (wide gamut)", 0xff000000,
                         umbra_shader_linear_grad, umbra_fmt_8888,
                         (float[]){1.0f / 640.0f, 0.0f, 0.0f, 64.0f},
                         ctx->linear_lut, ctx->lut_n);
}

SLIDE(13, slide_gradient_radial_wide) {
    return make_grad_lut("13. Radial Gradient (wide gamut)", 0xff000000,
                         umbra_shader_radial_grad, umbra_fmt_8888,
                         (float[]){320.0f, 240.0f, 1.0f / 280.0f, 64.0f},
                         ctx->radial_lut, ctx->lut_n);
}
