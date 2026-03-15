#include "umbra_draw.h"
#include <math.h>
#include <stdint.h>

typedef struct umbra_basic_block BB;

#define imm(bb,v) umbra_imm_i32(bb, (uint32_t)(v))

static uint32_t f2b(float f) { union { float f; uint32_t u; } v = {.f=f}; return v.u; }

struct umbra_basic_block* umbra_draw_build(umbra_shader_fn   shader,
                                           umbra_coverage_fn coverage,
                                           umbra_blend_fn    blend,
                                           umbra_load_fn     load,
                                           umbra_store_fn    store,
                                           umbra_draw_layout *layout) {
    BB *bb = umbra_basic_block();
    umbra_i32 ix = umbra_lane(bb);

    int x0_ix = umbra_reserve_i32(bb, 1);
    int y_ix  = umbra_reserve_i32(bb, 1);

    umbra_i32 x0 = umbra_load_i32(bb, (umbra_ptr){1}, imm(bb, x0_ix));
    umbra_i32 y  = umbra_load_i32(bb, (umbra_ptr){1}, imm(bb, y_ix));

    umbra_f32 xf = umbra_f32_from_i32(bb, umbra_add_i32(bb, x0, ix));
    umbra_f32 yf = umbra_f32_from_i32(bb, y);

    umbra_color src = {
        umbra_imm_half(bb, 0),
        umbra_imm_half(bb, 0),
        umbra_imm_half(bb, 0),
        umbra_imm_half(bb, 0),
    };
    if (shader) {
        src = shader(bb, xf, yf);
    }

    umbra_half cov = {0};
    if (coverage) {
        cov = coverage(bb, xf, yf);
    }

    umbra_color dst = {
        umbra_imm_half(bb, 0),
        umbra_imm_half(bb, 0),
        umbra_imm_half(bb, 0),
        umbra_imm_half(bb, 0),
    };
    if (load) {
        dst = load(bb, (umbra_ptr){0}, ix);
    }

    umbra_color out;
    if (blend) {
        out = blend(bb, src, dst);
    } else {
        out = src;
    }

    if (coverage) {
        out.r = umbra_add_half(bb, dst.r, umbra_mul_half(bb, umbra_sub_half(bb, out.r, dst.r), cov));
        out.g = umbra_add_half(bb, dst.g, umbra_mul_half(bb, umbra_sub_half(bb, out.g, dst.g), cov));
        out.b = umbra_add_half(bb, dst.b, umbra_mul_half(bb, umbra_sub_half(bb, out.b, dst.b), cov));
        out.a = umbra_add_half(bb, dst.a, umbra_mul_half(bb, umbra_sub_half(bb, out.a, dst.a), cov));
    }

    if (store) {
        store(bb, (umbra_ptr){0}, ix, out);
    }

    if (layout) {
        layout->x0      = x0_ix * 4;
        layout->y        = y_ix  * 4;
        layout->uni_len  = umbra_uni_len(bb);
    }

    return bb;
}

umbra_color umbra_shader_solid(BB *bb, umbra_f32 x, umbra_f32 y) {
    (void)x; (void)y;
    int hi = umbra_reserve_half(bb, 4);
    umbra_half r = umbra_load_half(bb, (umbra_ptr){1}, imm(bb, hi+0));
    umbra_half g = umbra_load_half(bb, (umbra_ptr){1}, imm(bb, hi+1));
    umbra_half b = umbra_load_half(bb, (umbra_ptr){1}, imm(bb, hi+2));
    umbra_half a = umbra_load_half(bb, (umbra_ptr){1}, imm(bb, hi+3));
    return (umbra_color){r, g, b, a};
}

static umbra_f32 clamp01(BB *bb, umbra_f32 t) {
    return umbra_min_f32(bb, umbra_max_f32(bb, t, umbra_imm_f32(bb, 0)),
                              umbra_imm_f32(bb, 0x3f800000));
}

static umbra_half lerp_h(BB *bb, umbra_half a, umbra_half b, umbra_half t) {
    return umbra_add_half(bb, a, umbra_mul_half(bb, umbra_sub_half(bb, b, a), t));
}

static umbra_f32 linear_t_(BB *bb, int fi, umbra_f32 x, umbra_f32 y) {
    umbra_f32 a = umbra_load_f32(bb, (umbra_ptr){1}, imm(bb, fi+0));
    umbra_f32 b = umbra_load_f32(bb, (umbra_ptr){1}, imm(bb, fi+1));
    umbra_f32 c = umbra_load_f32(bb, (umbra_ptr){1}, imm(bb, fi+2));
    return clamp01(bb, umbra_add_f32(bb, umbra_add_f32(bb, umbra_mul_f32(bb, a, x),
                                                            umbra_mul_f32(bb, b, y)), c));
}

