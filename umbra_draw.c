#include "umbra_draw.h"
#include <math.h>
#include <stdint.h>

typedef struct umbra_basic_block BB;


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

    umbra_i32 x0 = umbra_iload(bb, (umbra_ptr){1}, umbra_iimm(bb, x0_ix));
    umbra_i32 y  = umbra_iload(bb, (umbra_ptr){1}, umbra_iimm(bb, y_ix));

    umbra_f32 xf = umbra_itof(bb, umbra_iadd(bb, x0, ix));
    umbra_f32 yf = umbra_itof(bb, y);

    int shader_off = umbra_uni_len(bb);
    umbra_color src = {
        umbra_fimm(bb, 0.0f),
        umbra_fimm(bb, 0.0f),
        umbra_fimm(bb, 0.0f),
        umbra_fimm(bb, 0.0f),
    };
    if (shader) {
        src = shader(bb, xf, yf);
    }

    int coverage_off = umbra_uni_len(bb);
    umbra_f32 cov = {0};
    if (coverage) {
        cov = coverage(bb, xf, yf);
    }

    umbra_color dst = {
        umbra_fimm(bb, 0.0f),
        umbra_fimm(bb, 0.0f),
        umbra_fimm(bb, 0.0f),
        umbra_fimm(bb, 0.0f),
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
        out.r = umbra_fadd(bb, dst.r, umbra_fmul(bb, umbra_fsub(bb, out.r, dst.r), cov));
        out.g = umbra_fadd(bb, dst.g, umbra_fmul(bb, umbra_fsub(bb, out.g, dst.g), cov));
        out.b = umbra_fadd(bb, dst.b, umbra_fmul(bb, umbra_fsub(bb, out.b, dst.b), cov));
        out.a = umbra_fadd(bb, dst.a, umbra_fmul(bb, umbra_fsub(bb, out.a, dst.a), cov));
    }

    if (store) {
        store(bb, (umbra_ptr){0}, ix, out);
    }

    if (layout) {
        layout->x0       = x0_ix * 4;
        layout->y        = y_ix  * 4;
        layout->shader   = shader_off;
        layout->coverage = coverage_off;
        layout->uni_len  = umbra_uni_len(bb);
    }

    return bb;
}

umbra_color umbra_shader_solid(BB *bb, umbra_f32 x, umbra_f32 y) {
    (void)x; (void)y;
    int fi = umbra_reserve_f32(bb, 4);
    umbra_f32 r = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi));
    umbra_f32 g = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+1));
    umbra_f32 b = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+2));
    umbra_f32 a = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+3));
    return (umbra_color){r, g, b, a};
}

static umbra_f32 clamp01(BB *bb, umbra_f32 t) {
    return umbra_fmin(bb, umbra_fmax(bb, t, umbra_fimm(bb, 0.0f)),
                              umbra_fimm(bb, 1.0f));
}

static umbra_f32 lerp_f(BB *bb, umbra_f32 a, umbra_f32 b, umbra_f32 t) {
    return umbra_fadd(bb, a, umbra_fmul(bb, umbra_fsub(bb, b, a), t));
}

static umbra_f32 linear_t_(BB *bb, int fi, umbra_f32 x, umbra_f32 y) {
    umbra_f32 a = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi));
    umbra_f32 b = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+1));
    umbra_f32 c = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+2));
    return clamp01(bb, umbra_fadd(bb, umbra_fadd(bb, umbra_fmul(bb, a, x),
                                                            umbra_fmul(bb, b, y)), c));
}

static umbra_f32 radial_t_(BB *bb, int fi, umbra_f32 x, umbra_f32 y) {
    umbra_f32 cx    = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi));
    umbra_f32 cy    = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+1));
    umbra_f32 inv_r = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+2));
    umbra_f32 dx = umbra_fsub(bb, x, cx);
    umbra_f32 dy = umbra_fsub(bb, y, cy);
    umbra_f32 d2 = umbra_fadd(bb, umbra_fmul(bb, dx, dx), umbra_fmul(bb, dy, dy));
    return clamp01(bb, umbra_fmul(bb, umbra_fsqrt(bb, d2), inv_r));
}

