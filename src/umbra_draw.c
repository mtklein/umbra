#include "../include/umbra_draw.h"
#include "bb.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct umbra_builder builder;

struct umbra_builder *umbra_draw_build(umbra_shader_fn shader, umbra_coverage_fn coverage,
                                       umbra_blend_fn blend, umbra_fmt fmt,
                                       umbra_draw_layout *layout) {
    builder  *builder = umbra_builder();
    umbra_val const x = umbra_x(builder);
    umbra_val const y = umbra_y(builder);
    umbra_val const xf = umbra_f32_from_i32(builder, x);
    umbra_val const yf = umbra_f32_from_i32(builder, y);

    struct umbra_uniforms *uni = calloc(1, sizeof(struct umbra_uniforms));

    size_t const shader_off = uni->size;
    umbra_color src = {
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
    };
    if (shader) {
        src = shader(builder, uni, xf, yf);
    }

    size_t const coverage_off = uni->size;
    umbra_val cov = {0};
    if (coverage) {
        cov = coverage(builder, uni, xf, yf);
    }

    umbra_color dst = {
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
    };
    if (blend || coverage) {
        dst = umbra_load_color(builder, (umbra_ptr){1, 0}, fmt);
    }

    umbra_color out;
    if (blend) {
        out = blend(builder, src, dst);
    } else {
        out = src;
    }

    if (coverage) {
        out.r = umbra_add_f32(builder, dst.r,
                              umbra_mul_f32(builder, umbra_sub_f32(builder, out.r, dst.r),
                                            cov));
        out.g = umbra_add_f32(builder, dst.g,
                              umbra_mul_f32(builder, umbra_sub_f32(builder, out.g, dst.g),
                                            cov));
        out.b = umbra_add_f32(builder, dst.b,
                              umbra_mul_f32(builder, umbra_sub_f32(builder, out.b, dst.b),
                                            cov));
        out.a = umbra_add_f32(builder, dst.a,
                              umbra_mul_f32(builder, umbra_sub_f32(builder, out.a, dst.a),
                                            cov));
    }

    umbra_store_color(builder, (umbra_ptr){1, 0}, out, fmt);

    if (layout) {
        layout->uni = uni;
        layout->shader = shader_off;
        layout->coverage = coverage_off;
    } else {
        if (uni) { free(uni->data); free(uni); }
    }

    return builder;
}

umbra_color umbra_shader_solid(builder *builder, struct umbra_uniforms *u, umbra_val x, umbra_val y) {
    (void)x;
    (void)y;
    size_t const fi = umbra_reserve_f32(u, 4);
    umbra_val const r = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi);
    umbra_val const g = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 4);
    umbra_val const b = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 8);
    umbra_val const a = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 12);
    return (umbra_color){r, g, b, a};
}

static umbra_val clamp01(builder *builder, umbra_val t) {
    return umbra_min_f32(builder, umbra_max_f32(builder, t, umbra_imm_f32(builder, 0.0f)),
                         umbra_imm_f32(builder, 1.0f));
}

static umbra_val lerp_f(builder *builder, umbra_val p, umbra_val q, umbra_val t) {
    return umbra_add_f32(builder, p,
                         umbra_mul_f32(builder, umbra_sub_f32(builder, q, p), t));
}

static umbra_val linear_t_(builder *builder, size_t fi, umbra_val x, umbra_val y) {
    umbra_val const a = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi);
    umbra_val const b = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 4);
    umbra_val const c = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 8);
    umbra_val const t = umbra_add_f32(builder,
                                      umbra_add_f32(builder, umbra_mul_f32(builder, a, x),
                                                    umbra_mul_f32(builder, b, y)),
                                      c);
    return clamp01(builder, t);
}

static umbra_val radial_t_(builder *builder, size_t fi, umbra_val x, umbra_val y) {
    umbra_val const cx = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi);
    umbra_val const cy = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 4);
    umbra_val const inv_r = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 8);
    umbra_val const dx = umbra_sub_f32(builder, x, cx);
    umbra_val const dy = umbra_sub_f32(builder, y, cy);
    umbra_val const d2 = umbra_add_f32(builder, umbra_mul_f32(builder, dx, dx),
                                       umbra_mul_f32(builder, dy, dy));
    return clamp01(builder, umbra_mul_f32(builder, umbra_sqrt_f32(builder, d2), inv_r));
}

