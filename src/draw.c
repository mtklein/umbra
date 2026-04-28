#include "../include/umbra_draw.h"

#include "assume.h"
#include "count.h"
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
                      umbra_blend    blend_fn,    void *blend_ctx) {
    umbra_val32 coverage = {0};
    if (coverage_fn) { coverage = coverage_fn(coverage_ctx, b, x, y); }

    umbra_val32 const zero = umbra_imm_f32(b, 0.0f);
    umbra_color_val32 src = {zero, zero, zero, zero};
    if (shader_fn) { src = shader_fn(shader_ctx, b, x, y); }

    umbra_color_val32 dst = {zero, zero, zero, zero};
    if (blend_fn || coverage_fn) { dst = fmt.load(b, dst_ptr); }

    umbra_color_val32 out = blend_fn ? blend_fn(blend_ctx, b, src, dst) : src;

    if (coverage_fn) {
        out.r = umbra_fma_f32(b, umbra_sub_f32(b, out.r, dst.r), coverage, dst.r);
        out.g = umbra_fma_f32(b, umbra_sub_f32(b, out.g, dst.g), coverage, dst.g);
        out.b = umbra_fma_f32(b, umbra_sub_f32(b, out.b, dst.b), coverage, dst.b);
        out.a = umbra_fma_f32(b, umbra_sub_f32(b, out.a, dst.a), coverage, dst.a);
    }

    fmt.store(b, dst_ptr, out);
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
        umbra_add_f32(b, umbra_fma_f32(b, p0, x, umbra_mul_f32(b, p1, y)), p2);
    umbra_val32 const xp =
        umbra_div_f32(b, umbra_add_f32(b, umbra_fma_f32(b, sx, x, umbra_mul_f32(b, kx, y)), tx),
                         w);
    umbra_val32 const yp =
        umbra_div_f32(b, umbra_add_f32(b, umbra_fma_f32(b, ky, x, umbra_mul_f32(b, sy, y)), ty),
                         w);
    return (umbra_point_val32){xp, yp};
}

umbra_point_val32 umbra_transform_affine(struct umbra_affine const *mat,
                                         struct umbra_builder *b,
                                         umbra_val32 x, umbra_val32 y) {
    umbra_ptr const u = umbra_bind_uniforms(b, mat, (int)(sizeof *mat / 4));
    umbra_val32 const sx = umbra_uniform_32(b, u, 0),
                      kx = umbra_uniform_32(b, u, 1),
                      tx = umbra_uniform_32(b, u, 2),
                      ky = umbra_uniform_32(b, u, 3),
                      sy = umbra_uniform_32(b, u, 4),
                      ty = umbra_uniform_32(b, u, 5);
    umbra_val32 const xp = umbra_fma_f32(b, sx, x, umbra_fma_f32(b, kx, y, tx));
    umbra_val32 const yp = umbra_fma_f32(b, ky, x, umbra_fma_f32(b, sy, y, ty));
    return (umbra_point_val32){xp, yp};
}

struct coverage_from_sdf {
    umbra_sdf *sdf_fn;
    void      *sdf_ctx;
};

static umbra_val32 coverage_from_sdf(void *ctx, struct umbra_builder *b,
                                     umbra_val32 x, umbra_val32 y) {
    struct coverage_from_sdf const *self = ctx;
    umbra_val32 const zero = umbra_imm_f32(b, 0.0f),
                      one  = umbra_imm_f32(b, 1.0f);
    umbra_interval const f = self->sdf_fn(self->sdf_ctx, b,
                                          (umbra_interval){x, umbra_add_f32(b, x, one)},
                                          (umbra_interval){y, umbra_add_f32(b, y, one)});
    umbra_val32 const width   = umbra_sub_f32(b, f.hi, f.lo),
                      neg_lo  = umbra_sub_f32(b, zero, f.lo),
                      edge    = umbra_div_f32(b, neg_lo, width),
                      inside  = umbra_le_f32 (b, f.hi, zero),
                      outside = umbra_le_f32 (b, zero, f.lo);
    return umbra_sel_32(b, inside,  one,
           umbra_sel_32(b, outside, zero, edge));
}

