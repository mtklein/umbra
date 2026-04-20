#pragma once
#include "../include/umbra_draw.h"
#include <stdint.h>

// A point transform viewed either as its full 3x3 perspective form (for the
// draw-side umbra_transform_perspective call) or as its affine prefix (for the
// bounds-side umbra_sdf_bounds_program call).  Per C11 6.5.2.3 common-initial-
// sequence, writing through one member lets readers access the shared prefix
// through the other wherever this declaration is visible.
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

    // SDF-backed slides set sdf_fn/sdf_ctx and implement build_sdf_draw to
    // emit the draw IR (slide_runtime builds the bounds program separately
    // from sdf_fn/sdf_ctx via umbra_sdf_bounds()).
    umbra_sdf *sdf_fn;
    void      *sdf_ctx;
    void (*build_sdf_draw)(struct slide*, struct umbra_builder *b,
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
    struct umbra_sdf_bounds_program *bounds;   // NULL for non-SDF

    struct umbra_fmt                 fmt;
    int                              w, h;
    struct umbra_buf                 dst_buf;
};

struct slide_runtime* slide_runtime(struct slide*, int w, int h,
                                    struct umbra_backend*, struct umbra_fmt,
                                    union transform const *pre_transform);
void   slide_runtime_draw(struct slide_runtime*, struct slide*,
                          double secs, int l, int t, int r, int b);
void   slide_runtime_free(struct slide_runtime*);

// Build a fresh draw-side builder for a slide, without compiling.  Returns
// NULL for slides that don't draw anything (no build_draw / build_sdf_draw).
// `dst` is a stable umbra_buf whose address the returned builder binds as
// the dst; keep it alive until any IR / program derived from this builder
// is freed.
struct umbra_builder* slide_draw_builder(struct slide*,
                                          struct umbra_buf *dst,
                                          struct umbra_fmt,
                                          union transform const *pre);
