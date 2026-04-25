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

// 2x3 affine transform: no perspective divide.
umbra_point_val32 umbra_transform_affine(struct umbra_affine const*,
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
// dst_ptr must already be bound on the builder (early or late).
void umbra_build_draw(struct umbra_builder*,
                      umbra_ptr dst_ptr, struct umbra_fmt dst_fmt,
                      umbra_val32 x, umbra_val32 y,
                      umbra_coverage, void *coverage_ctx,
                      umbra_shader  , void *shader_ctx,
                      umbra_blend   , void *blend_ctx);

// SDF where f(x,y)<0 -> inside, defined over intervals.
typedef umbra_interval umbra_sdf(void *ctx, struct umbra_builder*,
                                 umbra_interval x, umbra_interval y);

// Wrap an SDF with |sdf| - width, stroking the zero crossing of the inner
// SDF with total thickness 2*width.  Apply twice to stroke a stroke.
struct umbra_sdf_stroke {
    umbra_sdf *inner_fn;
    void      *inner_ctx;
    float      width;
    int       :32;
};
umbra_sdf umbra_sdf_stroke;

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

// TODO: move SDF bounds-eval onto the draw backend so the host CPU isn't the
//       wall on heavy-bounds workloads.  Profiling SDF Text Analytic on metal
//       shows ~all main-thread CPU is the JIT bounds eval today.
//
//   The plan: shader-side cull, no readback, one submit per frame.
//
//   Bounds shader writes cov[] as it does today.  draw_partial / draw_full
//   each gain a per-tile early-exit at the top: read cov[tile_idx], skip the
//   body if not my tile kind.  Host queues bounds + both draws over the full
//   (l, t, r, b) into one cmdbuf and submits once -- zero mid-frame sync,
//   zero cov readback.  Backends already insert a barrier between bounds
//   (writes cov) and draws (read cov) via dispatch_overlap_check.
//
//   On GPU backends the per-tile early-exit is essentially free: idle threads
//   that hit a return on the first instruction cost almost nothing.  We give
//   up the geometric "don't even encode dispatches for NONE rectangles"
//   optimization, but on a GPU that cost is one branch per thread, not a CPU
//   encode.  This is what Vello does: it dispatches over every tile and
//   early-exits empty ones rather than compacting a worklist.
//
//   Tried (commit 8f1910cf, reverted in 507742b6): compile bounds on the
//   caller-supplied backend, queue+flush mid-dispatch, then read cov[] back
//   to drive the per-tile draws on the host.  Big win for heavy bounds (SDF
//   Text Analytic on metal: 2.53 -> 1.81 ns/px @ 40% -> 5% CPU), but the
//   added per-frame GPU sync tanked simple SDFs (Union: .22 -> .11 ns/px but
//   72% CPU; wgpu went .22 -> 2.82 ns/px @ 99% CPU because it has no zero-
//   copy buffer transfer and pays a full staging-buffer round-trip per frame
//   for the cov download).  The mid-dispatch flush is the cost; readback is
//   the second cost on backends without zero-copy.  Both costs vanish under
//   the shader-side-cull plan.
//
//   What's needed to land it:
//     - A builder helper to wrap a draw body in the cov-check (uses if/endif).
//     - Make if/endif actually skip its body on CPU backends (interp + JIT)
//       when the condition is uniform across the lane batch.  Today CPU
//       backends lower if/endif as a per-lane mask that only gates store_var
//       (not memory stores), so a draw body with stores inside an "if cov ==
//       me" would write garbage on CPU.  GPU backends already lower if as
//       real control flow.  The IR already tracks per-instruction uniform-
//       ness; CPU backends can branch on that to pick masking vs real branch.
//     - Wire dispatch to enqueue bounds + both draws over (l, t, r, b) into
//       the same cmdbuf instead of cov-readback + per-tile draws.

// Use an SDF bounds program to intelligently dispatch draw->queue() calls for a
// draw program built by umbra_build_sdf_draw() from the same SDF, skipping
// uncovered rectangles.  Optional affine coordinate transform.
struct umbra_sdf_bounds_program* umbra_sdf_bounds_program(struct umbra_builder*,
                                                          struct umbra_affine const *transform,
                                                          umbra_sdf, void *sdf_ctx);
void   umbra_sdf_bounds_program_free(struct umbra_sdf_bounds_program*);
// `draw_partial` runs on tiles the bounds classifier marks as containing the
// shape edge; `draw_full` runs on tiles entirely inside the shape, where
// coverage=1 is known and the SDF eval can be skipped.  Passing the same
// program for both is legal, just forfeits the FULL-tile speedup.
// Late bindings are forwarded to each internal draw->queue() call; typically a
// single {dst_ptr, dst} binding so callers can supply per-dispatch dsts.
void   umbra_sdf_dispatch(struct umbra_sdf_bounds_program *bounds,
                          struct umbra_program            *draw_partial,
                          struct umbra_program            *draw_full,
                          int l, int t, int r, int b,
                          struct umbra_late_binding const[], int late_bindings);
