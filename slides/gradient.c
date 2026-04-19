#include "slide.h"
#include "gradient.h"
#include <stdlib.h>

// Slot offset (in 32-bit units) of a field within the flat ctx struct
// pointed at by `self`.  Used after umbra_bind_uniforms32(b, self, sizeof *self / 4)
// to address named fields as uniform slots.
#define SLOT(field) ((int)(__builtin_offsetof(__typeof__(*self), field) / 4))

static umbra_val32 clamp01(struct umbra_builder *builder, umbra_val32 t) {
    return umbra_min_f32(builder, umbra_max_f32(builder, t, umbra_imm_f32(builder, 0.0f)),
                         umbra_imm_f32(builder, 1.0f));
}

static umbra_val32 lerp_f(struct umbra_builder *builder, umbra_val32 p, umbra_val32 q,
                           umbra_val32 t) {
    return umbra_add_f32(builder, p,
                         umbra_mul_f32(builder, umbra_sub_f32(builder, q, p), t));
}

umbra_val32 gradient_linear(void *ctx, struct umbra_builder *b, umbra_point_val32 xy) {
    struct gradient_linear const *self = ctx;
    umbra_ptr32 const u = umbra_bind_uniforms32(b, self, sizeof *self / 4);
    umbra_val32 const a = umbra_uniform_32(b, u, SLOT(a)),
                      bb = umbra_uniform_32(b, u, SLOT(b)),
                      c = umbra_uniform_32(b, u, SLOT(c));
    umbra_val32 const t = umbra_add_f32(b,
                                      umbra_add_f32(b, umbra_mul_f32(b, a, xy.x),
                                                    umbra_mul_f32(b, bb, xy.y)),
                                      c);
    return clamp01(b, t);
}

umbra_val32 gradient_radial(void *ctx, struct umbra_builder *b, umbra_point_val32 xy) {
    struct gradient_radial const *self = ctx;
    umbra_ptr32 const u = umbra_bind_uniforms32(b, self, sizeof *self / 4);
    umbra_val32 const cx    = umbra_uniform_32(b, u, SLOT(cx)),
                      cy    = umbra_uniform_32(b, u, SLOT(cy)),
                      inv_r = umbra_uniform_32(b, u, SLOT(inv_r));
    umbra_val32 const dx = umbra_sub_f32(b, xy.x, cx),
                      dy = umbra_sub_f32(b, xy.y, cy);
    umbra_val32 const d2 = umbra_add_f32(b, umbra_mul_f32(b, dx, dx),
                                       umbra_mul_f32(b, dy, dy));
    return clamp01(b, umbra_mul_f32(b, umbra_sqrt_f32(b, d2), inv_r));
}

struct gradient_linear gradient_linear_from(umbra_point p0, umbra_point p1) {
    float const dx = p1.x - p0.x,
                dy = p1.y - p0.y,
                L2 = dx*dx + dy*dy,
                a  = dx / L2,
                b  = dy / L2;
    return (struct gradient_linear){
        .a = a, .b = b,
        .c = -(a * p0.x + b * p0.y),
    };
}

struct gradient_radial gradient_radial_from(umbra_point center, float radius) {
    return (struct gradient_radial){
        .cx = center.x, .cy = center.y, .inv_r = 1.0f / radius,
    };
}

