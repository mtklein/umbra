#include "../include/umbra_draw.h"
#include <math.h>
#include <stdint.h>

typedef struct umbra_builder builder;

struct umbra_builder *umbra_draw_build(umbra_shader_fn shader, umbra_coverage_fn coverage,
                                       umbra_blend_fn blend, umbra_format format,
                                       umbra_draw_layout *layout) {
    umbra_load_fn  load  = format.load;
    umbra_store_fn store = format.store;
    builder  *builder = umbra_builder();
    umbra_val x = umbra_x(builder);
    umbra_val y = umbra_y(builder);
    umbra_val xf = umbra_f32_from_i32(builder, x);
    umbra_val yf = umbra_f32_from_i32(builder, y);

    int         shader_off = umbra_uni_len(builder);
    umbra_color src = {
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
    };
    if (shader) { src = shader(builder, xf, yf); }

    int       coverage_off = umbra_uni_len(builder);
    umbra_val cov = {0};
    if (coverage) { cov = coverage(builder, xf, yf); }

    umbra_color dst = {
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
    };
    if (load) { dst = load(builder, (umbra_ptr){1, 0}); }

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

    if (store) { store(builder, (umbra_ptr){1, 0}, out); }

    if (layout) {
        layout->shader = shader_off;
        layout->coverage = coverage_off;
        layout->uni_len = umbra_uni_len(builder);
        layout->ps = umbra_max_ptr(builder) - 1;
        layout->pixel_bytes = format.pixel_bytes;
    }

    return builder;
}

umbra_color umbra_shader_solid(builder *builder, umbra_val x, umbra_val y) {
    (void)x;
    (void)y;
    int       fi = umbra_reserve(builder, 4);
    umbra_val r = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi);
    umbra_val g = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 1);
    umbra_val b = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 2);
    umbra_val a = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 3);
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

static umbra_val linear_t_(builder *builder, int fi, umbra_val x, umbra_val y) {
    umbra_val a = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi);
    umbra_val b = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 1);
    umbra_val c = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 2);
    umbra_val t = umbra_add_f32(builder,
                                umbra_add_f32(builder, umbra_mul_f32(builder, a, x),
                                              umbra_mul_f32(builder, b, y)),
                                c);
    return clamp01(builder, t);
}

static umbra_val radial_t_(builder *builder, int fi, umbra_val x, umbra_val y) {
    umbra_val cx = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi);
    umbra_val cy = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 1);
    umbra_val inv_r = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 2);
    umbra_val dx = umbra_sub_f32(builder, x, cx);
    umbra_val dy = umbra_sub_f32(builder, y, cy);
    umbra_val d2 = umbra_add_f32(builder, umbra_mul_f32(builder, dx, dx),
                                 umbra_mul_f32(builder, dy, dy));
    return clamp01(builder, umbra_mul_f32(builder, umbra_sqrt_f32(builder, d2), inv_r));
}

static umbra_color lerp_2stop_(builder *builder, umbra_val t, int fi) {
    umbra_val r0 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi);
    umbra_val g0 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 1);
    umbra_val b0 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 2);
    umbra_val a0 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 3);
    umbra_val r1 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 4);
    umbra_val g1 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 5);
    umbra_val b1 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 6);
    umbra_val a1 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 7);
    return (umbra_color){
        lerp_f(builder, r0, r1, t),
        lerp_f(builder, g0, g1, t),
        lerp_f(builder, b0, b1, t),
        lerp_f(builder, a0, a1, t),
    };
}

