#include "umbra_draw.h"

typedef struct umbra_basic_block BB;

// --- Pipeline builder ---

struct umbra_basic_block* umbra_draw_build(umbra_shader_fn   shader,
                                           umbra_coverage_fn coverage,
                                           umbra_blend_fn    blend,
                                           umbra_load_fn     load,
                                           umbra_store_fn    store) {
    BB *bb = umbra_basic_block();
    umbra_v32 ix = umbra_lane(bb);

    // x0 and y are uniforms (single values), loaded at constant index 0.
    umbra_v32 x0 = umbra_load_32(bb, (umbra_ptr){1}, umbra_imm_32(bb, 0));
    umbra_v32 y  = umbra_load_32(bb, (umbra_ptr){2}, umbra_imm_32(bb, 0));

    // x = x0 + lane gives the global x coordinate for each pixel.
    umbra_v32 xf = umbra_f32_from_i32(bb, umbra_add_i32(bb, x0, ix));
    umbra_v32 yf = umbra_f32_from_i32(bb, y);

    // Coverage first — enables future early-out if all zero.
    umbra_half cov = {0};
    if (coverage) {
        cov = coverage(bb, xf, yf);
        // TODO: early-out when coverage is all zero (ptest)
    }

    // Source color
    umbra_color src = {
        umbra_imm_half(bb, 0),
        umbra_imm_half(bb, 0),
        umbra_imm_half(bb, 0),
        umbra_imm_half(bb, 0),
    };
    if (shader) {
        src = shader(bb, xf, yf);
    }

    // Destination
    umbra_color dst = {
        umbra_imm_half(bb, 0),
        umbra_imm_half(bb, 0),
        umbra_imm_half(bb, 0),
        umbra_imm_half(bb, 0),
    };
    if (load) {
        dst = load(bb, (umbra_ptr){0}, ix);
    }

    // Blend
    umbra_color out;
    if (blend) {
        out = blend(bb, src, dst);
    } else {
        out = src;
    }

    // Apply coverage: lerp(dst, out, cov)  →  dst + (out - dst) * cov
    if (coverage) {
        out.r = umbra_add_half(bb, dst.r, umbra_mul_half(bb, umbra_sub_half(bb, out.r, dst.r), cov));
        out.g = umbra_add_half(bb, dst.g, umbra_mul_half(bb, umbra_sub_half(bb, out.g, dst.g), cov));
        out.b = umbra_add_half(bb, dst.b, umbra_mul_half(bb, umbra_sub_half(bb, out.b, dst.b), cov));
        out.a = umbra_add_half(bb, dst.a, umbra_mul_half(bb, umbra_sub_half(bb, out.a, dst.a), cov));
    }

    // Store
    if (store) {
        store(bb, (umbra_ptr){0}, ix, out);
    }

    return bb;
}

// --- Built-in shaders ---

umbra_color umbra_shader_solid(BB *bb, umbra_v32 x, umbra_v32 y) {
    (void)x; (void)y;
    // Color from uniforms: p3 = packed RGBA as fp16x4 (r,g,b,a at indices 0-3)
    umbra_half r = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_32(bb, 0));
    umbra_half g = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_32(bb, 1));
    umbra_half b = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_32(bb, 2));
    umbra_half a = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_32(bb, 3));
    return (umbra_color){r, g, b, a};
}

static umbra_v32 clamp01(BB *bb, umbra_v32 t) {
    return umbra_min_f32(bb, umbra_max_f32(bb, t, umbra_imm_32(bb, 0)),
                              umbra_imm_32(bb, 0x3f800000));
}

static umbra_half lerp_h(BB *bb, umbra_half a, umbra_half b, umbra_half t) {
    return umbra_add_half(bb, a, umbra_mul_half(bb, umbra_sub_half(bb, b, a), t));
}

