#include "slide.h"
#include "text.h"
#include <stdlib.h>

typedef struct {
    text_cov *tc;
} text_state;

static void text_render(slide *s, int w, int h, void *buf,
                         umbra_draw_layout const *lay, struct umbra_program *program) {
    text_state *st = s->state;
    float       hc[4];
    for (int i = 0; i < 4; i++) { hc[i] = s->color[i]; }
    long long uni_[6] = {0};
    char     *uni = (char *)uni_;
    slide_uni_f32(uni, lay->shader, hc, 4);
    slide_uni_ptr(uni, lay->coverage, st->tc->data, (long)(w * h * 2));
    int       ps = lay->ps;
    size_t plane_sz = (size_t)w * (size_t)h * lay->pixel_bytes;
    umbra_buf ubuf[5];
    ubuf[0] = (umbra_buf){uni, (size_t)lay->uni_len, 1};
    ubuf[1] = (umbra_buf){buf, plane_sz, 0};
    for (int i = 0; i < ps; i++) {
        ubuf[2 + i] = (umbra_buf){(char *)buf + plane_sz * (size_t)(i + 1), plane_sz, 0};
    }
    umbra_program_queue(program, w, h, ubuf);
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
        .format = umbra_format_8888,
        .color = {1.0f, 1.0f, 1.0f, 1.0f},
        .bg = 0xff1a1a2e,
        .render = text_render,
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
        .format = umbra_format_8888,
        .color = {0.2f, 0.8f, 1.0f, 1.0f},
        .bg = 0xff1a1a2e,
        .render = text_render,
        .cleanup = text_cleanup,
        .state = st,
    };
}