static umbra_color sample_lut_(builder *builder, umbra_val t_f32, int fi, umbra_ptr lut) {
    umbra_val N_f = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 3);
    umbra_val one_f = umbra_imm_f32(builder, 1.0f);
    umbra_val two_f = umbra_imm_f32(builder, 2.0f);
    umbra_val N_m1 = umbra_sub_f32(builder, N_f, one_f);
    umbra_val N_m2 = umbra_sub_f32(builder, N_f, two_f);

    umbra_val t_sc = umbra_mul_f32(builder, t_f32, N_m1);
    umbra_val idx_f = umbra_min_f32(builder, umbra_floor_f32(builder, t_sc), N_m2);
    umbra_val frac = umbra_sub_f32(builder, t_sc, idx_f);

    umbra_val idx = umbra_i32_from_f32(builder, idx_f);
    umbra_val base = umbra_shl_i32(builder, idx, umbra_imm_i32(builder, 2));
    umbra_val nxt = umbra_add_i32(builder, base, umbra_imm_i32(builder, 4));

    umbra_val off1 = umbra_imm_i32(builder, 1);
    umbra_val off2 = umbra_imm_i32(builder, 2);
    umbra_val off3 = umbra_imm_i32(builder, 3);
    umbra_val r0 = umbra_gather_32(builder, lut, base);
    umbra_val g0 = umbra_gather_32(builder, lut, umbra_add_i32(builder, base, off1));
    umbra_val b0 = umbra_gather_32(builder, lut, umbra_add_i32(builder, base, off2));
    umbra_val a0 = umbra_gather_32(builder, lut, umbra_add_i32(builder, base, off3));
    umbra_val r1 = umbra_gather_32(builder, lut, nxt);
    umbra_val g1 = umbra_gather_32(builder, lut, umbra_add_i32(builder, nxt, off1));
    umbra_val b1 = umbra_gather_32(builder, lut, umbra_add_i32(builder, nxt, off2));
    umbra_val a1 = umbra_gather_32(builder, lut, umbra_add_i32(builder, nxt, off3));

    return (umbra_color){
        lerp_f(builder, r0, r1, frac),
        lerp_f(builder, g0, g1, frac),
        lerp_f(builder, b0, b1, frac),
        lerp_f(builder, a0, a1, frac),
    };
}

umbra_color umbra_shader_linear_2(builder *builder, umbra_val x, umbra_val y) {
    int fi = umbra_reserve(builder, 3);
    int ci = umbra_reserve(builder, 8);
    return lerp_2stop_(builder, linear_t_(builder, fi, x, y), ci);
}
umbra_color umbra_shader_radial_2(builder *builder, umbra_val x, umbra_val y) {
    int fi = umbra_reserve(builder, 3);
    int ci = umbra_reserve(builder, 8);
    return lerp_2stop_(builder, radial_t_(builder, fi, x, y), ci);
}
umbra_color umbra_shader_linear_grad(builder *builder, umbra_val x, umbra_val y) {
    int       fi = umbra_reserve(builder, 4);
    int       lut_off = umbra_reserve_ptr(builder);
    umbra_ptr lut = umbra_deref_ptr(builder, (umbra_ptr){0, 0}, lut_off);
    return sample_lut_(builder, linear_t_(builder, fi, x, y), fi, lut);
}
umbra_color umbra_shader_radial_grad(builder *builder, umbra_val x, umbra_val y) {
    int       fi = umbra_reserve(builder, 4);
    int       lut_off = umbra_reserve_ptr(builder);
    umbra_ptr lut = umbra_deref_ptr(builder, (umbra_ptr){0, 0}, lut_off);
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

    int         saved = umbra_uni_len(builder);
    umbra_color sum = inner(builder, x, y);

    for (int s = 1; s < n; s++) {
        umbra_set_uni_len(builder, saved);
        umbra_val sx = umbra_add_f32(builder, x, umbra_imm_f32(builder, jitter[s - 1][0]));
        umbra_val sy = umbra_add_f32(builder, y, umbra_imm_f32(builder, jitter[s - 1][1]));
        umbra_color c = inner(builder, sx, sy);
        sum.r = umbra_add_f32(builder, sum.r, c.r);
        sum.g = umbra_add_f32(builder, sum.g, c.g);
        sum.b = umbra_add_f32(builder, sum.b, c.b);
        sum.a = umbra_add_f32(builder, sum.a, c.a);
    }

    umbra_val inv = umbra_imm_f32(builder, 1.0f / (float)n);
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
    umbra_val one = umbra_imm_f32(builder, 1.0f);
    umbra_val inv_a = umbra_sub_f32(builder, one, src.a);
    return (umbra_color){
        umbra_add_f32(builder, src.r, umbra_mul_f32(builder, dst.r, inv_a)),
        umbra_add_f32(builder, src.g, umbra_mul_f32(builder, dst.g, inv_a)),
        umbra_add_f32(builder, src.b, umbra_mul_f32(builder, dst.b, inv_a)),
        umbra_add_f32(builder, src.a, umbra_mul_f32(builder, dst.a, inv_a)),
    };
}

