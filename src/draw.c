#include "../include/umbra_draw.h"
#include "assume.h"
#include "bb.h"
#include <math.h>
#include <stdlib.h>
#include <stdint.h>

typedef umbra_val32 val;

static val pack_unorm(struct umbra_builder *b, val ch, val scale) {
    val zero = umbra_imm_f32(b, 0.0f), one = umbra_imm_f32(b, 1.0f);
    return umbra_round_i32(b, umbra_mul_f32(b,
        umbra_min_f32(b, umbra_max_f32(b, ch, zero), one), scale));
}

umbra_color umbra_load_8888(struct umbra_builder *b, umbra_ptr32 src) {
    val r, g, bl, a;
    umbra_load_8x4(b, src, &r, &g, &bl, &a);
    val const inv = umbra_imm_f32(b, 1.0f/255);
    return (umbra_color){
        umbra_mul_f32(b, umbra_f32_from_i32(b, r), inv),
        umbra_mul_f32(b, umbra_f32_from_i32(b, g), inv),
        umbra_mul_f32(b, umbra_f32_from_i32(b, bl), inv),
        umbra_mul_f32(b, umbra_f32_from_i32(b, a), inv),
    };
}
void umbra_store_8888(struct umbra_builder *b, umbra_ptr32 dst, umbra_color c) {
    val s = umbra_imm_f32(b, 255.0f);
    umbra_store_8x4(b, dst, pack_unorm(b, c.r, s), pack_unorm(b, c.g, s),
                            pack_unorm(b, c.b, s), pack_unorm(b, c.a, s));
}

umbra_color umbra_load_565(struct umbra_builder *b, umbra_ptr16 src) {
    val const px = umbra_i32_from_u16(b, umbra_load_16(b, src));
    val const r5 = umbra_shr_u32(b, px, umbra_imm_i32(b, 11));
    val const g6 = umbra_and_32(b, umbra_shr_u32(b, px, umbra_imm_i32(b, 5)),
                                 umbra_imm_i32(b, 0x3F));
    val const b5 = umbra_and_32(b, px, umbra_imm_i32(b, 0x1F));
    return (umbra_color){
        umbra_mul_f32(b, umbra_f32_from_i32(b, r5), umbra_imm_f32(b, 1.0f/31)),
        umbra_mul_f32(b, umbra_f32_from_i32(b, g6), umbra_imm_f32(b, 1.0f/63)),
        umbra_mul_f32(b, umbra_f32_from_i32(b, b5), umbra_imm_f32(b, 1.0f/31)),
        umbra_imm_f32(b, 1.0f),
    };
}
void umbra_store_565(struct umbra_builder *b, umbra_ptr16 dst, umbra_color c) {
    val px = pack_unorm(b, c.b, umbra_imm_f32(b, 31.0f));
    px = umbra_or_32(b, px, umbra_shl_i32(b, pack_unorm(b, c.g, umbra_imm_f32(b, 63.0f)),
                                            umbra_imm_i32(b, 5)));
    px = umbra_or_32(b, px, umbra_shl_i32(b, pack_unorm(b, c.r, umbra_imm_f32(b, 31.0f)),
                                            umbra_imm_i32(b, 11)));
    umbra_store_16(b, dst, umbra_i16_from_i32(b, px));
}

