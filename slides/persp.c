#include "slide.h"
#include "text.h"
#include <stdlib.h>

struct persp_state {
    struct slide base;

    float            persp_t;
    float            mat[11];
    struct text_cov *bitmap;
    int              w, h;
    float            color[4];

    umbra_shader_fn    shader;
    umbra_coverage_fn  coverage;
    umbra_blend_fn     blend;

    struct umbra_fmt            fmt;
    struct umbra_draw_layout   lay;
    struct umbra_basic_block  *bb;
    struct umbra_program      *prog;
};

static void persp_init(struct slide *s, int w, int h) {
    struct persp_state *st = (struct persp_state *)s;
    st->w = w;
    st->h = h;
    st->persp_t = 0.0f;
    slide_perspective_matrix(st->mat, 0.0f, w, h, st->bitmap->w, st->bitmap->h);
}

static void persp_animate(struct slide *s, float dt) {
    struct persp_state *st = (struct persp_state *)s;
    (void)dt;
    st->persp_t += 0.016f;
    slide_perspective_matrix(st->mat, st->persp_t, st->w, st->h, st->bitmap->w,
                             st->bitmap->h);
}

static void persp_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct persp_state *st = (struct persp_state *)s;
    if (st->fmt.name != fmt.name || !st->bb) {
        st->fmt = fmt;
        umbra_basic_block_free(st->bb);
        if (st->lay.uni) { free(st->lay.data); free(st->lay.uni); st->lay.uni = NULL; }
        struct umbra_builder *b = umbra_draw_build(st->shader, st->coverage, st->blend, fmt,
                                                    &st->lay);
        st->bb = umbra_basic_block(b);
        umbra_builder_free(b);
    }
    if (st->prog) { st->prog->free(st->prog); }
    st->prog = be->compile(be, st->bb);
}

static void persp_draw(struct slide *s, int l, int t, int r, int b, void *buf) {
    struct persp_state *st = (struct persp_state *)s;
    umbra_uniforms_fill_f32(st->lay.data, st->lay.shader,   st->color, 4);
    umbra_uniforms_fill_f32(st->lay.data, st->lay.coverage, st->mat, 11);
    umbra_uniforms_fill_ptr(st->lay.data, (st->lay.coverage + 44 + 7) & ~(size_t)7,
                  (struct umbra_buf){.ptr=st->bitmap->data,
                                     .sz=(size_t)(st->bitmap->w * st->bitmap->h * 2)});
    size_t    pb = st->fmt.bpp;
    size_t plane_sz = (size_t)st->w * (size_t)st->h * pb;
    size_t rb = (size_t)st->w * pb;
    struct umbra_buf ubuf[] = {
        {.ptr=st->lay.data, .sz=st->lay.uni->size, .read_only=1},
        {.ptr=buf, .sz=plane_sz * (size_t)st->fmt.planes, .row_bytes=rb},
    };
    st->prog->queue(st->prog, l, t, r, b, ubuf);
}

static struct umbra_builder *persp_get_builder(struct slide *s, struct umbra_fmt fmt) {
    struct persp_state *st = (struct persp_state *)s;
    return umbra_draw_build(st->shader, st->coverage, st->blend, fmt, NULL);
}

static void persp_free(struct slide *s) {
    struct persp_state *st = (struct persp_state *)s;
    if (st->prog) { st->prog->free(st->prog); }
    umbra_basic_block_free(st->bb);
    if (st->lay.uni) { free(st->lay.data); free(st->lay.uni); }
    free(st);
}

struct slide *slide_persp(struct text_cov *);

struct slide *slide_persp(struct text_cov *bitmap) {
    struct persp_state *st = calloc(1, sizeof *st);
    st->bitmap = bitmap;
    st->shader = umbra_shader_solid;
    st->coverage = umbra_coverage_bitmap_matrix;
    st->blend = umbra_blend_srcover;
    st->fmt = umbra_fmt_8888;
    st->color[0] = 1.0f; st->color[1] = 0.8f; st->color[2] = 0.2f; st->color[3] = 1.0f;
    st->base = (struct slide){
        .title = "9. Perspective Text",
        .bg = 0xff0a0a1e,
        .init = persp_init,
        .animate = persp_animate,
        .prepare = persp_prepare,
        .draw = persp_draw,
        .free = persp_free,
        .get_builder = persp_get_builder,
    };
    return &st->base;
}