static umbra_color_val32 sample_lut(struct umbra_builder *b, umbra_val32 t,
                                    umbra_ptr32 uniforms, int N_slot, umbra_ptr32 lut) {
    umbra_val32 const N_f     = umbra_uniform_32(b, uniforms, N_slot);
    umbra_val32 const N_m1    = umbra_sub_f32(b, N_f, umbra_imm_f32(b, 1.0f));
    umbra_val32 const N_m2    = umbra_sub_f32(b, N_f, umbra_imm_f32(b, 2.0f));
    umbra_val32 const seg     = umbra_mul_f32(b, t, N_m1);
    umbra_val32 const clamped = umbra_min_f32(b, seg, N_m2);
    umbra_val32 const idx     = umbra_floor_i32(b, clamped);
    umbra_val32 const idx1    = umbra_add_i32(b, idx, umbra_imm_i32(b, 1));
    umbra_val32 const frac    = umbra_sub_f32(b, seg, umbra_floor_f32(b, clamped));

    umbra_val32 const N_i = umbra_i32_from_f32(b, N_f);
    umbra_val32 const n2  = umbra_add_i32(b, N_i, N_i);
    umbra_val32 const n3  = umbra_add_i32(b, n2,  N_i);

    umbra_val32 const r0 = umbra_gather_32(b, lut, idx),
                      r1 = umbra_gather_32(b, lut, idx1),
                      g0 = umbra_gather_32(b, lut, umbra_add_i32(b, idx,  N_i)),
                      g1 = umbra_gather_32(b, lut, umbra_add_i32(b, idx1, N_i)),
                      b0 = umbra_gather_32(b, lut, umbra_add_i32(b, idx,  n2)),
                      b1 = umbra_gather_32(b, lut, umbra_add_i32(b, idx1, n2)),
                      a0 = umbra_gather_32(b, lut, umbra_add_i32(b, idx,  n3)),
                      a1 = umbra_gather_32(b, lut, umbra_add_i32(b, idx1, n3));

    return (umbra_color_val32){
        lerp_f(b, r0, r1, frac),
        lerp_f(b, g0, g1, frac),
        lerp_f(b, b0, b1, frac),
        lerp_f(b, a0, a1, frac),
    };
}

static umbra_color_val32 walk_stops(struct umbra_builder *b, umbra_val32 t,
                                    umbra_ptr32 uniforms, int N_slot,
                                    umbra_ptr32 colors, umbra_ptr32 pos) {
    umbra_val32 const n_i = umbra_i32_from_f32(b,
                                umbra_uniform_32(b, uniforms, N_slot));
    umbra_val32 const n_segs = umbra_sub_i32(b, n_i, umbra_imm_i32(b, 1));
    umbra_val32 const n2 = umbra_add_i32(b, n_i, n_i);
    umbra_val32 const n3 = umbra_add_i32(b, n2, n_i);

    umbra_var32 vr = umbra_declare_var32(b),
                       vg = umbra_declare_var32(b),
                       vb = umbra_declare_var32(b),
                       va = umbra_declare_var32(b);

    umbra_val32 i = umbra_loop(b, n_segs); {
        umbra_val32 i1 = umbra_add_i32(b, i, umbra_imm_i32(b, 1));

        umbra_val32 p0 = umbra_gather_32(b, pos, i);
        umbra_val32 p1 = umbra_gather_32(b, pos, i1);
        umbra_val32 in_seg = umbra_and_32(b, umbra_le_f32(b, p0, t),
                                             umbra_le_f32(b, t, p1));

        umbra_if(b, in_seg); {
            umbra_val32 frac = umbra_div_f32(b, umbra_sub_f32(b, t, p0),
                                                umbra_sub_f32(b, p1, p0));

            umbra_val32 r0 = umbra_gather_32(b, colors, i);
            umbra_val32 r1 = umbra_gather_32(b, colors, i1);
            umbra_val32 g0 = umbra_gather_32(b, colors, umbra_add_i32(b, i,  n_i));
            umbra_val32 g1 = umbra_gather_32(b, colors, umbra_add_i32(b, i1, n_i));
            umbra_val32 b0 = umbra_gather_32(b, colors, umbra_add_i32(b, i,  n2));
            umbra_val32 b1 = umbra_gather_32(b, colors, umbra_add_i32(b, i1, n2));
            umbra_val32 a0 = umbra_gather_32(b, colors, umbra_add_i32(b, i,  n3));
            umbra_val32 a1 = umbra_gather_32(b, colors, umbra_add_i32(b, i1, n3));

            umbra_store_var32(b, vr, lerp_f(b, r0, r1, frac));
            umbra_store_var32(b, vg, lerp_f(b, g0, g1, frac));
            umbra_store_var32(b, vb, lerp_f(b, b0, b1, frac));
            umbra_store_var32(b, va, lerp_f(b, a0, a1, frac));
        } umbra_end_if(b);
    } umbra_end_loop(b);

    return (umbra_color_val32){
        umbra_load_var32(b, vr),
        umbra_load_var32(b, vg),
        umbra_load_var32(b, vb),
        umbra_load_var32(b, va),
    };
}

