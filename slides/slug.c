#include "slide.h"
#include "slug.h"
#include <stdio.h>
#include <stdlib.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "../third_party/stb/stb_truetype.h"
#pragma clang diagnostic pop

struct slug_curves slug_extract(char const *text, float font_size) {
    struct slug_curves sc = {0};

    FILE *fp = fopen(
        "/System/Library/Fonts/Supplemental"
        "/Arial.ttf", "rb");
    if (!fp) { return sc; }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unsigned char *fd = malloc((size_t)sz);
    if (fread(fd, 1, (size_t)sz, fp) != (size_t)sz) {
        free(fd); fclose(fp); return sc;
    }
    fclose(fp);

    stbtt_fontinfo font;
    stbtt_InitFont(&font, fd,
        stbtt_GetFontOffsetForIndex(fd, 0));
    float scale =
        stbtt_ScaleForPixelHeight(&font, font_size);

    int asc, des, gap;
    stbtt_GetFontVMetrics(&font, &asc, &des, &gap);

    int total = 0;
    for (int i = 0; text[i]; i++) {
        stbtt_vertex *v;
        int nv = stbtt_GetCodepointShape(
            &font, text[i], &v);
        for (int j = 0; j < nv; j++) {
            if (v[j].type == STBTT_vline
             || v[j].type == STBTT_vcurve) {
                total++;
            }
        }
        stbtt_FreeShape(&font, v);
    }

    sc.data = malloc((size_t)total * 6
                     * sizeof(float));
    float baseline = (float)asc * scale;
    float cx = 0;
    float lx = 1e9f, ly = 1e9f;
    float hx = -1e9f, hy = -1e9f;

    for (int i = 0; text[i]; i++) {
        stbtt_vertex *v;
        int nv = stbtt_GetCodepointShape(
            &font, text[i], &v);
        float px = 0, py = 0;
        for (int j = 0; j < nv; j++) {
            float vx = cx + (float)v[j].x * scale;
            float vy = baseline
                      - (float)v[j].y * scale;
            if (v[j].type == STBTT_vmove) {
                px = vx; py = vy;
            } else if (v[j].type == STBTT_vline) {
                float *d = sc.data + sc.count * 6;
                d[0] = px; d[1] = py;
                d[2] = (px+vx)*0.5f;
                d[3] = (py+vy)*0.5f;
                d[4] = vx; d[5] = vy;
                sc.count++;
                if (px < lx) { lx = px; }
                if (vx < lx) { lx = vx; }
                if (px > hx) { hx = px; }
                if (vx > hx) { hx = vx; }
                if (py < ly) { ly = py; }
                if (vy < ly) { ly = vy; }
                if (py > hy) { hy = py; }
                if (vy > hy) { hy = vy; }
                px = vx; py = vy;
            } else {
                float kx = cx
                    + (float)v[j].cx * scale;
                float ky = baseline
                    - (float)v[j].cy * scale;
                float *d = sc.data + sc.count * 6;
                d[0] = px; d[1] = py;
                d[2] = kx; d[3] = ky;
                d[4] = vx; d[5] = vy;
                sc.count++;
                if (kx < lx) { lx = kx; }
                if (kx > hx) { hx = kx; }
                if (ky < ly) { ly = ky; }
                if (ky > hy) { hy = ky; }
                if (px < lx) { lx = px; }
                if (px > hx) { hx = px; }
                if (py < ly) { ly = py; }
                if (py > hy) { hy = py; }
                if (vx < lx) { lx = vx; }
                if (vx > hx) { hx = vx; }
                if (vy < ly) { ly = vy; }
                if (vy > hy) { hy = vy; }
                px = vx; py = vy;
            }
        }
        stbtt_FreeShape(&font, v);

        int adv, lsb;
        stbtt_GetCodepointHMetrics(
            &font, text[i], &adv, &lsb);
        cx += (float)adv * scale;
        if (text[i+1]) {
            int kern = stbtt_GetCodepointKernAdvance(
                &font, text[i], text[i+1]);
            cx += (float)kern * scale;
        }
    }

    lx -= 1.0f; ly -= 1.0f;
    hx += 1.0f; hy += 1.0f;
    for (int i = 0; i < sc.count; i++) {
        float *d = sc.data + i * 6;
        d[0] -= lx; d[1] -= ly;
        d[2] -= lx; d[3] -= ly;
        d[4] -= lx; d[5] -= ly;
    }
    sc.w = hx - lx;
    sc.h = hy - ly;

    free(fd);
    return sc;
}

void slug_free(struct slug_curves *sc) {
    free(sc->data);
    *sc = (struct slug_curves){0};
}