umbra_interval umbra_sdf_stroke(void *ctx, struct umbra_builder *b,
                                 umbra_interval x, umbra_interval y) {
    struct umbra_sdf_stroke const *self = ctx;
    umbra_interval const d     = self->inner_fn(self->inner_ctx, b, x, y),
                         abs_d = umbra_interval_abs_f32(b, d);
    umbra_ptr      const u     = umbra_bind_uniforms(b, &self->width, 1);
    umbra_interval const w     = umbra_interval_exact(umbra_uniform_32(b, u, 0));
    return umbra_interval_sub_f32(b, abs_d, w);
}

void umbra_build_sdf_draw(struct umbra_builder *b,
                          umbra_ptr dst_ptr, struct umbra_fmt dst_fmt,
                          umbra_val32 x, umbra_val32 y,
                          umbra_sdf sdf_fn, void *sdf_ctx,
                          umbra_shader shader_fn, void *shader_ctx,
                          umbra_blend  blend_fn,  void *blend_ctx) {
    struct coverage_from_sdf cov = {
        .sdf_fn  = sdf_fn,
        .sdf_ctx = sdf_ctx,
    };
    umbra_build_draw(b, dst_ptr, dst_fmt, x, y,
                     coverage_from_sdf, &cov,
                     shader_fn, shader_ctx,
                     blend_fn,  blend_ctx);
}

enum umbra_sdf_tile {
    UMBRA_SDF_TILE_NONE    = 0x00,  // f.lo >= 0: tile entirely outside
    UMBRA_SDF_TILE_PARTIAL = 0x01,  // f.lo <  0, f.hi >= 0: edge tile, needs per-pixel SDF eval
    UMBRA_SDF_TILE_FULL    = 0x02,  // f.hi <  0: tile entirely inside
};

struct umbra_sdf_bounds_program {
    struct umbra_backend *be;   // owned; freed in umbra_sdf_bounds_program_free()
    struct umbra_program *prog;
    umbra_ptr             cov_ptr;      // late-bound cov buffer
    umbra_ptr             uniform_ptr;  // late-bound {base_x, base_y, tile_w, tile_h}
};

// Produce (ix, iy) covering the tile at (umbra_x(), umbra_y()), sourced from
// a 4-slot uniform block supplied per dispatch by umbra_sdf_dispatch.
static void sdf_tile_intervals(struct umbra_builder *bb,
                               struct umbra_sdf_bounds_program *bounds,
                               umbra_interval *ix, umbra_interval *iy) {
    bounds->uniform_ptr = umbra_bind_uniforms(bb, NULL, 4);
    umbra_val32 const base_x = umbra_uniform_32(bb, bounds->uniform_ptr, 0),
                      base_y = umbra_uniform_32(bb, bounds->uniform_ptr, 1),
                      tile_w = umbra_uniform_32(bb, bounds->uniform_ptr, 2),
                      tile_h = umbra_uniform_32(bb, bounds->uniform_ptr, 3);
    umbra_val32 const xf = umbra_f32_from_i32(bb, umbra_x(bb)),
                      yf = umbra_f32_from_i32(bb, umbra_y(bb));
    umbra_val32 const one = umbra_imm_f32(bb, 1.0f);
    *ix = (umbra_interval){
        umbra_fma_f32(bb, xf, tile_w, base_x),
        umbra_fma_f32(bb, umbra_add_f32(bb, xf, one), tile_w, base_x),
    };
    *iy = (umbra_interval){
        umbra_fma_f32(bb, yf, tile_h, base_y),
        umbra_fma_f32(bb, umbra_add_f32(bb, yf, one), tile_h, base_y),
    };
}

static void apply_affine_interval(struct umbra_affine const *a,
                                  struct umbra_builder *b,
                                  umbra_interval *x, umbra_interval *y) {
    umbra_ptr const u = umbra_bind_uniforms(b, a, (int)(sizeof *a / 4));
    umbra_interval const sx = umbra_interval_exact(umbra_uniform_32(b, u, 0)),
                         kx = umbra_interval_exact(umbra_uniform_32(b, u, 1)),
                         tx = umbra_interval_exact(umbra_uniform_32(b, u, 2)),
                         ky = umbra_interval_exact(umbra_uniform_32(b, u, 3)),
                         sy = umbra_interval_exact(umbra_uniform_32(b, u, 4)),
                         ty = umbra_interval_exact(umbra_uniform_32(b, u, 5));
    umbra_interval const xp =
        umbra_interval_fma_f32(b, sx, *x,
            umbra_interval_fma_f32(b, kx, *y, tx));
    umbra_interval const yp =
        umbra_interval_fma_f32(b, ky, *x,
            umbra_interval_fma_f32(b, sy, *y, ty));
    *x = xp;
    *y = yp;
}

