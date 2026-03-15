#include "umbra_draw.h"

typedef struct umbra_basic_block BB;

struct umbra_basic_block* umbra_draw_build(umbra_shader_fn   shader,
                                           umbra_coverage_fn coverage,
                                           umbra_blend_fn    blend,
                                           umbra_load_fn     load,
                                           umbra_store_fn    store) {
    BB *bb = umbra_basic_block();
    umbra_i32 ix = umbra_lane(bb);

    umbra_i32 x0 = umbra_load_i32(bb, (umbra_ptr){1}, umbra_imm_i32(bb, 0));
    umbra_i32 y  = umbra_load_i32(bb, (umbra_ptr){2}, umbra_imm_i32(bb, 0));

    umbra_f32 xf = umbra_f32_from_i32(bb, umbra_add_i32(bb, x0, ix));
    umbra_f32 yf = umbra_f32_from_i32(bb, y);

    umbra_half cov = {0};
    if (coverage) {
        cov = coverage(bb, xf, yf);
    }

    umbra_color src = {
        umbra_imm_half(bb, 0),
        umbra_imm_half(bb, 0),
        umbra_imm_half(bb, 0),
        umbra_imm_half(bb, 0),
    };
    if (shader) {
        src = shader(bb, xf, yf);
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

    return bb;
}

umbra_color umbra_shader_solid(BB *bb, umbra_f32 x, umbra_f32 y) {
    (void)x; (void)y;
    umbra_half r = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_i32(bb, 0));
    umbra_half g = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_i32(bb, 1));
    umbra_half b = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_i32(bb, 2));
    umbra_half a = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_i32(bb, 3));
    return (umbra_color){r, g, b, a};
}

static umbra_f32 clamp01(BB *bb, umbra_f32 t) {
    return umbra_min_f32(bb, umbra_max_f32(bb, t, umbra_imm_f32(bb, 0)),
                              umbra_imm_f32(bb, 0x3f800000));
}

static umbra_half lerp_h(BB *bb, umbra_half a, umbra_half b, umbra_half t) {
    return umbra_add_half(bb, a, umbra_mul_half(bb, umbra_sub_half(bb, b, a), t));
}

static umbra_f32 linear_t(BB *bb, umbra_f32 x, umbra_f32 y) {
    umbra_f32 a = umbra_load_f32(bb, (umbra_ptr){5}, umbra_imm_i32(bb, 0));
    umbra_f32 b = umbra_load_f32(bb, (umbra_ptr){5}, umbra_imm_i32(bb, 1));
    umbra_f32 c = umbra_load_f32(bb, (umbra_ptr){5}, umbra_imm_i32(bb, 2));
    return clamp01(bb, umbra_add_f32(bb, umbra_add_f32(bb, umbra_mul_f32(bb, a, x),
                                                            umbra_mul_f32(bb, b, y)), c));
}

static umbra_f32 radial_t(BB *bb, umbra_f32 x, umbra_f32 y) {
    umbra_f32 cx    = umbra_load_f32(bb, (umbra_ptr){5}, umbra_imm_i32(bb, 0));
    umbra_f32 cy    = umbra_load_f32(bb, (umbra_ptr){5}, umbra_imm_i32(bb, 1));
    umbra_f32 inv_r = umbra_load_f32(bb, (umbra_ptr){5}, umbra_imm_i32(bb, 2));
    umbra_f32 dx = umbra_sub_f32(bb, x, cx);
    umbra_f32 dy = umbra_sub_f32(bb, y, cy);
    umbra_f32 d2 = umbra_add_f32(bb, umbra_mul_f32(bb, dx, dx), umbra_mul_f32(bb, dy, dy));
    return clamp01(bb, umbra_mul_f32(bb, umbra_sqrt_f32(bb, d2), inv_r));
}

static umbra_color lerp_2stop(BB *bb, umbra_f32 t_f32) {
    umbra_half r0 = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_i32(bb, 0));
    umbra_half g0 = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_i32(bb, 1));
    umbra_half b0 = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_i32(bb, 2));
    umbra_half a0 = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_i32(bb, 3));
    umbra_half r1 = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_i32(bb, 4));
    umbra_half g1 = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_i32(bb, 5));
    umbra_half b1 = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_i32(bb, 6));
    umbra_half a1 = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_i32(bb, 7));
    umbra_half t  = umbra_half_from_f32(bb, t_f32);
    return (umbra_color){
        lerp_h(bb, r0, r1, t),
        lerp_h(bb, g0, g1, t),
        lerp_h(bb, b0, b1, t),
        lerp_h(bb, a0, a1, t),
    };
}

