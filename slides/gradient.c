#include "slide.h"
#include <stdlib.h>

typedef struct {
    float *lut;
    int    lut_n;
    int    pad_;
} grad_lut_state;

static void grad_2stop_render(slide *s, int w, int h, void *buf,
                               umbra_draw_layout const *lay, struct umbra_program *program) {
    int       uni_len = lay->uni_len;
    long long uni_[8] = {0};
    char     *uni = (char *)uni_;
    slide_uni_f32(uni, lay->shader, s->grad, 3);
    slide_uni_f32(uni, lay->shader + 12, s->color, 8);
    int       ps = lay->ps;
    size_t plane_sz = (size_t)w * (size_t)h * lay->pixel_bytes;
    umbra_buf ubuf[5];
    ubuf[0] = (umbra_buf){uni, (size_t)uni_len, 1};
    ubuf[1] = (umbra_buf){buf, plane_sz, 0};
    for (int i = 0; i < ps; i++) {
        ubuf[2 + i] = (umbra_buf){(char *)buf + plane_sz * (size_t)(i + 1), plane_sz, 0};
    }
    umbra_program_queue(program, w, h, ubuf);
}

static void grad_lut_render(slide *s, int w, int h, void *buf,
                             umbra_draw_layout const *lay, struct umbra_program *program) {
    grad_lut_state *st = s->state;
    int             uni_len = lay->uni_len;
    long long       uni_[8] = {0};
    char           *uni = (char *)uni_;
    slide_uni_f32(uni, lay->shader, s->grad, 4);
    slide_uni_ptr(uni, (lay->shader + 16 + 7) & ~7, st->lut, (long)(st->lut_n * 4 * 4));
    int       ps = lay->ps;
    size_t plane_sz = (size_t)w * (size_t)h * lay->pixel_bytes;
    umbra_buf ubuf[5];
    ubuf[0] = (umbra_buf){uni, (size_t)uni_len, 1};
    ubuf[1] = (umbra_buf){buf, plane_sz, 0};
    for (int i = 0; i < ps; i++) {
        ubuf[2 + i] = (umbra_buf){(char *)buf + plane_sz * (size_t)(i + 1), plane_sz, 0};
    }
    umbra_program_queue(program, w, h, ubuf);
}

static void grad_lut_cleanup(slide *s) {
    free(s->state);
    s->state = NULL;
}

slide slide_gradient_2stop(char const *, uint32_t, umbra_shader_fn, umbra_format,
                           float const[8], float const[4]);
slide slide_gradient_lut(char const *, uint32_t, umbra_shader_fn, umbra_format,
                         float const[4], float *, int);

slide slide_gradient_2stop(char const *title, uint32_t bg, umbra_shader_fn shader,
                           umbra_format format, float const color[8], float const grad[4]) {
    return (slide){
        .title = title,
        .bg = bg,
        .shader = shader,
        .format = format,
        .color = {color[0], color[1], color[2], color[3], color[4], color[5], color[6],
                  color[7]},
        .grad = {grad[0], grad[1], grad[2], grad[3]},
        .render = grad_2stop_render,
    };
}

slide slide_gradient_lut(char const *title, uint32_t bg, umbra_shader_fn shader,
                         umbra_format format, float const grad[4], float *lut, int lut_n) {
    grad_lut_state *st = malloc(sizeof *st);
    st->lut = lut;
    st->lut_n = lut_n;
    st->pad_ = 0;
    return (slide){
        .title = title,
        .bg = bg,
        .shader = shader,
        .format = format,
        .grad = {grad[0], grad[1], grad[2], grad[3]},
        .render = grad_lut_render,
        .cleanup = grad_lut_cleanup,
        .state = st,
    };
}
