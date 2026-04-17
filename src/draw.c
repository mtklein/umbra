#include "../include/umbra_draw.h"
#include "assume.h"
#include "flat_ir.h"

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

umbra_color_val32 umbra_load_8888(struct umbra_builder *b, umbra_ptr32 src) {
    umbra_val32 r, g, bl, a;
    umbra_load_8x4(b, src, &r, &g, &bl, &a);
    umbra_val32 const inv = umbra_imm_f32(b, 1.0f/255);
    return (umbra_color_val32){
        umbra_mul_f32(b, umbra_f32_from_i32(b, r), inv),
        umbra_mul_f32(b, umbra_f32_from_i32(b, g), inv),
        umbra_mul_f32(b, umbra_f32_from_i32(b, bl), inv),
        umbra_mul_f32(b, umbra_f32_from_i32(b, a), inv),
    };
}
void umbra_store_8888(struct umbra_builder *b, umbra_ptr32 dst, umbra_color_val32 c) {
    umbra_val32 s = umbra_imm_f32(b, 255.0f);
    umbra_store_8x4(b, dst, pack_unorm(b, c.r, s), pack_unorm(b, c.g, s),
                            pack_unorm(b, c.b, s), pack_unorm(b, c.a, s));
}

umbra_color_val32 umbra_load_565(struct umbra_builder *b, umbra_ptr16 src) {
    umbra_val32 const px = umbra_i32_from_u16(b, umbra_load_16(b, src));
    umbra_val32 const r5 = umbra_shr_u32(b, px, umbra_imm_i32(b, 11));
    umbra_val32 const g6 = umbra_and_32(b, umbra_shr_u32(b, px, umbra_imm_i32(b, 5)),
                                         umbra_imm_i32(b, 0x3F));
    umbra_val32 const b5 = umbra_and_32(b, px, umbra_imm_i32(b, 0x1F));
    return (umbra_color_val32){
        umbra_mul_f32(b, umbra_f32_from_i32(b, r5), umbra_imm_f32(b, 1.0f/31)),
        umbra_mul_f32(b, umbra_f32_from_i32(b, g6), umbra_imm_f32(b, 1.0f/63)),
        umbra_mul_f32(b, umbra_f32_from_i32(b, b5), umbra_imm_f32(b, 1.0f/31)),
        umbra_imm_f32(b, 1.0f),
    };
}
void umbra_store_565(struct umbra_builder *b, umbra_ptr16 dst, umbra_color_val32 c) {
    umbra_val32 px = pack_unorm(b, c.b, umbra_imm_f32(b, 31.0f));
    px = umbra_or_32(b, px, umbra_shl_i32(b, pack_unorm(b, c.g, umbra_imm_f32(b, 63.0f)),
                                            umbra_imm_i32(b, 5)));
    px = umbra_or_32(b, px, umbra_shl_i32(b, pack_unorm(b, c.r, umbra_imm_f32(b, 31.0f)),
                                            umbra_imm_i32(b, 11)));
    umbra_store_16(b, dst, umbra_i16_from_i32(b, px));
}

umbra_color_val32 umbra_load_1010102(struct umbra_builder *b, umbra_ptr32 src) {
    umbra_val32 const px   = umbra_load_32(b, src);
    umbra_val32 const mask = umbra_imm_i32(b, 0x3FF);
    umbra_val32 const ri   = umbra_and_32(b, px, mask);
    umbra_val32 const gi   = umbra_and_32(b, umbra_shr_u32(b, px, umbra_imm_i32(b, 10)), mask);
    umbra_val32 const bi   = umbra_and_32(b, umbra_shr_u32(b, px, umbra_imm_i32(b, 20)), mask);
    umbra_val32 const ai   = umbra_shr_u32(b, px, umbra_imm_i32(b, 30));
    return (umbra_color_val32){
        umbra_mul_f32(b, umbra_f32_from_i32(b, ri), umbra_imm_f32(b, 1.0f/1023)),
        umbra_mul_f32(b, umbra_f32_from_i32(b, gi), umbra_imm_f32(b, 1.0f/1023)),
        umbra_mul_f32(b, umbra_f32_from_i32(b, bi), umbra_imm_f32(b, 1.0f/1023)),
        umbra_mul_f32(b, umbra_f32_from_i32(b, ai), umbra_imm_f32(b, 1.0f/3)),
    };
}
void umbra_store_1010102(struct umbra_builder *b, umbra_ptr32 dst, umbra_color_val32 c) {
    umbra_val32 s10 = umbra_imm_f32(b, 1023.0f);
    umbra_val32 px = pack_unorm(b, c.r, s10);
    px = umbra_or_32(b, px, umbra_shl_i32(b, pack_unorm(b, c.g, s10), umbra_imm_i32(b, 10)));
    px = umbra_or_32(b, px, umbra_shl_i32(b, pack_unorm(b, c.b, s10), umbra_imm_i32(b, 20)));
    px = umbra_or_32(b, px, umbra_shl_i32(b, pack_unorm(b, c.a, umbra_imm_f32(b, 3.0f)),
                                            umbra_imm_i32(b, 30)));
    umbra_store_32(b, dst, px);
}

umbra_color_val32 umbra_load_fp16(struct umbra_builder *b, umbra_ptr64 src) {
    umbra_val16 r, g, bl, a;
    umbra_load_16x4(b, src, &r, &g, &bl, &a);
    return (umbra_color_val32){
        umbra_f32_from_f16(b, r), umbra_f32_from_f16(b, g),
        umbra_f32_from_f16(b, bl), umbra_f32_from_f16(b, a),
    };
}
void umbra_store_fp16(struct umbra_builder *b, umbra_ptr64 dst, umbra_color_val32 c) {
    umbra_store_16x4(b, dst, umbra_f16_from_f32(b, c.r), umbra_f16_from_f32(b, c.g),
                             umbra_f16_from_f32(b, c.b), umbra_f16_from_f32(b, c.a));
}

umbra_color_val32 umbra_load_fp16_planar(struct umbra_builder *b, umbra_ptr16 src) {
    umbra_val16 r, g, bl, a;
    umbra_load_16x4_planar(b, src, &r, &g, &bl, &a);
    return (umbra_color_val32){
        umbra_f32_from_f16(b, r), umbra_f32_from_f16(b, g),
        umbra_f32_from_f16(b, bl), umbra_f32_from_f16(b, a),
    };
}
void umbra_store_fp16_planar(struct umbra_builder *b, umbra_ptr16 dst, umbra_color_val32 c) {
    umbra_store_16x4_planar(b, dst, umbra_f16_from_f32(b, c.r), umbra_f16_from_f32(b, c.g),
                                    umbra_f16_from_f32(b, c.b), umbra_f16_from_f32(b, c.a));
}

