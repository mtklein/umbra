#pragma once
#include "umbra.h"

typedef struct { umbra_half r, g, b, a; } umbra_color;

typedef umbra_color (*umbra_shader_fn) (struct umbra_basic_block*, umbra_f32 x, umbra_f32 y);
typedef umbra_half  (*umbra_coverage_fn)(struct umbra_basic_block*, umbra_f32 x, umbra_f32 y);
typedef umbra_color (*umbra_blend_fn)  (struct umbra_basic_block*, umbra_color src, umbra_color dst);
typedef umbra_color (*umbra_load_fn)   (struct umbra_basic_block*, umbra_ptr ptr, umbra_i32 ix);
typedef void        (*umbra_store_fn)  (struct umbra_basic_block*, umbra_ptr ptr, umbra_i32 ix,
                                        umbra_color);

struct umbra_basic_block* umbra_draw_build(umbra_shader_fn   shader,
                                           umbra_coverage_fn coverage,
                                           umbra_blend_fn    blend,
                                           umbra_load_fn     load,
                                           umbra_store_fn    store);

//   solid:        p3 = __fp16[4] {r,g,b,a}
//   linear_2:     p3 = __fp16[8] {r0,g0,b0,a0, r1,g1,b1,a1}, p5 = float[3] {a,b,c}
//   radial_2:     p3 = __fp16[8] {r0,g0,b0,a0, r1,g1,b1,a1}, p5 = float[3] {cx,cy,inv_r}
//   linear_grad:  p3 = __fp16[N*4] RGBA LUT,                  p5 = float[4] {a,b,c,N}
//   radial_grad:  p3 = __fp16[N*4] RGBA LUT,                  p5 = float[4] {cx,cy,inv_r,N}
umbra_color umbra_shader_solid      (struct umbra_basic_block*, umbra_f32 x, umbra_f32 y);
umbra_color umbra_shader_linear_2   (struct umbra_basic_block*, umbra_f32 x, umbra_f32 y);
umbra_color umbra_shader_radial_2   (struct umbra_basic_block*, umbra_f32 x, umbra_f32 y);
umbra_color umbra_shader_linear_grad(struct umbra_basic_block*, umbra_f32 x, umbra_f32 y);
umbra_color umbra_shader_radial_grad(struct umbra_basic_block*, umbra_f32 x, umbra_f32 y);

void umbra_gradient_lut_even(__fp16 *out, int lut_n,
                             int n_stops, __fp16 const colors[][4]);
void umbra_gradient_lut(__fp16 *out, int lut_n,
                        int n_stops, float const positions[],
                        __fp16 const colors[][4]);

umbra_half umbra_coverage_rect         (struct umbra_basic_block*, umbra_f32 x, umbra_f32 y);
umbra_half umbra_coverage_bitmap       (struct umbra_basic_block*, umbra_f32 x, umbra_f32 y);
umbra_half umbra_coverage_sdf          (struct umbra_basic_block*, umbra_f32 x, umbra_f32 y);
umbra_half umbra_coverage_bitmap_matrix(struct umbra_basic_block*, umbra_f32 x, umbra_f32 y);

umbra_color umbra_blend_src     (struct umbra_basic_block*, umbra_color src, umbra_color dst);
umbra_color umbra_blend_srcover (struct umbra_basic_block*, umbra_color src, umbra_color dst);
umbra_color umbra_blend_dstover (struct umbra_basic_block*, umbra_color src, umbra_color dst);
umbra_color umbra_blend_multiply(struct umbra_basic_block*, umbra_color src, umbra_color dst);

umbra_color umbra_load_8888 (struct umbra_basic_block*, umbra_ptr ptr, umbra_i32 ix);
void        umbra_store_8888(struct umbra_basic_block*, umbra_ptr ptr, umbra_i32 ix, umbra_color);
umbra_color umbra_load_fp16 (struct umbra_basic_block*, umbra_ptr ptr, umbra_i32 ix);
void        umbra_store_fp16(struct umbra_basic_block*, umbra_ptr ptr, umbra_i32 ix, umbra_color);
