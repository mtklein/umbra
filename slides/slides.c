#include "slide.h"
#include "text.h"
#include "slug.h"
#include <stdlib.h>

enum { LUT_N = 64, MAX_SLIDES = 32 };

static struct text_cov    bitmap_cov, sdf_cov;
static struct slug_curves slug;
static float              linear_lut[LUT_N * 4];
static float              radial_lut[LUT_N * 4];
static struct slide_ctx   ctx;

struct slide_entry {
    int               order, :32;
    slide_factory_fn  factory;
};

static struct slide_entry registry[MAX_SLIDES];
static int                registry_count;

static struct slide *all[MAX_SLIDES];
static int           count;

void slide_register(int order, slide_factory_fn factory) {
    if (registry_count < MAX_SLIDES) {
        registry[registry_count].order   = order;
        registry[registry_count].factory = factory;
        registry_count++;
    }
}

int           slide_count(void) { return count; }
struct slide *slide_get(int i)  { return all[i]; }

static int cmp_entry(void const *a, void const *b) {
    int oa = ((struct slide_entry const *)a)->order;
    int ob = ((struct slide_entry const *)b)->order;
    return (oa > ob) - (oa < ob);
}

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

void slides_init(int w, int h, slide_alloc_fn alloc, slide_free_fn sfree) {
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

    qsort(registry, (size_t)registry_count, sizeof registry[0], cmp_entry);

    count = 0;
    for (int i = 0; i < registry_count; i++) {
        all[count] = registry[i].factory(&ctx);
        all[count]->alloc = alloc;
        all[count]->sfree = sfree;
        if (all[count]->init) { all[count]->init(all[count], w, h); }
        count++;
    }
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
