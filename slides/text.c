#include "slide.h"
#include "text.h"
#include <stdlib.h>

typedef struct {
    text_cov *tc;
} text_state;

static void text_draw(slide *s, int w, int h, int y0, int y1, void *buf,
                         umbra_draw_layout const *lay, struct umbra_program *program) {
    text_state *st = s->state;
    float       hc[4];
    for (int i = 0; i < 4; i++) { hc[i] = s->color[i]; }
    uint64_t uni_[6] = {0};
    char     *uni = (char *)uni_;
    slide_uni_f32(uni, lay->shader, hc, 4);
    slide_uni_ptr(uni, lay->coverage, st->tc->data, (size_t)(w * h * 2), 0, (size_t)w * 2);
    int       pb = umbra_pixel_bytes(s->fmt);
    size_t plane_sz = (size_t)w * (size_t)h * (size_t)pb;
    umbra_buf ubuf[2];
    size_t rb = (size_t)w * (size_t)pb;
    ubuf[0] = (umbra_buf){.ptr=uni, .sz=(size_t)lay->uni_len, .read_only=1};
    ubuf[1] = (umbra_buf){.ptr=buf, .sz=plane_sz * (s->fmt == umbra_fmt_fp16_planar ? 4 : 1), .row_bytes=rb, .fmt=s->fmt};
    program->queue(program, 0, y0, w, y1, ubuf);
}

static void text_cleanup(slide *s) {
    free(s->state);
    s->state = NULL;
}

slide slide_text_bitmap(text_cov *);
slide slide_text_sdf(text_cov *);

slide slide_text_bitmap(text_cov *tc) {
    text_state *st = malloc(sizeof *st);
    st->tc = tc;
    return (slide){
        .title = "7. Text (8-bit AA)",
        .shader = umbra_shader_solid,
        .coverage = umbra_coverage_bitmap,
        .blend = umbra_blend_srcover,
        .fmt = umbra_fmt_8888,
        .color = {1.0f, 1.0f, 1.0f, 1.0f},
        .bg = 0xff1a1a2e,
        .draw = text_draw,
        .cleanup = text_cleanup,
        .state = st,
    };
}

slide slide_text_sdf(text_cov *tc) {
    text_state *st = malloc(sizeof *st);
    st->tc = tc;
    return (slide){
        .title = "8. Text (SDF)",
        .shader = umbra_shader_solid,
        .coverage = umbra_coverage_sdf,
        .blend = umbra_blend_srcover,
        .fmt = umbra_fmt_8888,
        .color = {0.2f, 0.8f, 1.0f, 1.0f},
        .bg = 0xff1a1a2e,
        .draw = text_draw,
        .cleanup = text_cleanup,
        .state = st,
    };
}