struct umbra_builder *slug_build_acc(struct slug_acc_layout *lay) {
    struct umbra_builder *b = umbra_builder();

    struct umbra_uniforms_layout u = {0};
    int fi = umbra_uniforms_reserve_f32(&u, 11);
    int co = umbra_uniforms_reserve_ptr(&u);
    umbra_ptr32 curves = umbra_deref_ptr32(b,
        (umbra_ptr32){0}, co);
    int ji = umbra_uniforms_reserve_f32(&u, 1);

    umbra_val32 xf = umbra_f32_from_i32(b, umbra_x(b));
    umbra_val32 yf = umbra_f32_from_i32(b, umbra_y(b));

    umbra_val32 m[9];
    for (int i = 0; i < 9; i++) {
        m[i] = umbra_uniform_32(b, (umbra_ptr32){0}, fi + i);
    }
    umbra_val32 bw = umbra_uniform_32(b, (umbra_ptr32){0}, fi + 9);
    umbra_val32 bh = umbra_uniform_32(b, (umbra_ptr32){0}, fi + 10);

    umbra_val32 pw = umbra_add_f32(b,
        umbra_add_f32(b,
            umbra_mul_f32(b, m[6], xf),
            umbra_mul_f32(b, m[7], yf)), m[8]);
    umbra_val32 gx = umbra_div_f32(b,
        umbra_add_f32(b,
            umbra_add_f32(b,
                umbra_mul_f32(b, m[0], xf),
                umbra_mul_f32(b, m[1], yf)),
            m[2]), pw);
    umbra_val32 gy = umbra_div_f32(b,
        umbra_add_f32(b,
            umbra_add_f32(b,
                umbra_mul_f32(b, m[3], xf),
                umbra_mul_f32(b, m[4], yf)),
            m[5]), pw);

    umbra_val32 z  = umbra_imm_f32(b, 0.0f);
    umbra_val32 o  = umbra_imm_f32(b, 1.0f);
    umbra_val32 tw = umbra_imm_f32(b, 2.0f);
    umbra_val32 ep = umbra_imm_f32(b, 1.0f/65536.0f);

    umbra_val32 in = umbra_and_32(b,
        umbra_and_32(b,
            umbra_le_f32(b, z, gx),
            umbra_lt_f32(b, gx, bw)),
        umbra_and_32(b,
            umbra_le_f32(b, z, gy),
            umbra_lt_f32(b, gy, bh)));

    umbra_val32 j = umbra_uniform_32(b, (umbra_ptr32){0}, ji);
    umbra_val32 k = umbra_mul_i32(b, j,
                      umbra_imm_i32(b, 6));

    umbra_val32 p0x = umbra_gather_32(b, curves, k);
    umbra_val32 p0y = umbra_gather_32(b, curves,
        umbra_add_i32(b, k, umbra_imm_i32(b, 1)));
    umbra_val32 p1x = umbra_gather_32(b, curves,
        umbra_add_i32(b, k, umbra_imm_i32(b, 2)));
    umbra_val32 p1y = umbra_gather_32(b, curves,
        umbra_add_i32(b, k, umbra_imm_i32(b, 3)));
    umbra_val32 p2x = umbra_gather_32(b, curves,
        umbra_add_i32(b, k, umbra_imm_i32(b, 4)));
    umbra_val32 p2y = umbra_gather_32(b, curves,
        umbra_add_i32(b, k, umbra_imm_i32(b, 5)));

    umbra_val32 q0y = umbra_sub_f32(b, p0y, gy);
    umbra_val32 q1y = umbra_sub_f32(b, p1y, gy);
    umbra_val32 q2y = umbra_sub_f32(b, p2y, gy);

    umbra_val32 a = umbra_add_f32(b,
        umbra_sub_f32(b, q0y,
            umbra_mul_f32(b, tw, q1y)), q2y);
    umbra_val32 bv = umbra_sub_f32(b, q0y, q1y);

    umbra_val32 disc = umbra_sub_f32(b,
        umbra_mul_f32(b, bv, bv),
        umbra_mul_f32(b, a, q0y));
    umbra_val32 ok = umbra_le_f32(b, z, disc);
    umbra_val32 sd = umbra_sqrt_f32(b,
        umbra_max_f32(b, disc, z));

    umbra_val32 abs_a = umbra_abs_f32(b, a);
    umbra_val32 is_quad = umbra_lt_f32(b, ep, abs_a);

    umbra_val32 ia = umbra_div_f32(b, o,
        umbra_sel_32(b, is_quad, a, o));

    umbra_val32 qt1 = umbra_mul_f32(b,
        umbra_sub_f32(b, bv, sd), ia);
    umbra_val32 qt2 = umbra_mul_f32(b,
        umbra_add_f32(b, bv, sd), ia);

