#pragma once
#include "umbra.h"

typedef struct { umbra_val32 r, g, b, a; } umbra_color;

// TODO: rename ptr_bits to ptr or ptr_ix in these interfaces.  a little unclear what ptr_bits is.
struct umbra_fmt {
    char const *name;
    size_t      bpp;
    int         planes, :32;
    umbra_color (*load) (struct umbra_builder*, int ptr_bits);
    void        (*store)(struct umbra_builder*, int ptr_bits, umbra_color);
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

typedef umbra_color (*umbra_shader_fn)(struct umbra_builder*, struct umbra_uniforms_layout*,
                                       umbra_val32 x, umbra_val32 y);
typedef umbra_val32 (*umbra_coverage_fn)(struct umbra_builder*, struct umbra_uniforms_layout*,
                                         umbra_val32 x, umbra_val32 y);
typedef umbra_color (*umbra_blend_fn)(struct umbra_builder*, umbra_color src,
                                      umbra_color dst);
struct umbra_draw_layout {
    struct umbra_uniforms_layout uni;
    void                        *uniforms;
    size_t shader, coverage;
};

struct umbra_builder* umbra_draw_build(umbra_shader_fn shader, umbra_coverage_fn coverage,
                                       umbra_blend_fn blend, struct umbra_fmt fmt,
                                       struct umbra_draw_layout *layout);

umbra_color umbra_shader_solid      (struct umbra_builder*, struct umbra_uniforms_layout*, umbra_val32 x, umbra_val32 y);
umbra_color umbra_shader_linear_2   (struct umbra_builder*, struct umbra_uniforms_layout*, umbra_val32 x, umbra_val32 y);
umbra_color umbra_shader_radial_2   (struct umbra_builder*, struct umbra_uniforms_layout*, umbra_val32 x, umbra_val32 y);
umbra_color umbra_shader_linear_grad(struct umbra_builder*, struct umbra_uniforms_layout*, umbra_val32 x, umbra_val32 y);
umbra_color umbra_shader_radial_grad(struct umbra_builder*, struct umbra_uniforms_layout*, umbra_val32 x, umbra_val32 y);

umbra_color umbra_supersample(struct umbra_builder*, struct umbra_uniforms_layout*,
                              umbra_val32 x, umbra_val32 y,
                              umbra_shader_fn inner, int n);

void umbra_gradient_lut_even(float *out, int lut_n, int n_stops, float const colors[][4]);
void umbra_gradient_lut(float *out, int lut_n, int n_stops, float const positions[],
                        float const colors[][4]);

umbra_val32 umbra_coverage_rect         (struct umbra_builder*, struct umbra_uniforms_layout*, umbra_val32 x, umbra_val32 y);
umbra_val32 umbra_coverage_bitmap       (struct umbra_builder*, struct umbra_uniforms_layout*, umbra_val32 x, umbra_val32 y);
umbra_val32 umbra_coverage_sdf          (struct umbra_builder*, struct umbra_uniforms_layout*, umbra_val32 x, umbra_val32 y);
umbra_val32 umbra_coverage_bitmap_matrix(struct umbra_builder*, struct umbra_uniforms_layout*, umbra_val32 x, umbra_val32 y);
umbra_val32 umbra_coverage_wind         (struct umbra_builder*, struct umbra_uniforms_layout*, umbra_val32 x, umbra_val32 y);

umbra_color umbra_blend_src     (struct umbra_builder*, umbra_color src, umbra_color dst);
umbra_color umbra_blend_srcover (struct umbra_builder*, umbra_color src, umbra_color dst);
umbra_color umbra_blend_dstover (struct umbra_builder*, umbra_color src, umbra_color dst);
umbra_color umbra_blend_multiply(struct umbra_builder*, umbra_color src, umbra_color dst);
