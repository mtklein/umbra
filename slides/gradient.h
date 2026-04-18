#pragma once
#include "../include/umbra_draw.h"

// A gradient is (x,y) -> t -> color.  gradient_coords is the first leg,
// a first-class effect in the same shape as umbra_shader / umbra_coverage /
// umbra_sdf: a flat (void *ctx, builder, xy) -> val32 callback that maps
// pixel coordinates to a clamped parameter t in [0, 1].  Colorizer shaders
// hold a (coords_fn, coords_ctx) pair, read once at IR-emit time.
typedef umbra_val32 gradient_coords(void *ctx, struct umbra_builder*,
                                    umbra_point_val32 xy);

// Linear gradient: t = a*x + b*y + c, clamped.  Fill from two points via
// gradient_linear_from().
struct gradient_linear {
    float a, b, c;
    int   :32;
};
struct gradient_linear gradient_linear_from(umbra_point p0, umbra_point p1);
umbra_val32 gradient_linear(void *gradient_linear, struct umbra_builder*,
                            umbra_point_val32 xy);

// Radial gradient: t = |xy - center| / radius, clamped.  Fill via
// gradient_radial_from().
struct gradient_radial {
    float cx, cy, inv_r;
    int   :32;
};
struct gradient_radial gradient_radial_from(umbra_point center, float radius);
umbra_val32 gradient_radial(void *gradient_radial, struct umbra_builder*,
                            umbra_point_val32 xy);

// Gradient colorizer shaders.  Each state struct holds the coords (fn, ctx)
// it invokes to map xy to t, plus the data (colors / lut / positions) needed
// to convert t to a color.  Callers own the state; pass &state as shader_ctx.
// coords_fn / coords_ctx are baked at IR-emit time -- mutating them after
// has no effect on a compiled program.  The colors/lut/pos *bytes* still
// mutate freely via umbra_uniforms.
struct shader_gradient_two_stops {
    gradient_coords *coords_fn;
    void            *coords_ctx;
    umbra_color      c0, c1;
};
umbra_color_val32 shader_gradient_two_stops(void *shader_gradient_two_stops,
                                            struct umbra_builder*,
                                            umbra_val32 x, umbra_val32 y);

struct shader_gradient_lut {
    gradient_coords *coords_fn;
    void            *coords_ctx;
    float            N;
    int              :32;
    struct umbra_buf lut;
};
umbra_color_val32 shader_gradient_lut(void *shader_gradient_lut,
                                      struct umbra_builder*,
                                      umbra_val32 x, umbra_val32 y);

struct shader_gradient_evenly_spaced_stops {
    gradient_coords *coords_fn;
    void            *coords_ctx;
    float            N;
    int              :32;
    struct umbra_buf colors;
};
umbra_color_val32 shader_gradient_evenly_spaced_stops(
    void *shader_gradient_evenly_spaced_stops, struct umbra_builder*,
    umbra_val32 x, umbra_val32 y);

struct shader_gradient {
    gradient_coords *coords_fn;
    void            *coords_ctx;
    float            N;
    int              :32;
    struct umbra_buf colors, pos;
};
umbra_color_val32 shader_gradient(void *shader_gradient, struct umbra_builder*,
                                  umbra_val32 x, umbra_val32 y);

void gradient_lut(float *out, int lut_n, int n_stops, float const positions[],
                  umbra_color const *colors);
