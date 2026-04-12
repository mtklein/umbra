#pragma once
#include "../include/umbra_draw.h"
#include <stdint.h>

// TODO: bg should probably be float bg_rgba[4] or __fp16 bg_rgba[4]
struct slide {
    char const     *title;
    uint32_t        bg, :32;

    void (*init)   (struct slide*, int w, int h);
    void (*prepare)(struct slide*, struct umbra_backend*, struct umbra_fmt);
    void (*draw)   (struct slide*, int frame, int l, int t, int r, int b, void *buf);
    void (*free)   (struct slide*);

    struct umbra_builder *(*get_builder)(struct slide*, struct umbra_fmt);
};

// TODO: slide_ctx is code smell.  why does the slide.h have to know details of particular slides?
struct text_cov;
struct slug_curves;
struct slide_ctx {
    struct text_cov    *bitmap_cov;
    struct text_cov    *sdf_cov;
    struct slug_curves *slug;
    float              *linear_lut;
    float              *radial_lut;
    int                 lut_n, :32;
};

typedef struct slide *(*slide_factory_fn)(struct slide_ctx const *);
void slide_register(slide_factory_fn factory);

#define SLIDE(NAME)                                                              \
    static struct slide *NAME(struct slide_ctx const *ctx);                      \
    _Pragma("clang diagnostic push")                                             \
    _Pragma("clang diagnostic ignored \"-Wglobal-constructors\"")                \
    __attribute__((constructor)) static void slide_ctor_##NAME(void) {           \
        slide_register(NAME);                                                    \
    }                                                                            \
    _Pragma("clang diagnostic pop")                                              \
    static struct slide *NAME(struct slide_ctx const *ctx)

int           slide_count        (void);
struct slide *slide_get          (int i);
void          slides_init        (int w, int h);
void          slides_cleanup     (void);

// TODO: also code smell... why does slide.h have to have awareness of a matrix?
void slide_perspective_matrix(float out[11], float t, int sw, int sh, int bw, int bh);
