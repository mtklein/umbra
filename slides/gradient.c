#include "slide.h"
#include <stdlib.h>

struct grad_2stop_state {
    struct slide base;

    int   w, h;
    int   is_radial, :32;

    union {
        struct umbra_shader_linear_2 linear;
        struct umbra_shader_radial_2 radial;
    } shader;

    struct umbra_fmt            fmt;
    struct umbra_draw_layout   lay;
    struct umbra_basic_block  *bb;
    struct umbra_program      *prog;
};

struct grad_lut_state {
    struct slide base;

    int    w, h;
    int    is_radial, :32;
    float *lut_data;

    union {
        struct umbra_shader_linear_grad linear;
        struct umbra_shader_radial_grad radial;
    } shader;

    struct umbra_fmt            fmt;
    struct umbra_draw_layout   lay;
    struct umbra_basic_block  *bb;
    struct umbra_program      *prog;
};

static void grad_2stop_init(struct slide *s, int w, int h) {
    struct grad_2stop_state *st = (struct grad_2stop_state *)s;
    st->w = w;
    st->h = h;
}

static void grad_2stop_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct grad_2stop_state *st = (struct grad_2stop_state *)s;
    if (st->fmt.name != fmt.name || !st->bb) {
        st->fmt = fmt;
        umbra_basic_block_free(st->bb);
        free(st->lay.uniforms);
        struct umbra_builder *b = umbra_draw_build(&st->shader.linear.base, NULL, NULL, fmt,
                                                    &st->lay);
        st->bb = umbra_basic_block(b);
        umbra_builder_free(b);
    }
    if (st->prog) { st->prog->free(st->prog); }
    st->prog = be->compile(be, st->bb);
}

static void grad_2stop_draw(struct slide *s, int frame, int l, int t, int r, int b, void *buf) {
    struct grad_2stop_state *st = (struct grad_2stop_state *)s;
    (void)frame;
    umbra_draw_fill(&st->lay, &st->shader.linear.base, NULL);
    struct umbra_buf ubuf[] = {
        {.ptr=st->lay.uniforms, .count=st->lay.uni.slots},
        {.ptr=buf, .count=st->w * st->h * st->fmt.planes, .stride=st->w},
    };
    st->prog->queue(st->prog, l, t, r, b, ubuf);
}

static struct umbra_builder *grad_2stop_get_builder(struct slide *s, struct umbra_fmt fmt) {
    struct grad_2stop_state *st = (struct grad_2stop_state *)s;
    return umbra_draw_build(&st->shader.linear.base, NULL, NULL, fmt, NULL);
}

static void grad_2stop_free(struct slide *s) {
    struct grad_2stop_state *st = (struct grad_2stop_state *)s;
    if (st->prog) { st->prog->free(st->prog); }
    umbra_basic_block_free(st->bb);
    free(st->lay.uniforms);
    free(st);
}

static void grad_lut_init(struct slide *s, int w, int h) {
    struct grad_lut_state *st = (struct grad_lut_state *)s;
    st->w = w;
    st->h = h;
}

static void grad_lut_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct grad_lut_state *st = (struct grad_lut_state *)s;
    if (st->fmt.name != fmt.name || !st->bb) {
        st->fmt = fmt;
        umbra_basic_block_free(st->bb);
        free(st->lay.uniforms);
        struct umbra_builder *b = umbra_draw_build(&st->shader.linear.base, NULL, NULL, fmt,
                                                    &st->lay);
        st->bb = umbra_basic_block(b);
        umbra_builder_free(b);
    }
    if (st->prog) { st->prog->free(st->prog); }
    st->prog = be->compile(be, st->bb);
}

static void grad_lut_draw(struct slide *s, int frame, int l, int t, int r, int b, void *buf) {
    struct grad_lut_state *st = (struct grad_lut_state *)s;
    (void)frame;
    umbra_draw_fill(&st->lay, &st->shader.linear.base, NULL);
    struct umbra_buf ubuf[] = {
        {.ptr=st->lay.uniforms, .count=st->lay.uni.slots},
        {.ptr=buf, .count=st->w * st->h * st->fmt.planes, .stride=st->w},
    };
    st->prog->queue(st->prog, l, t, r, b, ubuf);
}