static umbra_f32 radial_t_(BB *bb, int fi, umbra_f32 x, umbra_f32 y) {
    umbra_f32 cx    = umbra_load_f32(bb, (umbra_ptr){1}, imm(bb, fi+0));
    umbra_f32 cy    = umbra_load_f32(bb, (umbra_ptr){1}, imm(bb, fi+1));
    umbra_f32 inv_r = umbra_load_f32(bb, (umbra_ptr){1}, imm(bb, fi+2));
    umbra_f32 dx = umbra_sub_f32(bb, x, cx);
    umbra_f32 dy = umbra_sub_f32(bb, y, cy);
    umbra_f32 d2 = umbra_add_f32(bb, umbra_mul_f32(bb, dx, dx), umbra_mul_f32(bb, dy, dy));
    return clamp01(bb, umbra_mul_f32(bb, umbra_sqrt_f32(bb, d2), inv_r));
}

static umbra_color lerp_2stop_(BB *bb, umbra_f32 t_f32, int hi) {
    umbra_half r0 = umbra_load_half(bb, (umbra_ptr){1}, imm(bb, hi+0));
    umbra_half g0 = umbra_load_half(bb, (umbra_ptr){1}, imm(bb, hi+1));
    umbra_half b0 = umbra_load_half(bb, (umbra_ptr){1}, imm(bb, hi+2));
    umbra_half a0 = umbra_load_half(bb, (umbra_ptr){1}, imm(bb, hi+3));
    umbra_half r1 = umbra_load_half(bb, (umbra_ptr){1}, imm(bb, hi+4));
    umbra_half g1 = umbra_load_half(bb, (umbra_ptr){1}, imm(bb, hi+5));
    umbra_half b1 = umbra_load_half(bb, (umbra_ptr){1}, imm(bb, hi+6));
    umbra_half a1 = umbra_load_half(bb, (umbra_ptr){1}, imm(bb, hi+7));
    umbra_half t  = umbra_half_from_f32(bb, t_f32);
    return (umbra_color){
        lerp_h(bb, r0, r1, t),
        lerp_h(bb, g0, g1, t),
        lerp_h(bb, b0, b1, t),
        lerp_h(bb, a0, a1, t),
    };
}

static umbra_color sample_lut_(BB *bb, umbra_f32 t_f32, int fi, umbra_ptr lut) {
    umbra_f32 N_f   = umbra_load_f32(bb, (umbra_ptr){1}, imm(bb, fi+3));
    umbra_f32 one_f = umbra_imm_f32(bb, 0x3f800000);
    umbra_f32 two_f = umbra_imm_f32(bb, 0x40000000);
    umbra_f32 N_m1  = umbra_sub_f32(bb, N_f, one_f);
    umbra_f32 N_m2  = umbra_sub_f32(bb, N_f, two_f);

    umbra_f32 t_sc  = umbra_mul_f32(bb, t_f32, N_m1);
    umbra_f32 idx_f = umbra_min_f32(bb, umbra_f32_from_i32(bb, umbra_i32_from_f32(bb, t_sc)), N_m2);
    umbra_f32 frac  = umbra_sub_f32(bb, t_sc, idx_f);

    umbra_i32 idx  = umbra_i32_from_f32(bb, idx_f);
    umbra_i32 base = umbra_shl_i32(bb, idx, imm(bb, 2));
    umbra_i32 nxt  = umbra_add_i32(bb, base, imm(bb, 4));

    umbra_half r0 = umbra_load_half(bb, lut, base);
    umbra_half g0 = umbra_load_half(bb, lut, umbra_add_i32(bb, base, imm(bb,1)));
    umbra_half b0 = umbra_load_half(bb, lut, umbra_add_i32(bb, base, imm(bb,2)));
    umbra_half a0 = umbra_load_half(bb, lut, umbra_add_i32(bb, base, imm(bb,3)));
    umbra_half r1 = umbra_load_half(bb, lut, nxt);
    umbra_half g1 = umbra_load_half(bb, lut, umbra_add_i32(bb, nxt,  imm(bb,1)));
    umbra_half b1 = umbra_load_half(bb, lut, umbra_add_i32(bb, nxt,  imm(bb,2)));
    umbra_half a1 = umbra_load_half(bb, lut, umbra_add_i32(bb, nxt,  imm(bb,3)));

    umbra_half ft = umbra_half_from_f32(bb, frac);
    return (umbra_color){
        lerp_h(bb, r0, r1, ft),
        lerp_h(bb, g0, g1, ft),
        lerp_h(bb, b0, b1, ft),
        lerp_h(bb, a0, a1, ft),
    };
}