umbra_color umbra_load_1010102(struct umbra_builder *b, umbra_ptr32 src) {
    val const px   = umbra_load_32(b, src);
    val const mask = umbra_imm_i32(b, 0x3FF);
    val const ri   = umbra_and_32(b, px, mask);
    val const gi   = umbra_and_32(b, umbra_shr_u32(b, px, umbra_imm_i32(b, 10)), mask);
    val const bi   = umbra_and_32(b, umbra_shr_u32(b, px, umbra_imm_i32(b, 20)), mask);
    val const ai   = umbra_shr_u32(b, px, umbra_imm_i32(b, 30));
    return (umbra_color){
        umbra_mul_f32(b, umbra_f32_from_i32(b, ri), umbra_imm_f32(b, 1.0f/1023)),
        umbra_mul_f32(b, umbra_f32_from_i32(b, gi), umbra_imm_f32(b, 1.0f/1023)),
        umbra_mul_f32(b, umbra_f32_from_i32(b, bi), umbra_imm_f32(b, 1.0f/1023)),
        umbra_mul_f32(b, umbra_f32_from_i32(b, ai), umbra_imm_f32(b, 1.0f/3)),
    };
}
void umbra_store_1010102(struct umbra_builder *b, umbra_ptr32 dst, umbra_color c) {
    val s10 = umbra_imm_f32(b, 1023.0f);
    val px = pack_unorm(b, c.r, s10);
    px = umbra_or_32(b, px, umbra_shl_i32(b, pack_unorm(b, c.g, s10), umbra_imm_i32(b, 10)));
    px = umbra_or_32(b, px, umbra_shl_i32(b, pack_unorm(b, c.b, s10), umbra_imm_i32(b, 20)));
    px = umbra_or_32(b, px, umbra_shl_i32(b, pack_unorm(b, c.a, umbra_imm_f32(b, 3.0f)),
                                            umbra_imm_i32(b, 30)));
    umbra_store_32(b, dst, px);
}

umbra_color umbra_load_fp16(struct umbra_builder *b, umbra_ptr64 src) {
    umbra_val16 r, g, bl, a;
    umbra_load_16x4(b, src, &r, &g, &bl, &a);
    return (umbra_color){
        umbra_f32_from_f16(b, r), umbra_f32_from_f16(b, g),
        umbra_f32_from_f16(b, bl), umbra_f32_from_f16(b, a),
    };
}
void umbra_store_fp16(struct umbra_builder *b, umbra_ptr64 dst, umbra_color c) {
    umbra_store_16x4(b, dst, umbra_f16_from_f32(b, c.r), umbra_f16_from_f32(b, c.g),
                             umbra_f16_from_f32(b, c.b), umbra_f16_from_f32(b, c.a));
}

umbra_color umbra_load_fp16_planar(struct umbra_builder *b, umbra_ptr16 src) {
    umbra_val16 r, g, bl, a;
    umbra_load_16x4_planar(b, src, &r, &g, &bl, &a);
    return (umbra_color){
        umbra_f32_from_f16(b, r), umbra_f32_from_f16(b, g),
        umbra_f32_from_f16(b, bl), umbra_f32_from_f16(b, a),
    };
}
void umbra_store_fp16_planar(struct umbra_builder *b, umbra_ptr16 dst, umbra_color c) {
    umbra_store_16x4_planar(b, dst, umbra_f16_from_f32(b, c.r), umbra_f16_from_f32(b, c.g),
                                    umbra_f16_from_f32(b, c.b), umbra_f16_from_f32(b, c.a));
}

// Generic wrappers for struct umbra_fmt function pointers (type-erased ptr).
static umbra_color load_8888_(struct umbra_builder *b, int p) { return umbra_load_8888(b, (umbra_ptr32){.ix=(unsigned)p}); }
static umbra_color load_565_(struct umbra_builder *b, int p) { return umbra_load_565(b, (umbra_ptr16){.ix=(unsigned)p}); }
static umbra_color load_1010102_(struct umbra_builder *b, int p) { return umbra_load_1010102(b, (umbra_ptr32){.ix=(unsigned)p}); }
static umbra_color load_fp16_(struct umbra_builder *b, int p) { return umbra_load_fp16(b, (umbra_ptr64){.ix=(unsigned)p}); }
static umbra_color load_fp16p_(struct umbra_builder *b, int p) { return umbra_load_fp16_planar(b, (umbra_ptr16){.ix=(unsigned)p}); }
static void store_8888_(struct umbra_builder *b, int p, umbra_color c) { umbra_store_8888(b, (umbra_ptr32){.ix=(unsigned)p}, c); }
static void store_565_(struct umbra_builder *b, int p, umbra_color c) { umbra_store_565(b, (umbra_ptr16){.ix=(unsigned)p}, c); }
static void store_1010102_(struct umbra_builder *b, int p, umbra_color c) { umbra_store_1010102(b, (umbra_ptr32){.ix=(unsigned)p}, c); }
static void store_fp16_(struct umbra_builder *b, int p, umbra_color c) { umbra_store_fp16(b, (umbra_ptr64){.ix=(unsigned)p}, c); }
static void store_fp16p_(struct umbra_builder *b, int p, umbra_color c) { umbra_store_fp16_planar(b, (umbra_ptr16){.ix=(unsigned)p}, c); }

