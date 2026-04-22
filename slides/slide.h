#pragma once
#include "../include/umbra_draw.h"
#include <stdint.h>

union transform {
    struct umbra_matrix persp;
    struct umbra_affine affine;
};

struct slide {
    char const     *title;
    umbra_color     bg;
    int             w,h;

    void (*init)   (struct slide*);
    void (*prepare)(struct slide*, struct umbra_backend*, struct umbra_fmt);
    void (*draw)   (struct slide*, double secs, int l, int t, int r, int b, struct umbra_buf dst);
    void (*free)   (struct slide*);

    void (*build_draw)(struct slide*, struct umbra_builder *b,
                       umbra_ptr dst_ptr, struct umbra_fmt fmt,
                       umbra_val32 x, umbra_val32 y);

    umbra_sdf *sdf_fn;
    void      *sdf_ctx;
    void (*build_sdf_draw)(struct slide*, struct umbra_builder *b,
                           umbra_ptr dst_ptr, struct umbra_fmt fmt,
                           umbra_val32 x, umbra_val32 y);
    // Required alongside build_sdf_draw.  Same shader/blend as build_sdf_draw
    // but with coverage hardcoded to 1 -- used for tiles known to be entirely
    // inside the shape so the per-pixel SDF eval is skipped.
    void (*build_draw_full)(struct slide*, struct umbra_builder *b,
                            umbra_ptr dst_ptr, struct umbra_fmt fmt,
                            umbra_val32 x, umbra_val32 y);

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

struct slide_bg* slide_bg     (struct umbra_backend*, struct umbra_fmt);
void             slide_bg_draw(struct slide_bg*, umbra_color,
                               int l, int t, int r, int b, struct umbra_buf dst);
void             slide_bg_free(struct slide_bg*);

struct slide_runtime {
    struct umbra_program            *draw;
    struct umbra_program            *draw_full; // NULL iff bounds == NULL (non-SDF)
    struct umbra_sdf_bounds_program *bounds;    // NULL for non-SDF

    struct umbra_fmt                 fmt;
    int                              w, h;
    umbra_ptr                        dst_ptr;   // late-bound dst for draw/draw_full
    int :32;
};

struct slide_runtime* slide_runtime(struct slide*, int w, int h,
                                    struct umbra_backend*, struct umbra_fmt,
                                    union transform const *pre_transform);
void   slide_runtime_animate(struct slide*, double secs);
void   slide_runtime_draw   (struct slide_runtime*, struct umbra_buf dst,
                             int l, int t, int r, int b);
void   slide_runtime_free   (struct slide_runtime*);

// Build a fresh draw-side builder for a slide, without compiling.  Returns
// NULL for slides that don't draw anything (no build_draw / build_sdf_draw).
// The dst is late-bound; `out_dst_ptr` (nullable) receives the handle the
// caller needs to supply at queue() time.
struct umbra_builder* slide_draw_builder(struct slide*,
                                         umbra_ptr *out_dst_ptr,
                                         struct umbra_fmt,
                                         union transform const *pre);
