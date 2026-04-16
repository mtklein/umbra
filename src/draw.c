#include "../include/umbra_draw.h"
#include "assume.h"
#include "flat_ir.h"
#include "interval.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

static umbra_val32 sample(struct umbra_builder *b, umbra_ptr32 src, umbra_val32 ix) {
    umbra_val32 fl   = umbra_floor_i32(b, ix);
    umbra_val32 frac = umbra_sub_f32(b, ix, umbra_floor_f32(b, ix));
    umbra_val32 one  = umbra_imm_i32(b, 1);
    umbra_val32 lo   = umbra_gather_32(b, src, fl);
    umbra_val32 hi   = umbra_gather_32(b, src, umbra_add_i32(b, fl, one));
    umbra_val32 diff = umbra_sub_f32(b, hi, lo);
    return umbra_add_f32(b, lo, umbra_mul_f32(b, diff, frac));
}

static umbra_val32 pack_unorm(struct umbra_builder *b, umbra_val32 ch, umbra_val32 scale) {
    umbra_val32 const zero = umbra_imm_f32(b, 0.0f),
                       one = umbra_imm_f32(b, 1.0f);
    return umbra_round_i32(b, umbra_mul_f32(b,
        umbra_min_f32(b, umbra_max_f32(b, ch, zero), one), scale));
}

umbra_color umbra_load_8888(struct umbra_builder *b, umbra_ptr32 src) {
    umbra_val32 r, g, bl, a;
    umbra_load_8x4(b, src, &r, &g, &bl, &a);
    umbra_val32 const inv = umbra_imm_f32(b, 1.0f/255);
    return (umbra_color){
        umbra_mul_f32(b, umbra_f32_from_i32(b, r), inv),
        umbra_mul_f32(b, umbra_f32_from_i32(b, g), inv),
        umbra_mul_f32(b, umbra_f32_from_i32(b, bl), inv),
        umbra_mul_f32(b, umbra_f32_from_i32(b, a), inv),
    };
}
void umbra_store_8888(struct umbra_builder *b, umbra_ptr32 dst, umbra_color c) {
    umbra_val32 s = umbra_imm_f32(b, 255.0f);
    umbra_store_8x4(b, dst, pack_unorm(b, c.r, s), pack_unorm(b, c.g, s),
                            pack_unorm(b, c.b, s), pack_unorm(b, c.a, s));
}

umbra_color umbra_load_565(struct umbra_builder *b, umbra_ptr16 src) {
    umbra_val32 const px = umbra_i32_from_u16(b, umbra_load_16(b, src));
    umbra_val32 const r5 = umbra_shr_u32(b, px, umbra_imm_i32(b, 11));
    umbra_val32 const g6 = umbra_and_32(b, umbra_shr_u32(b, px, umbra_imm_i32(b, 5)),
                                         umbra_imm_i32(b, 0x3F));
    umbra_val32 const b5 = umbra_and_32(b, px, umbra_imm_i32(b, 0x1F));
    return (umbra_color){
        umbra_mul_f32(b, umbra_f32_from_i32(b, r5), umbra_imm_f32(b, 1.0f/31)),
        umbra_mul_f32(b, umbra_f32_from_i32(b, g6), umbra_imm_f32(b, 1.0f/63)),
        umbra_mul_f32(b, umbra_f32_from_i32(b, b5), umbra_imm_f32(b, 1.0f/31)),
        umbra_imm_f32(b, 1.0f),
    };
}
void umbra_store_565(struct umbra_builder *b, umbra_ptr16 dst, umbra_color c) {
    umbra_val32 px = pack_unorm(b, c.b, umbra_imm_f32(b, 31.0f));
    px = umbra_or_32(b, px, umbra_shl_i32(b, pack_unorm(b, c.g, umbra_imm_f32(b, 63.0f)),
                                            umbra_imm_i32(b, 5)));
    px = umbra_or_32(b, px, umbra_shl_i32(b, pack_unorm(b, c.r, umbra_imm_f32(b, 31.0f)),
                                            umbra_imm_i32(b, 11)));
    umbra_store_16(b, dst, umbra_i16_from_i32(b, px));
}