static umbra_color sample_lut(BB *bb, umbra_f32 t_f32) {
    umbra_f32 N_f   = umbra_load_f32(bb, (umbra_ptr){5}, umbra_imm_i32(bb, 3));
    umbra_f32 one_f = umbra_imm_f32(bb, 0x3f800000);
    umbra_f32 two_f = umbra_imm_f32(bb, 0x40000000);
    umbra_f32 N_m1  = umbra_sub_f32(bb, N_f, one_f);
    umbra_f32 N_m2  = umbra_sub_f32(bb, N_f, two_f);

    umbra_f32 t_sc  = umbra_mul_f32(bb, t_f32, N_m1);
    umbra_f32 idx_f = umbra_min_f32(bb, umbra_f32_from_i32(bb, umbra_i32_from_f32(bb, t_sc)), N_m2);
    umbra_f32 frac  = umbra_sub_f32(bb, t_sc, idx_f);

    umbra_i32 idx  = umbra_i32_from_f32(bb, idx_f);
    umbra_i32 base = umbra_shl_i32(bb, idx, umbra_imm_i32(bb, 2));
    umbra_i32 nxt  = umbra_add_i32(bb, base, umbra_imm_i32(bb, 4));

    umbra_half r0 = umbra_load_half(bb, (umbra_ptr){3}, base);
    umbra_half g0 = umbra_load_half(bb, (umbra_ptr){3}, umbra_add_i32(bb, base, umbra_imm_i32(bb,1)));
    umbra_half b0 = umbra_load_half(bb, (umbra_ptr){3}, umbra_add_i32(bb, base, umbra_imm_i32(bb,2)));
    umbra_half a0 = umbra_load_half(bb, (umbra_ptr){3}, umbra_add_i32(bb, base, umbra_imm_i32(bb,3)));
    umbra_half r1 = umbra_load_half(bb, (umbra_ptr){3}, nxt);
    umbra_half g1 = umbra_load_half(bb, (umbra_ptr){3}, umbra_add_i32(bb, nxt,  umbra_imm_i32(bb,1)));
    umbra_half b1 = umbra_load_half(bb, (umbra_ptr){3}, umbra_add_i32(bb, nxt,  umbra_imm_i32(bb,2)));
    umbra_half a1 = umbra_load_half(bb, (umbra_ptr){3}, umbra_add_i32(bb, nxt,  umbra_imm_i32(bb,3)));

    umbra_half ft = umbra_half_from_f32(bb, frac);
    return (umbra_color){
        lerp_h(bb, r0, r1, ft),
        lerp_h(bb, g0, g1, ft),
        lerp_h(bb, b0, b1, ft),
        lerp_h(bb, a0, a1, ft),
    };
}

umbra_color umbra_shader_linear_2(BB *bb, umbra_f32 x, umbra_f32 y) {
    return lerp_2stop(bb, linear_t(bb, x, y));
}
umbra_color umbra_shader_radial_2(BB *bb, umbra_f32 x, umbra_f32 y) {
    return lerp_2stop(bb, radial_t(bb, x, y));
}
umbra_color umbra_shader_linear_grad(BB *bb, umbra_f32 x, umbra_f32 y) {
    return sample_lut(bb, linear_t(bb, x, y));
}
umbra_color umbra_shader_radial_grad(BB *bb, umbra_f32 x, umbra_f32 y) {
    return sample_lut(bb, radial_t(bb, x, y));
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
    umbra_f32 l = umbra_load_f32(bb, (umbra_ptr){4}, umbra_imm_i32(bb, 0));
    umbra_f32 t = umbra_load_f32(bb, (umbra_ptr){4}, umbra_imm_i32(bb, 1));
    umbra_f32 r = umbra_load_f32(bb, (umbra_ptr){4}, umbra_imm_i32(bb, 2));
    umbra_f32 b = umbra_load_f32(bb, (umbra_ptr){4}, umbra_imm_i32(bb, 3));
    umbra_i32 inside = umbra_and_32(bb,
        umbra_and_32(bb, umbra_ge_f32(bb, x, l), umbra_lt_f32(bb, x, r)),
        umbra_and_32(bb, umbra_ge_f32(bb, y, t), umbra_lt_f32(bb, y, b)));
    umbra_f32 one_f  = umbra_imm_f32(bb, 0x3f800000);
    umbra_f32 zero_f = umbra_imm_f32(bb, 0);
    return umbra_half_from_f32(bb, umbra_sel_f32(bb, inside, one_f, zero_f));
}

umbra_half umbra_coverage_bitmap(BB *bb, umbra_f32 x, umbra_f32 y) {
    (void)x; (void)y;
    umbra_i32 ix = umbra_lane(bb);
    umbra_i16 val = umbra_load_i16(bb, (umbra_ptr){4}, ix);
    umbra_half inv255 = umbra_imm_half(bb, 0x1c04);
    return umbra_mul_half(bb, umbra_half_from_i16(bb, val), inv255);
}

