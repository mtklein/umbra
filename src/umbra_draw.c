#include "../include/umbra_draw.h"
#include <math.h>
#include <stdint.h>

typedef struct umbra_builder builder;

struct umbra_builder *umbra_draw_build(umbra_shader_fn shader, umbra_coverage_fn coverage,
                                       umbra_blend_fn blend,
                                       umbra_draw_layout *layout) {
    builder  *builder = umbra_builder();
    umbra_val const x = umbra_x(builder);
    umbra_val const y = umbra_y(builder);
    umbra_val const xf = umbra_f32_from_i32(builder, x);
    umbra_val const yf = umbra_f32_from_i32(builder, y);

    int         const shader_off = umbra_uni_len(builder);
    umbra_color src = {
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
    };
    if (shader) {
        src = shader(builder, xf, yf);
    }

    int       const coverage_off = umbra_uni_len(builder);
    umbra_val cov = {0};
    if (coverage) {
        cov = coverage(builder, xf, yf);
    }

    umbra_color dst = {
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
    };
    if (blend || coverage) {
        dst = umbra_load_color(builder, (umbra_ptr){1, 0});
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

    umbra_store_color(builder, (umbra_ptr){1, 0}, out);

    if (layout) {
        layout->shader = shader_off;
        layout->coverage = coverage_off;
        layout->uni_len = umbra_uni_len(builder);
        layout->ps = umbra_max_ptr(builder) - 1;
    }

    return builder;
}

umbra_color umbra_shader_solid(builder *builder, umbra_val x, umbra_val y) {
    (void)x;
    (void)y;
    int       const fi = umbra_reserve(builder, 4);
    umbra_val const r = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi);
    umbra_val const g = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 1);
    umbra_val const b = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 2);
    umbra_val const a = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 3);
    return (umbra_color){r, g, b, a};
}

static umbra_val clamp01(builder *builder, umbra_val t) {
    return umbra_min_f32(builder, umbra_max_f32(builder, t, umbra_imm_f32(builder, 0.0f)),
                         umbra_imm_f32(builder, 1.0f));
}
static umbra_color clamp01_color(builder *builder, umbra_color c) {
    return (umbra_color){
        clamp01(builder, c.r), clamp01(builder, c.g),
        clamp01(builder, c.b), clamp01(builder, c.a),
    };
}

static umbra_val lerp_f(builder *builder, umbra_val p, umbra_val q, umbra_val t) {
    return umbra_add_f32(builder, p,
                         umbra_mul_f32(builder, umbra_sub_f32(builder, q, p), t));
}

static umbra_val linear_t_(builder *builder, int fi, umbra_val x, umbra_val y) {
    umbra_val const a = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi);
    umbra_val const b = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 1);
    umbra_val const c = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 2);
    umbra_val const t = umbra_add_f32(builder,
                                      umbra_add_f32(builder, umbra_mul_f32(builder, a, x),
                                                    umbra_mul_f32(builder, b, y)),
                                      c);
    return clamp01(builder, t);
}

static umbra_val radial_t_(builder *builder, int fi, umbra_val x, umbra_val y) {
    umbra_val const cx = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi);
    umbra_val const cy = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 1);
    umbra_val const inv_r = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 2);
    umbra_val const dx = umbra_sub_f32(builder, x, cx);
    umbra_val const dy = umbra_sub_f32(builder, y, cy);
    umbra_val const d2 = umbra_add_f32(builder, umbra_mul_f32(builder, dx, dx),
                                       umbra_mul_f32(builder, dy, dy));
    return clamp01(builder, umbra_mul_f32(builder, umbra_sqrt_f32(builder, d2), inv_r));
}

static umbra_color lerp_2stop_(builder *builder, umbra_val t, int fi) {
    umbra_val const r0 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi);
    umbra_val const g0 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 1);
    umbra_val const b0 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 2);
    umbra_val const a0 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 3);
    umbra_val const r1 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 4);
    umbra_val const g1 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 5);
    umbra_val const b1 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 6);
    umbra_val const a1 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 7);
    return (umbra_color){
        lerp_f(builder, r0, r1, t),
        lerp_f(builder, g0, g1, t),
        lerp_f(builder, b0, b1, t),
        lerp_f(builder, a0, a1, t),
    };
}

