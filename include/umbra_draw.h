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
void umbra_build_sdf_draw(struct umbra_builder*,
                          umbra_ptr dst_ptr, struct umbra_fmt dst_fmt,
                          umbra_val32 x, umbra_val32 y,
                          umbra_sdf   , void *sdf_ctx, int aa_quality,
                          umbra_shader, void *shader_ctx,
                          umbra_blend , void *blend_ctx);

// Tile coverage class stored by umbra_build_sdf_bounds (one uint16 per tile).
// Ordered so `cov > 0` means "draw the tile" and `cov == FULL` means "skip the
// per-pixel SDF eval fast path (TODO)."
enum {
    UMBRA_SDF_TILE_NONE    = 0,  // f.lo >= 0: tile entirely outside
    UMBRA_SDF_TILE_PARTIAL = 1,  // f.lo <  0, f.hi >= 0: edge tile, needs the full draw program
    UMBRA_SDF_TILE_FULL    = 2,  // f.hi <  0: tile entirely inside
};

// Evaluate SDF f(x,y) and store a uint16 umbra_sdf_tile classification to cov.
void umbra_build_sdf_bounds(struct umbra_builder*,
                            umbra_ptr cov,
                            umbra_interval x, umbra_interval y,
                            umbra_sdf, void *sdf_ctx);

// TODO: remove umbra_draw_builder.  A thin convenience wrapper around
// umbra_build_draw: umbra_builder() + umbra_bind_buf(dst) + umbra_f32_from_i32
// of umbra_x / umbra_y + optional umbra_transform_perspective +
// umbra_build_draw -- exactly the four lines this wrapper inlines.  Only
// remaining callers are the SDF path (slides/sdf.c via umbra_coverage_from_sdf,
// and src/draw.c inside umbra_sdf_draw); both go away with the SDF rework,
// and this prototype + definition go with them.
//
// Creates a fresh builder, initializes (x, y) from the dispatch, applies
// umbra_transform_perspective through transform_mat (if non-NULL), and
// dispatches to umbra_build_draw.
struct umbra_builder* umbra_draw_builder(struct umbra_matrix const *transform_mat,
                                         umbra_coverage , void *coverage_ctx,
                                         umbra_shader   , void *shader_ctx,
                                         umbra_blend    , void *blend_ctx,
                                         struct umbra_buf *dst,
                                         struct umbra_fmt  dst_fmt);


// Drive a tile-culled SDF dispatch.  The dispatcher owns a caller-supplied
// compiled draw program (built via umbra_build_sdf_draw), builds+compiles
// its own bounds program from the same sdf (via umbra_build_sdf_bounds +
// tile-extent intervals), and at queue() iterates tile_size x tile_size
// tiles: bounds classifies each tile and the draw program fires on every
// non-NONE tile.  transform_mat matches the transform the caller applied
// in the draw IR (so bounds sees the same coordinate space); NULL or
// affine keeps tile culling, perspective falls back to full-rect dispatch.
// Whatever dst_buf the draw program was bound to at build time is where
// it draws -- update that buf before each queue() as usual.
struct umbra_sdf_dispatcher* umbra_sdf_dispatcher(
        umbra_sdf, void *sdf_ctx,
        struct umbra_matrix const *transform_mat,
        struct umbra_program *draw,            // takes ownership
        int tile_size);
void umbra_sdf_dispatcher_queue(struct umbra_sdf_dispatcher*, int l, int t, int r, int b);
void umbra_sdf_dispatcher_free (struct umbra_sdf_dispatcher*);

// TODO: remove.  Convenience wrapper around umbra_sdf_dispatcher that also
// builds the draw program, binding a dst slot inside the wrapper so
// callers can swap dst per queue.  Callers should migrate to building the
// draw program themselves (via umbra_build_sdf_draw on their own dst slot)
// and creating a dispatcher around it.
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
//
// TRAP: evaluates the SDF per pixel with no tile culling.  That throws away
// the whole point of SDF coverage -- umbra_sdf_draw exists specifically to
// build a separate interval bounds program that culls empty tiles before the
// draw program touches them.  Only used today because slides/sdf.c still
// goes through umbra_draw_builder; both will be replaced by a build_sdf_draw
// contract that keeps the bounds-program pair intact.
struct umbra_coverage_from_sdf {
    umbra_sdf *sdf_fn;
    void      *sdf_ctx;
    int        hard_edge, :32;
};
umbra_val32 umbra_coverage_from_sdf(void *ctx, struct umbra_builder*,
                                     umbra_val32 x, umbra_val32 y);

