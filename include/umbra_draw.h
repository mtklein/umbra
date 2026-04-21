#pragma once
#include "umbra.h"
#include "umbra_interval.h"

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
struct umbra_affine {
    float sx, kx, tx,
          ky, sy, ty;
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

// Like umbra_build_draw() but using an SDF for coverage.  Coverage is the
// fraction of the pixel-box interval [x,x+1]x[y,y+1] that the SDF reports
// as inside: cov = -lo / (hi - lo), with the fully-inside (hi <= 0) and
// fully-outside (lo >= 0) cases routed around the division.
void umbra_build_sdf_draw(struct umbra_builder*,
                          umbra_ptr dst_ptr, struct umbra_fmt dst_fmt,
                          umbra_val32 x, umbra_val32 y,
                          umbra_sdf   , void *sdf_ctx,
                          umbra_shader, void *shader_ctx,
                          umbra_blend , void *blend_ctx);

// Use an SDF bounds program to intelligently dispatch draw->queue() calls for a
// draw program built by umbra_build_sdf_draw() from the same SDF, skipping
// uncovered rectangles.  Optional affine coordinate transform.
// TODO: use a draw that skips per-pixel SDF eval when TILE_FULL.
struct umbra_sdf_bounds_program* umbra_sdf_bounds_program(struct umbra_builder*,
                                                          struct umbra_affine const *transform,
                                                          umbra_sdf, void *sdf_ctx);
void   umbra_sdf_bounds_program_free(struct umbra_sdf_bounds_program*);
void   umbra_sdf_dispatch(struct umbra_sdf_bounds_program *bounds,
                          struct umbra_program            *draw,
                          int l, int t, int r, int b);