static struct umbra_builder *grad_lut_get_builder(struct slide *s, struct umbra_fmt fmt) {
    struct grad_lut_state *st = (struct grad_lut_state *)s;
    return umbra_draw_build(&st->shader.linear.base, NULL, NULL, fmt, NULL);
}

static void grad_lut_free(struct slide *s) {
    struct grad_lut_state *st = (struct grad_lut_state *)s;
    if (st->prog) { st->prog->free(st->prog); }
    umbra_basic_block_free(st->bb);
    free(st->lay.uniforms);
    free(st->lut_data);
    free(st);
}

static struct slide *make_grad_2stop(char const *title, float const bg[4], _Bool is_radial,
                                     float const color[8], float const grad[3]) {
    struct grad_2stop_state *st = calloc(1, sizeof *st);
    st->is_radial = is_radial;
    if (is_radial) { st->shader.radial = umbra_shader_radial_2(grad, color); }
    else           { st->shader.linear = umbra_shader_linear_2(grad, color); }
    st->base = (struct slide){
        .title = title,
        .bg = {bg[0], bg[1], bg[2], bg[3]},
        .init = grad_2stop_init,
        .prepare = grad_2stop_prepare,
        .draw = grad_2stop_draw,
        .free = grad_2stop_free,
        .get_builder = grad_2stop_get_builder,
    };
    return &st->base;
}

static struct slide *make_grad_lut(char const *title, float const bg[4], _Bool is_radial,
                                   float const grad[4], float *lut, int lut_n) {
    struct grad_lut_state *st = calloc(1, sizeof *st);
    st->is_radial = is_radial;
    st->lut_data = lut;
    struct umbra_buf lut_buf = {.ptr = lut, .count = lut_n * 4};
    if (is_radial) { st->shader.radial = umbra_shader_radial_grad(grad, lut_buf); }
    else           { st->shader.linear = umbra_shader_linear_grad(grad, lut_buf); }
    st->base = (struct slide){
        .title = title,
        .bg = {bg[0], bg[1], bg[2], bg[3]},
        .init = grad_lut_init,
        .prepare = grad_lut_prepare,
        .draw = grad_lut_draw,
        .free = grad_lut_free,
        .get_builder = grad_lut_get_builder,
    };
    return &st->base;
}

SLIDE(slide_gradient_linear_2) {
    return make_grad_2stop("Linear Gradient (2-stop)", (float[]){0,0,0,1}, 0,
                           (float[]){1.0f, 0.4f, 0.0f, 1.0f, 0.0f, 0.3f, 1.0f, 1.0f},
                           (float[]){1.0f / 640.0f, 0.0f, 0.0f});
}

SLIDE(slide_gradient_radial_2) {
    return make_grad_2stop("Radial Gradient (2-stop)", (float[]){0,0,0,1}, 1,
                           (float[]){1.0f, 1.0f, 0.9f, 1.0f, 0.05f, 0.0f, 0.15f, 1.0f},
                           (float[]){320.0f, 240.0f, 1.0f / 300.0f});
}

struct grad_stops_state {
    struct slide base;

    int    w, h;
    float *colors_data, *pos_data;

    struct umbra_shader_linear_stops shader;

    struct umbra_fmt            fmt;
    struct umbra_draw_layout   lay;
    struct umbra_basic_block  *bb;
    struct umbra_program      *prog;
};

static void grad_stops_init(struct slide *s, int w, int h) {
    struct grad_stops_state *st = (struct grad_stops_state *)s;
    st->w = w;
    st->h = h;
}

static void grad_stops_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct grad_stops_state *st = (struct grad_stops_state *)s;
    if (st->fmt.name != fmt.name || !st->bb) {
        st->fmt = fmt;
        umbra_basic_block_free(st->bb);
        free(st->lay.uniforms);
        struct umbra_builder *b = umbra_draw_build(&st->shader.base, NULL, NULL, fmt, &st->lay);
        st->bb = umbra_basic_block(b);
        umbra_builder_free(b);
    }
    if (st->prog) { st->prog->free(st->prog); }
    st->prog = be->compile(be, st->bb);
}

