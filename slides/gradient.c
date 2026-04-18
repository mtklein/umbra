#include "slide.h"
#include <stdlib.h>

struct grad_slide {
    struct slide base;

    int    w, h;
    float *colors_data, *pos_data, *lut_data;

    struct umbra_shader        *shader;

    struct umbra_fmt            fmt;
    struct umbra_flat_ir       *ir;
    struct umbra_program       *prog;
};

static void grad_init(struct slide *s, int w, int h) {
    struct grad_slide *st = (struct grad_slide *)s;
    st->w = w;
    st->h = h;
}

static void grad_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct grad_slide *st = (struct grad_slide *)s;
    if (st->fmt.name != fmt.name || !st->ir) {
        st->fmt = fmt;
        umbra_flat_ir_free(st->ir);
        struct umbra_builder *b = umbra_draw_builder(NULL, st->shader, NULL, fmt);
        st->ir = umbra_flat_ir(b);
        umbra_builder_free(b);
    }
    umbra_program_free(st->prog);
    st->prog = be->compile(be, st->ir);
}

static void grad_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct grad_slide *st = (struct grad_slide *)s;
    (void)secs;
    struct umbra_buf ubuf[] = {
        {.ptr=buf, .count=st->w * st->h * st->fmt.planes, .stride=st->w},
        {0},
        st->shader->uniforms,
    };
    st->prog->queue(st->prog, l, t, r, b, ubuf);
}

static int grad_get_builders(struct slide *s, struct umbra_fmt fmt,
                             struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct grad_slide *st = (struct grad_slide *)s;
    out[0] = umbra_draw_builder(NULL, st->shader, NULL, fmt);
    return out[0] ? 1 : 0;
}

static void grad_free(struct slide *s) {
    struct grad_slide *st = (struct grad_slide *)s;
    umbra_program_free(st->prog);
    umbra_flat_ir_free(st->ir);
    umbra_shader_free(st->shader);
    free(st->colors_data);
    free(st->pos_data);
    free(st->lut_data);
    free(st);
}

static struct slide* make_grad(char const *title, struct umbra_shader *shader,
                               float *colors_data, float *pos_data, float *lut_data) {
    struct grad_slide *st = calloc(1, sizeof *st);
    st->shader      = shader;
    st->colors_data = colors_data;
    st->pos_data    = pos_data;
    st->lut_data    = lut_data;
    st->base = (struct slide){
        .title        = title,
        .bg           = {0, 0, 0, 1},
        .init         = grad_init,
        .prepare      = grad_prepare,
        .draw         = grad_draw,
        .free         = grad_free,
        .get_builders = grad_get_builders,
    };
    return &st->base;
}

SLIDE(slide_gradient_linear_two_stop) {
    return make_grad("Gradient Linear Two-Stop",
        umbra_shader_gradient_linear_two_stops((umbra_point){0, 0}, (umbra_point){640, 0},
                                              (umbra_color){1.0f, 0.4f, 0.0f, 1.0f},
                                              (umbra_color){0.0f, 0.3f, 1.0f, 1.0f}),
        NULL, NULL, NULL);
}

// Fixed: was ~131 µs Metal vs ~102 µs Vulkan.  Now ~62 and ~66 respectively,
// thanks to umbra_if() wrapping the gathers+lerps so GPU backends can branch-skip
// them when the pixel isn't in the current gradient segment.
SLIDE(slide_gradient_linear) {
    static umbra_color const colors[] = {
        {1.2f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.8f, 0.0f, 1.0f}, {0.0f, 1.2f, 0.0f, 1.0f},
        {0.0f, 0.8f, 1.2f, 1.0f}, {0.0f, 0.0f, 1.2f, 1.0f}, {0.8f, 0.0f, 1.0f, 1.0f},
    };
    enum { N = 6 };
    float *planar = malloc(N * 4 * sizeof(float));
    float *pos    = malloc(N * sizeof(float));
    for (int i = 0; i < N; i++) {
        for (int c = 0; c < 4; c++) {
            planar[c * N + i] = (&colors[i].r)[c];
        }
        pos[i] = (float)i / (float)(N - 1);
    }
    return make_grad("Gradient Linear",
        umbra_shader_gradient_linear(
            (umbra_point){0, 0}, (umbra_point){640, 0},
            (struct umbra_buf){.ptr=planar, .count=N * 4},
            (struct umbra_buf){.ptr=pos,    .count=N}),
        planar, pos, NULL);
}