struct umbra_sdf_bounds_program* umbra_sdf_bounds_program(struct umbra_builder *b,
                                                          struct umbra_affine const *transform,
                                                          umbra_sdf sdf_fn, void *sdf_ctx) {
    struct umbra_sdf_bounds_program *bounds = calloc(1, sizeof *bounds);

    bounds->cov_ptr = umbra_bind_buf(b, NULL);
    umbra_interval x, y;
    sdf_tile_intervals(b, bounds, &x, &y);
    if (transform) {
        apply_affine_interval(transform, b, &x, &y);
    }

    // Emit the tri-state constants BEFORE calling sdf_fn so any matching
    // constants the sdf also needs (e.g. imm 0/1 as index offsets inside an
    // SDF loop) CSE onto these outer definitions.  Otherwise CSE puts the
    // imm inside the first-use loop body, and Metal / SPIRV-Cross won't
    // hoist scalar constants across loop scope -- later uses outside the
    // loop reference a variable declared inside it and fail to compile.
    umbra_val32 const zero_f    = umbra_imm_f32(b, 0.0f);
    umbra_val32 const none_i    = umbra_imm_i32(b, UMBRA_SDF_TILE_NONE);
    umbra_val32 const partial_i = umbra_imm_i32(b, UMBRA_SDF_TILE_PARTIAL);
    umbra_val32 const full_i    = umbra_imm_i32(b, UMBRA_SDF_TILE_FULL);

    umbra_interval const f = sdf_fn(sdf_ctx, b, x, y);

    umbra_val32 const partial = umbra_lt_f32(b, f.lo, zero_f),
                      full    = umbra_lt_f32(b, f.hi, zero_f);
    umbra_val32 const base = umbra_sel_32(b, partial, partial_i, none_i);
    umbra_val32 const tri  = umbra_sel_32(b, full,    full_i,    base);
    umbra_store_16(b, bounds->cov_ptr, umbra_i16_from_i32(b, tri));

    // Snapshot + compile internally.  Leave `b` alive; caller owns its
    // lifetime and may inspect/dump/recompile after this returns.
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    bounds->be = umbra_backend_jit();
    if (!bounds->be) { bounds->be = umbra_backend_interp(); }
    bounds->prog = bounds->be->compile(bounds->be, ir);
    umbra_flat_ir_free(ir);
    return bounds;
}

void umbra_sdf_bounds_program_free(struct umbra_sdf_bounds_program *b) {
    if (b) {
        umbra_program_free(b->prog);
        umbra_backend_free(b->be);
        free(b);
    }
}

