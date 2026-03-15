#pragma once
#include "umbra.h"

// 4-channel half color.
typedef struct { umbra_half r, g, b, a; } umbra_color;

// Stage function types.  Each emits IR into a shared basic block.
//
// Shader:   (x,y) -> color.  Procedural source color (gradients, solid, etc.)
// Coverage: (x,y) -> half.   Geometric coverage mask (rect, circle, path, etc.)
// Blend:    (src, dst) -> color.  Compositing (srcover, multiply, etc.)
// Load:     (ptr, ix) -> color.  Decode pixels from memory.
// Store:    (ptr, ix, color).     Encode pixels to memory.

typedef umbra_color (*umbra_shader_fn) (struct umbra_basic_block*, umbra_v32 x, umbra_v32 y);
typedef umbra_half  (*umbra_coverage_fn)(struct umbra_basic_block*, umbra_v32 x, umbra_v32 y);
typedef umbra_color (*umbra_blend_fn)  (struct umbra_basic_block*, umbra_color src, umbra_color dst);
typedef umbra_color (*umbra_load_fn)   (struct umbra_basic_block*, umbra_ptr ptr, umbra_v32 ix);
typedef void        (*umbra_store_fn)  (struct umbra_basic_block*, umbra_ptr ptr, umbra_v32 ix,
                                        umbra_color);

// Build a complete draw pipeline into a new basic block.
//
// Pointer layout:
//   p0 = destination pixels (read/write)
//   p1 = x0 (uniform int32: x offset of this span)
//   p2 = y  (uniform int32: y coordinate of this scanline)
//
// The pipeline iterates n pixels starting at x0:
//   x = x0 + lane  (varying)
//   y = y           (uniform)
//
// Pipeline:
//   src  = shader(x, y)
//   dst  = load(p0, lane)
//   out  = blend(src, dst)
//   out  = lerp(dst, out, coverage(x, y))   [if coverage != NULL]
//   store(p0, lane, out)
//
// Any stage can be NULL:
//   shader=NULL   → src = {0,0,0,0}
//   coverage=NULL → no coverage lerp (full coverage)
//   blend=NULL    → out = src (no blending, just paint)
//
// Returns a basic block ready for optimize + compile.
struct umbra_basic_block* umbra_draw_build(umbra_shader_fn   shader,
                                           umbra_coverage_fn coverage,
                                           umbra_blend_fn    blend,
                                           umbra_load_fn     load,
                                           umbra_store_fn    store);

// --- Built-in stages ---

// Shaders
//   solid:        p3 = __fp16[4] {r,g,b,a}
//   linear_2:     p3 = __fp16[8] {r0,g0,b0,a0, r1,g1,b1,a1}, p5 = float[3] {a,b,c}
//   radial_2:     p3 = __fp16[8] {r0,g0,b0,a0, r1,g1,b1,a1}, p5 = float[3] {cx,cy,inv_r}
//   linear_grad:  p3 = __fp16[N*4] RGBA LUT,                  p5 = float[4] {a,b,c,N}
//   radial_grad:  p3 = __fp16[N*4] RGBA LUT,                  p5 = float[4] {cx,cy,inv_r,N}
umbra_color umbra_shader_solid      (struct umbra_basic_block*, umbra_v32 x, umbra_v32 y);
umbra_color umbra_shader_linear_2   (struct umbra_basic_block*, umbra_v32 x, umbra_v32 y);
umbra_color umbra_shader_radial_2   (struct umbra_basic_block*, umbra_v32 x, umbra_v32 y);
umbra_color umbra_shader_linear_grad(struct umbra_basic_block*, umbra_v32 x, umbra_v32 y);
umbra_color umbra_shader_radial_grad(struct umbra_basic_block*, umbra_v32 x, umbra_v32 y);

// Build an evenly-spaced gradient LUT from N color stops.
// out must hold lut_n * 4 __fp16 values (RGBA interleaved).
void umbra_gradient_lut_even(__fp16 *out, int lut_n,
                             int n_stops, __fp16 const colors[][4]);

// Build a gradient LUT from N color stops at arbitrary positions.
// positions[i] in [0,1], must be sorted ascending.
void umbra_gradient_lut(__fp16 *out, int lut_n,
                        int n_stops, float const positions[],
                        __fp16 const colors[][4]);

// Coverage
umbra_half umbra_coverage_rect   (struct umbra_basic_block*, umbra_v32 x, umbra_v32 y);
umbra_half umbra_coverage_bitmap (struct umbra_basic_block*, umbra_v32 x, umbra_v32 y);
umbra_half umbra_coverage_sdf           (struct umbra_basic_block*, umbra_v32 x, umbra_v32 y);
umbra_half umbra_coverage_bitmap_matrix(struct umbra_basic_block*, umbra_v32 x, umbra_v32 y);

// Blend modes
umbra_color umbra_blend_src     (struct umbra_basic_block*, umbra_color src, umbra_color dst);
umbra_color umbra_blend_srcover (struct umbra_basic_block*, umbra_color src, umbra_color dst);
umbra_color umbra_blend_dstover (struct umbra_basic_block*, umbra_color src, umbra_color dst);
umbra_color umbra_blend_multiply(struct umbra_basic_block*, umbra_color src, umbra_color dst);

// Pixel formats
umbra_color umbra_load_8888 (struct umbra_basic_block*, umbra_ptr ptr, umbra_v32 ix);
void        umbra_store_8888(struct umbra_basic_block*, umbra_ptr ptr, umbra_v32 ix, umbra_color);
umbra_color umbra_load_fp16 (struct umbra_basic_block*, umbra_ptr ptr, umbra_v32 ix);
void        umbra_store_fp16(struct umbra_basic_block*, umbra_ptr ptr, umbra_v32 ix, umbra_color);
