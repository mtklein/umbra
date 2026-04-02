#pragma once
#include "../include/umbra_draw.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "../third_party/stb/stb_truetype.h"
#pragma clang diagnostic pop

struct text_cov {
    uint16_t *data;
    int       w, h;
};

static inline unsigned char* text_load_font(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = malloc((size_t)sz);
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    return buf;
}

static inline struct text_cov text_rasterize(int W, int H, float font_size, _Bool sdf) {
    struct text_cov tc = { .data = calloc((size_t)(W * H), sizeof(uint16_t)), .w = W, .h = H };

    unsigned char *font_data = text_load_font("/System/Library/Fonts/Supplemental/Arial.ttf");
    if (!font_data) { return tc; }

    stbtt_fontinfo font;
    stbtt_InitFont(&font, font_data, stbtt_GetFontOffsetForIndex(font_data, 0));
    float scale = stbtt_ScaleForPixelHeight(&font, font_size);

    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &line_gap);

    char const *text = "hamburgefons";

    // Measure total width for centering.
    float total_w = 0;
    for (int i = 0; text[i]; i++) {
        int adv, lsb;
        stbtt_GetCodepointHMetrics(&font, text[i], &adv, &lsb);
        total_w += (float)adv * scale;
        if (text[i+1]) {
            int kern = stbtt_GetCodepointKernAdvance(&font, text[i], text[i+1]);
            total_w += (float)kern * scale;
        }
    }

    float cx = ((float)W - total_w) / 2;
    int baseline = (H - (int)font_size) / 2 + (int)((float)ascent * scale);

    for (int i = 0; text[i]; i++) {
        int gw, gh, gx, gy;
        unsigned char *bmp;

        if (sdf) {
            bmp = stbtt_GetCodepointSDF(&font, scale, text[i],
                                        5, 128, 128.0f / 5.0f,
                                        &gw, &gh, &gx, &gy);
        } else {
            bmp = stbtt_GetCodepointBitmap(&font, scale, scale, text[i],
                                           &gw, &gh, &gx, &gy);
        }

        if (bmp) {
            int dx = (int)cx + gx;
            int dy = baseline + gy;
            for (int row = 0; row < gh; row++) {
                int py = dy + row;
                if (py < 0 || py >= H) { continue; }
                for (int col = 0; col < gw; col++) {
                    int px = dx + col;
                    if (px < 0 || px >= W) { continue; }
                    uint8_t val = bmp[row * gw + col];
                    uint16_t cur = tc.data[py * W + px];
                    if (val > cur) { tc.data[py * W + px] = val; }
                }
            }
            if (sdf) { stbtt_FreeSDF(bmp, NULL); }
            else     { stbtt_FreeBitmap(bmp, NULL); }
        }

        int adv, lsb;
        stbtt_GetCodepointHMetrics(&font, text[i], &adv, &lsb);
        cx += (float)adv * scale;
        if (text[i+1]) {
            int kern = stbtt_GetCodepointKernAdvance(&font, text[i], text[i+1]);
            cx += (float)kern * scale;
        }
    }

    free(font_data);
    return tc;
}

static inline void text_cov_free(struct text_cov *tc) {
    free(tc->data);
    tc->data = NULL;
}
