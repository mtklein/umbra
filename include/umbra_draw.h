#pragma once
#include "umbra.h"
#include "umbra_interval.h"

typedef struct { float       x, y; } umbra_point;
typedef struct { umbra_val32 x, y; } umbra_point_val32;

typedef struct {
    float l, t, r, b;
} umbra_rect;

typedef struct { float       r, g, b, a; } umbra_color;
typedef struct { umbra_val32 r, g, b, a; } umbra_color_val32;

struct umbra_matrix {
    float sx, kx, tx,
          ky, sy, ty,
          p0, p1, p2;
};

struct umbra_bitmap {
    struct umbra_buf buf;
    int              w,h;
};

struct umbra_fmt {
    char const *name;
    size_t      bpp;
    int         planes, :32;
    umbra_color_val32 (*load) (struct umbra_builder*, int ptr_ix);
    void              (*store)(struct umbra_builder*, int ptr_ix, umbra_color_val32);
};
extern struct umbra_fmt const umbra_fmt_8888,
                              umbra_fmt_565,
                              umbra_fmt_1010102,
                              umbra_fmt_fp16,
                              umbra_fmt_fp16_planar;

umbra_color_val32 umbra_load_8888        (struct umbra_builder*, umbra_ptr32);
void              umbra_store_8888       (struct umbra_builder*, umbra_ptr32, umbra_color_val32);
umbra_color_val32 umbra_load_565         (struct umbra_builder*, umbra_ptr16);
void              umbra_store_565        (struct umbra_builder*, umbra_ptr16, umbra_color_val32);
umbra_color_val32 umbra_load_1010102     (struct umbra_builder*, umbra_ptr32);
void              umbra_store_1010102    (struct umbra_builder*, umbra_ptr32, umbra_color_val32);
umbra_color_val32 umbra_load_fp16        (struct umbra_builder*, umbra_ptr64);
void              umbra_store_fp16       (struct umbra_builder*, umbra_ptr64, umbra_color_val32);
umbra_color_val32 umbra_load_fp16_planar (struct umbra_builder*, umbra_ptr16);
void              umbra_store_fp16_planar(struct umbra_builder*, umbra_ptr16, umbra_color_val32);

// Effects emit IR into a builder.  Any uniforms registered with
// umbra_uniforms() must outlive the builder, flat_ir, and program.  These are
// function types (not pointer types), so declarations read as
// `umbra_blend umbra_blend_src, umbra_blend_srcover, ...;` and struct fields
// carrying them write as `umbra_shader *inner_fn`.
typedef umbra_color_val32 umbra_shader  (void *ctx, struct umbra_builder*,
                                         umbra_val32 x, umbra_val32 y);
typedef umbra_val32       umbra_coverage(void *ctx, struct umbra_builder*,
                                         umbra_val32 x, umbra_val32 y);
typedef umbra_interval    umbra_sdf     (void *ctx, struct umbra_builder*,
                                         umbra_interval x, umbra_interval y);
typedef umbra_color_val32 umbra_blend   (void *ctx, struct umbra_builder*,
                                         umbra_color_val32 src,
                                         umbra_color_val32 dst);

// Shade a single color; ctx is an umbra_color*.
umbra_color_val32 umbra_shader_color(void *umbra_color, struct umbra_builder*,
                                     umbra_val32 x, umbra_val32 y);

umbra_blend umbra_blend_src,
            umbra_blend_srcover,
            umbra_blend_dstover,
            umbra_blend_multiply;

// Compose coverage, shader, and blend into an IR builder that reads and writes
// dst_fmt at ptr.ix=0.  Any of the effects may be NULL for default behavior:
// coverage=1, shader={0,0,0,0}, blend=src.
struct umbra_builder* umbra_draw_builder(umbra_coverage, void *coverage_ctx,
                                         umbra_shader  , void *shader_ctx,
                                         umbra_blend   , void *blend_ctx,
                                         struct umbra_fmt dst_fmt);

// Draw using an umbra_sdf as coverage.  hard_edge=1 gives a binary mask;
// hard_edge=0 clamps -sdf into [0, 1] for a 1px AA ramp.
// TODO: _Bool hard_edge -> int quality
struct umbra_sdf_draw* umbra_sdf_draw(struct umbra_backend*,
                                      umbra_sdf, void *sdf_ctx,
                                      _Bool hard_edge,
                                      umbra_shader, void *shader_ctx,
                                      umbra_blend,  void *blend_ctx,
                                      struct umbra_fmt);
void umbra_sdf_draw_queue(struct umbra_sdf_draw const*,
                          int l, int t, int r, int b, struct umbra_buf[]);
void umbra_sdf_draw_free(struct umbra_sdf_draw*);


// Adapt an umbra_sdf as umbra_coverage.
struct umbra_coverage_from_sdf {
    umbra_sdf *sdf_fn;
    void      *sdf_ctx;
    int        hard_edge, :32;
};
umbra_val32 umbra_coverage_from_sdf(void *ctx, struct umbra_builder*,
                                     umbra_val32 x, umbra_val32 y);

// TODO: move gradient shaders out into slides/gradient.c
// TODO: name all these gradient ctx parameters after the expected type,
//       e.g. void *ctx -> void *umbra_gradient_linear for umbra_gradient_linear().

