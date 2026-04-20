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

// One-pass winding coverage: uniform-count loop over all curves, accumulate
// signed scanline crossings at (x, y), clamp |acc| into [0, 1].  Returns 0
// outside [0, bw) x [0, bh) so the loop work doesn't contribute outside the
// glyph bounds.  ctx is a struct slug_cov_ctx*.
//
// TODO: wrong at some animation steps and/or at large canvas sizes.  Observed
// by resizing the demo window: small sizes showed rare pixel flicker (easy to
// dismiss as noise); large sizes clearly mis-render slug past the animation
// step we capture in dumps/.  Every backend (interp, jit, metal, vulkan, wgpu)
// shows the same wrongness -- so it's not a backend issue, it's this winding
// algorithm.  Candidates to investigate:
//   - quadratic root conditioning when a is small but is_quad is still true
//     (the ep=1/65536 threshold may be too tight or too loose for the scaled
//     coordinates at large canvases).
//   - division by pw (perspective divide) when pw is near zero or negative,
//     which can happen off-screen at large matrix tilt angles.
//   - the half-open [z, o) interval for t1/t2 combined with the >0 test on x1/x2
//     can drop or double-count curves that pass exactly through the scanline
//     start.
//   - direction (dy1/dy2) sign picked at the exact root sometimes differs from
//     the actual curve-crossing sign, giving cancelling winding contributions
//     that shouldn't cancel.
static umbra_val32 coverage_slug_winding(void *vctx, struct umbra_builder *b,
                                         umbra_val32 x, umbra_val32 y) {
    struct slug_cov_ctx const *ctx = vctx;
    umbra_ptr const curves = umbra_bind_buf(b, ctx->curves);
    umbra_ptr const u      = umbra_bind_uniforms(b, ctx->uni,
                                                 (int)(sizeof *ctx->uni / 4));

    umbra_val32 const bw = umbra_uniform_32(b, u, UNI_SLOT(struct slug_cov_uniforms, bw)),
                      bh = umbra_uniform_32(b, u, UNI_SLOT(struct slug_cov_uniforms, bh)),
                      n  = umbra_uniform_32(b, u, UNI_SLOT(struct slug_cov_uniforms, n_curves));

    umbra_val32 const z  = umbra_imm_f32(b, 0.0f),
                      o  = umbra_imm_f32(b, 1.0f),
                      tw = umbra_imm_f32(b, 2.0f),
                      ep = umbra_imm_f32(b, 1.0f/65536.0f),
                      po = umbra_imm_f32(b, 1.0f),
                      no = umbra_imm_f32(b, -1.0f);

    umbra_val32 const in = umbra_and_32(b,
        umbra_and_32(b, umbra_le_f32(b, z, x), umbra_lt_f32(b, x, bw)),
        umbra_and_32(b, umbra_le_f32(b, z, y), umbra_lt_f32(b, y, bh)));

    umbra_var32 const acc = umbra_declare_var32(b);

    umbra_val32 j = umbra_loop(b, n); {
        umbra_val32 k = umbra_mul_i32(b, j, umbra_imm_i32(b, 6));

        umbra_val32 p0x = umbra_gather_32(b, curves, k),
                    p0y = umbra_gather_32(b, curves, umbra_add_i32(b, k, umbra_imm_i32(b, 1))),
                    p1x = umbra_gather_32(b, curves, umbra_add_i32(b, k, umbra_imm_i32(b, 2))),
                    p1y = umbra_gather_32(b, curves, umbra_add_i32(b, k, umbra_imm_i32(b, 3))),
                    p2x = umbra_gather_32(b, curves, umbra_add_i32(b, k, umbra_imm_i32(b, 4))),
                    p2y = umbra_gather_32(b, curves, umbra_add_i32(b, k, umbra_imm_i32(b, 5)));

        umbra_val32 q0y = umbra_sub_f32(b, p0y, y),
                    q1y = umbra_sub_f32(b, p1y, y),
                    q2y = umbra_sub_f32(b, p2y, y);

        umbra_val32 a  = umbra_add_f32(b, umbra_sub_f32(b, q0y, umbra_mul_f32(b, tw, q1y)), q2y);
        umbra_val32 bv = umbra_sub_f32(b, q0y, q1y);

        umbra_val32 disc = umbra_sub_f32(b, umbra_mul_f32(b, bv, bv),
                                            umbra_mul_f32(b, a, q0y));
        umbra_val32 ok = umbra_le_f32(b, z, disc);
        umbra_val32 sd = umbra_sqrt_f32(b, umbra_max_f32(b, disc, z));

        umbra_val32 abs_a   = umbra_abs_f32(b, a);
        umbra_val32 is_quad = umbra_lt_f32(b, ep, abs_a);

        umbra_val32 ia = umbra_div_f32(b, o, umbra_sel_32(b, is_quad, a, o));

        umbra_val32 qt1 = umbra_mul_f32(b, umbra_sub_f32(b, bv, sd), ia);
        umbra_val32 qt2 = umbra_mul_f32(b, umbra_add_f32(b, bv, sd), ia);

        umbra_val32 abs_bv = umbra_abs_f32(b, bv);
        umbra_val32 lt = umbra_div_f32(b, q0y,
                             umbra_sel_32(b, umbra_lt_f32(b, ep, abs_bv),
                                         umbra_mul_f32(b, tw, bv), o));

        umbra_val32 t1 = umbra_sel_32(b, is_quad, qt1, lt);
        umbra_val32 t2 = qt2;

        umbra_val32 t1ok = umbra_and_32(b, ok, umbra_and_32(b, umbra_le_f32(b, z, t1),
                                                                umbra_lt_f32(b, t1, o)));
        umbra_val32 t2ok = umbra_and_32(b, umbra_and_32(b, ok, is_quad),
                                           umbra_and_32(b, umbra_le_f32(b, z, t2),
                                                           umbra_lt_f32(b, t2, o)));

        umbra_val32 q0x = umbra_sub_f32(b, p0x, x),
                    q1x = umbra_sub_f32(b, p1x, x),
                    q2x = umbra_sub_f32(b, p2x, x);
        umbra_val32 ax = umbra_add_f32(b, umbra_sub_f32(b, q0x, umbra_mul_f32(b, tw, q1x)), q2x);
        umbra_val32 bx = umbra_mul_f32(b, tw, umbra_sub_f32(b, q1x, q0x));

        umbra_val32 x1 = umbra_add_f32(b,
                             umbra_mul_f32(b, umbra_add_f32(b, umbra_mul_f32(b, ax, t1), bx), t1),
                             q0x);
        umbra_val32 x2 = umbra_add_f32(b,
                             umbra_mul_f32(b, umbra_add_f32(b, umbra_mul_f32(b, ax, t2), bx), t2),
                             q0x);

        umbra_val32 dy1 = umbra_sub_f32(b, umbra_mul_f32(b, a, t1), bv);
        umbra_val32 dy2 = umbra_sub_f32(b, umbra_mul_f32(b, a, t2), bv);

        umbra_val32 dir1 = umbra_sel_32(b, umbra_lt_f32(b, z, dy1), po,
                               umbra_sel_32(b, umbra_lt_f32(b, dy1, z), no, z));
        umbra_val32 dir2 = umbra_sel_32(b, umbra_lt_f32(b, z, dy2), po,
                               umbra_sel_32(b, umbra_lt_f32(b, dy2, z), no, z));

        umbra_val32 w1 = umbra_sel_32(b, umbra_and_32(b, t1ok, umbra_lt_f32(b, z, x1)), dir1, z);
        umbra_val32 w2 = umbra_sel_32(b, umbra_and_32(b, t2ok, umbra_lt_f32(b, z, x2)), dir2, z);

        umbra_val32 dw = umbra_add_f32(b, w1, w2);

        umbra_store_var32(b, acc, umbra_add_f32(b, umbra_load_var32(b, acc), dw));
    } umbra_end_loop(b);

    umbra_val32 const wind = umbra_load_var32(b, acc);
    umbra_val32 const cov  = umbra_min_f32(b, umbra_abs_f32(b, wind), o);
    return umbra_sel_32(b, in, cov, z);
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
    int                      w, h;
    struct slug_cov_uniforms cov_uni;
    struct slug_cov_ctx      cov_ctx;
    struct umbra_matrix      mat; int :32;

    umbra_color              color;
};

static void slug_init(struct slide *s, int w, int h) {
    struct slug_slide *st = (struct slug_slide *)s;
    st->w = w;
    st->h = h;
    st->slug = slug_extract("Slug", (float)h * 0.3125f);
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
    slide_perspective_matrix(&st->mat, (float)secs, st->w, st->h,
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
