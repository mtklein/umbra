#include "slide.h"
#include "text.h"
#include <stdlib.h>

struct text_state {
    struct slide base;

    struct text_cov *tc;
    float            color[4];
    int              w, h;

    umbra_shader_fn    shader;
    umbra_coverage_fn  coverage;
    umbra_blend_fn     blend;

    struct umbra_fmt            fmt;
    struct umbra_draw_layout   lay;
    struct umbra_basic_block  *bb;
    struct umbra_program      *prog;
};

static void text_init(struct slide *s, int w, int h) {
    struct text_state *st = (struct text_state *)s;
    st->w = w;
    st->h = h;
}

static void text_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct text_state *st = (struct text_state *)s;
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

static void text_draw(struct slide *s, int frame, int l, int t, int r, int b, void *buf) {
    struct text_state *st = (struct text_state *)s;
    (void)frame;
    umbra_uniforms_fill_f32(st->lay.uniforms, st->lay.shader, st->color, 4);
    umbra_uniforms_fill_ptr(st->lay.uniforms, st->lay.coverage,
                  (struct umbra_buf){.ptr=st->tc->data, .sz=(size_t)(st->w * st->h * 2), .row_bytes=(size_t)st->w * 2});
    size_t    pb = st->fmt.bpp;
    size_t plane_sz = (size_t)st->w * (size_t)st->h * pb;
    size_t rb = (size_t)st->w * pb;
    struct umbra_buf ubuf[] = {
        {.ptr=st->lay.uniforms, .sz=st->lay.uni.size, .read_only=1},
        {.ptr=buf, .sz=plane_sz * (size_t)st->fmt.planes, .row_bytes=rb},
    };
    st->prog->queue(st->prog, l, t, r, b, ubuf);
}

static struct umbra_builder *text_get_builder(struct slide *s, struct umbra_fmt fmt) {
    struct text_state *st = (struct text_state *)s;
    return umbra_draw_build(st->shader, st->coverage, st->blend, fmt, NULL);
}

static void text_free(struct slide *s) {
    struct text_state *st = (struct text_state *)s;
    if (st->prog) { st->prog->free(st->prog); }
    umbra_basic_block_free(st->bb);
    free(st->lay.uniforms);
    free(st);
}

SLIDE(slide_text_bitmap) {
    struct text_state *st = calloc(1, sizeof *st);
    st->tc = ctx->bitmap_cov;
    st->shader = umbra_shader_solid;
    st->coverage = umbra_coverage_bitmap;
    st->blend = umbra_blend_srcover;
    st->fmt = umbra_fmt_8888;
    st->color[0] = 1.0f; st->color[1] = 1.0f; st->color[2] = 1.0f; st->color[3] = 1.0f;
    st->base = (struct slide){
        .title = "Text (8-bit AA)",
        .bg = 0xff1a1a2e,
        .init = text_init,
        .prepare = text_prepare,
        .draw = text_draw,
        .free = text_free,
        .get_builder = text_get_builder,
    };
    return &st->base;
}

SLIDE(slide_text_sdf) {
    struct text_state *st = calloc(1, sizeof *st);
    st->tc = ctx->sdf_cov;
    st->shader = umbra_shader_solid;
    st->coverage = umbra_coverage_sdf;
    st->blend = umbra_blend_srcover;
    st->fmt = umbra_fmt_8888;
    st->color[0] = 0.2f; st->color[1] = 0.8f; st->color[2] = 1.0f; st->color[3] = 1.0f;
    st->base = (struct slide){
        .title = "Text (SDF)",
        .bg = 0xff1a1a2e,
        .init = text_init,
        .prepare = text_prepare,
        .draw = text_draw,
        .free = text_free,
        .get_builder = text_get_builder,
    };
    return &st->base;
}