static umbra_v32 linear_t(BB *bb, umbra_v32 x, umbra_v32 y) {
    umbra_v32 a = umbra_load_32(bb, (umbra_ptr){5}, umbra_imm_32(bb, 0));
    umbra_v32 b = umbra_load_32(bb, (umbra_ptr){5}, umbra_imm_32(bb, 1));
    umbra_v32 c = umbra_load_32(bb, (umbra_ptr){5}, umbra_imm_32(bb, 2));
    return clamp01(bb, umbra_add_f32(bb, umbra_add_f32(bb, umbra_mul_f32(bb, a, x),
                                                            umbra_mul_f32(bb, b, y)), c));
}

static umbra_v32 radial_t(BB *bb, umbra_v32 x, umbra_v32 y) {
    umbra_v32 cx    = umbra_load_32(bb, (umbra_ptr){5}, umbra_imm_32(bb, 0));
    umbra_v32 cy    = umbra_load_32(bb, (umbra_ptr){5}, umbra_imm_32(bb, 1));
    umbra_v32 inv_r = umbra_load_32(bb, (umbra_ptr){5}, umbra_imm_32(bb, 2));
    umbra_v32 dx = umbra_sub_f32(bb, x, cx);
    umbra_v32 dy = umbra_sub_f32(bb, y, cy);
    umbra_v32 d2 = umbra_add_f32(bb, umbra_mul_f32(bb, dx, dx), umbra_mul_f32(bb, dy, dy));
    return clamp01(bb, umbra_mul_f32(bb, umbra_sqrt_f32(bb, d2), inv_r));
}

static umbra_color lerp_2stop(BB *bb, umbra_v32 t_f32) {
    umbra_half r0 = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_32(bb, 0));
    umbra_half g0 = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_32(bb, 1));
    umbra_half b0 = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_32(bb, 2));
    umbra_half a0 = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_32(bb, 3));
    umbra_half r1 = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_32(bb, 4));
    umbra_half g1 = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_32(bb, 5));
    umbra_half b1 = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_32(bb, 6));
    umbra_half a1 = umbra_load_half(bb, (umbra_ptr){3}, umbra_imm_32(bb, 7));
    umbra_half t  = umbra_half_from_f32(bb, t_f32);
    return (umbra_color){
        lerp_h(bb, r0, r1, t),
        lerp_h(bb, g0, g1, t),
        lerp_h(bb, b0, b1, t),
        lerp_h(bb, a0, a1, t),
    };
}

static umbra_color sample_lut(BB *bb, umbra_v32 t_f32) {
    umbra_v32 N_f   = umbra_load_32(bb, (umbra_ptr){5}, umbra_imm_32(bb, 3));
    umbra_v32 one_f = umbra_imm_32(bb, 0x3f800000);
    umbra_v32 two_f = umbra_imm_32(bb, 0x40000000);
    umbra_v32 N_m1  = umbra_sub_f32(bb, N_f, one_f);
    umbra_v32 N_m2  = umbra_sub_f32(bb, N_f, two_f);

    umbra_v32 t_sc  = umbra_mul_f32(bb, t_f32, N_m1);
    umbra_v32 idx_f = umbra_min_f32(bb, umbra_f32_from_i32(bb, umbra_i32_from_f32(bb, t_sc)), N_m2);
    umbra_v32 frac  = umbra_sub_f32(bb, t_sc, idx_f);

    umbra_v32 idx  = umbra_i32_from_f32(bb, idx_f);
    umbra_v32 base = umbra_shl_i32(bb, idx, umbra_imm_32(bb, 2));
    umbra_v32 nxt  = umbra_add_i32(bb, base, umbra_imm_32(bb, 4));

    umbra_half r0 = umbra_load_half(bb, (umbra_ptr){3}, base);
    umbra_half g0 = umbra_load_half(bb, (umbra_ptr){3}, umbra_add_i32(bb, base, umbra_imm_32(bb,1)));
    umbra_half b0 = umbra_load_half(bb, (umbra_ptr){3}, umbra_add_i32(bb, base, umbra_imm_32(bb,2)));
    umbra_half a0 = umbra_load_half(bb, (umbra_ptr){3}, umbra_add_i32(bb, base, umbra_imm_32(bb,3)));
    umbra_half r1 = umbra_load_half(bb, (umbra_ptr){3}, nxt);
    umbra_half g1 = umbra_load_half(bb, (umbra_ptr){3}, umbra_add_i32(bb, nxt,  umbra_imm_32(bb,1)));
    umbra_half b1 = umbra_load_half(bb, (umbra_ptr){3}, umbra_add_i32(bb, nxt,  umbra_imm_32(bb,2)));
    umbra_half a1 = umbra_load_half(bb, (umbra_ptr){3}, umbra_add_i32(bb, nxt,  umbra_imm_32(bb,3)));

    umbra_half ft = umbra_half_from_f32(bb, frac);
    return (umbra_color){
        lerp_h(bb, r0, r1, ft),
        lerp_h(bb, g0, g1, ft),
        lerp_h(bb, b0, b1, ft),
        lerp_h(bb, a0, a1, ft),
    };
}

