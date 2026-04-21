#include "slide.h"
#include "slug.h"
#include <stdio.h>
#include <stdlib.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "../third_party/stb/stb_truetype.h"
#pragma clang diagnostic pop

#define UNI_SLOT(type, field) \
    ((int)(__builtin_offsetof(type, field) / 4))

struct slug_cov_uniforms {
    float bw, bh;
    int   n_curves;
    int   :32;
};

struct slug_cov_ctx {
    struct umbra_buf         const *curves;
    struct slug_cov_uniforms const *uni;
};

// Two-ray winding coverage following the Slug algorithm
// (https://github.com/EricLengyel/Slug, https://terathon.com/i3d2018_lengyel.pdf).
// Cast both a horizontal ray (solve y(t)=0, evaluate x(t)) and a vertical
// ray (solve x(t)=0, evaluate y(t)) from each sample.  Root eligibility
// comes from the 0x2E74 sign-bit lookup over (q0s, q1s, q2s) -- bit 0 picks
// t1, bit 8 picks t2.  Per-ray partial coverage is sum of saturate(e + 0.5)
// with Slug's sign convention; per-ray weight is max saturate(1 - 2|e|)
// across contributing roots.  The combine takes the weighted mean (high
// coverage when either ray is cleanly centered) and falls back to
// min(|xcov|,|ycov|) so a near-tangent ray can't drag coverage to zero.
struct slug_consts {
    umbra_val32 z, o, tw, half, ep;
    umbra_val32 sb31, i_one, i_two, i_8, lut;
};

static void slug_ray(struct umbra_builder *b, struct slug_consts const *c,
                     umbra_val32 q0s, umbra_val32 q1s, umbra_val32 q2s,
                     umbra_val32 q0e, umbra_val32 q1e, umbra_val32 q2e,
                     umbra_val32 sign_t1, umbra_val32 sign_t2,
                     umbra_var32 acc, umbra_var32 wgt) {
    umbra_val32 const z = c->z, o = c->o, tw = c->tw, half = c->half, ep = c->ep;

    umbra_val32 s1 = umbra_shr_u32(b, q0s, c->sb31),
                s2 = umbra_shr_u32(b, q1s, c->sb31),
                s3 = umbra_shr_u32(b, q2s, c->sb31);
    umbra_val32 shift = umbra_or_32(b, s1,
                        umbra_or_32(b, umbra_shl_i32(b, s2, c->i_one),
                                       umbra_shl_i32(b, s3, c->i_two)));
    umbra_val32 code = umbra_shr_u32(b, c->lut, shift);
    umbra_val32 t1ok = umbra_eq_i32(b, umbra_and_32(b, code, c->i_one), c->i_one);
    umbra_val32 t2ok = umbra_eq_i32(b, umbra_and_32(b, umbra_shr_u32(b, code, c->i_8),
                                                      c->i_one), c->i_one);

    umbra_val32 a    = umbra_add_f32(b, umbra_sub_f32(b, q0s, umbra_mul_f32(b, tw, q1s)), q2s);
    umbra_val32 bv   = umbra_sub_f32(b, q0s, q1s);
    umbra_val32 disc = umbra_sub_f32(b, umbra_mul_f32(b, bv, bv),
                                        umbra_mul_f32(b, a, q0s));
    umbra_val32 sd   = umbra_sqrt_f32(b, umbra_max_f32(b, disc, z));

    umbra_val32 is_linear = umbra_le_f32(b, umbra_abs_f32(b, a), ep);
    umbra_val32 ra = umbra_div_f32(b, o, umbra_sel_32(b, is_linear, o, a));
    umbra_val32 rb = umbra_div_f32(b, half, bv);
    umbra_val32 qt1 = umbra_mul_f32(b, umbra_sub_f32(b, bv, sd), ra);
    umbra_val32 qt2 = umbra_mul_f32(b, umbra_add_f32(b, bv, sd), ra);
    umbra_val32 lt  = umbra_mul_f32(b, q0s, rb);
    umbra_val32 t1 = umbra_sel_32(b, is_linear, lt, qt1);
    umbra_val32 t2 = umbra_sel_32(b, is_linear, lt, qt2);

    umbra_val32 ae = umbra_add_f32(b, umbra_sub_f32(b, q0e, umbra_mul_f32(b, tw, q1e)), q2e);
    umbra_val32 be = umbra_mul_f32(b, tw, umbra_sub_f32(b, q1e, q0e));
    umbra_val32 e1 = umbra_add_f32(b,
                         umbra_mul_f32(b, umbra_add_f32(b, umbra_mul_f32(b, ae, t1), be), t1),
                         q0e);
    umbra_val32 e2 = umbra_add_f32(b,
                         umbra_mul_f32(b, umbra_add_f32(b, umbra_mul_f32(b, ae, t2), be), t2),
                         q0e);

    umbra_val32 sat1 = umbra_min_f32(b, umbra_max_f32(b, umbra_add_f32(b, e1, half), z), o);
    umbra_val32 sat2 = umbra_min_f32(b, umbra_max_f32(b, umbra_add_f32(b, e2, half), z), o);
    umbra_val32 c1   = umbra_sel_32(b, t1ok, umbra_mul_f32(b, sign_t1, sat1), z);
    umbra_val32 c2   = umbra_sel_32(b, t2ok, umbra_mul_f32(b, sign_t2, sat2), z);
    umbra_val32 dacc = umbra_add_f32(b, c1, c2);

    umbra_val32 wk1 = umbra_min_f32(b, umbra_max_f32(b,
                          umbra_sub_f32(b, o, umbra_mul_f32(b, tw, umbra_abs_f32(b, e1))),
                          z), o);
    umbra_val32 wk2 = umbra_min_f32(b, umbra_max_f32(b,
                          umbra_sub_f32(b, o, umbra_mul_f32(b, tw, umbra_abs_f32(b, e2))),
                          z), o);
    umbra_val32 dwgt = umbra_max_f32(b, umbra_sel_32(b, t1ok, wk1, z),
                                        umbra_sel_32(b, t2ok, wk2, z));

    umbra_store_var32(b, acc, umbra_add_f32(b, umbra_load_var32(b, acc), dacc));
    umbra_store_var32(b, wgt, umbra_max_f32(b, umbra_load_var32(b, wgt), dwgt));
}

