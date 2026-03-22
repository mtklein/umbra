#include "slide.h"
#include <stdlib.h>

typedef struct {
    float *lut;
    int    lut_n;
    int    pad_;
} grad_lut_state;

static void grad_2stop_render_row(slide *s, int y, int w, void *row, long row_sz,
                                  umbra_draw_layout const *lay, int ps, int32_t stride,
                                  struct umbra_program *program) {
    int       uni_len = lay->uni_len;
    long long uni_[8] = {0};
    char     *uni = (char *)uni_;
    slide_uni_i32(uni, lay->x0, 0);
    slide_uni_i32(uni, lay->y, y);
    slide_uni_f32(uni, lay->shader, s->grad, 3);
    slide_uni_f32(uni, lay->shader + 12, s->color, 8);
    for (int i = 0; i < ps; i++) { slide_uni_i32(uni, uni_len - (ps - i) * 4, stride); }
    umbra_buf buf[] = {
        {row, row_sz},
        {uni, -(long)uni_len},
    };
    umbra_program_queue(program, w, buf);
}

static void grad_lut_render_row(slide *s, int y, int w, void *row, long row_sz,
                                umbra_draw_layout const *lay, int ps, int32_t stride,
                                struct umbra_program *program) {
    grad_lut_state *st = s->state;
    int             uni_len = lay->uni_len;
    long long       uni_[8] = {0};
    char           *uni = (char *)uni_;
    slide_uni_i32(uni, lay->x0, 0);
    slide_uni_i32(uni, lay->y, y);
    slide_uni_f32(uni, lay->shader, s->grad, 4);
    slide_uni_ptr(uni, (lay->shader + 16 + 7) & ~7, st->lut, (long)(st->lut_n * 4 * 4));
    for (int i = 0; i < ps; i++) { slide_uni_i32(uni, uni_len - (ps - i) * 4, stride); }
    umbra_buf buf[] = {
        {row, row_sz},
        {uni, -(long)uni_len},
    };
    umbra_program_queue(program, w, buf);
}

static void grad_lut_cleanup(slide *s) {
    free(s->state);
    s->state = NULL;
}

slide slide_gradient_2stop(char const *, uint32_t, umbra_shader_fn, umbra_store_fn,
                           float const[8], float const[4]);
slide slide_gradient_lut(char const *, uint32_t, umbra_shader_fn, umbra_store_fn,
                         float const[4], float *, int);

slide slide_gradient_2stop(char const *title, uint32_t bg, umbra_shader_fn shader,
                           umbra_store_fn store, float const color[8], float const grad[4]) {
    return (slide){
        .title = title,
        .bg = bg,
        .shader = shader,
        .store = store,
        .color = {color[0], color[1], color[2], color[3], color[4], color[5], color[6],
                  color[7]},
        .grad = {grad[0], grad[1], grad[2], grad[3]},
        .render_row = grad_2stop_render_row,
    };
}

slide slide_gradient_lut(char const *title, uint32_t bg, umbra_shader_fn shader,
                         umbra_store_fn store, float const grad[4], float *lut, int lut_n) {
    grad_lut_state *st = malloc(sizeof *st);
    st->lut = lut;
    st->lut_n = lut_n;
    st->pad_ = 0;
    return (slide){
        .title = title,
        .bg = bg,
        .shader = shader,
        .store = store,
        .grad = {grad[0], grad[1], grad[2], grad[3]},
        .render_row = grad_lut_render_row,
        .cleanup = grad_lut_cleanup,
        .state = st,
    };
}
