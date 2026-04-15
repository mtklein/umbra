#include "../include/umbra.h"
#include "../src/flat_ir.h"
#include "../src/interval.h"
#include "test.h"
#include <math.h>
#include <stdlib.h>

static _Bool interval_equiv(interval a, interval b) {
    return equiv(a.lo, b.lo) && equiv(a.hi, b.hi);
}
static _Bool interval_contains(interval outer, float x) {
    return outer.lo <= x && x <= outer.hi;
}

// interval_program_run() returns the last umbra_store_32() to this pointer.
static umbra_ptr32 const SINK    = {.ix = 0};
// interval_program_run()'s float const *uniform array is read via this pointer.
static umbra_ptr32 const UNIFORM = {.ix = 1};

static struct interval_program *interval_program_and_free(struct umbra_builder *b) {
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    ir != NULL here;
    umbra_builder_free(b);

    struct interval_program *p = interval_program(ir);
    umbra_flat_ir_free(ir);

    return p;
}

static interval eval_x_imm(umbra_val32 (*op)(struct umbra_builder*, umbra_val32, umbra_val32),
                           interval x, float imm) {
    struct umbra_builder *b = umbra_builder();
    umbra_store_32(b, SINK, op(b, umbra_x(b), umbra_imm_f32(b,imm)));

    struct interval_program *p = interval_program_and_free(b);
    p != NULL here;
    interval const r = interval_program_run(p, x, (interval){0,0}, NULL, 0);

    interval_program_free(p);
    return r;
}

TEST(interval_add_sub) {
    interval_equiv(eval_x_imm(umbra_add_f32, (interval){-1.0f,2.0f}, 3.0f),
                   (interval){2.0f, 5.0f}) here;

    interval_equiv(eval_x_imm(umbra_sub_f32, (interval){-1.0f,2.0f}, 3.0f),
                   (interval){-4.0f,-1.0f}) here;
}

TEST(interval_mul_signed) {
    struct umbra_builder *b = umbra_builder();
    umbra_store_32(b, SINK, umbra_mul_f32(b, umbra_x(b), umbra_y(b)));

    struct interval_program *p = interval_program_and_free(b);
    interval const r = interval_program_run(p, (interval){-1,2}, (interval){-3,4}, NULL, 0);
    interval_equiv(r, (interval){-6, 8}) here;

    interval_program_free(p);
}

TEST(interval_div_straddle) {
    interval const r = eval_x_imm(umbra_div_f32, (interval){1.0f,2.0f}, 0.0f);
    !interval_is_finite(r) here;
}

TEST(interval_div_safe) {
    interval_equiv(eval_x_imm(umbra_div_f32, (interval){1.0f,4.0f}, 2.0f),
                   (interval){0.5f, 2.0f}) here;
}

static interval eval_x(umbra_val32 (*op)(struct umbra_builder*, umbra_val32), interval x) {
    struct umbra_builder *b = umbra_builder();
    umbra_store_32(b, SINK, op(b, umbra_x(b)));

    struct interval_program *p = interval_program_and_free(b);
    interval const r = interval_program_run(p, x, (interval){0,0}, NULL, 0);

    interval_program_free(p);
    return r;
}

TEST(interval_abs_straddle) {
    interval_equiv(eval_x(umbra_abs_f32, (interval){-2.0f,1.0f}),
                   (interval){0,2}) here;
}
TEST(interval_abs_fully_neg) {
    interval_equiv(eval_x(umbra_abs_f32, (interval){-5.0f,-2.0f}),
                   (interval){2,5}) here;
}
TEST(interval_sqrt_clamp) {
    interval_equiv(eval_x(umbra_sqrt_f32, (interval){-1.0f,4.0f}),
                   (interval){0,2}) here;
}
TEST(interval_floor) {
    interval_equiv(eval_x(umbra_floor_f32, (interval){1.7f,3.2f}),
                   (interval){1,3}) here;
}
TEST(interval_ceil) {
    interval_equiv(eval_x(umbra_ceil_f32, (interval){1.1f,3.1f}),
                   (interval){2,4}) here;
}