static umbra_color lerp_2stop_(BB *bb, umbra_f32 t, int fi) {
    umbra_f32 r0 = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi));
    umbra_f32 g0 = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+1));
    umbra_f32 b0 = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+2));
    umbra_f32 a0 = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+3));
    umbra_f32 r1 = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+4));
    umbra_f32 g1 = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+5));
    umbra_f32 b1 = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+6));
    umbra_f32 a1 = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+7));
    return (umbra_color){
        lerp_f(bb, r0, r1, t),
        lerp_f(bb, g0, g1, t),
        lerp_f(bb, b0, b1, t),
        lerp_f(bb, a0, a1, t),
    };
}

static umbra_color sample_lut_(BB *bb, umbra_f32 t_f32, int fi, umbra_ptr lut) {
    umbra_f32 N_f   = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+3));
    umbra_f32 one_f = umbra_fimm(bb, 1.0f);
    umbra_f32 two_f = umbra_fimm(bb, 2.0f);
    umbra_f32 N_m1  = umbra_fsub(bb, N_f, one_f);
    umbra_f32 N_m2  = umbra_fsub(bb, N_f, two_f);

    umbra_f32 t_sc  = umbra_fmul(bb, t_f32, N_m1);
    umbra_f32 idx_f = umbra_fmin(bb, umbra_itof(bb, umbra_ftoi(bb, t_sc)), N_m2);
    umbra_f32 frac  = umbra_fsub(bb, t_sc, idx_f);

    umbra_i32 idx  = umbra_ftoi(bb, idx_f);
    umbra_i32 base = umbra_ishl(bb, idx, umbra_iimm(bb, 2));
    umbra_i32 nxt  = umbra_iadd(bb, base, umbra_iimm(bb, 4));

    umbra_f32 r0 = umbra_fload(bb, lut, base);
    umbra_f32 g0 = umbra_fload(bb, lut, umbra_iadd(bb, base, umbra_iimm(bb,1)));
    umbra_f32 b0 = umbra_fload(bb, lut, umbra_iadd(bb, base, umbra_iimm(bb,2)));
    umbra_f32 a0 = umbra_fload(bb, lut, umbra_iadd(bb, base, umbra_iimm(bb,3)));
    umbra_f32 r1 = umbra_fload(bb, lut, nxt);
    umbra_f32 g1 = umbra_fload(bb, lut, umbra_iadd(bb, nxt,  umbra_iimm(bb,1)));
    umbra_f32 b1 = umbra_fload(bb, lut, umbra_iadd(bb, nxt,  umbra_iimm(bb,2)));
    umbra_f32 a1 = umbra_fload(bb, lut, umbra_iadd(bb, nxt,  umbra_iimm(bb,3)));

    return (umbra_color){
        lerp_f(bb, r0, r1, frac),
        lerp_f(bb, g0, g1, frac),
        lerp_f(bb, b0, b1, frac),
        lerp_f(bb, a0, a1, frac),
    };
}

umbra_color umbra_shader_linear_2(BB *bb, umbra_f32 x, umbra_f32 y) {
    int fi = umbra_reserve_f32(bb, 3);
    int ci = umbra_reserve_f32(bb, 8);
    return lerp_2stop_(bb, linear_t_(bb, fi, x, y), ci);
}
umbra_color umbra_shader_radial_2(BB *bb, umbra_f32 x, umbra_f32 y) {
    int fi = umbra_reserve_f32(bb, 3);
    int ci = umbra_reserve_f32(bb, 8);
    return lerp_2stop_(bb, radial_t_(bb, fi, x, y), ci);
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
        umbra_f32 sx = umbra_fadd(bb, x, umbra_fimm(bb, jitter[s-1][0]));
        umbra_f32 sy = umbra_fadd(bb, y, umbra_fimm(bb, jitter[s-1][1]));
        umbra_color c = inner(bb, sx, sy);
        sum.r = umbra_fadd(bb, sum.r, c.r);
        sum.g = umbra_fadd(bb, sum.g, c.g);
        sum.b = umbra_fadd(bb, sum.b, c.b);
        sum.a = umbra_fadd(bb, sum.a, c.a);
    }

    umbra_f32 inv = umbra_fimm(bb, 1.0f / (float)n);
    return (umbra_color){
        umbra_fmul(bb, sum.r, inv),
        umbra_fmul(bb, sum.g, inv),
        umbra_fmul(bb, sum.b, inv),
        umbra_fmul(bb, sum.a, inv),
    };
}

