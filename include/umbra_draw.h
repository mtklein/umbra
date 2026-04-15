#pragma once
#include "umbra.h"

typedef struct { umbra_val32 r, g, b, a; } umbra_color;

struct umbra_matrix {
    float sx, kx, tx,
          ky, sy, ty,
          p0, p1, p2;
};

struct umbra_bitmap {
    struct umbra_buf buf;
    int w, h;
};

struct umbra_fmt {
    char const *name;
    size_t      bpp;
    int         planes, :32;
    umbra_color (*load) (struct umbra_builder*, int ptr_ix);
    void        (*store)(struct umbra_builder*, int ptr_ix, umbra_color);
};
extern struct umbra_fmt const umbra_fmt_8888;
extern struct umbra_fmt const umbra_fmt_565;
extern struct umbra_fmt const umbra_fmt_1010102;
extern struct umbra_fmt const umbra_fmt_fp16;
extern struct umbra_fmt const umbra_fmt_fp16_planar;

umbra_color umbra_load_8888       (struct umbra_builder*, umbra_ptr32);
void        umbra_store_8888      (struct umbra_builder*, umbra_ptr32, umbra_color);
umbra_color umbra_load_565        (struct umbra_builder*, umbra_ptr16);
void        umbra_store_565       (struct umbra_builder*, umbra_ptr16, umbra_color);
umbra_color umbra_load_1010102    (struct umbra_builder*, umbra_ptr32);
void        umbra_store_1010102   (struct umbra_builder*, umbra_ptr32, umbra_color);
umbra_color umbra_load_fp16       (struct umbra_builder*, umbra_ptr64);
void        umbra_store_fp16      (struct umbra_builder*, umbra_ptr64, umbra_color);
umbra_color umbra_load_fp16_planar(struct umbra_builder*, umbra_ptr16);
void        umbra_store_fp16_planar(struct umbra_builder*, umbra_ptr16, umbra_color);

struct umbra_shader {
    umbra_color (*build)(struct umbra_shader*, struct umbra_builder*,
                         struct umbra_uniforms_layout*, umbra_val32 x, umbra_val32 y);
    void        (*fill )(struct umbra_shader const*, void *uniforms);
};

struct umbra_coverage {
    umbra_val32 (*build)(struct umbra_coverage*, struct umbra_builder*,
                         struct umbra_uniforms_layout*, umbra_val32 x, umbra_val32 y);
    void        (*fill )(struct umbra_coverage const*, void *uniforms);
};

typedef umbra_color (*umbra_blend_fn)(struct umbra_builder*, umbra_color src, umbra_color dst);

struct umbra_draw_layout {
    struct umbra_uniforms_layout uni; int :32;
    void                        *uniforms;
};

struct umbra_builder* umbra_draw_builder(struct umbra_shader*, struct umbra_coverage*,
                                       umbra_blend_fn, struct umbra_fmt,
                                       struct umbra_draw_layout*);
void umbra_draw_fill(struct umbra_draw_layout const*,
                     struct umbra_shader const*,
                     struct umbra_coverage const*);

// Compiled draw bundle: one program with coverage multiplied in (boundary
// pixels), one without (interior, α == 1), and an optional interval_program
// over the coverage alone for tile pruning.  The interval side is omitted
// when the coverage IR contains an op interval.c cannot yet bound — in that
// case umbra_draw_queue falls back to a single flat partial-coverage dispatch.
struct umbra_draw* umbra_draw(struct umbra_backend*,
                              struct umbra_shader*, struct umbra_coverage*,
                              umbra_blend_fn, struct umbra_fmt,
                              struct umbra_draw_layout*);
void               umbra_draw_free(struct umbra_draw*);

// Dispatch the bundle over rect [l, t, r, b).  If the draw has an
// interval_program, descends a quadtree — skipping tiles where the α bound
// is <= 0, running the full-coverage program on tiles where the α bound is
// >= 1, running the partial-coverage program only on tiles that straddle the
// boundary down to a minimum-size floor.  Otherwise, a single flat dispatch.
void umbra_draw_queue(struct umbra_draw const*,
                      int l, int t, int r, int b, struct umbra_buf[]);

struct umbra_shader_solid {
    struct umbra_shader base;
    float  color[4];
    int off_, :32;
};
struct umbra_shader_solid umbra_shader_solid(float const color[4]);

struct umbra_shader_linear_2 {
    struct umbra_shader base;
    float  grad[3], color[8]; int :32;
    int fi_, ci_;
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

struct umbra_shader_supersample {
    struct umbra_shader base;
    struct umbra_shader *inner;
    int n, :32;
};
struct umbra_shader_supersample umbra_shader_supersample(struct umbra_shader *inner, int n);

struct umbra_coverage_rect {
    struct umbra_coverage base;
    float  rect[4];
    int off_, :32;
};
struct umbra_coverage_rect umbra_coverage_rect(float const rect[4]);

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

struct umbra_coverage_wind {
    struct umbra_coverage base;
    struct umbra_buf wind;
    int off_, :32;
};
struct umbra_coverage_wind umbra_coverage_wind(struct umbra_buf wind);

umbra_color umbra_blend_src     (struct umbra_builder*, umbra_color src, umbra_color dst);
umbra_color umbra_blend_srcover (struct umbra_builder*, umbra_color src, umbra_color dst);
umbra_color umbra_blend_dstover (struct umbra_builder*, umbra_color src, umbra_color dst);
umbra_color umbra_blend_multiply(struct umbra_builder*, umbra_color src, umbra_color dst);

void umbra_gradient_lut_even(float *out, int lut_n, int n_stops, float const colors[][4]);
void umbra_gradient_lut(float *out, int lut_n, int n_stops, float const positions[],
                        float const colors[][4]);
