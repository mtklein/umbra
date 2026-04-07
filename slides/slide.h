#pragma once
#include "../include/umbra_draw.h"
#include <math.h>
#include <stdint.h>

struct slide {
    char const     *title;
    uint32_t        bg, :32;

    void (*init)   (struct slide*, int w, int h);
    void (*animate)(struct slide*, float dt);
    void (*prepare)(struct slide*, struct umbra_backend*, struct umbra_fmt);
    void (*draw)   (struct slide*, int l, int t, int r, int b, void *buf);
    void (*free)   (struct slide*);

    struct umbra_builder *(*get_builder)(struct slide*, struct umbra_fmt);
};

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

static inline void slide_perspective_matrix(
        float out[11], float t,
        int sw, int sh, int bw, int bh) {
    float cx = (float)sw * 0.5f;
    float cy = (float)sh * 0.5f;
    float bx = (float)bw * 0.5f;
    float by = (float)bh * 0.5f;
    float angle = t * 0.3f;
    float tilt  = sinf(t * 0.7f) * 0.0008f;
    float sc    = 1.0f + 0.2f * sinf(t * 0.5f);
    float ca = cosf(angle), sa = sinf(angle);
    float w0 = 1.0f - tilt * cx;
    out[0] = ca * sc;     out[1] = sa * sc;
    out[2] = -cx*ca*sc - cy*sa*sc + bx*w0;
    out[3] = -sa * sc;    out[4] = ca * sc;
    out[5] = cx*sa*sc - cy*ca*sc + by*w0;
    out[6] = tilt;  out[7] = 0.0f;  out[8] = w0;
    out[9] = (float)bw; out[10] = (float)bh;
}