umbra_color umbra_blend_src(BB *bb, umbra_color src, umbra_color dst) {
    (void)bb; (void)dst;
    return src;
}

umbra_color umbra_blend_srcover(BB *bb, umbra_color src, umbra_color dst) {
    umbra_f32 one   = umbra_fimm(bb, 1.0f);
    umbra_f32 inv_a = umbra_fsub(bb, one, src.a);
    return (umbra_color){
        umbra_fadd(bb, src.r, umbra_fmul(bb, dst.r, inv_a)),
        umbra_fadd(bb, src.g, umbra_fmul(bb, dst.g, inv_a)),
        umbra_fadd(bb, src.b, umbra_fmul(bb, dst.b, inv_a)),
        umbra_fadd(bb, src.a, umbra_fmul(bb, dst.a, inv_a)),
    };
}

umbra_color umbra_blend_dstover(BB *bb, umbra_color src, umbra_color dst) {
    umbra_f32 one   = umbra_fimm(bb, 1.0f);
    umbra_f32 inv_a = umbra_fsub(bb, one, dst.a);
    return (umbra_color){
        umbra_fadd(bb, dst.r, umbra_fmul(bb, src.r, inv_a)),
        umbra_fadd(bb, dst.g, umbra_fmul(bb, src.g, inv_a)),
        umbra_fadd(bb, dst.b, umbra_fmul(bb, src.b, inv_a)),
        umbra_fadd(bb, dst.a, umbra_fmul(bb, src.a, inv_a)),
    };
}

umbra_color umbra_blend_multiply(BB *bb, umbra_color src, umbra_color dst) {
    umbra_f32 one = umbra_fimm(bb, 1.0f);
    umbra_f32 inv_sa = umbra_fsub(bb, one, src.a);
    umbra_f32 inv_da = umbra_fsub(bb, one, dst.a);
    umbra_f32 r = umbra_fadd(bb, umbra_fmul(bb, src.r, dst.r),
                   umbra_fadd(bb, umbra_fmul(bb, src.r, inv_da),
                                      umbra_fmul(bb, dst.r, inv_sa)));
    umbra_f32 g = umbra_fadd(bb, umbra_fmul(bb, src.g, dst.g),
                   umbra_fadd(bb, umbra_fmul(bb, src.g, inv_da),
                                      umbra_fmul(bb, dst.g, inv_sa)));
    umbra_f32 b = umbra_fadd(bb, umbra_fmul(bb, src.b, dst.b),
                   umbra_fadd(bb, umbra_fmul(bb, src.b, inv_da),
                                      umbra_fmul(bb, dst.b, inv_sa)));
    umbra_f32 a = umbra_fadd(bb, umbra_fmul(bb, src.a, dst.a),
                   umbra_fadd(bb, umbra_fmul(bb, src.a, inv_da),
                                      umbra_fmul(bb, dst.a, inv_sa)));
    return (umbra_color){r, g, b, a};
}

umbra_f32 umbra_coverage_rect(BB *bb, umbra_f32 x, umbra_f32 y) {
    int fi = umbra_reserve_f32(bb, 4);
    umbra_f32 l = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi));
    umbra_f32 t = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+1));
    umbra_f32 r = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+2));
    umbra_f32 b = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+3));
    umbra_i32 inside = umbra_and(bb,
        umbra_and(bb, umbra_fge(bb, x, l), umbra_flt(bb, x, r)),
        umbra_and(bb, umbra_fge(bb, y, t), umbra_flt(bb, y, b)));
    umbra_f32 one_f  = umbra_fimm(bb, 1.0f);
    umbra_f32 zero_f = umbra_fimm(bb, 0.0f);
    return umbra_fsel(bb, inside, one_f, zero_f);
}

umbra_f32 umbra_coverage_bitmap(BB *bb, umbra_f32 x, umbra_f32 y) {
    (void)x; (void)y;
    int bmp_off = umbra_reserve_ptr(bb);
    umbra_ptr bmp = umbra_deref_ptr(bb, (umbra_ptr){1}, bmp_off);
    umbra_i32 ix = umbra_lane(bb);
    umbra_i32 val = umbra_i16load(bb, bmp, ix);
    umbra_f32 inv255 = umbra_fimm(bb, 1.0f / 255.0f);
    return umbra_fmul(bb, umbra_itof(bb, val), inv255);
}