umbra_color umbra_shader_linear_2(BB *bb, umbra_v32 x, umbra_v32 y) {
    return lerp_2stop(bb, linear_t(bb, x, y));
}
umbra_color umbra_shader_radial_2(BB *bb, umbra_v32 x, umbra_v32 y) {
    return lerp_2stop(bb, radial_t(bb, x, y));
}
umbra_color umbra_shader_linear_grad(BB *bb, umbra_v32 x, umbra_v32 y) {
    return sample_lut(bb, linear_t(bb, x, y));
}
umbra_color umbra_shader_radial_grad(BB *bb, umbra_v32 x, umbra_v32 y) {
    return sample_lut(bb, radial_t(bb, x, y));
}

// --- Built-in blend modes ---

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
    // multiply: src * dst + src * (1 - dst.a) + dst * (1 - src.a)
    // Simplified: src*dst + src*(1-da) + dst*(1-sa)
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

// --- Built-in coverage ---

umbra_half umbra_coverage_rect(BB *bb, umbra_v32 x, umbra_v32 y) {
    // Rect bounds from p4: {left, top, right, bottom} as f32.
    umbra_v32 ix = umbra_lane(bb);
    umbra_v32 l = umbra_load_32(bb, (umbra_ptr){4}, umbra_imm_32(bb, 0));
    umbra_v32 t = umbra_load_32(bb, (umbra_ptr){4}, umbra_imm_32(bb, 1));
    umbra_v32 r = umbra_load_32(bb, (umbra_ptr){4}, umbra_imm_32(bb, 2));
    umbra_v32 b = umbra_load_32(bb, (umbra_ptr){4}, umbra_imm_32(bb, 3));
    (void)ix;
    // 1.0 if inside, 0.0 if outside.
    umbra_v32 inside = umbra_and_32(bb,
        umbra_and_32(bb, umbra_ge_f32(bb, x, l), umbra_lt_f32(bb, x, r)),
        umbra_and_32(bb, umbra_ge_f32(bb, y, t), umbra_lt_f32(bb, y, b)));
    // Convert all-ones mask to 1.0h
    umbra_half one = umbra_imm_half(bb, 0x3c00);
    umbra_half zero = umbra_imm_half(bb, 0);
    return umbra_sel_half(bb, umbra_half_from_f32(bb, inside), one, zero);
}

umbra_half umbra_coverage_bitmap(BB *bb, umbra_v32 x, umbra_v32 y) {
    (void)x; (void)y;
    umbra_v32 ix = umbra_lane(bb);
    umbra_v16 val = umbra_load_16(bb, (umbra_ptr){4}, ix);
    umbra_half inv255 = umbra_imm_half(bb, 0x1c04);
    return umbra_mul_half(bb, umbra_half_from_i16(bb, val), inv255);
}

