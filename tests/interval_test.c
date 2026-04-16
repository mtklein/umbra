#include "../include/umbra.h"
#include "../include/umbra_interval.h"
#include "../src/flat_ir.h"
#include "test.h"
#include <stdlib.h>

typedef struct { float lo, hi; } iv;

static _Bool iv_equiv(iv a, iv b) {
    return equiv(a.lo, b.lo) && equiv(a.hi, b.hi);
}

static iv eval_binary(umbra_interval (*op)(struct umbra_builder*,
                                           umbra_interval, umbra_interval),
                      iv a, iv b) {
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
    return (iv){lo_out, hi_out};
}

static iv eval_unary(umbra_interval (*op)(struct umbra_builder*, umbra_interval),
                     iv a) {
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
    return (iv){lo_out, hi_out};
}

TEST(interval_add) {
    iv_equiv(eval_binary(umbra_interval_add_f32, (iv){1,3}, (iv){2,5}), (iv){3,8}) here;
    iv_equiv(eval_binary(umbra_interval_add_f32, (iv){-2,1}, (iv){-3,4}), (iv){-5,5}) here;
}

TEST(interval_sub) {
    iv_equiv(eval_binary(umbra_interval_sub_f32, (iv){1,3}, (iv){2,5}), (iv){-4,1}) here;
    iv_equiv(eval_binary(umbra_interval_sub_f32, (iv){-2,1}, (iv){-3,4}), (iv){-6,4}) here;
}

TEST(interval_mul) {
    iv_equiv(eval_binary(umbra_interval_mul_f32, (iv){2,3}, (iv){4,5}), (iv){8,15}) here;
    iv_equiv(eval_binary(umbra_interval_mul_f32, (iv){-2,3}, (iv){-1,4}), (iv){-8,12}) here;
    iv_equiv(eval_binary(umbra_interval_mul_f32, (iv){-3,-1}, (iv){-5,-2}), (iv){2,15}) here;
}

TEST(interval_min) {
    iv_equiv(eval_binary(umbra_interval_min_f32, (iv){1,5}, (iv){3,7}), (iv){1,5}) here;
    iv_equiv(eval_binary(umbra_interval_min_f32, (iv){-4,2}, (iv){-1,6}), (iv){-4,2}) here;
}

TEST(interval_max) {
    iv_equiv(eval_binary(umbra_interval_max_f32, (iv){1,5}, (iv){3,7}), (iv){3,7}) here;
    iv_equiv(eval_binary(umbra_interval_max_f32, (iv){-4,2}, (iv){-1,6}), (iv){-1,6}) here;
}

TEST(interval_sqrt) {
    iv_equiv(eval_unary(umbra_interval_sqrt_f32, (iv){4,9}), (iv){2,3}) here;
    iv_equiv(eval_unary(umbra_interval_sqrt_f32, (iv){0,16}), (iv){0,4}) here;
}

TEST(interval_abs) {
    iv const straddle = eval_unary(umbra_interval_abs_f32, (iv){-3,5});
    straddle.lo == 0.0f here;
    straddle.hi >= 5.0f here;

    iv const positive = eval_unary(umbra_interval_abs_f32, (iv){2,7});
    positive.lo == 0.0f here;
    positive.hi >= 7.0f here;
}

TEST(interval_div) {
    iv_equiv(eval_binary(umbra_interval_div_f32, (iv){2,6}, (iv){1,3}),
             (iv){2.0f/3, 6}) here;
    iv_equiv(eval_binary(umbra_interval_div_f32, (iv){-4,2}, (iv){2,5}),
             (iv){-2, 1}) here;
}

static iv eval_square(iv a) {
    struct umbra_builder *bld = umbra_builder();
    umbra_interval const ia = {umbra_uniform_32(bld, (umbra_ptr32){0}, 0),
                               umbra_uniform_32(bld, (umbra_ptr32){0}, 1)};
    umbra_interval const r = umbra_interval_mul_f32(bld, ia, ia);
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
    return (iv){lo_out, hi_out};
}