static umbra_color sample_lut_(builder *builder, umbra_val t_f32, int fi, umbra_ptr lut) {
    umbra_val const N_f = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 3);
    umbra_val const one_f = umbra_imm_f32(builder, 1.0f);
    umbra_val const two_f = umbra_imm_f32(builder, 2.0f);
    umbra_val const N_m1 = umbra_sub_f32(builder, N_f, one_f);
    umbra_val const N_m2 = umbra_sub_f32(builder, N_f, two_f);

    umbra_val const t_sc = umbra_mul_f32(builder, t_f32, N_m1);
    umbra_val const idx_f = umbra_min_f32(builder, umbra_floor_f32(builder, t_sc), N_m2);
    umbra_val const frac = umbra_sub_f32(builder, t_sc, idx_f);

    umbra_val const idx = umbra_i32_from_f32(builder, idx_f);
    umbra_val const base = umbra_shl_i32(builder, idx, umbra_imm_i32(builder, 2));
    umbra_val const nxt = umbra_add_i32(builder, base, umbra_imm_i32(builder, 4));

    umbra_val const off1 = umbra_imm_i32(builder, 1);
    umbra_val const off2 = umbra_imm_i32(builder, 2);
    umbra_val const off3 = umbra_imm_i32(builder, 3);
    umbra_val const r0 = umbra_gather_32(builder, lut, base);
    umbra_val const g0 = umbra_gather_32(builder, lut, umbra_add_i32(builder, base, off1));
    umbra_val const b0 = umbra_gather_32(builder, lut, umbra_add_i32(builder, base, off2));
    umbra_val const a0 = umbra_gather_32(builder, lut, umbra_add_i32(builder, base, off3));
    umbra_val const r1 = umbra_gather_32(builder, lut, nxt);
    umbra_val const g1 = umbra_gather_32(builder, lut, umbra_add_i32(builder, nxt, off1));
    umbra_val const b1 = umbra_gather_32(builder, lut, umbra_add_i32(builder, nxt, off2));
    umbra_val const a1 = umbra_gather_32(builder, lut, umbra_add_i32(builder, nxt, off3));

    return (umbra_color){
        lerp_f(builder, r0, r1, frac),
        lerp_f(builder, g0, g1, frac),
        lerp_f(builder, b0, b1, frac),
        lerp_f(builder, a0, a1, frac),
    };
}

umbra_color umbra_shader_linear_2(builder *builder, umbra_val x, umbra_val y) {
    int const fi = umbra_reserve(builder, 3);
    int const ci = umbra_reserve(builder, 8);
    return lerp_2stop_(builder, linear_t_(builder, fi, x, y), ci);
}
umbra_color umbra_shader_radial_2(builder *builder, umbra_val x, umbra_val y) {
    int const fi = umbra_reserve(builder, 3);
    int const ci = umbra_reserve(builder, 8);
    return lerp_2stop_(builder, radial_t_(builder, fi, x, y), ci);
}
umbra_color umbra_shader_linear_grad(builder *builder, umbra_val x, umbra_val y) {
    int       const fi = umbra_reserve(builder, 4);
    int       const lut_off = umbra_reserve_ptr(builder);
    umbra_ptr const lut = umbra_deref_ptr(builder, (umbra_ptr){0, 0}, lut_off);
    return sample_lut_(builder, linear_t_(builder, fi, x, y), fi, lut);
}
umbra_color umbra_shader_radial_grad(builder *builder, umbra_val x, umbra_val y) {
    int       const fi = umbra_reserve(builder, 4);
    int       const lut_off = umbra_reserve_ptr(builder);
    umbra_ptr const lut = umbra_deref_ptr(builder, (umbra_ptr){0, 0}, lut_off);
    return sample_lut_(builder, radial_t_(builder, fi, x, y), fi, lut);
}

