#include "slide.h"
#include <stdlib.h>

struct grad_2stop_slide {
    struct slide base;

    int   w, h;
    int   is_radial, :32;

    struct umbra_shader        *shader;

    struct umbra_fmt            fmt;
    struct umbra_draw_layout   lay;
    struct umbra_flat_ir  *ir;
    struct umbra_program      *prog;
};

struct grad_lut_slide {
    struct slide base;

    int    w, h;
    int    is_radial, :32;
    float *lut_data;

    struct umbra_shader        *shader;

    struct umbra_fmt            fmt;
    struct umbra_draw_layout   lay;
    struct umbra_flat_ir  *ir;
    struct umbra_program      *prog;
};

static void grad_2stop_init(struct slide *s, int w, int h) {
    struct grad_2stop_slide *st = (struct grad_2stop_slide *)s;
    st->w = w;
    st->h = h;
}

static void grad_2stop_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct grad_2stop_slide *st = (struct grad_2stop_slide *)s;
    if (st->fmt.name != fmt.name || !st->ir) {
        st->fmt = fmt;
        umbra_flat_ir_free(st->ir);
        free(st->lay.uniforms);
        struct umbra_builder *b = umbra_draw_builder(NULL, st->shader, NULL, fmt,
                                                    &st->lay);
        st->ir = umbra_flat_ir(b);
        umbra_builder_free(b);
    }
    if (st->prog) { st->prog->free(st->prog); }
    st->prog = be->compile(be, st->ir);
}

static void grad_2stop_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct grad_2stop_slide *st = (struct grad_2stop_slide *)s;
    (void)secs;
    umbra_draw_fill(&st->lay, NULL, st->shader);
    struct umbra_buf ubuf[] = {
        {.ptr=st->lay.uniforms, .count=st->lay.uni.slots},
        {.ptr=buf, .count=st->w * st->h * st->fmt.planes, .stride=st->w},
    };
    st->prog->queue(st->prog, l, t, r, b, ubuf);
}

static int grad_2stop_get_builders(struct slide *s, struct umbra_fmt fmt,
                                   struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct grad_2stop_slide *st = (struct grad_2stop_slide *)s;
    out[0] = umbra_draw_builder(NULL, st->shader, NULL, fmt, NULL);
    return out[0] ? 1 : 0;
}

static void grad_2stop_free(struct slide *s) {
    struct grad_2stop_slide *st = (struct grad_2stop_slide *)s;
    if (st->prog) { st->prog->free(st->prog); }
    umbra_flat_ir_free(st->ir);
    free(st->lay.uniforms);
    umbra_shader_free(st->shader);
    free(st);
}

static void grad_lut_init(struct slide *s, int w, int h) {
    struct grad_lut_slide *st = (struct grad_lut_slide *)s;
    st->w = w;
    st->h = h;
}

static void grad_lut_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct grad_lut_slide *st = (struct grad_lut_slide *)s;
    if (st->fmt.name != fmt.name || !st->ir) {
        st->fmt = fmt;
        umbra_flat_ir_free(st->ir);
        free(st->lay.uniforms);
        struct umbra_builder *b = umbra_draw_builder(NULL, st->shader, NULL, fmt,
                                                    &st->lay);
        st->ir = umbra_flat_ir(b);
        umbra_builder_free(b);
    }
    if (st->prog) { st->prog->free(st->prog); }
    st->prog = be->compile(be, st->ir);
}

static void grad_lut_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct grad_lut_slide *st = (struct grad_lut_slide *)s;
    (void)secs;
    umbra_draw_fill(&st->lay, NULL, st->shader);
    struct umbra_buf ubuf[] = {
        {.ptr=st->lay.uniforms, .count=st->lay.uni.slots},
        {.ptr=buf, .count=st->w * st->h * st->fmt.planes, .stride=st->w},
    };
    st->prog->queue(st->prog, l, t, r, b, ubuf);
}

static int grad_lut_get_builders(struct slide *s, struct umbra_fmt fmt,
                                 struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct grad_lut_slide *st = (struct grad_lut_slide *)s;
    out[0] = umbra_draw_builder(NULL, st->shader, NULL, fmt, NULL);
    return out[0] ? 1 : 0;
}

