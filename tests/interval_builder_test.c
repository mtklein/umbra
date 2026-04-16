#include "../include/umbra.h"
#include "../src/interval_builder.h"
#include "../src/flat_ir.h"
#include "../src/interval.h"
#include "test.h"
#include <stdlib.h>

static _Bool interval_equiv(interval a, interval b) {
    return equiv(a.lo, b.lo) && equiv(a.hi, b.hi);
}

// Run an interval builder op on the interp backend.  The interval [a.lo, a.hi]
// comes in as uniforms 0,1 and [b.lo, b.hi] as uniforms 2,3.  The result's
// lo and hi are stored to two separate output planes.
static interval eval_binary(umbra_interval (*op)(struct umbra_builder*,
                                                 umbra_interval, umbra_interval),
                            interval a, interval b) {
    struct umbra_builder *bld = umbra_builder();
    umbra_interval const ia = {umbra_uniform_32(bld, (umbra_ptr32){0}, 0),
                               umbra_uniform_32(bld, (umbra_ptr32){0}, 1)};
    umbra_interval const ib = {umbra_uniform_32(bld, (umbra_ptr32){0}, 2),
                               umbra_uniform_32(bld, (umbra_ptr32){0}, 3)};
    umbra_interval const r = op(bld, ia, ib);
    umbra_store_32(bld, (umbra_ptr32){.ix = 1}, r.lo);
    umbra_store_32(bld, (umbra_ptr32){.ix = 2}, r.hi);

    struct umbra_flat_ir *ir = umbra_flat_ir(bld);
    umbra_builder_free(bld);
    struct umbra_backend *be = umbra_backend_interp();
    struct umbra_program *prog = be->compile(be, ir);
    umbra_flat_ir_free(ir);

    float uniforms[] = {a.lo, a.hi, b.lo, b.hi};
    float lo_out = 0, hi_out = 0;
    struct umbra_buf buf[] = {
        {.ptr = uniforms, .count = 4},
        {.ptr = &lo_out,  .count = 1},
        {.ptr = &hi_out,  .count = 1},
    };
    prog->queue(prog, 0, 0, 1, 1, buf);
    be->flush(be);
    prog->free(prog);
    be->free(be);
    return (interval){lo_out, hi_out};
}

static interval eval_unary(umbra_interval (*op)(struct umbra_builder*, umbra_interval),
                           interval a) {
    struct umbra_builder *bld = umbra_builder();
    umbra_interval const ia = {umbra_uniform_32(bld, (umbra_ptr32){0}, 0),
                               umbra_uniform_32(bld, (umbra_ptr32){0}, 1)};
    umbra_interval const r = op(bld, ia);
    umbra_store_32(bld, (umbra_ptr32){.ix = 1}, r.lo);
    umbra_store_32(bld, (umbra_ptr32){.ix = 2}, r.hi);

    struct umbra_flat_ir *ir = umbra_flat_ir(bld);
    umbra_builder_free(bld);
    struct umbra_backend *be = umbra_backend_interp();
    struct umbra_program *prog = be->compile(be, ir);
    umbra_flat_ir_free(ir);

    float uniforms[] = {a.lo, a.hi};
    float lo_out = 0, hi_out = 0;
    struct umbra_buf buf[] = {
        {.ptr = uniforms, .count = 2},
        {.ptr = &lo_out,  .count = 1},
        {.ptr = &hi_out,  .count = 1},
    };
    prog->queue(prog, 0, 0, 1, 1, buf);
    be->flush(be);
    prog->free(prog);
    be->free(be);
    return (interval){lo_out, hi_out};
}

// Reference: run the same scalar op through interval.c.
static interval ref_binary(umbra_val32 (*op)(struct umbra_builder*, umbra_val32, umbra_val32),
                           interval a, interval b) {
    struct umbra_builder *bld = umbra_builder();
    umbra_val32 const va = umbra_x(bld),
                      vb = umbra_y(bld);
    umbra_store_32(bld, (umbra_ptr32){.ix = 1}, op(bld, va, vb));
    struct umbra_flat_ir *ir = umbra_flat_ir(bld);
    umbra_builder_free(bld);
    struct interval_program *p = interval_program(ir);
    umbra_flat_ir_free(ir);
    p != NULL here;
    interval const r = interval_program_run(p, a, b, NULL);
    interval_program_free(p);
    return r;
}