umbra_color umbra_load_1010102(struct umbra_builder *b, umbra_ptr32 src) {
    umbra_val32 const px   = umbra_load_32(b, src);
    umbra_val32 const mask = umbra_imm_i32(b, 0x3FF);
    umbra_val32 const ri   = umbra_and_32(b, px, mask);
    umbra_val32 const gi   = umbra_and_32(b, umbra_shr_u32(b, px, umbra_imm_i32(b, 10)), mask);
    umbra_val32 const bi   = umbra_and_32(b, umbra_shr_u32(b, px, umbra_imm_i32(b, 20)), mask);
    umbra_val32 const ai   = umbra_shr_u32(b, px, umbra_imm_i32(b, 30));
    return (umbra_color){
        umbra_mul_f32(b, umbra_f32_from_i32(b, ri), umbra_imm_f32(b, 1.0f/1023)),
        umbra_mul_f32(b, umbra_f32_from_i32(b, gi), umbra_imm_f32(b, 1.0f/1023)),
        umbra_mul_f32(b, umbra_f32_from_i32(b, bi), umbra_imm_f32(b, 1.0f/1023)),
        umbra_mul_f32(b, umbra_f32_from_i32(b, ai), umbra_imm_f32(b, 1.0f/3)),
    };
}
void umbra_store_1010102(struct umbra_builder *b, umbra_ptr32 dst, umbra_color c) {
    umbra_val32 s10 = umbra_imm_f32(b, 1023.0f);
    umbra_val32 px = pack_unorm(b, c.r, s10);
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

static umbra_color load_8888_(struct umbra_builder *b, int p) {
    return umbra_load_8888(b, (umbra_ptr32){.ix=p});
}
static umbra_color load_565_(struct umbra_builder *b, int p) {
    return umbra_load_565(b, (umbra_ptr16){.ix=p});
}
static umbra_color load_1010102_(struct umbra_builder *b, int p) {
    return umbra_load_1010102(b, (umbra_ptr32){.ix=p});
}
static umbra_color load_fp16_(struct umbra_builder *b, int p) {
    return umbra_load_fp16(b, (umbra_ptr64){.ix=p});
}
static umbra_color load_fp16p_(struct umbra_builder *b, int p) {
    return umbra_load_fp16_planar(b, (umbra_ptr16){.ix=p});
}
static void store_8888_(struct umbra_builder *b, int p, umbra_color c) {
    umbra_store_8888(b, (umbra_ptr32){.ix=p}, c);
}
static void store_565_(struct umbra_builder *b, int p, umbra_color c) {
    umbra_store_565(b, (umbra_ptr16){.ix=p}, c);
}
static void store_1010102_(struct umbra_builder *b, int p, umbra_color c) {
    umbra_store_1010102(b, (umbra_ptr32){.ix=p}, c);
}
static void store_fp16_(struct umbra_builder *b, int p, umbra_color c) {
    umbra_store_fp16(b, (umbra_ptr64){.ix=p}, c);
}
static void store_fp16p_(struct umbra_builder *b, int p, umbra_color c) {
    umbra_store_fp16_planar(b, (umbra_ptr16){.ix=p}, c);
}

struct umbra_fmt const umbra_fmt_8888 = {
    .name="8888", .bpp=4, .planes=1, .load=load_8888_, .store=store_8888_,
};
struct umbra_fmt const umbra_fmt_565 = {
    .name="565", .bpp=2, .planes=1, .load=load_565_, .store=store_565_,
};
struct umbra_fmt const umbra_fmt_1010102 = {
    .name="1010102", .bpp=4, .planes=1, .load=load_1010102_, .store=store_1010102_,
};
struct umbra_fmt const umbra_fmt_fp16 = {
    .name="fp16", .bpp=8, .planes=1, .load=load_fp16_, .store=store_fp16_,
};
struct umbra_fmt const umbra_fmt_fp16_planar = {
    .name="fp16_planar", .bpp=2, .planes=4, .load=load_fp16p_, .store=store_fp16p_,
};

struct umbra_builder* umbra_draw_builder(struct umbra_shader *shader,
                                         struct umbra_coverage *coverage,
                                         umbra_blend_fn blend,
                                         struct umbra_fmt fmt,
                                         struct umbra_draw_layout *layout) {
    struct umbra_builder  *builder = umbra_builder();
    umbra_val32 const x = umbra_x(builder);
    umbra_val32 const y = umbra_y(builder);
    umbra_val32 const xf = umbra_f32_from_i32(builder, x);
    umbra_val32 const yf = umbra_f32_from_i32(builder, y);

    struct umbra_uniforms_layout uni = {0};

    umbra_val32 cov = {0};
    if (coverage) {
        cov = coverage->build(coverage, builder, &uni, xf, yf);
    }

    umbra_color src = {
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
    };
    if (shader) {
        src = shader->build(shader, builder, &uni, xf, yf);
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
        layout->uniforms = umbra_uniforms_alloc(&uni);
    }

    return builder;
}

void umbra_draw_fill(struct umbra_draw_layout const *layout,
                     struct umbra_shader const *shader,
                     struct umbra_coverage const *coverage) {
    if (shader)   { shader->fill(shader, layout->uniforms); }
    if (coverage) { coverage->fill(coverage, layout->uniforms); }
}

static umbra_val32 sdf_as_coverage_build_(struct umbra_coverage *s, struct umbra_builder *b,
                                          struct umbra_uniforms_layout *u,
                                          umbra_val32 x, umbra_val32 y) {
    struct umbra_sdf_coverage *self = (struct umbra_sdf_coverage *)s;
    umbra_val32 const f = self->sdf->build(self->sdf, b, u, x, y);
    return umbra_min_f32(b, umbra_imm_f32(b, 1.0f),
                         umbra_max_f32(b, umbra_imm_f32(b, 0.0f),
                                       umbra_sub_f32(b, umbra_imm_f32(b, 0.0f), f)));
}
static void sdf_as_coverage_fill_(struct umbra_coverage const *s, void *uniforms) {
    struct umbra_sdf_coverage const *self =
        (struct umbra_sdf_coverage const *)s;
    self->sdf->fill(self->sdf, uniforms);
}
struct umbra_sdf_coverage umbra_sdf_coverage(struct umbra_sdf *sdf) {
    return (struct umbra_sdf_coverage){
        .base = {.build = sdf_as_coverage_build_, .fill = sdf_as_coverage_fill_},
        .sdf  = sdf,
    };
}

static struct interval_program* build_sdf_interval(struct umbra_sdf *sdf) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 const x = umbra_x(b),
                      y = umbra_y(b);

    struct umbra_uniforms_layout uni = {0};
    umbra_val32 const f = sdf->build(sdf, b, &uni, x, y);
    umbra_store_32(b, (umbra_ptr32){.ix = 1}, f);

    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    struct interval_program *p = interval_program(ir);
    umbra_flat_ir_free(ir);

    return p;
}

