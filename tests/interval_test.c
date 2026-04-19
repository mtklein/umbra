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
    float uniforms[] = {a.lo, a.hi, b.lo, b.hi};
    float lo_out = 0, hi_out = 0;
    struct umbra_buf lo_buf = {.ptr = &lo_out, .count = 1},
                     hi_buf = {.ptr = &hi_out, .count = 1};

    struct umbra_builder *bld = umbra_builder();
    umbra_ptr32 const u    = umbra_uniforms  (bld, uniforms, 4);
    umbra_ptr32 const lop  = umbra_bind_buf32(bld, &lo_buf);
    umbra_ptr32 const hip  = umbra_bind_buf32(bld, &hi_buf);
    umbra_interval const ia = {umbra_uniform_32(bld, u, 0),
                               umbra_uniform_32(bld, u, 1)};
    umbra_interval const ib = {umbra_uniform_32(bld, u, 2),
                               umbra_uniform_32(bld, u, 3)};
    umbra_interval const r = op(bld, ia, ib);
    umbra_store_32(bld, lop, r.lo);
    umbra_store_32(bld, hip, r.hi);

    struct umbra_flat_ir *ir = umbra_flat_ir(bld);
    umbra_builder_free(bld);
    struct umbra_backend *be = umbra_backend_interp();
    struct umbra_program *prog = be->compile(be, ir);
    umbra_flat_ir_free(ir);

    prog->queue(prog, 0, 0, 1, 1);
    be->flush(be);
    umbra_program_free(prog);
    umbra_backend_free(be);
    return (iv){lo_out, hi_out};
}

static iv eval_unary(umbra_interval (*op)(struct umbra_builder*, umbra_interval),
                     iv a) {
    float uniforms[] = {a.lo, a.hi};
    float lo_out = 0, hi_out = 0;
    struct umbra_buf lo_buf = {.ptr = &lo_out, .count = 1},
                     hi_buf = {.ptr = &hi_out, .count = 1};

    struct umbra_builder *bld = umbra_builder();
    umbra_ptr32 const u    = umbra_uniforms  (bld, uniforms, 2);
    umbra_ptr32 const lop  = umbra_bind_buf32(bld, &lo_buf);
    umbra_ptr32 const hip  = umbra_bind_buf32(bld, &hi_buf);
    umbra_interval const ia = {umbra_uniform_32(bld, u, 0),
                               umbra_uniform_32(bld, u, 1)};
    umbra_interval const r = op(bld, ia);
    umbra_store_32(bld, lop, r.lo);
    umbra_store_32(bld, hip, r.hi);

    struct umbra_flat_ir *ir = umbra_flat_ir(bld);
    umbra_builder_free(bld);
    struct umbra_backend *be = umbra_backend_interp();
    struct umbra_program *prog = be->compile(be, ir);
    umbra_flat_ir_free(ir);

    prog->queue(prog, 0, 0, 1, 1);
    be->flush(be);
    umbra_program_free(prog);
    umbra_backend_free(be);
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
    iv_equiv(eval_unary(umbra_interval_abs_f32, (iv){-3, 5}), (iv){0, 5}) here;
    iv_equiv(eval_unary(umbra_interval_abs_f32, (iv){ 2, 7}), (iv){2, 7}) here;
    iv_equiv(eval_unary(umbra_interval_abs_f32, (iv){-7,-2}), (iv){2, 7}) here;
    iv_equiv(eval_unary(umbra_interval_abs_f32, (iv){ 0, 5}), (iv){0, 5}) here;
    iv_equiv(eval_unary(umbra_interval_abs_f32, (iv){-5, 0}), (iv){0, 5}) here;
}

TEST(interval_div) {
    iv_equiv(eval_binary(umbra_interval_div_f32, (iv){2,6}, (iv){1,3}),
             (iv){2.0f/3, 6}) here;
    iv_equiv(eval_binary(umbra_interval_div_f32, (iv){-4,2}, (iv){2,5}),
             (iv){-2, 1}) here;
}