static umbra_val32 coverage_slug_winding(void *vctx, struct umbra_builder *b,
                                         umbra_val32 x, umbra_val32 y) {
    struct slug_cov_ctx const *ctx = vctx;
    umbra_ptr const curves = umbra_bind_buf(b, ctx->curves);
    umbra_ptr const u      = umbra_bind_uniforms(b, ctx->uni,
                                                 (int)(sizeof *ctx->uni / 4));

    umbra_val32 const bw = umbra_uniform_32(b, u, UNI_SLOT(struct slug_cov_uniforms, bw)),
                      bh = umbra_uniform_32(b, u, UNI_SLOT(struct slug_cov_uniforms, bh)),
                      n  = umbra_uniform_32(b, u, UNI_SLOT(struct slug_cov_uniforms, n_curves));

    struct slug_consts const c = {
        .z     = umbra_imm_f32(b, 0.0f),
        .o     = umbra_imm_f32(b, 1.0f),
        .tw    = umbra_imm_f32(b, 2.0f),
        .half  = umbra_imm_f32(b, 0.5f),
        .ep    = umbra_imm_f32(b, 1.0f/65536.0f),
        .sb31  = umbra_imm_i32(b, 31),
        .i_one = umbra_imm_i32(b, 1),
        .i_two = umbra_imm_i32(b, 2),
        .i_8   = umbra_imm_i32(b, 8),
        .lut   = umbra_imm_i32(b, 0x2E74),
    };
    umbra_val32 const po = umbra_imm_f32(b, +1.0f),
                      no = umbra_imm_f32(b, -1.0f);

    umbra_val32 const in = umbra_and_32(b,
        umbra_and_32(b, umbra_le_f32(b, c.z, x), umbra_lt_f32(b, x, bw)),
        umbra_and_32(b, umbra_le_f32(b, c.z, y), umbra_lt_f32(b, y, bh)));

    umbra_var32 const xacc = umbra_declare_var32(b),
                      yacc = umbra_declare_var32(b),
                      xwgt = umbra_declare_var32(b),
                      ywgt = umbra_declare_var32(b);

    umbra_val32 j = umbra_loop(b, n); {
        umbra_val32 k = umbra_mul_i32(b, j, umbra_imm_i32(b, 6));

        umbra_val32 p0x = umbra_gather_32(b, curves, k),
                    p0y = umbra_gather_32(b, curves, umbra_add_i32(b, k, c.i_one)),
                    p1x = umbra_gather_32(b, curves, umbra_add_i32(b, k, c.i_two)),
                    p1y = umbra_gather_32(b, curves, umbra_add_i32(b, k, umbra_imm_i32(b, 3))),
                    p2x = umbra_gather_32(b, curves, umbra_add_i32(b, k, umbra_imm_i32(b, 4))),
                    p2y = umbra_gather_32(b, curves, umbra_add_i32(b, k, umbra_imm_i32(b, 5)));

        umbra_val32 q0x = umbra_sub_f32(b, p0x, x),
                    q1x = umbra_sub_f32(b, p1x, x),
                    q2x = umbra_sub_f32(b, p2x, x),
                    q0y = umbra_sub_f32(b, p0y, y),
                    q1y = umbra_sub_f32(b, p1y, y),
                    q2y = umbra_sub_f32(b, p2y, y);

        slug_ray(b, &c, q0y, q1y, q2y, q0x, q1x, q2x, po, no, xacc, xwgt);
        slug_ray(b, &c, q0x, q1x, q2x, q0y, q1y, q2y, no, po, yacc, ywgt);
    } umbra_end_loop(b);

    umbra_val32 const xa = umbra_load_var32(b, xacc),
                      ya = umbra_load_var32(b, yacc),
                      xw = umbra_load_var32(b, xwgt),
                      yw = umbra_load_var32(b, ywgt);

    umbra_val32 const num = umbra_abs_f32(b, umbra_add_f32(b,
                                umbra_mul_f32(b, xa, xw),
                                umbra_mul_f32(b, ya, yw)));
    umbra_val32 const den = umbra_max_f32(b, umbra_add_f32(b, xw, yw), c.ep);
    umbra_val32 const avg = umbra_div_f32(b, num, den);
    umbra_val32 const fallback = umbra_min_f32(b, umbra_abs_f32(b, xa), umbra_abs_f32(b, ya));
    umbra_val32 const cov = umbra_min_f32(b, umbra_max_f32(b, avg, fallback), c.o);

    return umbra_sel_32(b, in, cov, c.z);
}