static umbra_color lerp_2stop_(builder *builder, umbra_val t, size_t fi) {
    umbra_val const r0 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi);
    umbra_val const g0 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 4);
    umbra_val const b0 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 8);
    umbra_val const a0 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 12);
    umbra_val const r1 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 16);
    umbra_val const g1 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 20);
    umbra_val const b1 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 24);
    umbra_val const a1 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 28);
    return (umbra_color){
        lerp_f(builder, r0, r1, t),
        lerp_f(builder, g0, g1, t),
        lerp_f(builder, b0, b1, t),
        lerp_f(builder, a0, a1, t),
    };
}

static umbra_color sample_lut_(builder *builder, umbra_val t_f32, size_t fi, umbra_ptr lut) {
    // Planar LUT: [R0..R_{N-1}, G0..G_{N-1}, B0..B_{N-1}, A0..A_{N-1}]
    umbra_val const N_f  = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 12);
    umbra_val const N_m1 = umbra_sub_f32(builder, N_f, umbra_imm_f32(builder, 1.0f));
    umbra_val const N_m2 = umbra_sub_f32(builder, N_f, umbra_imm_f32(builder, 2.0f));
    umbra_val const t_sc = umbra_mul_f32(builder, t_f32, N_m1);
    umbra_val const frac_idx = umbra_min_f32(builder, t_sc, N_m2);

    umbra_val const N2_f = umbra_add_f32(builder, N_f, N_f);
    umbra_val const N3_f = umbra_add_f32(builder, N2_f, N_f);
    return (umbra_color){
        umbra_sample_32(builder, lut, frac_idx),
        umbra_sample_32(builder, lut, umbra_add_f32(builder, frac_idx, N_f)),
        umbra_sample_32(builder, lut, umbra_add_f32(builder, frac_idx, N2_f)),
        umbra_sample_32(builder, lut, umbra_add_f32(builder, frac_idx, N3_f)),
    };
}

umbra_color umbra_shader_linear_2(builder *builder, struct umbra_uniforms *u, umbra_val x, umbra_val y) {
    size_t const fi = umbra_reserve_f32(u, 3);
    size_t const ci = umbra_reserve_f32(u, 8);
    return lerp_2stop_(builder, linear_t_(builder, fi, x, y), ci);
}
umbra_color umbra_shader_radial_2(builder *builder, struct umbra_uniforms *u, umbra_val x, umbra_val y) {
    size_t const fi = umbra_reserve_f32(u, 3);
    size_t const ci = umbra_reserve_f32(u, 8);
    return lerp_2stop_(builder, radial_t_(builder, fi, x, y), ci);
}
umbra_color umbra_shader_linear_grad(builder *builder, struct umbra_uniforms *u, umbra_val x, umbra_val y) {
    size_t const fi = umbra_reserve_f32(u, 4);
    size_t    const lut_off = umbra_reserve_ptr(u);
    umbra_ptr const lut = umbra_deref_ptr(builder, (umbra_ptr){0, 0}, lut_off);
    return sample_lut_(builder, linear_t_(builder, fi, x, y), fi, lut);
}
umbra_color umbra_shader_radial_grad(builder *builder, struct umbra_uniforms *u, umbra_val x, umbra_val y) {
    size_t const fi = umbra_reserve_f32(u, 4);
    size_t    const lut_off = umbra_reserve_ptr(u);
    umbra_ptr const lut = umbra_deref_ptr(builder, (umbra_ptr){0, 0}, lut_off);
    return sample_lut_(builder, radial_t_(builder, fi, x, y), fi, lut);
}