    umbra_val32 abs_bv = umbra_abs_f32(b, bv);
    umbra_val32 lt = umbra_div_f32(b, q0y,
        umbra_sel_32(b,
            umbra_lt_f32(b, ep, abs_bv),
            umbra_mul_f32(b, tw, bv), o));

    umbra_val32 t1 = umbra_sel_32(b,
        is_quad, qt1, lt);
    umbra_val32 t2 = qt2;

    umbra_val32 t1ok = umbra_and_32(b, ok,
        umbra_and_32(b,
            umbra_le_f32(b, z, t1),
            umbra_lt_f32(b, t1, o)));
    umbra_val32 t2ok = umbra_and_32(b,
        umbra_and_32(b, ok, is_quad),
        umbra_and_32(b,
            umbra_le_f32(b, z, t2),
            umbra_lt_f32(b, t2, o)));

    umbra_val32 q0x = umbra_sub_f32(b, p0x, gx);
    umbra_val32 q1x = umbra_sub_f32(b, p1x, gx);
    umbra_val32 q2x = umbra_sub_f32(b, p2x, gx);
    umbra_val32 ax = umbra_add_f32(b,
        umbra_sub_f32(b, q0x,
            umbra_mul_f32(b, tw, q1x)), q2x);
    umbra_val32 bx = umbra_mul_f32(b, tw,
        umbra_sub_f32(b, q1x, q0x));

    umbra_val32 x1 = umbra_add_f32(b,
        umbra_mul_f32(b,
            umbra_add_f32(b,
                umbra_mul_f32(b, ax, t1), bx),
            t1), q0x);
    umbra_val32 x2 = umbra_add_f32(b,
        umbra_mul_f32(b,
            umbra_add_f32(b,
                umbra_mul_f32(b, ax, t2), bx),
            t2), q0x);

    umbra_val32 dy1 = umbra_sub_f32(b,
        umbra_mul_f32(b, a, t1), bv);
    umbra_val32 dy2 = umbra_sub_f32(b,
        umbra_mul_f32(b, a, t2), bv);

    umbra_val32 po = umbra_imm_f32(b, 1.0f);
    umbra_val32 no = umbra_imm_f32(b, -1.0f);

    umbra_val32 dir1 = umbra_sel_32(b,
        umbra_lt_f32(b, z, dy1), po,
        umbra_sel_32(b,
            umbra_lt_f32(b, dy1, z), no, z));
    umbra_val32 dir2 = umbra_sel_32(b,
        umbra_lt_f32(b, z, dy2), po,
        umbra_sel_32(b,
            umbra_lt_f32(b, dy2, z), no, z));

    umbra_val32 w1 = umbra_sel_32(b,
        umbra_and_32(b, t1ok,
            umbra_lt_f32(b, z, x1)),
        dir1, z);
    umbra_val32 w2 = umbra_sel_32(b,
        umbra_and_32(b, t2ok,
            umbra_lt_f32(b, z, x2)),
        dir2, z);

    umbra_val32 dw = umbra_sel_32(b, in,
        umbra_add_f32(b, w1, w2), z);

    umbra_val32 acc = umbra_load_32(b, (umbra_ptr32){.ix=1});
    acc = umbra_add_f32(b, acc, dw);
    umbra_store_32(b, (umbra_ptr32){.ix=1}, acc);

    if (lay) {
        lay->mat        = fi;
        lay->curves_off = co;
        lay->loop_off   = ji;
        lay->uni        = u;
        lay->uniforms   = umbra_uniforms_alloc(&u);
    }

    return b;
}

struct slug_state {
    struct slide base;

    struct slug_curves        slug;
    int                       w, h;
    float                    *wind_buf;
    struct slug_acc_layout    acc_lay;
    struct umbra_basic_block *acc_bb;
    struct umbra_program     *acc_prog;

    struct umbra_shader_solid   shader;
    struct umbra_coverage_wind  cov;
    struct umbra_fmt            fmt;
    struct umbra_draw_layout    draw_lay;
    struct umbra_basic_block   *draw_bb;
    struct umbra_program       *draw_prog;
};

static void slug_init(struct slide *s, int w, int h) {
    struct slug_state *st = (struct slug_state *)s;
    st->w = w;
    st->h = h;
    st->slug = slug_extract("Slug", (float)h * 0.3125f);
    st->wind_buf = malloc((size_t)w * (size_t)h * sizeof(float));

    struct umbra_builder *b = slug_build_acc(&st->acc_lay);
    st->acc_bb = umbra_basic_block(b);
    umbra_builder_free(b);
}