struct slug_curves slug_extract(char const *text, float font_size) {
    struct slug_curves sc = {0};

    FILE *fp = fopen("/System/Library/Fonts/Supplemental/Arial.ttf", "rb");
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
    stbtt_InitFont(&font, fd, stbtt_GetFontOffsetForIndex(fd, 0));
    float scale = stbtt_ScaleForPixelHeight(&font, font_size);

    int asc, des, gap;
    stbtt_GetFontVMetrics(&font, &asc, &des, &gap);

    int total = 0;
    for (int i = 0; text[i]; i++) {
        stbtt_vertex *v;
        int nv = stbtt_GetCodepointShape(&font, text[i], &v);
        for (int j = 0; j < nv; j++) {
            if (v[j].type == STBTT_vline || v[j].type == STBTT_vcurve) {
                total++;
            }
        }
        stbtt_FreeShape(&font, v);
    }

    sc.data = malloc((size_t)total * 6 * sizeof(float));
    float baseline = (float)asc * scale;
    float cx = 0;
    float lx = 1e9f, ly = 1e9f;
    float hx = -1e9f, hy = -1e9f;

    for (int i = 0; text[i]; i++) {
        stbtt_vertex *v;
        int nv = stbtt_GetCodepointShape(&font, text[i], &v);
        float px = 0, py = 0;
        for (int j = 0; j < nv; j++) {
            float vx = cx + (float)v[j].x * scale;
            float vy = baseline - (float)v[j].y * scale;
            if (v[j].type == STBTT_vmove) {
                px = vx; py = vy;
            } else if (v[j].type == STBTT_vline) {
                float *d = sc.data + sc.count * 6;
                d[0] = px; d[1] = py;
                d[2] = (px + vx) * 0.5f;
                d[3] = (py + vy) * 0.5f;
                d[4] = vx; d[5] = vy;
                sc.count++;
                if (px < lx) { lx = px; } if (vx < lx) { lx = vx; }
                if (px > hx) { hx = px; } if (vx > hx) { hx = vx; }
                if (py < ly) { ly = py; } if (vy < ly) { ly = vy; }
                if (py > hy) { hy = py; } if (vy > hy) { hy = vy; }
                px = vx; py = vy;
            } else {
                float kx = cx + (float)v[j].cx * scale;
                float ky = baseline - (float)v[j].cy * scale;
                float *d = sc.data + sc.count * 6;
                d[0] = px; d[1] = py;
                d[2] = kx; d[3] = ky;
                d[4] = vx; d[5] = vy;
                sc.count++;
                if (kx < lx) { lx = kx; } if (kx > hx) { hx = kx; }
                if (ky < ly) { ly = ky; } if (ky > hy) { hy = ky; }
                if (px < lx) { lx = px; } if (px > hx) { hx = px; }
                if (py < ly) { ly = py; } if (py > hy) { hy = py; }
                if (vx < lx) { lx = vx; } if (vx > hx) { hx = vx; }
                if (vy < ly) { ly = vy; } if (vy > hy) { hy = vy; }
                px = vx; py = vy;
            }
        }
        stbtt_FreeShape(&font, v);

        int adv, lsb;
        stbtt_GetCodepointHMetrics(&font, text[i], &adv, &lsb);
        cx += (float)adv * scale;
        if (text[i+1]) {
            int kern = stbtt_GetCodepointKernAdvance(&font, text[i], text[i+1]);
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

struct slug_slide {
    struct slide base;

    struct slug_curves       slug;
    struct umbra_buf         curves_buf;
    struct slug_cov_uniforms cov_uni;
    struct slug_cov_ctx      cov_ctx;
    struct umbra_matrix      mat; int :32;

    umbra_color              color;
};

static void slug_init(struct slide *s) {
    struct slug_slide *st = (struct slug_slide *)s;
    st->slug = slug_extract("Slug", (float)s->h * 0.3125f);
    st->curves_buf = (struct umbra_buf){
        .ptr = st->slug.data, .count = st->slug.count * 6,
    };
    st->cov_uni = (struct slug_cov_uniforms){
        .bw = st->slug.w, .bh = st->slug.h, .n_curves = st->slug.count,
    };
    st->cov_ctx = (struct slug_cov_ctx){
        .curves = &st->curves_buf, .uni = &st->cov_uni,
    };
}

static void slug_build_draw(struct slide *s, struct umbra_builder *b,
                            umbra_ptr dst_ptr, struct umbra_fmt fmt,
                            umbra_val32 x, umbra_val32 y) {
    struct slug_slide *st = (struct slug_slide *)s;
    umbra_point_val32 const p = umbra_transform_perspective(&st->mat, b, x, y);
    umbra_build_draw(b, dst_ptr, fmt, p.x, p.y,
                     coverage_slug_winding, &st->cov_ctx,
                     umbra_shader_color,    &st->color,
                     umbra_blend_srcover,   NULL);
}

static void slug_animate(struct slide *s, double secs) {
    struct slug_slide *st = (struct slug_slide *)s;
    slide_perspective_matrix(&st->mat, (float)secs, s->w, s->h,
                             (int)st->slug.w, (int)st->slug.h);
}

static void slug_free_slide(struct slide *s) {
    struct slug_slide *st = (struct slug_slide *)s;
    slug_free(&st->slug);
    free(st);
}

SLIDE(slide_slug) {
    struct slug_slide *st = calloc(1, sizeof *st);
    st->color = (umbra_color){0.2f, 1.0f, 0.6f, 1.0f};
    st->base = (struct slide){
        .title = "Slug",
        .bg = {0.12f, 0.04f, 0.04f, 1},
        .init = slug_init,
        .free = slug_free_slide,
        .build_draw   = slug_build_draw,
        .animate      = slug_animate,
    };
    return &st->base;
}
