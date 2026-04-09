#include "slide.h"
#include <math.h>
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
    struct solid_state *st = (struct solid_state *)s;
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

static void solid_draw(struct slide *s, int frame, int l, int t, int r, int b, void *buf) {
    struct solid_state *st = (struct solid_state *)s;
    float rx = bounce(st->rx, st->vx, frame, (float)st->w - st->rect_w);
    float ry = bounce(st->ry, st->vy, frame, (float)st->h - st->rect_h);
    float rect[4] = { rx, ry, rx + st->rect_w, ry + st->rect_h };
    umbra_uniforms_fill_f32(st->lay.uniforms, st->lay.shader, st->color, 4);
    if (st->coverage) { umbra_uniforms_fill_f32(st->lay.uniforms, st->lay.coverage, rect, 4); }
    size_t pb = st->fmt.bpp;
    size_t plane_sz = (size_t)st->w * (size_t)st->h * pb;
    size_t rb = (size_t)st->w * pb;
    struct umbra_buf ubuf[] = {
        {.ptr=st->lay.uniforms, .sz=st->lay.uni.size, .read_only=1},
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
    free(st->lay.uniforms);
    free(st);
}

static struct slide *make_solid(char const *title, uint32_t bg, float const color[4],
                                umbra_coverage_fn coverage, umbra_blend_fn blend,
                                struct umbra_fmt fmt) {
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
        .prepare = solid_prepare,
        .draw = solid_draw,
        .free = solid_free,
        .get_builder = solid_get_builder,
    };
    return &st->base;
}

SLIDE(slide_solid_src) {
    (void)ctx;
    return make_solid("Solid Fill (src)", 0xff202020,
                      (float[]){0.0f, 0.6f, 1.0f, 1.0f},
                      umbra_coverage_rect, umbra_blend_src, umbra_fmt_8888);
}

SLIDE(slide_solid_srcover) {
    (void)ctx;
    return make_solid("Source Over (srcover)", 0xff00ff00,
                      (float[]){0.45f, 0.0f, 0.0f, 0.5f},
                      umbra_coverage_rect, umbra_blend_srcover, umbra_fmt_8888);
}

SLIDE(slide_solid_dstover) {
    (void)ctx;
    return make_solid("Destination Over (dstover)", 0xc0008000,
                      (float[]){0.0f, 0.0f, 0.9f, 0.9f},
                      umbra_coverage_rect, umbra_blend_dstover, umbra_fmt_8888);
}

SLIDE(slide_solid_multiply) {
    (void)ctx;
    return make_solid("Multiply Blend", 0xff804020,
                      (float[]){1.0f, 0.5f, 0.0f, 1.0f},
                      umbra_coverage_rect, umbra_blend_multiply, umbra_fmt_8888);
}

SLIDE(slide_solid_full_cov) {
    (void)ctx;
    return make_solid("Full Coverage (no rect clip)", 0xffffffff,
                      (float[]){0.15f, 0.0f, 0.3f, 0.3f},
                      NULL, umbra_blend_srcover, umbra_fmt_8888);
}

SLIDE(slide_solid_no_blend) {
    (void)ctx;
    return make_solid("No Blend (direct paint)", 0xff000000,
                      (float[]){0.9f, 0.4f, 0.1f, 1.0f},
                      umbra_coverage_rect, NULL, umbra_fmt_8888);
}
