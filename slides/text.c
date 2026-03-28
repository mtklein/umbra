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
    int       ps = lay->ps;
    size_t plane_sz = (size_t)w * (size_t)h * lay->pixel_bytes;
    umbra_buf ubuf[5];
    size_t rb = (size_t)w * lay->pixel_bytes;
    ubuf[0] = (umbra_buf){.ptr=uni, .sz=(size_t)lay->uni_len, .read_only=1};
    ubuf[1] = (umbra_buf){.ptr=buf, .sz=plane_sz, .row_bytes=rb};
    for (int i = 0; i < ps; i++) {
        ubuf[2 + i] = (umbra_buf){.ptr=(char *)buf + plane_sz * (size_t)(i + 1), .sz=plane_sz, .row_bytes=rb};
    }
    umbra_program_queue(program, 0, y0, w, y1, ubuf);
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
        .format = umbra_format_8888,
        .color = {0.2f, 0.8f, 1.0f, 1.0f},
        .bg = 0xff1a1a2e,
        .draw = text_draw,
        .cleanup = text_cleanup,
        .state = st,
    };
}