umbra_color umbra_shader_linear_2(BB *bb, umbra_f32 x, umbra_f32 y) {
    int fi = umbra_reserve_f32(bb, 3);
    int hi = umbra_reserve_half(bb, 8);
    return lerp_2stop_(bb, linear_t_(bb, fi, x, y), hi);
}
umbra_color umbra_shader_radial_2(BB *bb, umbra_f32 x, umbra_f32 y) {
    int fi = umbra_reserve_f32(bb, 3);
    int hi = umbra_reserve_half(bb, 8);
    return lerp_2stop_(bb, radial_t_(bb, fi, x, y), hi);
}
umbra_color umbra_shader_linear_grad(BB *bb, umbra_f32 x, umbra_f32 y) {
    int fi = umbra_reserve_f32(bb, 4);
    int lut_off = umbra_reserve_ptr(bb);
    umbra_ptr lut = umbra_deref_ptr(bb, (umbra_ptr){1}, lut_off);
    return sample_lut_(bb, linear_t_(bb, fi, x, y), fi, lut);
}
umbra_color umbra_shader_radial_grad(BB *bb, umbra_f32 x, umbra_f32 y) {
    int fi = umbra_reserve_f32(bb, 4);
    int lut_off = umbra_reserve_ptr(bb);
    umbra_ptr lut = umbra_deref_ptr(bb, (umbra_ptr){1}, lut_off);
    return sample_lut_(bb, radial_t_(bb, fi, x, y), fi, lut);
}

umbra_color umbra_supersample(BB *bb, umbra_f32 x, umbra_f32 y,
                              umbra_shader_fn inner, int n) {
    static float const jitter[][2] = {
        {-0.375f, -0.125f}, { 0.125f, -0.375f},
        { 0.375f,  0.125f}, {-0.125f,  0.375f},
        {-0.250f,  0.375f}, { 0.250f, -0.250f},
        { 0.375f,  0.250f}, {-0.375f, -0.250f},
    };
    if (n < 1) { n = 1; }
    if (n > 8) { n = 8; }

    int saved = umbra_uni_len(bb);
    umbra_color sum = inner(bb, x, y);

    for (int s = 1; s < n; s++) {
        umbra_set_uni_len(bb, saved);
        umbra_f32 sx = umbra_add_f32(bb, x, umbra_imm_f32(bb, f2b(jitter[s-1][0])));
        umbra_f32 sy = umbra_add_f32(bb, y, umbra_imm_f32(bb, f2b(jitter[s-1][1])));
        umbra_color c = inner(bb, sx, sy);
        sum.r = umbra_add_half(bb, sum.r, c.r);
        sum.g = umbra_add_half(bb, sum.g, c.g);
        sum.b = umbra_add_half(bb, sum.b, c.b);
        sum.a = umbra_add_half(bb, sum.a, c.a);
    }

    uint16_t inv_bits = n == 1 ? 0x3c00
                      : n == 2 ? 0x3800
                      : n == 3 ? 0x3555
                      : n == 4 ? 0x3400
                      : n == 5 ? 0x3266
                      : n == 6 ? 0x3155
                      : n == 7 ? 0x3092
                      :          0x3000;
    umbra_half inv = umbra_imm_half(bb, inv_bits);
    return (umbra_color){
        umbra_mul_half(bb, sum.r, inv),
        umbra_mul_half(bb, sum.g, inv),
        umbra_mul_half(bb, sum.b, inv),
        umbra_mul_half(bb, sum.a, inv),
    };
}

umbra_color umbra_blend_src(BB *bb, umbra_color src, umbra_color dst) {
    (void)bb; (void)dst;
    return src;
}

umbra_color umbra_blend_srcover(BB *bb, umbra_color src, umbra_color dst) {
    umbra_half one   = umbra_imm_half(bb, 0x3c00);
    umbra_half inv_a = umbra_sub_half(bb, one, src.a);
    return (umbra_color){
        umbra_add_half(bb, src.r, umbra_mul_half(bb, dst.r, inv_a)),
        umbra_add_half(bb, src.g, umbra_mul_half(bb, dst.g, inv_a)),
        umbra_add_half(bb, src.b, umbra_mul_half(bb, dst.b, inv_a)),
        umbra_add_half(bb, src.a, umbra_mul_half(bb, dst.a, inv_a)),
    };
}

umbra_color umbra_blend_dstover(BB *bb, umbra_color src, umbra_color dst) {
    umbra_half one   = umbra_imm_half(bb, 0x3c00);
    umbra_half inv_a = umbra_sub_half(bb, one, dst.a);
    return (umbra_color){
        umbra_add_half(bb, dst.r, umbra_mul_half(bb, src.r, inv_a)),
        umbra_add_half(bb, dst.g, umbra_mul_half(bb, src.g, inv_a)),
        umbra_add_half(bb, dst.b, umbra_mul_half(bb, src.b, inv_a)),
        umbra_add_half(bb, dst.a, umbra_mul_half(bb, src.a, inv_a)),
    };
}

