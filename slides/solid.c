#include "slide.h"
#include <stdlib.h>

struct solid_state {
    struct slide base;

    float rx, ry, vx, vy;
    float rect_w, rect_h;
    float color[4];
    int   w, h;

    umbra_shader_fn    shader;
    umbra_coverage_fn  coverage;
    umbra_blend_fn     blend;

    struct umbra_fmt            fmt;
    struct umbra_draw_layout   lay;
    struct umbra_basic_block      *bb;
    struct umbra_program          *prog;
};

static void solid_init(struct slide *s, int w, int h) {
    struct solid_state *st = (struct solid_state *)s;
    st->w = w;
    st->h = h;
    st->rect_w = (float)w * 5.0f / 16.0f;
    st->rect_h = (float)h * 5.0f / 16.0f;
    st->rx = (float)w * 5.0f / 32.0f;
    st->ry = (float)h / 6.0f;
    st->vx = 1.5f;
    st->vy = 1.1f;
}

static void solid_animate(struct slide *s, float dt) {
    struct solid_state *st = (struct solid_state *)s;
    (void)dt;
    st->rx += st->vx;
    st->ry += st->vy;
    if (st->rx < 0.0f) { st->rx = 0.0f; st->vx = -st->vx; }
    if ((float)st->w < st->rx + st->rect_w) { st->rx = (float)st->w - st->rect_w; st->vx = -st->vx; }
    if (st->ry < 0.0f) { st->ry = 0.0f; st->vy = -st->vy; }
    if ((float)st->h < st->ry + st->rect_h) { st->ry = (float)st->h - st->rect_h; st->vy = -st->vy; }
}

static void solid_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct solid_state *st = (struct solid_state *)s;
    if (st->fmt.name != fmt.name || !st->bb) {
        st->fmt = fmt;
        umbra_basic_block_free(st->bb);
        if (st->lay.uni) { free(st->lay.uni->data); free(st->lay.uni); st->lay.uni = NULL; }
        struct umbra_builder *b = umbra_draw_build(st->shader, st->coverage, st->blend, fmt,
                                                    &st->lay);
        st->bb = umbra_basic_block(b);
        umbra_builder_free(b);
    }
    if (st->prog) { st->prog->free(st->prog); }
    st->prog = be->compile(be, st->bb);
}

static void solid_draw(struct slide *s, int l, int t, int r, int b, void *buf) {
    struct solid_state *st = (struct solid_state *)s;
    float rect[4] = { st->rx, st->ry, st->rx + st->rect_w, st->ry + st->rect_h };
    umbra_uniforms_fill_f32(st->lay.uni, st->lay.shader, st->color, 4);
    if (st->coverage) { umbra_uniforms_fill_f32(st->lay.uni, st->lay.coverage, rect, 4); }
    size_t pb = st->fmt.bpp;
    size_t plane_sz = (size_t)st->w * (size_t)st->h * pb;
    size_t rb = (size_t)st->w * pb;
    struct umbra_buf ubuf[] = {
        {.ptr=st->lay.uni->data, .sz=st->lay.uni->size, .read_only=1},
        {.ptr=buf, .sz=plane_sz * (size_t)st->fmt.planes, .row_bytes=rb},
    };
    st->prog->queue(st->prog, l, t, r, b, ubuf);
}

static struct umbra_builder *solid_get_builder(struct slide *s, struct umbra_fmt fmt) {
    struct solid_state *st = (struct solid_state *)s;
    return umbra_draw_build(st->shader, st->coverage, st->blend, fmt, NULL);
}

static void solid_free(struct slide *s) {
    struct solid_state *st = (struct solid_state *)s;
    if (st->prog) { st->prog->free(st->prog); }
    umbra_basic_block_free(st->bb);
    if (st->lay.uni) { free(st->lay.uni->data); free(st->lay.uni); }
    free(st);
}

struct slide *slide_solid(char const *, uint32_t, float const[4], umbra_coverage_fn,
                         umbra_blend_fn, struct umbra_fmt);

struct slide *slide_solid(char const *title, uint32_t bg, float const color[4],
                          umbra_coverage_fn coverage, umbra_blend_fn blend, struct umbra_fmt fmt) {
    struct solid_state *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid;
    st->coverage = coverage;
    st->blend = blend;
    st->fmt = fmt;
    for (int i = 0; i < 4; i++) { st->color[i] = color[i]; }
    st->base = (struct slide){
        .title = title,
        .bg = bg,
        .init = solid_init,
        .animate = solid_animate,
        .prepare = solid_prepare,
        .draw = solid_draw,
        .free = solid_free,
        .get_builder = solid_get_builder,
    };
    return &st->base;
}