TEST(interval_min) {
    struct umbra_builder *b = umbra_builder();
    umbra_store_32(b, SINK, umbra_min_f32(b, umbra_x(b), umbra_y(b)));

    struct interval_program *p = interval_program_and_free(b);
    interval_equiv(interval_program_run(p, (interval){-1,3}, (interval){0,5}, NULL, 0),
                   (interval){-1,3}) here;

    interval_program_free(p);
}

TEST(interval_max) {
    struct umbra_builder *b = umbra_builder();
    umbra_store_32(b, SINK, umbra_max_f32(b, umbra_x(b), umbra_y(b)));

    struct interval_program *p = interval_program_and_free(b);
    interval_equiv(interval_program_run(p, (interval){-1,3}, (interval){0,5}, NULL, 0),
                   (interval){0,5}) here;

    interval_program_free(p);
}

TEST(interval_compare_tri_valued) {
    struct umbra_builder *b = umbra_builder();
    umbra_store_32(b, SINK, umbra_lt_f32(b, umbra_x(b), umbra_imm_f32(b, 0.0f)));

    struct interval_program *p = interval_program_and_free(b);
    interval_equiv(interval_program_run(p, (interval){ 1, 2}, (interval){0,0}, NULL, 0),
                   (interval){0, 0}) here;
    interval_equiv(interval_program_run(p, (interval){-2,-1}, (interval){0,0}, NULL, 0),
                   (interval){1, 1}) here;
    interval_equiv(interval_program_run(p, (interval){-1, 1}, (interval){0,0}, NULL, 0),
                   (interval){0, 1}) here;

    interval_program_free(p);
}

TEST(interval_uniform_exact) {
    // f(x) = x + u, u a uniform (exact value) at slot 7.
    struct umbra_builder *b = umbra_builder();
    umbra_store_32(b, SINK, umbra_add_f32(b, umbra_x(b),
                                             umbra_uniform_32(b, UNIFORM, 7)));

    struct interval_program *p = interval_program_and_free(b);
    float const u[] = {0,0,0,0,0,0,0, 2.5f};
    interval_equiv(interval_program_run(p, (interval){0, 2}, (interval){0,0}, u, 8),
                   (interval){2.5f, 4.5f}) here;

    // TODO: I'm not sure if we want this to be defined behavior or just leave it untested
    // Passing fewer uniforms than needed → the uniform read becomes maximal,
    // which propagates to the output.
    !interval_is_finite(interval_program_run(p, (interval){0, 2}, (interval){0,0}, NULL, 0)) here;

    interval_program_free(p);
}

TEST(interval_fma) {
    // umbra_add_f32(mul(x,y), z) is auto-fused to op_fma_f32 by the builder.
    struct umbra_builder *b = umbra_builder();
    umbra_store_32(b, SINK, umbra_add_f32(b, umbra_mul_f32(b, umbra_x(b), umbra_y(b)),
                                             umbra_imm_f32(b, 7.0f)));

    struct interval_program *p = interval_program_and_free(b);
    // x*y in [3, 8], + 7 → [10, 15]
    interval_equiv(interval_program_run(p, (interval){1,2}, (interval){3,4}, NULL, 0),
                   (interval){10, 15}) here;

    interval_program_free(p);
}

// Prove interval_fma is single-rounded like hardware fmaf, not two-rounded
// mul+add.  Pick a where a*a isn't exactly representable (1 + 2^-22 squared
// is 1 + 2^-21 + 2^-44; the 2^-44 bit is below f32 mantissa precision and
// gets dropped by a separate rounding).  Then add -(rounded a*a): two-op
// math gives 0 (the rounding error is already gone); fused fma preserves
// it and returns ~2^-44.
TEST(interval_fma_single_rounding) {
    float const a        = 1.0f + ldexpf(1.0f, -22);
    float const aa_round = a * a;

    struct umbra_builder *b = umbra_builder();
    umbra_store_32(b, SINK, umbra_add_f32(b, umbra_mul_f32(b, umbra_x(b), umbra_x(b)),
                                             umbra_imm_f32(b, -aa_round)));

    struct interval_program *p = interval_program_and_free(b);
    float const expected = fmaf(a, a, -aa_round);

    // Sanity: the test only distinguishes fused vs two-op if they disagree.
    !equiv(expected, 0.0f) here;
    interval_equiv(interval_program_run(p, (interval){a, a}, (interval){0,0}, NULL, 0),
                   (interval){expected, expected}) here;

    interval_program_free(p);
}