umbra_color umbra_blend_dstover(builder *builder, umbra_color src, umbra_color dst) {
    umbra_val one = umbra_imm_f32(builder, 1.0f);
    umbra_val inv_a = umbra_sub_f32(builder, one, dst.a);
    return (umbra_color){
        umbra_add_f32(builder, dst.r, umbra_mul_f32(builder, src.r, inv_a)),
        umbra_add_f32(builder, dst.g, umbra_mul_f32(builder, src.g, inv_a)),
        umbra_add_f32(builder, dst.b, umbra_mul_f32(builder, src.b, inv_a)),
        umbra_add_f32(builder, dst.a, umbra_mul_f32(builder, src.a, inv_a)),
    };
}

umbra_color umbra_blend_multiply(builder *builder, umbra_color src, umbra_color dst) {
    umbra_val one = umbra_imm_f32(builder, 1.0f);
    umbra_val inv_sa = umbra_sub_f32(builder, one, src.a);
    umbra_val inv_da = umbra_sub_f32(builder, one, dst.a);
    umbra_val r = umbra_add_f32(builder, umbra_mul_f32(builder, src.r, dst.r),
                                umbra_add_f32(builder, umbra_mul_f32(builder, src.r, inv_da),
                                              umbra_mul_f32(builder, dst.r, inv_sa)));
    umbra_val g = umbra_add_f32(builder, umbra_mul_f32(builder, src.g, dst.g),
                                umbra_add_f32(builder, umbra_mul_f32(builder, src.g, inv_da),
                                              umbra_mul_f32(builder, dst.g, inv_sa)));
    umbra_val b = umbra_add_f32(builder, umbra_mul_f32(builder, src.b, dst.b),
                                umbra_add_f32(builder, umbra_mul_f32(builder, src.b, inv_da),
                                              umbra_mul_f32(builder, dst.b, inv_sa)));
    umbra_val a = umbra_add_f32(builder, umbra_mul_f32(builder, src.a, dst.a),
                                umbra_add_f32(builder, umbra_mul_f32(builder, src.a, inv_da),
                                              umbra_mul_f32(builder, dst.a, inv_sa)));
    return (umbra_color){r, g, b, a};
}

umbra_val umbra_coverage_rect(builder *builder, umbra_val x, umbra_val y) {
    int       fi = umbra_reserve(builder, 4);
    umbra_val l = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi);
    umbra_val t = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 1);
    umbra_val r = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 2);
    umbra_val b = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 3);
    umbra_val inside = umbra_and_i32(builder,
                                     umbra_and_i32(builder, umbra_ge_f32(builder, x, l),
                                                   umbra_lt_f32(builder, x, r)),
                                     umbra_and_i32(builder, umbra_ge_f32(builder, y, t),
                                                   umbra_lt_f32(builder, y, b)));
    umbra_val one_f = umbra_imm_f32(builder, 1.0f);
    umbra_val zero_f = umbra_imm_f32(builder, 0.0f);
    return umbra_sel_i32(builder, inside, one_f, zero_f);
}

