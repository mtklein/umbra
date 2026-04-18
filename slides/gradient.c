#include "slide.h"
#include <stdlib.h>

struct grad_slide {
    struct slide base;

    int    w, h;
    float *colors_data, *pos_data, *lut_data;

    union {
        struct umbra_gradient_linear linear;
        struct umbra_gradient_radial radial;
    } coords;
    union {
        struct umbra_shader_gradient_two_stops           two_stops;
        struct umbra_shader_gradient_lut                 lut;
        struct umbra_shader_gradient_evenly_spaced_stops even;
        struct umbra_shader_gradient                     full;
    } colorizer;
    umbra_shader          *shader_fn;
    void                 *shader_ctx;

    struct umbra_fmt      fmt;
    struct umbra_flat_ir *ir;
    struct umbra_program *prog;
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
        struct umbra_builder *b = umbra_draw_builder(
            NULL, NULL,
            st->shader_fn, st->shader_ctx,
            NULL, NULL,
            fmt);
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
    };
    st->prog->queue(st->prog, l, t, r, b, ubuf);
}

static int grad_get_builders(struct slide *s, struct umbra_fmt fmt,
                             struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct grad_slide *st = (struct grad_slide *)s;
    out[0] = umbra_draw_builder(
        NULL, NULL,
        st->shader_fn, st->shader_ctx,
        NULL, NULL,
        fmt);
    return out[0] ? 1 : 0;
}

static void grad_free(struct slide *s) {
    struct grad_slide *st = (struct grad_slide *)s;
    umbra_program_free(st->prog);
    umbra_flat_ir_free(st->ir);
    free(st->colors_data);
    free(st->pos_data);
    free(st->lut_data);
    free(st);
}

static struct grad_slide* make_grad(char const *title) {
    struct grad_slide *st = calloc(1, sizeof *st);
    st->base = (struct slide){
        .title        = title,
        .bg           = {0, 0, 0, 1},
        .init         = grad_init,
        .prepare      = grad_prepare,
        .draw         = grad_draw,
        .free         = grad_free,
        .get_builders = grad_get_builders,
    };
    return st;
}

SLIDE(slide_gradient_linear_two_stop) {
    struct grad_slide *st = make_grad("Gradient Linear Two-Stop");
    st->coords.linear = umbra_gradient_linear_from((umbra_point){0, 0}, (umbra_point){640, 0});
    st->colorizer.two_stops = (struct umbra_shader_gradient_two_stops){
        .coords_fn  = umbra_gradient_linear,
        .coords_ctx = &st->coords.linear,
        .c0         = (umbra_color){1.0f, 0.4f, 0.0f, 1.0f},
        .c1         = (umbra_color){0.0f, 0.3f, 1.0f, 1.0f},
    };
    st->shader_fn  = umbra_shader_gradient_two_stops;
    st->shader_ctx = &st->colorizer.two_stops;
    return &st->base;
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
    struct grad_slide *st = make_grad("Gradient Linear");
    st->colors_data = planar;
    st->pos_data    = pos;
    st->coords.linear = umbra_gradient_linear_from((umbra_point){0, 0}, (umbra_point){640, 0});
    st->colorizer.full = (struct umbra_shader_gradient){
        .coords_fn  = umbra_gradient_linear,
        .coords_ctx = &st->coords.linear,
        .N          = (float)N,
        .colors     = {.ptr=planar, .count=N * 4},
        .pos        = {.ptr=pos,    .count=N},
    };
    st->shader_fn  = umbra_shader_gradient;
    st->shader_ctx = &st->colorizer.full;
    return &st->base;
}