// A gradient is (x,y) -> t -> color.  umbra_gradient_coords is the first leg,
// a first-class effect in the same shape as umbra_shader / umbra_coverage /
// umbra_sdf: a flat (void *ctx, builder, xy) -> val32 callback that maps
// pixel coordinates to a clamped parameter t in [0, 1].  Colorizer shaders
// hold a (coords_fn, coords_ctx) pair, read once at IR-emit time.
typedef umbra_val32 umbra_gradient_coords(void *ctx, struct umbra_builder*,
                                           umbra_point_val32 xy);

// Linear gradient: t = a*x + b*y + c, clamped.  Fill from two points via
// umbra_gradient_linear_from().
struct umbra_gradient_linear {
    float a, b, c;
    int   :32;
};
struct umbra_gradient_linear umbra_gradient_linear_from(umbra_point p0, umbra_point p1);
umbra_val32 umbra_gradient_linear(void *ctx, struct umbra_builder*, umbra_point_val32 xy);

// Radial gradient: t = |xy - center| / radius, clamped.  Fill via
// umbra_gradient_radial_from().
struct umbra_gradient_radial {
    float cx, cy, inv_r;
    int   :32;
};
struct umbra_gradient_radial umbra_gradient_radial_from(umbra_point center, float radius);
umbra_val32 umbra_gradient_radial(void *ctx, struct umbra_builder*, umbra_point_val32 xy);

// Gradient colorizer shaders.  Each state struct holds the coords (fn, ctx)
// it invokes to map xy to t, plus the data (colors / lut / positions) needed
// to convert t to a color.  Callers own the state; pass &state as shader_ctx.
// coords_fn / coords_ctx are baked at IR-emit time -- mutating them after
// has no effect on a compiled program.  The colors/lut/pos *bytes* still
// mutate freely via umbra_uniforms.
struct umbra_shader_gradient_two_stops {
    umbra_gradient_coords *coords_fn;
    void                  *coords_ctx;
    umbra_color            c0, c1;
};
umbra_color_val32 umbra_shader_gradient_two_stops(void *ctx, struct umbra_builder*,
                                                   umbra_val32 x, umbra_val32 y);

struct umbra_shader_gradient_lut {
    umbra_gradient_coords *coords_fn;
    void                  *coords_ctx;
    float                  N;
    int                    :32;
    struct umbra_buf       lut;
};
umbra_color_val32 umbra_shader_gradient_lut(void *ctx, struct umbra_builder*,
                                             umbra_val32 x, umbra_val32 y);

struct umbra_shader_gradient_evenly_spaced_stops {
    umbra_gradient_coords *coords_fn;
    void                  *coords_ctx;
    float                  N;
    int                    :32;
    struct umbra_buf       colors;
};
umbra_color_val32 umbra_shader_gradient_evenly_spaced_stops(
    void *ctx, struct umbra_builder*, umbra_val32 x, umbra_val32 y);

struct umbra_shader_gradient {
    umbra_gradient_coords *coords_fn;
    void                  *coords_ctx;
    float                  N;
    int                    :32;
    struct umbra_buf       colors, pos;
};
umbra_color_val32 umbra_shader_gradient(void *umbra_shader_gradient, struct umbra_builder*,
                                         umbra_val32 x, umbra_val32 y);

// Supersample a wrapped shader.
struct umbra_supersample {
    umbra_shader *inner_fn;
    void         *inner_ctx;
    int           samples, :32;
};
umbra_color_val32 umbra_shader_supersample(void *umbra_supersample, struct umbra_builder*,
                                            umbra_val32 x, umbra_val32 y);

// Cover a rectangle, ctx is an umbra_rect*.
umbra_val32 umbra_coverage_rect(void *umbra_rect, struct umbra_builder*,
                                 umbra_val32 x, umbra_val32 y);

// A rectangle SDF, ctx is an umbra_rect*.
umbra_interval umbra_sdf_rect(void *umbra_rect, struct umbra_builder*,
                              umbra_interval x, umbra_interval y);

// TODO: move to slides
// 8-bit and SDF bitmap coverage, ctx is an umbra_buf*.
umbra_val32 umbra_coverage_bitmap(void *umbra_buf, struct umbra_builder*,
                                  umbra_val32 x, umbra_val32 y);
umbra_val32 umbra_coverage_sdf   (void *umbra_buf, struct umbra_builder*,
                                  umbra_val32 x, umbra_val32 y);

// TODO: move to slides
// Coverage from winding count buffer used by 2-pass Slug, ctx is an umbra_buf*.
umbra_val32 umbra_coverage_winding(void *umbra_buf, struct umbra_builder*,
                                   umbra_val32 x, umbra_val32 y);

// TODO: split this apart into a bitmap shader and a matrix shader combinator.
struct umbra_coverage_bitmap_matrix {
    struct umbra_matrix mat; int :32;
    struct umbra_bitmap bmp;
};
umbra_val32 umbra_coverage_bitmap_matrix(void *umbra_coverage_bitmap_matrix,
                                         struct umbra_builder*,
                                         umbra_val32 x, umbra_val32 y);

// TODO: move _lut() to slides, drop _lut_even().
void umbra_gradient_lut_even(float *out, int lut_n, int n_stops, umbra_color const *colors);
void umbra_gradient_lut(float *out, int lut_n, int n_stops, float const positions[],
                        umbra_color const *colors);