// Same idea for fms_f32(x, y, z) = z - x*y.  sub(1, mul(x, x)) fuses to
// op_fms_f32 in the builder.
TEST(interval_fms_single_rounding) {
    float const a = 1.0f + ldexpf(1.0f, -22);

    struct umbra_builder *b = umbra_builder();
    umbra_store_32(b, SINK, umbra_sub_f32(b, umbra_imm_f32(b, 1.0f),
                                             umbra_mul_f32(b, umbra_x(b), umbra_x(b))));

    struct interval_program *p = interval_program_and_free(b);
    float const expected = fmaf(-a, a, 1.0f);    // single-rounded 1 - a*a

    // Sanity: the test only distinguishes fused vs two-op if they disagree.
    // Materializing a*a in its own full expression prevents clang's
    // -ffp-contract=on from fusing `1.0f - a * a` into the same fmaf we're
    // trying to contrast with — contraction is a within-expression transform.
    float const aa = a * a;
    !equiv(expected, 1.0f - aa) here;
    interval_equiv(interval_program_run(p, (interval){a, a}, (interval){0,0}, NULL, 0),
                   (interval){expected, expected}) here;

    interval_program_free(p);
}

// End-to-end: circle SDF  f(x,y) = x*x + y*y - r*r, with r uniform.
// For the unit circle:
//   - box fully inside  → interval strictly < 0
//   - box fully outside → interval strictly > 0
//   - box straddles     → interval straddles 0
static struct interval_program *circle_sdf_make(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 const X  = umbra_x(b),
                      Y  = umbra_y(b),
                      R  = umbra_uniform_32(b, UNIFORM, 0),
                      xx = umbra_mul_f32(b, X, X),
                      yy = umbra_mul_f32(b, Y, Y),
                      rr = umbra_mul_f32(b, R, R),
                      d2 = umbra_add_f32(b, xx, yy),
                      f  = umbra_sub_f32(b, d2, rr);
    umbra_store_32(b, SINK, f);

    return interval_program_and_free(b);
}

TEST(interval_circle_sdf_inside) {
    struct interval_program *p = circle_sdf_make();
    float const r[] = {1.0f};
    interval const out = interval_program_run(p, (interval){-0.25f, 0.25f},
                                                 (interval){-0.25f, 0.25f}, r, 1);
    (out.hi < 0.0f) here;
    interval_contains(out, -0.875f) here;   // tightest sample at (0.25,0.25): 0.125-1 = -0.875
    interval_program_free(p);
}

TEST(interval_circle_sdf_outside) {
    struct interval_program *p = circle_sdf_make();
    float const r[] = {1.0f};
    interval const out = interval_program_run(p, (interval){2, 3}, (interval){2, 3}, r, 1);
    (out.lo > 0.0f) here;
    interval_contains(out, 7.0f) here;      // tightest at (2,2): 8-1 = 7
    interval_program_free(p);
}

TEST(interval_circle_sdf_straddle) {
    struct interval_program *p = circle_sdf_make();
    float const r[] = {1.0f};
    interval const out = interval_program_run(p, (interval){0.5f, 1.5f},
                                                 (interval){-0.25f, 0.25f}, r, 1);
    (out.lo < 0.0f && out.hi > 0.0f) here;
    interval_program_free(p);
}

