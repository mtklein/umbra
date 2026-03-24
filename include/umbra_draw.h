#pragma once
#include "umbra.h"

typedef struct {
    umbra_val r, g, b, a;
} umbra_color;

typedef umbra_color (*umbra_shader_fn)(struct umbra_builder *, umbra_val x, umbra_val y);
typedef umbra_val (*umbra_coverage_fn)(struct umbra_builder *, umbra_val x, umbra_val y,
                                      umbra_val ix);
typedef umbra_color (*umbra_blend_fn)(struct umbra_builder *, umbra_color src,
                                      umbra_color dst);
typedef umbra_color (*umbra_load_fn)(struct umbra_builder *, umbra_ptr ptr, umbra_val ix);
typedef void (*umbra_store_fn)(struct umbra_builder *, umbra_ptr ptr, umbra_val ix,
                               umbra_color);

typedef struct {
    int rs;
    int shader, coverage;
    int uni_len, ps;
} umbra_draw_layout;

struct umbra_builder *umbra_draw_build(umbra_shader_fn shader, umbra_coverage_fn coverage,
                                       umbra_blend_fn blend, umbra_load_fn load,
                                       umbra_store_fn store, umbra_draw_layout *layout);

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

umbra_val umbra_coverage_rect(struct umbra_builder *, umbra_val x, umbra_val y, umbra_val ix);
umbra_val umbra_coverage_bitmap(struct umbra_builder *, umbra_val x, umbra_val y, umbra_val ix);
umbra_val umbra_coverage_sdf(struct umbra_builder *, umbra_val x, umbra_val y, umbra_val ix);
umbra_val umbra_coverage_bitmap_matrix(struct umbra_builder *, umbra_val x, umbra_val y,
                                       umbra_val ix);
umbra_val umbra_coverage_wind(struct umbra_builder *, umbra_val x, umbra_val y, umbra_val ix);

umbra_color umbra_blend_src(struct umbra_builder *, umbra_color src, umbra_color dst);
umbra_color umbra_blend_srcover(struct umbra_builder *, umbra_color src, umbra_color dst);
umbra_color umbra_blend_dstover(struct umbra_builder *, umbra_color src, umbra_color dst);
umbra_color umbra_blend_multiply(struct umbra_builder *, umbra_color src, umbra_color dst);

umbra_color umbra_load_8888(struct umbra_builder *, umbra_ptr ptr, umbra_val ix);
void umbra_store_8888(struct umbra_builder *, umbra_ptr ptr, umbra_val ix, umbra_color);
umbra_color umbra_load_565(struct umbra_builder *, umbra_ptr ptr, umbra_val ix);
void umbra_store_565(struct umbra_builder *, umbra_ptr ptr, umbra_val ix, umbra_color);
umbra_color umbra_load_1010102(struct umbra_builder *, umbra_ptr ptr, umbra_val ix);
void umbra_store_1010102(struct umbra_builder *, umbra_ptr ptr, umbra_val ix, umbra_color);
umbra_color umbra_load_fp16(struct umbra_builder *, umbra_ptr ptr, umbra_val ix);
void umbra_store_fp16(struct umbra_builder *, umbra_ptr ptr, umbra_val ix, umbra_color);
umbra_color umbra_load_fp16_planar(struct umbra_builder *, umbra_ptr ptr, umbra_val ix);
void        umbra_store_fp16_planar(struct umbra_builder *, umbra_ptr ptr, umbra_val ix,
                                    umbra_color);

typedef struct {
    float a, b, c, d, e, f, g;
} umbra_transfer;

extern umbra_transfer const umbra_transfer_srgb;
extern umbra_transfer const umbra_transfer_gamma22;

umbra_color umbra_transfer_apply(struct umbra_builder *, umbra_color,
                                 umbra_transfer const *);
umbra_color umbra_transfer_invert(struct umbra_builder *, umbra_color,
                                  umbra_transfer const *);

void umbra_transfer_lut_invert(float out[256], umbra_transfer const *);
void umbra_transfer_lut_apply(float out[256], umbra_transfer const *);
