#pragma once
#include "../include/umbra_draw.h"

struct slug_curves {
    float *data;
    int    count;
    int    pad;
    float  w, h;
};

struct slug_acc_uniforms {
    struct umbra_matrix mat; int :32;
    float               bw, bh;
    float               j;
    int                 :32;
};

struct slug_uniforms {
    struct umbra_matrix mat; int :32;
    float               bw, bh;
    float               n_curves;
    int                 :32;
};

struct slug_curves    slug_extract  (char const *text, float font_size);
void                  slug_free     (struct slug_curves *sc);
struct umbra_builder* slug_build_acc(struct umbra_buf const *curves);
struct umbra_builder* slug_build    (struct umbra_buf const *curves);

// Coverage from winding count buffer used by 2-pass Slug, ctx is an umbra_buf*.
umbra_val32 coverage_winding(void *umbra_buf, struct umbra_builder*,
                             umbra_val32 x, umbra_val32 y);