static umbra_color_val32 load_8888(struct umbra_builder *b, int p) {
    return umbra_load_8888(b, (umbra_ptr32){.ix=p});
}
static umbra_color_val32 load_565(struct umbra_builder *b, int p) {
    return umbra_load_565(b, (umbra_ptr16){.ix=p});
}
static umbra_color_val32 load_1010102(struct umbra_builder *b, int p) {
    return umbra_load_1010102(b, (umbra_ptr32){.ix=p});
}
static umbra_color_val32 load_fp16(struct umbra_builder *b, int p) {
    return umbra_load_fp16(b, (umbra_ptr64){.ix=p});
}
static umbra_color_val32 load_fp16p(struct umbra_builder *b, int p) {
    return umbra_load_fp16_planar(b, (umbra_ptr16){.ix=p});
}
static void store_8888(struct umbra_builder *b, int p, umbra_color_val32 c) {
    umbra_store_8888(b, (umbra_ptr32){.ix=p}, c);
}
static void store_565(struct umbra_builder *b, int p, umbra_color_val32 c) {
    umbra_store_565(b, (umbra_ptr16){.ix=p}, c);
}
static void store_1010102(struct umbra_builder *b, int p, umbra_color_val32 c) {
    umbra_store_1010102(b, (umbra_ptr32){.ix=p}, c);
}
static void store_fp16(struct umbra_builder *b, int p, umbra_color_val32 c) {
    umbra_store_fp16(b, (umbra_ptr64){.ix=p}, c);
}
static void store_fp16p(struct umbra_builder *b, int p, umbra_color_val32 c) {
    umbra_store_fp16_planar(b, (umbra_ptr16){.ix=p}, c);
}

struct umbra_fmt const umbra_fmt_8888 = {
    .name="8888", .bpp=4, .planes=1, .load=load_8888, .store=store_8888,
};
struct umbra_fmt const umbra_fmt_565 = {
    .name="565", .bpp=2, .planes=1, .load=load_565, .store=store_565,
};
struct umbra_fmt const umbra_fmt_1010102 = {
    .name="1010102", .bpp=4, .planes=1, .load=load_1010102, .store=store_1010102,
};
struct umbra_fmt const umbra_fmt_fp16 = {
    .name="fp16", .bpp=8, .planes=1, .load=load_fp16, .store=store_fp16,
};
struct umbra_fmt const umbra_fmt_fp16_planar = {
    .name="fp16_planar", .bpp=2, .planes=4, .load=load_fp16p, .store=store_fp16p,
};

struct umbra_builder* umbra_draw_builder(struct umbra_coverage *coverage,
                                         struct umbra_shader *shader,
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
        cov = coverage->build(coverage, builder, &uni, 0, xf, yf);
    }

    umbra_color_val32 src = {
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
    };
    if (shader) {
        src = shader->build(shader, builder, &uni, 0, xf, yf);
    }

    umbra_color_val32 dst = {
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
    };
    if (blend || coverage) {
        dst = fmt.load(builder, 1);
    }

    umbra_color_val32 out;
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
                     struct umbra_coverage const *coverage,
                     struct umbra_shader const *shader) {
    if (shader)   { shader->fill(shader, layout->uniforms); }
    if (coverage) { coverage->fill(coverage, layout->uniforms); }
}

struct sdf_coverage {
    struct umbra_coverage base;
    struct umbra_sdf     *sdf;
    int                   hard_edge, :32;
};

static umbra_val32 sdf_as_coverage_build(struct umbra_coverage *s, struct umbra_builder *b,
                                          struct umbra_uniforms_layout *u,
                                          int buf_index,
                                          umbra_val32 x, umbra_val32 y) {
    struct sdf_coverage *self = (struct sdf_coverage *)s;
    umbra_val32 const half = umbra_imm_f32(b, 0.5f);
    umbra_val32 const xc = umbra_add_f32(b, x, half),
                      yc = umbra_add_f32(b, y, half);
    umbra_val32 const f = self->sdf->build(self->sdf, b, u, buf_index,
                                           (umbra_interval){xc, xc},
                                           (umbra_interval){yc, yc}).lo;
    if (self->hard_edge) {
        return umbra_sel_32(b, umbra_lt_f32(b, f, umbra_imm_f32(b, 0.0f)),
                            umbra_imm_f32(b, 1.0f), umbra_imm_f32(b, 0.0f));
    }
    return umbra_min_f32(b, umbra_imm_f32(b, 1.0f),
                         umbra_max_f32(b, umbra_imm_f32(b, 0.0f),
                                       umbra_sub_f32(b, umbra_imm_f32(b, 0.0f), f)));
}
static void sdf_as_coverage_fill(struct umbra_coverage const *s, void *uniforms) {
    struct sdf_coverage const *self = (struct sdf_coverage const *)s;
    self->sdf->fill(self->sdf, uniforms);
}
static void sdf_as_coverage_free(struct umbra_coverage *s) { free(s); }

struct umbra_coverage* umbra_sdf_coverage(struct umbra_sdf *sdf, _Bool hard_edge) {
    struct sdf_coverage *s = malloc(sizeof *s);
    *s = (struct sdf_coverage){
        .base      = {.build = sdf_as_coverage_build,
                      .fill  = sdf_as_coverage_fill,
                      .free  = sdf_as_coverage_free},
        .sdf       = sdf,
        .hard_edge = hard_edge,
    };
    return &s->base;
}

void umbra_shader_free(struct umbra_shader *s) {
    if (s && s->free) { s->free(s); }
}
void umbra_coverage_free(struct umbra_coverage *c) {
    if (c && c->free) { c->free(c); }
}
void umbra_sdf_free(struct umbra_sdf *s) {
    if (s && s->free) { s->free(s); }
}

struct umbra_sdf_draw {
    struct umbra_program *draw;
    struct umbra_program *bounds;
    struct umbra_backend *bounds_be;
    int                   grid_slot;
    int                   draw_slots;
};

// TODO: query per-backend dispatch_granularity instead of this one global
// compromise.  CPU-optimal is ~16-32, GPU-optimal is ~512-1024.
enum { QUEUE_MIN_TILE = 512 };

