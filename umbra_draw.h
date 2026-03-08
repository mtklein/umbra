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
umbra_color umbra_shader_solid(struct umbra_basic_block*, umbra_v32 x, umbra_v32 y);

// Coverage
// (none yet — full coverage by passing NULL)

// Blend modes
umbra_color umbra_blend_src    (struct umbra_basic_block*, umbra_color src, umbra_color dst);
umbra_color umbra_blend_srcover(struct umbra_basic_block*, umbra_color src, umbra_color dst);

// Pixel formats
umbra_color umbra_load_8888 (struct umbra_basic_block*, umbra_ptr ptr, umbra_v32 ix);
void        umbra_store_8888(struct umbra_basic_block*, umbra_ptr ptr, umbra_v32 ix, umbra_color);
umbra_color umbra_load_fp16 (struct umbra_basic_block*, umbra_ptr ptr, umbra_v32 ix);
void        umbra_store_fp16(struct umbra_basic_block*, umbra_ptr ptr, umbra_v32 ix, umbra_color);