umbra_color umbra_supersample(builder *builder, umbra_val x, umbra_val y,
                              umbra_shader_fn inner, int n) {
    static float const jitter[][2] = {
        {-0.375f, -0.125f}, {0.125f, -0.375f}, {0.375f, 0.125f}, {-0.125f, 0.375f},
        {-0.250f, 0.375f},  {0.250f, -0.250f}, {0.375f, 0.250f}, {-0.375f, -0.250f},
    };
    if (n < 1) { n = 1; }
    if (n > 8) { n = 8; }

    int         const saved = umbra_uni_len(builder);
    umbra_color sum = inner(builder, x, y);

    for (int s = 1; s < n; s++) {
        umbra_set_uni_len(builder, saved);
        umbra_val const sx = umbra_add_f32(builder, x, umbra_imm_f32(builder, jitter[s - 1][0]));
        umbra_val const sy = umbra_add_f32(builder, y, umbra_imm_f32(builder, jitter[s - 1][1]));
        umbra_color const c = inner(builder, sx, sy);
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

umbra_val umbra_coverage_rect(builder *builder, umbra_val x, umbra_val y) {
    int       const fi = umbra_reserve(builder, 4);
    umbra_val const l = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi);
    umbra_val const t = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 1);
    umbra_val const r = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 2);
    umbra_val const b = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 3);
    umbra_val const inside = umbra_and_i32(builder,
                                           umbra_and_i32(builder, umbra_ge_f32(builder, x, l),
                                                         umbra_lt_f32(builder, x, r)),
                                           umbra_and_i32(builder, umbra_ge_f32(builder, y, t),
                                                         umbra_lt_f32(builder, y, b)));
    umbra_val const one_f = umbra_imm_f32(builder, 1.0f);
    umbra_val const zero_f = umbra_imm_f32(builder, 0.0f);
    return umbra_sel_i32(builder, inside, one_f, zero_f);
}

umbra_val umbra_coverage_bitmap(builder *builder, umbra_val x, umbra_val y) {
    (void)x;
    (void)y;
    int       const bmp_off = umbra_reserve_ptr(builder);
    umbra_ptr const bmp = umbra_deref_ptr(builder, (umbra_ptr){0, 0}, bmp_off);
    umbra_val const val = umbra_i32_from_s16(builder, umbra_load_16(builder, bmp));
    umbra_val const inv255 = umbra_imm_f32(builder, 1.0f / 255.0f);
    return umbra_mul_f32(builder, umbra_f32_from_i32(builder, val), inv255);
}