struct umbra_sdf_draw* umbra_sdf_draw(struct umbra_backend *be,
                                              struct umbra_sdf *sdf,
                                              struct umbra_sdf_draw_config cfg,
                                              struct umbra_shader *shader,
                                              umbra_blend_fn blend,
                                              struct umbra_fmt fmt,
                                              struct umbra_draw_layout *layout) {
    // Build the draw program (shader + SDF coverage + blend).
    struct umbra_coverage *adapter = umbra_sdf_coverage(sdf, cfg.hard_edge);
    struct umbra_builder *db = umbra_draw_builder(adapter, shader, blend, fmt, layout);
    umbra_coverage_free(adapter);
    struct umbra_flat_ir *dir = umbra_flat_ir(db);
    umbra_builder_free(db);
    struct umbra_program *draw = be->compile(be, dir);
    umbra_flat_ir_free(dir);
    if (!draw) { return NULL; }

    // Build the bounds program using sdf->build with tile-extent intervals.
    // x() and y() are tile indices.  SDF uniforms at offset 0 (matching the
    // draw program), then base_x, base_y, tile_w, tile_h appended after
    // all draw uniforms.
    int const grid_slot = layout->uni.slots;

    struct umbra_builder *bb = umbra_builder();
    umbra_ptr32 const u = {.ix = 0};

    // sdf->build reserves the SDF's uniforms at offset 0.
    struct umbra_uniforms_layout buni = {0};
    umbra_val32 const base_x = umbra_uniform_32(bb, u, grid_slot),
                      base_y = umbra_uniform_32(bb, u, grid_slot + 1),
                      tile_w = umbra_uniform_32(bb, u, grid_slot + 2),
                      tile_h = umbra_uniform_32(bb, u, grid_slot + 3);
    umbra_val32 const xf = umbra_f32_from_i32(bb, umbra_x(bb)),
                      yf = umbra_f32_from_i32(bb, umbra_y(bb));
    umbra_interval const x = {umbra_add_f32(bb, base_x, umbra_mul_f32(bb, xf, tile_w)),
                              umbra_add_f32(bb, base_x,
                                  umbra_mul_f32(bb, umbra_add_f32(bb, xf,
                                      umbra_imm_f32(bb, 1.0f)), tile_w))},
                         y = {umbra_add_f32(bb, base_y, umbra_mul_f32(bb, yf, tile_h)),
                              umbra_add_f32(bb, base_y,
                                  umbra_mul_f32(bb, umbra_add_f32(bb, yf,
                                      umbra_imm_f32(bb, 1.0f)), tile_h))};

    umbra_interval const f = sdf->build(sdf, bb, &buni, 0, x, y);

    umbra_store_32(bb, (umbra_ptr32){.ix = 1}, f.lo);

    struct umbra_flat_ir *bir = umbra_flat_ir(bb);
    umbra_builder_free(bb);
    struct umbra_backend *bounds_be = umbra_backend_jit();
    if (!bounds_be) { bounds_be = umbra_backend_interp(); }
    struct umbra_program *bounds = bounds_be->compile(bounds_be, bir);
    umbra_flat_ir_free(bir);

    struct umbra_sdf_draw *d = calloc(1, sizeof *d);
    d->draw       = draw;
    d->bounds     = bounds;
    d->bounds_be  = bounds_be;
    d->grid_slot  = grid_slot;
    d->draw_slots = layout->uni.slots;
    return d;
}

void umbra_sdf_draw_queue(struct umbra_sdf_draw const *d,
                              int l, int t, int r, int b, struct umbra_buf buf[]) {
    if (!d) { return; }
    int const w  = r - l,
              h  = b - t,
              xt = (w + QUEUE_MIN_TILE - 1) / QUEUE_MIN_TILE,
              yt = (h + QUEUE_MIN_TILE - 1) / QUEUE_MIN_TILE;
    int const tiles = xt * yt;

    int const bounds_slots = d->grid_slot + 4;
    float *bounds_uni = malloc((size_t)bounds_slots * sizeof(float));
    __builtin_memcpy(bounds_uni, buf[0].ptr, (size_t)d->draw_slots * sizeof(float));
    bounds_uni[d->grid_slot]     = (float)l;
    bounds_uni[d->grid_slot + 1] = (float)t;
    bounds_uni[d->grid_slot + 2] = (float)QUEUE_MIN_TILE;
    bounds_uni[d->grid_slot + 3] = (float)QUEUE_MIN_TILE;

    float *lo = calloc((size_t)tiles, sizeof(float));
    struct umbra_buf bounds_buf[] = {
        {.ptr = bounds_uni, .count = bounds_slots},
        {.ptr = lo,         .count = tiles, .stride = xt},
    };
    d->bounds->queue(d->bounds, 0, 0, xt, yt, bounds_buf);

    // TODO: coalesce horizontally adjacent covered tiles into one draw->queue() call.
    for (int ty = 0; ty < yt; ty++) {
        for (int tx = 0; tx < xt; tx++) {
            if (lo[ty * xt + tx] < 0) {
                int const tl = l + tx * QUEUE_MIN_TILE,
                          tt = t + ty * QUEUE_MIN_TILE,
                          tr = tl + QUEUE_MIN_TILE < r ? tl + QUEUE_MIN_TILE : r,
                          tb = tt + QUEUE_MIN_TILE < b ? tt + QUEUE_MIN_TILE : b;
                d->draw->queue(d->draw, tl, tt, tr, tb, buf);
            }
        }
    }

    free(lo);
    free(bounds_uni);
}

void umbra_sdf_draw_fill(struct umbra_draw_layout const *layout,
                             struct umbra_sdf const *sdf,
                             struct umbra_shader const *shader) {
    if (sdf)    { sdf->fill(sdf, layout->uniforms); }
    if (shader) { shader->fill(shader, layout->uniforms); }
}

void umbra_sdf_draw_free(struct umbra_sdf_draw *d) {
    if (d) {
        d->draw->free(d->draw);
        d->bounds->free(d->bounds);
        d->bounds_be->free(d->bounds_be);
        free(d);
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

static umbra_val32 linear_t(struct umbra_builder *builder, int fi,
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

static umbra_val32 radial_t(struct umbra_builder *builder, int fi,
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

static umbra_color_val32 lerp_2stop(struct umbra_builder *builder, umbra_val32 t, int fi) {
    umbra_val32 const r0 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi);
    umbra_val32 const g0 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 1);
    umbra_val32 const b0 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 2);
    umbra_val32 const a0 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 3);
    umbra_val32 const r1 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 4);
    umbra_val32 const g1 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 5);
    umbra_val32 const b1 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 6);
    umbra_val32 const a1 = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 7);
    return (umbra_color_val32){
        lerp_f(builder, r0, r1, t),
        lerp_f(builder, g0, g1, t),
        lerp_f(builder, b0, b1, t),
        lerp_f(builder, a0, a1, t),
    };
}

static umbra_color_val32 sample_lut(struct umbra_builder *builder, umbra_val32 t_f32, int fi,
                                umbra_ptr32 lut) {
    umbra_val32 const N_f  = umbra_uniform_32(builder, (umbra_ptr32){0}, fi + 3);
    umbra_val32 const N_m1 = umbra_sub_f32(builder, N_f, umbra_imm_f32(builder, 1.0f));
    umbra_val32 const N_m2 = umbra_sub_f32(builder, N_f, umbra_imm_f32(builder, 2.0f));
    umbra_val32 const t_sc = umbra_mul_f32(builder, t_f32, N_m1);
    umbra_val32 const frac_idx = umbra_min_f32(builder, t_sc, N_m2);

    umbra_val32 const N2_f = umbra_add_f32(builder, N_f, N_f);
    umbra_val32 const N3_f = umbra_add_f32(builder, N2_f, N_f);
    return (umbra_color_val32){
        sample(builder, lut, frac_idx),
        sample(builder, lut, umbra_add_f32(builder, frac_idx, N_f)),
        sample(builder, lut, umbra_add_f32(builder, frac_idx, N2_f)),
        sample(builder, lut, umbra_add_f32(builder, frac_idx, N3_f)),
    };
}

