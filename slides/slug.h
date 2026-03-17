#pragma once
#include "../umbra_draw.h"
#include <stdlib.h>
#include <stdio.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "../stb/stb_truetype.h"
#pragma clang diagnostic pop

typedef struct {
    float *data;
    int    count;
    int    pad_;
    float  w, h;
} slug_curves;

static int slug_n_curves;

static slug_curves slug_extract(
        char const *text, float font_size) {
    slug_curves sc = {0};

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

static void slug_free(slug_curves *sc) {
    free(sc->data);
    *sc = (slug_curves){0};
}

static void slug_ray_(
        struct umbra_builder *b,
        umbra_ptr buf, int i,
        umbra_val gx, umbra_val gy,
        umbra_val *wind,
        umbra_val z, umbra_val o,
        umbra_val tw, umbra_val ep) {
    int k = i * 6;
    umbra_val p0x = umbra_load_i32(b, buf,
                        umbra_imm_i32(b, k));
    umbra_val p0y = umbra_load_i32(b, buf,
                        umbra_imm_i32(b, k+1));
    umbra_val p1x = umbra_load_i32(b, buf,
                        umbra_imm_i32(b, k+2));
    umbra_val p1y = umbra_load_i32(b, buf,
                        umbra_imm_i32(b, k+3));
    umbra_val p2x = umbra_load_i32(b, buf,
                        umbra_imm_i32(b, k+4));
    umbra_val p2y = umbra_load_i32(b, buf,
                        umbra_imm_i32(b, k+5));

    umbra_val q0y = umbra_sub_f32(b, p0y, gy);
    umbra_val q1y = umbra_sub_f32(b, p1y, gy);
    umbra_val q2y = umbra_sub_f32(b, p2y, gy);

    umbra_val a = umbra_add_f32(b,
        umbra_sub_f32(b, q0y,
            umbra_mul_f32(b, tw, q1y)), q2y);
    umbra_val bh = umbra_sub_f32(b, q0y, q1y);

    umbra_val disc = umbra_sub_f32(b,
        umbra_mul_f32(b, bh, bh),
        umbra_mul_f32(b, a, q0y));
    umbra_val ok = umbra_ge_f32(b, disc, z);
    umbra_val sd = umbra_sqrt_f32(b,
        umbra_max_f32(b, disc, z));

    umbra_val abs_a = umbra_max_f32(b, a,
        umbra_sub_f32(b, z, a));
    umbra_val is_quad = umbra_gt_f32(b, abs_a, ep);

    umbra_val ia = umbra_div_f32(b, o,
        umbra_sel_i32(b, is_quad, a, o));

    umbra_val qt1 = umbra_mul_f32(b,
        umbra_sub_f32(b, bh, sd), ia);
    umbra_val qt2 = umbra_mul_f32(b,
        umbra_add_f32(b, bh, sd), ia);

    umbra_val abs_bh = umbra_max_f32(b, bh,
        umbra_sub_f32(b, z, bh));
    umbra_val lt = umbra_div_f32(b, q0y,
        umbra_sel_i32(b,
            umbra_gt_f32(b, abs_bh, ep),
            umbra_mul_f32(b, tw, bh), o));

    umbra_val t1 = umbra_sel_i32(b,
        is_quad, qt1, lt);
    umbra_val t2 = qt2;

    umbra_val t1ok = umbra_and_i32(b, ok,
        umbra_and_i32(b,
            umbra_ge_f32(b, t1, z),
            umbra_lt_f32(b, t1, o)));
    umbra_val t2ok = umbra_and_i32(b,
        umbra_and_i32(b, ok, is_quad),
        umbra_and_i32(b,
            umbra_ge_f32(b, t2, z),
            umbra_lt_f32(b, t2, o)));

    umbra_val q0x = umbra_sub_f32(b, p0x, gx);
    umbra_val q1x = umbra_sub_f32(b, p1x, gx);
    umbra_val q2x = umbra_sub_f32(b, p2x, gx);
    umbra_val ax = umbra_add_f32(b,
        umbra_sub_f32(b, q0x,
            umbra_mul_f32(b, tw, q1x)), q2x);
    umbra_val bx = umbra_mul_f32(b, tw,
        umbra_sub_f32(b, q1x, q0x));

    umbra_val x1 = umbra_add_f32(b,
        umbra_mul_f32(b,
            umbra_add_f32(b,
                umbra_mul_f32(b, ax, t1), bx),
            t1), q0x);
    umbra_val x2 = umbra_add_f32(b,
        umbra_mul_f32(b,
            umbra_add_f32(b,
                umbra_mul_f32(b, ax, t2), bx),
            t2), q0x);

    umbra_val dy1 = umbra_sub_f32(b,
        umbra_mul_f32(b, a, t1), bh);
    umbra_val dy2 = umbra_sub_f32(b,
        umbra_mul_f32(b, a, t2), bh);

    umbra_val po = umbra_imm_f32(b, 1.0f);
    umbra_val no = umbra_imm_f32(b, -1.0f);

    umbra_val dir1 = umbra_sel_i32(b,
        umbra_gt_f32(b, dy1, z), po,
        umbra_sel_i32(b,
            umbra_lt_f32(b, dy1, z), no, z));
    umbra_val dir2 = umbra_sel_i32(b,
        umbra_gt_f32(b, dy2, z), po,
        umbra_sel_i32(b,
            umbra_lt_f32(b, dy2, z), no, z));

    umbra_val w1 = umbra_sel_i32(b,
        umbra_and_i32(b, t1ok,
            umbra_gt_f32(b, x1, z)),
        dir1, z);
    umbra_val w2 = umbra_sel_i32(b,
        umbra_and_i32(b, t2ok,
            umbra_gt_f32(b, x2, z)),
        dir2, z);

    *wind = umbra_add_f32(b, *wind,
        umbra_add_f32(b, w1, w2));
}

static umbra_val umbra_coverage_slug(
        struct umbra_builder *b,
        umbra_val x, umbra_val y) {
    int fi = umbra_reserve(b, 11);
    int buf_off = umbra_reserve_ptr(b);
    umbra_ptr curves = umbra_deref_ptr(b,
        (umbra_ptr){1}, buf_off);

    umbra_val m0 = umbra_load_i32(b, (umbra_ptr){1},
                       umbra_imm_i32(b, fi));
    umbra_val m1 = umbra_load_i32(b, (umbra_ptr){1},
                       umbra_imm_i32(b, fi+1));
    umbra_val m2 = umbra_load_i32(b, (umbra_ptr){1},
                       umbra_imm_i32(b, fi+2));
    umbra_val m3 = umbra_load_i32(b, (umbra_ptr){1},
                       umbra_imm_i32(b, fi+3));
    umbra_val m4 = umbra_load_i32(b, (umbra_ptr){1},
                       umbra_imm_i32(b, fi+4));
    umbra_val m5 = umbra_load_i32(b, (umbra_ptr){1},
                       umbra_imm_i32(b, fi+5));
    umbra_val m6 = umbra_load_i32(b, (umbra_ptr){1},
                       umbra_imm_i32(b, fi+6));
    umbra_val m7 = umbra_load_i32(b, (umbra_ptr){1},
                       umbra_imm_i32(b, fi+7));
    umbra_val m8 = umbra_load_i32(b, (umbra_ptr){1},
                       umbra_imm_i32(b, fi+8));
    umbra_val bw = umbra_load_i32(b, (umbra_ptr){1},
                       umbra_imm_i32(b, fi+9));
    umbra_val bh = umbra_load_i32(b, (umbra_ptr){1},
                       umbra_imm_i32(b, fi+10));

    umbra_val w = umbra_add_f32(b,
        umbra_add_f32(b,
            umbra_mul_f32(b, m6, x),
            umbra_mul_f32(b, m7, y)), m8);
    umbra_val gx = umbra_div_f32(b,
        umbra_add_f32(b,
            umbra_add_f32(b,
                umbra_mul_f32(b, m0, x),
                umbra_mul_f32(b, m1, y)), m2), w);
    umbra_val gy = umbra_div_f32(b,
        umbra_add_f32(b,
            umbra_add_f32(b,
                umbra_mul_f32(b, m3, x),
                umbra_mul_f32(b, m4, y)), m5), w);

    umbra_val z  = umbra_imm_f32(b, 0.0f);
    umbra_val o  = umbra_imm_f32(b, 1.0f);
    umbra_val tw = umbra_imm_f32(b, 2.0f);
    umbra_val ep = umbra_imm_f32(b, 1.0f/65536.0f);

    umbra_val in = umbra_and_i32(b,
        umbra_and_i32(b,
            umbra_ge_f32(b, gx, z),
            umbra_lt_f32(b, gx, bw)),
        umbra_and_i32(b,
            umbra_ge_f32(b, gy, z),
            umbra_lt_f32(b, gy, bh)));

    umbra_val wind = z;
    for (int i = 0; i < slug_n_curves; i++) {
        slug_ray_(b, curves, i, gx, gy, &wind,
                  z, o, tw, ep);
    }

    umbra_val aw = umbra_max_f32(b, wind,
        umbra_sub_f32(b, z, wind));
    umbra_val cov = umbra_min_f32(b, aw, o);
    return umbra_sel_i32(b, in, cov, z);
}