umbra_half umbra_coverage_sdf(BB *bb, umbra_f32 x, umbra_f32 y) {
    (void)x; (void)y;
    umbra_i32 ix = umbra_lane(bb);
    umbra_i16 raw = umbra_load_i16(bb, (umbra_ptr){4}, ix);
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
    umbra_f32 m0 = umbra_load_f32(bb, (umbra_ptr){5}, umbra_imm_i32(bb, 0));
    umbra_f32 m1 = umbra_load_f32(bb, (umbra_ptr){5}, umbra_imm_i32(bb, 1));
    umbra_f32 m2 = umbra_load_f32(bb, (umbra_ptr){5}, umbra_imm_i32(bb, 2));
    umbra_f32 m3 = umbra_load_f32(bb, (umbra_ptr){5}, umbra_imm_i32(bb, 3));
    umbra_f32 m4 = umbra_load_f32(bb, (umbra_ptr){5}, umbra_imm_i32(bb, 4));
    umbra_f32 m5 = umbra_load_f32(bb, (umbra_ptr){5}, umbra_imm_i32(bb, 5));
    umbra_f32 m6 = umbra_load_f32(bb, (umbra_ptr){5}, umbra_imm_i32(bb, 6));
    umbra_f32 m7 = umbra_load_f32(bb, (umbra_ptr){5}, umbra_imm_i32(bb, 7));
    umbra_f32 m8 = umbra_load_f32(bb, (umbra_ptr){5}, umbra_imm_i32(bb, 8));
    umbra_f32 bw = umbra_load_f32(bb, (umbra_ptr){5}, umbra_imm_i32(bb, 9));
    umbra_f32 bh = umbra_load_f32(bb, (umbra_ptr){5}, umbra_imm_i32(bb, 10));

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

    umbra_i16 val = umbra_load_i16(bb, (umbra_ptr){4}, idx);
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
    umbra_i32 m10   = umbra_imm_i32(bb, 0x3ff);
    umbra_i32 r32   = umbra_and_32(bb, px, m10);
    umbra_i32 g32   = umbra_and_32(bb, umbra_shr_u32(bb, px, umbra_imm_i32(bb, 10)), m10);
    umbra_i32 b32   = umbra_and_32(bb, umbra_shr_u32(bb, px, umbra_imm_i32(bb, 20)), m10);
    umbra_i32 a32   = umbra_shr_u32(bb, px, umbra_imm_i32(bb, 30));
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
        umbra_or_32(bb, r, umbra_shl_i32(bb, g, umbra_imm_i32(bb, 10))),
        umbra_or_32(bb, umbra_shl_i32(bb, b, umbra_imm_i32(bb, 20)),
                         umbra_shl_i32(bb, a, umbra_imm_i32(bb, 30))));
    umbra_store_i32(bb, ptr, ix, px);
}

umbra_color umbra_load_fp16(BB *bb, umbra_ptr ptr, umbra_i32 ix) {
    umbra_i32 ix4 = umbra_shl_i32(bb, ix, umbra_imm_i32(bb, 2));
    return (umbra_color){
        umbra_load_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_i32(bb, 0))),
        umbra_load_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_i32(bb, 1))),
        umbra_load_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_i32(bb, 2))),
        umbra_load_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_i32(bb, 3))),
    };
}

void umbra_store_fp16(BB *bb, umbra_ptr ptr, umbra_i32 ix, umbra_color c) {
    umbra_i32 ix4 = umbra_shl_i32(bb, ix, umbra_imm_i32(bb, 2));
    umbra_store_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_i32(bb, 0)), c.r);
    umbra_store_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_i32(bb, 1)), c.g);
    umbra_store_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_i32(bb, 2)), c.b);
    umbra_store_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_i32(bb, 3)), c.a);
}

umbra_color umbra_load_fp16_planar(BB *bb, umbra_ptr ptr, umbra_i32 ix) {
    umbra_i32 stride = umbra_load_i32(bb, (umbra_ptr){6}, umbra_imm_i32(bb, 0));
    umbra_i32 s2 = umbra_mul_i32(bb, stride, umbra_imm_i32(bb, 2));
    umbra_i32 s3 = umbra_mul_i32(bb, stride, umbra_imm_i32(bb, 3));
    return (umbra_color){
        umbra_load_half(bb, ptr, ix),
        umbra_load_half(bb, ptr, umbra_add_i32(bb, ix, stride)),
        umbra_load_half(bb, ptr, umbra_add_i32(bb, ix, s2)),
        umbra_load_half(bb, ptr, umbra_add_i32(bb, ix, s3)),
    };
}

void umbra_store_fp16_planar(BB *bb, umbra_ptr ptr, umbra_i32 ix, umbra_color c) {
    umbra_i32 stride = umbra_load_i32(bb, (umbra_ptr){6}, umbra_imm_i32(bb, 0));
    umbra_i32 s2 = umbra_mul_i32(bb, stride, umbra_imm_i32(bb, 2));
    umbra_i32 s3 = umbra_mul_i32(bb, stride, umbra_imm_i32(bb, 3));
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
