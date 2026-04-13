#include "slide.h"
#include <math.h>
#include <stdlib.h>

struct anim_state {
    struct slide base;

    int   w, h;

    struct umbra_shader_solid  shader;
    struct umbra_fmt           fmt;
    struct umbra_draw_layout   lay;
    struct umbra_basic_block  *bb;
    struct umbra_program      *prog;
};

static void anim_init(struct slide *s, int w, int h) {
    struct anim_state *st = (struct anim_state *)s;
    st->w = w;
    st->h = h;
}

static void anim_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct anim_state *st = (struct anim_state *)s;
    if (st->fmt.name != fmt.name || !st->bb) {
        st->fmt = fmt;
        umbra_basic_block_free(st->bb);
        free(st->lay.uniforms);
        struct umbra_builder *b = umbra_draw_build(&st->shader.base, NULL, umbra_blend_src,
                                                    fmt, &st->lay);
        st->bb = umbra_basic_block(b);
        umbra_builder_free(b);
    }
    if (st->prog) { st->prog->free(st->prog); }
    st->prog = be->compile(be, st->bb);
}

static void anim_draw(struct slide *s, int frame, int l, int t, int r, int b, void *buf) {
    struct anim_state *st = (struct anim_state *)s;
    float ft = (float)frame * 0.016f;
    st->shader.color[0] = 0.5f + 0.5f * sinf(ft);
    st->shader.color[1] = 0.5f + 0.5f * sinf(ft + 2.094f);
    st->shader.color[2] = 0.5f + 0.5f * sinf(ft + 4.189f);
    st->shader.color[3] = 1.0f;
    umbra_draw_fill(&st->lay, &st->shader.base, NULL);
    struct umbra_buf ubuf[] = {
        {.ptr=st->lay.uniforms, .count=(int)(st->lay.uni.size / 4)},
        {.ptr=buf, .count=st->w * st->h * st->fmt.planes, .stride=st->w},
    };
    st->prog->queue(st->prog, l, t, r, b, ubuf);
}

static struct umbra_builder *anim_get_builder(struct slide *s, struct umbra_fmt fmt) {
    struct anim_state *st = (struct anim_state *)s;
    return umbra_draw_build(&st->shader.base, NULL, umbra_blend_src, fmt, NULL);
}

static void anim_free(struct slide *s) {
    struct anim_state *st = (struct anim_state *)s;
    if (st->prog) { st->prog->free(st->prog); }
    umbra_basic_block_free(st->bb);
    free(st->lay.uniforms);
    free(st);
}

SLIDE(slide_anim_t) {
    struct anim_state *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid((float[]){0, 0, 0, 1});
    st->base = (struct slide){
        .title = "Animated (t in uniforms)",
        .bg = {0, 0, 0, 1},
        .init = anim_init,
        .prepare = anim_prepare,
        .draw = anim_draw,
        .free = anim_free,
        .get_builder = anim_get_builder,
    };
    return &st->base;
}