umbra_color umbra_supersample(builder *builder, struct umbra_uniforms *u, umbra_val x, umbra_val y,
                              umbra_shader_fn inner, int n) {
    static float const jitter[][2] = {
        {-0.375f, -0.125f}, {0.125f, -0.375f}, {0.375f, 0.125f}, {-0.125f, 0.375f},
        {-0.250f, 0.375f},  {0.250f, -0.250f}, {0.375f, 0.250f}, {-0.375f, -0.250f},
    };
    if (n < 1) { n = 1; }
    if (n > 8) { n = 8; }

    size_t      const saved = u->size;
    umbra_color sum = inner(builder, u, x, y);
    size_t      const after = u->size;

    for (int s = 1; s < n; s++) {
        struct umbra_uniforms scratch = {.size = saved};
        umbra_val const sx = umbra_add_f32(builder, x, umbra_imm_f32(builder, jitter[s - 1][0]));
        umbra_val const sy = umbra_add_f32(builder, y, umbra_imm_f32(builder, jitter[s - 1][1]));
        umbra_color const c = inner(builder, &scratch, sx, sy);
        assert(scratch.size == after);
        sum.r = umbra_add_f32(builder, sum.r, c.r);
        sum.g = umbra_add_f32(builder, sum.g, c.g);
        sum.b = umbra_add_f32(builder, sum.b, c.b);
        sum.a = umbra_add_f32(builder, sum.a, c.a);
    }

    umbra_val const inv = umbra_imm_f32(builder, 1.0f / (float)n);
    return (umbra_color){
        umbra_mul_f32(builder, sum.r, inv),
        umbra_mul_f32(builder, sum.g, inv),
        umbra_mul_f32(builder, sum.b, inv),
        umbra_mul_f32(builder, sum.a, inv),
    };
}

umbra_color umbra_blend_src(builder *builder, umbra_color src, umbra_color dst) {
    (void)builder;
    (void)dst;
    return src;
}

umbra_color umbra_blend_srcover(builder *builder, umbra_color src, umbra_color dst) {
    umbra_val const one = umbra_imm_f32(builder, 1.0f);
    umbra_val const inv_a = umbra_sub_f32(builder, one, src.a);
    return (umbra_color){
        umbra_add_f32(builder, src.r, umbra_mul_f32(builder, dst.r, inv_a)),
        umbra_add_f32(builder, src.g, umbra_mul_f32(builder, dst.g, inv_a)),
        umbra_add_f32(builder, src.b, umbra_mul_f32(builder, dst.b, inv_a)),
        umbra_add_f32(builder, src.a, umbra_mul_f32(builder, dst.a, inv_a)),
    };
}

umbra_color umbra_blend_dstover(builder *builder, umbra_color src, umbra_color dst) {
    umbra_val const one = umbra_imm_f32(builder, 1.0f);
    umbra_val const inv_a = umbra_sub_f32(builder, one, dst.a);
    return (umbra_color){
        umbra_add_f32(builder, dst.r, umbra_mul_f32(builder, src.r, inv_a)),
        umbra_add_f32(builder, dst.g, umbra_mul_f32(builder, src.g, inv_a)),
        umbra_add_f32(builder, dst.b, umbra_mul_f32(builder, src.b, inv_a)),
        umbra_add_f32(builder, dst.a, umbra_mul_f32(builder, src.a, inv_a)),
    };
}

umbra_color umbra_blend_multiply(builder *builder, umbra_color src, umbra_color dst) {
    umbra_val const one = umbra_imm_f32(builder, 1.0f);
    umbra_val const inv_sa = umbra_sub_f32(builder, one, src.a);
    umbra_val const inv_da = umbra_sub_f32(builder, one, dst.a);
    umbra_val const r = umbra_add_f32(builder, umbra_mul_f32(builder, src.r, dst.r),
                                      umbra_add_f32(builder, umbra_mul_f32(builder, src.r, inv_da),
                                                    umbra_mul_f32(builder, dst.r, inv_sa)));
    umbra_val const g = umbra_add_f32(builder, umbra_mul_f32(builder, src.g, dst.g),
                                      umbra_add_f32(builder, umbra_mul_f32(builder, src.g, inv_da),
                                                    umbra_mul_f32(builder, dst.g, inv_sa)));
    umbra_val const b = umbra_add_f32(builder, umbra_mul_f32(builder, src.b, dst.b),
                                      umbra_add_f32(builder, umbra_mul_f32(builder, src.b, inv_da),
                                                    umbra_mul_f32(builder, dst.b, inv_sa)));
    umbra_val const a = umbra_add_f32(builder, umbra_mul_f32(builder, src.a, dst.a),
                                      umbra_add_f32(builder, umbra_mul_f32(builder, src.a, inv_da),
                                                    umbra_mul_f32(builder, dst.a, inv_sa)));
    return (umbra_color){r, g, b, a};
}