umbra_val umbra_coverage_bitmap(builder *builder, umbra_val x, umbra_val y) {
    (void)x;
    (void)y;
    int       bmp_off = umbra_reserve_ptr(builder);
    umbra_ptr bmp = umbra_deref_ptr(builder, (umbra_ptr){0, 0}, bmp_off);
    umbra_val val = umbra_i32_from_s16(builder, umbra_load_16(builder, bmp));
    umbra_val inv255 = umbra_imm_f32(builder, 1.0f / 255.0f);
    return umbra_mul_f32(builder, umbra_f32_from_i32(builder, val), inv255);
}

umbra_val umbra_coverage_sdf(builder *builder, umbra_val x, umbra_val y) {
    (void)x;
    (void)y;
    int       bmp_off = umbra_reserve_ptr(builder);
    umbra_ptr bmp = umbra_deref_ptr(builder, (umbra_ptr){0, 0}, bmp_off);
    umbra_val raw = umbra_i32_from_s16(builder, umbra_load_16(builder, bmp));
    umbra_val inv255 = umbra_imm_f32(builder, 1.0f / 255.0f);
    umbra_val dist = umbra_mul_f32(builder, umbra_f32_from_i32(builder, raw), inv255);
    umbra_val lo = umbra_imm_f32(builder, 0.4375f);
    umbra_val scale = umbra_imm_f32(builder, 8.0f);
    umbra_val shifted = umbra_sub_f32(builder, dist, lo);
    umbra_val scaled = umbra_mul_f32(builder, shifted, scale);
    umbra_val zero = umbra_imm_f32(builder, 0.0f);
    umbra_val one = umbra_imm_f32(builder, 1.0f);
    return umbra_min_f32(builder, umbra_max_f32(builder, scaled, zero), one);
}

umbra_val umbra_coverage_wind(builder *builder, umbra_val x, umbra_val y) {
    (void)x;
    (void)y;
    int       off = umbra_reserve_ptr(builder);
    umbra_ptr w = umbra_deref_ptr(builder, (umbra_ptr){0, 0}, off);
    umbra_val raw = umbra_load_32(builder, w);
    return umbra_min_f32(builder, umbra_abs_f32(builder, raw),
                         umbra_imm_f32(builder, 1.0f));
}

umbra_val umbra_coverage_bitmap_matrix(builder *builder, umbra_val x, umbra_val y) {
    int       fi = umbra_reserve(builder, 11);
    int       bmp_off = umbra_reserve_ptr(builder);
    umbra_ptr bmp = umbra_deref_ptr(builder, (umbra_ptr){0, 0}, bmp_off);

    umbra_val m0 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi);
    umbra_val m1 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 1);
    umbra_val m2 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 2);
    umbra_val m3 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 3);
    umbra_val m4 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 4);
    umbra_val m5 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 5);
    umbra_val m6 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 6);
    umbra_val m7 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 7);
    umbra_val m8 = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 8);
    umbra_val bw = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 9);
    umbra_val bh = umbra_uniform_32(builder, (umbra_ptr){0, 0}, fi + 10);

    umbra_val w = umbra_add_f32(builder,
                                umbra_add_f32(builder, umbra_mul_f32(builder, m6, x),
                                              umbra_mul_f32(builder, m7, y)),
                                m8);
    umbra_val xp =
        umbra_div_f32(builder,
                      umbra_add_f32(builder,
                                    umbra_add_f32(builder, umbra_mul_f32(builder, m0, x),
                                                  umbra_mul_f32(builder, m1, y)),
                                    m2),
                      w);
    umbra_val yp =
        umbra_div_f32(builder,
                      umbra_add_f32(builder,
                                    umbra_add_f32(builder, umbra_mul_f32(builder, m3, x),
                                                  umbra_mul_f32(builder, m4, y)),
                                    m5),
                      w);

    umbra_val zero_f = umbra_imm_f32(builder, 0.0f);
    umbra_val in = umbra_and_i32(builder,
                                 umbra_and_i32(builder, umbra_ge_f32(builder, xp, zero_f),
                                               umbra_lt_f32(builder, xp, bw)),
                                 umbra_and_i32(builder, umbra_ge_f32(builder, yp, zero_f),
                                               umbra_lt_f32(builder, yp, bh)));

    umbra_val one_f = umbra_imm_f32(builder, 1.0f);
    umbra_val xc = umbra_min_f32(builder, umbra_max_f32(builder, xp, zero_f),
                                 umbra_sub_f32(builder, bw, one_f));
    umbra_val yc = umbra_min_f32(builder, umbra_max_f32(builder, yp, zero_f),
                                 umbra_sub_f32(builder, bh, one_f));
    umbra_val xi = umbra_floor_i32(builder, xc);
    umbra_val yi = umbra_floor_i32(builder, yc);
    umbra_val bwi = umbra_floor_i32(builder, bw);
    umbra_val idx = umbra_add_i32(builder, umbra_mul_i32(builder, yi, bwi), xi);

    umbra_val val = umbra_i32_from_s16(builder, umbra_gather_16(builder, bmp, idx));
    umbra_val inv255 = umbra_imm_f32(builder, 1.0f / 255.0f);
    umbra_val cov = umbra_mul_f32(builder, umbra_f32_from_i32(builder, val), inv255);

    return umbra_sel_i32(builder, in, cov, umbra_imm_f32(builder, 0.0f));
}

