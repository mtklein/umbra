#include "slide.h"
#include "text.h"
#include <stdlib.h>

typedef struct {
    text_cov *tc;
    float     color[4];

    umbra_shader_fn    shader;
    umbra_coverage_fn  coverage;
    umbra_blend_fn     blend;

    umbra_draw_layout          lay;
    struct umbra_basic_block  *bb;
    struct umbra_program      *prog;
} text_state;

static void text_init(slide *s, int w, int h) {
    (void)w; (void)h;
    text_state *st = s->state;
    struct umbra_builder *b = umbra_draw_build(st->shader, st->coverage, st->blend, s->fmt,
                                                &st->lay);
    st->bb = umbra_basic_block(b);
    umbra_builder_free(b);
}

static void text_prepare(slide *s, int w, int h, struct umbra_backend *be) {
    (void)w; (void)h;
    text_state *st = s->state;
    if (st->prog) { st->prog->free(st->prog); }
    st->prog = be->compile(be, st->bb);
}

static void text_draw(slide *s, int w, int h, int y0, int y1, void *buf) {
    text_state *st = s->state;
    umbra_uniforms_fill_f32(st->lay.uni, st->lay.shader, st->color, 4);
    umbra_uniforms_fill_ptr(st->lay.uni, st->lay.coverage,
                  (struct umbra_buf){.ptr=st->tc->data, .sz=(size_t)(w * h * 2), .row_bytes=(size_t)w * 2});
    size_t    pb = umbra_fmt_size(s->fmt);
    size_t plane_sz = (size_t)w * (size_t)h * pb;
    size_t rb = (size_t)w * pb;
    struct umbra_buf ubuf[] = {
        {.ptr=st->lay.uni->data, .sz=st->lay.uni->size, .read_only=1},
        {.ptr=buf, .sz=plane_sz * (s->fmt == umbra_fmt_fp16_planar ? 4 : 1), .row_bytes=rb},
    };
    st->prog->queue(st->prog, 0, y0, w, y1, ubuf);
}

static struct umbra_basic_block *text_get_bb(slide *s) {
    return ((text_state *)s->state)->bb;
}

static void text_cleanup(slide *s) {
    text_state *st = s->state;
    if (st->prog) { st->prog->free(st->prog); }
    umbra_basic_block_free(st->bb);
    if (st->lay.uni) { free(st->lay.uni->data); free(st->lay.uni); }
    free(st);
    s->state = NULL;
}

slide slide_text_bitmap(text_cov *);
slide slide_text_sdf(text_cov *);

slide slide_text_bitmap(text_cov *tc) {
    text_state *st = calloc(1, sizeof *st);
    st->tc = tc;
    st->shader = umbra_shader_solid;
    st->coverage = umbra_coverage_bitmap;
    st->blend = umbra_blend_srcover;
    st->color[0] = 1.0f; st->color[1] = 1.0f; st->color[2] = 1.0f; st->color[3] = 1.0f;
    return (slide){
        .title = "7. Text (8-bit AA)",
        .fmt = umbra_fmt_8888,
        .bg = 0xff1a1a2e,
        .init = text_init,
        .prepare = text_prepare,
        .draw = text_draw,
        .cleanup = text_cleanup,
        .get_bb = text_get_bb,
        .state = st,
    };
}

slide slide_text_sdf(text_cov *tc) {
    text_state *st = calloc(1, sizeof *st);
    st->tc = tc;
    st->shader = umbra_shader_solid;
    st->coverage = umbra_coverage_sdf;
    st->blend = umbra_blend_srcover;
    st->color[0] = 0.2f; st->color[1] = 0.8f; st->color[2] = 1.0f; st->color[3] = 1.0f;
    return (slide){
        .title = "8. Text (SDF)",
        .fmt = umbra_fmt_8888,
        .bg = 0xff1a1a2e,
        .init = text_init,
        .prepare = text_prepare,
        .draw = text_draw,
        .cleanup = text_cleanup,
        .get_bb = text_get_bb,
        .state = st,
    };
}