static umbra_color_val32 walk_stops(struct umbra_builder *b, umbra_val32 t,
                               int fi, umbra_ptr32 colors, umbra_ptr32 pos) {
    umbra_val32 const n_i = umbra_i32_from_f32(b,
                                umbra_uniform_32(b, (umbra_ptr32){0}, fi + 3));
    umbra_val32 const n_segs = umbra_sub_i32(b, n_i, umbra_imm_i32(b, 1));
    umbra_val32 const n2 = umbra_add_i32(b, n_i, n_i);
    umbra_val32 const n3 = umbra_add_i32(b, n2, n_i);

    struct umbra_var32 vr = umbra_var32(b),
                       vg = umbra_var32(b),
                       vb = umbra_var32(b),
                       va = umbra_var32(b);

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

            umbra_store_var32(b, vr, lerp_f(b, r0, r1, frac));
            umbra_store_var32(b, vg, lerp_f(b, g0, g1, frac));
            umbra_store_var32(b, vb, lerp_f(b, b0, b1, frac));
            umbra_store_var32(b, va, lerp_f(b, a0, a1, frac));
        } umbra_end_if(b);
    } umbra_end_loop(b);

    return (umbra_color_val32){
        umbra_load_var32(b, vr),
        umbra_load_var32(b, vg),
        umbra_load_var32(b, vb),
        umbra_load_var32(b, va),
    };
}

struct shader_solid {
    struct umbra_shader base;
    umbra_color color;
    int color_off, :32;
};

static umbra_color_val32 solid_build(struct umbra_shader *s, struct umbra_builder *builder,
                                struct umbra_uniforms_layout *u,
                                int buf_index,
                                umbra_val32 x, umbra_val32 y) {
    struct shader_solid *self = (struct shader_solid *)s;
    (void)x;
    (void)y;
    (void)buf_index;
    self->color_off = umbra_uniforms_reserve_f32(u, 4);
    umbra_val32 const r = umbra_uniform_32(builder, (umbra_ptr32){0}, self->color_off);
    umbra_val32 const g = umbra_uniform_32(builder, (umbra_ptr32){0}, self->color_off + 1);
    umbra_val32 const b = umbra_uniform_32(builder, (umbra_ptr32){0}, self->color_off + 2);
    umbra_val32 const a = umbra_uniform_32(builder, (umbra_ptr32){0}, self->color_off + 3);
    return (umbra_color_val32){r, g, b, a};
}
static void solid_fill(struct umbra_shader const *s, void *uniforms) {
    struct shader_solid const *self = (struct shader_solid const *)s;
    umbra_uniforms_fill_f32(uniforms, self->color_off, &self->color.r, 4);
}
static void solid_free(struct umbra_shader *s) { free(s); }

struct umbra_shader* umbra_shader_solid(umbra_color color) {
    struct shader_solid *s = malloc(sizeof *s);
    *s = (struct shader_solid){
        .base  = {.build = solid_build, .fill = solid_fill, .free = solid_free},
        .color = color,
    };
    return &s->base;
}

struct shader_gradient_linear_two_stops {
    struct umbra_shader base;
    umbra_point p0, p1;
    umbra_color c0, c1;
    int coeffs_off, colors_off;
};

static umbra_color_val32 linear_two_stops_build(struct umbra_shader *s, struct umbra_builder *builder,
                                   struct umbra_uniforms_layout *u,
                                   int buf_index,
                                   umbra_val32 x, umbra_val32 y) {
    struct shader_gradient_linear_two_stops *self = (struct shader_gradient_linear_two_stops *)s;
    (void)buf_index;
    self->coeffs_off = umbra_uniforms_reserve_f32(u, 3);
    self->colors_off = umbra_uniforms_reserve_f32(u, 8);
    return lerp_2stop(builder, linear_t(builder, self->coeffs_off, x, y), self->colors_off);
}
static void linear_two_stops_fill(struct umbra_shader const *s, void *uniforms) {
    struct shader_gradient_linear_two_stops const *self =
        (struct shader_gradient_linear_two_stops const *)s;
    float const dx = self->p1.x - self->p0.x,
                dy = self->p1.y - self->p0.y,
                L2 = dx*dx + dy*dy,
                a  = dx / L2,
                b  = dy / L2,
                c  = -(a * self->p0.x + b * self->p0.y);
    float const grad[3] = {a, b, c};
    umbra_uniforms_fill_f32(uniforms, self->coeffs_off,     grad,         3);
    umbra_uniforms_fill_f32(uniforms, self->colors_off,     &self->c0.r, 4);
    umbra_uniforms_fill_f32(uniforms, self->colors_off + 4, &self->c1.r, 4);
}
static void linear_two_stops_free(struct umbra_shader *s) { free(s); }

struct umbra_shader* umbra_shader_gradient_linear_two_stops(umbra_point p0, umbra_point p1,
                                                            umbra_color c0, umbra_color c1) {
    struct shader_gradient_linear_two_stops *s = malloc(sizeof *s);
    *s = (struct shader_gradient_linear_two_stops){
        .base = {.build = linear_two_stops_build,
                 .fill  = linear_two_stops_fill,
                 .free  = linear_two_stops_free},
        .p0   = p0, .p1 = p1,
        .c0   = c0, .c1 = c1,
    };
    return &s->base;
}

struct shader_gradient_radial_two_stops {
    struct umbra_shader base;
    umbra_point center;
    float       radius; int :32;
    umbra_color c0, c1;
    int coeffs_off, colors_off;
};

static umbra_color_val32 radial_two_stops_build(struct umbra_shader *s, struct umbra_builder *builder,
                                   struct umbra_uniforms_layout *u,
                                   int buf_index,
                                   umbra_val32 x, umbra_val32 y) {
    struct shader_gradient_radial_two_stops *self = (struct shader_gradient_radial_two_stops *)s;
    (void)buf_index;
    self->coeffs_off = umbra_uniforms_reserve_f32(u, 3);
    self->colors_off = umbra_uniforms_reserve_f32(u, 8);
    return lerp_2stop(builder, radial_t(builder, self->coeffs_off, x, y), self->colors_off);
}
static void radial_two_stops_fill(struct umbra_shader const *s, void *uniforms) {
    struct shader_gradient_radial_two_stops const *self =
        (struct shader_gradient_radial_two_stops const *)s;
    float const grad[3] = {self->center.x, self->center.y, 1.0f / self->radius};
    umbra_uniforms_fill_f32(uniforms, self->coeffs_off,     grad,         3);
    umbra_uniforms_fill_f32(uniforms, self->colors_off,     &self->c0.r, 4);
    umbra_uniforms_fill_f32(uniforms, self->colors_off + 4, &self->c1.r, 4);
}
static void radial_two_stops_free(struct umbra_shader *s) { free(s); }