umbra_color umbra_blend_multiply(BB *bb, umbra_color src, umbra_color dst) {
    umbra_half one = umbra_imm_half(bb, 0x3c00);
    umbra_half inv_sa = umbra_sub_half(bb, one, src.a);
    umbra_half inv_da = umbra_sub_half(bb, one, dst.a);
    umbra_half r = umbra_add_half(bb, umbra_mul_half(bb, src.r, dst.r),
                   umbra_add_half(bb, umbra_mul_half(bb, src.r, inv_da),
                                      umbra_mul_half(bb, dst.r, inv_sa)));
    umbra_half g = umbra_add_half(bb, umbra_mul_half(bb, src.g, dst.g),
                   umbra_add_half(bb, umbra_mul_half(bb, src.g, inv_da),
                                      umbra_mul_half(bb, dst.g, inv_sa)));
    umbra_half b = umbra_add_half(bb, umbra_mul_half(bb, src.b, dst.b),
                   umbra_add_half(bb, umbra_mul_half(bb, src.b, inv_da),
                                      umbra_mul_half(bb, dst.b, inv_sa)));
    umbra_half a = umbra_add_half(bb, umbra_mul_half(bb, src.a, dst.a),
                   umbra_add_half(bb, umbra_mul_half(bb, src.a, inv_da),
                                      umbra_mul_half(bb, dst.a, inv_sa)));
    return (umbra_color){r, g, b, a};
}

umbra_half umbra_coverage_rect(BB *bb, umbra_f32 x, umbra_f32 y) {
    int fi = umbra_reserve_f32(bb, 4);
    umbra_f32 l = umbra_load_f32(bb, (umbra_ptr){1}, imm(bb, fi+0));
    umbra_f32 t = umbra_load_f32(bb, (umbra_ptr){1}, imm(bb, fi+1));
    umbra_f32 r = umbra_load_f32(bb, (umbra_ptr){1}, imm(bb, fi+2));
    umbra_f32 b = umbra_load_f32(bb, (umbra_ptr){1}, imm(bb, fi+3));
    umbra_i32 inside = umbra_and_32(bb,
        umbra_and_32(bb, umbra_ge_f32(bb, x, l), umbra_lt_f32(bb, x, r)),
        umbra_and_32(bb, umbra_ge_f32(bb, y, t), umbra_lt_f32(bb, y, b)));
    umbra_f32 one_f  = umbra_imm_f32(bb, 0x3f800000);
    umbra_f32 zero_f = umbra_imm_f32(bb, 0);
    return umbra_half_from_f32(bb, umbra_sel_f32(bb, inside, one_f, zero_f));
}

umbra_half umbra_coverage_bitmap(BB *bb, umbra_f32 x, umbra_f32 y) {
    (void)x; (void)y;
    int bmp_off = umbra_reserve_ptr(bb);
    umbra_ptr bmp = umbra_deref_ptr(bb, (umbra_ptr){1}, bmp_off);
    umbra_i32 ix = umbra_lane(bb);
    umbra_i16 val = umbra_load_i16(bb, bmp, ix);
    umbra_half inv255 = umbra_imm_half(bb, 0x1c04);
    return umbra_mul_half(bb, umbra_half_from_i16(bb, val), inv255);
}

umbra_half umbra_coverage_sdf(BB *bb, umbra_f32 x, umbra_f32 y) {
    (void)x; (void)y;
    int bmp_off = umbra_reserve_ptr(bb);
    umbra_ptr bmp = umbra_deref_ptr(bb, (umbra_ptr){1}, bmp_off);
    umbra_i32 ix = umbra_lane(bb);
    umbra_i16 raw = umbra_load_i16(bb, bmp, ix);
    umbra_half inv255 = umbra_imm_half(bb, 0x1c04);
    umbra_half dist = umbra_mul_half(bb, umbra_half_from_i16(bb, raw), inv255);
    umbra_half lo    = umbra_imm_half(bb, 0x3733);
    umbra_half scale = umbra_imm_half(bb, 0x4900);
    umbra_half shifted = umbra_sub_half(bb, dist, lo);
    umbra_half scaled  = umbra_mul_half(bb, shifted, scale);
    umbra_half zero = umbra_imm_half(bb, 0);
    umbra_half one  = umbra_imm_half(bb, 0x3c00);
    return umbra_min_half(bb, umbra_max_half(bb, scaled, zero), one);
}