umbra_f32 umbra_coverage_sdf(BB *bb, umbra_f32 x, umbra_f32 y) {
    (void)x; (void)y;
    int bmp_off = umbra_reserve_ptr(bb);
    umbra_ptr bmp = umbra_deref_ptr(bb, (umbra_ptr){1}, bmp_off);
    umbra_i32 ix = umbra_lane(bb);
    umbra_i32 raw = umbra_i16load(bb, bmp, ix);
    umbra_f32 inv255 = umbra_fimm(bb, 1.0f / 255.0f);
    umbra_f32 dist = umbra_fmul(bb, umbra_itof(bb, raw), inv255);
    umbra_f32 lo    = umbra_fimm(bb, 0.4375f);
    umbra_f32 scale = umbra_fimm(bb, 8.0f);
    umbra_f32 shifted = umbra_fsub(bb, dist, lo);
    umbra_f32 scaled  = umbra_fmul(bb, shifted, scale);
    umbra_f32 zero = umbra_fimm(bb, 0.0f);
    umbra_f32 one  = umbra_fimm(bb, 1.0f);
    return umbra_fmin(bb, umbra_fmax(bb, scaled, zero), one);
}

umbra_f32 umbra_coverage_bitmap_matrix(BB *bb, umbra_f32 x, umbra_f32 y) {
    int fi = umbra_reserve_f32(bb, 11);
    int bmp_off = umbra_reserve_ptr(bb);
    umbra_ptr bmp = umbra_deref_ptr(bb, (umbra_ptr){1}, bmp_off);

    umbra_f32 m0 = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi));
    umbra_f32 m1 = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+1));
    umbra_f32 m2 = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+2));
    umbra_f32 m3 = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+3));
    umbra_f32 m4 = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+4));
    umbra_f32 m5 = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+5));
    umbra_f32 m6 = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+6));
    umbra_f32 m7 = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+7));
    umbra_f32 m8 = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+8));
    umbra_f32 bw = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+9));
    umbra_f32 bh = umbra_fload(bb, (umbra_ptr){1}, umbra_iimm(bb, fi+10));

    umbra_f32 w = umbra_fadd(bb, umbra_fadd(bb, umbra_fmul(bb, m6, x),
                                                       umbra_fmul(bb, m7, y)), m8);
    umbra_f32 xp = umbra_fdiv(bb,
        umbra_fadd(bb, umbra_fadd(bb, umbra_fmul(bb, m0, x),
                                             umbra_fmul(bb, m1, y)), m2), w);
    umbra_f32 yp = umbra_fdiv(bb,
        umbra_fadd(bb, umbra_fadd(bb, umbra_fmul(bb, m3, x),
                                             umbra_fmul(bb, m4, y)), m5), w);

    umbra_f32 zero_f = umbra_fimm(bb, 0.0f);
    umbra_i32 in = umbra_and(bb,
        umbra_and(bb, umbra_fge(bb, xp, zero_f), umbra_flt(bb, xp, bw)),
        umbra_and(bb, umbra_fge(bb, yp, zero_f), umbra_flt(bb, yp, bh)));

    umbra_f32 one_f = umbra_fimm(bb, 1.0f);
    umbra_f32 xc = umbra_fmin(bb, umbra_fmax(bb, xp, zero_f),
                                      umbra_fsub(bb, bw, one_f));
    umbra_f32 yc = umbra_fmin(bb, umbra_fmax(bb, yp, zero_f),
                                      umbra_fsub(bb, bh, one_f));
    umbra_i32 xi = umbra_ftoi(bb, xc);
    umbra_i32 yi = umbra_ftoi(bb, yc);
    umbra_i32 bwi = umbra_ftoi(bb, bw);
    umbra_i32 idx = umbra_iadd(bb, umbra_imul(bb, yi, bwi), xi);

    umbra_i32 val = umbra_i16load(bb, bmp, idx);
    umbra_f32 inv255 = umbra_fimm(bb, 1.0f / 255.0f);
    umbra_f32 cov = umbra_fmul(bb, umbra_itof(bb, val), inv255);

    return umbra_fsel(bb, in, cov, umbra_fimm(bb, 0.0f));
}