umbra_color_val32 shader_gradient_two_stops(void *ctx, struct umbra_builder *b,
                                             umbra_val32 x, umbra_val32 y) {
    struct shader_gradient_two_stops const *self = ctx;
    umbra_val32 const t = self->coords_fn(self->coords_ctx, b,
                                          (umbra_point_val32){x, y});
    umbra_ptr32 const u = umbra_bind_uniforms32(b, self, sizeof *self / 4);
    umbra_val32 const r0 = umbra_uniform_32(b, u, SLOT(c0.r)),
                      g0 = umbra_uniform_32(b, u, SLOT(c0.g)),
                      b0 = umbra_uniform_32(b, u, SLOT(c0.b)),
                      a0 = umbra_uniform_32(b, u, SLOT(c0.a)),
                      r1 = umbra_uniform_32(b, u, SLOT(c1.r)),
                      g1 = umbra_uniform_32(b, u, SLOT(c1.g)),
                      b1 = umbra_uniform_32(b, u, SLOT(c1.b)),
                      a1 = umbra_uniform_32(b, u, SLOT(c1.a));
    return (umbra_color_val32){
        lerp_f(b, r0, r1, t),
        lerp_f(b, g0, g1, t),
        lerp_f(b, b0, b1, t),
        lerp_f(b, a0, a1, t),
    };
}

umbra_color_val32 shader_gradient_lut(void *ctx, struct umbra_builder *b,
                                       umbra_val32 x, umbra_val32 y) {
    struct shader_gradient_lut const *self = ctx;
    umbra_val32 const t = self->coords_fn(self->coords_ctx, b,
                                          (umbra_point_val32){x, y});
    umbra_ptr32 const u   = umbra_bind_uniforms32(b, self, sizeof *self / 4);
    umbra_ptr32 const lut = umbra_bind_buf32(b, &self->lut);
    return sample_lut(b, t, u, SLOT(N), lut);
}

static umbra_color_val32 gather_even_stops(struct umbra_builder *b, umbra_val32 t,
                                           umbra_ptr32 uniforms, int N_slot,
                                           umbra_ptr32 colors) {
    umbra_val32 const N_f     = umbra_uniform_32(b, uniforms, N_slot);
    umbra_val32 const N_m1    = umbra_sub_f32(b, N_f, umbra_imm_f32(b, 1.0f));
    umbra_val32 const N_m2    = umbra_sub_f32(b, N_f, umbra_imm_f32(b, 2.0f));
    umbra_val32 const seg     = umbra_mul_f32(b, t, N_m1);
    umbra_val32 const clamped = umbra_min_f32(b, seg, N_m2);
    umbra_val32 const idx     = umbra_floor_i32(b, clamped);
    umbra_val32 const idx1    = umbra_add_i32(b, idx, umbra_imm_i32(b, 1));
    umbra_val32 const frac    = umbra_sub_f32(b, seg, umbra_floor_f32(b, clamped));

    umbra_val32 const N_i = umbra_i32_from_f32(b, N_f);
    umbra_val32 const n2  = umbra_add_i32(b, N_i, N_i);
    umbra_val32 const n3  = umbra_add_i32(b, n2,  N_i);

    umbra_val32 const r0 = umbra_gather_32(b, colors, idx),
                      r1 = umbra_gather_32(b, colors, idx1),
                      g0 = umbra_gather_32(b, colors, umbra_add_i32(b, idx,  N_i)),
                      g1 = umbra_gather_32(b, colors, umbra_add_i32(b, idx1, N_i)),
                      b0 = umbra_gather_32(b, colors, umbra_add_i32(b, idx,  n2)),
                      b1 = umbra_gather_32(b, colors, umbra_add_i32(b, idx1, n2)),
                      a0 = umbra_gather_32(b, colors, umbra_add_i32(b, idx,  n3)),
                      a1 = umbra_gather_32(b, colors, umbra_add_i32(b, idx1, n3));

    return (umbra_color_val32){
        lerp_f(b, r0, r1, frac),
        lerp_f(b, g0, g1, frac),
        lerp_f(b, b0, b1, frac),
        lerp_f(b, a0, a1, frac),
    };
}