struct umbra_shader* umbra_shader_gradient_radial_two_stops(umbra_point center, float radius,
                                                            umbra_color c0, umbra_color c1) {
    struct shader_gradient_radial_two_stops *s = malloc(sizeof *s);
    *s = (struct shader_gradient_radial_two_stops){
        .base   = {.build = radial_two_stops_build,
                   .fill  = radial_two_stops_fill,
                   .free  = radial_two_stops_free},
        .center = center,
        .radius = radius,
        .c0     = c0, .c1 = c1,
    };
    return &s->base;
}

struct shader_gradient_linear_lut {
    struct umbra_shader base;
    umbra_point p0, p1;
    struct umbra_buf lut;
    int coeffs_off, lut_off;
};

static umbra_color_val32 linear_lut_build(struct umbra_shader *s, struct umbra_builder *builder,
                                      struct umbra_uniforms_layout *u,
                                      int buf_index,
                                      umbra_val32 x, umbra_val32 y) {
    struct shader_gradient_linear_lut *self = (struct shader_gradient_linear_lut *)s;
    (void)buf_index;
    self->coeffs_off = umbra_uniforms_reserve_f32(u, 4);
    self->lut_off    = umbra_uniforms_reserve_ptr(u);
    umbra_ptr32 const lut = umbra_deref_ptr32(builder, (umbra_ptr32){0}, self->lut_off);
    return sample_lut(builder, linear_t(builder, self->coeffs_off, x, y), self->coeffs_off, lut);
}
static void linear_lut_fill(struct umbra_shader const *s, void *uniforms) {
    struct shader_gradient_linear_lut const *self = (struct shader_gradient_linear_lut const *)s;
    float const dx = self->p1.x - self->p0.x,
                dy = self->p1.y - self->p0.y,
                L2 = dx*dx + dy*dy,
                a  = dx / L2,
                b  = dy / L2,
                c  = -(a * self->p0.x + b * self->p0.y),
                 N = (float)(self->lut.count / 4);
    float const grad[4] = {a, b, c, N};
    umbra_uniforms_fill_f32(uniforms, self->coeffs_off, grad, 4);
    umbra_uniforms_fill_ptr(uniforms, self->lut_off,    self->lut);
}
static void linear_lut_free(struct umbra_shader *s) { free(s); }

struct umbra_shader* umbra_shader_gradient_linear_lut(umbra_point p0, umbra_point p1,
                                                      struct umbra_buf lut) {
    struct shader_gradient_linear_lut *s = malloc(sizeof *s);
    *s = (struct shader_gradient_linear_lut){
        .base = {.build = linear_lut_build, .fill = linear_lut_fill, .free = linear_lut_free},
        .p0   = p0, .p1 = p1,
        .lut  = lut,
    };
    return &s->base;
}

struct shader_gradient_radial_lut {
    struct umbra_shader base;
    umbra_point center;
    float       radius; int :32;
    struct umbra_buf lut;
    int coeffs_off, lut_off;
};

static umbra_color_val32 radial_lut_build(struct umbra_shader *s, struct umbra_builder *builder,
                                      struct umbra_uniforms_layout *u,
                                      int buf_index,
                                      umbra_val32 x, umbra_val32 y) {
    struct shader_gradient_radial_lut *self = (struct shader_gradient_radial_lut *)s;
    (void)buf_index;
    self->coeffs_off = umbra_uniforms_reserve_f32(u, 4);
    self->lut_off    = umbra_uniforms_reserve_ptr(u);
    umbra_ptr32 const lut = umbra_deref_ptr32(builder, (umbra_ptr32){0}, self->lut_off);
    return sample_lut(builder, radial_t(builder, self->coeffs_off, x, y), self->coeffs_off, lut);
}
static void radial_lut_fill(struct umbra_shader const *s, void *uniforms) {
    struct shader_gradient_radial_lut const *self = (struct shader_gradient_radial_lut const *)s;
    float const N = (float)(self->lut.count / 4);
    float const grad[4] = {self->center.x, self->center.y, 1.0f / self->radius, N};
    umbra_uniforms_fill_f32(uniforms, self->coeffs_off, grad, 4);
    umbra_uniforms_fill_ptr(uniforms, self->lut_off,    self->lut);
}
static void radial_lut_free(struct umbra_shader *s) { free(s); }

struct umbra_shader* umbra_shader_gradient_radial_lut(umbra_point center, float radius,
                                                      struct umbra_buf lut) {
    struct shader_gradient_radial_lut *s = malloc(sizeof *s);
    *s = (struct shader_gradient_radial_lut){
        .base   = {.build = radial_lut_build, .fill = radial_lut_fill, .free = radial_lut_free},
        .center = center,
        .radius = radius,
        .lut    = lut,
    };
    return &s->base;
}

struct shader_gradient_linear {
    struct umbra_shader base;
    umbra_point p0, p1;
    struct umbra_buf colors, pos;
    int coeffs_off, colors_off, pos_off, :32;
};

static umbra_color_val32 linear_build(struct umbra_shader *s, struct umbra_builder *builder,
                                       struct umbra_uniforms_layout *u,
                                       int buf_index,
                                       umbra_val32 x, umbra_val32 y) {
    struct shader_gradient_linear *self = (struct shader_gradient_linear *)s;
    (void)buf_index;
    self->coeffs_off = umbra_uniforms_reserve_f32(u, 4);
    self->colors_off = umbra_uniforms_reserve_ptr(u);
    self->pos_off    = umbra_uniforms_reserve_ptr(u);
    umbra_ptr32 const colors = umbra_deref_ptr32(builder, (umbra_ptr32){0}, self->colors_off);
    umbra_ptr32 const pos    = umbra_deref_ptr32(builder, (umbra_ptr32){0}, self->pos_off);
    return walk_stops(builder, linear_t(builder, self->coeffs_off, x, y),
                      self->coeffs_off, colors, pos);
}
static void linear_fill(struct umbra_shader const *s, void *uniforms) {
    struct shader_gradient_linear const *self = (struct shader_gradient_linear const *)s;
    float const dx = self->p1.x - self->p0.x,
                dy = self->p1.y - self->p0.y,
                L2 = dx*dx + dy*dy,
                a  = dx / L2,
                b  = dy / L2,
                c  = -(a * self->p0.x + b * self->p0.y),
                 N = (float)self->pos.count;
    float const grad[4] = {a, b, c, N};
    umbra_uniforms_fill_f32(uniforms, self->coeffs_off, grad, 4);
    umbra_uniforms_fill_ptr(uniforms, self->colors_off, self->colors);
    umbra_uniforms_fill_ptr(uniforms, self->pos_off,    self->pos);
}
static void linear_free(struct umbra_shader *s) { free(s); }

struct umbra_shader* umbra_shader_gradient_linear(umbra_point p0, umbra_point p1,
                                                  struct umbra_buf colors,
                                                  struct umbra_buf pos) {
    struct shader_gradient_linear *s = malloc(sizeof *s);
    *s = (struct shader_gradient_linear){
        .base   = {.build = linear_build, .fill = linear_fill, .free = linear_free},
        .p0     = p0, .p1 = p1,
        .colors = colors,
        .pos    = pos,
    };
    return &s->base;
}