static umbra_color umbra_load_8888(builder *builder, umbra_ptr ptr) {
    umbra_val px = umbra_load_32(builder, ptr), mask = umbra_imm_i32(builder, 0xFF);
    umbra_val ch[4] = {
        umbra_and_i32(builder, px, mask),
        umbra_and_i32(builder, umbra_shr_u32(builder, px, umbra_imm_i32(builder, 8)), mask),
        umbra_and_i32(builder, umbra_shr_u32(builder, px, umbra_imm_i32(builder, 16)), mask),
        umbra_shr_u32(builder, px, umbra_imm_i32(builder, 24)),
    };
    umbra_val inv255 = umbra_imm_f32(builder, 1.0f / 255.0f);
    return (umbra_color){
        umbra_mul_f32(builder, umbra_f32_from_i32(builder, ch[0]), inv255),
        umbra_mul_f32(builder, umbra_f32_from_i32(builder, ch[1]), inv255),
        umbra_mul_f32(builder, umbra_f32_from_i32(builder, ch[2]), inv255),
        umbra_mul_f32(builder, umbra_f32_from_i32(builder, ch[3]), inv255),
    };
}

static void umbra_store_8888(builder *builder, umbra_ptr ptr, umbra_color c) {
    umbra_val scale = umbra_imm_f32(builder, 255.0f);
    umbra_val ch[4] = {
        umbra_round_i32(builder, umbra_mul_f32(builder, c.r, scale)),
        umbra_round_i32(builder, umbra_mul_f32(builder, c.g, scale)),
        umbra_round_i32(builder, umbra_mul_f32(builder, c.b, scale)),
        umbra_round_i32(builder, umbra_mul_f32(builder, c.a, scale)),
    };
    umbra_val mask = umbra_imm_i32(builder, 0xFF);
    umbra_val px = umbra_and_i32(builder, ch[0], mask);
    px = umbra_pack(builder, px, umbra_and_i32(builder, ch[1], mask), 8);
    px = umbra_pack(builder, px, umbra_and_i32(builder, ch[2], mask), 16);
    px = umbra_pack(builder, px, ch[3], 24);
    umbra_store_32(builder, ptr, px);
}

