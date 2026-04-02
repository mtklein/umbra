#include "slide.h"
#include <stdlib.h>

typedef struct {
    slide base;

    float rx, ry, vx, vy;
    float rect_w, rect_h;
    float color[4];
    int   w, h;

    umbra_shader_fn    shader;
    umbra_coverage_fn  coverage;
    umbra_blend_fn     blend;

    umbra_draw_layout          lay;
    struct umbra_basic_block  *bb;
    struct umbra_program      *prog;
} solid_state;

static void solid_init(slide *s, int w, int h) {
    solid_state *st = (solid_state *)s;
    st->w = w;
    st->h = h;
    st->rect_w = (float)w * 5.0f / 16.0f;
    st->rect_h = (float)h * 5.0f / 16.0f;
    st->rx = (float)w * 5.0f / 32.0f;
    st->ry = (float)h / 6.0f;
    st->vx = 1.5f;
    st->vy = 1.1f;

    struct umbra_builder *b = umbra_draw_build(st->shader, st->coverage, st->blend, s->fmt,
                                                &st->lay);
    st->bb = umbra_basic_block(b);
    umbra_builder_free(b);
}

static void solid_animate(slide *s, float dt) {
    solid_state *st = (solid_state *)s;
    (void)dt;
    st->rx += st->vx;
    st->ry += st->vy;
    if (st->rx < 0.0f) { st->rx = 0.0f; st->vx = -st->vx; }
    if ((float)st->w < st->rx + st->rect_w) { st->rx = (float)st->w - st->rect_w; st->vx = -st->vx; }
    if (st->ry < 0.0f) { st->ry = 0.0f; st->vy = -st->vy; }
    if ((float)st->h < st->ry + st->rect_h) { st->ry = (float)st->h - st->rect_h; st->vy = -st->vy; }
}

static void solid_prepare(slide *s, int w, int h, struct umbra_backend *be) {
    (void)w; (void)h;
    solid_state *st = (solid_state *)s;
    if (st->prog) { st->prog->free(st->prog); }
    st->prog = be->compile(be, st->bb);
}

static void solid_draw(slide *s, int w, int h, int y0, int y1, void *buf) {
    solid_state *st = (solid_state *)s;
    float rect[4] = { st->rx, st->ry, st->rx + st->rect_w, st->ry + st->rect_h };
    umbra_uniforms_fill_f32(st->lay.uni, st->lay.shader, st->color, 4);
    if (st->coverage) { umbra_uniforms_fill_f32(st->lay.uni, st->lay.coverage, rect, 4); }
    size_t    pb = umbra_fmt_size(s->fmt);
    size_t plane_sz = (size_t)w * (size_t)h * pb;
    size_t rb = (size_t)w * pb;
    struct umbra_buf ubuf[] = {
        {.ptr=st->lay.uni->data, .sz=st->lay.uni->size, .read_only=1},
        {.ptr=buf, .sz=plane_sz * (s->fmt == umbra_fmt_fp16_planar ? 4 : 1), .row_bytes=rb},
    };
    st->prog->queue(st->prog, 0, y0, w, y1, ubuf);
}

static struct umbra_basic_block *solid_get_bb(slide *s) {
    return ((solid_state *)s)->bb;
}

static void solid_free(slide *s) {
    solid_state *st = (solid_state *)s;
    if (st->prog) { st->prog->free(st->prog); }
    umbra_basic_block_free(st->bb);
    if (st->lay.uni) { free(st->lay.uni->data); free(st->lay.uni); }
    free(st);
}

slide *slide_solid(char const *, uint32_t, float const[4], umbra_coverage_fn,
                   umbra_blend_fn, enum umbra_fmt);

slide *slide_solid(char const *title, uint32_t bg, float const color[4],
                   umbra_coverage_fn coverage, umbra_blend_fn blend, enum umbra_fmt fmt) {
    solid_state *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid;
    st->coverage = coverage;
    st->blend = blend;
    for (int i = 0; i < 4; i++) { st->color[i] = color[i]; }
    st->base = (slide){
        .title = title,
        .bg = bg,
        .fmt = fmt,
        .init = solid_init,
        .animate = solid_animate,
        .prepare = solid_prepare,
        .draw = solid_draw,
        .free = solid_free,
        .get_bb = solid_get_bb,
    };
    return &st->base;
}
