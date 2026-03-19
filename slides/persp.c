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
    slide_perspective_matrix(st->mat, 0.0f,
        w, h, st->bitmap->w, st->bitmap->h);
}

static void persp_animate(slide *s, float dt) {
    persp_state *st = s->state;
    (void)dt;
    st->persp_t += 0.016f;
    slide_perspective_matrix(st->mat, st->persp_t,
        st->w, st->h,
        st->bitmap->w, st->bitmap->h);
}

static void persp_render_row(
        slide *s, int y, int w,
        void *row, long row_sz,
        umbra_draw_layout const *lay,
        int ps, int32_t stride,
        struct umbra_program *backend) {
    persp_state *st = s->state;
    float hc[4];
    for (int i = 0; i < 4; i++) {
        hc[i] = s->color[i];
    }
    int uni_len = lay->uni_len;
    long long uni_[12] = {0};
    char *uni = (char*)uni_;
    slide_uni_i32(uni, lay->x0, 0);
    slide_uni_i32(uni, lay->y,  y);
    slide_uni_f32(uni, lay->shader, hc, 4);
    slide_uni_f32(uni, lay->coverage,
        st->mat, 11);
    slide_uni_ptr(uni,
        (lay->coverage + 11*4 + 7) & ~7,
        st->bitmap->data,
        (long)(st->w * st->h * 2));
    for (int i = 0; i < ps; i++) {
        slide_uni_i32(uni,
            uni_len - (ps-i) * 4, stride);
    }
    umbra_buf buf[] = {
        { row,  row_sz },
        { uni, -(long)uni_len },
    };
    umbra_program_queue(backend, w, buf);
}

static void persp_cleanup(slide *s) {
    free(s->state);
    s->state = NULL;
}

slide slide_persp(text_cov*);

slide slide_persp(text_cov *bitmap) {
    persp_state *st = calloc(1, sizeof *st);
    st->bitmap = bitmap;
    return (slide){
        .title="9. Perspective Text",
        .shader=umbra_shader_solid,
        .coverage=umbra_coverage_bitmap_matrix,
        .blend=umbra_blend_srcover,
        .load=umbra_load_8888,
        .store=umbra_store_8888,
        .color={1.0f, 0.8f, 0.2f, 1.0f},
        .bg=0xff0a0a1e,
        .init=persp_init,
        .animate=persp_animate,
        .render_row=persp_render_row,
        .cleanup=persp_cleanup,
        .state=st,
    };
}
