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
    struct umbra_buf    curves;
    float               j;
    int                 :32;
};

struct slug_uniforms {
    struct umbra_matrix mat; int :32;
    float               bw, bh;
    struct umbra_buf    curves;
    float               n_curves;
    int                 :32;
};

struct slug_curves    slug_extract  (char const *text, float font_size);
void                  slug_free     (struct slug_curves *sc);
struct umbra_builder* slug_build_acc(void);
struct umbra_builder* slug_build    (void);
