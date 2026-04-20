#pragma once
#include "umbra.h"
#include "umbra_interval.h"
#include <stdint.h>

typedef struct { float       x, y; } umbra_point;
typedef struct { umbra_val32 x, y; } umbra_point_val32;

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

typedef umbra_color_val32 umbra_load(struct umbra_builder*, umbra_ptr);
umbra_load umbra_load_8888,
           umbra_load_565,
           umbra_load_1010102,
           umbra_load_fp16,
           umbra_load_fp16_planar;

typedef void umbra_store(struct umbra_builder*, umbra_ptr, umbra_color_val32);
umbra_store umbra_store_8888,
            umbra_store_565,
            umbra_store_1010102,
            umbra_store_fp16,
            umbra_store_fp16_planar;

struct umbra_fmt {
    char const  *name;
    size_t       bpp;
    int          planes, :32;
    umbra_load  *load;
    umbra_store *store;
};
extern struct umbra_fmt const umbra_fmt_8888,
                              umbra_fmt_565,
                              umbra_fmt_1010102,
                              umbra_fmt_fp16,
                              umbra_fmt_fp16_planar;

// 3x3 matrix transform with perspective divide.
umbra_point_val32 umbra_transform_perspective(struct umbra_matrix const*,
                                              struct umbra_builder*,
                                              umbra_val32 x, umbra_val32 y);

typedef umbra_val32 umbra_coverage(void *ctx, struct umbra_builder*,
                                   umbra_val32 x, umbra_val32 y);
// Cover a rectangle; ctx is umbra_rect*.
umbra_coverage umbra_coverage_rect;

typedef umbra_color_val32 umbra_shader(void *ctx, struct umbra_builder*,
                                       umbra_val32 x, umbra_val32 y);
// Shade a single color; ctx is umbra_color*.
umbra_shader umbra_shader_color;

// Supersample a wrapped shader; ctx is struct umbra_supersample*.
struct umbra_supersample {
    umbra_shader *inner_fn;
    void         *inner_ctx;
    int           samples, :32;
};
umbra_shader umbra_shader_supersample;

typedef umbra_color_val32 umbra_blend(void *ctx, struct umbra_builder*,
                                      umbra_color_val32 src,
                                      umbra_color_val32 dst);
// ctx=NULL for all these blends.
umbra_blend umbra_blend_src,
            umbra_blend_srcover,
            umbra_blend_dstover,
            umbra_blend_multiply;

// Write a draw pipeline into an existing builder:
//   cov = coverage(x,y)
//   src = shader(x,y)
//   dst = fmt.load()
//   out = blend(src,dst)
//   fmt.store(lerp(cov, dst,out))
// NULL effects provide default behavior: coverage=1, shader={0,0,0,0}, blend=src.
// dst_ptr must already be bound on the builder via umbra_bind_buf().
void umbra_build_draw(struct umbra_builder*,
                      umbra_ptr dst_ptr, struct umbra_fmt dst_fmt,
                      umbra_val32 x, umbra_val32 y,
                      umbra_coverage, void *coverage_ctx,
                      umbra_shader  , void *shader_ctx,
                      umbra_blend   , void *blend_ctx);

// SDF where f(x,y)<0 -> inside, defined over intervals.
typedef umbra_interval umbra_sdf(void *ctx, struct umbra_builder*,
                                 umbra_interval x, umbra_interval y);

// Like umbra_build_draw() but using an SDF for coverage.
// Set aa_quality=0 for a hard edge, or >0 for increasing effort spent on AA quality.
// TODO: remove hard edge option, always use cov ≈ -lo / (hi-lo)
void umbra_build_sdf_draw(struct umbra_builder*,
                          umbra_ptr dst_ptr, struct umbra_fmt dst_fmt,
                          umbra_val32 x, umbra_val32 y,
                          umbra_sdf   , void *sdf_ctx, int aa_quality,
                          umbra_shader, void *shader_ctx,
                          umbra_blend , void *blend_ctx);

// A bounds program plus the storage it needs between dispatches: the grid
// uniforms it was built against, the cov buffer it writes tile classifications
// into, and our running capacity for that buffer.  Pass the bounds struct to
// umbra_build_sdf_bounds(), compile the resulting IR, stash the program in
// bounds.prog, then call umbra_sdf_dispatch().
struct umbra_sdf_bounds {
    struct umbra_program *prog;
    struct umbra_buf      cov_buf;
    uint16_t             *cov;
    int                   cov_cap, :32;
    float                 base_x,base_y,
                          tile_w,tile_h;
};

// Build an SDF bounds IR that classifies each tile at (umbra_x, umbra_y) into
// the uint16 TILE_NONE / TILE_PARTIAL / TILE_FULL enum and writes it to
// bounds->cov_buf.  Binds bounds->cov_buf (storage) and bounds->grid (uniforms)
// on the builder.  `transform`, if non-NULL, composes an affine pre-transform
// onto the per-tile intervals.
void umbra_build_sdf_bounds(struct umbra_builder*,
                            struct umbra_sdf_bounds *bounds,
                            struct umbra_matrix const *transform,
                            umbra_sdf, void *sdf_ctx);

// Frees bounds->prog and bounds->cov.  Does not free the struct itself —
// callers usually embed one.
void umbra_sdf_bounds_free(struct umbra_sdf_bounds*);

// Use the bounds program's coverage results to intelligently dispatch
// draw->queue() calls, skipping any uncovered rectangles.
// TODO: use a draw that skips per-pixel SDF eval when TILE_FULL.
void umbra_sdf_dispatch(struct umbra_sdf_bounds *bounds,
                        struct umbra_program    *draw,
                        int l, int t, int r, int b);