static void grad_lut_free(struct slide *s) {
    struct grad_lut_slide *st = (struct grad_lut_slide *)s;
    if (st->prog) { st->prog->free(st->prog); }
    umbra_flat_ir_free(st->ir);
    free(st->lay.uniforms);
    umbra_shader_free(st->shader);
    free(st->lut_data);
    free(st);
}

SLIDE(slide_gradient_linear_2) {
    struct grad_2stop_slide *st = calloc(1, sizeof *st);
    st->is_radial = 0;
    st->shader = umbra_shader_gradient_linear_two_stops((umbra_point){0, 0}, (umbra_point){640, 0},
                                              (umbra_color){1.0f, 0.4f, 0.0f, 1.0f},
                                              (umbra_color){0.0f, 0.3f, 1.0f, 1.0f});
    st->base = (struct slide){
        .title        = "Linear Gradient (2-stop)",
        .bg           = {0, 0, 0, 1},
        .init         = grad_2stop_init,
        .prepare      = grad_2stop_prepare,
        .draw         = grad_2stop_draw,
        .free         = grad_2stop_free,
        .get_builders = grad_2stop_get_builders,
    };
    return &st->base;
}

SLIDE(slide_gradient_radial_2) {
    struct grad_2stop_slide *st = calloc(1, sizeof *st);
    st->is_radial = 1;
    st->shader = umbra_shader_gradient_radial_two_stops((umbra_point){320, 240}, 300.0f,
                                              (umbra_color){1.0f, 1.0f, 0.9f, 1.0f},
                                              (umbra_color){0.05f, 0.0f, 0.15f, 1.0f});
    st->base = (struct slide){
        .title        = "Radial Gradient (2-stop)",
        .bg           = {0, 0, 0, 1},
        .init         = grad_2stop_init,
        .prepare      = grad_2stop_prepare,
        .draw         = grad_2stop_draw,
        .free         = grad_2stop_free,
        .get_builders = grad_2stop_get_builders,
    };
    return &st->base;
}

struct grad_stops_slide {
    struct slide base;

    int    w, h;
    int    is_radial, :32;
    float *colors_data, *pos_data;

    struct umbra_shader        *shader;

    struct umbra_fmt            fmt;
    struct umbra_draw_layout   lay;
    struct umbra_flat_ir  *ir;
    struct umbra_program      *prog;
};

static void grad_stops_init(struct slide *s, int w, int h) {
    struct grad_stops_slide *st = (struct grad_stops_slide *)s;
    st->w = w;
    st->h = h;
}

static void grad_stops_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct grad_stops_slide *st = (struct grad_stops_slide *)s;
    if (st->fmt.name != fmt.name || !st->ir) {
        st->fmt = fmt;
        umbra_flat_ir_free(st->ir);
        free(st->lay.uniforms);
        struct umbra_builder *b = umbra_draw_builder(NULL, st->shader, NULL, fmt,
                                                    &st->lay);
        st->ir = umbra_flat_ir(b);
        umbra_builder_free(b);
    }
    if (st->prog) { st->prog->free(st->prog); }
    st->prog = be->compile(be, st->ir);
}

static void grad_stops_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct grad_stops_slide *st = (struct grad_stops_slide *)s;
    (void)secs;
    umbra_draw_fill(&st->lay, NULL, st->shader);
    struct umbra_buf ubuf[] = {
        {.ptr=st->lay.uniforms, .count=st->lay.uni.slots},
        {.ptr=buf, .count=st->w * st->h * st->fmt.planes, .stride=st->w},
    };
    st->prog->queue(st->prog, l, t, r, b, ubuf);
}

static int grad_stops_get_builders(struct slide *s, struct umbra_fmt fmt,
                                   struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct grad_stops_slide *st = (struct grad_stops_slide *)s;
    out[0] = umbra_draw_builder(NULL, st->shader, NULL, fmt, NULL);
    return out[0] ? 1 : 0;
}

static void grad_stops_free(struct slide *s) {
    struct grad_stops_slide *st = (struct grad_stops_slide *)s;
    if (st->prog) { st->prog->free(st->prog); }
    umbra_flat_ir_free(st->ir);
    free(st->lay.uniforms);
    umbra_shader_free(st->shader);
    free(st->colors_data);
    free(st->pos_data);
    free(st);
}