const struct umbra_fmt umbra_fmt_8888        = { .name="8888",        .bpp=4, .planes=1, .load=load_8888_,    .store=store_8888_ };
const struct umbra_fmt umbra_fmt_565         = { .name="565",         .bpp=2, .planes=1, .load=load_565_,     .store=store_565_ };
const struct umbra_fmt umbra_fmt_1010102     = { .name="1010102",     .bpp=4, .planes=1, .load=load_1010102_, .store=store_1010102_ };
const struct umbra_fmt umbra_fmt_fp16        = { .name="fp16",        .bpp=8, .planes=1, .load=load_fp16_,    .store=store_fp16_ };
const struct umbra_fmt umbra_fmt_fp16_planar = { .name="fp16_planar", .bpp=2, .planes=4, .load=load_fp16p_,   .store=store_fp16p_ };

struct umbra_builder *umbra_draw_build(umbra_shader_fn shader, umbra_coverage_fn coverage,
                                       umbra_blend_fn blend, struct umbra_fmt fmt,
                                       struct umbra_draw_layout *layout) {
    struct umbra_builder  *builder = umbra_builder();
    umbra_val32 const x = umbra_x(builder);
    umbra_val32 const y = umbra_y(builder);
    umbra_val32 const xf = umbra_f32_from_i32(builder, x);
    umbra_val32 const yf = umbra_f32_from_i32(builder, y);

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
    umbra_val32 cov = {0};
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
        dst = fmt.load(builder, 1);
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

    fmt.store(builder, 1, out);

    if (layout) {
        layout->uni = uni;
        layout->data = umbra_uniforms_alloc(uni);
        layout->shader = shader_off;
        layout->coverage = coverage_off;
    } else {
        free(uni);
    }

    return builder;
}

umbra_color umbra_shader_solid(struct umbra_builder *builder, struct umbra_uniforms *u, umbra_val32 x, umbra_val32 y) {
    (void)x;
    (void)y;
    size_t const fi = umbra_uniforms_reserve_f32(u, 4);
    umbra_val32 const r = umbra_uniform_32(builder, (umbra_ptr32){0}, fi);
    umbra_val32 const g = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 4);
    umbra_val32 const b = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 8);
    umbra_val32 const a = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 12);
    return (umbra_color){r, g, b, a};
}

static umbra_val32 clamp01(struct umbra_builder *builder, umbra_val32 t) {
    return umbra_min_f32(builder, umbra_max_f32(builder, t, umbra_imm_f32(builder, 0.0f)),
                         umbra_imm_f32(builder, 1.0f));
}

static umbra_val32 lerp_f(struct umbra_builder *builder, umbra_val32 p, umbra_val32 q, umbra_val32 t) {
    return umbra_add_f32(builder, p,
                         umbra_mul_f32(builder, umbra_sub_f32(builder, q, p), t));
}

static umbra_val32 linear_t_(struct umbra_builder *builder, size_t fi, umbra_val32 x, umbra_val32 y) {
    umbra_val32 const a = umbra_uniform_32(builder, (umbra_ptr32){0}, fi);
    umbra_val32 const b = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 4);
    umbra_val32 const c = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 8);
    umbra_val32 const t = umbra_add_f32(builder,
                                      umbra_add_f32(builder, umbra_mul_f32(builder, a, x),
                                                    umbra_mul_f32(builder, b, y)),
                                      c);
    return clamp01(builder, t);
}

