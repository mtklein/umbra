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

static umbra_color_val32 load_8888(struct umbra_builder *b, void const *p) {
    return umbra_load_8888(b, *(umbra_ptr32 const*)p);
}
static umbra_color_val32 load_565(struct umbra_builder *b, void const *p) {
    return umbra_load_565(b, *(umbra_ptr16 const*)p);
}
static umbra_color_val32 load_1010102(struct umbra_builder *b, void const *p) {
    return umbra_load_1010102(b, *(umbra_ptr32 const*)p);
}
static umbra_color_val32 load_fp16(struct umbra_builder *b, void const *p) {
    return umbra_load_fp16(b, *(umbra_ptr64 const*)p);
}
static umbra_color_val32 load_fp16p(struct umbra_builder *b, void const *p) {
    return umbra_load_fp16_planar(b, *(umbra_ptr16 const*)p);
}
static void store_8888(struct umbra_builder *b, void const *p, umbra_color_val32 c) {
    umbra_store_8888(b, *(umbra_ptr32 const*)p, c);
}
static void store_565(struct umbra_builder *b, void const *p, umbra_color_val32 c) {
    umbra_store_565(b, *(umbra_ptr16 const*)p, c);
}
static void store_1010102(struct umbra_builder *b, void const *p, umbra_color_val32 c) {
    umbra_store_1010102(b, *(umbra_ptr32 const*)p, c);
}
static void store_fp16(struct umbra_builder *b, void const *p, umbra_color_val32 c) {
    umbra_store_fp16(b, *(umbra_ptr64 const*)p, c);
}
static void store_fp16p(struct umbra_builder *b, void const *p, umbra_color_val32 c) {
    umbra_store_fp16_planar(b, *(umbra_ptr16 const*)p, c);
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

struct umbra_builder* umbra_draw_builder(
    struct umbra_matrix const *transform_mat,
    umbra_coverage  coverage_fn, void *coverage_ctx,
    umbra_shader    shader_fn,   void *shader_ctx,
    umbra_blend     blend_fn,    void *blend_ctx,
    struct umbra_buf *dst_buf,
    struct umbra_fmt  fmt)
{
    struct umbra_builder *b = umbra_builder();
    umbra_ptr32 const dst_ptr = umbra_bind_buf32(b, dst_buf);
    umbra_val32 xf = umbra_f32_from_i32(b, umbra_x(b)),
                yf = umbra_f32_from_i32(b, umbra_y(b));
    if (transform_mat) {
        umbra_point_val32 const p = umbra_transform_perspective(transform_mat, b, xf, yf);
        xf = p.x;
        yf = p.y;
    }

    umbra_val32 coverage = {0};
    if (coverage_fn) { coverage = coverage_fn(coverage_ctx, b, xf, yf); }

    umbra_val32 const zero = umbra_imm_f32(b, 0.0f);
    umbra_color_val32 src = {zero, zero, zero, zero};
    if (shader_fn) { src = shader_fn(shader_ctx, b, xf, yf); }

    umbra_color_val32 dst = {zero, zero, zero, zero};
    if (blend_fn || coverage_fn) { dst = fmt.load(b, &dst_ptr); }

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

    fmt.store(b, &dst_ptr, out);
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
    umbra_ptr32 const u = umbra_bind_uniforms32(b, mat, (int)(sizeof *mat / 4));
    umbra_matrix_val32 const m = {
        .sx = umbra_uniform_32(b, u, M_SX),
        .kx = umbra_uniform_32(b, u, M_KX),
        .tx = umbra_uniform_32(b, u, M_TX),
        .ky = umbra_uniform_32(b, u, M_KY),
        .sy = umbra_uniform_32(b, u, M_SY),
        .ty = umbra_uniform_32(b, u, M_TY),
        .p0 = umbra_uniform_32(b, u, M_P0),
        .p1 = umbra_uniform_32(b, u, M_P1),
        .p2 = umbra_uniform_32(b, u, M_P2),
    };
    return umbra_apply_matrix(b, m, x, y);
}

// Interval affine transform, used only by umbra_sdf_draw's bounds program.
// Interval perspective is unavailable by design: umbra_interval_div_f32 is
// unsound when w straddles zero (see TODO near it in src/interval.c), so
// callers that want a transform applied to intervals must first verify the
// matrix is affine (p0 == p1 == 0 && p2 == 1) -- at which point the divide
// collapses to 1 and we just emit the six-parameter affine form here.
static void transform_affine_interval(struct umbra_matrix const *mat,
                                      struct umbra_builder *b,
                                      umbra_interval *x, umbra_interval *y) {
    umbra_ptr32 const u = umbra_bind_uniforms32(b, mat, (int)(sizeof *mat / 4));
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

void umbra_matrix_mul(struct umbra_matrix *out,
                      struct umbra_matrix const *a,
                      struct umbra_matrix const *b) {
    struct umbra_matrix const r = {
        .sx = a->sx*b->sx + a->kx*b->ky + a->tx*b->p0,
        .kx = a->sx*b->kx + a->kx*b->sy + a->tx*b->p1,
        .tx = a->sx*b->tx + a->kx*b->ty + a->tx*b->p2,
        .ky = a->ky*b->sx + a->sy*b->ky + a->ty*b->p0,
        .sy = a->ky*b->kx + a->sy*b->sy + a->ty*b->p1,
        .ty = a->ky*b->tx + a->sy*b->ty + a->ty*b->p2,
        .p0 = a->p0*b->sx + a->p1*b->ky + a->p2*b->p0,
        .p1 = a->p0*b->kx + a->p1*b->sy + a->p2*b->p1,
        .p2 = a->p0*b->tx + a->p1*b->ty + a->p2*b->p2,
    };
    *out = r;
}

umbra_point_val32 umbra_apply_matrix(struct umbra_builder *b, umbra_matrix_val32 m,
                                      umbra_val32 x, umbra_val32 y) {
    umbra_val32 const w =
        umbra_add_f32(b, umbra_add_f32(b, umbra_mul_f32(b, m.p0, x),
                                          umbra_mul_f32(b, m.p1, y)),
                         m.p2);
    umbra_val32 const xp =
        umbra_div_f32(b, umbra_add_f32(b, umbra_add_f32(b, umbra_mul_f32(b, m.sx, x),
                                                           umbra_mul_f32(b, m.kx, y)),
                                          m.tx),
                         w);
    umbra_val32 const yp =
        umbra_div_f32(b, umbra_add_f32(b, umbra_add_f32(b, umbra_mul_f32(b, m.ky, x),
                                                           umbra_mul_f32(b, m.sy, y)),
                                          m.ty),
                         w);
    return (umbra_point_val32){xp, yp};
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

struct grid_params {
    float base_x, base_y, tile_w, tile_h;
};

struct umbra_sdf_draw {
    struct umbra_coverage_from_sdf cov_state;
    struct umbra_program          *draw;
    struct umbra_program          *bounds;
    struct umbra_backend          *bounds_be;
    float                         *lo;
    struct grid_params             grid;
    int                            lo_cap;
    int                            :32;
    struct umbra_buf               lo_buf;
    struct umbra_buf               draw_dst_buf;
};

// TODO: add a second `covered` program alongside `draw` for tiles where the
// SDF is entirely inside the shape.  SDF convention is f<0 inside, so:
//   bounds.hi < 0 -> tile fully covered -> dispatch `covered` (shader+blend at cov=1)
//   bounds.lo < 0 -> some chance of coverage -> dispatch `draw` (current behavior)
//   else          -> skip tile
// `covered` would be built like `draw` but without the SDF coverage adapter,
// skipping per-pixel SDF evaluation entirely.  Prerequisites:
//   - bounds program must also emit f.hi; currently only f.lo is stored.
//   - bounds_buf grows a second float* output; allocate/free in _queue.
//   - tile loop in umbra_sdf_draw_queue branches on hi<0 vs lo<0.

// TODO: query per-backend dispatch_granularity instead of this one global
// compromise.  CPU-optimal is ~16-32, GPU-optimal is ~512-1024.
enum { QUEUE_MIN_TILE = 512 };

static _Bool matrix_is_affine(struct umbra_matrix const *m) {
    return m->p0 == 0.0f && m->p1 == 0.0f && m->p2 == 1.0f;
}

struct umbra_sdf_draw* umbra_sdf_draw(struct umbra_backend *be,
                                      struct umbra_matrix const *transform_mat,
                                      umbra_sdf sdf_fn, void *sdf_ctx,
                                      _Bool hard_edge,
                                      umbra_shader shader_fn, void *shader_ctx,
                                      umbra_blend blend_fn, void *blend_ctx,
                                      struct umbra_fmt fmt) {
    struct umbra_sdf_draw *d = calloc(1, sizeof *d);
    d->cov_state = (struct umbra_coverage_from_sdf){
        .sdf_fn    = sdf_fn,
        .sdf_ctx   = sdf_ctx,
        .hard_edge = hard_edge,
    };

    // Build the draw program (transform + shader + SDF coverage + blend).
    struct umbra_builder *db = umbra_draw_builder(
        transform_mat,
        umbra_coverage_from_sdf, &d->cov_state,
        shader_fn, shader_ctx,
        blend_fn,  blend_ctx,
        &d->draw_dst_buf, fmt);
    struct umbra_flat_ir *dir = umbra_flat_ir(db);
    umbra_builder_free(db);
    d->draw = be->compile(be, dir);
    umbra_flat_ir_free(dir);
    if (!d->draw) { free(d); return NULL; }

    // Gate the bounds program on affine-only transforms.  Perspective interval
    // divide is unsound when w straddles zero (see TODO in src/interval.c), so
    // we'd silently drop horizon-crossing tiles.  Non-affine transforms skip
    // bounds entirely and fall back to a full-rect dispatch in _queue.
    _Bool const build_bounds = !transform_mat || matrix_is_affine(transform_mat);
    if (!build_bounds) { return d; }

    // Build the bounds program: evaluate the sdf over tile-extent intervals.
    // x() and y() are tile indices.
    struct umbra_builder *bb = umbra_builder();
    umbra_ptr32 const g   = umbra_bind_uniforms32(bb, &d->grid,
                                                  (int)(sizeof d->grid / 4));
    umbra_ptr32 const dst = umbra_bind_buf32(bb, &d->lo_buf);
    umbra_val32 const base_x = umbra_uniform_32(bb, g, 0),
                      base_y = umbra_uniform_32(bb, g, 1),
                      tile_w = umbra_uniform_32(bb, g, 2),
                      tile_h = umbra_uniform_32(bb, g, 3);
    umbra_val32 const xf = umbra_f32_from_i32(bb, umbra_x(bb)),
                      yf = umbra_f32_from_i32(bb, umbra_y(bb));
    umbra_interval x = {umbra_add_f32(bb, base_x, umbra_mul_f32(bb, xf, tile_w)),
                        umbra_add_f32(bb, base_x,
                            umbra_mul_f32(bb, umbra_add_f32(bb, xf,
                                umbra_imm_f32(bb, 1.0f)), tile_w))},
                   y = {umbra_add_f32(bb, base_y, umbra_mul_f32(bb, yf, tile_h)),
                        umbra_add_f32(bb, base_y,
                            umbra_mul_f32(bb, umbra_add_f32(bb, yf,
                                umbra_imm_f32(bb, 1.0f)), tile_h))};
    if (transform_mat) { transform_affine_interval(transform_mat, bb, &x, &y); }

    umbra_interval const f = sdf_fn(sdf_ctx, bb, x, y);

    umbra_store_32(bb, dst, f.lo);

    struct umbra_flat_ir *bir = umbra_flat_ir(bb);
    umbra_builder_free(bb);
    struct umbra_backend *bounds_be = umbra_backend_jit();
    if (!bounds_be) { bounds_be = umbra_backend_interp(); }
    d->bounds    = bounds_be->compile(bounds_be, bir);
    d->bounds_be = bounds_be;
    umbra_flat_ir_free(bir);

    return d;
}

void umbra_sdf_draw_queue(struct umbra_sdf_draw *d,
                          int l, int t, int r, int b, struct umbra_buf dst) {
    if (!d) { return; }
    if (!d->bounds) {
        // Perspective transform: tile-culling bounds are unsound; dispatch full rect.
        d->draw_dst_buf = dst;
        d->draw->queue(d->draw, l, t, r, b);
        return;
    }
    int const w  = r - l,
              h  = b - t,
              xt = (w + QUEUE_MIN_TILE - 1) / QUEUE_MIN_TILE,
              yt = (h + QUEUE_MIN_TILE - 1) / QUEUE_MIN_TILE;
    int const tiles = xt * yt;

    d->grid = (struct grid_params){
        .base_x = (float)l,
        .base_y = (float)t,
        .tile_w = (float)QUEUE_MIN_TILE,
        .tile_h = (float)QUEUE_MIN_TILE,
    };
    if (tiles > d->lo_cap) {
        d->lo = realloc(d->lo, (size_t)tiles * sizeof(float));
        d->lo_cap = tiles;
    }
    __builtin_memset(d->lo, 0, (size_t)tiles * sizeof(float));
    d->lo_buf = (struct umbra_buf){.ptr = d->lo, .count = tiles, .stride = xt};
    d->bounds->queue(d->bounds, 0, 0, xt, yt);
    float *lo = d->lo;

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
                d->draw_dst_buf = dst;
                d->draw->queue(d->draw, tl, tt, tr, tb);
            }
        }
    }
}