umbra_color_val32 shader_gradient_evenly_spaced_stops(void *ctx, struct umbra_builder *b,
                                                       umbra_val32 x, umbra_val32 y) {
    struct shader_gradient_evenly_spaced_stops const *self = ctx;
    umbra_val32 const t = self->coords_fn(self->coords_ctx, b,
                                          (umbra_point_val32){x, y});
    umbra_ptr32 const u      = umbra_bind_uniforms32(b, self, sizeof *self / 4);
    umbra_ptr32 const colors = umbra_bind_buf32(b, &self->colors);
    return gather_even_stops(b, t, u, SLOT(N), colors);
}

umbra_color_val32 shader_gradient(void *ctx, struct umbra_builder *b,
                                   umbra_val32 x, umbra_val32 y) {
    struct shader_gradient const *self = ctx;
    umbra_val32 const t = self->coords_fn(self->coords_ctx, b,
                                          (umbra_point_val32){x, y});
    umbra_ptr32 const u      = umbra_bind_uniforms32(b, self, sizeof *self / 4);
    umbra_ptr32 const colors = umbra_bind_buf32(b, &self->colors);
    umbra_ptr32 const pos    = umbra_bind_buf32(b, &self->pos);
    return walk_stops(b, t, u, SLOT(N), colors, pos);
}

void gradient_lut(float *out, int lut_n, int n_stops, float const positions[],
                  umbra_color const *colors) {
    for (int i = 0; i < lut_n; i++) {
        float const t = (float)i / (float)(lut_n - 1);
        int   seg = 0;
        for (int j = 1; j < n_stops; j++) {
            if (t >= positions[j]) {
                seg = j;
            }
        }
        if (seg >= n_stops - 1) {
            seg = n_stops - 2;
        }
        float const span = positions[seg + 1] - positions[seg];
        float f = 0;
        if (span > 0) {
            f = (t - positions[seg]) / span;
            if (f < 0) { f = 0; }
            if (f > 1) { f = 1; }
        }
        out[0 * lut_n + i] = colors[seg].r * (1 - f) + colors[seg + 1].r * f;
        out[1 * lut_n + i] = colors[seg].g * (1 - f) + colors[seg + 1].g * f;
        out[2 * lut_n + i] = colors[seg].b * (1 - f) + colors[seg + 1].b * f;
        out[3 * lut_n + i] = colors[seg].a * (1 - f) + colors[seg + 1].a * f;
    }
}

struct grad_slide {
    struct slide base;

    int    w, h;
    float *colors_data, *pos_data, *lut_data;

    union {
        struct gradient_linear linear;
        struct gradient_radial radial;
    } coords;
    union {
        struct shader_gradient_two_stops           two_stops;
        struct shader_gradient_lut                 lut;
        struct shader_gradient_evenly_spaced_stops even;
        struct shader_gradient                     full;
    } colorizer;
    umbra_shader          *shader_fn;
    void                 *shader_ctx;

    struct umbra_fmt      fmt;
    struct umbra_flat_ir *ir;
    struct umbra_program *prog;
    struct umbra_buf      dst_buf;
};

static void grad_init(struct slide *s, int w, int h) {
    struct grad_slide *st = (struct grad_slide *)s;
    st->w = w;
    st->h = h;
}

