#include "slide.h"
#include "text.h"
#include <stdlib.h>

struct persp_slide {
    struct slide base;

    struct text_cov *bitmap;
    int              w, h;

    struct umbra_shader        *shader;
    struct umbra_coverage      *cov;

    struct umbra_fmt            fmt;
    struct umbra_program       *prog;
};

static void persp_init(struct slide *s, int w, int h) {
    struct persp_slide *st = (struct persp_slide *)s;
    st->w = w;
    st->h = h;
}

static void persp_prepare(struct slide *s, struct umbra_backend *be,
                          struct umbra_fmt fmt) {
    struct persp_slide *st = (struct persp_slide *)s;
    umbra_program_free(st->prog);
    umbra_coverage_free(st->cov);
    struct umbra_bitmap bmp = {
        .buf = {.ptr = st->bitmap->data, .count = st->bitmap->w * st->bitmap->h},
        .w   = st->bitmap->w,
        .h   = st->bitmap->h,
    };
    st->cov = umbra_coverage_bitmap_matrix((struct umbra_matrix){0}, bmp);
    st->fmt = fmt;
    struct umbra_builder *b = umbra_draw_builder(st->cov, st->shader,
                                                 umbra_blend_srcover, fmt);
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    st->prog = be->compile(be, ir);
    umbra_flat_ir_free(ir);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void persp_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct persp_slide *st = (struct persp_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    struct umbra_matrix mat;
    slide_perspective_matrix(&mat, (float)secs, st->w, st->h,
                             st->bitmap->w, st->bitmap->h);
    struct umbra_bitmap bmp = {
        .buf = {.ptr = st->bitmap->data, .count = st->bitmap->w * st->bitmap->h},
        .w   = st->bitmap->w,
        .h   = st->bitmap->h,
    };
    umbra_coverage_free(st->cov);
    st->cov = umbra_coverage_bitmap_matrix(mat, bmp);
    struct umbra_buf ubuf[] = {
        {.ptr=buf, .count=st->w * st->h * st->fmt.planes, .stride=st->w},
        umbra_coverage_uniforms(st->cov),
        umbra_shader_uniforms(st->shader),
    };
    st->prog->queue(st->prog, l, t, r, b, ubuf);
}

static int persp_get_builders(struct slide *s, struct umbra_fmt fmt,
                              struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct persp_slide *st = (struct persp_slide *)s;
    out[0] = umbra_draw_builder(st->cov, st->shader, umbra_blend_srcover, fmt);
    return out[0] ? 1 : 0;
}

static void persp_free(struct slide *s) {
    struct persp_slide *st = (struct persp_slide *)s;
    umbra_program_free(st->prog);
    umbra_shader_free  (st->shader);
    umbra_coverage_free(st->cov);
    free(st);
}

SLIDE(slide_persp) {
    struct persp_slide *st = calloc(1, sizeof *st);
    st->bitmap = text_shared_bitmap();
    st->shader = umbra_shader_solid((umbra_color){1.0f, 0.8f, 0.2f, 1.0f});
    st->cov    = umbra_coverage_bitmap_matrix((struct umbra_matrix){0}, (struct umbra_bitmap){0});
    st->base = (struct slide){
        .title = "Perspective Text",
        .bg = {0.12f, 0.04f, 0.04f, 1},
        .init = persp_init,
        .prepare = persp_prepare,
        .draw = persp_draw,
        .free = persp_free,
        .get_builders = persp_get_builders,
    };
    return &st->base;
}
