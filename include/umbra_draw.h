#pragma once
#include "umbra.h"

enum umbra_fmt {
    umbra_fmt_8888,
    umbra_fmt_565,
    umbra_fmt_1010102,
    umbra_fmt_fp16,
    umbra_fmt_fp16_planar,
};
size_t umbra_fmt_size(enum umbra_fmt);

typedef struct { umbra_val r, g, b, a; } umbra_color;

umbra_color umbra_load_color (struct umbra_builder*, umbra_ptr, enum umbra_fmt);
void        umbra_store_color(struct umbra_builder*, umbra_ptr, umbra_color, enum umbra_fmt);

typedef umbra_color (*umbra_shader_fn)(struct umbra_builder *, struct umbra_uniforms *,
                                       umbra_val x, umbra_val y);
typedef umbra_val (*umbra_coverage_fn)(struct umbra_builder *, struct umbra_uniforms *,
                                       umbra_val x, umbra_val y);
typedef umbra_color (*umbra_blend_fn)(struct umbra_builder *, umbra_color src,
                                      umbra_color dst);
struct umbra_draw_layout {
    struct umbra_uniforms *uni;
    size_t shader, coverage;
};

struct umbra_builder *umbra_draw_build(umbra_shader_fn shader, umbra_coverage_fn coverage,
                                       umbra_blend_fn blend, enum umbra_fmt fmt,
                                       struct umbra_draw_layout *layout);

umbra_color umbra_shader_solid      (struct umbra_builder *, struct umbra_uniforms *, umbra_val x, umbra_val y);
umbra_color umbra_shader_linear_2   (struct umbra_builder *, struct umbra_uniforms *, umbra_val x, umbra_val y);
umbra_color umbra_shader_radial_2   (struct umbra_builder *, struct umbra_uniforms *, umbra_val x, umbra_val y);
umbra_color umbra_shader_linear_grad(struct umbra_builder *, struct umbra_uniforms *, umbra_val x, umbra_val y);
umbra_color umbra_shader_radial_grad(struct umbra_builder *, struct umbra_uniforms *, umbra_val x, umbra_val y);

umbra_color umbra_supersample(struct umbra_builder *, struct umbra_uniforms *,
                              umbra_val x, umbra_val y,
                              umbra_shader_fn inner, int n);

void umbra_gradient_lut_even(float *out, int lut_n, int n_stops, float const colors[][4]);
void umbra_gradient_lut(float *out, int lut_n, int n_stops, float const positions[],
                        float const colors[][4]);

umbra_val umbra_coverage_rect         (struct umbra_builder *, struct umbra_uniforms *, umbra_val x, umbra_val y);
umbra_val umbra_coverage_bitmap       (struct umbra_builder *, struct umbra_uniforms *, umbra_val x, umbra_val y);
umbra_val umbra_coverage_sdf          (struct umbra_builder *, struct umbra_uniforms *, umbra_val x, umbra_val y);
umbra_val umbra_coverage_bitmap_matrix(struct umbra_builder *, struct umbra_uniforms *, umbra_val x, umbra_val y);
umbra_val umbra_coverage_wind         (struct umbra_builder *, struct umbra_uniforms *, umbra_val x, umbra_val y);

umbra_color umbra_blend_src     (struct umbra_builder *, umbra_color src, umbra_color dst);
umbra_color umbra_blend_srcover (struct umbra_builder *, umbra_color src, umbra_color dst);
umbra_color umbra_blend_dstover (struct umbra_builder *, umbra_color src, umbra_color dst);
umbra_color umbra_blend_multiply(struct umbra_builder *, umbra_color src, umbra_color dst);
