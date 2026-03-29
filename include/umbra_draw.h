#pragma once
#include "umbra.h"

typedef umbra_color (*umbra_shader_fn)(struct umbra_builder *, umbra_val x, umbra_val y);
typedef umbra_val (*umbra_coverage_fn)(struct umbra_builder *, umbra_val x, umbra_val y);
typedef umbra_color (*umbra_blend_fn)(struct umbra_builder *, umbra_color src,
                                      umbra_color dst);
typedef struct {
    int shader, coverage;
    int uni_len, ps;
} umbra_draw_layout;

struct umbra_builder *umbra_draw_build(umbra_shader_fn shader, umbra_coverage_fn coverage,
                                       umbra_blend_fn blend,
                                       umbra_draw_layout *layout);

umbra_color umbra_shader_solid(struct umbra_builder *, umbra_val x, umbra_val y);
umbra_color umbra_shader_linear_2(struct umbra_builder *, umbra_val x, umbra_val y);
umbra_color umbra_shader_radial_2(struct umbra_builder *, umbra_val x, umbra_val y);
umbra_color umbra_shader_linear_grad(struct umbra_builder *, umbra_val x, umbra_val y);
umbra_color umbra_shader_radial_grad(struct umbra_builder *, umbra_val x, umbra_val y);

umbra_color umbra_supersample(struct umbra_builder *, umbra_val x, umbra_val y,
                              umbra_shader_fn inner, int n);

void umbra_gradient_lut_even(float *out, int lut_n, int n_stops, float const colors[][4]);
void umbra_gradient_lut(float *out, int lut_n, int n_stops, float const positions[],
                        float const colors[][4]);

umbra_val umbra_coverage_rect(struct umbra_builder *, umbra_val x, umbra_val y);
umbra_val umbra_coverage_bitmap(struct umbra_builder *, umbra_val x, umbra_val y);
umbra_val umbra_coverage_sdf(struct umbra_builder *, umbra_val x, umbra_val y);
umbra_val umbra_coverage_bitmap_matrix(struct umbra_builder *, umbra_val x, umbra_val y);
umbra_val umbra_coverage_wind(struct umbra_builder *, umbra_val x, umbra_val y);

umbra_color umbra_blend_src(struct umbra_builder *, umbra_color src, umbra_color dst);
umbra_color umbra_blend_srcover(struct umbra_builder *, umbra_color src, umbra_color dst);
umbra_color umbra_blend_dstover(struct umbra_builder *, umbra_color src, umbra_color dst);
umbra_color umbra_blend_multiply(struct umbra_builder *, umbra_color src, umbra_color dst);

extern umbra_transfer const umbra_transfer_srgb;

umbra_color umbra_transfer_apply(struct umbra_builder *, umbra_color,
                                 umbra_transfer const *);
umbra_color umbra_transfer_invert(struct umbra_builder *, umbra_color,
                                  umbra_transfer const *);

void umbra_transfer_lut_invert(float out[256], umbra_transfer const *);
void umbra_transfer_lut_apply(float out[256], umbra_transfer const *);
