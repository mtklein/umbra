#include "slide.h"
#include "slug.h"

struct slug_state {
    struct slide base;

    float                     persp_t;
    float                     mat[11];
    float                     color[4];
    struct slug_curves       *slug;
    int                       w, h;
    float                    *wind_buf;
    struct slug_acc_layout    acc_lay;
    struct umbra_basic_block *acc_bb;
    struct umbra_program     *acc_prog;
    struct umbra_draw_layout  draw_lay;
    struct umbra_basic_block *draw_bb;
    struct umbra_program     *draw_prog;
};

static void slug_init(struct slide *s, int w, int h) {
    struct slug_state *st = (struct slug_state *)s;
    st->w = w;
    st->h = h;
    st->persp_t = 0.0f;
    st->wind_buf = malloc((size_t)w * (size_t)h * sizeof(float));

    struct umbra_builder *b = slug_build_acc(&st->acc_lay);
    st->acc_bb = umbra_basic_block(b);
    umbra_builder_free(b);

    b = umbra_draw_build(umbra_shader_solid, umbra_coverage_wind, umbra_blend_srcover, s->fmt,
                         &st->draw_lay);
    st->draw_bb = umbra_basic_block(b);
    umbra_builder_free(b);

    slide_perspective_matrix(st->mat, 0.0f, w, h, (int)st->slug->w, (int)st->slug->h);
    st->mat[9]  = st->slug->w;
    st->mat[10] = st->slug->h;
}

static void slug_animate(struct slide *s, float dt) {
    struct slug_state *st = (struct slug_state *)s;
    (void)dt;
    st->persp_t += 0.016f;
    slide_perspective_matrix(st->mat, st->persp_t, st->w, st->h, (int)st->slug->w,
                             (int)st->slug->h);
    st->mat[9] = st->slug->w;
    st->mat[10] = st->slug->h;
}

static void slug_prepare(struct slide *s, int w, int h, struct umbra_backend *be) {
    (void)w; (void)h;
    struct slug_state *st = (struct slug_state *)s;
    if (st->acc_prog) { st->acc_prog->free(st->acc_prog); }
    st->acc_prog = be->compile(be, st->acc_bb);
    if (st->draw_prog) { st->draw_prog->free(st->draw_prog); }
    st->draw_prog = be->compile(be, st->draw_bb);
}

static void slug_draw(struct slide *s, int w, int h, int y0, int y1, void *buf) {
    struct slug_state           *st = (struct slug_state *)s;
    struct umbra_backend *be = st->draw_prog->backend;
    struct umbra_program *acc = st->acc_prog;

    size_t wind_sz  = (size_t)w * (size_t)h * sizeof(float);
    size_t wind_row = (size_t)w * sizeof(float);
    __builtin_memset((char *)st->wind_buf + (size_t)y0 * wind_row, 0,
                     (size_t)(y1 - y0) * wind_row);

    umbra_uniforms_fill_f32(st->acc_lay.uni, st->acc_lay.mat, st->mat, 11);
    umbra_uniforms_fill_ptr(st->acc_lay.uni, st->acc_lay.curves_off,
                  (struct umbra_buf){.ptr=st->slug->data, .sz=(size_t)(st->slug->count * 6 * 4), .read_only=1});
    struct umbra_buf abuf[] = {
        (struct umbra_buf){.ptr=st->acc_lay.uni->data, .sz=st->acc_lay.uni->size, .read_only=1},
        {.ptr=st->wind_buf, .sz=wind_sz, .row_bytes=wind_row},
    };
    for (int j = 0; j < st->slug->count; j++) {
        float jf;
        int32_t j32 = j;
        __builtin_memcpy(&jf, &j32, 4);
        umbra_uniforms_fill_f32(st->acc_lay.uni, st->acc_lay.loop_off, &jf, 1);
        abuf[0] = (struct umbra_buf){.ptr=st->acc_lay.uni->data, .sz=st->acc_lay.uni->size, .read_only=1};
        acc->queue(acc, 0, y0, w, y1, abuf);
    }
    be->flush(be);

    umbra_uniforms_fill_f32(st->draw_lay.uni, st->draw_lay.shader, st->color, 4);
    umbra_uniforms_fill_ptr(st->draw_lay.uni, st->draw_lay.coverage,
                  (struct umbra_buf){.ptr=st->wind_buf, .sz=wind_sz, .read_only=1, .row_bytes=(size_t)w * sizeof(float)});
    size_t    pb = umbra_fmt_size(s->fmt);
    size_t plane_sz = (size_t)w * (size_t)h * pb;
    struct umbra_buf rbuf[2];
    size_t rb = (size_t)w * pb;
    rbuf[0] = (struct umbra_buf){.ptr=st->draw_lay.uni->data, .sz=st->draw_lay.uni->size, .read_only=1};
    rbuf[1] = (struct umbra_buf){.ptr=buf, .sz=plane_sz * (s->fmt == umbra_fmt_fp16_planar ? 4 : 1), .row_bytes=rb};
    st->draw_prog->queue(st->draw_prog, 0, y0, w, y1, rbuf);
}

static struct umbra_basic_block *slug_get_bb(struct slide *s) {
    return ((struct slug_state *)s)->draw_bb;
}

static void slug_slide_free(struct slide *s) {
    struct slug_state *st = (struct slug_state *)s;
    free(st->wind_buf);
    if (st->acc_prog) { st->acc_prog->free(st->acc_prog); st->acc_prog = 0; }
    umbra_basic_block_free(st->acc_bb);
    if (st->acc_lay.uni) { free(st->acc_lay.uni->data); free(st->acc_lay.uni); }
    if (st->draw_prog) { st->draw_prog->free(st->draw_prog); }
    umbra_basic_block_free(st->draw_bb);
    if (st->draw_lay.uni) { free(st->draw_lay.uni->data); free(st->draw_lay.uni); }
    free(st);
}

struct slide *slide_slug_wind(struct slug_curves *);

struct slide *slide_slug_wind(struct slug_curves *sc) {
    struct slug_state *st = calloc(1, sizeof *st);
    st->slug = sc;
    st->color[0] = 0.2f;
    st->color[1] = 1.0f;
    st->color[2] = 0.6f;
    st->color[3] = 1.0f;
    st->base = (struct slide){
        .title = "14. Slug Text (Bezier)",
        .fmt = umbra_fmt_8888,
        .bg = 0xff0a0a1e,
        .init = slug_init,
        .animate = slug_animate,
        .prepare = slug_prepare,
        .draw = slug_draw,
        .free = slug_slide_free,
        .get_bb = slug_get_bb,
    };
    return &st->base;
}
