#include "slide.h"
#include <math.h>
#include <stdlib.h>

struct solid_slide {
    struct slide base;

    float rx, ry, vx, vy;
    float rect_w, rect_h;
    int   w, h;
    int   has_sdf, :32;

    struct umbra_shader_solid    shader;
    struct umbra_sdf_rect        sdf;
    umbra_blend_fn               blend;

    struct umbra_fmt          fmt;
    struct umbra_draw_layout lay;
    struct umbra_sdf_draw   *qt;
    struct umbra_program    *prog;
};

static void solid_init(struct slide *s, int w, int h) {
    struct solid_slide *st = (struct solid_slide *)s;
    st->w = w;
    st->h = h;
    st->rect_w = (float)w * 5.0f / 16.0f;
    st->rect_h = (float)h * 5.0f / 16.0f;
    st->rx = (float)w * 5.0f / 32.0f;
    st->ry = (float)h / 6.0f;
    st->vx = 1.5f;
    st->vy = 1.1f;
}

static float bounce(float p0, float v, double secs, float range) {
    float p = fmodf(p0 + (float)secs * v, 2.0f * range);
    if (p < 0.0f) { p += 2.0f * range; }
    if (p > range) { p = 2.0f * range - p; }
    return p;
}

static void solid_prepare(struct slide *s, struct umbra_backend *be,
                          struct umbra_fmt fmt) {
    struct solid_slide *st = (struct solid_slide *)s;
    umbra_sdf_draw_free(st->qt);  st->qt   = NULL;
    if (st->prog) { st->prog->free(st->prog); st->prog = NULL; }
    free(st->lay.uniforms);
    st->fmt = fmt;
    if (st->has_sdf) {
        st->qt = umbra_sdf_draw(be, &st->sdf.base,
                                (struct umbra_sdf_draw_config){.hard_edge = 1},
                                &st->shader.base, st->blend, fmt, &st->lay);
    } else {
        struct umbra_builder *b = umbra_draw_builder(NULL, &st->shader.base,
                                                     st->blend, fmt, &st->lay);
        struct umbra_flat_ir *ir = umbra_flat_ir(b);
        umbra_builder_free(b);
        st->prog = be->compile(be, ir);
        umbra_flat_ir_free(ir);
    }
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void solid_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct solid_slide *st = (struct solid_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    if (st->has_sdf) {
        double const ticks = secs * 60.0;
        float rx = bounce(st->rx, st->vx, ticks, (float)st->w - st->rect_w);
        float ry = bounce(st->ry, st->vy, ticks, (float)st->h - st->rect_h);
        st->sdf.rect = (umbra_rect){rx, ry, rx + st->rect_w, ry + st->rect_h};
        umbra_sdf_draw_fill(&st->lay, &st->sdf.base, &st->shader.base);
    } else {
        umbra_draw_fill(&st->lay, NULL, &st->shader.base);
    }
    struct umbra_buf ubuf[] = {
        {.ptr=st->lay.uniforms, .count=st->lay.uni.slots},
        {.ptr=buf, .count=st->w * st->h * st->fmt.planes, .stride=st->w},
    };
    if (st->qt) {
        umbra_sdf_draw_queue(st->qt, l, t, r, b, ubuf);
    } else {
        st->prog->queue(st->prog, l, t, r, b, ubuf);
    }
}

static int solid_get_builders(struct slide *s, struct umbra_fmt fmt,
                              struct umbra_builder **out, int max) {
    if (max < 1) { return 0; }
    struct solid_slide *st = (struct solid_slide *)s;
    if (st->has_sdf) {
        struct umbra_sdf_coverage adapter = umbra_sdf_coverage(&st->sdf.base, 1);
        out[0] = umbra_draw_builder(&adapter.base, &st->shader.base, st->blend, fmt, NULL);
    } else {
        out[0] = umbra_draw_builder(NULL, &st->shader.base, st->blend, fmt, NULL);
    }
    return out[0] ? 1 : 0;
}

static void solid_free(struct slide *s) {
    struct solid_slide *st = (struct solid_slide *)s;
    umbra_sdf_draw_free(st->qt);
    if (st->prog) { st->prog->free(st->prog); }
    free(st->lay.uniforms);
    free(st);
}

static struct slide* make_solid(char const *title, float const bg[4], float const color[4],
                                _Bool has_sdf, umbra_blend_fn blend) {
    struct solid_slide *st = calloc(1, sizeof *st);
    umbra_color const c = {color[0], color[1], color[2], color[3]};
    st->shader  = umbra_shader_solid(c);
    st->has_sdf = has_sdf;
    if (has_sdf) { st->sdf = umbra_sdf_rect((umbra_rect){0, 0, 0, 0}); }
    st->blend   = blend;
    st->base = (struct slide){
        .title = title,
        .bg = {bg[0], bg[1], bg[2], bg[3]},
        .init = solid_init,
        .prepare = solid_prepare,
        .draw = solid_draw,
        .free = solid_free,
        .get_builders = solid_get_builders,
    };
    return &st->base;
}

SLIDE(slide_solid_src) {
    return make_solid("Solid Fill (src)", (float[]){0.125f, 0.125f, 0.125f, 1},
                      (float[]){0.0f, 0.6f, 1.0f, 1.0f},
                      1, umbra_blend_src);
}

SLIDE(slide_solid_srcover) {
    return make_solid("Source Over (srcover)", (float[]){0, 1, 0, 1},
                      (float[]){0.45f, 0.0f, 0.0f, 0.5f},
                      1, umbra_blend_srcover);
}

SLIDE(slide_solid_dstover) {
    return make_solid("Destination Over (dstover)", (float[]){0, 0.5f, 0, 0.75f},
                      (float[]){0.0f, 0.0f, 0.9f, 0.9f},
                      1, umbra_blend_dstover);
}

SLIDE(slide_solid_multiply) {
    return make_solid("Multiply Blend", (float[]){0.125f, 0.25f, 0.5f, 1},
                      (float[]){1.0f, 0.5f, 0.0f, 1.0f},
                      1, umbra_blend_multiply);
}

SLIDE(slide_solid_full_cov) {
    return make_solid("Full Coverage (no rect clip)", (float[]){1, 1, 1, 1},
                      (float[]){0.15f, 0.0f, 0.3f, 0.3f},
                      0, umbra_blend_srcover);
}

SLIDE(slide_solid_no_blend) {
    return make_solid("No Blend (direct paint)", (float[]){0, 0, 0, 1},
                      (float[]){0.9f, 0.4f, 0.1f, 1.0f},
                      1, NULL);
}
