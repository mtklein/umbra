#pragma once
#include "umbra.h"
#include "umbra_interval.h"

// TODO: rethink how effects like coverage/shader/sdf and systems that use them
// communicate their uniforms.  can we radically simplify by having them told
// what niform buffer index to use as part of the request to fill out the
// builder, and then later have them provide a umbra_buf pointing directly into
// their own managed memory?  things would probably trend towards one
// destination buffer at ix=0, and then a bunch of little uniform buffers at
// 1,2.., one for each effect in the system.  key signs of a good refactor are
// fewer types like umbra_uniforms_layout and umbra_draw_layout needed, ideally
// no calls to fill at all (rather, buffers just point to the uniforms directly).

typedef struct {
    float x, y;
} umbra_point;

// TODO: any more useful as union { struct { float l,t,r,b; } struct { point lt,rb } } ?
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

// TODO: move shader/coverage/sdf over to our full polymorphim pattern with opaque types,
//       free() hooks, etc.

struct umbra_shader {
    umbra_color_val32 (*build)(struct umbra_shader*,
                               struct umbra_builder*,
                               struct umbra_uniforms_layout*,
                               umbra_val32 x, umbra_val32 y);
    void (*fill)(struct umbra_shader const*, void *uniforms);
};

struct umbra_coverage {
    umbra_val32 (*build)(struct umbra_coverage*,
                         struct umbra_builder*,
                         struct umbra_uniforms_layout*,
                         umbra_val32 x, umbra_val32 y);
    void (*fill)(struct umbra_coverage const*, void *uniforms);
};

// Signed distance function returning f(x,y) where f < 0 means inside.
struct umbra_sdf {
    umbra_interval (*build)(struct umbra_sdf*,
                            struct umbra_builder*,
                            struct umbra_uniforms_layout*,
                            umbra_interval x, umbra_interval y);
    void (*fill)(struct umbra_sdf const*, void *uniforms);
};

// TODO: bool hard_edge -> int quality

struct umbra_sdf_coverage {
    struct umbra_coverage base;
    struct umbra_sdf     *sdf;
    int                   hard_edge, :32;
};
struct umbra_sdf_coverage umbra_sdf_coverage(struct umbra_sdf*, _Bool hard_edge);

typedef umbra_color_val32 (*umbra_blend_fn)(struct umbra_builder*,
                                            umbra_color_val32 src, umbra_color_val32 dst);
umbra_color_val32 umbra_blend_src     (struct umbra_builder*,
                                       umbra_color_val32 src, umbra_color_val32 dst);
umbra_color_val32 umbra_blend_srcover (struct umbra_builder*,
                                       umbra_color_val32 src, umbra_color_val32 dst);
umbra_color_val32 umbra_blend_dstover (struct umbra_builder*,
                                       umbra_color_val32 src, umbra_color_val32 dst);
umbra_color_val32 umbra_blend_multiply(struct umbra_builder*,
                                       umbra_color_val32 src, umbra_color_val32 dst);


struct umbra_draw_layout {
    struct umbra_uniforms_layout uni; int :32;
    void                        *uniforms;
};

// TODO: reorder to pass coverage first
struct umbra_builder* umbra_draw_builder(struct umbra_shader*,
                                         struct umbra_coverage*,
                                         umbra_blend_fn,
                                         struct umbra_fmt,
                                         struct umbra_draw_layout*);

void umbra_draw_fill(struct umbra_draw_layout const*,
                     struct umbra_shader const*,
                     struct umbra_coverage const*);

struct umbra_sdf_draw_config {
    _Bool hard_edge;
};

// TODO: rename umbra_sdf_draw and umbra_sdf_draw_foo()
struct umbra_sdf_draw* umbra_sdf_draw(struct umbra_backend*,
                                              struct umbra_sdf *coverage,
                                              struct umbra_sdf_draw_config,
                                              struct umbra_shader*,
                                              umbra_blend_fn,
                                              struct umbra_fmt,
                                              struct umbra_draw_layout*);
void umbra_sdf_draw_queue(struct umbra_sdf_draw const*,
                              int l, int t, int r, int b, struct umbra_buf[]);
void umbra_sdf_draw_fill(struct umbra_draw_layout const*,
                             struct umbra_sdf const*,
                             struct umbra_shader const*);
void umbra_sdf_draw_free(struct umbra_sdf_draw*);

// TODO: add umbra_shader_gradient_{linear,radial}_evenly_spaced_stops specializations
//       that share the general case's colors-buffer layout but skip the per-pixel stop
//       search: with uniform spacing, the bracketing segment is just idx = floor(t*(N-1)),
//       so the shader can gather the two neighboring colors directly.  No LUT, no pos
//       buffer, no runtime loop.  Would replace today's "evenly-spaced via umbra_gradient_lut_even
//       feeding _lut" pattern, which is the wrong shape: even spacing is a property the shader
//       can exploit, not something that motivates preprocessing.