umbra_val umbra_coverage_sdf(builder *builder, umbra_val x, umbra_val y) {
    (void)x;
    (void)y;
    int       const bmp_off = umbra_reserve_ptr(builder);
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

umbra_val umbra_coverage_wind(builder *builder, umbra_val x, umbra_val y) {
    (void)x;
    (void)y;
    int       const off = umbra_reserve_ptr(builder);
    umbra_ptr const w = umbra_deref_ptr(builder, (umbra_ptr){0, 0}, off);
    umbra_val const raw = umbra_load_32(builder, w);
    return umbra_min_f32(builder, umbra_abs_f32(builder, raw),
                         umbra_imm_f32(builder, 1.0f));
}

umbra_val umbra_coverage_bitmap_matrix(builder *builder, umbra_val x, umbra_val y) {
    int       const fi = umbra_reserve(builder, 11);
    int       const bmp_off = umbra_reserve_ptr(builder);
    umbra_ptr const bmp = umbra_deref_ptr(builder, (umbra_ptr){0, 0}, bmp_off);

    umbra_val const m0 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi);
    umbra_val const m1 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 1);
    umbra_val const m2 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 2);
    umbra_val const m3 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 3);
    umbra_val const m4 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 4);
    umbra_val const m5 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 5);
    umbra_val const m6 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 6);
    umbra_val const m7 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 7);
    umbra_val const m8 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 8);
    umbra_val const bw = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 9);
    umbra_val const bh = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 10);

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
    umbra_val const in = umbra_and_i32(builder,
                                       umbra_and_i32(builder, umbra_ge_f32(builder, xp, zero_f),
                                                     umbra_lt_f32(builder, xp, bw)),
                                       umbra_and_i32(builder, umbra_ge_f32(builder, yp, zero_f),
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

    return umbra_sel_i32(builder, in, cov, umbra_imm_f32(builder, 0.0f));
}

static umbra_color umbra_load_8888(builder *builder, umbra_ptr ptr) {
    umbra_val ch[4];
    umbra_load_8x4(builder, ptr, ch);
    umbra_val const inv255 = umbra_imm_f32(builder, 1.0f / 255.0f);
    return (umbra_color){
        umbra_mul_f32(builder, umbra_f32_from_i32(builder, ch[0]), inv255),
        umbra_mul_f32(builder, umbra_f32_from_i32(builder, ch[1]), inv255),
        umbra_mul_f32(builder, umbra_f32_from_i32(builder, ch[2]), inv255),
        umbra_mul_f32(builder, umbra_f32_from_i32(builder, ch[3]), inv255),
    };
}

static void umbra_store_8888(builder *builder, umbra_ptr ptr, umbra_color c) {
    c = clamp01_color(builder, c);
    umbra_val const scale = umbra_imm_f32(builder, 255.0f);
    umbra_val const ch[4] = {
        umbra_round_i32(builder, umbra_mul_f32(builder, c.r, scale)),
        umbra_round_i32(builder, umbra_mul_f32(builder, c.g, scale)),
        umbra_round_i32(builder, umbra_mul_f32(builder, c.b, scale)),
        umbra_round_i32(builder, umbra_mul_f32(builder, c.a, scale)),
    };
    umbra_store_8x4(builder, ptr, ch);
}

static umbra_color umbra_load_565(builder *builder, umbra_ptr ptr) {
    umbra_val const px = umbra_i32_from_u16(builder, umbra_load_16(builder, ptr));
    umbra_val const r32 = umbra_shr_u32(builder, px, umbra_imm_i32(builder, 11));
    umbra_val const g32 = umbra_and_i32(builder,
                                        umbra_shr_u32(builder, px, umbra_imm_i32(builder, 5)),
                                        umbra_imm_i32(builder, 0x3f));
    umbra_val const b32 = umbra_and_i32(builder, px, umbra_imm_i32(builder, 0x1f));
    umbra_val const inv31 = umbra_imm_f32(builder, 1.0f / 31.0f);
    umbra_val const inv63 = umbra_imm_f32(builder, 1.0f / 63.0f);
    return (umbra_color){
        umbra_mul_f32(builder, umbra_f32_from_i32(builder, r32), inv31),
        umbra_mul_f32(builder, umbra_f32_from_i32(builder, g32), inv63),
        umbra_mul_f32(builder, umbra_f32_from_i32(builder, b32), inv31),
        umbra_imm_f32(builder, 1.0f),
    };
}

static void umbra_store_565(builder *builder, umbra_ptr ptr, umbra_color c) {
    c = clamp01_color(builder, c);
    umbra_val const s31 = umbra_imm_f32(builder, 31.0f);
    umbra_val const s63 = umbra_imm_f32(builder, 63.0f);
    umbra_val const r = umbra_round_i32(builder, umbra_mul_f32(builder, c.r, s31));
    umbra_val const g = umbra_round_i32(builder, umbra_mul_f32(builder, c.g, s63));
    umbra_val const b = umbra_round_i32(builder, umbra_mul_f32(builder, c.b, s31));
    umbra_val const px = umbra_pack(builder, umbra_pack(builder, b, g, 5), r, 11);
    umbra_store_16(builder, ptr, umbra_i16_from_i32(builder, px));
}

static umbra_color umbra_load_1010102(builder *builder, umbra_ptr ptr) {
    umbra_val const px = umbra_load_32(builder, ptr);
    umbra_val const m10 = umbra_imm_i32(builder, 0x3ff);
    umbra_val const r32 = umbra_and_i32(builder, px, m10);
    umbra_val const s10 = umbra_imm_i32(builder, 10);
    umbra_val const g32 = umbra_and_i32(builder, umbra_shr_u32(builder, px, s10), m10);
    umbra_val const b32 =
        umbra_and_i32(builder, umbra_shr_u32(builder, px, umbra_imm_i32(builder, 20)), m10);
    umbra_val const a32 = umbra_shr_u32(builder, px, umbra_imm_i32(builder, 30));
    umbra_val const inv1023 = umbra_imm_f32(builder, 1.0f / 1023.0f);
    umbra_val const inv3 = umbra_imm_f32(builder, 1.0f / 3.0f);
    return (umbra_color){
        umbra_mul_f32(builder, umbra_f32_from_i32(builder, r32), inv1023),
        umbra_mul_f32(builder, umbra_f32_from_i32(builder, g32), inv1023),
        umbra_mul_f32(builder, umbra_f32_from_i32(builder, b32), inv1023),
        umbra_mul_f32(builder, umbra_f32_from_i32(builder, a32), inv3),
    };
}

static void umbra_store_1010102(builder *builder, umbra_ptr ptr, umbra_color c) {
    c = clamp01_color(builder, c);
    umbra_val const s1023 = umbra_imm_f32(builder, 1023.0f);
    umbra_val const s3 = umbra_imm_f32(builder, 3.0f);
    umbra_val const r = umbra_round_i32(builder, umbra_mul_f32(builder, c.r, s1023));
    umbra_val const g = umbra_round_i32(builder, umbra_mul_f32(builder, c.g, s1023));
    umbra_val const b = umbra_round_i32(builder, umbra_mul_f32(builder, c.b, s1023));
    umbra_val const a = umbra_round_i32(builder, umbra_mul_f32(builder, c.a, s3));
    umbra_val const px = umbra_pack(builder,
                              umbra_pack(builder, umbra_pack(builder, r, g, 10), b, 20),
                              a, 30);
    umbra_store_32(builder, ptr, px);
}

static umbra_color umbra_load_fp16(builder *builder, umbra_ptr ptr) {
    umbra_val lo, hi;
    umbra_load_32x2(builder, ptr, &lo, &hi);
    umbra_val const s16 = umbra_imm_i32(builder, 16);
    return (umbra_color){
        umbra_f32_from_f16(builder, umbra_i16_from_i32(builder, lo)),
        umbra_f32_from_f16(builder, umbra_i16_from_i32(builder, umbra_shr_u32(builder, lo, s16))),
        umbra_f32_from_f16(builder, umbra_i16_from_i32(builder, hi)),
        umbra_f32_from_f16(builder, umbra_i16_from_i32(builder, umbra_shr_u32(builder, hi, s16))),
    };
}

static void umbra_store_fp16(builder *builder, umbra_ptr ptr, umbra_color c) {
    umbra_val const lo = umbra_pack(builder, umbra_i32_from_u16(builder, umbra_f16_from_f32(builder, c.r)),
                                    umbra_i32_from_u16(builder, umbra_f16_from_f32(builder, c.g)), 16);
    umbra_val const hi = umbra_pack(builder, umbra_i32_from_u16(builder, umbra_f16_from_f32(builder, c.b)),
                                    umbra_i32_from_u16(builder, umbra_f16_from_f32(builder, c.a)), 16);
    umbra_store_32x2(builder, ptr, lo, hi);
}

static umbra_color umbra_load_fp16_planar(builder *builder, umbra_ptr ptr) {
    return (umbra_color){
        umbra_f32_from_f16(builder, umbra_load_16(builder, ptr)),
        umbra_f32_from_f16(builder, umbra_load_16(builder, (umbra_ptr){ptr.ix + 1, 0})),
        umbra_f32_from_f16(builder, umbra_load_16(builder, (umbra_ptr){ptr.ix + 2, 0})),
        umbra_f32_from_f16(builder, umbra_load_16(builder, (umbra_ptr){ptr.ix + 3, 0})),
    };
}

static void umbra_store_fp16_planar(builder *builder, umbra_ptr ptr, umbra_color c) {
    umbra_store_16(builder, ptr, umbra_f16_from_f32(builder, c.r));
    umbra_store_16(builder, (umbra_ptr){ptr.ix + 1, 0}, umbra_f16_from_f32(builder, c.g));
    umbra_store_16(builder, (umbra_ptr){ptr.ix + 2, 0}, umbra_f16_from_f32(builder, c.b));
    umbra_store_16(builder, (umbra_ptr){ptr.ix + 3, 0}, umbra_f16_from_f32(builder, c.a));
}

umbra_format const umbra_format_8888        = {4, umbra_load_8888,        umbra_store_8888};
umbra_format const umbra_format_565         = {2, umbra_load_565,         umbra_store_565};
umbra_format const umbra_format_1010102     = {4, umbra_load_1010102,     umbra_store_1010102};
umbra_format const umbra_format_fp16        = {8, umbra_load_fp16,        umbra_store_fp16};
umbra_format const umbra_format_fp16_planar = {2, umbra_load_fp16_planar, umbra_store_fp16_planar};

static umbra_color load_srgb_8888(builder *builder, umbra_ptr ptr) {
    return umbra_transfer_invert(builder, umbra_load_8888(builder, ptr), &umbra_transfer_srgb);
}
static void store_srgb_8888(builder *builder, umbra_ptr ptr, umbra_color c) {
    umbra_store_8888(builder, ptr, umbra_transfer_apply(builder, c, &umbra_transfer_srgb));
}
umbra_format const umbra_format_srgb_8888 = {4, load_srgb_8888, store_srgb_8888};

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
            out[i * 4 + ch] = colors[idx][ch] * (1 - f) + colors[idx + 1][ch] * f;
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
            out[i * 4 + ch] = colors[seg][ch] * (1 - f) + colors[seg + 1][ch] * f;
        }
    }
}

