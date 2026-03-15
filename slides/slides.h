#pragma once
#include "../umbra_draw.h"
#include "text.h"
#include <stdint.h>

// Each slide demonstrates a different pipeline configuration.
// The demo cycles through these on left/right arrow keys.

typedef struct {
    const char        *title;
    umbra_shader_fn    shader;
    umbra_coverage_fn  coverage;
    umbra_blend_fn     blend;
    umbra_load_fn      load;
    umbra_store_fn     store;
    float              color[4];   // premultiplied RGBA as float (converted to fp16 at runtime)
    uint32_t           bg;         // background fill, RGBA8888
    uint32_t           pad_;
} slide;

static const slide slides[] = {
    {
        .title    = "1. Solid Fill (src)",
        .shader   = umbra_shader_solid,
        .coverage = umbra_coverage_rect,
        .blend    = umbra_blend_src,
        .load     = umbra_load_8888,
        .store    = umbra_store_8888,
        .color    = {0.0f, 0.6f, 1.0f, 1.0f},
        .bg       = 0xff202020,
    },
    {
        .title    = "2. Source Over (srcover)",
        .shader   = umbra_shader_solid,
        .coverage = umbra_coverage_rect,
        .blend    = umbra_blend_srcover,
        .load     = umbra_load_8888,
        .store    = umbra_store_8888,
        .color    = {0.45f, 0.0f, 0.0f, 0.5f},
        .bg       = 0xff00ff00,
    },
    {
        .title    = "3. Destination Over (dstover)",
        .shader   = umbra_shader_solid,
        .coverage = umbra_coverage_rect,
        .blend    = umbra_blend_dstover,
        .load     = umbra_load_8888,
        .store    = umbra_store_8888,
        .color    = {0.0f, 0.0f, 0.9f, 0.9f},
        .bg       = 0xc0008000,
    },
    {
        .title    = "4. Multiply Blend",
        .shader   = umbra_shader_solid,
        .coverage = umbra_coverage_rect,
        .blend    = umbra_blend_multiply,
        .load     = umbra_load_8888,
        .store    = umbra_store_8888,
        .color    = {1.0f, 0.5f, 0.0f, 1.0f},
        .bg       = 0xff804020,
    },
    {
        .title    = "5. Full Coverage (no rect clip)",
        .shader   = umbra_shader_solid,
        .coverage = NULL,
        .blend    = umbra_blend_srcover,
        .load     = umbra_load_8888,
        .store    = umbra_store_8888,
        .color    = {0.15f, 0.0f, 0.3f, 0.3f},
        .bg       = 0xffffffff,
    },
    {
        .title    = "6. No Blend (direct paint)",
        .shader   = umbra_shader_solid,
        .coverage = umbra_coverage_rect,
        .blend    = NULL,
        .load     = NULL,
        .store    = umbra_store_8888,
        .color    = {0.9f, 0.4f, 0.1f, 1.0f},
        .bg       = 0xff000000,
    },
    {
        .title    = "7. Text (8-bit AA)",
        .shader   = umbra_shader_solid,
        .coverage = umbra_coverage_bitmap,
        .blend    = umbra_blend_srcover,
        .load     = umbra_load_8888,
        .store    = umbra_store_8888,
        .color    = {1.0f, 1.0f, 1.0f, 1.0f},
        .bg       = 0xff1a1a2e,
    },
    {
        .title    = "8. Text (SDF)",
        .shader   = umbra_shader_solid,
        .coverage = umbra_coverage_sdf,
        .blend    = umbra_blend_srcover,
        .load     = umbra_load_8888,
        .store    = umbra_store_8888,
        .color    = {0.2f, 0.8f, 1.0f, 1.0f},
        .bg       = 0xff1a1a2e,
    },
    {
        .title    = "9. Perspective Text",
        .shader   = umbra_shader_solid,
        .coverage = umbra_coverage_bitmap_matrix,
        .blend    = umbra_blend_srcover,
        .load     = umbra_load_8888,
        .store    = umbra_store_8888,
        .color    = {1.0f, 0.8f, 0.2f, 1.0f},
        .bg       = 0xff0a0a1e,
    },
};

#define SLIDE_COUNT ((int)(sizeof slides / sizeof slides[0]))