umbra_half umbra_coverage_sdf(BB *bb, umbra_v32 x, umbra_v32 y) {
    (void)x; (void)y;
    umbra_v32 ix = umbra_lane(bb);
    umbra_v16 raw = umbra_load_16(bb, (umbra_ptr){4}, ix);
    umbra_half inv255 = umbra_imm_half(bb, 0x1c04);
    umbra_half dist = umbra_mul_half(bb, umbra_half_from_i16(bb, raw), inv255);
    umbra_half lo    = umbra_imm_half(bb, 0x3733);   // ~0.45
    umbra_half scale = umbra_imm_half(bb, 0x4900);   // 10.0
    umbra_half shifted = umbra_sub_half(bb, dist, lo);
    umbra_half scaled  = umbra_mul_half(bb, shifted, scale);
    umbra_half zero = umbra_imm_half(bb, 0);
    umbra_half one  = umbra_imm_half(bb, 0x3c00);
    return umbra_min_half(bb, umbra_max_half(bb, scaled, zero), one);
}

umbra_half umbra_coverage_bitmap_matrix(BB *bb, umbra_v32 x, umbra_v32 y) {
    // p5 = { m[9], bw, bh } as f32: inverse 3x3 matrix + bitmap dimensions.
    // Maps screen (x,y) → bitmap (x',y') via projective transform.
    umbra_v32 m0 = umbra_load_32(bb, (umbra_ptr){5}, umbra_imm_32(bb, 0));
    umbra_v32 m1 = umbra_load_32(bb, (umbra_ptr){5}, umbra_imm_32(bb, 1));
    umbra_v32 m2 = umbra_load_32(bb, (umbra_ptr){5}, umbra_imm_32(bb, 2));
    umbra_v32 m3 = umbra_load_32(bb, (umbra_ptr){5}, umbra_imm_32(bb, 3));
    umbra_v32 m4 = umbra_load_32(bb, (umbra_ptr){5}, umbra_imm_32(bb, 4));
    umbra_v32 m5 = umbra_load_32(bb, (umbra_ptr){5}, umbra_imm_32(bb, 5));
    umbra_v32 m6 = umbra_load_32(bb, (umbra_ptr){5}, umbra_imm_32(bb, 6));
    umbra_v32 m7 = umbra_load_32(bb, (umbra_ptr){5}, umbra_imm_32(bb, 7));
    umbra_v32 m8 = umbra_load_32(bb, (umbra_ptr){5}, umbra_imm_32(bb, 8));
    umbra_v32 bw = umbra_load_32(bb, (umbra_ptr){5}, umbra_imm_32(bb, 9));
    umbra_v32 bh = umbra_load_32(bb, (umbra_ptr){5}, umbra_imm_32(bb, 10));

    // w = m6*x + m7*y + m8
    umbra_v32 w = umbra_add_f32(bb, umbra_add_f32(bb, umbra_mul_f32(bb, m6, x),
                                                       umbra_mul_f32(bb, m7, y)), m8);
    // x' = (m0*x + m1*y + m2) / w
    umbra_v32 xp = umbra_div_f32(bb,
        umbra_add_f32(bb, umbra_add_f32(bb, umbra_mul_f32(bb, m0, x),
                                             umbra_mul_f32(bb, m1, y)), m2), w);
    // y' = (m3*x + m4*y + m5) / w
    umbra_v32 yp = umbra_div_f32(bb,
        umbra_add_f32(bb, umbra_add_f32(bb, umbra_mul_f32(bb, m3, x),
                                             umbra_mul_f32(bb, m4, y)), m5), w);

    // Bounds check.
    umbra_v32 zero_f = umbra_imm_32(bb, 0);
    umbra_v32 in = umbra_and_32(bb,
        umbra_and_32(bb, umbra_ge_f32(bb, xp, zero_f), umbra_lt_f32(bb, xp, bw)),
        umbra_and_32(bb, umbra_ge_f32(bb, yp, zero_f), umbra_lt_f32(bb, yp, bh)));

    // Clamp coords to valid range so gather never touches out-of-bounds memory.
    // The sel_half at the end zeros out-of-bounds results anyway.
    umbra_v32 one_f = umbra_imm_32(bb, 0x3f800000);
    umbra_v32 xc = umbra_min_f32(bb, umbra_max_f32(bb, xp, zero_f),
                                      umbra_sub_f32(bb, bw, one_f));
    umbra_v32 yc = umbra_min_f32(bb, umbra_max_f32(bb, yp, zero_f),
                                      umbra_sub_f32(bb, bh, one_f));
    umbra_v32 xi = umbra_i32_from_f32(bb, xc);
    umbra_v32 yi = umbra_i32_from_f32(bb, yc);
    umbra_v32 bwi = umbra_i32_from_f32(bb, bw);
    umbra_v32 idx = umbra_add_i32(bb, umbra_mul_i32(bb, yi, bwi), xi);

    umbra_v16 val = umbra_load_16(bb, (umbra_ptr){4}, idx);
    umbra_half inv255 = umbra_imm_half(bb, 0x1c04);
    umbra_half cov = umbra_mul_half(bb, umbra_half_from_i16(bb, val), inv255);

    // Zero out-of-bounds pixels.
    // Select in f32 to avoid converting a 32-bit mask through float→half,
    // which produces NaN whose bit pattern is GPU-dependent.
    umbra_v32 cov_f32 = umbra_f32_from_half(bb, cov);
    umbra_v32 masked  = umbra_sel_32(bb, in, cov_f32, umbra_imm_32(bb, 0));
    return umbra_half_from_f32(bb, masked);
}

