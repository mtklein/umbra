#include "slide.h"
#include "text.h"
#include "slug.h"
#include <math.h>
#include <stdlib.h>

void slide_perspective_matrix(float out[11], float t, int sw, int sh, int bw, int bh) {
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

enum { LUT_N = 64, MAX_SLIDES = 32 };

struct slide *make_overview(struct slide_ctx const *);

static struct text_cov    bitmap_cov, sdf_cov;
static struct slug_curves slug;
static float              linear_lut[LUT_N * 4];
static float              radial_lut[LUT_N * 4];
static struct slide_ctx   ctx;

static slide_factory_fn registry[MAX_SLIDES];
static int              registry_count;

static struct slide *all[MAX_SLIDES];
static int           count;

void slide_register(slide_factory_fn factory) {
    if (registry_count < MAX_SLIDES) {
        registry[registry_count++] = factory;
    }
}

int           slide_count(void) { return count; }
struct slide *slide_get(int i)  { return all[i]; }

static void build_luts(void) {
    float const linear_stops[][4] = {
        {1.2f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.8f, 0.0f, 1.0f}, {0.0f, 1.2f, 0.0f, 1.0f},
        {0.0f, 0.8f, 1.2f, 1.0f}, {0.0f, 0.0f, 1.2f, 1.0f}, {0.8f, 0.0f, 1.0f, 1.0f},
    };
    umbra_gradient_lut_even(linear_lut, LUT_N, 6, linear_stops);

    float const radial_stops[][4] = {
        {1.5f, 1.5f, 1.2f, 1.0f},
        {1.2f, 0.8f, 0.0f, 1.0f},
        {0.8f, 0.0f, 0.2f, 1.0f},
        {0.05f, 0.0f, 0.15f, 1.0f},
    };
    umbra_gradient_lut_even(radial_lut, LUT_N, 4, radial_stops);
}

static void add_slide(struct slide *s, int w, int h) {
    all[count] = s;
    if (s->init) { s->init(s, w, h); }
    count++;
}

void slides_init(int w, int h) {
    float font = (float)h * 0.15f;
    bitmap_cov = text_rasterize(w, h, font, 0);
    sdf_cov    = text_rasterize(w, h, font, 1);
    slug       = slug_extract("Slug", (float)h * 0.3125f);
    build_luts();

    ctx.bitmap_cov = &bitmap_cov;
    ctx.sdf_cov    = &sdf_cov;
    ctx.slug       = &slug;
    ctx.linear_lut = linear_lut;
    ctx.radial_lut = radial_lut;
    ctx.lut_n      = LUT_N;

    count = 0;
    for (int i = 0; i < registry_count; i++) {
        add_slide(registry[i](&ctx), w, h);
    }
    add_slide(make_overview(&ctx), w, h);
}

void slides_cleanup(void) {
    for (int i = 0; i < count; i++) {
        if (all[i]->free) { all[i]->free(all[i]); }
    }
    text_cov_free(&bitmap_cov);
    text_cov_free(&sdf_cov);
    slug_free(&slug);
    count = 0;
}