static umbra_val32 radial_t_(struct umbra_builder *builder, size_t fi, umbra_val32 x, umbra_val32 y) {
    umbra_val32 const cx = umbra_uniform_32(builder, (umbra_ptr32){0}, fi);
    umbra_val32 const cy = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 4);
    umbra_val32 const inv_r = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 8);
    umbra_val32 const dx = umbra_sub_f32(builder, x, cx);
    umbra_val32 const dy = umbra_sub_f32(builder, y, cy);
    umbra_val32 const d2 = umbra_add_f32(builder, umbra_mul_f32(builder, dx, dx),
                                       umbra_mul_f32(builder, dy, dy));
    return clamp01(builder, umbra_mul_f32(builder, umbra_sqrt_f32(builder, d2), inv_r));
}

static umbra_color lerp_2stop_(struct umbra_builder *builder, umbra_val32 t, size_t fi) {
    umbra_val32 const r0 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi);
    umbra_val32 const g0 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 4);
    umbra_val32 const b0 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 8);
    umbra_val32 const a0 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 12);
    umbra_val32 const r1 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 16);
    umbra_val32 const g1 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 20);
    umbra_val32 const b1 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 24);
    umbra_val32 const a1 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 28);
    return (umbra_color){
        lerp_f(builder, r0, r1, t),
        lerp_f(builder, g0, g1, t),
        lerp_f(builder, b0, b1, t),
        lerp_f(builder, a0, a1, t),
    };
}

static umbra_color sample_lut_(struct umbra_builder *builder, umbra_val32 t_f32, size_t fi, umbra_ptr32 lut) {
    // Planar LUT: [R0..R_{N-1}, G0..G_{N-1}, B0..B_{N-1}, A0..A_{N-1}]
    umbra_val32 const N_f  = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 12);
    umbra_val32 const N_m1 = umbra_sub_f32(builder, N_f, umbra_imm_f32(builder, 1.0f));
    umbra_val32 const N_m2 = umbra_sub_f32(builder, N_f, umbra_imm_f32(builder, 2.0f));
    umbra_val32 const t_sc = umbra_mul_f32(builder, t_f32, N_m1);
    umbra_val32 const frac_idx = umbra_min_f32(builder, t_sc, N_m2);

    umbra_val32 const N2_f = umbra_add_f32(builder, N_f, N_f);
    umbra_val32 const N3_f = umbra_add_f32(builder, N2_f, N_f);
    return (umbra_color){
        umbra_sample_32(builder, lut, frac_idx),
        umbra_sample_32(builder, lut, umbra_add_f32(builder, frac_idx, N_f)),
        umbra_sample_32(builder, lut, umbra_add_f32(builder, frac_idx, N2_f)),
        umbra_sample_32(builder, lut, umbra_add_f32(builder, frac_idx, N3_f)),
    };
}

umbra_color umbra_shader_linear_2(struct umbra_builder *builder, struct umbra_uniforms *u, umbra_val32 x, umbra_val32 y) {
    size_t const fi = umbra_uniforms_reserve_f32(u, 3);
    size_t const ci = umbra_uniforms_reserve_f32(u, 8);
    return lerp_2stop_(builder, linear_t_(builder, fi, x, y), ci);
}
umbra_color umbra_shader_radial_2(struct umbra_builder *builder, struct umbra_uniforms *u, umbra_val32 x, umbra_val32 y) {
    size_t const fi = umbra_uniforms_reserve_f32(u, 3);
    size_t const ci = umbra_uniforms_reserve_f32(u, 8);
    return lerp_2stop_(builder, radial_t_(builder, fi, x, y), ci);
}
umbra_color umbra_shader_linear_grad(struct umbra_builder *builder, struct umbra_uniforms *u, umbra_val32 x, umbra_val32 y) {
    size_t const fi = umbra_uniforms_reserve_f32(u, 4);
    size_t    const lut_off = umbra_uniforms_reserve_ptr(u);
    umbra_ptr32 const lut = umbra_deref_ptr32(builder, (umbra_ptr32){0}, lut_off);
    return sample_lut_(builder, linear_t_(builder, fi, x, y), fi, lut);
}
umbra_color umbra_shader_radial_grad(struct umbra_builder *builder, struct umbra_uniforms *u, umbra_val32 x, umbra_val32 y) {
    size_t const fi = umbra_uniforms_reserve_f32(u, 4);
    size_t    const lut_off = umbra_uniforms_reserve_ptr(u);
    umbra_ptr32 const lut = umbra_deref_ptr32(builder, (umbra_ptr32){0}, lut_off);
    return sample_lut_(builder, radial_t_(builder, fi, x, y), fi, lut);
}