SLIDE(slide_gradient_linear_lut) {
    static umbra_color const colors[] = {
        {1.2f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.8f, 0.0f, 1.0f}, {0.0f, 1.2f, 0.0f, 1.0f},
        {0.0f, 0.8f, 1.2f, 1.0f}, {0.0f, 0.0f, 1.2f, 1.0f}, {0.8f, 0.0f, 1.0f, 1.0f},
    };
    enum { LUT_N = 64 };
    float *lut = malloc(LUT_N * 4 * sizeof(float));
    umbra_gradient_lut_even(lut, LUT_N, 6, colors);
    struct grad_slide *st = make_grad("Gradient Linear LUT");
    st->lut_data = lut;
    st->coords.linear = umbra_gradient_linear_from((umbra_point){0, 0}, (umbra_point){640, 0});
    st->colorizer.lut = (struct umbra_shader_gradient_lut){
        .coords_fn  = umbra_gradient_linear,
        .coords_ctx = &st->coords.linear,
        .N          = (float)LUT_N,
        .lut        = {.ptr=lut, .count=LUT_N * 4},
    };
    st->shader_fn  = umbra_shader_gradient_lut;
    st->shader_ctx = &st->colorizer.lut;
    return &st->base;
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
    struct grad_slide *st = make_grad("Gradient Linear Evenly-Spaced");
    st->colors_data = planar;
    st->coords.linear = umbra_gradient_linear_from((umbra_point){0, 0}, (umbra_point){640, 0});
    st->colorizer.even = (struct umbra_shader_gradient_evenly_spaced_stops){
        .coords_fn  = umbra_gradient_linear,
        .coords_ctx = &st->coords.linear,
        .N          = (float)N,
        .colors     = {.ptr=planar, .count=N * 4},
    };
    st->shader_fn  = umbra_shader_gradient_evenly_spaced_stops;
    st->shader_ctx = &st->colorizer.even;
    return &st->base;
}

SLIDE(slide_gradient_radial_two_stop) {
    struct grad_slide *st = make_grad("Gradient Radial Two-Stop");
    st->coords.radial = umbra_gradient_radial_from((umbra_point){320, 240}, 300.0f);
    st->colorizer.two_stops = (struct umbra_shader_gradient_two_stops){
        .coords_fn  = umbra_gradient_radial,
        .coords_ctx = &st->coords.radial,
        .c0         = (umbra_color){1.0f, 1.0f, 0.9f, 1.0f},
        .c1         = (umbra_color){0.05f, 0.0f, 0.15f, 1.0f},
    };
    st->shader_fn  = umbra_shader_gradient_two_stops;
    st->shader_ctx = &st->colorizer.two_stops;
    return &st->base;
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
    struct grad_slide *st = make_grad("Gradient Radial");
    st->colors_data = planar;
    st->pos_data    = pos;
    st->coords.radial = umbra_gradient_radial_from((umbra_point){320, 240}, 280.0f);
    st->colorizer.full = (struct umbra_shader_gradient){
        .coords_fn  = umbra_gradient_radial,
        .coords_ctx = &st->coords.radial,
        .N          = (float)N,
        .colors     = {.ptr=planar, .count=N * 4},
        .pos        = {.ptr=pos,    .count=N},
    };
    st->shader_fn  = umbra_shader_gradient;
    st->shader_ctx = &st->colorizer.full;
    return &st->base;
}

SLIDE(slide_gradient_radial_lut) {
    static umbra_color const colors[] = {
        {1.5f, 1.5f, 1.2f, 1.0f}, {1.2f, 0.8f, 0.0f, 1.0f},
        {0.8f, 0.0f, 0.2f, 1.0f}, {0.05f, 0.0f, 0.15f, 1.0f},
    };
    enum { LUT_N = 64 };
    float *lut = malloc(LUT_N * 4 * sizeof(float));
    umbra_gradient_lut_even(lut, LUT_N, 4, colors);
    struct grad_slide *st = make_grad("Gradient Radial LUT");
    st->lut_data = lut;
    st->coords.radial = umbra_gradient_radial_from((umbra_point){320, 240}, 280.0f);
    st->colorizer.lut = (struct umbra_shader_gradient_lut){
        .coords_fn  = umbra_gradient_radial,
        .coords_ctx = &st->coords.radial,
        .N          = (float)LUT_N,
        .lut        = {.ptr=lut, .count=LUT_N * 4},
    };
    st->shader_fn  = umbra_shader_gradient_lut;
    st->shader_ctx = &st->colorizer.lut;
    return &st->base;
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
    struct grad_slide *st = make_grad("Gradient Radial Evenly-Spaced");
    st->colors_data = planar;
    st->coords.radial = umbra_gradient_radial_from((umbra_point){320, 240}, 280.0f);
    st->colorizer.even = (struct umbra_shader_gradient_evenly_spaced_stops){
        .coords_fn  = umbra_gradient_radial,
        .coords_ctx = &st->coords.radial,
        .N          = (float)N,
        .colors     = {.ptr=planar, .count=N * 4},
    };
    st->shader_fn  = umbra_shader_gradient_evenly_spaced_stops;
    st->shader_ctx = &st->colorizer.even;
    return &st->base;
}