umbra_transfer const umbra_transfer_srgb = {
    .a = 1.055f,
    .b = -0.055f,
    .c = 12.92f,
    .d = 0.0031308f,
    .e = 0.04045f,
    .f = 0,
    .g = 2.4f,
};


// Invert: encoded -> linear.
// x >= e ? pow((x - b) / a, g) : (x - f) / c
static float tf_invert_scalar(umbra_transfer const *tf, float x) {
    if (x >= tf->e) {
        return powf((x - tf->b) / tf->a, tf->g);
    }
    return (x - tf->f) / tf->c;
}

// Apply: linear -> encoded.
// x >= d ? a * pow(x, 1/g) + b : c * x + f
static float tf_apply_scalar(umbra_transfer const *tf, float x) {
    if (x >= tf->d) {
        return tf->a * powf(x, 1.0f / tf->g) + tf->b;
    }
    return tf->c * x + tf->f;
}

void umbra_transfer_lut_invert(float out[256], umbra_transfer const *tf) {
    for (int i = 0; i < 256; i++) { out[i] = tf_invert_scalar(tf, (float)i / 255.0f); }
}

void umbra_transfer_lut_apply(float out[256], umbra_transfer const *tf) {
    for (int i = 0; i < 256; i++) { out[i] = tf_apply_scalar(tf, (float)i / 255.0f); }
}