SLIDE(slide_gradient_linear_lut) {
    static umbra_color const colors[] = {
        {1.2f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.8f, 0.0f, 1.0f}, {0.0f, 1.2f, 0.0f, 1.0f},
        {0.0f, 0.8f, 1.2f, 1.0f}, {0.0f, 0.0f, 1.2f, 1.0f}, {0.8f, 0.0f, 1.0f, 1.0f},
    };
    enum { LUT_N = 64 };
    float *lut = malloc(LUT_N * 4 * sizeof(float));
    umbra_gradient_lut_even(lut, LUT_N, 6, colors);
    return make_grad("Gradient Linear LUT",
        umbra_shader_gradient_linear_lut((umbra_point){0, 0}, (umbra_point){640, 0},
                                         (struct umbra_buf){.ptr=lut, .count=LUT_N * 4}),
        NULL, NULL, lut);
}

SLIDE(slide_gradient_linear_evenly_spaced) {
    static umbra_color const colors[] = {
        {1.2f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.8f, 0.0f, 1.0f}, {0.0f, 1.2f, 0.0f, 1.0f},
        {0.0f, 0.8f, 1.2f, 1.0f}, {0.0f, 0.0f, 1.2f, 1.0f}, {0.8f, 0.0f, 1.0f, 1.0f},
    };
    enum { N = 6 };
    float *planar = malloc(N * 4 * sizeof(float));
    for (int i = 0; i < N; i++) {
        for (int c = 0; c < 4; c++) {
            planar[c * N + i] = (&colors[i].r)[c];
        }
    }
    return make_grad("Gradient Linear Evenly-Spaced",
        umbra_shader_gradient_linear_evenly_spaced_stops(
            (umbra_point){0, 0}, (umbra_point){640, 0},
            (struct umbra_buf){.ptr=planar, .count=N * 4}),
        planar, NULL, NULL);
}

SLIDE(slide_gradient_radial_two_stop) {
    return make_grad("Gradient Radial Two-Stop",
        umbra_shader_gradient_radial_two_stops((umbra_point){320, 240}, 300.0f,
                                              (umbra_color){1.0f, 1.0f, 0.9f, 1.0f},
                                              (umbra_color){0.05f, 0.0f, 0.15f, 1.0f}),
        NULL, NULL, NULL);
}

SLIDE(slide_gradient_radial) {
    static umbra_color const colors[] = {
        {1.5f, 1.5f, 1.2f, 1.0f}, {1.2f, 0.8f, 0.0f, 1.0f},
        {0.8f, 0.0f, 0.2f, 1.0f}, {0.05f, 0.0f, 0.15f, 1.0f},
    };
    enum { N = 4 };
    float *planar = malloc(N * 4 * sizeof(float));
    float *pos    = malloc(N * sizeof(float));
    for (int i = 0; i < N; i++) {
        for (int c = 0; c < 4; c++) {
            planar[c * N + i] = (&colors[i].r)[c];
        }
        pos[i] = (float)i / (float)(N - 1);
    }
    return make_grad("Gradient Radial",
        umbra_shader_gradient_radial(
            (umbra_point){320, 240}, 280.0f,
            (struct umbra_buf){.ptr=planar, .count=N * 4},
            (struct umbra_buf){.ptr=pos,    .count=N}),
        planar, pos, NULL);
}

SLIDE(slide_gradient_radial_lut) {
    static umbra_color const colors[] = {
        {1.5f, 1.5f, 1.2f, 1.0f}, {1.2f, 0.8f, 0.0f, 1.0f},
        {0.8f, 0.0f, 0.2f, 1.0f}, {0.05f, 0.0f, 0.15f, 1.0f},
    };
    enum { LUT_N = 64 };
    float *lut = malloc(LUT_N * 4 * sizeof(float));
    umbra_gradient_lut_even(lut, LUT_N, 4, colors);
    return make_grad("Gradient Radial LUT",
        umbra_shader_gradient_radial_lut((umbra_point){320, 240}, 280.0f,
                                         (struct umbra_buf){.ptr=lut, .count=LUT_N * 4}),
        NULL, NULL, lut);
}

SLIDE(slide_gradient_radial_evenly_spaced) {
    static umbra_color const colors[] = {
        {1.5f, 1.5f, 1.2f, 1.0f}, {1.2f, 0.8f, 0.0f, 1.0f},
        {0.8f, 0.0f, 0.2f, 1.0f}, {0.05f, 0.0f, 0.15f, 1.0f},
    };
    enum { N = 4 };
    float *planar = malloc(N * 4 * sizeof(float));
    for (int i = 0; i < N; i++) {
        for (int c = 0; c < 4; c++) {
            planar[c * N + i] = (&colors[i].r)[c];
        }
    }
    return make_grad("Gradient Radial Evenly-Spaced",
        umbra_shader_gradient_radial_evenly_spaced_stops(
            (umbra_point){320, 240}, 280.0f,
            (struct umbra_buf){.ptr=planar, .count=N * 4}),
        planar, NULL, NULL);
}