static umbra_color umbra_load_565(builder *builder, umbra_ptr ptr) {
    umbra_val px = umbra_i32_from_u16(builder, umbra_load_16(builder, ptr));
    umbra_val r32 = umbra_shr_u32(builder, px, umbra_imm_i32(builder, 11));
    umbra_val g32 = umbra_and_i32(builder,
                                  umbra_shr_u32(builder, px, umbra_imm_i32(builder, 5)),
                                  umbra_imm_i32(builder, 0x3f));
    umbra_val b32 = umbra_and_i32(builder, px, umbra_imm_i32(builder, 0x1f));
    umbra_val inv31 = umbra_imm_f32(builder, 1.0f / 31.0f);
    umbra_val inv63 = umbra_imm_f32(builder, 1.0f / 63.0f);
    return (umbra_color){
        umbra_mul_f32(builder, umbra_f32_from_i32(builder, r32), inv31),
        umbra_mul_f32(builder, umbra_f32_from_i32(builder, g32), inv63),
        umbra_mul_f32(builder, umbra_f32_from_i32(builder, b32), inv31),
        umbra_imm_f32(builder, 1.0f),
    };
}

static void umbra_store_565(builder *builder, umbra_ptr ptr, umbra_color c) {
    umbra_val s31 = umbra_imm_f32(builder, 31.0f);
    umbra_val s63 = umbra_imm_f32(builder, 63.0f);
    umbra_val r = umbra_round_i32(builder, umbra_mul_f32(builder, c.r, s31));
    umbra_val g = umbra_round_i32(builder, umbra_mul_f32(builder, c.g, s63));
    umbra_val b = umbra_round_i32(builder, umbra_mul_f32(builder, c.b, s31));
    umbra_val px = umbra_pack(builder, umbra_pack(builder, b, g, 5), r, 11);
    umbra_store_16(builder, ptr, umbra_i16_from_i32(builder, px));
}

static umbra_color umbra_load_1010102(builder *builder, umbra_ptr ptr) {
    umbra_val px = umbra_load_32(builder, ptr);
    umbra_val m10 = umbra_imm_i32(builder, 0x3ff);
    umbra_val r32 = umbra_and_i32(builder, px, m10);
    umbra_val s10 = umbra_imm_i32(builder, 10);
    umbra_val g32 = umbra_and_i32(builder, umbra_shr_u32(builder, px, s10), m10);
    umbra_val b32 =
        umbra_and_i32(builder, umbra_shr_u32(builder, px, umbra_imm_i32(builder, 20)), m10);
    umbra_val a32 = umbra_shr_u32(builder, px, umbra_imm_i32(builder, 30));
    umbra_val inv1023 = umbra_imm_f32(builder, 1.0f / 1023.0f);
    umbra_val inv3 = umbra_imm_f32(builder, 1.0f / 3.0f);
    return (umbra_color){
        umbra_mul_f32(builder, umbra_f32_from_i32(builder, r32), inv1023),
        umbra_mul_f32(builder, umbra_f32_from_i32(builder, g32), inv1023),
        umbra_mul_f32(builder, umbra_f32_from_i32(builder, b32), inv1023),
        umbra_mul_f32(builder, umbra_f32_from_i32(builder, a32), inv3),
    };
}

static void umbra_store_1010102(builder *builder, umbra_ptr ptr, umbra_color c) {
    umbra_val zero = umbra_imm_f32(builder, 0.0f);
    umbra_val one = umbra_imm_f32(builder, 1.0f);
    umbra_val s1023 = umbra_imm_f32(builder, 1023.0f);
    umbra_val s3 = umbra_imm_f32(builder, 3.0f);
    c.r = umbra_min_f32(builder, umbra_max_f32(builder, c.r, zero), one);
    c.g = umbra_min_f32(builder, umbra_max_f32(builder, c.g, zero), one);
    c.b = umbra_min_f32(builder, umbra_max_f32(builder, c.b, zero), one);
    c.a = umbra_min_f32(builder, umbra_max_f32(builder, c.a, zero), one);
    umbra_val r = umbra_round_i32(builder, umbra_mul_f32(builder, c.r, s1023));
    umbra_val g = umbra_round_i32(builder, umbra_mul_f32(builder, c.g, s1023));
    umbra_val b = umbra_round_i32(builder, umbra_mul_f32(builder, c.b, s1023));
    umbra_val a = umbra_round_i32(builder, umbra_mul_f32(builder, c.a, s3));
    umbra_val px = umbra_pack(builder,
                              umbra_pack(builder, umbra_pack(builder, r, g, 10), b, 20),
                              a, 30);
    umbra_store_32(builder, ptr, px);
}

