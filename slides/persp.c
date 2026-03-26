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

static void persp_render(slide *s, int w, int h, void *buf, long buf_sz,
                          umbra_draw_layout const *lay, struct umbra_program *program) {
    persp_state *st = s->state;
    float        hc[4];
    for (int i = 0; i < 4; i++) { hc[i] = s->color[i]; }
    long long uni_[12] = {0};
    char     *uni = (char *)uni_;
    slide_uni_f32(uni, lay->shader, hc, 4);
    slide_uni_f32(uni, lay->coverage, st->mat, 11);
    slide_uni_ptr(uni, (lay->coverage + 11 * 4 + 7) & ~7, st->bitmap->data,
                  (long)(st->bitmap->w * st->bitmap->h * 2));
    int       ps = lay->ps;
    long      plane_sz = ps ? (long)w * h * 2 : buf_sz;
    umbra_buf ubuf[5];
    ubuf[0] = (umbra_buf){uni, -(long)lay->uni_len};
    ubuf[1] = (umbra_buf){buf, plane_sz};
    for (int i = 0; i < ps; i++) {
        ubuf[2 + i] = (umbra_buf){(char *)buf + plane_sz * (i + 1), plane_sz};
    }
    umbra_program_queue(program, w, h, ubuf);
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
        .load = umbra_load_8888,
        .store = umbra_store_8888,
        .color = {1.0f, 0.8f, 0.2f, 1.0f},
        .bg = 0xff0a0a1e,
        .init = persp_init,
        .animate = persp_animate,
        .render = persp_render,
        .cleanup = persp_cleanup,
        .state = st,
    };
}