static iv eval_square(iv a) {
    float uniforms[] = {a.lo, a.hi};
    float lo_out = 0, hi_out = 0;
    struct umbra_buf lo_buf = {.ptr = &lo_out, .count = 1},
                     hi_buf = {.ptr = &hi_out, .count = 1};

    struct umbra_builder *bld = umbra_builder();
    umbra_ptr32 const u    = umbra_uniforms  (bld, uniforms, 2);
    umbra_ptr32 const lop  = umbra_bind_buf32(bld, &lo_buf);
    umbra_ptr32 const hip  = umbra_bind_buf32(bld, &hi_buf);
    umbra_interval const ia = {umbra_uniform_32(bld, u, 0),
                               umbra_uniform_32(bld, u, 1)};
    umbra_interval const r = umbra_interval_mul_f32(bld, ia, ia);
    umbra_store_32(bld, lop, r.lo);
    umbra_store_32(bld, hip, r.hi);

    struct umbra_flat_ir *ir = umbra_flat_ir(bld);
    umbra_builder_free(bld);
    struct umbra_backend *be = umbra_backend_interp();
    struct umbra_program *prog = be->compile(be, ir);
    umbra_flat_ir_free(ir);

    prog->queue(prog, 0, 0, 1, 1);
    be->flush(be);
    umbra_program_free(prog);
    umbra_backend_free(be);
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
    float uniforms[] = {a, b};
    float lo_out = 0, hi_out = 0;
    struct umbra_buf lo_buf = {.ptr = &lo_out, .count = 1},
                     hi_buf = {.ptr = &hi_out, .count = 1};

    struct umbra_builder *bld = umbra_builder();
    umbra_ptr32 const u    = umbra_uniforms  (bld, uniforms, 2);
    umbra_ptr32 const lop  = umbra_bind_buf32(bld, &lo_buf);
    umbra_ptr32 const hip  = umbra_bind_buf32(bld, &hi_buf);
    umbra_val32 const va = umbra_uniform_32(bld, u, 0),
                      vb = umbra_uniform_32(bld, u, 1);
    umbra_interval const ia = {va, va},
                         ib = {vb, vb};
    umbra_interval const r = op(bld, ia, ib);
    umbra_store_32(bld, lop, r.lo);
    umbra_store_32(bld, hip, r.hi);

    struct umbra_flat_ir *ir = umbra_flat_ir(bld);
    umbra_builder_free(bld);
    struct umbra_backend *be = umbra_backend_interp();
    struct umbra_program *prog = be->compile(be, ir);
    umbra_flat_ir_free(ir);

    prog->queue(prog, 0, 0, 1, 1);
    be->flush(be);
    umbra_program_free(prog);
    umbra_backend_free(be);
    return (iv){lo_out, hi_out};
}

