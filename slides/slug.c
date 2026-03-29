#include "slide.h"
#include "slug.h"

typedef struct {
    float                     persp_t;
    float                     mat[11];
    slug_curves              *slug;
    int                       w, h;
    float                    *wind_buf;
    slug_acc_layout           acc_lay;
    struct umbra_basic_block *acc_bb;
} slug_state;

static void slug_init(slide *s, int w, int h) {
    slug_state *st = s->state;
    st->w = w;
    st->h = h;
    st->persp_t = 0.0f;
    st->wind_buf = malloc((size_t)w * (size_t)h * sizeof(float));

    struct umbra_builder *b = slug_build_acc(&st->acc_lay);
    st->acc_bb = umbra_basic_block(b);
    umbra_builder_free(b);

    slide_perspective_matrix(st->mat, 0.0f, w, h, (int)st->slug->w, (int)st->slug->h);
    st->mat[9] = st->slug->w;
    st->mat[10] = st->slug->h;
}

static void slug_animate(slide *s, float dt) {
    slug_state *st = s->state;
    (void)dt;
    st->persp_t += 0.016f;
    slide_perspective_matrix(st->mat, st->persp_t, st->w, st->h, (int)st->slug->w,
                             (int)st->slug->h);
    st->mat[9] = st->slug->w;
    st->mat[10] = st->slug->h;
}

static void slug_draw(slide *s, int w, int h, int y0, int y1, void *buf,
                       umbra_draw_layout const *lay, struct umbra_program *program) {
    slug_state           *st = s->state;
    struct umbra_backend *be = program->backend;
    struct umbra_program *acc = be->compile(be, st->acc_bb);

    size_t wind_sz  = (size_t)w * (size_t)h * sizeof(float);
    size_t wind_row = (size_t)w * sizeof(float);
    __builtin_memset((char *)st->wind_buf + (size_t)y0 * wind_row, 0,
                     (size_t)(y1 - y0) * wind_row);

    uint64_t au_[12] = {0};
    char     *au = (char *)au_;
    slide_uni_f32(au, st->acc_lay.mat, st->mat, 11);
    slide_uni_ptr(au, st->acc_lay.curves_off, st->slug->data,
                  (size_t)(st->slug->count * 6 * 4), 0, 0);
    umbra_buf abuf[] = {
        {.ptr=au, .sz=(size_t)st->acc_lay.uni_len, .read_only=1},
        {.ptr=st->wind_buf, .sz=wind_sz, .row_bytes=wind_row},
    };
    for (int j = 0; j < st->slug->count; j++) {
        int32_t j32 = j;
        __builtin_memcpy(au + st->acc_lay.loop_off, &j32, 4);
        umbra_program_queue(acc, 0, y0, w, y1, abuf);
    }
    umbra_backend_flush(be);
    acc->free(acc->ctx); free(acc);

    float hc[4];
    for (int i = 0; i < 4; i++) { hc[i] = s->color[i]; }
    uint64_t uni_[12] = {0};
    char     *uni = (char *)uni_;
    slide_uni_f32(uni, lay->shader, hc, 4);
    slide_uni_ptr(uni, lay->coverage, st->wind_buf, wind_sz, 1, (size_t)w * sizeof(float));
    int       pb = umbra_pixel_bytes(s->fmt);
    size_t plane_sz = (size_t)w * (size_t)h * (size_t)pb;
    umbra_buf rbuf[2];
    size_t rb = (size_t)w * (size_t)pb;
    rbuf[0] = (umbra_buf){.ptr=uni, .sz=(size_t)lay->uni_len, .read_only=1};
    rbuf[1] = (umbra_buf){.ptr=buf, .sz=plane_sz * (s->fmt == umbra_fmt_fp16_planar ? 4 : 1), .row_bytes=rb, .fmt=s->fmt};
    umbra_program_queue(program, 0, y0, w, y1, rbuf);
}

static void slug_cleanup(slide *s) {
    slug_state *st = s->state;
    free(st->wind_buf);
    umbra_basic_block_free(st->acc_bb);
    free(st);
    s->state = NULL;
}

slide slide_slug_wind(slug_curves *);

slide slide_slug_wind(slug_curves *sc) {
    slug_state *st = calloc(1, sizeof *st);
    st->slug = sc;
    return (slide){
        .title = "14. Slug Text (Bezier)",
        .shader = umbra_shader_solid,
        .coverage = umbra_coverage_wind,
        .blend = umbra_blend_srcover,
        .fmt = umbra_fmt_8888,
        .color = {0.2f, 1.0f, 0.6f, 1.0f},
        .bg = 0xff0a0a1e,
        .init = slug_init,
        .animate = slug_animate,
        .draw = slug_draw,
        .cleanup = slug_cleanup,
        .state = st,
    };
}