umbra_half umbra_coverage_bitmap_matrix(BB *bb, umbra_f32 x, umbra_f32 y) {
    int fi = umbra_reserve_f32(bb, 11);
    int bmp_off = umbra_reserve_ptr(bb);
    umbra_ptr bmp = umbra_deref_ptr(bb, (umbra_ptr){1}, bmp_off);

    umbra_f32 m0 = umbra_load_f32(bb, (umbra_ptr){1}, imm(bb, fi+0));
    umbra_f32 m1 = umbra_load_f32(bb, (umbra_ptr){1}, imm(bb, fi+1));
    umbra_f32 m2 = umbra_load_f32(bb, (umbra_ptr){1}, imm(bb, fi+2));
    umbra_f32 m3 = umbra_load_f32(bb, (umbra_ptr){1}, imm(bb, fi+3));
    umbra_f32 m4 = umbra_load_f32(bb, (umbra_ptr){1}, imm(bb, fi+4));
    umbra_f32 m5 = umbra_load_f32(bb, (umbra_ptr){1}, imm(bb, fi+5));
    umbra_f32 m6 = umbra_load_f32(bb, (umbra_ptr){1}, imm(bb, fi+6));
    umbra_f32 m7 = umbra_load_f32(bb, (umbra_ptr){1}, imm(bb, fi+7));
    umbra_f32 m8 = umbra_load_f32(bb, (umbra_ptr){1}, imm(bb, fi+8));
    umbra_f32 bw = umbra_load_f32(bb, (umbra_ptr){1}, imm(bb, fi+9));
    umbra_f32 bh = umbra_load_f32(bb, (umbra_ptr){1}, imm(bb, fi+10));

    umbra_f32 w = umbra_add_f32(bb, umbra_add_f32(bb, umbra_mul_f32(bb, m6, x),
                                                       umbra_mul_f32(bb, m7, y)), m8);
    umbra_f32 xp = umbra_div_f32(bb,
        umbra_add_f32(bb, umbra_add_f32(bb, umbra_mul_f32(bb, m0, x),
                                             umbra_mul_f32(bb, m1, y)), m2), w);
    umbra_f32 yp = umbra_div_f32(bb,
        umbra_add_f32(bb, umbra_add_f32(bb, umbra_mul_f32(bb, m3, x),
                                             umbra_mul_f32(bb, m4, y)), m5), w);

    umbra_f32 zero_f = umbra_imm_f32(bb, 0);
    umbra_i32 in = umbra_and_32(bb,
        umbra_and_32(bb, umbra_ge_f32(bb, xp, zero_f), umbra_lt_f32(bb, xp, bw)),
        umbra_and_32(bb, umbra_ge_f32(bb, yp, zero_f), umbra_lt_f32(bb, yp, bh)));

    umbra_f32 one_f = umbra_imm_f32(bb, 0x3f800000);
    umbra_f32 xc = umbra_min_f32(bb, umbra_max_f32(bb, xp, zero_f),
                                      umbra_sub_f32(bb, bw, one_f));
    umbra_f32 yc = umbra_min_f32(bb, umbra_max_f32(bb, yp, zero_f),
                                      umbra_sub_f32(bb, bh, one_f));
    umbra_i32 xi = umbra_i32_from_f32(bb, xc);
    umbra_i32 yi = umbra_i32_from_f32(bb, yc);
    umbra_i32 bwi = umbra_i32_from_f32(bb, bw);
    umbra_i32 idx = umbra_add_i32(bb, umbra_mul_i32(bb, yi, bwi), xi);

    umbra_i16 val = umbra_load_i16(bb, bmp, idx);
    umbra_half inv255 = umbra_imm_half(bb, 0x1c04);
    umbra_half cov = umbra_mul_half(bb, umbra_half_from_i16(bb, val), inv255);

    umbra_f32 cov_f32 = umbra_f32_from_half(bb, cov);
    umbra_f32 masked  = umbra_sel_f32(bb, in, cov_f32, umbra_imm_f32(bb, 0));
    return umbra_half_from_f32(bb, masked);
}

umbra_color umbra_load_8888(BB *bb, umbra_ptr ptr, umbra_i32 ix) {
    umbra_i16 ch[4];
    umbra_load_8x4(bb, ptr, ix, ch);
    umbra_half inv255 = umbra_imm_half(bb, 0x1c04);
    return (umbra_color){
        umbra_mul_half(bb, umbra_half_from_i16(bb, ch[0]), inv255),
        umbra_mul_half(bb, umbra_half_from_i16(bb, ch[1]), inv255),
        umbra_mul_half(bb, umbra_half_from_i16(bb, ch[2]), inv255),
        umbra_mul_half(bb, umbra_half_from_i16(bb, ch[3]), inv255),
    };
}

void umbra_store_8888(BB *bb, umbra_ptr ptr, umbra_i32 ix, umbra_color c) {
    umbra_half scale = umbra_imm_half(bb, 0x5bf8);
    umbra_half half_ = umbra_imm_half(bb, 0x3800);
    umbra_i16 ch[4] = {
        umbra_i16_from_half(bb, umbra_add_half(bb, umbra_mul_half(bb, c.r, scale), half_)),
        umbra_i16_from_half(bb, umbra_add_half(bb, umbra_mul_half(bb, c.g, scale), half_)),
        umbra_i16_from_half(bb, umbra_add_half(bb, umbra_mul_half(bb, c.b, scale), half_)),
        umbra_i16_from_half(bb, umbra_add_half(bb, umbra_mul_half(bb, c.a, scale), half_)),
    };
    umbra_store_8x4(bb, ptr, ix, ch);
}