// Numerical cross-check: for many boxes, the interval must contain every
// sampled point's ground-truth SDF value.
TEST(interval_circle_sdf_contains_samples) {
    struct interval_program *p = circle_sdf_make();
    float const r[] = {1.0f};
    struct { float xlo, xhi, ylo, yhi; } const boxes[] = {
        {-2, 2, -2, 2},
        {-0.3f, 0.3f, -0.3f, 0.3f},
        { 0.5f, 1.5f, 0.5f, 1.5f},
        {-1.5f, 0.0f, 0.25f, 0.75f},
    };
    // TODO: add src/len.h with #define len(arr) (int)(sizeof(arr) / sizeof(0[arr])),
    // which would let this (and ~a dozen similar sites in src/, tests/, tools/, slides/)
    // drop the sizeof/sizeof/(int)cast boilerplate in favor of `bi < len(boxes)`.
    for (int bi = 0; bi < (int)(sizeof boxes / sizeof *boxes); bi++) {
        interval const X = {boxes[bi].xlo, boxes[bi].xhi},
                       Y = {boxes[bi].ylo, boxes[bi].yhi};
        interval const bound = interval_program_run(p, X, Y, r, 1);
        interval_is_finite(bound) here;
        enum { N = 8 };
        for (int i = 0; i <= N; i++) {
            for (int j = 0; j <= N; j++) {
                float const xx = X.lo + (X.hi - X.lo) * (float)i / (float)N,
                            yy = Y.lo + (Y.hi - Y.lo) * (float)j / (float)N;
                float const truth = xx*xx + yy*yy - 1.0f;
                interval_contains(bound, truth) here;
            }
        }
    }
    interval_program_free(p);
}

// Reuse across "tiles" — one interval_program repeatedly run must not carry
// state between invocations.
TEST(interval_run_is_resettable) {
    struct interval_program *p = circle_sdf_make();
    float const r[] = {1.0f};

    interval const far  = interval_program_run(p, (interval){10, 11}, (interval){10, 11}, r, 1);
    (far.lo > 0.0f) here;

    interval const near = interval_program_run(p, (interval){-0.1f, 0.1f},
                                                  (interval){-0.1f, 0.1f}, r, 1);
    (near.hi < 0.0f) here;

    interval_program_free(p);
}

// Unsupported ops — integer arithmetic here — cause the constructor to bail.
TEST(interval_program_null_on_int_arith) {
    struct umbra_builder *b = umbra_builder();
    umbra_store_32(b, SINK, umbra_add_i32(b, umbra_i32_from_f32(b, umbra_x(b)),
                                             umbra_imm_i32(b, 3)));

    interval_program_and_free(b) == NULL here;
}

// Uniforms from any pointer other than UNIFORM → constructor returns NULL.
TEST(interval_program_null_on_wrong_uniform_ptr) {
    struct umbra_builder *b = umbra_builder();
    umbra_store_32(b, SINK, umbra_add_f32(b, umbra_x(b),
                                             umbra_uniform_32(b, (umbra_ptr32){.ix = 2}, 0)));

    interval_program_and_free(b) == NULL here;
}

// No store to SINK → constructor returns NULL: an IR with no defined output
// can't produce a well-defined interval, so we reject it up front.
TEST(interval_program_null_without_sink_store) {
    struct umbra_builder *b = umbra_builder();
    umbra_store_32(b, (umbra_ptr32){.ix = 2},
                   umbra_add_f32(b, umbra_x(b), umbra_imm_f32(b, 2.0f)));

    interval_program_and_free(b) == NULL here;
}

TEST(interval_free_null_is_noop) {
    interval_program_free(NULL);
}

// Empty IR (no stores → everything DCE'd to insts=0) → constructor returns
// NULL via the has_sink_store check, not via op_supported.
TEST(interval_program_null_on_empty_ir) {
    interval_program_and_free(umbra_builder()) == NULL here;
}

TEST(interval_is_finite_basics) {
    interval_is_finite((interval){-1, 1}) here;
    interval_is_finite((interval){0, 0})  here;
    !interval_is_finite((interval){-INFINITY, 1}) here;
    !interval_is_finite((interval){0, +INFINITY}) here;
    !interval_is_finite((interval){-INFINITY, +INFINITY}) here;
    !interval_is_finite((interval){NAN, NAN}) here;
}