umbra_val umbra_coverage_rect(builder *builder, struct umbra_uniforms *u, umbra_val x, umbra_val y) {
    size_t const fi = umbra_reserve_f32(u, 4);
    umbra_val const l = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi);
    umbra_val const t = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 4);
    umbra_val const r = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 8);
    umbra_val const b = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 12);
    umbra_val const inside = umbra_and_32(builder,
                                           umbra_and_32(builder, umbra_le_f32(builder, l, x),
                                                         umbra_lt_f32(builder, x, r)),
                                           umbra_and_32(builder, umbra_le_f32(builder, t, y),
                                                         umbra_lt_f32(builder, y, b)));
    umbra_val const one_f = umbra_imm_f32(builder, 1.0f);
    umbra_val const zero_f = umbra_imm_f32(builder, 0.0f);
    return umbra_sel_32(builder, inside, one_f, zero_f);
}

umbra_val umbra_coverage_bitmap(builder *builder, struct umbra_uniforms *u, umbra_val x, umbra_val y) {
    (void)x;
    (void)y;
    size_t    const bmp_off = umbra_reserve_ptr(u);
    umbra_ptr const bmp = umbra_deref_ptr(builder, (umbra_ptr){0, 0}, bmp_off);
    umbra_val const val = umbra_i32_from_s16(builder, umbra_load_16(builder, bmp));
    umbra_val const inv255 = umbra_imm_f32(builder, 1.0f / 255.0f);
    return umbra_mul_f32(builder, umbra_f32_from_i32(builder, val), inv255);
}

umbra_val umbra_coverage_sdf(builder *builder, struct umbra_uniforms *u, umbra_val x, umbra_val y) {
    (void)x;
    (void)y;
    size_t    const bmp_off = umbra_reserve_ptr(u);
    umbra_ptr const bmp = umbra_deref_ptr(builder, (umbra_ptr){0, 0}, bmp_off);
    umbra_val const raw = umbra_i32_from_s16(builder, umbra_load_16(builder, bmp));
    umbra_val const inv255 = umbra_imm_f32(builder, 1.0f / 255.0f);
    umbra_val const dist = umbra_mul_f32(builder, umbra_f32_from_i32(builder, raw), inv255);
    umbra_val const lo = umbra_imm_f32(builder, 0.4375f);
    umbra_val const scale = umbra_imm_f32(builder, 8.0f);
    umbra_val const shifted = umbra_sub_f32(builder, dist, lo);
    umbra_val const scaled = umbra_mul_f32(builder, shifted, scale);
    umbra_val const zero = umbra_imm_f32(builder, 0.0f);
    umbra_val const one = umbra_imm_f32(builder, 1.0f);
    return umbra_min_f32(builder, umbra_max_f32(builder, scaled, zero), one);
}

umbra_val umbra_coverage_wind(builder *builder, struct umbra_uniforms *u, umbra_val x, umbra_val y) {
    (void)x;
    (void)y;
    size_t    const off = umbra_reserve_ptr(u);
    umbra_ptr const w = umbra_deref_ptr(builder, (umbra_ptr){0, 0}, off);
    umbra_val const raw = umbra_load_32(builder, w);
    return umbra_min_f32(builder, umbra_abs_f32(builder, raw),
                         umbra_imm_f32(builder, 1.0f));
}

