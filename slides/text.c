#include "slide.h"
#include "text.h"
#include <stdlib.h>

typedef struct {
    text_cov *tc;
} text_state;

static void text_render(slide *s, int w, int h, void *buf, long buf_sz, int row_bytes,
                         umbra_draw_layout const *lay, struct umbra_program *program) {
    text_state *st = s->state;
    float       hc[4];
    for (int i = 0; i < 4; i++) { hc[i] = s->color[i]; }
    long long uni_[6] = {0};
    char     *uni = (char *)uni_;
    (void)row_bytes;
    slide_uni_i32(uni, lay->rs, w);
    {
        int ps = lay->ps;
        int32_t planar_stride = (int32_t)(w * h);
        for (int i = 0; i < ps; i++) {
            slide_uni_i32(uni, lay->uni_len - (ps - i) * 4, planar_stride);
        }
    }
    slide_uni_f32(uni, lay->shader, hc, 4);
    slide_uni_ptr(uni, lay->coverage, st->tc->data, (long)(w * h * 2));
    umbra_buf ubuf[] = {
        {buf, buf_sz},
        {uni, -(long)lay->uni_len},
    };
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
        .load = umbra_load_8888,
        .store = umbra_store_8888,
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
        .load = umbra_load_8888,
        .store = umbra_store_8888,
        .color = {0.2f, 0.8f, 1.0f, 1.0f},
        .bg = 0xff1a1a2e,
        .render = text_render,
        .cleanup = text_cleanup,
        .state = st,
    };
}