// TODO: move SDF bounds-eval onto the caller's draw backend and collapse
//       umbra_sdf_dispatch to a single submit per frame on GPU.  Today the
//       bounds program is hardcoded to JIT and the host reads cov[] back to
//       choose draw_partial / draw_full / skip per tile; profiling SDF Text
//       Analytic on metal shows ~all main-thread CPU is that JIT bounds
//       eval.  All-on-GPU should reclaim it.
//
//   Plan (all one commit, since the API only makes sense end-to-end):
//
//   1. Bounds compiles on draw_partial->backend instead of always JIT.
//      cov[] stays a normal umbra_bind_buf — backends already stage host
//      buffers across queue boundaries.
//
//   2. draw_partial / draw_full grow a per-tile early-exit at the top
//      (`if cov[tile_idx] matches my kind`).  GPU dispatches both over
//      (l, t, r, b) once; the in-shader gate filters NONE / wrong-kind
//      tiles.  No mid-frame flush, no readback.  dispatch_overlap_check
//      already inserts the cov RAW barrier on vulkan; metal/wgpu spec
//      it implicitly.
//
//   3. CPU dispatch (program_queue_is_cheap=true on jit/interp) keeps
//      today's per-tile path: bounds runs synchronously on the same
//      backend, host reads cov, host queues per tile.  No regression.
//
//   API constraint: keep the cov-gate machinery internal to this file.
//   Don't expose enums for tile kinds, gate-builder helpers, or "draw
//   bundled with cov ptrs" structs in umbra_draw.h — that's just the
//   inside of umbra_sdf_dispatch leaking out.  umbra_build_sdf_draw and
//   umbra_sdf_dispatch keep their current shapes; whatever extra plumbing
//   the gate needs (likely two ptrs returned via out-params from
//   umbra_build_sdf_draw, then handed back to umbra_sdf_dispatch as
//   bare umbra_ptr args) lives here and nowhere else.
//
//   Two JIT bugs landed first that this work surfaced (both already on
//   main):
//     - jit: gather_* uses flat addressing, not Y*stride row offset.
//     - ra: ra_ensure auto-holds; ra_step releases at instruction
//       boundary.
//   With those in place, draw bodies with masked stores wrapped in
//   umbra_if(varying_cond) work correctly on JIT — which is what the
//   shader-side cov-check on CPU backends would need.
//
//   Earlier dead-end (commit 8f1910cf, reverted in 507742b6): compile
//   bounds on the caller-supplied backend, queue+flush mid-dispatch,
//   then read cov[] back to drive per-tile draws on the host.  Big win
//   for heavy bounds (SDF Text Analytic on metal: 2.53 -> 1.81 ns/px @
//   40% -> 5% CPU), but the per-frame GPU sync tanked simple SDFs
//   (Union: .22 -> .11 ns/px but 72% CPU; wgpu went .22 -> 2.82 ns/px @
//   99% CPU — no zero-copy means a full staging round-trip per frame
//   for the cov download).  The mid-dispatch flush is the cost; readback
//   is the second cost on backends without zero-copy.  Both vanish
//   under the shader-side-cull plan above.
void umbra_sdf_dispatch(struct umbra_sdf_bounds_program *bounds,
                        struct umbra_program            *draw_partial,
                        struct umbra_program            *draw_full,
                        int l, int t, int r, int b,
                        struct umbra_late_binding const *late, int lates) {
    struct umbra_program* prog_for_cov[] = {
        [UMBRA_SDF_TILE_NONE   ] = NULL,
        [UMBRA_SDF_TILE_PARTIAL] = draw_partial,
        [UMBRA_SDF_TILE_FULL   ] = draw_partial->backend->program_switch_is_cheap ? draw_full
                                                                                  : draw_partial,
    };

    int const T = draw_partial->backend->program_queue_is_cheap ? 8 : 64,
              x_tiles = (r - l + T - 1) / T,
              y_tiles = (b - t + T - 1) / T,
              tiles = x_tiles * y_tiles;

    uint16_t *cov = malloc((size_t)tiles * sizeof *cov);
    float const uniforms[] = {(float)l, (float)t, (float)T, (float)T};

    struct umbra_late_binding const bounds_late[] = {
        {.ptr = bounds->cov_ptr    , .buf = {.ptr = cov, .count = tiles, .stride = x_tiles}},
        {.ptr = bounds->uniform_ptr, .uniforms = uniforms},
    };
    bounds->prog->queue(bounds->prog, 0, 0, x_tiles, y_tiles, bounds_late, count(bounds_late));

    // Coalesce horizontally adjacent tiles with the same program to amortize dispatch overhead.
    for (int ty = 0; ty < y_tiles; ty++) {
        int const tt = t + ty*T,
                  tb = tt + T < b ? tt + T : b;
        struct umbra_program *run_prog = NULL;
        int                   run_start = 0;
        for (int tx = 0; tx < x_tiles; tx++) {
            struct umbra_program *prog = prog_for_cov[ cov[ty*x_tiles + tx] ];
            if (prog != run_prog) {
                if (run_prog) {
                    run_prog->queue(run_prog, l + run_start*T, tt, l + tx*T, tb, late, lates);
                }
                run_prog  = prog;
                run_start = tx;
            }
        }
        if (run_prog) {
            run_prog->queue(run_prog, l + run_start*T, tt, r, tb, late, lates);
        }
    }
    free(cov);
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
    int const samples = self->samples;
    assume(1 <= samples && samples <= 8);

    umbra_color_val32 sum = self->inner_fn(self->inner_ctx, b, x, y);
    for (int i = 1; i < samples; i++) {
        umbra_val32 const sx = umbra_add_f32(b, x, umbra_imm_f32(b, jitter[i - 1][0])),
                          sy = umbra_add_f32(b, y, umbra_imm_f32(b, jitter[i - 1][1]));
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
    umbra_val32 const l   = umbra_uniform_32(b, u, 0),
                      t   = umbra_uniform_32(b, u, 1),
                      r   = umbra_uniform_32(b, u, 2),
                      bot = umbra_uniform_32(b, u, 3);
    umbra_val32 const inside = umbra_and_32(b,
                                   umbra_and_32(b, umbra_le_f32(b, l, x),
                                                   umbra_lt_f32(b, x, r)),
                                   umbra_and_32(b, umbra_le_f32(b, t, y),
                                                   umbra_lt_f32(b, y, bot)));
    umbra_val32 const one  = umbra_imm_f32(b, 1.0f),
                      zero = umbra_imm_f32(b, 0.0f);
    return umbra_sel_32(b, inside, one, zero);
}

umbra_color_val32 umbra_blend_src(void *ctx, struct umbra_builder *b,
                                  umbra_color_val32 src, umbra_color_val32 dst) {
    (void)ctx;
    (void)b;
    (void)dst;
    return src;
}

umbra_color_val32 umbra_blend_srcover(void *ctx, struct umbra_builder *b,
                                      umbra_color_val32 src, umbra_color_val32 dst) {
    (void)ctx;
    umbra_val32 const one   = umbra_imm_f32(b, 1.0f),
                      inv_a = umbra_sub_f32(b, one, src.a);
    return (umbra_color_val32){
        umbra_fma_f32(b, dst.r, inv_a, src.r),
        umbra_fma_f32(b, dst.g, inv_a, src.g),
        umbra_fma_f32(b, dst.b, inv_a, src.b),
        umbra_fma_f32(b, dst.a, inv_a, src.a),
    };
}

umbra_color_val32 umbra_blend_dstover(void *ctx, struct umbra_builder *b,
                                      umbra_color_val32 src, umbra_color_val32 dst) {
    (void)ctx;
    umbra_val32 const one   = umbra_imm_f32(b, 1.0f),
                      inv_a = umbra_sub_f32(b, one, dst.a);
    return (umbra_color_val32){
        umbra_fma_f32(b, src.r, inv_a, dst.r),
        umbra_fma_f32(b, src.g, inv_a, dst.g),
        umbra_fma_f32(b, src.b, inv_a, dst.b),
        umbra_fma_f32(b, src.a, inv_a, dst.a),
    };
}

umbra_color_val32 umbra_blend_multiply(void *ctx, struct umbra_builder *b,
                                       umbra_color_val32 src, umbra_color_val32 dst) {
    (void)ctx;
    umbra_val32 const one    = umbra_imm_f32(b, 1.0f),
                      inv_sa = umbra_sub_f32(b, one, src.a),
                      inv_da = umbra_sub_f32(b, one, dst.a);
    umbra_val32 const r  = umbra_fma_f32(b, src.r, dst.r,
                               umbra_fma_f32(b, src.r, inv_da,
                                   umbra_mul_f32(b, dst.r, inv_sa))),
                      g  = umbra_fma_f32(b, src.g, dst.g,
                               umbra_fma_f32(b, src.g, inv_da,
                                   umbra_mul_f32(b, dst.g, inv_sa))),
                      bl = umbra_fma_f32(b, src.b, dst.b,
                               umbra_fma_f32(b, src.b, inv_da,
                                   umbra_mul_f32(b, dst.b, inv_sa))),
                      a  = umbra_fma_f32(b, src.a, dst.a,
                               umbra_fma_f32(b, src.a, inv_da,
                                   umbra_mul_f32(b, dst.a, inv_sa)));
    return (umbra_color_val32){r, g, bl, a};
}