struct umbra_shader_solid {
    struct umbra_shader base;
    umbra_color color;
    int color_off, :32;
};
struct umbra_shader_solid umbra_shader_solid(umbra_color color);

struct umbra_shader_gradient_linear_two_stops {
    struct umbra_shader base;
    umbra_point p0, p1;
    umbra_color c0, c1;
    int coeffs_off, colors_off;
};
struct umbra_shader_gradient_linear_two_stops umbra_shader_gradient_linear_two_stops(
    umbra_point p0, umbra_point p1, umbra_color c0, umbra_color c1);

struct umbra_shader_gradient_radial_two_stops {
    struct umbra_shader base;
    umbra_point center;
    float       radius; int :32;
    umbra_color c0, c1;
    int coeffs_off, colors_off;
};
struct umbra_shader_gradient_radial_two_stops umbra_shader_gradient_radial_two_stops(
    umbra_point center, float radius, umbra_color c0, umbra_color c1);

struct umbra_shader_gradient_linear_lut {
    struct umbra_shader base;
    umbra_point p0, p1;
    struct umbra_buf lut;
    int coeffs_off, lut_off;
};
struct umbra_shader_gradient_linear_lut umbra_shader_gradient_linear_lut(
    umbra_point p0, umbra_point p1, struct umbra_buf lut);

struct umbra_shader_gradient_radial_lut {
    struct umbra_shader base;
    umbra_point center;
    float       radius; int :32;
    struct umbra_buf lut;
    int coeffs_off, lut_off;
};
struct umbra_shader_gradient_radial_lut umbra_shader_gradient_radial_lut(
    umbra_point center, float radius, struct umbra_buf lut);

struct umbra_shader_gradient_linear {
    struct umbra_shader base;
    umbra_point p0, p1;
    struct umbra_buf colors, pos;
    int coeffs_off, colors_off, pos_off, :32;
};
struct umbra_shader_gradient_linear umbra_shader_gradient_linear(umbra_point p0, umbra_point p1,
                                                           struct umbra_buf colors,
                                                           struct umbra_buf pos);

struct umbra_shader_gradient_radial {
    struct umbra_shader base;
    umbra_point center;
    float       radius; int :32;
    struct umbra_buf colors, pos;
    int coeffs_off, colors_off, pos_off, :32;
};
struct umbra_shader_gradient_radial umbra_shader_gradient_radial(umbra_point center, float radius,
                                                           struct umbra_buf colors,
                                                           struct umbra_buf pos);

  // TODO: n -> samples
struct umbra_shader_supersample {
    struct umbra_shader base;
    struct umbra_shader *inner;
    int n, :32;
};
struct umbra_shader_supersample umbra_shader_supersample(struct umbra_shader *inner, int n);

struct umbra_coverage_rect {
    struct umbra_coverage base;
    umbra_rect rect;
    int rect_off, :32;
};
struct umbra_coverage_rect umbra_coverage_rect(umbra_rect);

struct umbra_sdf_rect {
    struct umbra_sdf base;
    umbra_rect rect;
    int rect_off, :32;
};
struct umbra_sdf_rect umbra_sdf_rect(umbra_rect);

struct umbra_coverage_bitmap {
    struct umbra_coverage base;
    struct umbra_buf bmp;
    int bmp_off, :32;
};
struct umbra_coverage_bitmap umbra_coverage_bitmap(struct umbra_buf bmp);

struct umbra_coverage_sdf {
    struct umbra_coverage base;
    struct umbra_buf bmp;
    int bmp_off, :32;
};
struct umbra_coverage_sdf umbra_coverage_sdf(struct umbra_buf bmp);

struct umbra_coverage_bitmap_matrix {
    struct umbra_coverage base;
    struct umbra_matrix mat; int :32;
    struct umbra_bitmap bmp;
    int mat_off, bmp_off;
};
struct umbra_coverage_bitmap_matrix umbra_coverage_bitmap_matrix(struct umbra_matrix,
                                                                 struct umbra_bitmap);
// TODO: needs a better name I think.
struct umbra_coverage_wind {
    struct umbra_coverage base;
    struct umbra_buf wind;
    int wind_off, :32;
};
struct umbra_coverage_wind umbra_coverage_wind(struct umbra_buf wind);

void umbra_gradient_lut_even(float *out, int lut_n, int n_stops, umbra_color const *colors);
void umbra_gradient_lut(float *out, int lut_n, int n_stops, float const positions[],
                        umbra_color const *colors);