static umbra_color umbra_load_fp16(builder *builder, umbra_ptr ptr) {
    umbra_val lo, hi;
    umbra_load_64(builder, ptr, &lo, &hi);
    umbra_val s16 = umbra_imm_i32(builder, 16);
    return (umbra_color){
        umbra_f32_from_f16(builder, umbra_i16_from_i32(builder, lo)),
        umbra_f32_from_f16(builder, umbra_i16_from_i32(builder, umbra_shr_u32(builder, lo, s16))),
        umbra_f32_from_f16(builder, umbra_i16_from_i32(builder, hi)),
        umbra_f32_from_f16(builder, umbra_i16_from_i32(builder, umbra_shr_u32(builder, hi, s16))),
    };
}

static void umbra_store_fp16(builder *builder, umbra_ptr ptr, umbra_color c) {
    umbra_val lo = umbra_pack(builder, umbra_i32_from_u16(builder, umbra_f16_from_f32(builder, c.r)),
                              umbra_i32_from_u16(builder, umbra_f16_from_f32(builder, c.g)), 16);
    umbra_val hi = umbra_pack(builder, umbra_i32_from_u16(builder, umbra_f16_from_f32(builder, c.b)),
                              umbra_i32_from_u16(builder, umbra_f16_from_f32(builder, c.a)), 16);
    umbra_store_64(builder, ptr, lo, hi);
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
    umbra_color c = umbra_load_8888(builder, ptr);
    umbra_val   a = c.a;
    c = umbra_transfer_invert(builder, c, &umbra_transfer_srgb);
    c.a = a;
    return c;
}
static void store_srgb_8888(builder *builder, umbra_ptr ptr, umbra_color c) {
    umbra_val a = c.a;
    c = umbra_transfer_apply(builder, c, &umbra_transfer_srgb);
    c.a = a;
    umbra_store_8888(builder, ptr, c);
}
umbra_format const umbra_format_srgb_8888 = {4, load_srgb_8888, store_srgb_8888};

void umbra_gradient_lut_even(float *out, int lut_n, int n_stops, float const colors[][4]) {
    for (int i = 0; i < lut_n; i++) {
        float t = (float)i / (float)(lut_n - 1);
        float seg = t * (float)(n_stops - 1);
        int   idx = (int)seg;
        if (idx >= n_stops - 1) { idx = n_stops - 2; }
        float f = seg - (float)idx;
        for (int ch = 0; ch < 4; ch++) {
            out[i * 4 + ch] = colors[idx][ch] * (1 - f) + colors[idx + 1][ch] * f;
        }
    }
}

