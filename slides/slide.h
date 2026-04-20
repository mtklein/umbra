#pragma once
#include "../include/umbra_draw.h"
#include <stdint.h>

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

    void (*build_sdf_draw)(struct slide*,

                           struct umbra_builder *b_draw,
                           umbra_ptr dst_ptr, struct umbra_fmt fmt,
                           umbra_val32 x, umbra_val32 y,

                           struct umbra_builder *b_bounds,
                           umbra_ptr cov_ptr,
                           umbra_interval ix, umbra_interval iy);

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

void slide_bg_prepare(struct umbra_backend *be, struct umbra_fmt fmt);
void slide_bg_draw   (umbra_color bg, int l, int t, int r, int b, struct umbra_buf dst);
void slide_bg_cleanup(void);

struct slide_runtime {
    struct umbra_program *draw;
    struct umbra_program *bounds;      // NULL for non-SDF; owns its backend
    uint16_t             *cov;

    struct umbra_fmt      fmt;
    int                   w, h;
    struct umbra_sdf_grid grid;
    struct umbra_buf      dst_buf;
    struct umbra_buf      cov_buf;
    int                   cov_cap;
    int                   :32;
};

struct slide_runtime* slide_runtime(struct slide*, int w, int h,
                                    struct umbra_backend*, struct umbra_fmt,
                                    struct umbra_matrix const *pre_transform);
void   slide_runtime_draw(struct slide_runtime*, struct slide*,
                          double secs, int l, int t, int r, int b);
void   slide_runtime_free(struct slide_runtime*);

struct slide_builders {
    struct umbra_builder *draw;
    struct umbra_builder *bounds;
};
struct slide_builders slide_builders(struct slide_runtime *rt,
                                     struct slide*, struct umbra_fmt, struct umbra_matrix const*);
