#pragma once
#include "umbra.h"
#include "umbra_interval.h"

typedef struct {
    umbra_val32 r, g, b, a;
} umbra_color_val32;

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

// TODO: maybe umbra_builder really should fold in umbra_uniforms_layout?
//       they seem to go together more often than not.

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


// TODO: gather the uniforms / layout types and methods together and explain them better
// TODO: maybe we need some better names for the fill() methods... something like update_uniforms?
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

struct umbra_sdf_dispatch_config {
    _Bool hard_edge;
};

// TODO: rename umbra_sdf_draw and umbra_sdf_draw_foo()
struct umbra_sdf_dispatch* umbra_sdf_dispatch(struct umbra_backend*,
                                              struct umbra_sdf *coverage,
                                              struct umbra_sdf_dispatch_config,
                                              struct umbra_shader*,
                                              umbra_blend_fn,
                                              struct umbra_fmt,
                                              struct umbra_draw_layout*);
void umbra_sdf_dispatch_queue(struct umbra_sdf_dispatch const*,
                              int l, int t, int r, int b, struct umbra_buf[]);
void umbra_sdf_dispatch_fill(struct umbra_draw_layout const*,
                             struct umbra_sdf const*,
                             struct umbra_shader const*);
void umbra_sdf_dispatch_free(struct umbra_sdf_dispatch*);

// TODO: no good reason for any of the trailing underscores, rename off_ -> off and similar
//       for all these types.  color -> rgba, split any color[8] into two color inputs,
//       grad[3] / grad[4] parameters are unclear.  rename shaders using luts to have _lut
//       in their names.  gradient shader names in general all need a fresh rethink so that
//       they make sense as an ensemble, with names clarifying their differences.

struct umbra_shader_solid {
    struct umbra_shader base;
    float  color[4];
    int off_, :32;
};
struct umbra_shader_solid umbra_shader_solid(float const color[4]);

struct umbra_shader_linear_2 {
    struct umbra_shader base;
    float  grad[3], color[8]; int :32;
    int fi_, ci_;  // TODO: fi and ci are unclear names, all throughout
};
struct umbra_shader_linear_2 umbra_shader_linear_2(float const grad[3], float const color[8]);

struct umbra_shader_radial_2 {
    struct umbra_shader base;
    float  grad[3], color[8]; int :32;
    int fi_, ci_;
};
struct umbra_shader_radial_2 umbra_shader_radial_2(float const grad[3], float const color[8]);

struct umbra_shader_linear_grad {
    struct umbra_shader base;
    float          grad[4];
    struct umbra_buf lut;
    int fi_, lut_off_;
};
struct umbra_shader_linear_grad umbra_shader_linear_grad(float const grad[4], struct umbra_buf lut);

struct umbra_shader_radial_grad {
    struct umbra_shader base;
    float          grad[4];
    struct umbra_buf lut;
    int fi_, lut_off_;
};
struct umbra_shader_radial_grad umbra_shader_radial_grad(float const grad[4], struct umbra_buf lut);

struct umbra_shader_linear_stops {
    struct umbra_shader base;
    float          grad[4];
    struct umbra_buf colors, pos;
    int fi_, colors_off_, pos_off_, :32;
};
struct umbra_shader_linear_stops umbra_shader_linear_stops(float const grad[4],
                                                           struct umbra_buf colors,
                                                           struct umbra_buf pos);

struct umbra_shader_radial_stops {
    struct umbra_shader base;
    float          grad[4];
    struct umbra_buf colors, pos;
    int fi_, colors_off_, pos_off_, :32;
};
struct umbra_shader_radial_stops umbra_shader_radial_stops(float const grad[4],
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
    float  rect[4];  // float ltrb
    int off_, :32;
};
// TODO: change rect interfaces from float[4] to float l, float t, float r, float b.
struct umbra_coverage_rect umbra_coverage_rect(float const rect[4]);

struct umbra_sdf_rect {
    struct umbra_sdf base;
    float  rect[4];
    int off_, :32;
};
struct umbra_sdf_rect umbra_sdf_rect(float const rect[4]);

struct umbra_coverage_bitmap {
    struct umbra_coverage base;
    struct umbra_buf bmp;
    int bmp_off_, :32;
};
struct umbra_coverage_bitmap umbra_coverage_bitmap(struct umbra_buf bmp);

struct umbra_coverage_sdf {
    struct umbra_coverage base;
    struct umbra_buf bmp;
    int bmp_off_, :32;
};
struct umbra_coverage_sdf umbra_coverage_sdf(struct umbra_buf bmp);

struct umbra_coverage_bitmap_matrix {
    struct umbra_coverage base;
    struct umbra_matrix mat; int :32;
    struct umbra_bitmap bmp;
    int fi_, bmp_off_;
};
struct umbra_coverage_bitmap_matrix umbra_coverage_bitmap_matrix(struct umbra_matrix,
                                                                 struct umbra_bitmap);
// TODO: needs a better name I think.
struct umbra_coverage_wind {
    struct umbra_coverage base;
    struct umbra_buf wind;
    int off_, :32;
};
struct umbra_coverage_wind umbra_coverage_wind(struct umbra_buf wind);

void umbra_gradient_lut_even(float *out, int lut_n, int n_stops, float const colors[][4]);
void umbra_gradient_lut(float *out, int lut_n, int n_stops, float const positions[],
                        float const colors[][4]);
