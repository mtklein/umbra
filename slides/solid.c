#include "slide.h"
#include <stdlib.h>

typedef struct {
    float rx, ry, vx, vy;
    float rect_w, rect_h;
    int   w, h;
} solid_state;

static void solid_init(slide *s, int w, int h) {
    solid_state *st = malloc(sizeof *st);
    st->w = w;
    st->h = h;
    st->rect_w = (float)w * 5.0f / 16.0f;
    st->rect_h = (float)h * 5.0f / 16.0f;
    st->rx = (float)w * 5.0f / 32.0f;
    st->ry = (float)h / 6.0f;
    st->vx = 1.5f;
    st->vy = 1.1f;
    s->state = st;
}

static void solid_animate(slide *s, float dt) {
    solid_state *st = s->state;
    (void)dt;
    st->rx += st->vx;
    st->ry += st->vy;
    if (st->rx < 0.0f) {
        st->rx = 0.0f;
        st->vx = -st->vx;
    }
    if ((float)st->w < st->rx + st->rect_w) {
        st->rx = (float)st->w - st->rect_w;
        st->vx = -st->vx;
    }
    if (st->ry < 0.0f) {
        st->ry = 0.0f;
        st->vy = -st->vy;
    }
    if ((float)st->h < st->ry + st->rect_h) {
        st->ry = (float)st->h - st->rect_h;
        st->vy = -st->vy;
    }
}

static void solid_draw(slide *s, int w, int h, int y0, int y1, void *buf,
                          umbra_draw_layout const *lay, struct umbra_program *program) {
    solid_state *st = s->state;
    float        rect[4] = {
        st->rx,
        st->ry,
        st->rx + st->rect_w,
        st->ry + st->rect_h,
    };
    float hc[4];
    for (int i = 0; i < 4; i++) { hc[i] = s->color[i]; }
    uint64_t uni_[6] = {0};
    char     *uni = (char *)uni_;
    slide_uni_f32(uni, lay->shader, hc, 4);
    if (s->coverage) { slide_uni_f32(uni, lay->coverage, rect, 4); }
    int       pb = umbra_pixel_bytes(s->fmt);
    size_t plane_sz = (size_t)w * (size_t)h * (size_t)pb;
    umbra_buf ubuf[2];
    size_t rb = (size_t)w * (size_t)pb;
    ubuf[0] = (umbra_buf){.ptr=uni, .sz=(size_t)lay->uni_len, .read_only=1};
    ubuf[1] = (umbra_buf){.ptr=buf, .sz=plane_sz * (s->fmt == umbra_fmt_fp16_planar ? 4 : 1), .row_bytes=rb, .fmt=s->fmt};
    umbra_program_queue(program, 0, y0, w, y1, ubuf);
}

static void solid_cleanup(slide *s) {
    free(s->state);
    s->state = NULL;
}

slide slide_solid(char const *, uint32_t, float const[4], umbra_coverage_fn,
                  umbra_blend_fn, umbra_fmt);

slide slide_solid(char const *title, uint32_t bg, float const color[4],
                  umbra_coverage_fn coverage, umbra_blend_fn blend, umbra_fmt fmt) {
    return (slide){
        .title = title,
        .bg = bg,
        .shader = umbra_shader_solid,
        .coverage = coverage,
        .blend = blend,
        .fmt = fmt,
        .color = {color[0], color[1], color[2], color[3]},
        .init = solid_init,
        .animate = solid_animate,
        .draw = solid_draw,
        .cleanup = solid_cleanup,
    };
}