umbra_color umbra_supersample(struct umbra_builder *builder, struct umbra_uniforms *u, umbra_val32 x, umbra_val32 y,
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
        umbra_val32 const sx = umbra_add_f32(builder, x, umbra_imm_f32(builder, jitter[s - 1][0]));
        umbra_val32 const sy = umbra_add_f32(builder, y, umbra_imm_f32(builder, jitter[s - 1][1]));
        umbra_color const c = inner(builder, &scratch, sx, sy);
        assume(scratch.size == after);
        sum.r = umbra_add_f32(builder, sum.r, c.r);
        sum.g = umbra_add_f32(builder, sum.g, c.g);
        sum.b = umbra_add_f32(builder, sum.b, c.b);
        sum.a = umbra_add_f32(builder, sum.a, c.a);
    }

    umbra_val32 const inv = umbra_imm_f32(builder, 1.0f / (float)n);
    return (umbra_color){
        umbra_mul_f32(builder, sum.r, inv),
        umbra_mul_f32(builder, sum.g, inv),
        umbra_mul_f32(builder, sum.b, inv),
        umbra_mul_f32(builder, sum.a, inv),
    };
}

umbra_color umbra_blend_src(struct umbra_builder *builder, umbra_color src, umbra_color dst) {
    (void)builder;
    (void)dst;
    return src;
}

umbra_color umbra_blend_srcover(struct umbra_builder *builder, umbra_color src, umbra_color dst) {
    umbra_val32 const one = umbra_imm_f32(builder, 1.0f);
    umbra_val32 const inv_a = umbra_sub_f32(builder, one, src.a);
    return (umbra_color){
        umbra_add_f32(builder, src.r, umbra_mul_f32(builder, dst.r, inv_a)),
        umbra_add_f32(builder, src.g, umbra_mul_f32(builder, dst.g, inv_a)),
        umbra_add_f32(builder, src.b, umbra_mul_f32(builder, dst.b, inv_a)),
        umbra_add_f32(builder, src.a, umbra_mul_f32(builder, dst.a, inv_a)),
    };
}

umbra_color umbra_blend_dstover(struct umbra_builder *builder, umbra_color src, umbra_color dst) {
    umbra_val32 const one = umbra_imm_f32(builder, 1.0f);
    umbra_val32 const inv_a = umbra_sub_f32(builder, one, dst.a);
    return (umbra_color){
        umbra_add_f32(builder, dst.r, umbra_mul_f32(builder, src.r, inv_a)),
        umbra_add_f32(builder, dst.g, umbra_mul_f32(builder, src.g, inv_a)),
        umbra_add_f32(builder, dst.b, umbra_mul_f32(builder, src.b, inv_a)),
        umbra_add_f32(builder, dst.a, umbra_mul_f32(builder, src.a, inv_a)),
    };
}

