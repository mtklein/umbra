#include "slide.h"
#include <stdlib.h>

typedef struct {
    float *lut;
    int    lut_n;
    int    pad_;
} grad_lut_state;

static void grad_2stop_draw(slide *s, int w, int h, int y0, int y1, void *buf,
                               umbra_draw_layout const *lay, struct umbra_program *program) {
    umbra_set_f32(lay->uni, (umbra_uniform){lay->shader},      s->grad,  3);
    umbra_set_f32(lay->uni, (umbra_uniform){lay->shader + 12}, s->color, 8);
    size_t    pb = umbra_fmt_size(s->fmt);
    size_t plane_sz = (size_t)w * (size_t)h * pb;
    umbra_buf ubuf[2];
    size_t rb = (size_t)w * pb;
    ubuf[0] = (umbra_buf){.ptr=lay->uni->data, .sz=lay->uni->size, .read_only=1};
    ubuf[1] = (umbra_buf){.ptr=buf, .sz=plane_sz * (s->fmt == umbra_fmt_fp16_planar ? 4 : 1), .row_bytes=rb};
    program->queue(program, 0, y0, w, y1, ubuf);
}

static void grad_lut_draw(slide *s, int w, int h, int y0, int y1, void *buf,
                             umbra_draw_layout const *lay, struct umbra_program *program) {
    grad_lut_state *st = s->state;
    umbra_set_f32(lay->uni, (umbra_uniform){lay->shader}, s->grad, 4);
    umbra_set_ptr(lay->uni, (umbra_uniform){(lay->shader + 16 + 7) & ~(size_t)7},
                  st->lut, (size_t)(st->lut_n * 4 * 4), 0, 0);
    size_t    pb = umbra_fmt_size(s->fmt);
    size_t plane_sz = (size_t)w * (size_t)h * pb;
    umbra_buf ubuf[2];
    size_t rb = (size_t)w * pb;
    ubuf[0] = (umbra_buf){.ptr=lay->uni->data, .sz=lay->uni->size, .read_only=1};
    ubuf[1] = (umbra_buf){.ptr=buf, .sz=plane_sz * (s->fmt == umbra_fmt_fp16_planar ? 4 : 1), .row_bytes=rb};
    program->queue(program, 0, y0, w, y1, ubuf);
}

static void grad_lut_cleanup(slide *s) {
    free(s->state);
    s->state = NULL;
}

slide slide_gradient_2stop(char const *, uint32_t, umbra_shader_fn, umbra_fmt,
                           float const[8], float const[4]);
slide slide_gradient_lut(char const *, uint32_t, umbra_shader_fn, umbra_fmt,
                         float const[4], float *, int);

slide slide_gradient_2stop(char const *title, uint32_t bg, umbra_shader_fn shader,
                           umbra_fmt fmt, float const color[8], float const grad[4]) {
    return (slide){
        .title = title,
        .bg = bg,
        .shader = shader,
        .fmt = fmt,
        .color = {color[0], color[1], color[2], color[3], color[4], color[5], color[6],
                  color[7]},
        .grad = {grad[0], grad[1], grad[2], grad[3]},
        .draw = grad_2stop_draw,
    };
}

slide slide_gradient_lut(char const *title, uint32_t bg, umbra_shader_fn shader,
                         umbra_fmt fmt, float const grad[4], float *lut, int lut_n) {
    grad_lut_state *st = malloc(sizeof *st);
    st->lut = lut;
    st->lut_n = lut_n;
    st->pad_ = 0;
    return (slide){
        .title = title,
        .bg = bg,
        .shader = shader,
        .fmt = fmt,
        .grad = {grad[0], grad[1], grad[2], grad[3]},
        .draw = grad_lut_draw,
        .cleanup = grad_lut_cleanup,
        .state = st,
    };
}