static void grad_build_draw(struct slide *s, struct umbra_builder *b,
                             umbra_ptr32 dst_ptr, struct umbra_fmt fmt,
                             umbra_val32 x, umbra_val32 y) {
    struct grad_slide *st = (struct grad_slide *)s;
    umbra_build_draw(b, dst_ptr, fmt, x, y,
                     NULL, NULL,
                     st->shader_fn, st->shader_ctx,
                     NULL, NULL);
}

static struct umbra_builder* grad_builder(struct slide *s, struct umbra_fmt fmt) {
    struct grad_slide *st = (struct grad_slide *)s;
    struct umbra_builder *b = umbra_builder();
    umbra_ptr32 const dst_ptr = umbra_bind_buf32(b, &st->dst_buf);
    umbra_val32 const x = umbra_f32_from_i32(b, umbra_x(b)),
                      y = umbra_f32_from_i32(b, umbra_y(b));
    grad_build_draw(s, b, dst_ptr, fmt, x, y);
    return b;
}

static void grad_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct grad_slide *st = (struct grad_slide *)s;
    if (st->fmt.name != fmt.name || !st->ir) {
        st->fmt = fmt;
        umbra_flat_ir_free(st->ir);
        struct umbra_builder *b = grad_builder(s, fmt);
        st->ir = umbra_flat_ir(b);
        umbra_builder_free(b);
    }
    umbra_program_free(st->prog);
    st->prog = be->compile(be, st->ir);
}

static void grad_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct grad_slide *st = (struct grad_slide *)s;
    (void)secs;
    st->dst_buf = (struct umbra_buf){
        .ptr=buf, .count=st->w * st->h * st->fmt.planes, .stride=st->w,
    };
    st->prog->queue(st->prog, l, t, r, b);
}

static int grad_get_builders(struct slide *s, struct umbra_fmt fmt,
                             struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    out[0] = grad_builder(s, fmt);
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
        .build_draw   = grad_build_draw,
    };
    return st;
}

SLIDE(slide_gradient_linear_two_stop) {
    struct grad_slide *st = make_grad("Gradient Linear Two-Stop");
    st->coords.linear = gradient_linear_from((umbra_point){0, 0}, (umbra_point){640, 0});
    st->colorizer.two_stops = (struct shader_gradient_two_stops){
        .coords_fn  = gradient_linear,
        .coords_ctx = &st->coords.linear,
        .c0         = (umbra_color){1.0f, 0.4f, 0.0f, 1.0f},
        .c1         = (umbra_color){0.0f, 0.3f, 1.0f, 1.0f},
    };
    st->shader_fn  = shader_gradient_two_stops;
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
    st->coords.linear = gradient_linear_from((umbra_point){0, 0}, (umbra_point){640, 0});
    st->colorizer.full = (struct shader_gradient){
        .coords_fn  = gradient_linear,
        .coords_ctx = &st->coords.linear,
        .N          = (float)N,
        .colors     = {.ptr=planar, .count=N * 4},
        .pos        = {.ptr=pos,    .count=N},
    };
    st->shader_fn  = shader_gradient;
    st->shader_ctx = &st->colorizer.full;
    return &st->base;
}