struct umbra_quadtree {
    struct umbra_program    *program;
    struct interval_program *interval;
};

struct umbra_quadtree* umbra_quadtree(struct umbra_backend *be,
                                      struct umbra_sdf *sdf,
                                      struct umbra_shader *shader,
                                      umbra_blend_fn blend,
                                      struct umbra_fmt fmt,
                                      struct umbra_draw_layout *layout) {
    struct interval_program *ip = build_sdf_interval(sdf);
    assume(ip);

    struct umbra_sdf_coverage adapter = umbra_sdf_coverage(sdf);
    struct umbra_builder *b = umbra_draw_builder(shader, &adapter.base, blend, fmt, layout);
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    struct umbra_program *prog = be->compile(be, ir);
    umbra_flat_ir_free(ir);

    struct umbra_quadtree *qt = calloc(1, sizeof *qt);
    qt->program  = prog;
    qt->interval = ip;
    return qt;
}

// TODO: query per-backend dispatch_granularity instead of this one global
// compromise.  CPU-optimal is ~16-32, GPU-optimal is ~512-1024.
enum { QUEUE_MIN_TILE = 512 };

static void quadtree_recurse(struct umbra_quadtree const *qt,
                        int l, int t, int r, int b,
                        struct umbra_buf buf[], float const *uniform) {
    assume(l < r && t < b);

    interval const sdf = interval_program_run(qt->interval,
                                              (interval){(float)l, (float)(r - 1)},
                                              (interval){(float)t, (float)(b - 1)},
                                              uniform);
    if (sdf.lo < 0) {
        if (sdf.hi <= -1 || (r-l <= QUEUE_MIN_TILE && b-t <= QUEUE_MIN_TILE)) {
            qt->program->queue(qt->program, l, t, r, b, buf);
            return;
        }
        int const mx = (l + r) / 2,
                  my = (t + b) / 2;
        quadtree_recurse(qt, l,  t,  mx, my, buf, uniform);
        quadtree_recurse(qt, mx, t,  r,  my, buf, uniform);
        quadtree_recurse(qt, l,  my, mx, b,  buf, uniform);
        quadtree_recurse(qt, mx, my, r,  b,  buf, uniform);
    }
}

void umbra_quadtree_queue(struct umbra_quadtree const *qt,
                          int l, int t, int r, int b, struct umbra_buf buf[]) {
    quadtree_recurse(qt, l, t, r, b, buf, buf[0].ptr);
}

void umbra_quadtree_fill(struct umbra_draw_layout const *layout,
                         struct umbra_sdf const *sdf,
                         struct umbra_shader const *shader) {
    if (sdf)    { sdf->fill(sdf, layout->uniforms); }
    if (shader) { shader->fill(shader, layout->uniforms); }
}

void umbra_quadtree_free(struct umbra_quadtree *qt) {
    if (qt) {
        qt->program->free(qt->program);
        interval_program_free(qt->interval);
        free(qt);
    }
}

static umbra_val32 clamp01(struct umbra_builder *builder, umbra_val32 t) {
    return umbra_min_f32(builder, umbra_max_f32(builder, t, umbra_imm_f32(builder, 0.0f)),
                         umbra_imm_f32(builder, 1.0f));
}

static umbra_val32 lerp_f(struct umbra_builder *builder, umbra_val32 p, umbra_val32 q,
                           umbra_val32 t) {
    return umbra_add_f32(builder, p,
                         umbra_mul_f32(builder, umbra_sub_f32(builder, q, p), t));
}

static umbra_val32 linear_t_(struct umbra_builder *builder, int fi,
                              umbra_val32 x, umbra_val32 y) {
    umbra_val32 const a = umbra_uniform_32(builder, (umbra_ptr32){0}, fi);
    umbra_val32 const b = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 1);
    umbra_val32 const c = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 2);
    umbra_val32 const t = umbra_add_f32(builder,
                                      umbra_add_f32(builder, umbra_mul_f32(builder, a, x),
                                                    umbra_mul_f32(builder, b, y)),
                                      c);
    return clamp01(builder, t);
}

static umbra_val32 radial_t_(struct umbra_builder *builder, int fi,
                              umbra_val32 x, umbra_val32 y) {
    umbra_val32 const cx = umbra_uniform_32(builder, (umbra_ptr32){0}, fi);
    umbra_val32 const cy = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 1);
    umbra_val32 const inv_r = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 2);
    umbra_val32 const dx = umbra_sub_f32(builder, x, cx);
    umbra_val32 const dy = umbra_sub_f32(builder, y, cy);
    umbra_val32 const d2 = umbra_add_f32(builder, umbra_mul_f32(builder, dx, dx),
                                       umbra_mul_f32(builder, dy, dy));
    return clamp01(builder, umbra_mul_f32(builder, umbra_sqrt_f32(builder, d2), inv_r));
}