static void slug_prepare(struct slide *s, struct umbra_backend *be, struct umbra_fmt fmt) {
    struct slug_state *st = (struct slug_state *)s;
    if (st->fmt.name != fmt.name || !st->draw_bb) {
        st->fmt = fmt;
        umbra_basic_block_free(st->draw_bb);
        free(st->draw_lay.uniforms);
        struct umbra_builder *b = umbra_draw_build(&st->shader.base, &st->cov.base,
                                                    umbra_blend_srcover, fmt, &st->draw_lay);
        st->draw_bb = umbra_basic_block(b);
        umbra_builder_free(b);
    }
    if (st->acc_prog) { st->acc_prog->free(st->acc_prog); }
    st->acc_prog = be->compile(be, st->acc_bb);
    if (st->draw_prog) { st->draw_prog->free(st->draw_prog); }
    st->draw_prog = be->compile(be, st->draw_bb);
}

static void slug_draw(struct slide *s, int frame, int l, int t, int r, int b, void *buf) {
    struct slug_state           *st = (struct slug_state *)s;
    struct umbra_program *acc = st->acc_prog;
    int w = st->w, h = st->h;

    size_t const wind_row = (size_t)w * sizeof(float);
    __builtin_memset((char *)st->wind_buf + (size_t)t * wind_row, 0,
                     (size_t)(b - t) * wind_row);

    float mat[11];
    slide_perspective_matrix(mat, (float)frame * 0.016f, w, h,
                             (int)st->slug.w, (int)st->slug.h);
    mat[9]  = st->slug.w;
    mat[10] = st->slug.h;
    umbra_uniforms_fill_f32(st->acc_lay.uniforms, st->acc_lay.mat, mat, 11);
    umbra_uniforms_fill_ptr(st->acc_lay.uniforms, st->acc_lay.curves_off,
                  (struct umbra_buf){.ptr=st->slug.data, .count=st->slug.count * 6});
    struct umbra_buf abuf[] = {
        (struct umbra_buf){.ptr=st->acc_lay.uniforms, .count=st->acc_lay.uni.slots},
        {.ptr=st->wind_buf, .count=w * h, .stride=w},
    };
    for (int j = 0; j < st->slug.count; j++) {
        float jf;
        int32_t j32 = j;
        __builtin_memcpy(&jf, &j32, 4);
        umbra_uniforms_fill_f32(st->acc_lay.uniforms, st->acc_lay.loop_off, &jf, 1);
        acc->queue(acc, l, t, r, b, abuf);
    }

    st->cov.wind = (struct umbra_buf){
        .ptr    = st->wind_buf,
        .count  = w * h,
        .stride = w,
    };
    umbra_draw_fill(&st->draw_lay, &st->shader.base, &st->cov.base);
    struct umbra_buf rbuf[2];
    rbuf[0] = (struct umbra_buf){.ptr = st->draw_lay.uniforms,
                                 .count = st->draw_lay.uni.slots};
    rbuf[1] = (struct umbra_buf){.ptr=buf, .count=w * h * st->fmt.planes, .stride=w};
    st->draw_prog->queue(st->draw_prog, l, t, r, b, rbuf);
}

static struct umbra_builder *slug_get_builder(struct slide *s, struct umbra_fmt fmt) {
    struct slug_state *st = (struct slug_state *)s;
    return umbra_draw_build(&st->shader.base, &st->cov.base, umbra_blend_srcover, fmt, NULL);
}

static void slug_slide_free(struct slide *s) {
    struct slug_state *st = (struct slug_state *)s;
    slug_free(&st->slug);
    free(st->wind_buf);
    if (st->acc_prog) { st->acc_prog->free(st->acc_prog); st->acc_prog = 0; }
    umbra_basic_block_free(st->acc_bb);
    free(st->acc_lay.uniforms);
    if (st->draw_prog) { st->draw_prog->free(st->draw_prog); }
    umbra_basic_block_free(st->draw_bb);
    free(st->draw_lay.uniforms);
    free(st);
}

SLIDE(slide_slug_wind) {
    struct slug_state *st = calloc(1, sizeof *st);
    st->shader = umbra_shader_solid((float[]){0.2f, 1.0f, 0.6f, 1.0f});
    st->cov    = umbra_coverage_wind((struct umbra_buf){0});
    st->base = (struct slide){
        .title = "Slug Text (Bezier)",
        .bg = {0.12f, 0.04f, 0.04f, 1},
        .init = slug_init,
        .prepare = slug_prepare,
        .draw = slug_draw,
        .free = slug_slide_free,
        .get_builder = slug_get_builder,
    };
    return &st->base;
}
