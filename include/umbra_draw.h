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

typedef struct {
    umbra_val32 sx, kx, tx,
                ky, sy, ty,
                p0, p1, p2;
} umbra_matrix_val32;

// Apply a 3x3 perspective matrix (with operands already loaded as umbra_val32s)
// to (x, y).  Emits w = p0*x + p1*y + p2; x' = (sx*x + kx*y + tx) / w;
// y' = (ky*x + sy*y + ty) / w.  Shared between umbra_transform_perspective
// (binds the matrix as uniforms for the caller) and internal impls that load
// the operands differently (e.g. slug's build functions which read them out
// of a larger uniform block).
umbra_point_val32 umbra_apply_matrix(struct umbra_builder*, umbra_matrix_val32,
                                      umbra_val32 x, umbra_val32 y);

// Compose two 3x3 matrices: out = a * b, such that applying `out` to a point
// is equivalent to applying b first, then a.  Useful for folding a caller's
// cell/viewport transform into a slide's intrinsic transform at build time.
void umbra_matrix_mul(struct umbra_matrix *out,
                      struct umbra_matrix const *a,
                      struct umbra_matrix const *b);

struct umbra_bitmap {
    struct umbra_buf buf;
    int              w,h;
};

struct umbra_fmt {
    char const *name;
    size_t      bpp;
    int         planes, :32;
    umbra_color_val32 (*load) (struct umbra_builder*, umbra_ptr);
    void              (*store)(struct umbra_builder*, umbra_ptr, umbra_color_val32);
};
extern struct umbra_fmt const umbra_fmt_8888,
                              umbra_fmt_565,
                              umbra_fmt_1010102,
                              umbra_fmt_fp16,
                              umbra_fmt_fp16_planar;

umbra_color_val32 umbra_load_8888        (struct umbra_builder*, umbra_ptr);
void              umbra_store_8888       (struct umbra_builder*, umbra_ptr, umbra_color_val32);
umbra_color_val32 umbra_load_565         (struct umbra_builder*, umbra_ptr);
void              umbra_store_565        (struct umbra_builder*, umbra_ptr, umbra_color_val32);
umbra_color_val32 umbra_load_1010102     (struct umbra_builder*, umbra_ptr);
void              umbra_store_1010102    (struct umbra_builder*, umbra_ptr, umbra_color_val32);
umbra_color_val32 umbra_load_fp16        (struct umbra_builder*, umbra_ptr);
void              umbra_store_fp16       (struct umbra_builder*, umbra_ptr, umbra_color_val32);
umbra_color_val32 umbra_load_fp16_planar (struct umbra_builder*, umbra_ptr);
void              umbra_store_fp16_planar(struct umbra_builder*, umbra_ptr, umbra_color_val32);

// Effects emit IR into a builder.  Any uniforms registered with
// umbra_bind_uniforms() must outlive the builder, flat_ir, and program.  These are
// function types (not pointer types), so declarations read as
// `umbra_blend umbra_blend_src, umbra_blend_srcover, ...;` and struct fields
// carrying them write as `umbra_shader *inner_fn`.
typedef umbra_color_val32 umbra_shader   (void *ctx, struct umbra_builder*,
                                          umbra_val32 x, umbra_val32 y);
typedef umbra_val32       umbra_coverage (void *ctx, struct umbra_builder*,
                                          umbra_val32 x, umbra_val32 y);
typedef umbra_interval    umbra_sdf      (void *ctx, struct umbra_builder*,
                                          umbra_interval x, umbra_interval y);
typedef umbra_color_val32 umbra_blend    (void *ctx, struct umbra_builder*,
                                          umbra_color_val32 src,
                                          umbra_color_val32 dst);

// Perspective transform: the matrix's 9 floats are re-read each dispatch
// through a bound uniforms span, then applied as
// w = p0*x + p1*y + p2;  x' = (sx*x + kx*y + tx) / w;  y' = (ky*x + sy*y + ty) / w.
// Operates on scalar dispatch coords; callers that need to chain another
// transform just call this again on the result.  (There is no interval
// entry point: tile-culled dispatch is affine-only by design.)
umbra_point_val32 umbra_transform_perspective(struct umbra_matrix const*,
                                              struct umbra_builder*,
                                              umbra_val32 x, umbra_val32 y);

// Shade a single color; ctx is an umbra_color*.
umbra_color_val32 umbra_shader_color(void *umbra_color, struct umbra_builder*,
                                     umbra_val32 x, umbra_val32 y);

umbra_blend umbra_blend_src,
            umbra_blend_srcover,
            umbra_blend_dstover,
            umbra_blend_multiply;