static umbra_color lerp_2stop_(struct umbra_builder *builder, umbra_val32 t, int fi) {
    umbra_val32 const r0 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi);
    umbra_val32 const g0 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 1);
    umbra_val32 const b0 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 2);
    umbra_val32 const a0 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 3);
    umbra_val32 const r1 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 4);
    umbra_val32 const g1 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 5);
    umbra_val32 const b1 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 6);
    umbra_val32 const a1 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 7);
    return (umbra_color){
        lerp_f(builder, r0, r1, t),
        lerp_f(builder, g0, g1, t),
        lerp_f(builder, b0, b1, t),
        lerp_f(builder, a0, a1, t),
    };
}

static umbra_color sample_lut_(struct umbra_builder *builder, umbra_val32 t_f32, int fi,
                                umbra_ptr32 lut) {
    umbra_val32 const N_f  = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 3);
    umbra_val32 const N_m1 = umbra_sub_f32(builder, N_f, umbra_imm_f32(builder, 1.0f));
    umbra_val32 const N_m2 = umbra_sub_f32(builder, N_f, umbra_imm_f32(builder, 2.0f));
    umbra_val32 const t_sc = umbra_mul_f32(builder, t_f32, N_m1);
    umbra_val32 const frac_idx = umbra_min_f32(builder, t_sc, N_m2);

    umbra_val32 const N2_f = umbra_add_f32(builder, N_f, N_f);
    umbra_val32 const N3_f = umbra_add_f32(builder, N2_f, N_f);
    return (umbra_color){
        sample(builder, lut, frac_idx),
        sample(builder, lut, umbra_add_f32(builder, frac_idx, N_f)),
        sample(builder, lut, umbra_add_f32(builder, frac_idx, N2_f)),
        sample(builder, lut, umbra_add_f32(builder, frac_idx, N3_f)),
    };
}

static umbra_color walk_stops_(struct umbra_builder *b, umbra_val32 t,
                               int fi, umbra_ptr32 colors, umbra_ptr32 pos) {
    umbra_val32 const n_i = umbra_i32_from_f32(b,
                                umbra_uniform_32(b, (umbra_ptr32){0}, fi + 3));
    umbra_val32 const n_segs = umbra_sub_i32(b, n_i, umbra_imm_i32(b, 1));
    umbra_val32 const n2 = umbra_add_i32(b, n_i, n_i);
    umbra_val32 const n3 = umbra_add_i32(b, n2, n_i);

    umbra_var vr = umbra_var_alloc(b),
              vg = umbra_var_alloc(b),
              vb = umbra_var_alloc(b),
              va = umbra_var_alloc(b);

    umbra_val32 i = umbra_loop(b, n_segs); {
        umbra_val32 i1 = umbra_add_i32(b, i, umbra_imm_i32(b, 1));

        umbra_val32 p0 = umbra_gather_32(b, pos, i);
        umbra_val32 p1 = umbra_gather_32(b, pos, i1);
        umbra_val32 in_seg = umbra_and_32(b, umbra_le_f32(b, p0, t),
                                             umbra_le_f32(b, t, p1));

        umbra_if(b, in_seg); {
            umbra_val32 frac = umbra_div_f32(b, umbra_sub_f32(b, t, p0),
                                                umbra_sub_f32(b, p1, p0));

            umbra_val32 r0 = umbra_gather_32(b, colors, i);
            umbra_val32 r1 = umbra_gather_32(b, colors, i1);
            umbra_val32 g0 = umbra_gather_32(b, colors, umbra_add_i32(b, i,  n_i));
            umbra_val32 g1 = umbra_gather_32(b, colors, umbra_add_i32(b, i1, n_i));
            umbra_val32 b0 = umbra_gather_32(b, colors, umbra_add_i32(b, i,  n2));
            umbra_val32 b1 = umbra_gather_32(b, colors, umbra_add_i32(b, i1, n2));
            umbra_val32 a0 = umbra_gather_32(b, colors, umbra_add_i32(b, i,  n3));
            umbra_val32 a1 = umbra_gather_32(b, colors, umbra_add_i32(b, i1, n3));

            umbra_store_var(b, vr, lerp_f(b, r0, r1, frac));
            umbra_store_var(b, vg, lerp_f(b, g0, g1, frac));
            umbra_store_var(b, vb, lerp_f(b, b0, b1, frac));
            umbra_store_var(b, va, lerp_f(b, a0, a1, frac));
        } umbra_endif(b);
    } umbra_loop_end(b);

    return (umbra_color){
        umbra_load_var(b, vr),
        umbra_load_var(b, vg),
        umbra_load_var(b, vb),
        umbra_load_var(b, va),
    };
}

static umbra_color solid_build_(struct umbra_shader *s, struct umbra_builder *builder,
                                struct umbra_uniforms_layout *u,
                                umbra_val32 x, umbra_val32 y) {
    struct umbra_shader_solid *self = (struct umbra_shader_solid *)s;
    (void)x;
    (void)y;
    self->off_ = umbra_uniforms_reserve_f32(u, 4);
    umbra_val32 const r = umbra_uniform_32(builder, (umbra_ptr32){0}, self->off_);
    umbra_val32 const g = umbra_uniform_32(builder, (umbra_ptr32){0}, self->off_ + 1);
    umbra_val32 const b = umbra_uniform_32(builder, (umbra_ptr32){0}, self->off_ + 2);
    umbra_val32 const a = umbra_uniform_32(builder, (umbra_ptr32){0}, self->off_ + 3);
    return (umbra_color){r, g, b, a};
}
static void solid_fill_(struct umbra_shader const *s, void *uniforms) {
    struct umbra_shader_solid const *self = (struct umbra_shader_solid const *)s;
    umbra_uniforms_fill_f32(uniforms, self->off_, self->color, 4);
}
struct umbra_shader_solid umbra_shader_solid(float const color[4]) {
    struct umbra_shader_solid s = {
        .base = {.build = solid_build_, .fill = solid_fill_},
    };
    __builtin_memcpy(s.color, color, 16);
    return s;
}

