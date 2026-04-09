#include "slide.h"
#include <math.h>
#include <stdlib.h>

struct anim_state {
    struct slide base;

    float t;
    int   w, h, :32;

    struct umbra_fmt           fmt;
    struct umbra_draw_layout   lay;
    struct umbra_basic_block  *bb;
    struct umbra_program      *prog;
};

static void anim_init(struct slide *s, int w, int h) {
    struct anim_state *st = (struct anim_state *)s;
    st->w = w;
    st->h = h;
    st->t = 0.0f;
}

static void anim_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct anim_state *st = (struct anim_state *)s;
    if (st->fmt.name != fmt.name || !st->bb) {
        st->fmt = fmt;
        umbra_basic_block_free(st->bb);
        free(st->lay.uniforms);
        struct umbra_builder *b = umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_src,
                                                    fmt, &st->lay);
        st->bb = umbra_basic_block(b);
        umbra_builder_free(b);
    }
    if (st->prog) { st->prog->free(st->prog); }
    st->prog = be->compile(be, st->bb);
    // TODO: golden_test correctness depends on prepare() resetting t to 0.
    // Brittle: any slide that mutates state inside draw() has the same
    // coupling. The cleaner shape would be to pass a frame index into draw()
    // explicitly, but bench's draw() signature doesn't accept one.
    st->t = 0.0f;
}

static void anim_draw(struct slide *s, int l, int t, int r, int b, void *buf) {
    struct anim_state *st = (struct anim_state *)s;
    st->t += 0.016f;
    float color[4] = {
        0.5f + 0.5f * sinf(st->t),
        0.5f + 0.5f * sinf(st->t + 2.094f),
        0.5f + 0.5f * sinf(st->t + 4.189f),
        1.0f,
    };
    umbra_uniforms_fill_f32(st->lay.uniforms, st->lay.shader, color, 4);
    size_t pb = st->fmt.bpp;
    size_t plane_sz = (size_t)st->w * (size_t)st->h * pb;
    size_t rb = (size_t)st->w * pb;
    struct umbra_buf ubuf[] = {
        {.ptr=st->lay.uniforms, .sz=st->lay.uni.size, .read_only=1},
        {.ptr=buf, .sz=plane_sz * (size_t)st->fmt.planes, .row_bytes=rb},
    };
    st->prog->queue(st->prog, l, t, r, b, ubuf);
}

static struct umbra_builder *anim_get_builder(struct slide *s, struct umbra_fmt fmt) {
    (void)s;
    return umbra_draw_build(umbra_shader_solid, NULL, umbra_blend_src, fmt, NULL);
}

static void anim_free(struct slide *s) {
    struct anim_state *st = (struct anim_state *)s;
    if (st->prog) { st->prog->free(st->prog); }
    umbra_basic_block_free(st->bb);
    free(st->lay.uniforms);
    free(st);
}

SLIDE(slide_anim_t) {
    (void)ctx;
    struct anim_state *st = calloc(1, sizeof *st);
    st->fmt = umbra_fmt_8888;
    st->base = (struct slide){
        .title = "Animated (t in uniforms)",
        .bg = 0xff000000,
        .init = anim_init,
        .prepare = anim_prepare,
        .draw = anim_draw,
        .free = anim_free,
        .get_builder = anim_get_builder,
    };
    return &st->base;
}