struct shader_gradient_radial {
    struct umbra_shader base;
    umbra_point center;
    float       radius; int :32;
    struct umbra_buf colors, pos;
    int coeffs_off, colors_off, pos_off, :32;
};

static umbra_color_val32 radial_build(struct umbra_shader *s, struct umbra_builder *builder,
                                       struct umbra_uniforms_layout *u,
                                       int buf_index,
                                       umbra_val32 x, umbra_val32 y) {
    struct shader_gradient_radial *self = (struct shader_gradient_radial *)s;
    (void)buf_index;
    self->coeffs_off = umbra_uniforms_reserve_f32(u, 4);
    self->colors_off = umbra_uniforms_reserve_ptr(u);
    self->pos_off    = umbra_uniforms_reserve_ptr(u);
    umbra_ptr32 const colors = umbra_deref_ptr32(builder, (umbra_ptr32){0}, self->colors_off);
    umbra_ptr32 const pos    = umbra_deref_ptr32(builder, (umbra_ptr32){0}, self->pos_off);
    return walk_stops(builder, radial_t(builder, self->coeffs_off, x, y),
                      self->coeffs_off, colors, pos);
}
static void radial_fill(struct umbra_shader const *s, void *uniforms) {
    struct shader_gradient_radial const *self = (struct shader_gradient_radial const *)s;
    float const N = (float)self->pos.count;
    float const grad[4] = {self->center.x, self->center.y, 1.0f / self->radius, N};
    umbra_uniforms_fill_f32(uniforms, self->coeffs_off, grad, 4);
    umbra_uniforms_fill_ptr(uniforms, self->colors_off, self->colors);
    umbra_uniforms_fill_ptr(uniforms, self->pos_off,    self->pos);
}
static void radial_free(struct umbra_shader *s) { free(s); }

struct umbra_shader* umbra_shader_gradient_radial(umbra_point center, float radius,
                                                  struct umbra_buf colors,
                                                  struct umbra_buf pos) {
    struct shader_gradient_radial *s = malloc(sizeof *s);
    *s = (struct shader_gradient_radial){
        .base   = {.build = radial_build, .fill = radial_fill, .free = radial_free},
        .center = center,
        .radius = radius,
        .colors = colors,
        .pos    = pos,
    };
    return &s->base;
}

struct shader_supersample {
    struct umbra_shader base;
    struct umbra_shader *inner;
    int samples, :32;
};

static umbra_color_val32 supersample_build(struct umbra_shader *s, struct umbra_builder *builder,
                                      struct umbra_uniforms_layout *u,
                                      int buf_index,
                                      umbra_val32 x, umbra_val32 y) {
    struct shader_supersample *self = (struct shader_supersample *)s;
    static float const jitter[][2] = {
        {-0.375f, -0.125f}, {0.125f, -0.375f}, {0.375f, 0.125f}, {-0.125f, 0.375f},
        {-0.250f, 0.375f},  {0.250f, -0.250f}, {0.375f, 0.250f}, {-0.375f, -0.250f},
    };
    int samples = self->samples;
    if (samples < 1) { samples = 1; }
    if (samples > 8) { samples = 8; }

    int         const saved = u->slots;
    umbra_color_val32 sum = self->inner->build(self->inner, builder, u, buf_index, x, y);
    int         const after = u->slots;

    for (int i = 1; i < samples; i++) {
        struct umbra_uniforms_layout scratch = {.slots = saved};
        umbra_val32 const sx = umbra_add_f32(builder, x,
                                              umbra_imm_f32(builder, jitter[i - 1][0]));
        umbra_val32 const sy = umbra_add_f32(builder, y,
                                              umbra_imm_f32(builder, jitter[i - 1][1]));
        umbra_color_val32 const c = self->inner->build(self->inner, builder, &scratch,
                                                       buf_index, sx, sy);
        assume(scratch.slots == after);
        sum.r = umbra_add_f32(builder, sum.r, c.r);
        sum.g = umbra_add_f32(builder, sum.g, c.g);
        sum.b = umbra_add_f32(builder, sum.b, c.b);
        sum.a = umbra_add_f32(builder, sum.a, c.a);
    }

    umbra_val32 const inv = umbra_imm_f32(builder, 1.0f / (float)samples);
    return (umbra_color_val32){
        umbra_mul_f32(builder, sum.r, inv),
        umbra_mul_f32(builder, sum.g, inv),
        umbra_mul_f32(builder, sum.b, inv),
        umbra_mul_f32(builder, sum.a, inv),
    };
}
static void supersample_fill(struct umbra_shader const *s, void *uniforms) {
    struct shader_supersample const *self = (struct shader_supersample const *)s;
    self->inner->fill(self->inner, uniforms);
}
static void supersample_free(struct umbra_shader *s) { free(s); }

struct umbra_shader* umbra_shader_supersample(struct umbra_shader *inner, int samples) {
    struct shader_supersample *s = malloc(sizeof *s);
    *s = (struct shader_supersample){
        .base    = {.build = supersample_build, .fill = supersample_fill, .free = supersample_free},
        .inner   = inner,
        .samples = samples,
    };
    return &s->base;
}

struct coverage_rect {
    struct umbra_coverage base;
    umbra_rect rect;
    int rect_off, :32;
};

static umbra_val32 rect_build(struct umbra_coverage *s, struct umbra_builder *builder,
                               struct umbra_uniforms_layout *u,
                               int buf_index,
                               umbra_val32 x, umbra_val32 y) {
    struct coverage_rect *self = (struct coverage_rect *)s;
    (void)buf_index;
    self->rect_off = umbra_uniforms_reserve_f32(u, 4);
    umbra_val32 const l = umbra_uniform_32(builder, (umbra_ptr32){0}, self->rect_off);
    umbra_val32 const t = umbra_uniform_32(builder, (umbra_ptr32){0}, self->rect_off + 1);
    umbra_val32 const r = umbra_uniform_32(builder, (umbra_ptr32){0}, self->rect_off + 2);
    umbra_val32 const b = umbra_uniform_32(builder, (umbra_ptr32){0}, self->rect_off + 3);
    umbra_val32 const inside = umbra_and_32(builder,
                                           umbra_and_32(builder, umbra_le_f32(builder, l, x),
                                                         umbra_lt_f32(builder, x, r)),
                                           umbra_and_32(builder, umbra_le_f32(builder, t, y),
                                                         umbra_lt_f32(builder, y, b)));
    umbra_val32 const one_f = umbra_imm_f32(builder, 1.0f);
    umbra_val32 const zero_f = umbra_imm_f32(builder, 0.0f);
    return umbra_sel_32(builder, inside, one_f, zero_f);
}
static void rect_fill(struct umbra_coverage const *s, void *uniforms) {
    struct coverage_rect const *self = (struct coverage_rect const *)s;
    umbra_uniforms_fill_f32(uniforms, self->rect_off, &self->rect.l, 4);
}
static void rect_free(struct umbra_coverage *s) { free(s); }

