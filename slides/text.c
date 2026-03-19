#include "slide.h"
#include "text.h"
#include <stdlib.h>

typedef struct {
    text_cov *tc;
} text_state;

static void text_render_row(
        slide *s, int y, int w,
        void *row, long row_sz,
        umbra_draw_layout const *lay,
        int ps, int32_t stride,
        struct umbra_program *backend) {
    text_state *st = s->state;
    float hc[4];
    for (int i = 0; i < 4; i++) {
        hc[i] = s->color[i];
    }
    int uni_len = lay->uni_len;
    long long uni_[6] = {0};
    char *uni = (char*)uni_;
    slide_uni_i32(uni, lay->x0, 0);
    slide_uni_i32(uni, lay->y,  y);
    slide_uni_f32(uni, lay->shader, hc, 4);
    slide_uni_ptr(uni, lay->coverage,
        st->tc->data + y * w,
        (long)(w * 2));
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

static void text_cleanup(slide *s) {
    free(s->state);
    s->state = NULL;
}

slide slide_text_bitmap(text_cov*);
slide slide_text_sdf(text_cov*);

slide slide_text_bitmap(text_cov *tc) {
    text_state *st = malloc(sizeof *st);
    st->tc = tc;
    return (slide){
        .title="7. Text (8-bit AA)",
        .shader=umbra_shader_solid,
        .coverage=umbra_coverage_bitmap,
        .blend=umbra_blend_srcover,
        .load=umbra_load_8888,
        .store=umbra_store_8888,
        .color={1.0f, 1.0f, 1.0f, 1.0f},
        .bg=0xff1a1a2e,
        .render_row=text_render_row,
        .cleanup=text_cleanup,
        .state=st,
    };
}

slide slide_text_sdf(text_cov *tc) {
    text_state *st = malloc(sizeof *st);
    st->tc = tc;
    return (slide){
        .title="8. Text (SDF)",
        .shader=umbra_shader_solid,
        .coverage=umbra_coverage_sdf,
        .blend=umbra_blend_srcover,
        .load=umbra_load_8888,
        .store=umbra_store_8888,
        .color={0.2f, 0.8f, 1.0f, 1.0f},
        .bg=0xff1a1a2e,
        .render_row=text_render_row,
        .cleanup=text_cleanup,
        .state=st,
    };
}