umbra_color umbra_load_565(BB *bb, umbra_ptr ptr, umbra_i32 ix) {
    umbra_i16 px = umbra_load_i16(bb, ptr, ix);
    umbra_i16 r16 = umbra_shr_u16(bb, px, umbra_imm_i16(bb, 11));
    umbra_i16 g16 = umbra_and_16(bb, umbra_shr_u16(bb, px, umbra_imm_i16(bb, 5)),
                                      umbra_imm_i16(bb, 0x3f));
    umbra_i16 b16 = umbra_and_16(bb, px, umbra_imm_i16(bb, 0x1f));
    umbra_half inv31 = umbra_imm_half(bb, 0x2821);
    umbra_half inv63 = umbra_imm_half(bb, 0x2410);
    return (umbra_color){
        umbra_mul_half(bb, umbra_half_from_i16(bb, r16), inv31),
        umbra_mul_half(bb, umbra_half_from_i16(bb, g16), inv63),
        umbra_mul_half(bb, umbra_half_from_i16(bb, b16), inv31),
        umbra_imm_half(bb, 0x3c00),
    };
}

void umbra_store_565(BB *bb, umbra_ptr ptr, umbra_i32 ix, umbra_color c) {
    umbra_half s31   = umbra_imm_half(bb, 0x4fc0);
    umbra_half s63   = umbra_imm_half(bb, 0x53e0);
    umbra_half half_ = umbra_imm_half(bb, 0x3800);
    umbra_i16 r = umbra_i16_from_half(bb, umbra_add_half(bb, umbra_mul_half(bb, c.r, s31), half_));
    umbra_i16 g = umbra_i16_from_half(bb, umbra_add_half(bb, umbra_mul_half(bb, c.g, s63), half_));
    umbra_i16 b = umbra_i16_from_half(bb, umbra_add_half(bb, umbra_mul_half(bb, c.b, s31), half_));
    umbra_i16 px = umbra_or_16(bb, umbra_or_16(bb, umbra_shl_i16(bb, r, umbra_imm_i16(bb, 11)),
                                                    umbra_shl_i16(bb, g, umbra_imm_i16(bb, 5))), b);
    umbra_store_i16(bb, ptr, ix, px);
}

umbra_color umbra_load_1010102(BB *bb, umbra_ptr ptr, umbra_i32 ix) {
    umbra_i32 px    = umbra_load_i32(bb, ptr, ix);
    umbra_i32 m10   = imm(bb, 0x3ff);
    umbra_i32 r32   = umbra_and_32(bb, px, m10);
    umbra_i32 g32   = umbra_and_32(bb, umbra_shr_u32(bb, px, imm(bb, 10)), m10);
    umbra_i32 b32   = umbra_and_32(bb, umbra_shr_u32(bb, px, imm(bb, 20)), m10);
    umbra_i32 a32   = umbra_shr_u32(bb, px, imm(bb, 30));
    umbra_half inv1023 = umbra_imm_half(bb, 0x1401);
    umbra_half inv3    = umbra_imm_half(bb, 0x3555);
    return (umbra_color){
        umbra_mul_half(bb, umbra_half_from_i32(bb, r32), inv1023),
        umbra_mul_half(bb, umbra_half_from_i32(bb, g32), inv1023),
        umbra_mul_half(bb, umbra_half_from_i32(bb, b32), inv1023),
        umbra_mul_half(bb, umbra_half_from_i32(bb, a32), inv3),
    };
}

void umbra_store_1010102(BB *bb, umbra_ptr ptr, umbra_i32 ix, umbra_color c) {
    umbra_half s1023 = umbra_imm_half(bb, 0x63fe);
    umbra_half s3    = umbra_imm_half(bb, 0x4200);
    umbra_half half_ = umbra_imm_half(bb, 0x3800);
    umbra_i32 r = umbra_i32_from_half(bb, umbra_add_half(bb, umbra_mul_half(bb, c.r, s1023), half_));
    umbra_i32 g = umbra_i32_from_half(bb, umbra_add_half(bb, umbra_mul_half(bb, c.g, s1023), half_));
    umbra_i32 b = umbra_i32_from_half(bb, umbra_add_half(bb, umbra_mul_half(bb, c.b, s1023), half_));
    umbra_i32 a = umbra_i32_from_half(bb, umbra_add_half(bb, umbra_mul_half(bb, c.a, s3),    half_));
    umbra_i32 px = umbra_or_32(bb,
        umbra_or_32(bb, r, umbra_shl_i32(bb, g, imm(bb, 10))),
        umbra_or_32(bb, umbra_shl_i32(bb, b, imm(bb, 20)),
                         umbra_shl_i32(bb, a, imm(bb, 30))));
    umbra_store_i32(bb, ptr, ix, px);
}

umbra_color umbra_load_fp16(BB *bb, umbra_ptr ptr, umbra_i32 ix) {
    umbra_i32 ix4 = umbra_shl_i32(bb, ix, imm(bb, 2));
    return (umbra_color){
        umbra_load_half(bb, ptr, umbra_add_i32(bb, ix4, imm(bb, 0))),
        umbra_load_half(bb, ptr, umbra_add_i32(bb, ix4, imm(bb, 1))),
        umbra_load_half(bb, ptr, umbra_add_i32(bb, ix4, imm(bb, 2))),
        umbra_load_half(bb, ptr, umbra_add_i32(bb, ix4, imm(bb, 3))),
    };
}