struct umbra_coverage* umbra_coverage_rect(umbra_rect rect) {
    struct coverage_rect *c = malloc(sizeof *c);
    *c = (struct coverage_rect){
        .base = {.build = rect_build, .fill = rect_fill, .free = rect_free},
        .rect = rect,
    };
    return &c->base;
}

struct sdf_rect {
    struct umbra_sdf base;
    umbra_rect rect;
    int rect_off, :32;
};

static umbra_interval sdf_rect_build(struct umbra_sdf *s, struct umbra_builder *b,
                                      struct umbra_uniforms_layout *u,
                                      int buf_index,
                                      umbra_interval x, umbra_interval y) {
    struct sdf_rect *self = (struct sdf_rect *)s;
    (void)buf_index;
    self->rect_off = umbra_uniforms_reserve_f32(u, 4);
    umbra_interval const l  = umbra_interval_exact(
                                 umbra_uniform_32(b, (umbra_ptr32){0}, self->rect_off)),
                         t  = umbra_interval_exact(
                                 umbra_uniform_32(b, (umbra_ptr32){0}, self->rect_off + 1)),
                         r  = umbra_interval_exact(
                                 umbra_uniform_32(b, (umbra_ptr32){0}, self->rect_off + 2)),
                         bo = umbra_interval_exact(
                                 umbra_uniform_32(b, (umbra_ptr32){0}, self->rect_off + 3));
    return umbra_interval_max_f32(b, umbra_interval_max_f32(b, umbra_interval_sub_f32(b, l, x),
                                                               umbra_interval_sub_f32(b, x, r)),
                                    umbra_interval_max_f32(b, umbra_interval_sub_f32(b, t, y),
                                                              umbra_interval_sub_f32(b, y, bo)));
}
static void sdf_rect_fill(struct umbra_sdf const *s, void *uniforms) {
    struct sdf_rect const *self = (struct sdf_rect const *)s;
    umbra_uniforms_fill_f32(uniforms, self->rect_off, &self->rect.l, 4);
}
static void sdf_rect_free(struct umbra_sdf *s) { free(s); }

struct umbra_sdf* umbra_sdf_rect(umbra_rect rect) {
    struct sdf_rect *s = malloc(sizeof *s);
    *s = (struct sdf_rect){
        .base = {.build = sdf_rect_build, .fill = sdf_rect_fill, .free = sdf_rect_free},
        .rect = rect,
    };
    return &s->base;
}

struct coverage_bitmap {
    struct umbra_coverage base;
    struct umbra_buf bmp;
    int bmp_off, :32;
};

static umbra_val32 bitmap_build(struct umbra_coverage *s, struct umbra_builder *builder,
                                 struct umbra_uniforms_layout *u,
                                 int buf_index,
                                 umbra_val32 x, umbra_val32 y) {
    struct coverage_bitmap *self = (struct coverage_bitmap *)s;
    (void)x;
    (void)y;
    (void)buf_index;
    self->bmp_off = umbra_uniforms_reserve_ptr(u);
    umbra_ptr16 const bmp = umbra_deref_ptr16(builder, (umbra_ptr32){0}, self->bmp_off);
    umbra_val32 const val = umbra_i32_from_s16(builder, umbra_load_16(builder, bmp));
    umbra_val32 const inv255 = umbra_imm_f32(builder, 1.0f / 255.0f);
    return umbra_mul_f32(builder, umbra_f32_from_i32(builder, val), inv255);
}
static void bitmap_fill(struct umbra_coverage const *s, void *uniforms) {
    struct coverage_bitmap const *self = (struct coverage_bitmap const *)s;
    umbra_uniforms_fill_ptr(uniforms, self->bmp_off, self->bmp);
}
static void bitmap_free(struct umbra_coverage *s) { free(s); }

struct umbra_coverage* umbra_coverage_bitmap(struct umbra_buf bmp) {
    struct coverage_bitmap *c = malloc(sizeof *c);
    *c = (struct coverage_bitmap){
        .base = {.build = bitmap_build, .fill = bitmap_fill, .free = bitmap_free},
        .bmp  = bmp,
    };
    return &c->base;
}

struct coverage_sdf {
    struct umbra_coverage base;
    struct umbra_buf bmp;
    int bmp_off, :32;
};