static umbra_color linear_2_build_(struct umbra_shader *s, struct umbra_builder *builder,
                                   struct umbra_uniforms_layout *u,
                                   umbra_val32 x, umbra_val32 y) {
    struct umbra_shader_linear_2 *self = (struct umbra_shader_linear_2 *)s;
    self->fi_ = umbra_uniforms_reserve_f32(u, 3);
    self->ci_ = umbra_uniforms_reserve_f32(u, 8);
    return lerp_2stop_(builder, linear_t_(builder, self->fi_, x, y), self->ci_);
}
static void linear_2_fill_(struct umbra_shader const *s, void *uniforms) {
    struct umbra_shader_linear_2 const *self = (struct umbra_shader_linear_2 const *)s;
    umbra_uniforms_fill_f32(uniforms, self->fi_, self->grad, 3);
    umbra_uniforms_fill_f32(uniforms, self->ci_, self->color, 8);
}
struct umbra_shader_linear_2 umbra_shader_linear_2(float const grad[3], float const color[8]) {
    struct umbra_shader_linear_2 s = {
        .base = {.build = linear_2_build_, .fill = linear_2_fill_},
    };
    __builtin_memcpy(s.grad, grad, 12);
    __builtin_memcpy(s.color, color, 32);
    return s;
}

static umbra_color radial_2_build_(struct umbra_shader *s, struct umbra_builder *builder,
                                   struct umbra_uniforms_layout *u,
                                   umbra_val32 x, umbra_val32 y) {
    struct umbra_shader_radial_2 *self = (struct umbra_shader_radial_2 *)s;
    self->fi_ = umbra_uniforms_reserve_f32(u, 3);
    self->ci_ = umbra_uniforms_reserve_f32(u, 8);
    return lerp_2stop_(builder, radial_t_(builder, self->fi_, x, y), self->ci_);
}
static void radial_2_fill_(struct umbra_shader const *s, void *uniforms) {
    struct umbra_shader_radial_2 const *self = (struct umbra_shader_radial_2 const *)s;
    umbra_uniforms_fill_f32(uniforms, self->fi_, self->grad, 3);
    umbra_uniforms_fill_f32(uniforms, self->ci_, self->color, 8);
}
struct umbra_shader_radial_2 umbra_shader_radial_2(float const grad[3], float const color[8]) {
    struct umbra_shader_radial_2 s = {
        .base = {.build = radial_2_build_, .fill = radial_2_fill_},
    };
    __builtin_memcpy(s.grad, grad, 12);
    __builtin_memcpy(s.color, color, 32);
    return s;
}

static umbra_color linear_grad_build_(struct umbra_shader *s, struct umbra_builder *builder,
                                      struct umbra_uniforms_layout *u,
                                      umbra_val32 x, umbra_val32 y) {
    struct umbra_shader_linear_grad *self = (struct umbra_shader_linear_grad *)s;
    self->fi_ = umbra_uniforms_reserve_f32(u, 4);
    self->lut_off_ = umbra_uniforms_reserve_ptr(u);
    umbra_ptr32 const lut = umbra_deref_ptr32(builder, (umbra_ptr32){0}, self->lut_off_);
    return sample_lut_(builder, linear_t_(builder, self->fi_, x, y), self->fi_, lut);
}
static void linear_grad_fill_(struct umbra_shader const *s, void *uniforms) {
    struct umbra_shader_linear_grad const *self = (struct umbra_shader_linear_grad const *)s;
    umbra_uniforms_fill_f32(uniforms, self->fi_, self->grad, 4);
    umbra_uniforms_fill_ptr(uniforms, self->lut_off_, self->lut);
}
struct umbra_shader_linear_grad umbra_shader_linear_grad(float const grad[4],
                                                         struct umbra_buf lut) {
    struct umbra_shader_linear_grad s = {
        .base = {.build = linear_grad_build_, .fill = linear_grad_fill_},
        .lut = lut,
    };
    __builtin_memcpy(s.grad, grad, 16);
    return s;
}

static umbra_color radial_grad_build_(struct umbra_shader *s, struct umbra_builder *builder,
                                      struct umbra_uniforms_layout *u,
                                      umbra_val32 x, umbra_val32 y) {
    struct umbra_shader_radial_grad *self = (struct umbra_shader_radial_grad *)s;
    self->fi_ = umbra_uniforms_reserve_f32(u, 4);
    self->lut_off_ = umbra_uniforms_reserve_ptr(u);
    umbra_ptr32 const lut = umbra_deref_ptr32(builder, (umbra_ptr32){0}, self->lut_off_);
    return sample_lut_(builder, radial_t_(builder, self->fi_, x, y), self->fi_, lut);
}
static void radial_grad_fill_(struct umbra_shader const *s, void *uniforms) {
    struct umbra_shader_radial_grad const *self = (struct umbra_shader_radial_grad const *)s;
    umbra_uniforms_fill_f32(uniforms, self->fi_, self->grad, 4);
    umbra_uniforms_fill_ptr(uniforms, self->lut_off_, self->lut);
}
struct umbra_shader_radial_grad umbra_shader_radial_grad(float const grad[4],
                                                         struct umbra_buf lut) {
    struct umbra_shader_radial_grad s = {
        .base = {.build = radial_grad_build_, .fill = radial_grad_fill_},
        .lut = lut,
    };
    __builtin_memcpy(s.grad, grad, 16);
    return s;
}

