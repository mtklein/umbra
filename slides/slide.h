#pragma once
#include "../include/umbra_draw.h"
#include <math.h>
#include <stdint.h>

typedef struct slide slide;

struct slide {
    char const        *title;
    umbra_shader_fn    shader;
    umbra_coverage_fn  coverage;
    umbra_blend_fn     blend;
    umbra_load_fn      load;
    umbra_store_fn     store;
    float              color[8];
    float              grad[4];
    uint32_t           bg;
    uint32_t           pad_;

    void (*init)   (slide*, int w, int h);
    void (*animate)(slide*, float dt);
    void (*render) (slide*, int w, int h,
                    void *buf, long buf_sz,
                    umbra_draw_layout const*,
                    struct umbra_program*);
    void (*cleanup)(slide*);
    void           *state;
};

int    slide_count        (void);
slide *slide_get          (int i);
void   slides_init        (int w, int h);
void   slides_init_for_dump(void);
void   slides_cleanup     (void);

static inline void slide_uni_i32(char *u, int off,
                                 int32_t v) {
    __builtin_memcpy(u+off, &v, 4);
}
static inline void slide_uni_f32(char *u, int off,
                                 float const *v,
                                 int n) {
    __builtin_memcpy(u+off, v, (unsigned long)n*4);
}
static inline void slide_uni_ptr(char *u, int off,
                                 void *p, long sz) {
    __builtin_memcpy(u+off,   &p,  8);
    __builtin_memcpy(u+off+8, &sz, 8);
}

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