// Write a draw pipeline into an existing builder: load dst (if needed),
// run coverage + shader + blend at the supplied (x, y), compose with dst,
// store back to dst.  Any of the effects may be NULL for default behavior:
// coverage=1, shader={0,0,0,0}, blend=src.  Callers can apply any transform
// they want by pre-processing (x, y) themselves (e.g. via
// umbra_transform_perspective) and chaining as they please.  dst_ptr must
// already be bound on the builder via umbra_bind_buf; the backing buf
// must outlive the builder, flat_ir, and program.
void umbra_build_draw(struct umbra_builder*,
                      umbra_ptr dst_ptr, struct umbra_fmt dst_fmt,
                      umbra_val32 x, umbra_val32 y,
                      umbra_coverage, void *coverage_ctx,
                      umbra_shader  , void *shader_ctx,
                      umbra_blend   , void *blend_ctx);

// TODO: remove umbra_draw_builder.  It is a thin convenience wrapper around
// umbra_build_draw that survives only so legacy call sites (slide code that
// hasn't migrated to build_draw yet, plus umbra_sdf_draw's internal draw
// program) don't all have to be rewritten at once.  Steady state is every
// caller does it themselves: umbra_builder() + umbra_bind_buf(dst) +
// umbra_f32_from_i32 of umbra_x / umbra_y + optional
// umbra_transform_perspective + umbra_build_draw -- exactly the same four
// lines this wrapper inlines.  Delete this prototype and src/draw.c's
// definition once the last caller is gone.
//
// Current callers to migrate first:
//   * slides/color.c, slides/slug.c, slides/sdf.c, slides/blend.c,
//     slides/slides.c bg program, slides/overview.c overlay program (each
//     will typically migrate alongside its own slide->build_draw conversion)
//   * tests/draw_test.c draw_builder_shim (every TEST using the shim)
//   * tests/golden_test.c
//   * src/draw.c inside umbra_sdf_draw (easy -- open-codes the same four
//     lines).
// Creates a fresh builder, initializes (x, y) from the dispatch, applies
// umbra_transform_perspective through transform_mat (if non-NULL), and
// dispatches to umbra_build_draw.
struct umbra_builder* umbra_draw_builder(struct umbra_matrix const *transform_mat,
                                         umbra_coverage , void *coverage_ctx,
                                         umbra_shader   , void *shader_ctx,
                                         umbra_blend    , void *blend_ctx,
                                         struct umbra_buf *dst,
                                         struct umbra_fmt  dst_fmt);

// Draw using an umbra_sdf as coverage.  hard_edge=1 gives a binary mask;
// hard_edge=0 clamps -sdf into [0, 1] for a 1px AA ramp.
//
// If transform is non-NULL, its matrix is applied before the sdf (draw and
// bounds programs both see transformed coordinates).  Affine matrices at
// build time (p0 == p1 == 0 && p2 == 1) keep the tile-culling bounds program;
// perspective matrices skip bounds and fall back to a single full-rect
// dispatch -- tile culling is affine-only by design.  This is a build-time
// decision: if you need an affine-gated sdf_draw, keep the perspective row
// zero for the lifetime of the program.
// TODO: _Bool hard_edge -> int quality
struct umbra_sdf_draw* umbra_sdf_draw(struct umbra_backend*,
                                      struct umbra_matrix const *transform_mat,
                                      umbra_sdf, void *sdf_ctx,
                                      _Bool hard_edge,
                                      umbra_shader, void *shader_ctx,
                                      umbra_blend,  void *blend_ctx,
                                      struct umbra_fmt);
void umbra_sdf_draw_queue(struct umbra_sdf_draw*, int l, int t, int r, int b,
                          struct umbra_buf dst);
void umbra_sdf_draw_free(struct umbra_sdf_draw*);


// Adapt an umbra_sdf as umbra_coverage.
struct umbra_coverage_from_sdf {
    umbra_sdf *sdf_fn;
    void      *sdf_ctx;
    int        hard_edge, :32;
};
umbra_val32 umbra_coverage_from_sdf(void *ctx, struct umbra_builder*,
                                     umbra_val32 x, umbra_val32 y);

// Supersample a wrapped shader.
struct umbra_supersample {
    umbra_shader *inner_fn;
    void         *inner_ctx;
    int           samples, :32;
};
umbra_color_val32 umbra_shader_supersample(void *umbra_supersample, struct umbra_builder*,
                                            umbra_val32 x, umbra_val32 y);

// Cover a rectangle, ctx is an umbra_rect*.
umbra_val32 umbra_coverage_rect(void *umbra_rect, struct umbra_builder*,
                                 umbra_val32 x, umbra_val32 y);

// A rectangle SDF, ctx is an umbra_rect*.
umbra_interval umbra_sdf_rect(void *umbra_rect, struct umbra_builder*,
                              umbra_interval x, umbra_interval y);