umbra_color umbra_blend_multiply(struct umbra_builder *builder, umbra_color src, umbra_color dst) {
    umbra_val32 const one = umbra_imm_f32(builder, 1.0f);
    umbra_val32 const inv_sa = umbra_sub_f32(builder, one, src.a);
    umbra_val32 const inv_da = umbra_sub_f32(builder, one, dst.a);
    umbra_val32 const r = umbra_add_f32(builder, umbra_mul_f32(builder, src.r, dst.r),
                                      umbra_add_f32(builder, umbra_mul_f32(builder, src.r, inv_da),
                                                    umbra_mul_f32(builder, dst.r, inv_sa)));
    umbra_val32 const g = umbra_add_f32(builder, umbra_mul_f32(builder, src.g, dst.g),
                                      umbra_add_f32(builder, umbra_mul_f32(builder, src.g, inv_da),
                                                    umbra_mul_f32(builder, dst.g, inv_sa)));
    umbra_val32 const b = umbra_add_f32(builder, umbra_mul_f32(builder, src.b, dst.b),
                                      umbra_add_f32(builder, umbra_mul_f32(builder, src.b, inv_da),
                                                    umbra_mul_f32(builder, dst.b, inv_sa)));
    umbra_val32 const a = umbra_add_f32(builder, umbra_mul_f32(builder, src.a, dst.a),
                                      umbra_add_f32(builder, umbra_mul_f32(builder, src.a, inv_da),
                                                    umbra_mul_f32(builder, dst.a, inv_sa)));
    return (umbra_color){r, g, b, a};
}

umbra_val32 umbra_coverage_rect(struct umbra_builder *builder, struct umbra_uniforms *u, umbra_val32 x, umbra_val32 y) {
    size_t const fi = umbra_uniforms_reserve_f32(u, 4);
    umbra_val32 const l = umbra_uniform_32(builder, (umbra_ptr32){0}, fi);
    umbra_val32 const t = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 4);
    umbra_val32 const r = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 8);
    umbra_val32 const b = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 12);
    umbra_val32 const inside = umbra_and_32(builder,
                                           umbra_and_32(builder, umbra_le_f32(builder, l, x),
                                                         umbra_lt_f32(builder, x, r)),
                                           umbra_and_32(builder, umbra_le_f32(builder, t, y),
                                                         umbra_lt_f32(builder, y, b)));
    umbra_val32 const one_f = umbra_imm_f32(builder, 1.0f);
    umbra_val32 const zero_f = umbra_imm_f32(builder, 0.0f);
    return umbra_sel_32(builder, inside, one_f, zero_f);
}

umbra_val32 umbra_coverage_bitmap(struct umbra_builder *builder, struct umbra_uniforms *u, umbra_val32 x, umbra_val32 y) {
    (void)x;
    (void)y;
    size_t    const bmp_off = umbra_uniforms_reserve_ptr(u);
    umbra_ptr16 const bmp = umbra_deref_ptr16(builder, (umbra_ptr32){0}, bmp_off);
    umbra_val32 const val = umbra_i32_from_s16(builder, umbra_load_16(builder, bmp));
    umbra_val32 const inv255 = umbra_imm_f32(builder, 1.0f / 255.0f);
    return umbra_mul_f32(builder, umbra_f32_from_i32(builder, val), inv255);
}

umbra_val32 umbra_coverage_sdf(struct umbra_builder *builder, struct umbra_uniforms *u, umbra_val32 x, umbra_val32 y) {
    (void)x;
    (void)y;
    size_t    const bmp_off = umbra_uniforms_reserve_ptr(u);
    umbra_ptr16 const bmp = umbra_deref_ptr16(builder, (umbra_ptr32){0}, bmp_off);
    umbra_val32 const raw = umbra_i32_from_s16(builder, umbra_load_16(builder, bmp));
    umbra_val32 const inv255 = umbra_imm_f32(builder, 1.0f / 255.0f);
    umbra_val32 const dist = umbra_mul_f32(builder, umbra_f32_from_i32(builder, raw), inv255);
    umbra_val32 const lo = umbra_imm_f32(builder, 0.4375f);
    umbra_val32 const scale = umbra_imm_f32(builder, 8.0f);
    umbra_val32 const shifted = umbra_sub_f32(builder, dist, lo);
    umbra_val32 const scaled = umbra_mul_f32(builder, shifted, scale);
    umbra_val32 const zero = umbra_imm_f32(builder, 0.0f);
    umbra_val32 const one = umbra_imm_f32(builder, 1.0f);
    return umbra_min_f32(builder, umbra_max_f32(builder, scaled, zero), one);
}

