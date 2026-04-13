#pragma once
#include "../include/umbra_draw.h"

struct slug_curves {
    float *data;
    int    count;
    int    pad_;
    float  w, h;
};

struct slug_acc_layout {
    struct umbra_uniforms_layout  uni; int :32;
    void                         *uniforms;
    int mat, wh, curves_off, loop_off;
};

struct slug_curves    slug_extract  (char const *text, float font_size);
void                  slug_free     (struct slug_curves *sc);
struct umbra_builder *slug_build_acc(struct slug_acc_layout *lay);
