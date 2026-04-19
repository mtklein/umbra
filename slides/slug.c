#include "slide.h"
#include "slug.h"
#include <stdio.h>
#include <stdlib.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "../third_party/stb/stb_truetype.h"
#pragma clang diagnostic pop

umbra_val32 coverage_winding(void *ctx, struct umbra_builder *b,
                             umbra_val32 x, umbra_val32 y) {
    struct umbra_buf const *self = ctx;
    (void)x; (void)y;
    umbra_ptr32 const w = umbra_bind_buf32(b, self);
    umbra_val32 const raw = umbra_load_32(b, w);
    return umbra_min_f32(b, umbra_abs_f32(b, raw), umbra_imm_f32(b, 1.0f));
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

// Uniform layout: buf[0] holds a `struct slug_acc_uniforms` or
// `struct slug_uniforms`; dst at buf[1].

// TODO: this rasterizer is wrong at some animation steps and/or at large canvas
// sizes.  Observed by resizing the demo window: small sizes showed rare pixel
// flicker (easy to dismiss as noise); large sizes clearly mis-render slug past
// the animation step we capture in dumps/.  Every backend (interp, jit, metal,
// vulkan, wgpu) shows the same wrongness, and both the one-pass loop variant
// (slug_build below) and the two-pass accumulator (this function) agree — so
// it's not a backend issue, it's the shared winding-contribution algorithm.
//
// Candidates to investigate:
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
//
// Both slug_build_acc and slug_build below duplicate this math — fix them
// together (or factor into a shared emit helper before fixing).

#define UNI_SLOT(type, field) \
    ((int)(__builtin_offsetof(type, field) / 4))

struct umbra_builder* slug_build_acc(struct umbra_buf const *curves_buf,
                                     struct slug_acc_uniforms const *uni,
                                     struct umbra_buf const *wind_buf) {
    struct umbra_builder *b = umbra_builder();

    umbra_ptr32 const u      = umbra_bind_uniforms32   (b, uni, (int)(sizeof *uni / 4));
    umbra_ptr32 const curves = umbra_bind_buf32 (b, curves_buf);
    umbra_ptr32 const wind   = umbra_bind_buf32 (b, wind_buf);

    umbra_val32 xf = umbra_f32_from_i32(b, umbra_x(b));
    umbra_val32 yf = umbra_f32_from_i32(b, umbra_y(b));

    umbra_val32 m[9];
    for (int i = 0; i < 9; i++) {
        m[i] = umbra_uniform_32(b, u,
                                 UNI_SLOT(struct slug_acc_uniforms, mat) + i);
    }
    umbra_val32 bw = umbra_uniform_32(b, u, UNI_SLOT(struct slug_acc_uniforms, bw));
    umbra_val32 bh = umbra_uniform_32(b, u, UNI_SLOT(struct slug_acc_uniforms, bh));

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

    umbra_val32 j = umbra_uniform_32(b, u, UNI_SLOT(struct slug_acc_uniforms, j));
    umbra_val32 k = umbra_mul_i32(b, j, umbra_imm_i32(b, 6));

    umbra_val32 p0x = umbra_gather_32(b, curves, k),
                p0y = umbra_gather_32(b, curves, umbra_add_i32(b, k, umbra_imm_i32(b, 1))),
                p1x = umbra_gather_32(b, curves, umbra_add_i32(b, k, umbra_imm_i32(b, 2))),
                p1y = umbra_gather_32(b, curves, umbra_add_i32(b, k, umbra_imm_i32(b, 3))),
                p2x = umbra_gather_32(b, curves, umbra_add_i32(b, k, umbra_imm_i32(b, 4))),
                p2y = umbra_gather_32(b, curves, umbra_add_i32(b, k, umbra_imm_i32(b, 5)));

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
    umbra_val32 sd = umbra_sqrt_f32(b, umbra_max_f32(b, disc, z));

    umbra_val32 abs_a = umbra_abs_f32(b, a);
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

    umbra_val32 q0x = umbra_sub_f32(b, p0x, gx);
    umbra_val32 q1x = umbra_sub_f32(b, p1x, gx);
    umbra_val32 q2x = umbra_sub_f32(b, p2x, gx);
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

    umbra_val32 po = umbra_imm_f32(b, 1.0f);
    umbra_val32 no = umbra_imm_f32(b, -1.0f);

    umbra_val32 dir1 = umbra_sel_32(b, umbra_lt_f32(b, z, dy1), po,
                           umbra_sel_32(b, umbra_lt_f32(b, dy1, z), no, z));
    umbra_val32 dir2 = umbra_sel_32(b, umbra_lt_f32(b, z, dy2), po,
                           umbra_sel_32(b, umbra_lt_f32(b, dy2, z), no, z));

    umbra_val32 w1 = umbra_sel_32(b, umbra_and_32(b, t1ok, umbra_lt_f32(b, z, x1)), dir1, z);
    umbra_val32 w2 = umbra_sel_32(b, umbra_and_32(b, t2ok, umbra_lt_f32(b, z, x2)), dir2, z);

    umbra_val32 dw = umbra_sel_32(b, in, umbra_add_f32(b, w1, w2), z);

    umbra_val32 acc = umbra_load_32(b, wind);
    acc = umbra_add_f32(b, acc, dw);
    umbra_store_32(b, wind, acc);

    return b;
}

struct umbra_builder* slug_build(struct umbra_buf const *curves_buf,
                                 struct slug_uniforms const *uni,
                                 struct umbra_buf const *wind_buf) {
    struct umbra_builder *b = umbra_builder();

    umbra_ptr32 const u      = umbra_bind_uniforms32   (b, uni, (int)(sizeof *uni / 4));
    umbra_ptr32 const curves = umbra_bind_buf32 (b, curves_buf);
    umbra_ptr32 const wind   = umbra_bind_buf32 (b, wind_buf);

    umbra_val32 xf = umbra_f32_from_i32(b, umbra_x(b));
    umbra_val32 yf = umbra_f32_from_i32(b, umbra_y(b));

    umbra_val32 m[9];
    for (int i = 0; i < 9; i++) {
        m[i] = umbra_uniform_32(b, u,
                                 UNI_SLOT(struct slug_uniforms, mat) + i);
    }
    umbra_val32 bw = umbra_uniform_32(b, u, UNI_SLOT(struct slug_uniforms, bw));
    umbra_val32 bh = umbra_uniform_32(b, u, UNI_SLOT(struct slug_uniforms, bh));

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

    umbra_val32 count = umbra_uniform_32(b, u, UNI_SLOT(struct slug_uniforms, n_curves));
    umbra_var32 wind_var = umbra_declare_var32(b);

    umbra_val32 j = umbra_loop(b, count); {
        umbra_val32 k = umbra_mul_i32(b, j, umbra_imm_i32(b, 6));

        umbra_val32 p0x = umbra_gather_32(b, curves, k),
                    p0y = umbra_gather_32(b, curves, umbra_add_i32(b, k, umbra_imm_i32(b, 1))),
                    p1x = umbra_gather_32(b, curves, umbra_add_i32(b, k, umbra_imm_i32(b, 2))),
                    p1y = umbra_gather_32(b, curves, umbra_add_i32(b, k, umbra_imm_i32(b, 3))),
                    p2x = umbra_gather_32(b, curves, umbra_add_i32(b, k, umbra_imm_i32(b, 4))),
                    p2y = umbra_gather_32(b, curves, umbra_add_i32(b, k, umbra_imm_i32(b, 5)));

        umbra_val32 q0y = umbra_sub_f32(b, p0y, gy),
                    q1y = umbra_sub_f32(b, p1y, gy),
                    q2y = umbra_sub_f32(b, p2y, gy);

        umbra_val32 a  = umbra_add_f32(b, umbra_sub_f32(b, q0y, umbra_mul_f32(b, tw, q1y)), q2y);
        umbra_val32 bv = umbra_sub_f32(b, q0y, q1y);

        umbra_val32 disc = umbra_sub_f32(b, umbra_mul_f32(b, bv, bv),
                                            umbra_mul_f32(b, a, q0y));
        umbra_val32 ok = umbra_le_f32(b, z, disc);
        umbra_val32 sd = umbra_sqrt_f32(b, umbra_max_f32(b, disc, z));

        umbra_val32 abs_a = umbra_abs_f32(b, a);
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

        umbra_val32 q0x = umbra_sub_f32(b, p0x, gx),
                    q1x = umbra_sub_f32(b, p1x, gx),
                    q2x = umbra_sub_f32(b, p2x, gx);
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

        umbra_val32 po = umbra_imm_f32(b, 1.0f);
        umbra_val32 no = umbra_imm_f32(b, -1.0f);

        umbra_val32 dir1 = umbra_sel_32(b, umbra_lt_f32(b, z, dy1), po,
                               umbra_sel_32(b, umbra_lt_f32(b, dy1, z), no, z));
        umbra_val32 dir2 = umbra_sel_32(b, umbra_lt_f32(b, z, dy2), po,
                               umbra_sel_32(b, umbra_lt_f32(b, dy2, z), no, z));

        umbra_val32 w1 = umbra_sel_32(b, umbra_and_32(b, t1ok, umbra_lt_f32(b, z, x1)),
                                      dir1, z);
        umbra_val32 w2 = umbra_sel_32(b, umbra_and_32(b, t2ok, umbra_lt_f32(b, z, x2)),
                                      dir2, z);

        umbra_val32 dw = umbra_sel_32(b, in, umbra_add_f32(b, w1, w2), z);

        umbra_store_var32(b, wind_var, umbra_add_f32(b, umbra_load_var32(b, wind_var), dw));
    } umbra_end_loop(b);

    umbra_store_32(b, wind, umbra_load_var32(b, wind_var));

    return b;
}

struct slug_two_pass_slide {
    struct slide base;

    struct slug_curves        slug;
    struct umbra_buf          curves_buf;
    int                       w, h;
    float                    *wind_buf;
    struct umbra_buf          wind_uniform;
    struct slug_acc_uniforms  au;
    struct umbra_flat_ir     *acc_ir;
    struct umbra_program     *acc_prog;

    umbra_color               color;
    struct umbra_fmt          fmt;
    struct umbra_program     *draw_prog;
    struct umbra_buf          dst_buf;
};

static void slug_two_pass_init(struct slide *s, int w, int h) {
    struct slug_two_pass_slide *st = (struct slug_two_pass_slide *)s;
    st->w = w;
    st->h = h;
    st->slug = slug_extract("Slug", (float)h * 0.3125f);
    st->curves_buf = (struct umbra_buf){
        .ptr = st->slug.data, .count = st->slug.count * 6,
    };
    st->wind_buf = malloc((size_t)w * (size_t)h * sizeof(float));
    st->wind_uniform = (struct umbra_buf){
        .ptr = st->wind_buf, .count = w * h, .stride = w,
    };

    struct umbra_builder *b = slug_build_acc(&st->curves_buf, &st->au, &st->wind_uniform);
    st->acc_ir = umbra_flat_ir(b);
    umbra_builder_free(b);
}

static void slug_two_pass_prepare(struct slide *s,
                                  struct umbra_backend *be,
                                  struct umbra_fmt fmt) {
    struct slug_two_pass_slide *st = (struct slug_two_pass_slide *)s;
    umbra_program_free(st->acc_prog);
    st->acc_prog = be->compile(be, st->acc_ir);
    umbra_program_free(st->draw_prog);
    st->fmt = fmt;
    {
        struct umbra_builder *b = umbra_draw_builder(
            coverage_winding,       &st->wind_uniform,
            umbra_shader_color,     &st->color,
            umbra_blend_srcover,    NULL,
            &st->dst_buf,           fmt);
        struct umbra_flat_ir *ir = umbra_flat_ir(b);
        umbra_builder_free(b);
        st->draw_prog = be->compile(be, ir);
        umbra_flat_ir_free(ir);
    }
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void slug_two_pass_draw(struct slide *s, double secs, int l, int t, int r, int b,
                               void *buf) {
    struct slug_two_pass_slide           *st = (struct slug_two_pass_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    struct umbra_program *acc = st->acc_prog;
    int w = st->w, h = st->h;

    size_t const wind_row = (size_t)w * sizeof(float);
    __builtin_memset((char *)st->wind_buf + (size_t)t * wind_row, 0,
                     (size_t)(b - t) * wind_row);

    slide_perspective_matrix(&st->au.mat, (float)secs, w, h,
                             (int)st->slug.w, (int)st->slug.h);
    st->au.bw = st->slug.w;
    st->au.bh = st->slug.h;

    for (int j = 0; j < st->slug.count; j++) {
        __builtin_memcpy(&st->au.j, &j, 4);
        acc->queue(acc, l, t, r, b);
    }

    st->dst_buf = (struct umbra_buf){
        .ptr=buf, .count=w * h * st->fmt.planes, .stride=w,
    };
    st->draw_prog->queue(st->draw_prog, l, t, r, b);
}

static int slug_two_pass_get_builders(struct slide *s, struct umbra_fmt fmt,
                                      struct umbra_builder **out, int max) {
    if (max < 2) { return 0; }
    struct slug_two_pass_slide *st = (struct slug_two_pass_slide *)s;
    out[0] = slug_build_acc(&st->curves_buf, &st->au, &st->wind_uniform);
    out[1] = umbra_draw_builder(
        coverage_winding,       &st->wind_uniform,
        umbra_shader_color,     &st->color,
        umbra_blend_srcover,    NULL,
        &st->dst_buf,           fmt);
    return 2;
}

static void slug_two_pass_free(struct slide *s) {
    struct slug_two_pass_slide *st = (struct slug_two_pass_slide *)s;
    slug_free(&st->slug);
    free(st->wind_buf);
    umbra_program_free(st->acc_prog);
    umbra_flat_ir_free(st->acc_ir);
    umbra_program_free(st->draw_prog);
    free(st);
}

SLIDE(slide_slug_wind) {
    struct slug_two_pass_slide *st = calloc(1, sizeof *st);
    st->color = (umbra_color){0.2f, 1.0f, 0.6f, 1.0f};
    st->base = (struct slide){
        .title = "Slug (two-pass)",
        .bg = {0.12f, 0.04f, 0.04f, 1},
        .init = slug_two_pass_init,
        .prepare = slug_two_pass_prepare,
        .draw = slug_two_pass_draw,
        .free = slug_two_pass_free,
        .get_builders = slug_two_pass_get_builders,
    };
    return &st->base;
}

struct slug_slide {
    struct slide base;

    struct slug_curves            slug;
    struct umbra_buf              curves_buf;
    int                           w, h;
    float                        *wind_buf;
    struct umbra_buf              wind_uniform;
    struct slug_uniforms          au;
    struct umbra_flat_ir         *acc_ir;
    struct umbra_program         *acc_prog;

    umbra_color                   color;
    struct umbra_fmt              fmt;
    struct umbra_program         *draw_prog;
    struct umbra_buf              dst_buf;
};

static void slug_init(struct slide *s, int w, int h) {
    struct slug_slide *st = (struct slug_slide *)s;
    st->w = w;
    st->h = h;
    st->slug = slug_extract("Slug", (float)h * 0.3125f);
    st->curves_buf = (struct umbra_buf){
        .ptr = st->slug.data, .count = st->slug.count * 6,
    };
    st->wind_buf = malloc((size_t)w * (size_t)h * sizeof(float));
    st->wind_uniform = (struct umbra_buf){
        .ptr = st->wind_buf, .count = w * h, .stride = w,
    };

    struct umbra_builder *b = slug_build(&st->curves_buf, &st->au, &st->wind_uniform);
    st->acc_ir = umbra_flat_ir(b);
    umbra_builder_free(b);
}

static void slug_prepare(struct slide *s, struct umbra_backend *be,
                         struct umbra_fmt fmt) {
    struct slug_slide *st = (struct slug_slide *)s;
    umbra_program_free(st->acc_prog);
    st->acc_prog = be->compile(be, st->acc_ir);
    umbra_program_free(st->draw_prog);
    st->fmt = fmt;
    {
        struct umbra_builder *b = umbra_draw_builder(
            coverage_winding,       &st->wind_uniform,
            umbra_shader_color,     &st->color,
            umbra_blend_srcover,    NULL,
            &st->dst_buf,           fmt);
        struct umbra_flat_ir *ir = umbra_flat_ir(b);
        umbra_builder_free(b);
        st->draw_prog = be->compile(be, ir);
        umbra_flat_ir_free(ir);
    }
    slide_bg_prepare(be, fmt, st->w, st->h);
}

static void slug_draw(struct slide *s, double secs, int l, int t, int r, int b, void *buf) {
    struct slug_slide *st = (struct slug_slide *)s;
    slide_bg_draw(s->bg, l, t, r, b, buf);
    int w = st->w, h = st->h;

    size_t const wind_row = (size_t)w * sizeof(float);
    __builtin_memset((char *)st->wind_buf + (size_t)t * wind_row, 0,
                     (size_t)(b - t) * wind_row);

    slide_perspective_matrix(&st->au.mat, (float)secs, w, h,
                             (int)st->slug.w, (int)st->slug.h);
    st->au.bw = st->slug.w;
    st->au.bh = st->slug.h;
    {
        int const n = st->slug.count;
        __builtin_memcpy(&st->au.n_curves, &n, 4);
    }

    st->acc_prog->queue(st->acc_prog, l, t, r, b);

    st->dst_buf = (struct umbra_buf){
        .ptr=buf, .count=w * h * st->fmt.planes, .stride=w,
    };
    st->draw_prog->queue(st->draw_prog, l, t, r, b);
}

static void slug_free_slide(struct slide *s) {
    struct slug_slide *st = (struct slug_slide *)s;
    slug_free(&st->slug);
    free(st->wind_buf);
    umbra_program_free(st->acc_prog);
    umbra_flat_ir_free(st->acc_ir);
    umbra_program_free(st->draw_prog);
    free(st);
}

static int slug_one_pass_get_builders(struct slide *s, struct umbra_fmt fmt,
                                      struct umbra_builder **out, int max) {
    (void)fmt;
    if (max < 1) { return 0; }
    struct slug_slide *st = (struct slug_slide *)s;
    out[0] = slug_build(&st->curves_buf, &st->au, &st->wind_uniform);
    return out[0] ? 1 : 0;
}

SLIDE(slide_slug_wind_loop) {
    struct slug_slide *st = calloc(1, sizeof *st);
    st->color = (umbra_color){0.2f, 1.0f, 0.6f, 1.0f};
    st->base = (struct slide){
        .title = "Slug (one-pass)",
        .bg = {0.12f, 0.04f, 0.04f, 1},
        .init = slug_init,
        .prepare = slug_prepare,
        .draw = slug_draw,
        .free = slug_free_slide,
        .get_builders = slug_one_pass_get_builders,
    };
    return &st->base;
}