static umbra_val32 cov_sdf_build(struct umbra_coverage *s, struct umbra_builder *builder,
                                  struct umbra_uniforms_layout *u,
                                  int buf_index,
                                  umbra_val32 x, umbra_val32 y) {
    struct coverage_sdf *self = (struct coverage_sdf *)s;
    (void)x;
    (void)y;
    (void)buf_index;
    self->bmp_off = umbra_uniforms_reserve_ptr(u);
    umbra_ptr16 const bmp = umbra_deref_ptr16(builder, (umbra_ptr32){0}, self->bmp_off);
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
static void cov_sdf_fill(struct umbra_coverage const *s, void *uniforms) {
    struct coverage_sdf const *self = (struct coverage_sdf const *)s;
    umbra_uniforms_fill_ptr(uniforms, self->bmp_off, self->bmp);
}
static void cov_sdf_free(struct umbra_coverage *s) { free(s); }

struct umbra_coverage* umbra_coverage_sdf(struct umbra_buf bmp) {
    struct coverage_sdf *c = malloc(sizeof *c);
    *c = (struct coverage_sdf){
        .base = {.build = cov_sdf_build, .fill = cov_sdf_fill, .free = cov_sdf_free},
        .bmp  = bmp,
    };
    return &c->base;
}

struct coverage_winding {
    struct umbra_coverage base;
    struct umbra_buf winding;
    int winding_off, :32;
};

static umbra_val32 winding_build(struct umbra_coverage *s, struct umbra_builder *builder,
                               struct umbra_uniforms_layout *u,
                               int buf_index,
                               umbra_val32 x, umbra_val32 y) {
    struct coverage_winding *self = (struct coverage_winding *)s;
    (void)x;
    (void)y;
    (void)buf_index;
    self->winding_off = umbra_uniforms_reserve_ptr(u);
    umbra_ptr32 const w = umbra_deref_ptr32(builder, (umbra_ptr32){0}, self->winding_off);
    umbra_val32 const raw = umbra_load_32(builder, w);
    return umbra_min_f32(builder, umbra_abs_f32(builder, raw),
                         umbra_imm_f32(builder, 1.0f));
}
static void winding_fill(struct umbra_coverage const *s, void *uniforms) {
    struct coverage_winding const *self = (struct coverage_winding const *)s;
    umbra_uniforms_fill_ptr(uniforms, self->winding_off, self->winding);
}
static void winding_free(struct umbra_coverage *s) { free(s); }

struct umbra_coverage* umbra_coverage_winding(struct umbra_buf wind) {
    struct coverage_winding *c = malloc(sizeof *c);
    *c = (struct coverage_winding){
        .base    = {.build = winding_build, .fill = winding_fill, .free = winding_free},
        .winding = wind,
    };
    return &c->base;
}

struct coverage_bitmap_matrix {
    struct umbra_coverage base;
    struct umbra_matrix mat; int :32;
    struct umbra_bitmap bmp;
    int mat_off, bmp_off;
};

static umbra_val32 bitmap_matrix_build(struct umbra_coverage *s, struct umbra_builder *builder,
                                        struct umbra_uniforms_layout *u,
                                        int buf_index,
                                        umbra_val32 x, umbra_val32 y) {
    struct coverage_bitmap_matrix *self = (struct coverage_bitmap_matrix *)s;
    (void)buf_index;
    self->mat_off = umbra_uniforms_reserve_f32(u, 11);
    self->bmp_off = umbra_uniforms_reserve_ptr(u);
    umbra_ptr16 const bmp = umbra_deref_ptr16(builder, (umbra_ptr32){0}, self->bmp_off);

    umbra_val32 const m0 = umbra_uniform_32(builder, (umbra_ptr32){0}, self->mat_off);
    umbra_val32 const m1 = umbra_uniform_32(builder, (umbra_ptr32){0}, self->mat_off + 1);
    umbra_val32 const m2 = umbra_uniform_32(builder, (umbra_ptr32){0}, self->mat_off + 2);
    umbra_val32 const m3 = umbra_uniform_32(builder, (umbra_ptr32){0}, self->mat_off + 3);
    umbra_val32 const m4 = umbra_uniform_32(builder, (umbra_ptr32){0}, self->mat_off + 4);
    umbra_val32 const m5 = umbra_uniform_32(builder, (umbra_ptr32){0}, self->mat_off + 5);
    umbra_val32 const m6 = umbra_uniform_32(builder, (umbra_ptr32){0}, self->mat_off + 6);
    umbra_val32 const m7 = umbra_uniform_32(builder, (umbra_ptr32){0}, self->mat_off + 7);
    umbra_val32 const m8 = umbra_uniform_32(builder, (umbra_ptr32){0}, self->mat_off + 8);
    umbra_val32 const bw = umbra_uniform_32(builder, (umbra_ptr32){0}, self->mat_off + 9);
    umbra_val32 const bh = umbra_uniform_32(builder, (umbra_ptr32){0}, self->mat_off + 10);

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
static void bitmap_matrix_fill(struct umbra_coverage const *s, void *uniforms) {
    struct coverage_bitmap_matrix const *self = (struct coverage_bitmap_matrix const *)s;
    umbra_uniforms_fill_f32(uniforms, self->mat_off, &self->mat.sx, 9);
    float const wh[2] = {(float)self->bmp.w, (float)self->bmp.h};
    umbra_uniforms_fill_f32(uniforms, self->mat_off + 9, wh, 2);
    umbra_uniforms_fill_ptr(uniforms, self->bmp_off, self->bmp.buf);
}
static void bitmap_matrix_free(struct umbra_coverage *s) { free(s); }

struct umbra_coverage* umbra_coverage_bitmap_matrix(struct umbra_matrix mat,
                                                    struct umbra_bitmap bmp) {
    struct coverage_bitmap_matrix *c = malloc(sizeof *c);
    *c = (struct coverage_bitmap_matrix){
        .base = {.build = bitmap_matrix_build,
                 .fill  = bitmap_matrix_fill,
                 .free  = bitmap_matrix_free},
        .mat  = mat,
        .bmp  = bmp,
    };
    return &c->base;
}

umbra_color_val32 umbra_blend_src(struct umbra_builder *builder, umbra_color_val32 src, umbra_color_val32 dst) {
    (void)builder;
    (void)dst;
    return src;
}

umbra_color_val32 umbra_blend_srcover(struct umbra_builder *builder, umbra_color_val32 src, umbra_color_val32 dst) {
    umbra_val32 const one = umbra_imm_f32(builder, 1.0f);
    umbra_val32 const inv_a = umbra_sub_f32(builder, one, src.a);
    return (umbra_color_val32){
        umbra_add_f32(builder, src.r, umbra_mul_f32(builder, dst.r, inv_a)),
        umbra_add_f32(builder, src.g, umbra_mul_f32(builder, dst.g, inv_a)),
        umbra_add_f32(builder, src.b, umbra_mul_f32(builder, dst.b, inv_a)),
        umbra_add_f32(builder, src.a, umbra_mul_f32(builder, dst.a, inv_a)),
    };
}

umbra_color_val32 umbra_blend_dstover(struct umbra_builder *builder, umbra_color_val32 src, umbra_color_val32 dst) {
    umbra_val32 const one = umbra_imm_f32(builder, 1.0f);
    umbra_val32 const inv_a = umbra_sub_f32(builder, one, dst.a);
    return (umbra_color_val32){
        umbra_add_f32(builder, dst.r, umbra_mul_f32(builder, src.r, inv_a)),
        umbra_add_f32(builder, dst.g, umbra_mul_f32(builder, src.g, inv_a)),
        umbra_add_f32(builder, dst.b, umbra_mul_f32(builder, src.b, inv_a)),
        umbra_add_f32(builder, dst.a, umbra_mul_f32(builder, src.a, inv_a)),
    };
}

umbra_color_val32 umbra_blend_multiply(struct umbra_builder *builder, umbra_color_val32 src, umbra_color_val32 dst) {
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
    return (umbra_color_val32){r, g, b, a};
}

void umbra_gradient_lut_even(float *out, int lut_n, int n_stops, umbra_color const *colors) {
    for (int i = 0; i < lut_n; i++) {
        float const t = (float)i / (float)(lut_n - 1);
        float const seg = t * (float)(n_stops - 1);
        int   idx = (int)seg;
        if (idx >= n_stops - 1) {
            idx = n_stops - 2;
        }
        float const f = seg - (float)idx;
        out[0 * lut_n + i] = colors[idx].r * (1 - f) + colors[idx + 1].r * f;
        out[1 * lut_n + i] = colors[idx].g * (1 - f) + colors[idx + 1].g * f;
        out[2 * lut_n + i] = colors[idx].b * (1 - f) + colors[idx + 1].b * f;
        out[3 * lut_n + i] = colors[idx].a * (1 - f) + colors[idx + 1].a * f;
    }
}

void umbra_gradient_lut(float *out, int lut_n, int n_stops, float const positions[],
                        umbra_color const *colors) {
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
        out[0 * lut_n + i] = colors[seg].r * (1 - f) + colors[seg + 1].r * f;
        out[1 * lut_n + i] = colors[seg].g * (1 - f) + colors[seg + 1].g * f;
        out[2 * lut_n + i] = colors[seg].b * (1 - f) + colors[seg + 1].b * f;
        out[3 * lut_n + i] = colors[seg].a * (1 - f) + colors[seg + 1].a * f;
    }
}