void umbra_sdf_draw_free(struct umbra_sdf_draw *d) {
    if (d) {
        umbra_program_free(d->draw);
        umbra_program_free(d->bounds);
        umbra_backend_free(d->bounds_be);
        free(d->lo);
        free(d);
    }
}

umbra_color_val32 umbra_shader_color(void *ctx, struct umbra_builder *b,
                                     umbra_val32 x, umbra_val32 y) {
    umbra_color const *self = ctx;
    (void)x; (void)y;
    umbra_ptr32 const u = umbra_bind_uniforms32(b, self, sizeof *self / 4);
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
    umbra_ptr32 const u = umbra_bind_uniforms32(b, self, sizeof *self / 4);
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

umbra_interval umbra_sdf_rect(void *ctx, struct umbra_builder *b,
                              umbra_interval x, umbra_interval y) {
    umbra_rect const *self = ctx;
    umbra_ptr32 const u = umbra_bind_uniforms32(b, self, sizeof *self / 4);
    umbra_interval const l  = umbra_interval_exact(umbra_uniform_32(b, u, 0)),
                         t  = umbra_interval_exact(umbra_uniform_32(b, u, 1)),
                         r  = umbra_interval_exact(umbra_uniform_32(b, u, 2)),
                         bo = umbra_interval_exact(umbra_uniform_32(b, u, 3));
    return umbra_interval_max_f32(b, umbra_interval_max_f32(b, umbra_interval_sub_f32(b, l, x),
                                                               umbra_interval_sub_f32(b, x, r)),
                                    umbra_interval_max_f32(b, umbra_interval_sub_f32(b, t, y),
                                                              umbra_interval_sub_f32(b, y, bo)));
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