// Fixed: was ~131 µs Metal vs ~102 µs Vulkan.  Now ~62 and ~66 respectively,
// thanks to umbra_if() wrapping the gathers+lerps so GPU backends can branch-skip
// them when the pixel isn't in the current gradient segment.
SLIDE(slide_gradient_linear_stops) {
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
    struct grad_stops_slide *st = calloc(1, sizeof *st);
    st->colors_data = planar;
    st->pos_data    = pos;
    st->shader = umbra_shader_gradient_linear(
        (umbra_point){0, 0}, (umbra_point){640, 0},
        (struct umbra_buf){.ptr=planar, .count=N * 4},
        (struct umbra_buf){.ptr=pos,    .count=N});
    st->base = (struct slide){
        .title       = "Linear Gradient (loop stops)",
        .bg          = {0, 0, 0, 1},
        .init        = grad_stops_init,
        .prepare     = grad_stops_prepare,
        .draw        = grad_stops_draw,
        .free        = grad_stops_free,
        .get_builders = grad_stops_get_builders,
    };
    return &st->base;
}

SLIDE(slide_gradient_radial_stops) {
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
    struct grad_stops_slide *st = calloc(1, sizeof *st);
    st->is_radial   = 1;
    st->colors_data = planar;
    st->pos_data    = pos;
    st->shader = umbra_shader_gradient_radial(
        (umbra_point){320, 240}, 280.0f,
        (struct umbra_buf){.ptr=planar, .count=N * 4},
        (struct umbra_buf){.ptr=pos,    .count=N});
    st->base = (struct slide){
        .title       = "Radial Gradient (loop stops)",
        .bg          = {0, 0, 0, 1},
        .init        = grad_stops_init,
        .prepare     = grad_stops_prepare,
        .draw        = grad_stops_draw,
        .free        = grad_stops_free,
        .get_builders = grad_stops_get_builders,
    };
    return &st->base;
}

SLIDE(slide_gradient_linear_wide) {
    static umbra_color const colors[] = {
        {1.2f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.8f, 0.0f, 1.0f}, {0.0f, 1.2f, 0.0f, 1.0f},
        {0.0f, 0.8f, 1.2f, 1.0f}, {0.0f, 0.0f, 1.2f, 1.0f}, {0.8f, 0.0f, 1.0f, 1.0f},
    };
    enum { LUT_N = 64 };
    float *lut = malloc(LUT_N * 4 * sizeof(float));
    umbra_gradient_lut_even(lut, LUT_N, 6, colors);
    struct grad_lut_slide *st = calloc(1, sizeof *st);
    st->is_radial     = 0;
    st->lut_data      = lut;
    st->shader = umbra_shader_gradient_linear_lut((umbra_point){0, 0}, (umbra_point){640, 0},
                                                 (struct umbra_buf){.ptr=lut, .count=LUT_N * 4});
    st->base = (struct slide){
        .title        = "Linear Gradient (wide gamut)",
        .bg           = {0, 0, 0, 1},
        .init         = grad_lut_init,
        .prepare      = grad_lut_prepare,
        .draw         = grad_lut_draw,
        .free         = grad_lut_free,
        .get_builders = grad_lut_get_builders,
    };
    return &st->base;
}

SLIDE(slide_gradient_radial_wide) {
    static umbra_color const colors[] = {
        {1.5f, 1.5f, 1.2f, 1.0f}, {1.2f, 0.8f, 0.0f, 1.0f},
        {0.8f, 0.0f, 0.2f, 1.0f}, {0.05f, 0.0f, 0.15f, 1.0f},
    };
    enum { LUT_N = 64 };
    float *lut = malloc(LUT_N * 4 * sizeof(float));
    umbra_gradient_lut_even(lut, LUT_N, 4, colors);
    struct grad_lut_slide *st = calloc(1, sizeof *st);
    st->is_radial     = 1;
    st->lut_data      = lut;
    st->shader = umbra_shader_gradient_radial_lut((umbra_point){320, 240}, 280.0f,
                                                 (struct umbra_buf){.ptr=lut, .count=LUT_N * 4});
    st->base = (struct slide){
        .title        = "Radial Gradient (wide gamut)",
        .bg           = {0, 0, 0, 1},
        .init         = grad_lut_init,
        .prepare      = grad_lut_prepare,
        .draw         = grad_lut_draw,
        .free         = grad_lut_free,
        .get_builders = grad_lut_get_builders,
    };
    return &st->base;
}