umbra_color umbra_load_8888(BB *bb, umbra_ptr ptr, umbra_i32 ix) {
    umbra_i32 ch[4];
    umbra_load8x4(bb, ptr, ix, ch);
    umbra_f32 inv255 = umbra_fimm(bb, 1.0f / 255.0f);
    return (umbra_color){
        umbra_fmul(bb, umbra_itof(bb, ch[0]), inv255),
        umbra_fmul(bb, umbra_itof(bb, ch[1]), inv255),
        umbra_fmul(bb, umbra_itof(bb, ch[2]), inv255),
        umbra_fmul(bb, umbra_itof(bb, ch[3]), inv255),
    };
}

void umbra_store_8888(BB *bb, umbra_ptr ptr, umbra_i32 ix, umbra_color c) {
    umbra_f32 scale = umbra_fimm(bb, 255.0f);
    umbra_f32 half_ = umbra_fimm(bb, 0.5f);
    umbra_i32 ch[4] = {
        umbra_ftoi(bb, umbra_fadd(bb, umbra_fmul(bb, c.r, scale), half_)),
        umbra_ftoi(bb, umbra_fadd(bb, umbra_fmul(bb, c.g, scale), half_)),
        umbra_ftoi(bb, umbra_fadd(bb, umbra_fmul(bb, c.b, scale), half_)),
        umbra_ftoi(bb, umbra_fadd(bb, umbra_fmul(bb, c.a, scale), half_)),
    };
    umbra_store8x4(bb, ptr, ix, ch);
}

umbra_color umbra_load_565(BB *bb, umbra_ptr ptr, umbra_i32 ix) {
    umbra_i32 px = umbra_i16load(bb, ptr, ix);
    umbra_i32 r32 = umbra_ushr(bb, px, umbra_iimm(bb, 11));
    umbra_i32 g32 = umbra_and(bb, umbra_ushr(bb, px, umbra_iimm(bb, 5)),
                                      umbra_iimm(bb, 0x3f));
    umbra_i32 b32 = umbra_and(bb, px, umbra_iimm(bb, 0x1f));
    umbra_f32 inv31 = umbra_fimm(bb, 1.0f / 31.0f);
    umbra_f32 inv63 = umbra_fimm(bb, 1.0f / 63.0f);
    return (umbra_color){
        umbra_fmul(bb, umbra_itof(bb, r32), inv31),
        umbra_fmul(bb, umbra_itof(bb, g32), inv63),
        umbra_fmul(bb, umbra_itof(bb, b32), inv31),
        umbra_fimm(bb, 1.0f),
    };
}

void umbra_store_565(BB *bb, umbra_ptr ptr, umbra_i32 ix, umbra_color c) {
    umbra_f32 s31   = umbra_fimm(bb, 31.0f);
    umbra_f32 s63   = umbra_fimm(bb, 63.0f);
    umbra_f32 half_ = umbra_fimm(bb, 0.5f);
    umbra_i32 r = umbra_ftoi(bb, umbra_fadd(bb, umbra_fmul(bb, c.r, s31), half_));
    umbra_i32 g = umbra_ftoi(bb, umbra_fadd(bb, umbra_fmul(bb, c.g, s63), half_));
    umbra_i32 b = umbra_ftoi(bb, umbra_fadd(bb, umbra_fmul(bb, c.b, s31), half_));
    umbra_i32 px = umbra_or(bb, umbra_or(bb, umbra_ishl(bb, r, umbra_iimm(bb, 11)),
                                                    umbra_ishl(bb, g, umbra_iimm(bb, 5))), b);
    umbra_i16store(bb, ptr, ix, px);
}