void umbra_gradient_lut(float *out, int lut_n, int n_stops, float const positions[],
                        float const colors[][4]) {
    for (int i = 0; i < lut_n; i++) {
        float t = (float)i / (float)(lut_n - 1);
        int   seg = 0;
        for (int j = 1; j < n_stops; j++) {
            if (t >= positions[j]) { seg = j; }
        }
        if (seg >= n_stops - 1) { seg = n_stops - 2; }
        float span = positions[seg + 1] - positions[seg];
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
    .a = 1.0f / 1.055f,
    .b = 0.055f / 1.055f,
    .c = 1.0f / 12.92f,
    .d = 0.04045f,
    .e = 0,
    .f = 0,
    .g = 2.4f,
};


static float tf_invert_scalar(umbra_transfer const *tf, float x) {
    if (x >= tf->d) { return powf(tf->a * x + tf->b, tf->g) + tf->e; }
    return tf->c * x + tf->f;
}

static float tf_apply_scalar(umbra_transfer const *tf, float y) {
    float lin_thresh = tf->c * tf->d + tf->f;
    if (y >= lin_thresh) {
        if (tf->a <= 0) { return 0; }
        return (powf(y - tf->e, 1.0f / tf->g) - tf->b) / tf->a;
    }
    if (tf->c <= 0) { return 0; }
    return (y - tf->f) / tf->c;
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
        double x = (double)i / (double)(N - 1);
        double y = pow(x, gamma);
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
            if (fabs(ata[r][k]) > fabs(ata[mx][k])) { mx = r; }
        }
        for (int c = 0; c < n; c++) {
            double t = ata[k][c];
            ata[k][c] = ata[mx][c];
            ata[mx][c] = t;
        }
        {
            double t = atb[k];
            atb[k] = atb[mx];
            atb[mx] = t;
        }
        for (int r = k + 1; r < n; r++) {
            double f = ata[r][k] / ata[k][k];
            for (int c = k; c < n; c++) { ata[r][c] -= f * ata[k][c]; }
            atb[r] -= f * atb[k];
        }
    }
    for (int k = n - 1; k >= 0; k--) {
        double s = atb[k];
        for (int c = k + 1; c < n; c++) { s -= ata[k][c] * coeffs[c]; }
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

umbra_color umbra_transfer_invert(builder *builder, umbra_color c,
                                  umbra_transfer const *tf) {
    double poly[POLY_N];
    int    deg = 7;
    fit_poly((double)tf->g, deg, poly);

    for (int ch = 0; ch < 3; ch++) {
        umbra_val *h = ch == 0 ? &c.r : ch == 1 ? &c.g : &c.b;
        umbra_val  x = clamp01(builder, *h);

        umbra_val lin =
            umbra_add_f32(builder, umbra_mul_f32(builder, x, umbra_imm_f32(builder, tf->c)),
                          umbra_imm_f32(builder, tf->f));

        umbra_val t =
            umbra_add_f32(builder, umbra_mul_f32(builder, x, umbra_imm_f32(builder, tf->a)),
                          umbra_imm_f32(builder, tf->b));
        t = umbra_max_f32(builder, t, umbra_imm_f32(builder, 0.0f));
        umbra_val cur = umbra_add_f32(builder, eval_poly_ir(builder, t, deg, poly),
                                      umbra_imm_f32(builder, tf->e));

        umbra_val mask = umbra_ge_f32(builder, x, umbra_imm_f32(builder, tf->d));
        *h = umbra_sel_i32(builder, mask, cur, lin);
    }
    return c;
}

umbra_color umbra_transfer_apply(builder *builder, umbra_color c, umbra_transfer const *tf) {
    double poly[POLY_N];
    int    deg = 10;
    fit_poly(2.0 / (double)tf->g, deg, poly);

    float lin_thresh = tf->c * tf->d + tf->f;
    float inv_c = tf->c > 0 ? 1.0f / tf->c : 0;
    float inv_a = tf->a > 0 ? 1.0f / tf->a : 0;

    for (int ch = 0; ch < 3; ch++) {
        umbra_val *h = ch == 0 ? &c.r : ch == 1 ? &c.g : &c.b;
        umbra_val  y = clamp01(builder, *h);

        umbra_val lin =
            umbra_mul_f32(builder, umbra_sub_f32(builder, y, umbra_imm_f32(builder, tf->f)),
                          umbra_imm_f32(builder, inv_c));

        umbra_val t = umbra_sub_f32(builder, y, umbra_imm_f32(builder, tf->e));
        t = umbra_max_f32(builder, t, umbra_imm_f32(builder, 0.0f));
        umbra_val s = umbra_sqrt_f32(builder, t);
        umbra_val cur =
            umbra_mul_f32(builder,
                          umbra_sub_f32(builder, eval_poly_ir(builder, s, deg, poly),
                                        umbra_imm_f32(builder, tf->b)),
                          umbra_imm_f32(builder, inv_a));

        umbra_val mask = umbra_ge_f32(builder, y, umbra_imm_f32(builder, lin_thresh));
        *h = umbra_sel_i32(builder, mask, cur, lin);
    }
    return c;
}