static interval ref_unary(umbra_val32 (*op)(struct umbra_builder*, umbra_val32),
                          interval a) {
    struct umbra_builder *bld = umbra_builder();
    umbra_store_32(bld, (umbra_ptr32){.ix = 1}, op(bld, umbra_x(bld)));
    struct umbra_flat_ir *ir = umbra_flat_ir(bld);
    umbra_builder_free(bld);
    struct interval_program *p = interval_program(ir);
    umbra_flat_ir_free(ir);
    p != NULL here;
    interval const r = interval_program_run(p, a, (interval){0,0}, NULL);
    interval_program_free(p);
    return r;
}

TEST(interval_builder_add) {
    interval const a = {1, 3}, b = {2, 5};
    interval_equiv(eval_binary(umbra_interval_add_f32, a, b),
                   ref_binary(umbra_add_f32, a, b)) here;

    interval const c = {-2, 1}, d = {-3, 4};
    interval_equiv(eval_binary(umbra_interval_add_f32, c, d),
                   ref_binary(umbra_add_f32, c, d)) here;
}

TEST(interval_builder_sub) {
    interval const a = {1, 3}, b = {2, 5};
    interval_equiv(eval_binary(umbra_interval_sub_f32, a, b),
                   ref_binary(umbra_sub_f32, a, b)) here;

    interval const c = {-2, 1}, d = {-3, 4};
    interval_equiv(eval_binary(umbra_interval_sub_f32, c, d),
                   ref_binary(umbra_sub_f32, c, d)) here;
}

TEST(interval_builder_mul) {
    interval const a = {2, 3}, b = {4, 5};
    interval_equiv(eval_binary(umbra_interval_mul_f32, a, b),
                   ref_binary(umbra_mul_f32, a, b)) here;

    interval const c = {-2, 3}, d = {-1, 4};
    interval_equiv(eval_binary(umbra_interval_mul_f32, c, d),
                   ref_binary(umbra_mul_f32, c, d)) here;

    interval const e = {-3, -1}, f = {-5, -2};
    interval_equiv(eval_binary(umbra_interval_mul_f32, e, f),
                   ref_binary(umbra_mul_f32, e, f)) here;
}

TEST(interval_builder_min) {
    interval const a = {1, 5}, b = {3, 7};
    interval_equiv(eval_binary(umbra_interval_min_f32, a, b),
                   ref_binary(umbra_min_f32, a, b)) here;

    interval const c = {-4, 2}, d = {-1, 6};
    interval_equiv(eval_binary(umbra_interval_min_f32, c, d),
                   ref_binary(umbra_min_f32, c, d)) here;
}

TEST(interval_builder_max) {
    interval const a = {1, 5}, b = {3, 7};
    interval_equiv(eval_binary(umbra_interval_max_f32, a, b),
                   ref_binary(umbra_max_f32, a, b)) here;

    interval const c = {-4, 2}, d = {-1, 6};
    interval_equiv(eval_binary(umbra_interval_max_f32, c, d),
                   ref_binary(umbra_max_f32, c, d)) here;
}

TEST(interval_builder_sqrt) {
    interval const a = {4, 9};
    interval_equiv(eval_unary(umbra_interval_sqrt_f32, a),
                   ref_unary(umbra_sqrt_f32, a)) here;

    interval const b = {0, 16};
    interval_equiv(eval_unary(umbra_interval_sqrt_f32, b),
                   ref_unary(umbra_sqrt_f32, b)) here;
}

TEST(interval_builder_abs_straddle) {
    interval const a = {-3, 5};
    interval const built = eval_unary(umbra_interval_abs_f32, a);
    interval const ref = ref_unary(umbra_abs_f32, a);
    built.lo <= ref.lo here;
    built.hi >= ref.hi here;
}

TEST(interval_builder_abs_positive) {
    interval const a = {2, 7};
    interval const built = eval_unary(umbra_interval_abs_f32, a);
    interval const ref = ref_unary(umbra_abs_f32, a);
    built.lo <= ref.lo here;
    built.hi >= ref.hi here;
}

TEST(interval_builder_div) {
    interval const a = {2, 6}, b = {1, 3};
    interval_equiv(eval_binary(umbra_interval_div_f32, a, b),
                   ref_binary(umbra_div_f32, a, b)) here;

    interval const c = {-4, 2}, d = {2, 5};
    interval_equiv(eval_binary(umbra_interval_div_f32, c, d),
                   ref_binary(umbra_div_f32, c, d)) here;
}
