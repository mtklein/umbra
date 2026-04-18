#include "../include/umbra_draw.h"
#include "assume.h"
#include "flat_ir.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

// Slot index (in 4-byte units) of `field` within an effect struct's own
// uniform buffer.  The effect's uniforms start right after the base, so
// subtract the base size before dividing.  Works for any base type since
// we access it via self->base.
#define SLOT(field) \
    ((int)((__builtin_offsetof(__typeof__(*self), field) - sizeof(self->base)) / 4))

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

// umbra_ptr32 .ix assignment for umbra_draw_builder:
//   buf[0] = destination render target
//   buf[1] = coverage uniforms  (even if no coverage)
//   buf[2] = shader   uniforms  (even if no shader)
enum { DRAW_DST_IX = 0, COVERAGE_IX = 1, SHADER_IX = 2 };

struct umbra_builder* umbra_draw_builder(struct umbra_coverage *coverage,
                                         struct umbra_shader *shader,
                                         umbra_blend_fn blend,
                                         struct umbra_fmt fmt) {
    struct umbra_builder  *builder = umbra_builder();
    umbra_val32 const x = umbra_x(builder);
    umbra_val32 const y = umbra_y(builder);
    umbra_val32 const xf = umbra_f32_from_i32(builder, x);
    umbra_val32 const yf = umbra_f32_from_i32(builder, y);

    umbra_val32 cov = {0};
    if (coverage) {
        cov = coverage->build(coverage, builder, (umbra_ptr32){.ix = COVERAGE_IX}, xf, yf);
    }

    umbra_color_val32 src = {
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
    };
    if (shader) {
        src = shader->build(shader, builder, (umbra_ptr32){.ix = SHADER_IX}, xf, yf);
    }

    umbra_color_val32 dst = {
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
        umbra_imm_f32(builder, 0.0f),
    };
    if (blend || coverage) {
        dst = fmt.load(builder, DRAW_DST_IX);
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

    fmt.store(builder, DRAW_DST_IX, out);

    return builder;
}

struct sdf_coverage {
    struct umbra_coverage base;
    struct umbra_sdf     *sdf;
    int                   hard_edge, :32;
};

static umbra_val32 sdf_as_coverage_build(struct umbra_coverage *s, struct umbra_builder *b,
                                          umbra_ptr32 uniforms,
                                          umbra_val32 x, umbra_val32 y) {
    struct sdf_coverage *self = (struct sdf_coverage *)s;
    umbra_val32 const half = umbra_imm_f32(b, 0.5f);
    umbra_val32 const xc = umbra_add_f32(b, x, half),
                      yc = umbra_add_f32(b, y, half);
    umbra_val32 const f = self->sdf->build(self->sdf, b, uniforms,
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
static void sdf_as_coverage_free(struct umbra_coverage *s) { free(s); }

struct umbra_coverage* umbra_sdf_coverage(struct umbra_sdf *sdf, _Bool hard_edge) {
    struct sdf_coverage *s = malloc(sizeof *s);
    *s = (struct sdf_coverage){
        .base      = {.build    = sdf_as_coverage_build,
                      .free     = sdf_as_coverage_free,
                      .uniforms = sdf->uniforms},
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
};

// TODO: add a second `covered` program alongside `draw` for tiles where the
// SDF is entirely inside the shape.  SDF convention is f<0 inside, so:
//   bounds.hi < 0 -> tile fully covered -> dispatch `covered` (shader+blend at cov=1)
//   bounds.lo < 0 -> some chance of coverage -> dispatch `draw` (current behavior)
//   else          -> skip tile
// `covered` would be built like `draw` but without the SDF coverage adapter,
// skipping per-pixel SDF evaluation entirely.  Prerequisites:
//   - bounds program must also emit f.hi; currently only f.lo is stored
//     (see BOUNDS_DST_IX and umbra_store_32 in umbra_sdf_draw below).
//   - bounds_buf grows a second float* output; allocate/free in _queue.
//   - tile loop in umbra_sdf_draw_queue branches on hi<0 vs lo<0.

// TODO: query per-backend dispatch_granularity instead of this one global
// compromise.  CPU-optimal is ~16-32, GPU-optimal is ~512-1024.
enum { QUEUE_MIN_TILE = 512 };

// Bounds program buf layout:
//   buf[0] = SDF uniforms     (matches sdf_as_coverage_build's COVERAGE_IX=1)
// wait, the draw program has SDF uniforms at COVERAGE_IX=1, SHADER_IX=2.
// For the bounds program we only need the sdf.  We place them:
//   buf[0] = tile-lo output
//   buf[1] = sdf uniforms
//   buf[2] = grid params (base_x, base_y, tile_w, tile_h)
enum { BOUNDS_DST_IX = 0, BOUNDS_SDF_IX = 1, BOUNDS_GRID_IX = 2 };

struct grid_params {
    float base_x, base_y, tile_w, tile_h;
};

struct umbra_sdf_draw* umbra_sdf_draw(struct umbra_backend *be,
                                      struct umbra_sdf *sdf,
                                      struct umbra_sdf_draw_config cfg,
                                      struct umbra_shader *shader,
                                      umbra_blend_fn blend,
                                      struct umbra_fmt fmt) {
    // Build the draw program (shader + SDF coverage + blend).
    struct umbra_coverage *adapter = umbra_sdf_coverage(sdf, cfg.hard_edge);
    struct umbra_builder *db = umbra_draw_builder(adapter, shader, blend, fmt);
    umbra_coverage_free(adapter);
    struct umbra_flat_ir *dir = umbra_flat_ir(db);
    umbra_builder_free(db);
    struct umbra_program *draw = be->compile(be, dir);
    umbra_flat_ir_free(dir);
    if (!draw) { return NULL; }

    // Build the bounds program using sdf->build with tile-extent intervals.
    // x() and y() are tile indices.
    struct umbra_builder *bb = umbra_builder();
    umbra_ptr32 const g = {.ix = BOUNDS_GRID_IX};
    umbra_val32 const base_x = umbra_uniform_32(bb, g, 0),
                      base_y = umbra_uniform_32(bb, g, 1),
                      tile_w = umbra_uniform_32(bb, g, 2),
                      tile_h = umbra_uniform_32(bb, g, 3);
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

    umbra_interval const f = sdf->build(sdf, bb, (umbra_ptr32){.ix = BOUNDS_SDF_IX}, x, y);

    umbra_store_32(bb, (umbra_ptr32){.ix = BOUNDS_DST_IX}, f.lo);

    struct umbra_flat_ir *bir = umbra_flat_ir(bb);
    umbra_builder_free(bb);
    struct umbra_backend *bounds_be = umbra_backend_jit();
    if (!bounds_be) { bounds_be = umbra_backend_interp(); }
    struct umbra_program *bounds = bounds_be->compile(bounds_be, bir);
    umbra_flat_ir_free(bir);

    struct umbra_sdf_draw *d = calloc(1, sizeof *d);
    d->draw      = draw;
    d->bounds    = bounds;
    d->bounds_be = bounds_be;
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

    struct grid_params grid = {
        .base_x = (float)l,
        .base_y = (float)t,
        .tile_w = (float)QUEUE_MIN_TILE,
        .tile_h = (float)QUEUE_MIN_TILE,
    };
    float *lo = calloc((size_t)tiles, sizeof(float));
    struct umbra_buf bounds_buf[] = {
        {.ptr = lo,    .count = tiles,                 .stride = xt},
        buf[COVERAGE_IX],  // sdf uniforms (caller's coverage slot)
        {.ptr = &grid, .count = (int)(sizeof grid / 4)},
    };
    d->bounds->queue(d->bounds, 0, 0, xt, yt, bounds_buf);

    // TODO: coalesce horizontally adjacent covered tiles into one draw->queue() call.

    // TODO: once we have a better handle on the ideal tile shapes (QUEUE_MIN_TILE
    // is currently a global compromise), try compiling per-tile SDF draw programs
    // so the IR can fold in l/t/r/b as constants rather than reading them as
    // uniforms each dispatch.  The SDF's build() currently has no compile-time
    // knowledge of tile bounds — they arrive through the queue() args consumed
    // by umbra_x()/umbra_y() at runtime.  Per-tile specialization would let e.g.
    // axis-aligned SDFs fold the half-plane clips away entirely, and generally
    // shrink the per-pixel work near tile edges.
    //
    // Cost: we'd build-and-cache N (or N*M) variants of the draw program instead
    // of one.  Worth it if tile count is small and per-pixel savings are large.
    // The existing umbra_builder dedup/CSE handles most of the redundancy across
    // variants; the compile path is the dominating unknown.
    //
    // Metal function constants (MTLFunctionConstantValues) are a plausible
    // vehicle here: one source shader with bounds declared as function
    // constants, specialized per-tile at pipeline creation.  Would let the
    // Metal backend share source across tile variants instead of generating
    // N separate MSL strings.  Check whether the Vulkan SPIR-V backend wants
    // the analogous specialization-constant path.
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
}

void umbra_sdf_draw_free(struct umbra_sdf_draw *d) {
    if (d) {
        umbra_program_free(d->draw);
        umbra_program_free(d->bounds);
        umbra_backend_free(d->bounds_be);
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

struct gradient_coords_linear {
    struct umbra_gradient_coords base;
    float                        a, b, c;
    int                          :32;
};

static umbra_val32 gradient_linear_t(struct umbra_gradient_coords *s, struct umbra_builder *b,
                                     umbra_ptr32 uniforms, umbra_point_val32 xy) {
    struct gradient_coords_linear *self = (struct gradient_coords_linear *)s;
    umbra_val32 const a = umbra_uniform_32(b, uniforms, SLOT(a)),
                      bb = umbra_uniform_32(b, uniforms, SLOT(b)),
                      c = umbra_uniform_32(b, uniforms, SLOT(c));
    umbra_val32 const t = umbra_add_f32(b,
                                      umbra_add_f32(b, umbra_mul_f32(b, a, xy.x),
                                                    umbra_mul_f32(b, bb, xy.y)),
                                      c);
    return clamp01(b, t);
}

struct gradient_coords_radial {
    struct umbra_gradient_coords base;
    float                        cx, cy, inv_r;
    int                          :32;
};

static umbra_val32 gradient_radial_t(struct umbra_gradient_coords *s, struct umbra_builder *b,
                                     umbra_ptr32 uniforms, umbra_point_val32 xy) {
    struct gradient_coords_radial *self = (struct gradient_coords_radial *)s;
    umbra_val32 const cx    = umbra_uniform_32(b, uniforms, SLOT(cx)),
                      cy    = umbra_uniform_32(b, uniforms, SLOT(cy)),
                      inv_r = umbra_uniform_32(b, uniforms, SLOT(inv_r));
    umbra_val32 const dx = umbra_sub_f32(b, xy.x, cx),
                      dy = umbra_sub_f32(b, xy.y, cy);
    umbra_val32 const d2 = umbra_add_f32(b, umbra_mul_f32(b, dx, dx),
                                       umbra_mul_f32(b, dy, dy));
    return clamp01(b, umbra_mul_f32(b, umbra_sqrt_f32(b, d2), inv_r));
}

static void generic_coords_free(struct umbra_gradient_coords *s) { free(s); }

void umbra_gradient_coords_free(struct umbra_gradient_coords *s) {
    if (s && s->free) { s->free(s); }
}

struct umbra_gradient_coords* umbra_gradient_linear(umbra_point p0, umbra_point p1) {
    struct gradient_coords_linear *s = malloc(sizeof *s);
    float const dx = p1.x - p0.x,
                dy = p1.y - p0.y,
                L2 = dx*dx + dy*dy,
                a  = dx / L2,
                b  = dy / L2;
    *s = (struct gradient_coords_linear){
        .base = {.t        = gradient_linear_t,
                 .free     = generic_coords_free,
                 .uniforms = UMBRA_UNIFORMS_OF(s)},
        .a = a, .b = b,
        .c = -(a * p0.x + b * p0.y),
    };
    return &s->base;
}

struct umbra_gradient_coords* umbra_gradient_radial(umbra_point center, float radius) {
    struct gradient_coords_radial *s = malloc(sizeof *s);
    *s = (struct gradient_coords_radial){
        .base = {.t        = gradient_radial_t,
                 .free     = generic_coords_free,
                 .uniforms = UMBRA_UNIFORMS_OF(s)},
        .cx = center.x, .cy = center.y, .inv_r = 1.0f / radius,
    };
    return &s->base;
}

static umbra_color_val32 sample_lut(struct umbra_builder *b, umbra_val32 t,
                                    umbra_ptr32 uniforms, int N_slot, umbra_ptr32 lut) {
    umbra_val32 const N_f     = umbra_uniform_32(b, uniforms, N_slot);
    umbra_val32 const N_m1    = umbra_sub_f32(b, N_f, umbra_imm_f32(b, 1.0f));
    umbra_val32 const N_m2    = umbra_sub_f32(b, N_f, umbra_imm_f32(b, 2.0f));
    umbra_val32 const seg     = umbra_mul_f32(b, t, N_m1);
    umbra_val32 const clamped = umbra_min_f32(b, seg, N_m2);
    umbra_val32 const idx     = umbra_floor_i32(b, clamped);
    umbra_val32 const idx1    = umbra_add_i32(b, idx, umbra_imm_i32(b, 1));
    umbra_val32 const frac    = umbra_sub_f32(b, seg, umbra_floor_f32(b, clamped));

    umbra_val32 const N_i = umbra_i32_from_f32(b, N_f);
    umbra_val32 const n2  = umbra_add_i32(b, N_i, N_i);
    umbra_val32 const n3  = umbra_add_i32(b, n2,  N_i);

    umbra_val32 const r0 = umbra_gather_32(b, lut, idx),
                      r1 = umbra_gather_32(b, lut, idx1),
                      g0 = umbra_gather_32(b, lut, umbra_add_i32(b, idx,  N_i)),
                      g1 = umbra_gather_32(b, lut, umbra_add_i32(b, idx1, N_i)),
                      b0 = umbra_gather_32(b, lut, umbra_add_i32(b, idx,  n2)),
                      b1 = umbra_gather_32(b, lut, umbra_add_i32(b, idx1, n2)),
                      a0 = umbra_gather_32(b, lut, umbra_add_i32(b, idx,  n3)),
                      a1 = umbra_gather_32(b, lut, umbra_add_i32(b, idx1, n3));

    return (umbra_color_val32){
        lerp_f(b, r0, r1, frac),
        lerp_f(b, g0, g1, frac),
        lerp_f(b, b0, b1, frac),
        lerp_f(b, a0, a1, frac),
    };
}

static umbra_color_val32 walk_stops(struct umbra_builder *b, umbra_val32 t,
                               umbra_ptr32 uniforms, int N_slot, umbra_ptr32 colors, umbra_ptr32 pos) {
    umbra_val32 const n_i = umbra_i32_from_f32(b,
                                umbra_uniform_32(b, uniforms, N_slot));
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
};

static umbra_color_val32 solid_build(struct umbra_shader *s, struct umbra_builder *b,
                                umbra_ptr32 uniforms,
                                umbra_val32 x, umbra_val32 y) {
    struct shader_solid *self = (struct shader_solid *)s;
    (void)uniforms;
    (void)x;
    (void)y;
    umbra_ptr32 const u = umbra_uniforms(b, &self->color, sizeof self->color / 4);
    return (umbra_color_val32){
        umbra_uniform_32(b, u, 0),
        umbra_uniform_32(b, u, 1),
        umbra_uniform_32(b, u, 2),
        umbra_uniform_32(b, u, 3),
    };
}
static void solid_free(struct umbra_shader *s) { free(s); }

struct umbra_shader* umbra_shader_solid(umbra_color color) {
    struct shader_solid *s = malloc(sizeof *s);
    *s = (struct shader_solid){
        .base  = {.build          = solid_build,
                  .free           = solid_free,
                  .uniforms = UMBRA_UNIFORMS_OF(s)},
        .color = color,
    };
    return &s->base;
}

void umbra_shader_solid_set_color(struct umbra_shader *s, umbra_color color) {
    ((struct shader_solid *)s)->color = color;
}

struct shader_gradient_two_stops {
    struct umbra_shader           base;
    struct umbra_gradient_coords *coords;
    struct umbra_buf              coords_uniforms;
    umbra_color                   c0, c1;
};

static umbra_color_val32 gradient_two_stops_build(struct umbra_shader *s,
                                                  struct umbra_builder *b,
                                                  umbra_ptr32 uniforms,
                                                  umbra_val32 x, umbra_val32 y) {
    struct shader_gradient_two_stops *self = (struct shader_gradient_two_stops *)s;
    umbra_ptr32 const coords_u = umbra_deref_ptr32(b, uniforms, SLOT(coords_uniforms));
    umbra_val32 const t = self->coords->t(self->coords, b, coords_u, (umbra_point_val32){x, y});
    umbra_val32 const r0 = umbra_uniform_32(b, uniforms, SLOT(c0.r)),
                      g0 = umbra_uniform_32(b, uniforms, SLOT(c0.g)),
                      b0 = umbra_uniform_32(b, uniforms, SLOT(c0.b)),
                      a0 = umbra_uniform_32(b, uniforms, SLOT(c0.a)),
                      r1 = umbra_uniform_32(b, uniforms, SLOT(c1.r)),
                      g1 = umbra_uniform_32(b, uniforms, SLOT(c1.g)),
                      b1 = umbra_uniform_32(b, uniforms, SLOT(c1.b)),
                      a1 = umbra_uniform_32(b, uniforms, SLOT(c1.a));
    return (umbra_color_val32){
        lerp_f(b, r0, r1, t),
        lerp_f(b, g0, g1, t),
        lerp_f(b, b0, b1, t),
        lerp_f(b, a0, a1, t),
    };
}
static void gradient_two_stops_free(struct umbra_shader *s) {
    struct shader_gradient_two_stops *self = (struct shader_gradient_two_stops *)s;
    umbra_gradient_coords_free(self->coords);
    free(s);
}

struct umbra_shader* umbra_shader_gradient_two_stops(struct umbra_gradient_coords *coords,
                                                     umbra_color c0, umbra_color c1) {
    struct shader_gradient_two_stops *s = malloc(sizeof *s);
    *s = (struct shader_gradient_two_stops){
        .base            = {.build    = gradient_two_stops_build,
                            .free     = gradient_two_stops_free,
                            .uniforms = UMBRA_UNIFORMS_OF(s)},
        .coords          = coords,
        .coords_uniforms = coords->uniforms,
        .c0 = c0, .c1 = c1,
    };
    return &s->base;
}

struct shader_gradient_lut {
    struct umbra_shader           base;
    struct umbra_gradient_coords *coords;
    struct umbra_buf              coords_uniforms;
    float                         N;
    int                           :32;
    struct umbra_buf              lut;
};

static umbra_color_val32 gradient_lut_build(struct umbra_shader *s, struct umbra_builder *b,
                                            umbra_ptr32 uniforms,
                                            umbra_val32 x, umbra_val32 y) {
    struct shader_gradient_lut *self = (struct shader_gradient_lut *)s;
    umbra_ptr32 const coords_u = umbra_deref_ptr32(b, uniforms, SLOT(coords_uniforms));
    umbra_ptr32 const lut = umbra_deref_ptr32(b, uniforms, SLOT(lut));
    umbra_val32 const t = self->coords->t(self->coords, b, coords_u, (umbra_point_val32){x, y});
    return sample_lut(b, t, uniforms, SLOT(N), lut);
}
static void gradient_lut_free(struct umbra_shader *s) {
    struct shader_gradient_lut *self = (struct shader_gradient_lut *)s;
    umbra_gradient_coords_free(self->coords);
    free(s);
}

struct umbra_shader* umbra_shader_gradient_lut(struct umbra_gradient_coords *coords,
                                               struct umbra_buf lut) {
    struct shader_gradient_lut *s = malloc(sizeof *s);
    *s = (struct shader_gradient_lut){
        .base            = {.build    = gradient_lut_build,
                            .free     = gradient_lut_free,
                            .uniforms = UMBRA_UNIFORMS_OF(s)},
        .coords          = coords,
        .coords_uniforms = coords->uniforms,
        .N               = (float)(lut.count / 4),
        .lut             = lut,
    };
    return &s->base;
}

static umbra_color_val32 gather_even_stops(struct umbra_builder *b, umbra_val32 t,
                                           umbra_ptr32 uniforms, int N_slot,
                                           umbra_ptr32 colors) {
    umbra_val32 const N_f     = umbra_uniform_32(b, uniforms, N_slot);
    umbra_val32 const N_m1    = umbra_sub_f32(b, N_f, umbra_imm_f32(b, 1.0f));
    umbra_val32 const N_m2    = umbra_sub_f32(b, N_f, umbra_imm_f32(b, 2.0f));
    umbra_val32 const seg     = umbra_mul_f32(b, t, N_m1);
    umbra_val32 const clamped = umbra_min_f32(b, seg, N_m2);
    umbra_val32 const idx     = umbra_floor_i32(b, clamped);
    umbra_val32 const idx1    = umbra_add_i32(b, idx, umbra_imm_i32(b, 1));
    umbra_val32 const frac    = umbra_sub_f32(b, seg, umbra_floor_f32(b, clamped));

    umbra_val32 const N_i = umbra_i32_from_f32(b, N_f);
    umbra_val32 const n2  = umbra_add_i32(b, N_i, N_i);
    umbra_val32 const n3  = umbra_add_i32(b, n2,  N_i);

    umbra_val32 const r0 = umbra_gather_32(b, colors, idx),
                      r1 = umbra_gather_32(b, colors, idx1),
                      g0 = umbra_gather_32(b, colors, umbra_add_i32(b, idx,  N_i)),
                      g1 = umbra_gather_32(b, colors, umbra_add_i32(b, idx1, N_i)),
                      b0 = umbra_gather_32(b, colors, umbra_add_i32(b, idx,  n2)),
                      b1 = umbra_gather_32(b, colors, umbra_add_i32(b, idx1, n2)),
                      a0 = umbra_gather_32(b, colors, umbra_add_i32(b, idx,  n3)),
                      a1 = umbra_gather_32(b, colors, umbra_add_i32(b, idx1, n3));

    return (umbra_color_val32){
        lerp_f(b, r0, r1, frac),
        lerp_f(b, g0, g1, frac),
        lerp_f(b, b0, b1, frac),
        lerp_f(b, a0, a1, frac),
    };
}

struct shader_gradient_evenly_spaced_stops {
    struct umbra_shader           base;
    struct umbra_gradient_coords *coords;
    struct umbra_buf              coords_uniforms;
    float                         N;
    int                           :32;
    struct umbra_buf              colors;
};

static umbra_color_val32 gradient_evenly_spaced_stops_build(struct umbra_shader *s,
                                                            struct umbra_builder *b,
                                                            umbra_ptr32 uniforms,
                                                            umbra_val32 x, umbra_val32 y) {
    struct shader_gradient_evenly_spaced_stops *self =
        (struct shader_gradient_evenly_spaced_stops *)s;
    umbra_ptr32 const coords_u = umbra_deref_ptr32(b, uniforms, SLOT(coords_uniforms));
    umbra_ptr32 const colors   = umbra_deref_ptr32(b, uniforms, SLOT(colors));
    umbra_val32 const t = self->coords->t(self->coords, b, coords_u, (umbra_point_val32){x, y});
    return gather_even_stops(b, t, uniforms, SLOT(N), colors);
}
static void gradient_evenly_spaced_stops_free(struct umbra_shader *s) {
    struct shader_gradient_evenly_spaced_stops *self =
        (struct shader_gradient_evenly_spaced_stops *)s;
    umbra_gradient_coords_free(self->coords);
    free(s);
}

struct umbra_shader* umbra_shader_gradient_evenly_spaced_stops(struct umbra_gradient_coords *coords,
                                                               struct umbra_buf colors) {
    struct shader_gradient_evenly_spaced_stops *s = malloc(sizeof *s);
    *s = (struct shader_gradient_evenly_spaced_stops){
        .base            = {.build    = gradient_evenly_spaced_stops_build,
                            .free     = gradient_evenly_spaced_stops_free,
                            .uniforms = UMBRA_UNIFORMS_OF(s)},
        .coords          = coords,
        .coords_uniforms = coords->uniforms,
        .N               = (float)(colors.count / 4),
        .colors          = colors,
    };
    return &s->base;
}

struct shader_gradient {
    struct umbra_shader           base;
    struct umbra_gradient_coords *coords;
    struct umbra_buf              coords_uniforms;
    float                         N;
    int                           :32;
    struct umbra_buf              colors, pos;
};

static umbra_color_val32 gradient_build(struct umbra_shader *s, struct umbra_builder *b,
                                        umbra_ptr32 uniforms,
                                        umbra_val32 x, umbra_val32 y) {
    struct shader_gradient *self = (struct shader_gradient *)s;
    umbra_ptr32 const coords_u = umbra_deref_ptr32(b, uniforms, SLOT(coords_uniforms));
    umbra_ptr32 const colors   = umbra_deref_ptr32(b, uniforms, SLOT(colors));
    umbra_ptr32 const pos      = umbra_deref_ptr32(b, uniforms, SLOT(pos));
    umbra_val32 const t = self->coords->t(self->coords, b, coords_u, (umbra_point_val32){x, y});
    return walk_stops(b, t, uniforms, SLOT(N), colors, pos);
}
static void gradient_free(struct umbra_shader *s) {
    struct shader_gradient *self = (struct shader_gradient *)s;
    umbra_gradient_coords_free(self->coords);
    free(s);
}

struct umbra_shader* umbra_shader_gradient(struct umbra_gradient_coords *coords,
                                           struct umbra_buf colors, struct umbra_buf pos) {
    struct shader_gradient *s = malloc(sizeof *s);
    *s = (struct shader_gradient){
        .base            = {.build    = gradient_build,
                            .free     = gradient_free,
                            .uniforms = UMBRA_UNIFORMS_OF(s)},
        .coords          = coords,
        .coords_uniforms = coords->uniforms,
        .N               = (float)pos.count,
        .colors          = colors,
        .pos             = pos,
    };
    return &s->base;
}

struct shader_supersample {
    struct umbra_shader  base;
    struct umbra_shader *inner;
    int                  samples, :32;
};

static umbra_color_val32 supersample_build(struct umbra_shader *s, struct umbra_builder *builder,
                                      umbra_ptr32 uniforms,
                                      umbra_val32 x, umbra_val32 y) {
    struct shader_supersample *self = (struct shader_supersample *)s;
    static float const jitter[][2] = {
        {-0.375f, -0.125f}, {0.125f, -0.375f}, {0.375f, 0.125f}, {-0.125f, 0.375f},
        {-0.250f, 0.375f},  {0.250f, -0.250f}, {0.375f, 0.250f}, {-0.375f, -0.250f},
    };
    int samples = self->samples;
    if (samples < 1) { samples = 1; }
    if (samples > 8) { samples = 8; }

    umbra_color_val32 sum = self->inner->build(self->inner, builder, uniforms, x, y);
    for (int i = 1; i < samples; i++) {
        umbra_val32 const sx = umbra_add_f32(builder, x,
                                              umbra_imm_f32(builder, jitter[i - 1][0]));
        umbra_val32 const sy = umbra_add_f32(builder, y,
                                              umbra_imm_f32(builder, jitter[i - 1][1]));
        umbra_color_val32 const c = self->inner->build(self->inner, builder, uniforms, sx, sy);
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
static void supersample_free(struct umbra_shader *s) { free(s); }

struct umbra_shader* umbra_shader_supersample(struct umbra_shader *inner, int samples) {
    // supersample forwards its uniform buffer directly to inner, so we adopt
    // inner's uniforms layout (there are no uniforms of our own).
    assume(inner != NULL);
    struct shader_supersample *s = malloc(sizeof *s);
    *s = (struct shader_supersample){
        .base    = {.build    = supersample_build,
                    .free     = supersample_free,
                    .uniforms = inner->uniforms},
        .inner   = inner,
        .samples = samples,
    };
    return &s->base;
}

struct coverage_rect {
    struct umbra_coverage base;
    umbra_rect            rect;
};

static umbra_val32 rect_build(struct umbra_coverage *s, struct umbra_builder *builder,
                               umbra_ptr32 uniforms,
                               umbra_val32 x, umbra_val32 y) {
    struct coverage_rect *self = (struct coverage_rect *)s;
    umbra_val32 const l = umbra_uniform_32(builder, uniforms, SLOT(rect.l));
    umbra_val32 const t = umbra_uniform_32(builder, uniforms, SLOT(rect.t));
    umbra_val32 const r = umbra_uniform_32(builder, uniforms, SLOT(rect.r));
    umbra_val32 const b = umbra_uniform_32(builder, uniforms, SLOT(rect.b));
    umbra_val32 const inside = umbra_and_32(builder,
                                           umbra_and_32(builder, umbra_le_f32(builder, l, x),
                                                         umbra_lt_f32(builder, x, r)),
                                           umbra_and_32(builder, umbra_le_f32(builder, t, y),
                                                         umbra_lt_f32(builder, y, b)));
    umbra_val32 const one_f = umbra_imm_f32(builder, 1.0f);
    umbra_val32 const zero_f = umbra_imm_f32(builder, 0.0f);
    return umbra_sel_32(builder, inside, one_f, zero_f);
}
static void rect_free(struct umbra_coverage *s) { free(s); }

struct umbra_coverage* umbra_coverage_rect(umbra_rect rect) {
    struct coverage_rect *c = malloc(sizeof *c);
    *c = (struct coverage_rect){
        .base = {.build          = rect_build,
                 .free           = rect_free,
                 .uniforms = UMBRA_UNIFORMS_OF(c)},
        .rect = rect,
    };
    return &c->base;
}

struct sdf_rect {
    struct umbra_sdf base;
    umbra_rect       rect;
};

static umbra_interval sdf_rect_build(struct umbra_sdf *s, struct umbra_builder *b,
                                      umbra_ptr32 uniforms,
                                      umbra_interval x, umbra_interval y) {
    struct sdf_rect *self = (struct sdf_rect *)s;
    umbra_interval const l  = umbra_interval_exact(
                                 umbra_uniform_32(b, uniforms, SLOT(rect.l))),
                         t  = umbra_interval_exact(
                                 umbra_uniform_32(b, uniforms, SLOT(rect.t))),
                         r  = umbra_interval_exact(
                                 umbra_uniform_32(b, uniforms, SLOT(rect.r))),
                         bo = umbra_interval_exact(
                                 umbra_uniform_32(b, uniforms, SLOT(rect.b)));
    return umbra_interval_max_f32(b, umbra_interval_max_f32(b, umbra_interval_sub_f32(b, l, x),
                                                               umbra_interval_sub_f32(b, x, r)),
                                    umbra_interval_max_f32(b, umbra_interval_sub_f32(b, t, y),
                                                              umbra_interval_sub_f32(b, y, bo)));
}
static void sdf_rect_free(struct umbra_sdf *s) { free(s); }

struct umbra_sdf* umbra_sdf_rect(umbra_rect rect) {
    struct sdf_rect *s = malloc(sizeof *s);
    *s = (struct sdf_rect){
        .base = {.build          = sdf_rect_build,
                 .free           = sdf_rect_free,
                 .uniforms = UMBRA_UNIFORMS_OF(s)},
        .rect = rect,
    };
    return &s->base;
}

struct coverage_bitmap {
    struct umbra_coverage base;
    struct umbra_buf      bmp;
};

static umbra_val32 bitmap_build(struct umbra_coverage *s, struct umbra_builder *builder,
                                 umbra_ptr32 uniforms,
                                 umbra_val32 x, umbra_val32 y) {
    struct coverage_bitmap *self = (struct coverage_bitmap *)s;
    (void)x;
    (void)y;
    umbra_ptr16 const bmp = umbra_deref_ptr16(builder, uniforms,
                                              SLOT(bmp));
    umbra_val32 const val = umbra_i32_from_s16(builder, umbra_load_16(builder, bmp));
    umbra_val32 const inv255 = umbra_imm_f32(builder, 1.0f / 255.0f);
    return umbra_mul_f32(builder, umbra_f32_from_i32(builder, val), inv255);
}
static void bitmap_free(struct umbra_coverage *s) { free(s); }

struct umbra_coverage* umbra_coverage_bitmap(struct umbra_buf bmp) {
    struct coverage_bitmap *c = malloc(sizeof *c);
    *c = (struct coverage_bitmap){
        .base = {.build          = bitmap_build,
                 .free           = bitmap_free,
                 .uniforms = UMBRA_UNIFORMS_OF(c)},
        .bmp  = bmp,
    };
    return &c->base;
}

struct coverage_sdf {
    struct umbra_coverage base;
    struct umbra_buf      bmp;
};

static umbra_val32 cov_sdf_build(struct umbra_coverage *s, struct umbra_builder *builder,
                                  umbra_ptr32 uniforms,
                                  umbra_val32 x, umbra_val32 y) {
    struct coverage_sdf *self = (struct coverage_sdf *)s;
    (void)x;
    (void)y;
    umbra_ptr16 const bmp = umbra_deref_ptr16(builder, uniforms,
                                              SLOT(bmp));
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
static void cov_sdf_free(struct umbra_coverage *s) { free(s); }

struct umbra_coverage* umbra_coverage_sdf(struct umbra_buf bmp) {
    struct coverage_sdf *c = malloc(sizeof *c);
    *c = (struct coverage_sdf){
        .base = {.build          = cov_sdf_build,
                 .free           = cov_sdf_free,
                 .uniforms = UMBRA_UNIFORMS_OF(c)},
        .bmp  = bmp,
    };
    return &c->base;
}

struct coverage_winding {
    struct umbra_coverage base;
    struct umbra_buf      winding;
};

static umbra_val32 winding_build(struct umbra_coverage *s, struct umbra_builder *builder,
                               umbra_ptr32 uniforms,
                               umbra_val32 x, umbra_val32 y) {
    struct coverage_winding *self = (struct coverage_winding *)s;
    (void)x;
    (void)y;
    umbra_ptr32 const w = umbra_deref_ptr32(builder, uniforms,
                                            SLOT(winding));
    umbra_val32 const raw = umbra_load_32(builder, w);
    return umbra_min_f32(builder, umbra_abs_f32(builder, raw),
                         umbra_imm_f32(builder, 1.0f));
}
static void winding_free(struct umbra_coverage *s) { free(s); }

struct umbra_coverage* umbra_coverage_winding(struct umbra_buf wind) {
    struct coverage_winding *c = malloc(sizeof *c);
    *c = (struct coverage_winding){
        .base    = {.build          = winding_build,
                    .free           = winding_free,
                    .uniforms = UMBRA_UNIFORMS_OF(c)},
        .winding = wind,
    };
    return &c->base;
}

struct coverage_bitmap_matrix {
    struct umbra_coverage base;
    struct umbra_matrix   mat; int :32;
    struct umbra_bitmap   bmp;
};

static umbra_val32 bitmap_matrix_build(struct umbra_coverage *s, struct umbra_builder *builder,
                                        umbra_ptr32 uniforms,
                                        umbra_val32 x, umbra_val32 y) {
    struct coverage_bitmap_matrix *self = (struct coverage_bitmap_matrix *)s;
    umbra_ptr16 const bmp = umbra_deref_ptr16(builder, uniforms, SLOT(bmp.buf));

    umbra_val32 const m0 = umbra_uniform_32(builder, uniforms, SLOT(mat.sx));
    umbra_val32 const m1 = umbra_uniform_32(builder, uniforms, SLOT(mat.kx));
    umbra_val32 const m2 = umbra_uniform_32(builder, uniforms, SLOT(mat.tx));
    umbra_val32 const m3 = umbra_uniform_32(builder, uniforms, SLOT(mat.ky));
    umbra_val32 const m4 = umbra_uniform_32(builder, uniforms, SLOT(mat.sy));
    umbra_val32 const m5 = umbra_uniform_32(builder, uniforms, SLOT(mat.ty));
    umbra_val32 const m6 = umbra_uniform_32(builder, uniforms, SLOT(mat.p0));
    umbra_val32 const m7 = umbra_uniform_32(builder, uniforms, SLOT(mat.p1));
    umbra_val32 const m8 = umbra_uniform_32(builder, uniforms, SLOT(mat.p2));
    umbra_val32 const bw = umbra_f32_from_i32(builder,
                                              umbra_uniform_32(builder, uniforms, SLOT(bmp.w)));
    umbra_val32 const bh = umbra_f32_from_i32(builder,
                                              umbra_uniform_32(builder, uniforms, SLOT(bmp.h)));

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
static void bitmap_matrix_free(struct umbra_coverage *s) { free(s); }

struct umbra_coverage* umbra_coverage_bitmap_matrix(struct umbra_matrix mat,
                                                    struct umbra_bitmap bmp) {
    struct coverage_bitmap_matrix *c = malloc(sizeof *c);
    *c = (struct coverage_bitmap_matrix){
        .base = {.build          = bitmap_matrix_build,
                 .free           = bitmap_matrix_free,
                 .uniforms = UMBRA_UNIFORMS_OF(c)},
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
