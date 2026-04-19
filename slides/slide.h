#pragma once
#include "../include/umbra_draw.h"
#include <stdint.h>

// The endgame for this interface is for drivers (bench, demo, dump, golden
// tests, overview) to stop knowing anything about slide internals and just
// drive the slide through .build_draw + .animate.  Each driver creates a
// builder, binds its chosen dst, initializes (x, y) from the dispatch,
// optionally pre-applies a viewport transform, calls build_draw, and takes
// it from there (compile, dispatch, dump, time, pairwise-compare).
//
// Partially landed:
//   * build_draw exists and lives alongside prepare/draw/get_builders.
//   * persp_slide, cov_null_slide, and all 8 gradient variants implement it.
//   * The overview consumes it.
//
// Still ahead:
//   * Migrate the remaining slides.  Today-blockers: text_slide (golden-test
//     disagreement the move surfaced -- see TODO in slides/coverage.c);
//     sdf / blend slides (need a tile-culled sibling path -- see TODO in
//     slides/overview.c); slug (two-pass, needs multi-program support);
//     swatch (multi-dispatch per frame).
//   * Extend build_draw (or add a sibling) to let a slide produce more than
//     one program so the two-pass slug accumulator fits.  The driver would
//     compile each in sequence and dispatch them in order.
//   * Once every slide has a build_draw (or equivalent), retire prepare /
//     draw / get_builders and have every driver just loop over
//     slide->build_draw.

struct slide {
    char const     *title;
    float           bg[4];

    void (*init)   (struct slide*, int w, int h);
    void (*prepare)(struct slide*, struct umbra_backend*, struct umbra_fmt);
    void (*draw)   (struct slide*, double secs, int l, int t, int r, int b, void *buf);
    void (*free)   (struct slide*);

    int (*get_builders)(struct slide*, struct umbra_fmt,
                        struct umbra_builder **out, int max);

    // Fill builder `b` with the slide's draw IR.  `dst_ptr` is already bound
    // on `b` (via umbra_bind_buf32) and is the final destination; `fmt` is
    // its format.  `(x, y)` are the post-transform dispatch coords -- if the
    // caller wanted a viewport transform applied, they've already issued it.
    // The slide may still issue its own transforms (e.g. an animated
    // perspective matrix) on top before calling umbra_build_draw.
    //
    // NULL means the slide has no composable draw path; consumers fall back
    // to the slide's own prepare/draw cycle or a placeholder.
    void (*build_draw)(struct slide*, struct umbra_builder *b,
                       umbra_ptr32 dst_ptr, struct umbra_fmt fmt,
                       umbra_val32 x, umbra_val32 y);

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
