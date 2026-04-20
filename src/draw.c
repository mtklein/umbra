#include "../include/umbra_draw.h"
#include "assume.h"
#include "flat_ir.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

static umbra_val32 pack_unorm(struct umbra_builder *b, umbra_val32 ch, umbra_val32 scale) {
    umbra_val32 const zero = umbra_imm_f32(b, 0.0f),
                       one = umbra_imm_f32(b, 1.0f);
    return umbra_round_i32(b, umbra_mul_f32(b,
        umbra_min_f32(b, umbra_max_f32(b, ch, zero), one), scale));
}

umbra_color_val32 umbra_load_8888(struct umbra_builder *b, umbra_ptr src) {
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
void umbra_store_8888(struct umbra_builder *b, umbra_ptr dst, umbra_color_val32 c) {
    umbra_val32 const s = umbra_imm_f32(b, 255.0f);
    umbra_store_8x4(b, dst, pack_unorm(b, c.r, s), pack_unorm(b, c.g, s),
                            pack_unorm(b, c.b, s), pack_unorm(b, c.a, s));
}

umbra_color_val32 umbra_load_565(struct umbra_builder *b, umbra_ptr src) {
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
void umbra_store_565(struct umbra_builder *b, umbra_ptr dst, umbra_color_val32 c) {
    umbra_val32 px = pack_unorm(b, c.b, umbra_imm_f32(b, 31.0f));
    px = umbra_or_32(b, px, umbra_shl_i32(b, pack_unorm(b, c.g, umbra_imm_f32(b, 63.0f)),
                                            umbra_imm_i32(b, 5)));
    px = umbra_or_32(b, px, umbra_shl_i32(b, pack_unorm(b, c.r, umbra_imm_f32(b, 31.0f)),
                                            umbra_imm_i32(b, 11)));
    umbra_store_16(b, dst, umbra_i16_from_i32(b, px));
}

umbra_color_val32 umbra_load_1010102(struct umbra_builder *b, umbra_ptr src) {
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
void umbra_store_1010102(struct umbra_builder *b, umbra_ptr dst, umbra_color_val32 c) {
    umbra_val32 const s10 = umbra_imm_f32(b, 1023.0f);
    umbra_val32 px = pack_unorm(b, c.r, s10);
    px = umbra_or_32(b, px, umbra_shl_i32(b, pack_unorm(b, c.g, s10), umbra_imm_i32(b, 10)));
    px = umbra_or_32(b, px, umbra_shl_i32(b, pack_unorm(b, c.b, s10), umbra_imm_i32(b, 20)));
    px = umbra_or_32(b, px, umbra_shl_i32(b, pack_unorm(b, c.a, umbra_imm_f32(b, 3.0f)),
                                            umbra_imm_i32(b, 30)));
    umbra_store_32(b, dst, px);
}

umbra_color_val32 umbra_load_fp16(struct umbra_builder *b, umbra_ptr src) {
    umbra_val16 r, g, bl, a;
    umbra_load_16x4(b, src, &r, &g, &bl, &a);
    return (umbra_color_val32){
        umbra_f32_from_f16(b, r), umbra_f32_from_f16(b, g),
        umbra_f32_from_f16(b, bl), umbra_f32_from_f16(b, a),
    };
}
void umbra_store_fp16(struct umbra_builder *b, umbra_ptr dst, umbra_color_val32 c) {
    umbra_store_16x4(b, dst, umbra_f16_from_f32(b, c.r), umbra_f16_from_f32(b, c.g),
                             umbra_f16_from_f32(b, c.b), umbra_f16_from_f32(b, c.a));
}

umbra_color_val32 umbra_load_fp16_planar(struct umbra_builder *b, umbra_ptr src) {
    umbra_val16 r, g, bl, a;
    umbra_load_16x4_planar(b, src, &r, &g, &bl, &a);
    return (umbra_color_val32){
        umbra_f32_from_f16(b, r), umbra_f32_from_f16(b, g),
        umbra_f32_from_f16(b, bl), umbra_f32_from_f16(b, a),
    };
}
void umbra_store_fp16_planar(struct umbra_builder *b, umbra_ptr dst, umbra_color_val32 c) {
    umbra_store_16x4_planar(b, dst, umbra_f16_from_f32(b, c.r), umbra_f16_from_f32(b, c.g),
                                    umbra_f16_from_f32(b, c.b), umbra_f16_from_f32(b, c.a));
}

struct umbra_fmt const umbra_fmt_8888 = {
    .name="8888", .bpp=4, .planes=1,
    .load=umbra_load_8888, .store=umbra_store_8888,
};
struct umbra_fmt const umbra_fmt_565 = {
    .name="565", .bpp=2, .planes=1,
    .load=umbra_load_565, .store=umbra_store_565,
};
struct umbra_fmt const umbra_fmt_1010102 = {
    .name="1010102", .bpp=4, .planes=1,
    .load=umbra_load_1010102, .store=umbra_store_1010102,
};
struct umbra_fmt const umbra_fmt_fp16 = {
    .name="fp16", .bpp=8, .planes=1,
    .load=umbra_load_fp16, .store=umbra_store_fp16,
};
struct umbra_fmt const umbra_fmt_fp16_planar = {
    .name="fp16_planar", .bpp=2, .planes=4,
    .load=umbra_load_fp16_planar, .store=umbra_store_fp16_planar,
};

void umbra_build_draw(struct umbra_builder *b,
                      umbra_ptr dst_ptr, struct umbra_fmt fmt,
                      umbra_val32 x, umbra_val32 y,
                      umbra_coverage coverage_fn, void *coverage_ctx,
                      umbra_shader   shader_fn,   void *shader_ctx,
                      umbra_blend    blend_fn,    void *blend_ctx)
{
    umbra_val32 coverage = {0};
    if (coverage_fn) { coverage = coverage_fn(coverage_ctx, b, x, y); }

    umbra_val32 const zero = umbra_imm_f32(b, 0.0f);
    umbra_color_val32 src = {zero, zero, zero, zero};
    if (shader_fn) { src = shader_fn(shader_ctx, b, x, y); }

    umbra_color_val32 dst = {zero, zero, zero, zero};
    if (blend_fn || coverage_fn) { dst = fmt.load(b, dst_ptr); }

    umbra_color_val32 out = blend_fn ? blend_fn(blend_ctx, b, src, dst) : src;

    if (coverage_fn) {
        out.r = umbra_add_f32(b, dst.r,
                              umbra_mul_f32(b, umbra_sub_f32(b, out.r, dst.r), coverage));
        out.g = umbra_add_f32(b, dst.g,
                              umbra_mul_f32(b, umbra_sub_f32(b, out.g, dst.g), coverage));
        out.b = umbra_add_f32(b, dst.b,
                              umbra_mul_f32(b, umbra_sub_f32(b, out.b, dst.b), coverage));
        out.a = umbra_add_f32(b, dst.a,
                              umbra_mul_f32(b, umbra_sub_f32(b, out.a, dst.a), coverage));
    }

    fmt.store(b, dst_ptr, out);
}

struct umbra_builder* umbra_draw_builder(
    struct umbra_matrix const *transform_mat,
    umbra_coverage  coverage_fn, void *coverage_ctx,
    umbra_shader    shader_fn,   void *shader_ctx,
    umbra_blend     blend_fn,    void *blend_ctx,
    struct umbra_buf *dst_buf,
    struct umbra_fmt  fmt)
{
    struct umbra_builder *b = umbra_builder();
    umbra_ptr const dst_ptr = umbra_bind_buf(b, dst_buf);
    umbra_val32 x = umbra_f32_from_i32(b, umbra_x(b)),
                y = umbra_f32_from_i32(b, umbra_y(b));
    if (transform_mat) {
        umbra_point_val32 const p = umbra_transform_perspective(transform_mat, b, x, y);
        x = p.x;
        y = p.y;
    }
    umbra_build_draw(b, dst_ptr, fmt, x, y,
                     coverage_fn, coverage_ctx,
                     shader_fn,   shader_ctx,
                     blend_fn,    blend_ctx);
    return b;
}

enum {
    M_SX = (int)(__builtin_offsetof(struct umbra_matrix, sx) / 4),
    M_KX = (int)(__builtin_offsetof(struct umbra_matrix, kx) / 4),
    M_TX = (int)(__builtin_offsetof(struct umbra_matrix, tx) / 4),
    M_KY = (int)(__builtin_offsetof(struct umbra_matrix, ky) / 4),
    M_SY = (int)(__builtin_offsetof(struct umbra_matrix, sy) / 4),
    M_TY = (int)(__builtin_offsetof(struct umbra_matrix, ty) / 4),
    M_P0 = (int)(__builtin_offsetof(struct umbra_matrix, p0) / 4),
    M_P1 = (int)(__builtin_offsetof(struct umbra_matrix, p1) / 4),
    M_P2 = (int)(__builtin_offsetof(struct umbra_matrix, p2) / 4),
};

umbra_point_val32 umbra_transform_perspective(struct umbra_matrix const *mat,
                                              struct umbra_builder *b,
                                              umbra_val32 x, umbra_val32 y) {
    umbra_ptr const u  = umbra_bind_uniforms(b, mat, (int)(sizeof *mat / 4));
    umbra_val32 const sx = umbra_uniform_32(b, u, M_SX),
                      kx = umbra_uniform_32(b, u, M_KX),
                      tx = umbra_uniform_32(b, u, M_TX),
                      ky = umbra_uniform_32(b, u, M_KY),
                      sy = umbra_uniform_32(b, u, M_SY),
                      ty = umbra_uniform_32(b, u, M_TY),
                      p0 = umbra_uniform_32(b, u, M_P0),
                      p1 = umbra_uniform_32(b, u, M_P1),
                      p2 = umbra_uniform_32(b, u, M_P2);
    umbra_val32 const w =
        umbra_add_f32(b, umbra_add_f32(b, umbra_mul_f32(b, p0, x),
                                          umbra_mul_f32(b, p1, y)),
                         p2);
    umbra_val32 const xp =
        umbra_div_f32(b, umbra_add_f32(b, umbra_add_f32(b, umbra_mul_f32(b, sx, x),
                                                           umbra_mul_f32(b, kx, y)),
                                          tx),
                         w);
    umbra_val32 const yp =
        umbra_div_f32(b, umbra_add_f32(b, umbra_add_f32(b, umbra_mul_f32(b, ky, x),
                                                           umbra_mul_f32(b, sy, y)),
                                          ty),
                         w);
    return (umbra_point_val32){xp, yp};
}

// Interval affine transform, used only by umbra_sdf_draw's bounds program.
// Interval perspective is unavailable by design: umbra_interval_div_f32
// cannot safely handle a zero-straddling divisor and we don't expect to
// tile-dispatch anything but affine transforms.  Callers gate on the matrix
// being affine (p0 == p1 == 0 && p2 == 1) and route through this helper,
// which just emits the six-parameter affine form -- no divide.
static void transform_affine_interval(struct umbra_matrix const *mat,
                                      struct umbra_builder *b,
                                      umbra_interval *x, umbra_interval *y) {
    umbra_ptr const u = umbra_bind_uniforms(b, mat, (int)(sizeof *mat / 4));
    umbra_interval const sx = umbra_interval_exact(umbra_uniform_32(b, u, M_SX)),
                         kx = umbra_interval_exact(umbra_uniform_32(b, u, M_KX)),
                         tx = umbra_interval_exact(umbra_uniform_32(b, u, M_TX)),
                         ky = umbra_interval_exact(umbra_uniform_32(b, u, M_KY)),
                         sy = umbra_interval_exact(umbra_uniform_32(b, u, M_SY)),
                         ty = umbra_interval_exact(umbra_uniform_32(b, u, M_TY));
    umbra_interval const xp =
        umbra_interval_add_f32(b, umbra_interval_add_f32(b, umbra_interval_mul_f32(b, sx, *x),
                                                            umbra_interval_mul_f32(b, kx, *y)),
                                  tx);
    umbra_interval const yp =
        umbra_interval_add_f32(b, umbra_interval_add_f32(b, umbra_interval_mul_f32(b, ky, *x),
                                                            umbra_interval_mul_f32(b, sy, *y)),
                                  ty);
    *x = xp;
    *y = yp;
}

umbra_val32 umbra_coverage_from_sdf(void *ctx, struct umbra_builder *b,
                                     umbra_val32 x, umbra_val32 y) {
    struct umbra_coverage_from_sdf const *self = ctx;
    umbra_val32 const half = umbra_imm_f32(b, 0.5f);
    umbra_val32 const xc = umbra_add_f32(b, x, half),
                      yc = umbra_add_f32(b, y, half);
    umbra_val32 const f = self->sdf_fn(self->sdf_ctx, b,
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

void umbra_build_sdf_draw(struct umbra_builder *b,
                          umbra_ptr dst_ptr, struct umbra_fmt dst_fmt,
                          umbra_val32 x, umbra_val32 y,
                          umbra_sdf sdf_fn, void *sdf_ctx, int aa_quality,
                          umbra_shader shader_fn, void *shader_ctx,
                          umbra_blend  blend_fn,  void *blend_ctx) {
    struct umbra_coverage_from_sdf cov = {
        .sdf_fn    = sdf_fn,
        .sdf_ctx   = sdf_ctx,
        .hard_edge = aa_quality == 0,
    };
    umbra_build_draw(b, dst_ptr, dst_fmt, x, y,
                     umbra_coverage_from_sdf, &cov,
                     shader_fn, shader_ctx,
                     blend_fn,  blend_ctx);
}

void umbra_build_sdf_bounds(struct umbra_builder *b,
                            umbra_ptr cov,
                            umbra_interval x, umbra_interval y,
                            umbra_sdf sdf_fn, void *sdf_ctx) {
    umbra_interval const f = sdf_fn(sdf_ctx, b, x, y);
    umbra_val32 const zero_f = umbra_imm_f32(b, 0.0f);
    umbra_val32 const partial = umbra_lt_f32(b, f.lo, zero_f),
                      full    = umbra_lt_f32(b, f.hi, zero_f);
    umbra_val32 const none_i    = umbra_imm_i32(b, UMBRA_SDF_TILE_NONE),
                      partial_i = umbra_imm_i32(b, UMBRA_SDF_TILE_PARTIAL),
                      full_i    = umbra_imm_i32(b, UMBRA_SDF_TILE_FULL);
    umbra_val32 const base = umbra_sel_32(b, partial, partial_i, none_i);
    umbra_val32 const tri  = umbra_sel_32(b, full,    full_i,    base);
    umbra_store_16(b, cov, umbra_i16_from_i32(b, tri));
}

// TODO: add a second `covered` program alongside `draw` for tiles where the
// SDF is entirely inside the shape.  bounds already tags such tiles as
// UMBRA_SDF_TILE_FULL; the fast path dispatches `covered` (shader+blend at
// cov=1, no per-pixel SDF eval) instead of `draw`.  `covered` would be built
// like `draw` but with umbra_build_draw (no SDF coverage adapter).

static _Bool matrix_is_affine(struct umbra_matrix const *m) {
    return m->p0 == 0.0f && m->p1 == 0.0f && m->p2 == 1.0f;
}

void umbra_sdf_tile_intervals(struct umbra_builder *bb,
                              struct umbra_sdf_grid *grid,
                              struct umbra_matrix const *transform_mat,
                              umbra_interval *ix, umbra_interval *iy) {
    umbra_ptr const g = umbra_bind_uniforms(bb, grid, (int)(sizeof *grid / 4));
    umbra_val32 const base_x = umbra_uniform_32(bb, g, 0),
                      base_y = umbra_uniform_32(bb, g, 1),
                      tile_w = umbra_uniform_32(bb, g, 2),
                      tile_h = umbra_uniform_32(bb, g, 3);
    umbra_val32 const xf = umbra_f32_from_i32(bb, umbra_x(bb)),
                      yf = umbra_f32_from_i32(bb, umbra_y(bb));
    umbra_val32 const one = umbra_imm_f32(bb, 1.0f);
    *ix = (umbra_interval){
        umbra_add_f32(bb, base_x, umbra_mul_f32(bb, xf, tile_w)),
        umbra_add_f32(bb, base_x, umbra_mul_f32(bb, umbra_add_f32(bb, xf, one), tile_w)),
    };
    *iy = (umbra_interval){
        umbra_add_f32(bb, base_y, umbra_mul_f32(bb, yf, tile_h)),
        umbra_add_f32(bb, base_y, umbra_mul_f32(bb, umbra_add_f32(bb, yf, one), tile_h)),
    };
    if (transform_mat && matrix_is_affine(transform_mat)) {
        transform_affine_interval(transform_mat, bb, ix, iy);
    }
}

void umbra_sdf_dispatch(struct umbra_program *bounds,
                        struct umbra_program *draw,
                        struct umbra_sdf_grid *grid,
                        struct umbra_buf *cov,
                        int tile_size, int l, int t, int r, int b) {
    if (!bounds) {
        draw->queue(draw, l, t, r, b);
        return;
    }
    int const T  = tile_size,
              xt = (r - l + T - 1) / T,
              yt = (b - t + T - 1) / T,
              tiles = xt * yt;

    *grid = (struct umbra_sdf_grid){
        .base_x = (float)l, .base_y = (float)t,
        .tile_w = (float)T, .tile_h = (float)T,
    };
    cov->count  = tiles;
    cov->stride = xt;
    __builtin_memset(cov->ptr, 0, (size_t)tiles * sizeof(uint16_t));
    bounds->queue(bounds, 0, 0, xt, yt);
    uint16_t const *c = cov->ptr;

    // TODO: coalesce horizontally adjacent covered tiles into one draw->queue() call.

    // TODO: once we have a better handle on the ideal tile shapes (tile_size
    // is still a global compromise), try compiling per-tile SDF draw programs
    // so the IR can fold in l/t/r/b as constants rather than reading them as
    // uniforms each dispatch.  Per-tile specialization would let e.g. axis-
    // aligned SDFs fold their half-plane clips away entirely, and generally
    // shrink the per-pixel work near tile edges.
    //
    // Cost: build-and-cache N (or N*M) variants of the draw program instead
    // of one.  Worth it if tile count is small and per-pixel savings are
    // large.  Existing builder dedup/CSE handles most of the redundancy
    // across variants; the compile path is the dominating unknown.
    //
    // Metal function constants (MTLFunctionConstantValues) are a plausible
    // vehicle here: one source shader with bounds as function constants,
    // specialized per-tile at pipeline creation.  Vulkan/SPIR-V has
    // analogous specialization constants.
    for (int ty = 0; ty < yt; ty++) {
        for (int tx = 0; tx < xt; tx++) {
            if (c[ty * xt + tx] != UMBRA_SDF_TILE_NONE) {
                int const tl = l + tx * T,
                          tt = t + ty * T,
                          tr = tl + T < r ? tl + T : r,
                          tb = tt + T < b ? tt + T : b;
                draw->queue(draw, tl, tt, tr, tb);
            }
        }
    }
}

// TODO: query per-backend dispatch_granularity instead of this one global
// compromise.  CPU-optimal is ~16-32, GPU-optimal is ~512-1024.
enum { QUEUE_MIN_TILE = 512 };

struct umbra_sdf_draw {
    struct umbra_program *draw;
    struct umbra_program *bounds;
    struct umbra_backend *bounds_be;   // owns its own jit backend for bounds
    struct umbra_sdf_grid grid;
    struct umbra_buf      cov_buf;
    struct umbra_buf      draw_dst_buf;
    uint16_t             *cov;
    int                   cov_cap;
    int                   :32;
};

struct umbra_sdf_draw* umbra_sdf_draw(struct umbra_backend *be,
                                      struct umbra_matrix const *transform_mat,
                                      umbra_sdf sdf_fn, void *sdf_ctx,
                                      _Bool hard_edge,
                                      umbra_shader shader_fn, void *shader_ctx,
                                      umbra_blend blend_fn, void *blend_ctx,
                                      struct umbra_fmt fmt) {
    struct umbra_sdf_draw *w = calloc(1, sizeof *w);

    // Draw program: caller coords -> optional transform -> sdf coverage + shader + blend.
    {
        struct umbra_builder *db = umbra_builder();
        umbra_ptr const dst_ptr = umbra_bind_buf(db, &w->draw_dst_buf);
        umbra_val32 x = umbra_f32_from_i32(db, umbra_x(db)),
                    y = umbra_f32_from_i32(db, umbra_y(db));
        if (transform_mat) {
            umbra_point_val32 const p = umbra_transform_perspective(transform_mat, db, x, y);
            x = p.x;
            y = p.y;
        }
        umbra_build_sdf_draw(db, dst_ptr, fmt, x, y,
                             sdf_fn, sdf_ctx, hard_edge ? 0 : 1,
                             shader_fn, shader_ctx,
                             blend_fn,  blend_ctx);
        struct umbra_flat_ir *ir = umbra_flat_ir(db);
        umbra_builder_free(db);
        w->draw = be->compile(be, ir);
        umbra_flat_ir_free(ir);
        if (!w->draw) { free(w); return NULL; }
    }

    // Bounds program (skipped for perspective: interval divide is unsound).
    if (!transform_mat || matrix_is_affine(transform_mat)) {
        struct umbra_builder *bb = umbra_builder();
        umbra_ptr const cov_ptr = umbra_bind_buf(bb, &w->cov_buf);
        umbra_interval ix, iy;
        umbra_sdf_tile_intervals(bb, &w->grid, transform_mat, &ix, &iy);
        umbra_build_sdf_bounds(bb, cov_ptr, ix, iy, sdf_fn, sdf_ctx);
        struct umbra_flat_ir *ir = umbra_flat_ir(bb);
        umbra_builder_free(bb);
        struct umbra_backend *bounds_be = umbra_backend_jit();
        if (!bounds_be) { bounds_be = umbra_backend_interp(); }
        w->bounds    = bounds_be->compile(bounds_be, ir);
        w->bounds_be = bounds_be;
        umbra_flat_ir_free(ir);
    }
    return w;
}

void umbra_sdf_draw_queue(struct umbra_sdf_draw *w,
                          int l, int t, int r, int b, struct umbra_buf dst) {
    if (!w) { return; }
    w->draw_dst_buf = dst;
    int const T  = QUEUE_MIN_TILE,
              xt = (r - l + T - 1) / T,
              yt = (b - t + T - 1) / T,
              tiles = xt * yt;
    if (tiles > w->cov_cap) {
        w->cov     = realloc(w->cov, (size_t)tiles * sizeof *w->cov);
        w->cov_cap = tiles;
    }
    w->cov_buf = (struct umbra_buf){.ptr = w->cov, .count = tiles, .stride = xt};
    umbra_sdf_dispatch(w->bounds, w->draw, &w->grid, &w->cov_buf, T, l, t, r, b);
}

void umbra_sdf_draw_free(struct umbra_sdf_draw *w) {
    if (w) {
        umbra_program_free(w->draw);
        umbra_program_free(w->bounds);
        umbra_backend_free(w->bounds_be);
        free(w->cov);
        free(w);
    }
}

umbra_color_val32 umbra_shader_color(void *ctx, struct umbra_builder *b,
                                     umbra_val32 x, umbra_val32 y) {
    umbra_color const *self = ctx;
    (void)x; (void)y;
    umbra_ptr const u = umbra_bind_uniforms(b, self, sizeof *self / 4);
    return (umbra_color_val32){
        umbra_uniform_32(b, u, 0),
        umbra_uniform_32(b, u, 1),
        umbra_uniform_32(b, u, 2),
        umbra_uniform_32(b, u, 3),
    };
}

umbra_color_val32 umbra_shader_supersample(void *ctx, struct umbra_builder *b,
                                            umbra_val32 x, umbra_val32 y) {
    struct umbra_supersample const *self = ctx;
    static float const jitter[][2] = {
        {-0.375f, -0.125f}, {0.125f, -0.375f}, {0.375f, 0.125f}, {-0.125f, 0.375f},
        {-0.250f, 0.375f},  {0.250f, -0.250f}, {0.375f, 0.250f}, {-0.375f, -0.250f},
    };
    int samples = self->samples;
    if (samples < 1) { samples = 1; }
    if (samples > 8) { samples = 8; }

    umbra_color_val32 sum = self->inner_fn(self->inner_ctx, b, x, y);
    for (int i = 1; i < samples; i++) {
        umbra_val32 const sx = umbra_add_f32(b, x,
                                              umbra_imm_f32(b, jitter[i - 1][0])),
                          sy = umbra_add_f32(b, y,
                                              umbra_imm_f32(b, jitter[i - 1][1]));
        umbra_color_val32 const c = self->inner_fn(self->inner_ctx, b, sx, sy);
        sum.r = umbra_add_f32(b, sum.r, c.r);
        sum.g = umbra_add_f32(b, sum.g, c.g);
        sum.b = umbra_add_f32(b, sum.b, c.b);
        sum.a = umbra_add_f32(b, sum.a, c.a);
    }

    umbra_val32 const inv = umbra_imm_f32(b, 1.0f / (float)samples);
    return (umbra_color_val32){
        umbra_mul_f32(b, sum.r, inv),
        umbra_mul_f32(b, sum.g, inv),
        umbra_mul_f32(b, sum.b, inv),
        umbra_mul_f32(b, sum.a, inv),
    };
}

umbra_val32 umbra_coverage_rect(void *ctx, struct umbra_builder *b,
                                 umbra_val32 x, umbra_val32 y) {
    umbra_rect const *self = ctx;
    umbra_ptr const u = umbra_bind_uniforms(b, self, sizeof *self / 4);
    umbra_val32 const l = umbra_uniform_32(b, u, 0),
                      t = umbra_uniform_32(b, u, 1),
                      r = umbra_uniform_32(b, u, 2),
                      bot = umbra_uniform_32(b, u, 3);
    umbra_val32 const inside = umbra_and_32(b,
                                    umbra_and_32(b, umbra_le_f32(b, l, x),
                                                     umbra_lt_f32(b, x, r)),
                                    umbra_and_32(b, umbra_le_f32(b, t, y),
                                                     umbra_lt_f32(b, y, bot)));
    umbra_val32 const one_f  = umbra_imm_f32(b, 1.0f),
                      zero_f = umbra_imm_f32(b, 0.0f);
    return umbra_sel_32(b, inside, one_f, zero_f);
}

umbra_color_val32 umbra_blend_src(void *ctx, struct umbra_builder *builder,
                                  umbra_color_val32 src, umbra_color_val32 dst) {
    (void)ctx;
    (void)builder;
    (void)dst;
    return src;
}

umbra_color_val32 umbra_blend_srcover(void *ctx, struct umbra_builder *builder,
                                      umbra_color_val32 src, umbra_color_val32 dst) {
    (void)ctx;
    umbra_val32 const one = umbra_imm_f32(builder, 1.0f);
    umbra_val32 const inv_a = umbra_sub_f32(builder, one, src.a);
    return (umbra_color_val32){
        umbra_add_f32(builder, src.r, umbra_mul_f32(builder, dst.r, inv_a)),
        umbra_add_f32(builder, src.g, umbra_mul_f32(builder, dst.g, inv_a)),
        umbra_add_f32(builder, src.b, umbra_mul_f32(builder, dst.b, inv_a)),
        umbra_add_f32(builder, src.a, umbra_mul_f32(builder, dst.a, inv_a)),
    };
}

umbra_color_val32 umbra_blend_dstover(void *ctx, struct umbra_builder *builder,
                                      umbra_color_val32 src, umbra_color_val32 dst) {
    (void)ctx;
    umbra_val32 const one = umbra_imm_f32(builder, 1.0f);
    umbra_val32 const inv_a = umbra_sub_f32(builder, one, dst.a);
    return (umbra_color_val32){
        umbra_add_f32(builder, dst.r, umbra_mul_f32(builder, src.r, inv_a)),
        umbra_add_f32(builder, dst.g, umbra_mul_f32(builder, src.g, inv_a)),
        umbra_add_f32(builder, dst.b, umbra_mul_f32(builder, src.b, inv_a)),
        umbra_add_f32(builder, dst.a, umbra_mul_f32(builder, src.a, inv_a)),
    };
}

umbra_color_val32 umbra_blend_multiply(void *ctx, struct umbra_builder *builder,
                                       umbra_color_val32 src, umbra_color_val32 dst) {
    (void)ctx;
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

