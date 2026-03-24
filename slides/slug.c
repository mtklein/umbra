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
    struct umbra_program     *acc;
    struct umbra_backend     *acc_be;
} slug_state;

static void slug_init(slide *s, int w, int h) {
    slug_state *st = s->state;
    st->w = w;
    st->h = h;
    st->persp_t = 0.0f;
    st->wind_buf = malloc((size_t)w * sizeof(float));

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

static void slug_render(slide *s, int w, int h, void *buf, long buf_sz, int row_bytes,
                         umbra_draw_layout const *lay, struct umbra_program *program) {
    (void)buf_sz;
    slug_state           *st = s->state;
    struct umbra_backend *be = umbra_program_backend(program);
    if (be != st->acc_be) {
        umbra_program_free(st->acc);
        st->acc = umbra_backend_compile(be, st->acc_bb);
        st->acc_be = be;
    }

    for (int y = 0; y < h; y++) {
        void *row = (char *)buf + y * row_bytes;
        __builtin_memset(st->wind_buf, 0, (size_t)w * sizeof(float));

        long long au_[12] = {0};
        char     *au = (char *)au_;
        slide_uni_i32(au, st->acc_lay.x0, 0);
        slide_uni_i32(au, st->acc_lay.y, y);
        slide_uni_f32(au, st->acc_lay.mat, st->mat, 11);
        slide_uni_ptr(au, st->acc_lay.curves_off, st->slug->data,
                      (long)(st->slug->count * 6 * 4));
        umbra_buf abuf[] = {
            {st->wind_buf, (long)((int)sizeof(float) * w)},
            {au, -(long)st->acc_lay.uni_len},
        };
        for (int j = 0; j < st->slug->count; j++) {
            int32_t j32 = j;
            __builtin_memcpy(au + st->acc_lay.loop_off, &j32, 4);
            umbra_program_queue(st->acc, w, 1, abuf);
        }
        umbra_backend_flush(st->acc_be);

        float hc[4];
        for (int i = 0; i < 4; i++) { hc[i] = s->color[i]; }
        long long uni_[12] = {0};
        char     *uni = (char *)uni_;
        slide_uni_i32(uni, lay->rs, w);
        slide_uni_f32(uni, lay->shader, hc, 4);
        slide_uni_ptr(uni, lay->coverage, st->wind_buf, -(long)((int)sizeof(float) * w));
        umbra_buf rbuf[] = {
            {row, (long)row_bytes},
            {uni, -(long)lay->uni_len},
        };
        umbra_program_queue(program, w, 1, rbuf);
    }
}

static void slug_cleanup(slide *s) {
    slug_state *st = s->state;
    free(st->wind_buf);
    umbra_program_free(st->acc);
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
        .load = umbra_load_8888,
        .store = umbra_store_8888,
        .color = {0.2f, 1.0f, 0.6f, 1.0f},
        .bg = 0xff0a0a1e,
        .init = slug_init,
        .animate = slug_animate,
        .render = slug_render,
        .cleanup = slug_cleanup,
        .state = st,
    };
}
