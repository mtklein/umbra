#pragma once
#include "../include/umbra_draw.h"
#include <math.h>
#include <stdint.h>

typedef void *(*slide_alloc_fn)(size_t);
typedef void  (*slide_free_fn)(void *);

struct slide {
    char const     *title;
    uint32_t        bg, :32;

    slide_alloc_fn  alloc;
    slide_free_fn   sfree;

    void (*init)   (struct slide*, int w, int h);
    void (*animate)(struct slide*, float dt);
    void (*prepare)(struct slide*, struct umbra_backend*, struct umbra_fmt);
    void (*draw)   (struct slide*, int l, int t, int r, int b, void *buf);
    void (*free)   (struct slide*);

    struct umbra_builder *(*get_builder)(struct slide*, struct umbra_fmt);
};

int           slide_count        (void);
struct slide *slide_get          (int i);
void          slides_init        (int w, int h, slide_alloc_fn, slide_free_fn);
void          slides_init_for_dump(void);
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