// --- Built-in pixel formats ---

umbra_color umbra_load_8888(BB *bb, umbra_ptr ptr, umbra_v32 ix) {
    umbra_v16 ch[4];
    umbra_load_8x4(bb, ptr, ix, ch);
    umbra_half inv255 = umbra_imm_half(bb, 0x1c04);  // 1/255 in fp16
    return (umbra_color){
        umbra_mul_half(bb, umbra_half_from_i16(bb, ch[0]), inv255),
        umbra_mul_half(bb, umbra_half_from_i16(bb, ch[1]), inv255),
        umbra_mul_half(bb, umbra_half_from_i16(bb, ch[2]), inv255),
        umbra_mul_half(bb, umbra_half_from_i16(bb, ch[3]), inv255),
    };
}

void umbra_store_8888(BB *bb, umbra_ptr ptr, umbra_v32 ix, umbra_color c) {
    umbra_half scale = umbra_imm_half(bb, 0x5bf8);  // 255.0 in fp16
    umbra_half half_ = umbra_imm_half(bb, 0x3800);   // 0.5 in fp16
    umbra_v16 ch[4] = {
        umbra_i16_from_half(bb, umbra_add_half(bb, umbra_mul_half(bb, c.r, scale), half_)),
        umbra_i16_from_half(bb, umbra_add_half(bb, umbra_mul_half(bb, c.g, scale), half_)),
        umbra_i16_from_half(bb, umbra_add_half(bb, umbra_mul_half(bb, c.b, scale), half_)),
        umbra_i16_from_half(bb, umbra_add_half(bb, umbra_mul_half(bb, c.a, scale), half_)),
    };
    umbra_store_8x4(bb, ptr, ix, ch);
}

umbra_color umbra_load_fp16(BB *bb, umbra_ptr ptr, umbra_v32 ix) {
    // fp16x4 layout: r at ptr+0, g at ptr+1, b at ptr+2, a at ptr+3 (interleaved)
    // We need 4 separate half loads at strided offsets.
    // Use ptr as base, index*4+ch for each channel.
    umbra_v32 ix4 = umbra_shl_i32(bb, ix, umbra_imm_32(bb, 2));
    return (umbra_color){
        umbra_load_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_32(bb, 0))),
        umbra_load_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_32(bb, 1))),
        umbra_load_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_32(bb, 2))),
        umbra_load_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_32(bb, 3))),
    };
}

void umbra_store_fp16(BB *bb, umbra_ptr ptr, umbra_v32 ix, umbra_color c) {
    umbra_v32 ix4 = umbra_shl_i32(bb, ix, umbra_imm_32(bb, 2));
    umbra_store_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_32(bb, 0)), c.r);
    umbra_store_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_32(bb, 1)), c.g);
    umbra_store_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_32(bb, 2)), c.b);
    umbra_store_half(bb, ptr, umbra_add_i32(bb, ix4, umbra_imm_32(bb, 3)), c.a);
}

// --- Gradient LUT helpers (CPU-side, no IR) ---

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