umbra_val32 umbra_coverage_wind(struct umbra_builder *builder, struct umbra_uniforms *u, umbra_val32 x, umbra_val32 y) {
    (void)x;
    (void)y;
    size_t    const off = umbra_uniforms_reserve_ptr(u);
    umbra_ptr32 const w = umbra_deref_ptr32(builder, (umbra_ptr32){0}, off);
    umbra_val32 const raw = umbra_load_32(builder, w);
    return umbra_min_f32(builder, umbra_abs_f32(builder, raw),
                         umbra_imm_f32(builder, 1.0f));
}

umbra_val32 umbra_coverage_bitmap_matrix(struct umbra_builder *builder, struct umbra_uniforms *u, umbra_val32 x, umbra_val32 y) {
    size_t const fi = umbra_uniforms_reserve_f32(u, 11);
    size_t    const bmp_off = umbra_uniforms_reserve_ptr(u);
    umbra_ptr16 const bmp = umbra_deref_ptr16(builder, (umbra_ptr32){0}, bmp_off);

    umbra_val32 const m0 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi);
    umbra_val32 const m1 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 4);
    umbra_val32 const m2 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 8);
    umbra_val32 const m3 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 12);
    umbra_val32 const m4 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 16);
    umbra_val32 const m5 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 20);
    umbra_val32 const m6 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 24);
    umbra_val32 const m7 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 28);
    umbra_val32 const m8 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 32);
    umbra_val32 const bw = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 36);
    umbra_val32 const bh = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 40);

    umbra_val32 const w = umbra_add_f32(builder,
                                      umbra_add_f32(builder, umbra_mul_f32(builder, m6, x),
                                                    umbra_mul_f32(builder, m7, y)),
                                      m8);
    umbra_val32 const xp =
        umbra_div_f32(builder,
                      umbra_add_f32(builder,
                                    umbra_add_f32(builder, umbra_mul_f32(builder, m0, x),
                                                  umbra_mul_f32(builder, m1, y)),
                                    m2),
                      w);
    umbra_val32 const yp =
        umbra_div_f32(builder,
                      umbra_add_f32(builder,
                                    umbra_add_f32(builder, umbra_mul_f32(builder, m3, x),
                                                  umbra_mul_f32(builder, m4, y)),
                                    m5),
                      w);

    umbra_val32 const zero_f = umbra_imm_f32(builder, 0.0f);
    umbra_val32 const in = umbra_and_32(builder,
                                       umbra_and_32(builder, umbra_le_f32(builder, zero_f, xp),
                                                     umbra_lt_f32(builder, xp, bw)),
                                       umbra_and_32(builder, umbra_le_f32(builder, zero_f, yp),
                                                     umbra_lt_f32(builder, yp, bh)));

    umbra_val32 const one_f = umbra_imm_f32(builder, 1.0f);
    umbra_val32 const xc = umbra_min_f32(builder, umbra_max_f32(builder, xp, zero_f),
                                       umbra_sub_f32(builder, bw, one_f));
    umbra_val32 const yc = umbra_min_f32(builder, umbra_max_f32(builder, yp, zero_f),
                                       umbra_sub_f32(builder, bh, one_f));
    umbra_val32 const xi = umbra_floor_i32(builder, xc);
    umbra_val32 const yi = umbra_floor_i32(builder, yc);
    umbra_val32 const bwi = umbra_floor_i32(builder, bw);
    umbra_val32 const idx = umbra_add_i32(builder, umbra_mul_i32(builder, yi, bwi), xi);

    umbra_val32 const val = umbra_i32_from_s16(builder, umbra_gather_16(builder, bmp, idx));
    umbra_val32 const inv255 = umbra_imm_f32(builder, 1.0f / 255.0f);
    umbra_val32 const cov = umbra_mul_f32(builder, umbra_f32_from_i32(builder, val), inv255);

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