void umbra_store_fp16(BB *bb, umbra_ptr ptr, umbra_i32 ix, umbra_color c) {
    umbra_i32 ix4 = umbra_shl_i32(bb, ix, imm(bb, 2));
    umbra_store_half(bb, ptr, umbra_add_i32(bb, ix4, imm(bb, 0)), c.r);
    umbra_store_half(bb, ptr, umbra_add_i32(bb, ix4, imm(bb, 1)), c.g);
    umbra_store_half(bb, ptr, umbra_add_i32(bb, ix4, imm(bb, 2)), c.b);
    umbra_store_half(bb, ptr, umbra_add_i32(bb, ix4, imm(bb, 3)), c.a);
}

umbra_color umbra_load_fp16_planar(BB *bb, umbra_ptr ptr, umbra_i32 ix) {
    int si = umbra_reserve_i32(bb, 1);
    umbra_i32 stride = umbra_load_i32(bb, (umbra_ptr){1}, imm(bb, si));
    umbra_i32 s2 = umbra_mul_i32(bb, stride, imm(bb, 2));
    umbra_i32 s3 = umbra_mul_i32(bb, stride, imm(bb, 3));
    return (umbra_color){
        umbra_load_half(bb, ptr, ix),
        umbra_load_half(bb, ptr, umbra_add_i32(bb, ix, stride)),
        umbra_load_half(bb, ptr, umbra_add_i32(bb, ix, s2)),
        umbra_load_half(bb, ptr, umbra_add_i32(bb, ix, s3)),
    };
}

void umbra_store_fp16_planar(BB *bb, umbra_ptr ptr, umbra_i32 ix, umbra_color c) {
    int si = umbra_reserve_i32(bb, 1);
    umbra_i32 stride = umbra_load_i32(bb, (umbra_ptr){1}, imm(bb, si));
    umbra_i32 s2 = umbra_mul_i32(bb, stride, imm(bb, 2));
    umbra_i32 s3 = umbra_mul_i32(bb, stride, imm(bb, 3));
    umbra_store_half(bb, ptr, ix, c.r);
    umbra_store_half(bb, ptr, umbra_add_i32(bb, ix, stride), c.g);
    umbra_store_half(bb, ptr, umbra_add_i32(bb, ix, s2), c.b);
    umbra_store_half(bb, ptr, umbra_add_i32(bb, ix, s3), c.a);
}

void umbra_gradient_lut_even(__fp16 *out, int lut_n,
                             int n_stops, __fp16 const colors[][4]) {
    for (int i = 0; i < lut_n; i++) {
        float t   = (float)i / (float)(lut_n - 1);
        float seg = t * (float)(n_stops - 1);
        int   idx = (int)seg;
        if (idx >= n_stops - 1) { idx = n_stops - 2; }
        float f = seg - (float)idx;
        for (int ch = 0; ch < 4; ch++) {
            out[i*4+ch] = (__fp16)((float)colors[idx][ch]*(1-f) + (float)colors[idx+1][ch]*f);
        }
    }
}

void umbra_gradient_lut(__fp16 *out, int lut_n,
                        int n_stops, float const positions[],
                        __fp16 const colors[][4]) {
    for (int i = 0; i < lut_n; i++) {
        float t = (float)i / (float)(lut_n - 1);
        int seg = 0;
        for (int j = 1; j < n_stops; j++) {
            if (t >= positions[j]) { seg = j; }
        }
        if (seg >= n_stops - 1) { seg = n_stops - 2; }
        float span = positions[seg+1] - positions[seg];
        float f = 0;
        if (span > 0) {
            f = (t - positions[seg]) / span;
            if (f < 0) { f = 0; }
            if (f > 1) { f = 1; }
        }
        for (int ch = 0; ch < 4; ch++) {
            out[i*4+ch] = (__fp16)((float)colors[seg][ch]*(1-f) + (float)colors[seg+1][ch]*f);
        }
    }
}

umbra_transfer const umbra_transfer_srgb = {
    .a = 1.0f/1.055f,
    .b = 0.055f/1.055f,
    .c = 1.0f/12.92f,
    .d = 0.04045f,
    .e = 0,
    .f = 0,
    .g = 2.4f,
};
umbra_transfer const umbra_transfer_gamma22 = {
    .a = 1,
    .b = 0,
    .c = 0,
    .d = 0,
    .e = 0,
    .f = 0,
    .g = 2.2f,
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
    for (int i = 0; i < 256; i++) {
        out[i] = tf_invert_scalar(tf, (float)i / 255.0f);
    }
}

void umbra_transfer_lut_apply(float out[256], umbra_transfer const *tf) {
    for (int i = 0; i < 256; i++) {
        out[i] = tf_apply_scalar(tf, (float)i / 255.0f);
    }
}

#define POLY_N 10