umbra_color umbra_load_1010102(BB *bb, umbra_ptr ptr, umbra_i32 ix) {
    umbra_i32 px    = umbra_iload(bb, ptr, ix);
    umbra_i32 m10   = umbra_iimm(bb, 0x3ff);
    umbra_i32 r32   = umbra_and(bb, px, m10);
    umbra_i32 g32   = umbra_and(bb, umbra_ushr(bb, px, umbra_iimm(bb, 10)), m10);
    umbra_i32 b32   = umbra_and(bb, umbra_ushr(bb, px, umbra_iimm(bb, 20)), m10);
    umbra_i32 a32   = umbra_ushr(bb, px, umbra_iimm(bb, 30));
    umbra_f32 inv1023 = umbra_fimm(bb, 1.0f / 1023.0f);
    umbra_f32 inv3    = umbra_fimm(bb, 1.0f / 3.0f);
    return (umbra_color){
        umbra_fmul(bb, umbra_itof(bb, r32), inv1023),
        umbra_fmul(bb, umbra_itof(bb, g32), inv1023),
        umbra_fmul(bb, umbra_itof(bb, b32), inv1023),
        umbra_fmul(bb, umbra_itof(bb, a32), inv3),
    };
}

void umbra_store_1010102(BB *bb, umbra_ptr ptr, umbra_i32 ix, umbra_color c) {
    umbra_f32 s1023 = umbra_fimm(bb, 1023.0f);
    umbra_f32 s3    = umbra_fimm(bb, 3.0f);
    umbra_f32 half_ = umbra_fimm(bb, 0.5f);
    umbra_i32 r = umbra_ftoi(bb, umbra_fadd(bb, umbra_fmul(bb, c.r, s1023), half_));
    umbra_i32 g = umbra_ftoi(bb, umbra_fadd(bb, umbra_fmul(bb, c.g, s1023), half_));
    umbra_i32 b = umbra_ftoi(bb, umbra_fadd(bb, umbra_fmul(bb, c.b, s1023), half_));
    umbra_i32 a = umbra_ftoi(bb, umbra_fadd(bb, umbra_fmul(bb, c.a, s3),    half_));
    umbra_i32 px = umbra_or(bb,
        umbra_or(bb, r, umbra_ishl(bb, g, umbra_iimm(bb, 10))),
        umbra_or(bb, umbra_ishl(bb, b, umbra_iimm(bb, 20)),
                         umbra_ishl(bb, a, umbra_iimm(bb, 30))));
    umbra_istore(bb, ptr, ix, px);
}

umbra_color umbra_load_fp16(BB *bb, umbra_ptr ptr, umbra_i32 ix) {
    umbra_i32 ix4 = umbra_ishl(bb, ix, umbra_iimm(bb, 2));
    return (umbra_color){
        umbra_load_f16(bb, ptr, umbra_iadd(bb, ix4, umbra_iimm(bb, 0))),
        umbra_load_f16(bb, ptr, umbra_iadd(bb, ix4, umbra_iimm(bb, 1))),
        umbra_load_f16(bb, ptr, umbra_iadd(bb, ix4, umbra_iimm(bb, 2))),
        umbra_load_f16(bb, ptr, umbra_iadd(bb, ix4, umbra_iimm(bb, 3))),
    };
}

void umbra_store_fp16(BB *bb, umbra_ptr ptr, umbra_i32 ix, umbra_color c) {
    umbra_i32 ix4 = umbra_ishl(bb, ix, umbra_iimm(bb, 2));
    umbra_store_f16(bb, ptr, umbra_iadd(bb, ix4, umbra_iimm(bb, 0)), c.r);
    umbra_store_f16(bb, ptr, umbra_iadd(bb, ix4, umbra_iimm(bb, 1)), c.g);
    umbra_store_f16(bb, ptr, umbra_iadd(bb, ix4, umbra_iimm(bb, 2)), c.b);
    umbra_store_f16(bb, ptr, umbra_iadd(bb, ix4, umbra_iimm(bb, 3)), c.a);
}

umbra_color umbra_load_fp16_planar(BB *bb, umbra_ptr ptr, umbra_i32 ix) {
    int si = umbra_reserve_i32(bb, 1);
    umbra_i32 stride = umbra_iload(bb, (umbra_ptr){1}, umbra_iimm(bb, si));
    umbra_i32 s2 = umbra_imul(bb, stride, umbra_iimm(bb, 2));
    umbra_i32 s3 = umbra_imul(bb, stride, umbra_iimm(bb, 3));
    return (umbra_color){
        umbra_load_f16(bb, ptr, ix),
        umbra_load_f16(bb, ptr, umbra_iadd(bb, ix, stride)),
        umbra_load_f16(bb, ptr, umbra_iadd(bb, ix, s2)),
        umbra_load_f16(bb, ptr, umbra_iadd(bb, ix, s3)),
    };
}