static iv eval_exact_unary(umbra_interval (*op)(struct umbra_builder*, umbra_interval),
                           float a) {
    float uniforms[] = {a};
    float lo_out = 0, hi_out = 0;
    struct umbra_buf lo_buf = {.ptr = &lo_out, .count = 1},
                     hi_buf = {.ptr = &hi_out, .count = 1};

    struct umbra_builder *bld = umbra_builder();
    umbra_ptr32 const u    = umbra_uniforms  (bld, uniforms, 1);
    umbra_ptr32 const lop  = umbra_bind_buf32(bld, &lo_buf);
    umbra_ptr32 const hip  = umbra_bind_buf32(bld, &hi_buf);
    umbra_val32 const va = umbra_uniform_32(bld, u, 0);
    umbra_interval const ia = {va, va};
    umbra_interval const r = op(bld, ia);
    umbra_store_32(bld, lop, r.lo);
    umbra_store_32(bld, hip, r.hi);

    struct umbra_flat_ir *ir = umbra_flat_ir(bld);
    umbra_builder_free(bld);
    struct umbra_backend *be = umbra_backend_interp();
    struct umbra_program *prog = be->compile(be, ir);
    umbra_flat_ir_free(ir);

    prog->queue(prog, 0, 0, 1, 1);
    be->flush(be);
    umbra_program_free(prog);
    umbra_backend_free(be);
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

TEST(interval_loop) {
    // Compute max(a[i]*x - b[i]) over i=0..2 with x as an interval.
    // Data: a={1, -1, 0.5}, b={2, 1, 3}, stored interleaved as [a0,b0,a1,b1,a2,b2].
    // For x=[0,4]: i=0: [0-2, 4-2]=[−2,2], i=1: [−4-1, 0-1]=[−5,−1], i=2: [0-3, 2-3]=[−3,−1]
    // max = [−2, 2].

    // Fixed uniform layout: struct with {data: umbra_buf, n: float, x: float[2]}.
    struct uni {
        float            n;
        int              :32;
        float            x[2];
    };
    int const n_off   = (int)(__builtin_offsetof(struct uni, n) / 4),
              x_off   = (int)(__builtin_offsetof(struct uni, x) / 4);

    float ab[] = {1, 2, -1, 1, 0.5f, 3};
    struct umbra_buf ab_buf = {.ptr = ab, .count = 6};

    struct uni uniforms = {
        .x    = {0.0f, 4.0f},
    };
    { int const count = 3; __builtin_memcpy(&uniforms.n, &count, 4); }

    float lo_out = 0, hi_out = 0;
    struct umbra_buf lo_buf = {.ptr = &lo_out, .count = 1},
                     hi_buf = {.ptr = &hi_out, .count = 1};

    struct umbra_builder *bld = umbra_builder();
    umbra_ptr32 const u    = umbra_uniforms  (bld, &uniforms,
                                              (int)(sizeof uniforms / 4));
    umbra_ptr32 const lop  = umbra_bind_buf32(bld, &lo_buf);
    umbra_ptr32 const hip  = umbra_bind_buf32(bld, &hi_buf);
    umbra_ptr32 const data = umbra_bind_buf32(bld, &ab_buf);
    umbra_val32 const n    = umbra_uniform_32(bld, u, n_off);
    umbra_interval const x = {umbra_uniform_32(bld, u, x_off),
                              umbra_uniform_32(bld, u, x_off + 1)};

    umbra_var32 lo_var = umbra_declare_var32(bld);
    umbra_var32 hi_var = umbra_declare_var32(bld);
    umbra_store_var32(bld, lo_var, umbra_imm_f32(bld, -1e9f));
    umbra_store_var32(bld, hi_var, umbra_imm_f32(bld, -1e9f));

    umbra_val32 const j = umbra_loop(bld, n); {
        umbra_val32 const idx = umbra_mul_i32(bld, j, umbra_imm_i32(bld, 2));
        umbra_interval const a = umbra_interval_exact(umbra_gather_32(bld, data, idx));
        umbra_interval const b_iv = umbra_interval_exact(
            umbra_gather_32(bld, data, umbra_add_i32(bld, idx, umbra_imm_i32(bld, 1))));
        umbra_interval const val = umbra_interval_sub_f32(bld,
                                       umbra_interval_mul_f32(bld, a, x), b_iv);
        umbra_store_var32(bld, lo_var, umbra_max_f32(bld, umbra_load_var32(bld, lo_var), val.lo));
        umbra_store_var32(bld, hi_var, umbra_max_f32(bld, umbra_load_var32(bld, hi_var), val.hi));
    } umbra_end_loop(bld);

    umbra_store_32(bld, lop, umbra_load_var32(bld, lo_var));
    umbra_store_32(bld, hip, umbra_load_var32(bld, hi_var));

    struct umbra_flat_ir *ir = umbra_flat_ir(bld);
    umbra_builder_free(bld);
    struct umbra_backend *be = umbra_backend_interp();
    struct umbra_program *prog = be->compile(be, ir);
    umbra_flat_ir_free(ir);

    prog->queue(prog, 0, 0, 1, 1);
    be->flush(be);

    equiv(lo_out, -2.0f) here;
    equiv(hi_out,  2.0f) here;

    umbra_program_free(prog);
    umbra_backend_free(be);
}