SLIDE(slide_gradient_linear_lut) {
    static umbra_color const colors[] = {
        {1.2f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.8f, 0.0f, 1.0f}, {0.0f, 1.2f, 0.0f, 1.0f},
        {0.0f, 0.8f, 1.2f, 1.0f}, {0.0f, 0.0f, 1.2f, 1.0f}, {0.8f, 0.0f, 1.0f, 1.0f},
    };
    enum { N_STOPS = 6, LUT_N = 64 };
    float pos[N_STOPS];
    for (int i = 0; i < N_STOPS; i++) { pos[i] = (float)i / (float)(N_STOPS - 1); }
    float *lut = malloc(LUT_N * 4 * sizeof(float));
    gradient_lut(lut, LUT_N, N_STOPS, pos, colors);
    struct grad_slide *st = make_grad("Gradient Linear LUT");
    st->lut_data = lut;
    st->coords.linear = gradient_linear_from((umbra_point){0, 0}, (umbra_point){640, 0});
    st->colorizer.lut = (struct shader_gradient_lut){
        .coords_fn  = gradient_linear,
        .coords_ctx = &st->coords.linear,
        .N          = (float)LUT_N,
        .lut        = {.ptr=lut, .count=LUT_N * 4},
    };
    st->shader_fn  = shader_gradient_lut;
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
    st->coords.linear = gradient_linear_from((umbra_point){0, 0}, (umbra_point){640, 0});
    st->colorizer.even = (struct shader_gradient_evenly_spaced_stops){
        .coords_fn  = gradient_linear,
        .coords_ctx = &st->coords.linear,
        .N          = (float)N,
        .colors     = {.ptr=planar, .count=N * 4},
    };
    st->shader_fn  = shader_gradient_evenly_spaced_stops;
    st->shader_ctx = &st->colorizer.even;
    return &st->base;
}

SLIDE(slide_gradient_radial_two_stop) {
    struct grad_slide *st = make_grad("Gradient Radial Two-Stop");
    st->coords.radial = gradient_radial_from((umbra_point){320, 240}, 300.0f);
    st->colorizer.two_stops = (struct shader_gradient_two_stops){
        .coords_fn  = gradient_radial,
        .coords_ctx = &st->coords.radial,
        .c0         = (umbra_color){1.0f, 1.0f, 0.9f, 1.0f},
        .c1         = (umbra_color){0.05f, 0.0f, 0.15f, 1.0f},
    };
    st->shader_fn  = shader_gradient_two_stops;
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
    st->coords.radial = gradient_radial_from((umbra_point){320, 240}, 280.0f);
    st->colorizer.full = (struct shader_gradient){
        .coords_fn  = gradient_radial,
        .coords_ctx = &st->coords.radial,
        .N          = (float)N,
        .colors     = {.ptr=planar, .count=N * 4},
        .pos        = {.ptr=pos,    .count=N},
    };
    st->shader_fn  = shader_gradient;
    st->shader_ctx = &st->colorizer.full;
    return &st->base;
}

SLIDE(slide_gradient_radial_lut) {
    static umbra_color const colors[] = {
        {1.5f, 1.5f, 1.2f, 1.0f}, {1.2f, 0.8f, 0.0f, 1.0f},
        {0.8f, 0.0f, 0.2f, 1.0f}, {0.05f, 0.0f, 0.15f, 1.0f},
    };
    enum { N_STOPS = 4, LUT_N = 64 };
    float pos[N_STOPS];
    for (int i = 0; i < N_STOPS; i++) { pos[i] = (float)i / (float)(N_STOPS - 1); }
    float *lut = malloc(LUT_N * 4 * sizeof(float));
    gradient_lut(lut, LUT_N, N_STOPS, pos, colors);
    struct grad_slide *st = make_grad("Gradient Radial LUT");
    st->lut_data = lut;
    st->coords.radial = gradient_radial_from((umbra_point){320, 240}, 280.0f);
    st->colorizer.lut = (struct shader_gradient_lut){
        .coords_fn  = gradient_radial,
        .coords_ctx = &st->coords.radial,
        .N          = (float)LUT_N,
        .lut        = {.ptr=lut, .count=LUT_N * 4},
    };
    st->shader_fn  = shader_gradient_lut;
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
    st->coords.radial = gradient_radial_from((umbra_point){320, 240}, 280.0f);
    st->colorizer.even = (struct shader_gradient_evenly_spaced_stops){
        .coords_fn  = gradient_radial,
        .coords_ctx = &st->coords.radial,
        .N          = (float)N,
        .colors     = {.ptr=planar, .count=N * 4},
    };
    st->shader_fn  = shader_gradient_evenly_spaced_stops;
    st->shader_ctx = &st->colorizer.even;
    return &st->base;
}