#define POLY_N 10

static void fit_poly(double gamma, int n, double coeffs[]) {
    enum { N = 512 };
    double ata[POLY_N][POLY_N] = {{0}};
    double atb[POLY_N] = {0};
    for (int i = 0; i < N; i++) {
        double const x = (double)i / (double)(N - 1);
        double const y = pow(x, gamma);
        double xp = 1;
        for (int r = 0; r < n; r++) {
            double xq = 1;
            for (int c = 0; c < n; c++) {
                ata[r][c] += xp * xq;
                xq *= x;
            }
            atb[r] += xp * y;
            xp *= x;
        }
    }
    for (int k = 0; k < n; k++) {
        int mx = k;
        for (int r = k + 1; r < n; r++) {
            if (fabs(ata[r][k]) > fabs(ata[mx][k])) {
                mx = r;
            }
        }
        for (int c = 0; c < n; c++) {
            double const t = ata[k][c];
            ata[k][c] = ata[mx][c];
            ata[mx][c] = t;
        }
        {
            double const t = atb[k];
            atb[k] = atb[mx];
            atb[mx] = t;
        }
        for (int r = k + 1; r < n; r++) {
            double const f = ata[r][k] / ata[k][k];
            for (int c = k; c < n; c++) {
                ata[r][c] -= f * ata[k][c];
            }
            atb[r] -= f * atb[k];
        }
    }
    for (int k = n - 1; k >= 0; k--) {
        double s = atb[k];
        for (int c = k + 1; c < n; c++) {
            s -= ata[k][c] * coeffs[c];
        }
        coeffs[k] = s / ata[k][k];
    }
}

