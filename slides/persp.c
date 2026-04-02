#include "slide.h"
#include "text.h"
#include <stdlib.h>

typedef struct {
    float     persp_t;
    float     mat[11];
    text_cov *bitmap;
    int       w, h;
} persp_state;

static void persp_init(slide *s, int w, int h) {
    persp_state *st = s->state;
    st->w = w;
    st->h = h;
    st->persp_t = 0.0f;
    slide_perspective_matrix(st->mat, 0.0f, w, h, st->bitmap->w, st->bitmap->h);
}

static void persp_animate(slide *s, float dt) {
    persp_state *st = s->state;
    (void)dt;
    st->persp_t += 0.016f;
    slide_perspective_matrix(st->mat, st->persp_t, st->w, st->h, st->bitmap->w,
                             st->bitmap->h);
}

static void persp_draw(slide *s, int w, int h, int y0, int y1, void *buf,
                          umbra_draw_layout const *lay, struct umbra_program *program) {
    persp_state *st = s->state;
    float        hc[4];
    for (int i = 0; i < 4; i++) { hc[i] = s->color[i]; }
    umbra_set_f32(lay->uni, lay->shader,   hc, 4);
    umbra_set_f32(lay->uni, lay->coverage, st->mat, 11);
    umbra_set_ptr(lay->uni, (lay->coverage + 44 + 7) & ~(size_t)7,
                  st->bitmap->data,
                  (size_t)(st->bitmap->w * st->bitmap->h * 2), 0, 0);
    size_t    pb = umbra_fmt_size(s->fmt);
    size_t plane_sz = (size_t)w * (size_t)h * pb;
    umbra_buf ubuf[2];
    size_t rb = (size_t)w * pb;
    ubuf[0] = (umbra_buf){.ptr=lay->uni->data, .sz=lay->uni->size, .read_only=1};
    ubuf[1] = (umbra_buf){.ptr=buf, .sz=plane_sz * (s->fmt == umbra_fmt_fp16_planar ? 4 : 1), .row_bytes=rb};
    program->queue(program, 0, y0, w, y1, ubuf);
}

static void persp_cleanup(slide *s) {
    free(s->state);
    s->state = NULL;
}

slide slide_persp(text_cov *);

slide slide_persp(text_cov *bitmap) {
    persp_state *st = calloc(1, sizeof *st);
    st->bitmap = bitmap;
    return (slide){
        .title = "9. Perspective Text",
        .shader = umbra_shader_solid,
        .coverage = umbra_coverage_bitmap_matrix,
        .blend = umbra_blend_srcover,
        .fmt = umbra_fmt_8888,
        .color = {1.0f, 0.8f, 0.2f, 1.0f},
        .bg = 0xff0a0a1e,
        .init = persp_init,
        .animate = persp_animate,
        .draw = persp_draw,
        .cleanup = persp_cleanup,
        .state = st,
    };
}