void umbra_store_fp16_planar(BB *bb, umbra_ptr ptr, umbra_i32 ix, umbra_color c) {
    int si = umbra_reserve_i32(bb, 1);
    umbra_i32 stride = umbra_iload(bb, (umbra_ptr){1}, umbra_iimm(bb, si));
    umbra_i32 s2 = umbra_imul(bb, stride, umbra_iimm(bb, 2));
    umbra_i32 s3 = umbra_imul(bb, stride, umbra_iimm(bb, 3));
    umbra_store_f16(bb, ptr, ix, c.r);
    umbra_store_f16(bb, ptr, umbra_iadd(bb, ix, stride), c.g);
    umbra_store_f16(bb, ptr, umbra_iadd(bb, ix, s2), c.b);
    umbra_store_f16(bb, ptr, umbra_iadd(bb, ix, s3), c.a);
}

void umbra_gradient_lut_even(float *out, int lut_n,
                             int n_stops, float const colors[][4]) {
    for (int i = 0; i < lut_n; i++) {
        float t   = (float)i / (float)(lut_n - 1);
        float seg = t * (float)(n_stops - 1);
        int   idx = (int)seg;
        if (idx >= n_stops - 1) { idx = n_stops - 2; }
        float f = seg - (float)idx;
        for (int ch = 0; ch < 4; ch++) {
            out[i*4+ch] = colors[idx][ch]*(1-f) + colors[idx+1][ch]*f;
        }
    }
}

void umbra_gradient_lut(float *out, int lut_n,
                        int n_stops, float const positions[],
                        float const colors[][4]) {
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
            out[i*4+ch] = colors[seg][ch]*(1-f) + colors[seg+1][ch]*f;
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
    umbra_f32 r = umbra_fimm(bb, (float)coeffs[n-1]);
    for (int i = n - 2; i >= 0; i--) {
        r = umbra_fadd(bb, umbra_fmul(bb, r, x), umbra_fimm(bb, (float)coeffs[i]));
    }
    return r;
}

umbra_color umbra_transfer_invert(BB *bb, umbra_color c, umbra_transfer const *tf) {
    double poly[POLY_N];
    int deg = 7;
    fit_poly((double)tf->g, deg, poly);

    for (int ch = 0; ch < 3; ch++) {
        umbra_f32 *h = ch == 0 ? &c.r : ch == 1 ? &c.g : &c.b;
        umbra_f32 x = clamp01(bb, *h);

        umbra_f32 lin = umbra_fadd(bb,
            umbra_fmul(bb, x, umbra_fimm(bb, tf->c)),
            umbra_fimm(bb, tf->f));

        umbra_f32 t = umbra_fadd(bb,
            umbra_fmul(bb, x, umbra_fimm(bb, tf->a)),
            umbra_fimm(bb, tf->b));
        t = umbra_fmax(bb, t, umbra_fimm(bb, 0.0f));
        umbra_f32 cur = umbra_fadd(bb, eval_poly_ir(bb, t, deg, poly),
                                           umbra_fimm(bb, tf->e));

        umbra_i32 mask = umbra_fge(bb, x, umbra_fimm(bb, tf->d));
        *h = umbra_fsel(bb, mask, cur, lin);
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
        umbra_f32 *h = ch == 0 ? &c.r : ch == 1 ? &c.g : &c.b;
        umbra_f32 y = clamp01(bb, *h);

        umbra_f32 lin = umbra_fmul(bb, umbra_fsub(bb, y,
            umbra_fimm(bb, tf->f)), umbra_fimm(bb, inv_c));

        umbra_f32 t = umbra_fsub(bb, y, umbra_fimm(bb, tf->e));
        t = umbra_fmax(bb, t, umbra_fimm(bb, 0.0f));
        umbra_f32 s = umbra_fsqrt(bb, t);
        umbra_f32 cur = umbra_fmul(bb,
            umbra_fsub(bb, eval_poly_ir(bb, s, deg, poly), umbra_fimm(bb, tf->b)),
            umbra_fimm(bb, inv_a));

        umbra_i32 mask = umbra_fge(bb, y, umbra_fimm(bb, lin_thresh));
        *h = umbra_fsel(bb, mask, cur, lin);
    }
    return c;
}