static umbra_val eval_poly_ir(builder *builder, umbra_val x, int n, double const coeffs[]) {
    umbra_val r = umbra_imm_f32(builder, (float)coeffs[n - 1]);
    for (int i = n - 2; i >= 0; i--) {
        r = umbra_add_f32(builder, umbra_mul_f32(builder, r, x),
                          umbra_imm_f32(builder, (float)coeffs[i]));
    }
    return r;
}

// IR for invert: encoded -> linear.
// x >= e ? pow((x - b) / a, g) : (x - f) / c
umbra_color umbra_transfer_invert(builder *builder, umbra_color c,
                                  umbra_transfer const *tf) {
    double poly[POLY_N];
    int    const deg = 7;
    fit_poly((double)tf->g, deg, poly);

    float const inv_c = tf->c > 0 ? 1.0f / tf->c : 0;

    for (int ch = 0; ch < 3; ch++) {
        umbra_val *h = ch == 0 ? &c.r : ch == 1 ? &c.g : &c.b;
        umbra_val  const x = clamp01(builder, *h);

        // Linear segment: (x - f) / c
        umbra_val const lin =
            umbra_mul_f32(builder,
                          umbra_sub_f32(builder, x, umbra_imm_f32(builder, tf->f)),
                          umbra_imm_f32(builder, inv_c));

        // Curved segment: pow((x - b) / a, g)
        // t = (x - b) / a, then poly approx of pow(t, g)
        umbra_val t = umbra_mul_f32(builder,
                                    umbra_sub_f32(builder, x, umbra_imm_f32(builder, tf->b)),
                                    umbra_imm_f32(builder, 1.0f / tf->a));
        t = umbra_max_f32(builder, t, umbra_imm_f32(builder, 0.0f));
        umbra_val const cur = eval_poly_ir(builder, t, deg, poly);

        umbra_val const mask = umbra_ge_f32(builder, x, umbra_imm_f32(builder, tf->e));
        *h = umbra_sel_i32(builder, mask, cur, lin);
    }
    return c;
}

// IR for apply: linear -> encoded.
// x >= d ? a * pow(x, 1/g) + b : c * x + f
umbra_color umbra_transfer_apply(builder *builder, umbra_color c, umbra_transfer const *tf) {
    double poly[POLY_N];
    int    const deg = 10;
    fit_poly(2.0 / (double)tf->g, deg, poly);

    for (int ch = 0; ch < 3; ch++) {
        umbra_val *h = ch == 0 ? &c.r : ch == 1 ? &c.g : &c.b;
        umbra_val  const x = clamp01(builder, *h);

        // Linear segment: c * x + f
        umbra_val const lin =
            umbra_add_f32(builder,
                          umbra_mul_f32(builder, x, umbra_imm_f32(builder, tf->c)),
                          umbra_imm_f32(builder, tf->f));

        // Curved segment: a * pow(x, 1/g) + b
        // poly approx of pow(x, 1/g) via sqrt: sqrt(x)^(2/g)
        umbra_val t = umbra_max_f32(builder, x, umbra_imm_f32(builder, 0.0f));
        umbra_val const s = umbra_sqrt_f32(builder, t);
        umbra_val const cur =
            umbra_add_f32(builder,
                          umbra_mul_f32(builder, eval_poly_ir(builder, s, deg, poly),
                                        umbra_imm_f32(builder, tf->a)),
                          umbra_imm_f32(builder, tf->b));

        umbra_val const mask = umbra_ge_f32(builder, x, umbra_imm_f32(builder, tf->d));
        *h = umbra_sel_i32(builder, mask, cur, lin);
    }
    return c;
}
