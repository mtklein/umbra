#include "slide.h"
#include "text.h"
#include "slug.h"

extern slide slide_solid(char const *, uint32_t, float const[4], umbra_coverage_fn,
                         umbra_blend_fn, umbra_fmt);
extern slide slide_text_bitmap(text_cov *);
extern slide slide_text_sdf(text_cov *);
extern slide slide_persp(text_cov *);
extern slide slide_gradient_2stop(char const *, uint32_t, umbra_shader_fn, umbra_fmt,
                                  float const[8], float const[4]);
extern slide slide_gradient_lut(char const *, uint32_t, umbra_shader_fn, umbra_fmt,
                                float const[4], float *, int);
extern slide slide_slug_wind(slug_curves *);
extern slide slide_overview(void);

static text_cov    bitmap_cov, sdf_cov;
static slug_curves slug;

enum { LUT_N = 64 };
static float linear_lut[LUT_N * 4];
static float radial_lut[LUT_N * 4];

static slide all[18];
static int   count;

int    slide_count(void) { return count; }
slide *slide_get(int i) { return &all[i]; }

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

static void register_slides(void) {
    count = 0;
    all[count++] = slide_solid("1. Solid Fill (src)", 0xff202020,
                               (float[]){0.0f, 0.6f, 1.0f, 1.0f}, umbra_coverage_rect,
                               umbra_blend_src, umbra_fmt_8888);
    all[count++] = slide_solid("2. Source Over (srcover)", 0xff00ff00,
                               (float[]){0.45f, 0.0f, 0.0f, 0.5f}, umbra_coverage_rect,
                               umbra_blend_srcover, umbra_fmt_8888);
    all[count++] = slide_solid("3. Destination Over (dstover)", 0xc0008000,
                               (float[]){0.0f, 0.0f, 0.9f, 0.9f}, umbra_coverage_rect,
                               umbra_blend_dstover, umbra_fmt_8888);
    all[count++] = slide_solid("4. Multiply Blend", 0xff804020,
                               (float[]){1.0f, 0.5f, 0.0f, 1.0f}, umbra_coverage_rect,
                               umbra_blend_multiply, umbra_fmt_8888);
    all[count++] = slide_solid("5. Full Coverage (no rect clip)", 0xffffffff,
                               (float[]){0.15f, 0.0f, 0.3f, 0.3f}, NULL,
                               umbra_blend_srcover, umbra_fmt_8888);
    all[count++] = slide_solid("6. No Blend (direct paint)", 0xff000000,
                               (float[]){0.9f, 0.4f, 0.1f, 1.0f}, umbra_coverage_rect,
                               NULL, umbra_fmt_8888);
    all[count++] = slide_text_bitmap(&bitmap_cov);
    all[count++] = slide_text_sdf(&sdf_cov);
    all[count++] = slide_persp(&bitmap_cov);
    all[count++] =
        slide_gradient_2stop("10. Linear Gradient (2-stop)", 0xff000000,
                             umbra_shader_linear_2, umbra_fmt_8888,
                             (float[]){1.0f, 0.4f, 0.0f, 1.0f, 0.0f, 0.3f, 1.0f, 1.0f},
                             (float[]){1.0f / 640.0f, 0.0f, 0.0f, 0.0f});
    all[count++] =
        slide_gradient_2stop("11. Radial Gradient (2-stop)", 0xff000000,
                             umbra_shader_radial_2, umbra_fmt_8888,
                             (float[]){1.0f, 1.0f, 0.9f, 1.0f, 0.05f, 0.0f, 0.15f, 1.0f},
                             (float[]){320.0f, 240.0f, 1.0f / 300.0f, 0.0f});
    all[count++] = slide_gradient_lut("12. Linear Gradient (wide gamut)", 0xff000000,
                                      umbra_shader_linear_grad, umbra_fmt_8888,
                                      (float[]){1.0f / 640.0f, 0.0f, 0.0f, 64.0f},
                                      linear_lut, LUT_N);
    all[count++] = slide_gradient_lut("13. Radial Gradient (wide gamut)", 0xff000000,
                                      umbra_shader_radial_grad, umbra_fmt_8888,
                                      (float[]){320.0f, 240.0f, 1.0f / 280.0f, 64.0f},
                                      radial_lut, LUT_N);
    all[count++] = slide_slug_wind(&slug);
}

void slides_init(int w, int h) {
    float font = (float)h * 0.15f;
    bitmap_cov = text_rasterize(w, h, font, 0);
    sdf_cov = text_rasterize(w, h, font, 1);
    slug = slug_extract("Slug", (float)h * 0.3125f);
    build_luts();

    register_slides();

    for (int i = 0; i < count; i++) {
        if (all[i].init) { all[i].init(&all[i], w, h); }
    }

    all[count++] = slide_overview();
    all[count - 1].init(&all[count - 1], w, h);
}

void slides_init_for_dump(void) { register_slides(); }

void slides_cleanup(void) {
    for (int i = 0; i < count; i++) {
        if (all[i].cleanup) { all[i].cleanup(&all[i]); }
    }
    text_cov_free(&bitmap_cov);
    text_cov_free(&sdf_cov);
    slug_free(&slug);
    count = 0;
}