static void grad_stops_draw(struct slide *s, int frame, int l, int t, int r, int b, void *buf) {
    struct grad_stops_state *st = (struct grad_stops_state *)s;
    (void)frame;
    umbra_draw_fill(&st->lay, &st->shader.base, NULL);
    struct umbra_buf ubuf[] = {
        {.ptr = st->lay.uniforms, .count = st->lay.uni.slots},
        {.ptr = buf, .count = st->w * st->h * st->fmt.planes, .stride = st->w},
    };
    st->prog->queue(st->prog, l, t, r, b, ubuf);
}

static struct umbra_builder *grad_stops_get_builder(struct slide *s, struct umbra_fmt fmt) {
    struct grad_stops_state *st = (struct grad_stops_state *)s;
    return umbra_draw_build(&st->shader.base, NULL, NULL, fmt, NULL);
}

static void grad_stops_free(struct slide *s) {
    struct grad_stops_state *st = (struct grad_stops_state *)s;
    if (st->prog) { st->prog->free(st->prog); }
    umbra_basic_block_free(st->bb);
    free(st->lay.uniforms);
    free(st->colors_data);
    free(st->pos_data);
    free(st);
}

// TODO: Vulkan draws this faster than Metal.  We've isolated this down to the
// shader dispatch speed itself, and think that the likely remaining gap has to
// do with scheduling memory loads and stores.
SLIDE(slide_gradient_linear_stops) {
    static float const colors[][4] = {
        {1.2f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.8f, 0.0f, 1.0f}, {0.0f, 1.2f, 0.0f, 1.0f},
        {0.0f, 0.8f, 1.2f, 1.0f}, {0.0f, 0.0f, 1.2f, 1.0f}, {0.8f, 0.0f, 1.0f, 1.0f},
    };
    enum { N = 6 };
    float *planar = malloc(N * 4 * sizeof(float));
    float *pos    = malloc(N * sizeof(float));
    for (int i = 0; i < N; i++) {
        for (int c = 0; c < 4; c++) {
            planar[c * N + i] = colors[i][c];
        }
        pos[i] = (float)i / (float)(N - 1);
    }
    struct grad_stops_state *st = calloc(1, sizeof *st);
    st->colors_data = planar;
    st->pos_data    = pos;
    st->shader = umbra_shader_linear_stops(
        (float[]){1.0f / 640.0f, 0.0f, 0.0f, (float)N},
        (struct umbra_buf){.ptr = planar, .count = N * 4},
        (struct umbra_buf){.ptr = pos,    .count = N});
    st->base = (struct slide){
        .title       = "Linear Gradient (loop stops)",
        .bg          = {0, 0, 0, 1},
        .init        = grad_stops_init,
        .prepare     = grad_stops_prepare,
        .draw        = grad_stops_draw,
        .free        = grad_stops_free,
        .get_builder = grad_stops_get_builder,
    };
    return &st->base;
}

SLIDE(slide_gradient_linear_wide) {
    static float const colors[][4] = {
        {1.2f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.8f, 0.0f, 1.0f}, {0.0f, 1.2f, 0.0f, 1.0f},
        {0.0f, 0.8f, 1.2f, 1.0f}, {0.0f, 0.0f, 1.2f, 1.0f}, {0.8f, 0.0f, 1.0f, 1.0f},
    };
    enum { LUT_N = 64 };
    float *lut = malloc(LUT_N * 4 * sizeof(float));
    umbra_gradient_lut_even(lut, LUT_N, 6, colors);
    return make_grad_lut("Linear Gradient (wide gamut)", (float[]){0,0,0,1}, 0,
                         (float[]){1.0f / 640.0f, 0.0f, 0.0f, (float)LUT_N},
                         lut, LUT_N);
}

SLIDE(slide_gradient_radial_wide) {
    static float const colors[][4] = {
        {1.5f, 1.5f, 1.2f, 1.0f}, {1.2f, 0.8f, 0.0f, 1.0f},
        {0.8f, 0.0f, 0.2f, 1.0f}, {0.05f, 0.0f, 0.15f, 1.0f},
    };
    enum { LUT_N = 64 };
    float *lut = malloc(LUT_N * 4 * sizeof(float));
    umbra_gradient_lut_even(lut, LUT_N, 4, colors);
    return make_grad_lut("Radial Gradient (wide gamut)", (float[]){0,0,0,1}, 1,
                         (float[]){320.0f, 240.0f, 1.0f / 280.0f, (float)LUT_N},
                         lut, LUT_N);
}
