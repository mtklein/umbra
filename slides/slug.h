#pragma once
#include "../include/umbra_draw.h"

struct slug_curves {
    float *data;
    int    count, :32;
    float  w, h;
};

struct slug_curves slug_extract(char const *text, float font_size);
void               slug_free   (struct slug_curves *sc);
