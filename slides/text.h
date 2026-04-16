#pragma once
#include "../include/umbra_draw.h"
#include <stdint.h>

struct text_cov {
    uint16_t *data;
    int       w, h;
};

struct text_cov text_rasterize(int W, int H, float font_size, _Bool sdf);
void            text_cov_free (struct text_cov *tc);

void             text_shared_init   (int w, int h, float font_size);
void             text_shared_cleanup(void);
struct text_cov* text_shared_bitmap (void);
struct text_cov* text_shared_sdf    (void);