umbra_val umbra_coverage_bitmap_matrix(builder *builder, struct umbra_uniforms *u, umbra_val x, umbra_val y) {
    size_t const fi = umbra_reserve_f32(u, 11);
    size_t    const bmp_off = umbra_reserve_ptr(u);
    umbra_ptr const bmp = umbra_deref_ptr(builder, (umbra_ptr){0, 0}, bmp_off);

    umbra_val const m0 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi);
    umbra_val const m1 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 4);
    umbra_val const m2 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 8);
    umbra_val const m3 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 12);
    umbra_val const m4 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 16);
    umbra_val const m5 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 20);
    umbra_val const m6 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 24);
    umbra_val const m7 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 28);
    umbra_val const m8 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 32);
    umbra_val const bw = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 36);
    umbra_val const bh = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 40);

    umbra_val const w = umbra_add_f32(builder,
                                      umbra_add_f32(builder, umbra_mul_f32(builder, m6, x),
                                                    umbra_mul_f32(builder, m7, y)),
                                      m8);
    umbra_val const xp =
        umbra_div_f32(builder,
                      umbra_add_f32(builder,
                                    umbra_add_f32(builder, umbra_mul_f32(builder, m0, x),
                                                  umbra_mul_f32(builder, m1, y)),
                                    m2),
                      w);
    umbra_val const yp =
        umbra_div_f32(builder,
                      umbra_add_f32(builder,
                                    umbra_add_f32(builder, umbra_mul_f32(builder, m3, x),
                                                  umbra_mul_f32(builder, m4, y)),
                                    m5),
                      w);

    umbra_val const zero_f = umbra_imm_f32(builder, 0.0f);
    umbra_val const in = umbra_and_32(builder,
                                       umbra_and_32(builder, umbra_le_f32(builder, zero_f, xp),
                                                     umbra_lt_f32(builder, xp, bw)),
                                       umbra_and_32(builder, umbra_le_f32(builder, zero_f, yp),
                                                     umbra_lt_f32(builder, yp, bh)));

    umbra_val const one_f = umbra_imm_f32(builder, 1.0f);
    umbra_val const xc = umbra_min_f32(builder, umbra_max_f32(builder, xp, zero_f),
                                       umbra_sub_f32(builder, bw, one_f));
    umbra_val const yc = umbra_min_f32(builder, umbra_max_f32(builder, yp, zero_f),
                                       umbra_sub_f32(builder, bh, one_f));
    umbra_val const xi = umbra_floor_i32(builder, xc);
    umbra_val const yi = umbra_floor_i32(builder, yc);
    umbra_val const bwi = umbra_floor_i32(builder, bw);
    umbra_val const idx = umbra_add_i32(builder, umbra_mul_i32(builder, yi, bwi), xi);

    umbra_val const val = umbra_i32_from_s16(builder, umbra_gather_16(builder, bmp, idx));
    umbra_val const inv255 = umbra_imm_f32(builder, 1.0f / 255.0f);
    umbra_val const cov = umbra_mul_f32(builder, umbra_f32_from_i32(builder, val), inv255);

    return umbra_sel_32(builder, in, cov, umbra_imm_f32(builder, 0.0f));
}


void umbra_gradient_lut_even(float *out, int lut_n, int n_stops, float const colors[][4]) {
    for (int i = 0; i < lut_n; i++) {
        float const t = (float)i / (float)(lut_n - 1);
        float const seg = t * (float)(n_stops - 1);
        int   idx = (int)seg;
        if (idx >= n_stops - 1) {
            idx = n_stops - 2;
        }
        float const f = seg - (float)idx;
        for (int ch = 0; ch < 4; ch++) {
            out[ch * lut_n + i] = colors[idx][ch] * (1 - f) + colors[idx + 1][ch] * f;
        }
    }
}

void umbra_gradient_lut(float *out, int lut_n, int n_stops, float const positions[],
                        float const colors[][4]) {
    for (int i = 0; i < lut_n; i++) {
        float const t = (float)i / (float)(lut_n - 1);
        int   seg = 0;
        for (int j = 1; j < n_stops; j++) {
            if (t >= positions[j]) {
                seg = j;
            }
        }
        if (seg >= n_stops - 1) {
            seg = n_stops - 2;
        }
        float const span = positions[seg + 1] - positions[seg];
        float f = 0;
        if (span > 0) {
            f = (t - positions[seg]) / span;
            if (f < 0) { f = 0; }
            if (f > 1) { f = 1; }
        }
        for (int ch = 0; ch < 4; ch++) {
            out[ch * lut_n + i] = colors[seg][ch] * (1 - f) + colors[seg + 1][ch] * f;
        }
    }
}