static umbra_color linear_stops_build_(struct umbra_shader *s, struct umbra_builder *builder,
                                       struct umbra_uniforms_layout *u,
                                       umbra_val32 x, umbra_val32 y) {
    struct umbra_shader_linear_stops *self = (struct umbra_shader_linear_stops *)s;
    self->fi_ = umbra_uniforms_reserve_f32(u, 4);
    self->colors_off_ = umbra_uniforms_reserve_ptr(u);
    self->pos_off_ = umbra_uniforms_reserve_ptr(u);
    umbra_ptr32 const colors = umbra_deref_ptr32(builder, (umbra_ptr32){0}, self->colors_off_);
    umbra_ptr32 const pos    = umbra_deref_ptr32(builder, (umbra_ptr32){0}, self->pos_off_);
    return walk_stops_(builder, linear_t_(builder, self->fi_, x, y), self->fi_, colors, pos);
}
static void linear_stops_fill_(struct umbra_shader const *s, void *uniforms) {
    struct umbra_shader_linear_stops const *self = (struct umbra_shader_linear_stops const *)s;
    umbra_uniforms_fill_f32(uniforms, self->fi_, self->grad, 4);
    umbra_uniforms_fill_ptr(uniforms, self->colors_off_, self->colors);
    umbra_uniforms_fill_ptr(uniforms, self->pos_off_, self->pos);
}
struct umbra_shader_linear_stops umbra_shader_linear_stops(float const grad[4],
                                                           struct umbra_buf colors,
                                                           struct umbra_buf pos) {
    struct umbra_shader_linear_stops s = {
        .base = {.build = linear_stops_build_, .fill = linear_stops_fill_},
        .colors = colors,
        .pos = pos,
    };
    __builtin_memcpy(s.grad, grad, 16);
    return s;
}

static umbra_color radial_stops_build_(struct umbra_shader *s, struct umbra_builder *builder,
                                       struct umbra_uniforms_layout *u,
                                       umbra_val32 x, umbra_val32 y) {
    struct umbra_shader_radial_stops *self = (struct umbra_shader_radial_stops *)s;
    self->fi_ = umbra_uniforms_reserve_f32(u, 4);
    self->colors_off_ = umbra_uniforms_reserve_ptr(u);
    self->pos_off_ = umbra_uniforms_reserve_ptr(u);
    umbra_ptr32 const colors = umbra_deref_ptr32(builder, (umbra_ptr32){0}, self->colors_off_);
    umbra_ptr32 const pos    = umbra_deref_ptr32(builder, (umbra_ptr32){0}, self->pos_off_);
    return walk_stops_(builder, radial_t_(builder, self->fi_, x, y), self->fi_, colors, pos);
}
static void radial_stops_fill_(struct umbra_shader const *s, void *uniforms) {
    struct umbra_shader_radial_stops const *self = (struct umbra_shader_radial_stops const *)s;
    umbra_uniforms_fill_f32(uniforms, self->fi_, self->grad, 4);
    umbra_uniforms_fill_ptr(uniforms, self->colors_off_, self->colors);
    umbra_uniforms_fill_ptr(uniforms, self->pos_off_, self->pos);
}
struct umbra_shader_radial_stops umbra_shader_radial_stops(float const grad[4],
                                                           struct umbra_buf colors,
                                                           struct umbra_buf pos) {
    struct umbra_shader_radial_stops s = {
        .base = {.build = radial_stops_build_, .fill = radial_stops_fill_},
        .colors = colors,
        .pos = pos,
    };
    __builtin_memcpy(s.grad, grad, 16);
    return s;
}

