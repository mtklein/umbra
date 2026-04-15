// Circle-coverage slide.  A smooth-edge circle SDF as a coverage trait, paired
// with a solid shader and srcover blend.  The coverage math is deliberately
// interval-program-friendly (square, square_add, sqrt, clamp), so plan 02's
// quadtree dispatcher will have a first-class customer to plug into once it
// lands — no changes to this slide needed at that point.
#include "slide.h"
#include <math.h>
#include <stdlib.h>

struct circle_cov {
    struct umbra_coverage base;
    float cx, cy, r, pad_;
    int   off_, pad__;
};

static umbra_val32 circle_build_(struct umbra_coverage *s, struct umbra_builder *b,
                                 struct umbra_uniforms_layout *u,
                                 umbra_val32 x, umbra_val32 y) {
    struct circle_cov *self = (struct circle_cov *)s;
    self->off_ = umbra_uniforms_reserve_f32(u, 3);

    umbra_val32 const cx = umbra_uniform_32(b, (umbra_ptr32){0}, self->off_),
                      cy = umbra_uniform_32(b, (umbra_ptr32){0}, self->off_ + 1),
                      r  = umbra_uniform_32(b, (umbra_ptr32){0}, self->off_ + 2);
    // x, y arrive as f32 from umbra_draw_build's f32_from_i32 conversion.
    umbra_val32 const dx = umbra_sub_f32(b, x, cx),
                      dy = umbra_sub_f32(b, y, cy),
                      // mul(v, v) folds to square; add(square, square) folds to square_add.
                      d2 = umbra_add_f32(b, umbra_mul_f32(b, dx, dx),
                                            umbra_mul_f32(b, dy, dy)),
                      d  = umbra_sqrt_f32(b, d2),
                      // clamp(r - d, 0, 1) — 1-pixel smooth edge.
                      a  = umbra_min_f32(b, umbra_imm_f32(b, 1.0f),
                               umbra_max_f32(b, umbra_imm_f32(b, 0.0f),
                                                umbra_sub_f32(b, r, d)));
    return a;
}
static void circle_fill_(struct umbra_coverage const *s, void *uniforms) {
    struct circle_cov const *self = (struct circle_cov const *)s;
    float const vals[3] = {self->cx, self->cy, self->r};
    umbra_uniforms_fill_f32(uniforms, self->off_, vals, 3);
}

struct circle_slide {
    struct slide base;

    float cx0, cy0, vx, vy, r;
    int   w, h, pad_;

    struct umbra_shader_solid shader;
    struct circle_cov         cov;

    struct umbra_fmt          fmt;
    struct umbra_draw_layout  lay;
    struct umbra_flat_ir     *bb;
    struct umbra_program     *prog;
};

static void circle_init(struct slide *s, int w, int h) {
    struct circle_slide *st = (struct circle_slide *)s;
    st->w  = w;
    st->h  = h;
    st->r  = (float)(w < h ? w : h) * 0.18f;
    st->cx0 = (float)w * 0.3f;
    st->cy0 = (float)h * 0.4f;
    st->vx  = 1.7f;
    st->vy  = 1.3f;
}

static float bounce(float p0, float v, int frame, float range) {
    float p = fmodf(p0 + (float)frame * v, 2.0f * range);
    if (p < 0.0f) { p += 2.0f * range; }
    if (p > range) { p = 2.0f * range - p; }
    return p;
}

static void circle_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct circle_slide *st = (struct circle_slide *)s;
    if (st->fmt.name != fmt.name || !st->bb) {
        st->fmt = fmt;
        umbra_flat_ir_free(st->bb);
        free(st->lay.uniforms);
        struct umbra_builder *b = umbra_draw_build(&st->shader.base, &st->cov.base,
                                                   umbra_blend_srcover, fmt, &st->lay);
        st->bb = umbra_flat_ir(b);
        umbra_builder_free(b);
    }
    if (st->prog) { st->prog->free(st->prog); }
    st->prog = be->compile(be, st->bb);
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void circle_draw(struct slide *s, int frame, int l, int t, int r, int b, void *buf) {
    struct circle_slide *st = (struct circle_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);

    float const pad = st->r + 2.0f;
    st->cov.cx = pad + bounce(st->cx0 - pad, st->vx, frame, (float)st->w - 2.0f*pad);
    st->cov.cy = pad + bounce(st->cy0 - pad, st->vy, frame, (float)st->h - 2.0f*pad);
    st->cov.r  = st->r;

    umbra_draw_fill(&st->lay, &st->shader.base, &st->cov.base);
    struct umbra_buf ubuf[] = {
        {.ptr=st->lay.uniforms, .count=st->lay.uni.slots},
        {.ptr=buf, .count=st->w * st->h * st->fmt.planes, .stride=st->w},
    };
    st->prog->queue(st->prog, l, t, r, b, ubuf);
}

static int circle_get_builders(struct slide *s, struct umbra_fmt fmt,
                               struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct circle_slide *st = (struct circle_slide *)s;
    out[0] = umbra_draw_build(&st->shader.base, &st->cov.base,
                              umbra_blend_srcover, fmt, NULL);
    return out[0] ? 1 : 0;
}

static void circle_free(struct slide *s) {
    struct circle_slide *st = (struct circle_slide *)s;
    if (st->prog) { st->prog->free(st->prog); }
    umbra_flat_ir_free(st->bb);
    free(st->lay.uniforms);
    free(st);
}

SLIDE(slide_circle_coverage) {
    struct circle_slide *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid((float[]){0.95f, 0.45f, 0.10f, 1.0f});
    st->cov = (struct circle_cov){
        .base = {.build = circle_build_, .fill = circle_fill_},
    };
    st->base = (struct slide){
        .title = "Circle Coverage (interval-ready)",
        .bg = {0.08f, 0.10f, 0.14f, 1.0f},
        .init = circle_init,
        .prepare = circle_prepare,
        .draw = circle_draw,
        .free = circle_free,
        .get_builders = circle_get_builders,
    };
    return &st->base;
}
