#include "slide.h"
#include <math.h>
#include <stdlib.h>

struct anim_slide {
    struct slide base;

    int   w, h;

    struct umbra_shader       *shader;
    struct umbra_fmt           fmt;
    struct umbra_flat_ir      *ir;
    struct umbra_program      *prog;
};

static void anim_init(struct slide *s, int w, int h) {
    struct anim_slide *st = (struct anim_slide *)s;
    st->w = w;
    st->h = h;
}

static void anim_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct anim_slide *st = (struct anim_slide *)s;
    if (st->fmt.name != fmt.name || !st->ir) {
        st->fmt = fmt;
        umbra_flat_ir_free(st->ir);
        struct umbra_builder *b = umbra_draw_builder(NULL, st->shader, umbra_blend_src, fmt);
        st->ir = umbra_flat_ir(b);
        umbra_builder_free(b);
    }
    umbra_program_free(st->prog);
    st->prog = be->compile(be, st->ir);
}

static void anim_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct anim_slide *st = (struct anim_slide *)s;
    float ft = (float)secs;
    umbra_color const c = {
        0.5f + 0.5f * sinf(ft),
        0.5f + 0.5f * sinf(ft + 2.094f),
        0.5f + 0.5f * sinf(ft + 4.189f),
        1.0f,
    };
    umbra_shader_free(st->shader);
    st->shader = umbra_shader_solid(c);
    struct umbra_buf ubuf[] = {
        {.ptr=buf, .count=st->w * st->h * st->fmt.planes, .stride=st->w},
        {0},
        st->shader->uniforms,
    };
    st->prog->queue(st->prog, l, t, r, b, ubuf);
}

static int anim_get_builders(struct slide *s, struct umbra_fmt fmt,
                             struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct anim_slide *st = (struct anim_slide *)s;
    out[0] = umbra_draw_builder(NULL, st->shader, umbra_blend_src, fmt);
    return out[0] ? 1 : 0;
}

static void anim_free(struct slide *s) {
    struct anim_slide *st = (struct anim_slide *)s;
    umbra_program_free(st->prog);
    umbra_flat_ir_free(st->ir);
    umbra_shader_free(st->shader);
    free(st);
}

SLIDE(slide_anim_t) {
    struct anim_slide *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid((umbra_color){0, 0, 0, 1});
    st->base = (struct slide){
        .title = "Animated (t in uniforms)",
        .bg = {0, 0, 0, 1},
        .init = anim_init,
        .prepare = anim_prepare,
        .draw = anim_draw,
        .free = anim_free,
        .get_builders = anim_get_builders,
    };
    return &st->base;
}