static umbra_color supersample_build_(struct umbra_shader *s, struct umbra_builder *builder,
                                      struct umbra_uniforms_layout *u,
                                      umbra_val32 x, umbra_val32 y) {
    struct umbra_shader_supersample *self = (struct umbra_shader_supersample *)s;
    static float const jitter[][2] = {
        {-0.375f, -0.125f}, {0.125f, -0.375f}, {0.375f, 0.125f}, {-0.125f, 0.375f},
        {-0.250f, 0.375f},  {0.250f, -0.250f}, {0.375f, 0.250f}, {-0.375f, -0.250f},
    };
    int n = self->n;
    if (n < 1) { n = 1; }
    if (n > 8) { n = 8; }

    int         const saved = u->slots;
    umbra_color sum = self->inner->build(self->inner, builder, u, x, y);
    int         const after = u->slots;

    for (int i = 1; i < n; i++) {
        struct umbra_uniforms_layout scratch = {.slots = saved};
        umbra_val32 const sx = umbra_add_f32(builder, x,
                                              umbra_imm_f32(builder, jitter[i - 1][0]));
        umbra_val32 const sy = umbra_add_f32(builder, y,
                                              umbra_imm_f32(builder, jitter[i - 1][1]));
        umbra_color const c = self->inner->build(self->inner, builder, &scratch, sx, sy);
        assume(scratch.slots == after);
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
static void supersample_fill_(struct umbra_shader const *s, void *uniforms) {
    struct umbra_shader_supersample const *self = (struct umbra_shader_supersample const *)s;
    self->inner->fill(self->inner, uniforms);
}
struct umbra_shader_supersample umbra_shader_supersample(struct umbra_shader *inner, int n) {
    return (struct umbra_shader_supersample){
        .base = {.build = supersample_build_, .fill = supersample_fill_},
        .inner = inner,
        .n = n,
    };
}

static umbra_val32 rect_build_(struct umbra_coverage *s, struct umbra_builder *builder,
                               struct umbra_uniforms_layout *u,
                               umbra_val32 x, umbra_val32 y) {
    struct umbra_coverage_rect *self = (struct umbra_coverage_rect *)s;
    self->off_ = umbra_uniforms_reserve_f32(u, 4);
    umbra_val32 const l = umbra_uniform_32(builder, (umbra_ptr32){0}, self->off_);
    umbra_val32 const t = umbra_uniform_32(builder, (umbra_ptr32){0}, self->off_ + 1);
    umbra_val32 const r = umbra_uniform_32(builder, (umbra_ptr32){0}, self->off_ + 2);
    umbra_val32 const b = umbra_uniform_32(builder, (umbra_ptr32){0}, self->off_ + 3);
    umbra_val32 const inside = umbra_and_32(builder,
                                           umbra_and_32(builder, umbra_le_f32(builder, l, x),
                                                         umbra_lt_f32(builder, x, r)),
                                           umbra_and_32(builder, umbra_le_f32(builder, t, y),
                                                         umbra_lt_f32(builder, y, b)));
    umbra_val32 const one_f = umbra_imm_f32(builder, 1.0f);
    umbra_val32 const zero_f = umbra_imm_f32(builder, 0.0f);
    return umbra_sel_32(builder, inside, one_f, zero_f);
}
static void rect_fill_(struct umbra_coverage const *s, void *uniforms) {
    struct umbra_coverage_rect const *self = (struct umbra_coverage_rect const *)s;
    umbra_uniforms_fill_f32(uniforms, self->off_, self->rect, 4);
}
struct umbra_coverage_rect umbra_coverage_rect(float const rect[4]) {
    struct umbra_coverage_rect c = {
        .base = {.build = rect_build_, .fill = rect_fill_},
    };
    __builtin_memcpy(c.rect, rect, 16);
    return c;
}

static umbra_val32 sdf_rect_build_(struct umbra_sdf *s, struct umbra_builder *b,
                                   struct umbra_uniforms_layout *u,
                                   umbra_val32 x, umbra_val32 y) {
    struct umbra_sdf_rect *self = (struct umbra_sdf_rect *)s;
    self->off_ = umbra_uniforms_reserve_f32(u, 4);
    umbra_val32 const l = umbra_uniform_32(b, (umbra_ptr32){0}, self->off_);
    umbra_val32 const t = umbra_uniform_32(b, (umbra_ptr32){0}, self->off_ + 1);
    umbra_val32 const r = umbra_uniform_32(b, (umbra_ptr32){0}, self->off_ + 2);
    umbra_val32 const bo = umbra_uniform_32(b, (umbra_ptr32){0}, self->off_ + 3);
    return umbra_max_f32(b, umbra_max_f32(b, umbra_sub_f32(b, l, x),
                                             umbra_sub_f32(b, x, r)),
                            umbra_max_f32(b, umbra_sub_f32(b, t, y),
                                             umbra_sub_f32(b, y, bo)));
}
static void sdf_rect_fill_(struct umbra_sdf const *s, void *uniforms) {
    struct umbra_sdf_rect const *self = (struct umbra_sdf_rect const *)s;
    umbra_uniforms_fill_f32(uniforms, self->off_, self->rect, 4);
}
struct umbra_sdf_rect umbra_sdf_rect(float const rect[4]) {
    struct umbra_sdf_rect c = {
        .base = {.build = sdf_rect_build_, .fill = sdf_rect_fill_},
    };
    __builtin_memcpy(c.rect, rect, 16);
    return c;
}

static umbra_val32 bitmap_build_(struct umbra_coverage *s, struct umbra_builder *builder,
                                 struct umbra_uniforms_layout *u,
                                 umbra_val32 x, umbra_val32 y) {
    struct umbra_coverage_bitmap *self = (struct umbra_coverage_bitmap *)s;
    (void)x;
    (void)y;
    self->bmp_off_ = umbra_uniforms_reserve_ptr(u);
    umbra_ptr16 const bmp = umbra_deref_ptr16(builder, (umbra_ptr32){0}, self->bmp_off_);
    umbra_val32 const val = umbra_i32_from_s16(builder, umbra_load_16(builder, bmp));
    umbra_val32 const inv255 = umbra_imm_f32(builder, 1.0f / 255.0f);
    return umbra_mul_f32(builder, umbra_f32_from_i32(builder, val), inv255);
}
static void bitmap_fill_(struct umbra_coverage const *s, void *uniforms) {
    struct umbra_coverage_bitmap const *self = (struct umbra_coverage_bitmap const *)s;
    umbra_uniforms_fill_ptr(uniforms, self->bmp_off_, self->bmp);
}
struct umbra_coverage_bitmap umbra_coverage_bitmap(struct umbra_buf bmp) {
    return (struct umbra_coverage_bitmap){
        .base = {.build = bitmap_build_, .fill = bitmap_fill_},
        .bmp = bmp,
    };
}

static umbra_val32 sdf_build_(struct umbra_coverage *s, struct umbra_builder *builder,
                              struct umbra_uniforms_layout *u,
                              umbra_val32 x, umbra_val32 y) {
    struct umbra_coverage_sdf *self = (struct umbra_coverage_sdf *)s;
    (void)x;
    (void)y;
    self->bmp_off_ = umbra_uniforms_reserve_ptr(u);
    umbra_ptr16 const bmp = umbra_deref_ptr16(builder, (umbra_ptr32){0}, self->bmp_off_);
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
static void sdf_fill_(struct umbra_coverage const *s, void *uniforms) {
    struct umbra_coverage_sdf const *self = (struct umbra_coverage_sdf const *)s;
    umbra_uniforms_fill_ptr(uniforms, self->bmp_off_, self->bmp);
}
struct umbra_coverage_sdf umbra_coverage_sdf(struct umbra_buf bmp) {
    return (struct umbra_coverage_sdf){
        .base = {.build = sdf_build_, .fill = sdf_fill_},
        .bmp = bmp,
    };
}

static umbra_val32 wind_build_(struct umbra_coverage *s, struct umbra_builder *builder,
                               struct umbra_uniforms_layout *u,
                               umbra_val32 x, umbra_val32 y) {
    struct umbra_coverage_wind *self = (struct umbra_coverage_wind *)s;
    (void)x;
    (void)y;
    self->off_ = umbra_uniforms_reserve_ptr(u);
    umbra_ptr32 const w = umbra_deref_ptr32(builder, (umbra_ptr32){0}, self->off_);
    umbra_val32 const raw = umbra_load_32(builder, w);
    return umbra_min_f32(builder, umbra_abs_f32(builder, raw),
                         umbra_imm_f32(builder, 1.0f));
}
static void wind_fill_(struct umbra_coverage const *s, void *uniforms) {
    struct umbra_coverage_wind const *self = (struct umbra_coverage_wind const *)s;
    umbra_uniforms_fill_ptr(uniforms, self->off_, self->wind);
}
struct umbra_coverage_wind umbra_coverage_wind(struct umbra_buf wind) {
    return (struct umbra_coverage_wind){
        .base = {.build = wind_build_, .fill = wind_fill_},
        .wind = wind,
    };
}

static umbra_val32 bitmap_matrix_build_(struct umbra_coverage *s, struct umbra_builder *builder,
                                        struct umbra_uniforms_layout *u,
                                        umbra_val32 x, umbra_val32 y) {
    struct umbra_coverage_bitmap_matrix *self = (struct umbra_coverage_bitmap_matrix *)s;
    self->fi_ = umbra_uniforms_reserve_f32(u, 11);
    self->bmp_off_ = umbra_uniforms_reserve_ptr(u);
    umbra_ptr16 const bmp = umbra_deref_ptr16(builder, (umbra_ptr32){0}, self->bmp_off_);

    umbra_val32 const m0 = umbra_uniform_32(builder, (umbra_ptr32){0}, self->fi_);
    umbra_val32 const m1 = umbra_uniform_32(builder, (umbra_ptr32){0}, self->fi_ + 1);
    umbra_val32 const m2 = umbra_uniform_32(builder, (umbra_ptr32){0}, self->fi_ + 2);
    umbra_val32 const m3 = umbra_uniform_32(builder, (umbra_ptr32){0}, self->fi_ + 3);
    umbra_val32 const m4 = umbra_uniform_32(builder, (umbra_ptr32){0}, self->fi_ + 4);
    umbra_val32 const m5 = umbra_uniform_32(builder, (umbra_ptr32){0}, self->fi_ + 5);
    umbra_val32 const m6 = umbra_uniform_32(builder, (umbra_ptr32){0}, self->fi_ + 6);
    umbra_val32 const m7 = umbra_uniform_32(builder, (umbra_ptr32){0}, self->fi_ + 7);
    umbra_val32 const m8 = umbra_uniform_32(builder, (umbra_ptr32){0}, self->fi_ + 8);
    umbra_val32 const bw = umbra_uniform_32(builder, (umbra_ptr32){0}, self->fi_ + 9);
    umbra_val32 const bh = umbra_uniform_32(builder, (umbra_ptr32){0}, self->fi_ + 10);

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
static void bitmap_matrix_fill_(struct umbra_coverage const *s, void *uniforms) {
    struct umbra_coverage_bitmap_matrix const *self =
        (struct umbra_coverage_bitmap_matrix const *)s;
    umbra_uniforms_fill_f32(uniforms, self->fi_, &self->mat.sx, 9);
    float const wh[2] = {(float)self->bmp.w, (float)self->bmp.h};
    umbra_uniforms_fill_f32(uniforms, self->fi_ + 9, wh, 2);
    umbra_uniforms_fill_ptr(uniforms, self->bmp_off_, self->bmp.buf);
}
struct umbra_coverage_bitmap_matrix umbra_coverage_bitmap_matrix(struct umbra_matrix mat,
                                                                 struct umbra_bitmap bmp) {
    return (struct umbra_coverage_bitmap_matrix){
        .base = {.build = bitmap_matrix_build_, .fill = bitmap_matrix_fill_},
        .mat = mat,
        .bmp = bmp,
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
