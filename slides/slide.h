#pragma once
#include "../include/umbra_draw.h"
#include <stdint.h>

// TODO: drive every slide through .build_draw / .build_sdf_draw + .animate
// so the drivers (bench, demo, dump, golden tests, overview) can stop
// knowing anything about slide internals.  Each driver owns a
// slide_runtime (see below), compiles a slide into it with its chosen
// backend + format + optional viewport transform, and per-frame updates
// runtime->dst_buf and calls slide_runtime_draw.  No driver should call
// slide->prepare / slide->draw / slide->get_builders.
//
// Landed:
//   * build_draw and build_sdf_draw contracts exist; every slide
//     implements one.  Non-SDF: persp, cov_null, text (bitmap + SDF
//     bitmap), 5 blend, 8 gradient, swatch, slug.  SDF: csg x3, circle,
//     ring, rounded_rect, capsule, halfplane, sdf_text, n_gon.  Slug
//     folds the winding loop into a single coverage fn; swatch gathers
//     ten colors so its grid stays one program.
//   * slide_runtime helper (below) owns compile + dispatch for either
//     flavor.  Overview uses it per cell.  sdf_common embeds it, so all
//     10 SDF slides route their prepare/draw through it.
//
// Still ahead:
//   * Migrate non-SDF slide state (prog/fmt/dst_buf in each *_slide
//     struct) to embed slide_runtime too; their _prepare / _draw
//     shrink to thin wrappers that call slide_runtime_compile / draw.
//     ~30-line win per slide; mechanical.
//   * Migrate top-level drivers (dump.c, bench.c, demo.c, golden_test.c)
//     to create their own slide_runtime and call slide_runtime_compile /
//     draw directly instead of s->prepare / s->draw.
//   * Overview is a driver for its sub-slides; its overview_prepare does
//     non-compile setup (overlay buf, cell grid) beyond slide_runtime.
//     Move that to overview_init or keep a specialized prepare -- design
//     call needed before the hook retires.
//   * Once every callsite routes through slide_runtime + build_draw /
//     build_sdf_draw directly, delete .prepare / .draw / .get_builders
//     hooks from struct slide and the thin wrappers in each slide file.

struct slide {
    char const     *title;
    float           bg[4];

    void (*init)   (struct slide*, int w, int h);
    void (*prepare)(struct slide*, struct umbra_backend*, struct umbra_fmt);
    void (*draw)   (struct slide*, double secs, int l, int t, int r, int b, void *buf);
    void (*free)   (struct slide*);

    // Fill builder `b` with the slide's draw IR.  Consumers compile the
    // result and dispatch it.
    //
    // `dst_ptr` is already bound on `b` (via umbra_bind_buf) and is the final
    // destination; `fmt` is its format.  `(x, y)` are the post-transform
    // dispatch coords -- if the caller wanted a viewport transform applied,
    // they've already issued it.  The slide may still issue its own transforms
    // (e.g. an animated perspective matrix) on top before calling
    // umbra_build_draw.
    //
    // NULL means the slide has no composable draw path; consumers fall back
    // to the slide's own prepare/draw cycle or a placeholder.
    void (*build_draw)(struct slide*, struct umbra_builder *b,
                       umbra_ptr dst_ptr, struct umbra_fmt fmt,
                       umbra_val32 x, umbra_val32 y);

    // SDF sibling of build_draw: fill b_draw with the pixel-side draw IR
    // (sdf coverage + shader + blend) and b_bounds with the tile-side
    // interval-evaluation IR.  dst_ptr and cov_ptr are already bound on
    // their respective builders.  (x, y) are the post-transform dispatch
    // coords; (ix, iy) are the matching tile-extent intervals.  The slide
    // may layer its own transforms before calling umbra_build_sdf_draw /
    // umbra_build_sdf_bounds.  Consumers compile both programs and drive
    // them via umbra_sdf_dispatch.  NULL means the slide has no SDF
    // composable path.
    void (*build_sdf_draw)(struct slide*,
                           struct umbra_builder *b_draw,
                           umbra_ptr dst_ptr, struct umbra_fmt fmt,
                           umbra_val32 x, umbra_val32 y,
                           struct umbra_builder *b_bounds,
                           umbra_ptr cov_ptr,
                           umbra_interval ix, umbra_interval iy);

    // Update animation state (e.g. per-frame matrix uniforms) without
    // emitting any GPU/CPU work.  NULL means static.
    void  (*animate)(struct slide*, double secs);
};

typedef struct slide *(*slide_factory_fn)(void);
void slide_register(slide_factory_fn factory);

#define SLIDE(NAME)                                                              \
    static struct slide* NAME(void);                                            \
    _Pragma("clang diagnostic push")                                             \
    _Pragma("clang diagnostic ignored \"-Wglobal-constructors\"")                \
    __attribute__((constructor)) static void slide_ctor_##NAME(void) {           \
        slide_register(NAME);                                                    \
    }                                                                            \
    _Pragma("clang diagnostic pop")                                              \
    static struct slide* NAME(void)

int           slide_count        (void);
struct slide* slide_get          (int i);
void          slides_init        (int w, int h);
void          slides_cleanup     (void);

void slide_perspective_matrix(struct umbra_matrix *out, float t, int sw, int sh, int bw, int bh);

void slide_bg_prepare(struct umbra_backend *be, struct umbra_fmt fmt, int w, int h);
void slide_bg_draw   (float const bg[4], int l, int t, int r, int b, void *buf);
void slide_bg_cleanup(void);

// Compiled pipeline for one slide on one backend + format, optionally with
// a pre-applied transform (for overview cells).  Lets drivers drive a slide
// entirely through its build_draw / build_sdf_draw hooks -- compile once
// and dispatch per frame -- without caring which flavor it is.
//
// Caller owns the struct.  Update `dst_buf` before each slide_runtime_draw
// to point at the current frame's output; the slide's draw program was
// bound to `&rt->dst_buf` at compile time.
struct slide_runtime {
    struct umbra_program *draw;
    struct umbra_program *bounds;      // NULL for non-SDF slides
    struct umbra_backend *bounds_be;   // owned (SDF only)
    uint16_t             *cov;

    struct umbra_fmt      fmt;
    int                   w, h;
    struct umbra_sdf_grid grid;
    struct umbra_buf      dst_buf;
    struct umbra_buf      cov_buf;
    int                   cov_cap;
    int                   :32;
};

void slide_runtime_compile(struct slide_runtime*, struct slide*,
                           int w, int h,
                           struct umbra_backend*, struct umbra_fmt,
                           struct umbra_matrix const *pre_transform);
void slide_runtime_draw   (struct slide_runtime*, struct slide*,
                           double secs, int l, int t, int r, int b);
void slide_runtime_cleanup(struct slide_runtime*);

// Build builders for the slide's composable draw path(s) for inspection.
// Uses `rt` as backing storage for bind sites -- rt must outlive any
// program compiled from the returned builders' IR.  Returns:
//   1 for build_draw slides      (out[0] = draw builder)
//   2 for build_sdf_draw slides  (out[0] = draw builder, out[1] = bounds builder)
//   0 for slides with neither    (custom slides like overview)
// Caller owns the returned builders and must umbra_builder_free each
// (typically after converting to umbra_flat_ir).
int slide_builders(struct slide_runtime *rt, struct slide*,
                   struct umbra_fmt, struct umbra_matrix const *pre,
                   struct umbra_builder **out, int max);