static void fit_poly(double gamma, int n, double coeffs[]) {
    enum { N = 512 };
    double ata[POLY_N][POLY_N] = {{0}};
    double atb[POLY_N]         = {0};
    for (int i = 0; i < N; i++) {
        double x = (double)i / (double)(N - 1);
        double y = pow(x, gamma);
        double xp = 1;
        for (int r = 0; r < n; r++) {
            double xq = 1;
            for (int c = 0; c < n; c++) { ata[r][c] += xp * xq; xq *= x; }
            atb[r] += xp * y;
            xp *= x;
        }
    }
    for (int k = 0; k < n; k++) {
        int mx = k;
        for (int r = k+1; r < n; r++) {
            if (fabs(ata[r][k]) > fabs(ata[mx][k])) { mx = r; }
        }
        for (int c = 0; c < n; c++) { double t = ata[k][c]; ata[k][c] = ata[mx][c]; ata[mx][c] = t; }
        { double t = atb[k]; atb[k] = atb[mx]; atb[mx] = t; }
        for (int r = k+1; r < n; r++) {
            double f = ata[r][k] / ata[k][k];
            for (int c = k; c < n; c++) { ata[r][c] -= f * ata[k][c]; }
            atb[r] -= f * atb[k];
        }
    }
    for (int k = n-1; k >= 0; k--) {
        double s = atb[k];
        for (int c = k+1; c < n; c++) { s -= ata[k][c] * coeffs[c]; }
        coeffs[k] = s / ata[k][k];
    }
}

static umbra_f32 eval_poly_ir(BB *bb, umbra_f32 x, int n, double const coeffs[]) {
    umbra_f32 r = umbra_imm_f32(bb, f2b((float)coeffs[n-1]));
    for (int i = n - 2; i >= 0; i--) {
        r = umbra_add_f32(bb, umbra_mul_f32(bb, r, x), umbra_imm_f32(bb, f2b((float)coeffs[i])));
    }
    return r;
}

umbra_color umbra_transfer_invert(BB *bb, umbra_color c, umbra_transfer const *tf) {
    double poly[POLY_N];
    int deg = 7;
    fit_poly((double)tf->g, deg, poly);

    for (int ch = 0; ch < 3; ch++) {
        umbra_half *h = ch == 0 ? &c.r : ch == 1 ? &c.g : &c.b;
        umbra_f32 x = clamp01(bb, umbra_f32_from_half(bb, *h));

        umbra_f32 lin = umbra_add_f32(bb,
            umbra_mul_f32(bb, x, umbra_imm_f32(bb, f2b(tf->c))),
            umbra_imm_f32(bb, f2b(tf->f)));

        umbra_f32 t = umbra_add_f32(bb,
            umbra_mul_f32(bb, x, umbra_imm_f32(bb, f2b(tf->a))),
            umbra_imm_f32(bb, f2b(tf->b)));
        t = umbra_max_f32(bb, t, umbra_imm_f32(bb, 0));
        umbra_f32 cur = umbra_add_f32(bb, eval_poly_ir(bb, t, deg, poly),
                                           umbra_imm_f32(bb, f2b(tf->e)));

        umbra_i32 mask = umbra_ge_f32(bb, x, umbra_imm_f32(bb, f2b(tf->d)));
        *h = umbra_half_from_f32(bb, umbra_sel_f32(bb, mask, cur, lin));
    }
    return c;
}

umbra_color umbra_transfer_apply(BB *bb, umbra_color c, umbra_transfer const *tf) {
    double poly[POLY_N];
    int deg = 10;
    fit_poly(2.0 / (double)tf->g, deg, poly);

    float lin_thresh = tf->c * tf->d + tf->f;
    float inv_c = tf->c > 0 ? 1.0f / tf->c : 0;
    float inv_a = tf->a > 0 ? 1.0f / tf->a : 0;

    for (int ch = 0; ch < 3; ch++) {
        umbra_half *h = ch == 0 ? &c.r : ch == 1 ? &c.g : &c.b;
        umbra_f32 y = clamp01(bb, umbra_f32_from_half(bb, *h));

        umbra_f32 lin = umbra_mul_f32(bb, umbra_sub_f32(bb, y,
            umbra_imm_f32(bb, f2b(tf->f))), umbra_imm_f32(bb, f2b(inv_c)));

        umbra_f32 t = umbra_sub_f32(bb, y, umbra_imm_f32(bb, f2b(tf->e)));
        t = umbra_max_f32(bb, t, umbra_imm_f32(bb, 0));
        umbra_f32 s = umbra_sqrt_f32(bb, t);
        umbra_f32 cur = umbra_mul_f32(bb,
            umbra_sub_f32(bb, eval_poly_ir(bb, s, deg, poly), umbra_imm_f32(bb, f2b(tf->b))),
            umbra_imm_f32(bb, f2b(inv_a)));

        umbra_i32 mask = umbra_ge_f32(bb, y, umbra_imm_f32(bb, f2b(lin_thresh)));
        *h = umbra_half_from_f32(bb, umbra_sel_f32(bb, mask, cur, lin));
    }
    return c;
}
