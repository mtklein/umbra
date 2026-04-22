#pragma once
#include "../include/umbra_draw.h"
#include <stdint.h>

struct text_cov {
    uint8_t *data;
    int      w, h;
};

struct text_cov text_rasterize(int W, int H, float font_size, _Bool sdf);
void            text_cov_free (struct text_cov *tc);