TEST(interval_square) {
    iv_equiv(eval_square((iv){2,3}), (iv){0,9}) here;
    iv_equiv(eval_square((iv){-3,5}), (iv){0,25}) here;
    iv_equiv(eval_square((iv){-5,-2}), (iv){0,25}) here;
}

static iv eval_exact_binary(umbra_interval (*op)(struct umbra_builder*,
                                                 umbra_interval, umbra_interval),
                            float a, float b) {
    struct umbra_builder *bld = umbra_builder();
    umbra_val32 const va = umbra_uniform_32(bld, (umbra_ptr32){0}, 0),
                      vb = umbra_uniform_32(bld, (umbra_ptr32){0}, 1);
    umbra_interval const ia = {va, va},
                         ib = {vb, vb};
    umbra_interval const r = op(bld, ia, ib);
    umbra_store_32(bld, (umbra_ptr32){.ix = 1}, r.lo);
    umbra_store_32(bld, (umbra_ptr32){.ix = 2}, r.hi);

    struct umbra_flat_ir *ir = umbra_flat_ir(bld);
    umbra_builder_free(bld);
    struct umbra_backend *be = umbra_backend_interp();
    struct umbra_program *prog = be->compile(be, ir);
    umbra_flat_ir_free(ir);

    float uniforms[] = {a, b};
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
    return (iv){lo_out, hi_out};
}

static iv eval_exact_unary(umbra_interval (*op)(struct umbra_builder*, umbra_interval),
                           float a) {
    struct umbra_builder *bld = umbra_builder();
    umbra_val32 const va = umbra_uniform_32(bld, (umbra_ptr32){0}, 0);
    umbra_interval const ia = {va, va};
    umbra_interval const r = op(bld, ia);
    umbra_store_32(bld, (umbra_ptr32){.ix = 1}, r.lo);
    umbra_store_32(bld, (umbra_ptr32){.ix = 2}, r.hi);

    struct umbra_flat_ir *ir = umbra_flat_ir(bld);
    umbra_builder_free(bld);
    struct umbra_backend *be = umbra_backend_interp();
    struct umbra_program *prog = be->compile(be, ir);
    umbra_flat_ir_free(ir);

    float uniforms[] = {a};
    float lo_out = 0, hi_out = 0;
    struct umbra_buf buf[] = {
        {.ptr = uniforms, .count = 1},
        {.ptr = &lo_out,  .count = 1},
        {.ptr = &hi_out,  .count = 1},
    };
    prog->queue(prog, 0, 0, 1, 1, buf);
    be->flush(be);
    prog->free(prog);
    be->free(be);
    return (iv){lo_out, hi_out};
}

TEST(interval_exact_stays_exact) {
    iv_equiv(eval_exact_binary(umbra_interval_add_f32, 3, 5), (iv){8, 8}) here;
    iv_equiv(eval_exact_binary(umbra_interval_sub_f32, 3, 5), (iv){-2, -2}) here;
    iv_equiv(eval_exact_binary(umbra_interval_mul_f32, 3, 5), (iv){15, 15}) here;
    iv_equiv(eval_exact_binary(umbra_interval_mul_f32, -3, -3), (iv){9, 9}) here;
    iv_equiv(eval_exact_binary(umbra_interval_div_f32, 6, 3), (iv){2, 2}) here;
    iv_equiv(eval_exact_binary(umbra_interval_min_f32, 3, 5), (iv){3, 3}) here;
    iv_equiv(eval_exact_binary(umbra_interval_max_f32, 3, 5), (iv){5, 5}) here;
    iv_equiv(eval_exact_unary(umbra_interval_sqrt_f32, 9), (iv){3, 3}) here;
    iv_equiv(eval_exact_unary(umbra_interval_abs_f32, -7), (iv){7, 7}) here;
    iv_equiv(eval_exact_unary(umbra_interval_abs_f32, 4), (iv){4, 4}) here;
}
