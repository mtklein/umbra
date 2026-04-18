#include "test.h"
#include "../include/umbra.h"
#include "../src/count.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

static struct umbra_builder* build_srcover(void) {
    struct umbra_builder *b = umbra_builder();

    umbra_val32 px = umbra_load_32(b, (umbra_ptr32){0}), mask = umbra_imm_i32(b, 0xFF);
    umbra_val32 rgba[4] = {
        umbra_and_32(b, px, mask),
        umbra_and_32(b, umbra_shr_u32(b, px, umbra_imm_i32(b, 8)), mask),
        umbra_and_32(b, umbra_shr_u32(b, px, umbra_imm_i32(b, 16)), mask),
        umbra_shr_u32(b, px, umbra_imm_i32(b, 24)),
    };

    umbra_val32 inv255 = umbra_imm_f32(b, 1.0f / 255.0f);
    umbra_val32 sr = umbra_mul_f32(b, umbra_f32_from_i32(b, rgba[0]), inv255),
              sg = umbra_mul_f32(b, umbra_f32_from_i32(b, rgba[1]), inv255),
              sb = umbra_mul_f32(b, umbra_f32_from_i32(b, rgba[2]), inv255),
              sa = umbra_mul_f32(b, umbra_f32_from_i32(b, rgba[3]), inv255),
              dr = umbra_f32_from_f16(b, umbra_load_16(b, (umbra_ptr16){.ix=1})),
              dg = umbra_f32_from_f16(b, umbra_load_16(b, (umbra_ptr16){.ix=2})),
              db = umbra_f32_from_f16(b, umbra_load_16(b, (umbra_ptr16){.ix=3})),
              da = umbra_f32_from_f16(b, umbra_load_16(b, (umbra_ptr16){.ix=4})),
              one = umbra_imm_f32(b, 1.0f), inv_a = umbra_sub_f32(b, one, sa),
              rout = umbra_add_f32(b, sr, umbra_mul_f32(b, dr, inv_a)),
              gout = umbra_add_f32(b, sg, umbra_mul_f32(b, dg, inv_a)),
              bout = umbra_add_f32(b, sb, umbra_mul_f32(b, db, inv_a)),
              aout = umbra_add_f32(b, sa, umbra_mul_f32(b, da, inv_a));
    umbra_store_16(b, (umbra_ptr16){.ix=1}, umbra_f16_from_f32(b, rout));
    umbra_store_16(b, (umbra_ptr16){.ix=2}, umbra_f16_from_f32(b, gout));
    umbra_store_16(b, (umbra_ptr16){.ix=3}, umbra_f16_from_f32(b, bout));
    umbra_store_16(b, (umbra_ptr16){.ix=4}, umbra_f16_from_f32(b, aout));
    return b;
}

static struct test_backends make(struct umbra_builder *builder) {
    struct umbra_flat_ir *ir = umbra_flat_ir(builder);
    umbra_builder_free(builder);
    struct test_backends B = test_backends_make(ir);
    umbra_flat_ir_free(ir);
    return B;
}
static _Bool run(struct test_backends *B, int b, int w, int h, struct umbra_buf buf[]) {
    return test_backends_run(B, b, w, h, buf);
}
static void cleanup(struct test_backends *B) { test_backends_free(B); }

#define BINOP(op, B)                                                                 \
    do {                                                                             \
        struct umbra_builder *b_ = umbra_builder();                                  \
        umbra_val32 x_ = umbra_load_32(b_, (umbra_ptr32){0}),                        \
                    y_ = umbra_load_32(b_, (umbra_ptr32){.ix=1}),                    \
                    r_ = op(b_, x_, y_);                                             \
        umbra_store_32(b_, (umbra_ptr32){.ix=2}, r_);                                \
        B = make(b_);                                                                \
    } while (0)

TEST(test_f32_ops) {
    {
        struct test_backends B;
        BINOP(umbra_mul_f32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            float x[] = {1, 2, 3, 4, 5};
            float y[] = {6, 7, 8, 9, 0}, z[5] = {0};
            if (run(&B, bi, 5, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=5},
                         {.ptr=y, .count=5},
                         {.ptr=z, .count=5},
                     })) {
                equiv(z[0], 6) here;
                equiv(z[1], 14) here;
                equiv(z[2], 24) here;
                equiv(z[3], 36) here;
                equiv(z[4], 0) here;
            }
        }
        cleanup(&B);
    }
    {
        struct test_backends B;
        BINOP(umbra_add_f32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            float x[] = {1, 2, 3};
            float y[] = {10, 20, 30}, z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=3},
                         {.ptr=y, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                equiv(z[0], 11) here;
                equiv(z[1], 22) here;
                equiv(z[2], 33) here;
            }
        }
        cleanup(&B);
    }
    {
        struct test_backends B;
        BINOP(umbra_sub_f32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            float x[] = {10, 20, 30};
            float y[] = {1, 2, 3}, z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=3},
                         {.ptr=y, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                equiv(z[0], 9) here;
                equiv(z[1], 18) here;
                equiv(z[2], 27) here;
            }
        }
        cleanup(&B);
    }
    {
        struct test_backends B;
        BINOP(umbra_div_f32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            float x[] = {10, 20, 30};
            float y[] = {2, 4, 5}, z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=3},
                         {.ptr=y, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                equiv(z[0], 5) here;
                equiv(z[1], 5) here;
                equiv(z[2], 6) here;
            }
        }
        cleanup(&B);
    }
}

TEST(test_i32_ops) {
    {
        struct test_backends B;
        BINOP(umbra_add_i32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int x[] = {1, 2, 3};
            int y[] = {10, 20, 30}, z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=3},
                         {.ptr=y, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                z[0] == 11 here;
                z[1] == 22 here;
                z[2] == 33 here;
            }
        }
        cleanup(&B);
    }
    {
        struct test_backends B;
        BINOP(umbra_sub_i32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int x[] = {10, 20, 30};
            int y[] = {1, 2, 3}, z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=3},
                         {.ptr=y, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                z[0] == 9 here;
                z[1] == 18 here;
                z[2] == 27 here;
            }
        }
        cleanup(&B);
    }
    {
        struct test_backends B;
        BINOP(umbra_mul_i32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int x[] = {2, 3, 4};
            int y[] = {5, 6, 7}, z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=3},
                         {.ptr=y, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                z[0] == 10 here;
                z[1] == 18 here;
                z[2] == 28 here;
            }
        }
        cleanup(&B);
    }
    {
        struct test_backends B;
        BINOP(umbra_shl_i32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int x[] = {1, 3, 7};
            int y[] = {1, 2, 3}, z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=3},
                         {.ptr=y, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                z[0] == 2 here;
                z[1] == 12 here;
                z[2] == 56 here;
            }
        }
        cleanup(&B);
    }
    {
        struct test_backends B;
        BINOP(umbra_shr_u32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int x[] = {-1, 8, 64};
            int y[] = {1, 1, 3}, z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=3},
                         {.ptr=y, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                z[0] == (int)(0xffffffffu >> 1) here;
                z[1] == 4 here;
                z[2] == 8 here;
            }
        }
        cleanup(&B);
    }
    {
        struct test_backends B;
        BINOP(umbra_shr_s32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int x[] = {-8, 8, 64};
            int y[] = {1, 1, 3}, z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=3},
                         {.ptr=y, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                z[0] == -4 here;
                z[1] == 4 here;
                z[2] == 8 here;
            }
        }
        cleanup(&B);
    }
    {
        struct test_backends B;
        BINOP(umbra_and_32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int x[] = {0xff, 0x0f};
            int y[] = {0x0f, 0xff}, z[2] = {0};
            if (run(&B, bi, 2, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=2},
                         {.ptr=y, .count=2},
                         {.ptr=z, .count=2},
                     })) {
                z[0] == 0x0f here;
                z[1] == 0x0f here;
            }
        }
        cleanup(&B);
    }
    {
        struct test_backends B;
        BINOP(umbra_or_32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int x[] = {0xf0, 0x0f};
            int y[] = {0x0f, 0xf0}, z[2] = {0};
            if (run(&B, bi, 2, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=2},
                         {.ptr=y, .count=2},
                         {.ptr=z, .count=2},
                     })) {
                z[0] == 0xff here;
                z[1] == 0xff here;
            }
        }
        cleanup(&B);
    }
    {
        struct test_backends B;
        BINOP(umbra_xor_32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int x[] = {0xff, 0xff};
            int y[] = {0x0f, 0xff}, z[2] = {0};
            if (run(&B, bi, 2, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=2},
                         {.ptr=y, .count=2},
                         {.ptr=z, .count=2},
                     })) {
                z[0] == 0xf0 here;
                z[1] == 0x00 here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32            c = umbra_load_32(builder, (umbra_ptr32){0}),
                              t = umbra_load_32(builder, (umbra_ptr32){.ix=1}),
                              f = umbra_load_32(builder, (umbra_ptr32){.ix=2}),
                              r = umbra_sel_32(builder, c, t, f);
        umbra_store_32(builder, (umbra_ptr32){.ix=3}, r);
        struct test_backends B = make(builder);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int cond[] = {-1, 0, -1};
            int va[] = {10, 20, 30};
            int vb[] = {40, 50, 60};
            int z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=cond, .count=3},
                         {.ptr=va, .count=3},
                         {.ptr=vb, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                z[0] == 10 here;
                z[1] == 50 here;
                z[2] == 30 here;
            }
        }
        cleanup(&B);
    }
}

TEST(test_cmp_i32) {
    {
        struct test_backends B;
        BINOP(umbra_eq_i32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int x[] = {1, 2, 3};
            int y[] = {1, 9, 3}, z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=3},
                         {.ptr=y, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                z[0] == -1 here;
                z[1] == 0 here;
                z[2] == -1 here;
            }
        }
        cleanup(&B);
    }
    {
        struct test_backends B;
        BINOP(umbra_lt_s32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int x[] = {1, 5, 3};
            int y[] = {2, 5, 1}, z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=3},
                         {.ptr=y, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                z[0] == -1 here;
                z[1] == 0 here;
                z[2] == 0 here;
            }
        }
        cleanup(&B);
    }
    {
        struct test_backends B;
        BINOP(umbra_le_s32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int x[] = {1, 5, 3};
            int y[] = {2, 5, 1}, z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=3},
                         {.ptr=y, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                z[0] == -1 here;
                z[1] == -1 here;
                z[2] == 0 here;
            }
        }
        cleanup(&B);
    }
    {
        struct test_backends B;
        BINOP(umbra_lt_u32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int x[] = {1, -1};
            int y[] = {2, 1}, z[2] = {0};
            if (run(&B, bi, 2, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=2},
                         {.ptr=y, .count=2},
                         {.ptr=z, .count=2},
                     })) {
                z[0] == -1 here;
                z[1] == 0 here;
            }
        }
        cleanup(&B);
    }
    {
        struct test_backends B;
        BINOP(umbra_le_u32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int x[] = {1, 2, -1};
            int y[] = {2, 2, 1}, z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=3},
                         {.ptr=y, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                z[0] == -1 here;
                z[1] == -1 here;
                z[2] == 0 here;
            }
        }
        cleanup(&B);
    }
}

TEST(test_cmp_f32) {
    {
        struct test_backends B;
        BINOP(umbra_eq_f32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            float x[] = {1, 2, 3};
            float y[] = {1, 9, 3};
            int   z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=3},
                         {.ptr=y, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                z[0] == -1 here;
                z[1] == 0 here;
                z[2] == -1 here;
            }
        }
        cleanup(&B);
    }
    {
        struct test_backends B;
        BINOP(umbra_lt_f32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            float x[] = {1, 5, 3};
            float y[] = {2, 5, 1};
            int   z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=3},
                         {.ptr=y, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                z[0] == -1 here;
                z[1] == 0 here;
                z[2] == 0 here;
            }
        }
        cleanup(&B);
    }
    {
        struct test_backends B;
        BINOP(umbra_le_f32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            float x[] = {1, 5, 3};
            float y[] = {2, 5, 1};
            int   z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=3},
                         {.ptr=y, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                z[0] == -1 here;
                z[1] == -1 here;
                z[2] == 0 here;
            }
        }
        cleanup(&B);
    }
}

TEST(test_imm) {
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32 v = umbra_imm_i32(builder, 42);
        umbra_store_32(builder, (umbra_ptr32){0}, v);
        struct test_backends B = make(builder);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=z, .count=3},
                     })) {
                z[0] == 42 here;
                z[1] == 42 here;
                z[2] == 42 here;
            }
        }
        cleanup(&B);
    }
}

TEST(test_fma_f32) {
    struct umbra_builder *builder = umbra_builder();
    umbra_val32 x = umbra_load_32(builder, (umbra_ptr32){0}),
              y = umbra_load_32(builder, (umbra_ptr32){.ix=1}),
              w = umbra_load_32(builder, (umbra_ptr32){.ix=2}),
              m = umbra_mul_f32(builder, x, y), r = umbra_add_f32(builder, m, w);
    umbra_store_32(builder, (umbra_ptr32){.ix=3}, r);
    struct test_backends B = make(builder);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        float a[] = {2, 3}, c[] = {4, 5};
        float d[] = {10, 20}, z[2] = {0};
        if (run(&B, bi, 2, 1,
                 (struct umbra_buf[]){
                     {.ptr=a, .count=2},
                     {.ptr=c, .count=2},
                     {.ptr=d, .count=2},
                     {.ptr=z, .count=2},
                 })) {
            equiv(z[0], 18) here;
            equiv(z[1], 35) here;
        }
    }
    cleanup(&B);
}

// Prove each backend emits a genuinely-fused single-rounded fmaf for
// op_fma_f32, not a two-rounded mul+add.  With a = 1 + 2^-22, a*a exact =
// 1 + 2^-21 + 2^-44 but f32 rounds the 2^-44 bit away to 1 + 2^-21.  So
// a*y + (-round(a*a)) returns 0 under two-op math and ~2^-44 under fma;
// a backend that lowered op_fma_f32 to mul+add would zero out.  Same
// pattern as interval_fma_single_rounding in interval_test.c.
// mul(X, X) auto-rewrites to op_square_f32 in the builder.  Sanity-check the
// result matches x*x across every backend.
TEST(test_square_f32) {
    struct umbra_builder *builder = umbra_builder();
    umbra_val32 x = umbra_load_32(builder, (umbra_ptr32){0}),
                r = umbra_mul_f32(builder, x, x);
    umbra_store_32(builder, (umbra_ptr32){.ix=1}, r);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        float a[] = {-3.0f, 0.0f, 1.5f, 4.0f}, z[4] = {0};
        if (run(&B, bi, 4, 1,
                 (struct umbra_buf[]){
                     {.ptr=a, .count=count(a)},
                     {.ptr=z, .count=count(z)},
                 })) {
            equiv(z[0], 9.0f)  here;
            equiv(z[1], 0.0f)  here;
            equiv(z[2], 2.25f) here;
            equiv(z[3], 16.0f) here;
        }
    }
    cleanup(&B);
}

// add(mul(X, X), w) auto-rewrites to op_square_add_f32.  Same single-rounding
// pattern as test_fma_f32_single_rounding but exercising the new op.
TEST(test_square_add_f32_single_rounding) {
    struct umbra_builder *builder = umbra_builder();
    umbra_val32 x = umbra_load_32(builder, (umbra_ptr32){0}),
                w = umbra_load_32(builder, (umbra_ptr32){.ix=1}),
                m = umbra_mul_f32(builder, x, x),
                r = umbra_add_f32(builder, m, w);
    umbra_store_32(builder, (umbra_ptr32){.ix=2}, r);
    struct test_backends B = make(builder);

    float const a = 1.0f + ldexpf(1.0f, -22);
    float const aa_round = a * a;
    float const expected = fmaf(a, a, -aa_round);

    !equiv(expected, 0.0f) here;

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        float ax[] = {a}, aw[] = {-aa_round}, z[1] = {0};
        if (run(&B, bi, 1, 1,
                 (struct umbra_buf[]){
                     {.ptr=ax, .count=1},
                     {.ptr=aw, .count=1},
                     {.ptr=z,  .count=1},
                 })) {
            equiv(z[0], expected) here;
        }
    }
    cleanup(&B);
}

// sub(w, mul(X, X)) auto-rewrites to op_square_sub_f32.
TEST(test_square_sub_f32_single_rounding) {
    struct umbra_builder *builder = umbra_builder();
    umbra_val32 x = umbra_load_32(builder, (umbra_ptr32){0}),
                w = umbra_load_32(builder, (umbra_ptr32){.ix=1}),
                m = umbra_mul_f32(builder, x, x),
                r = umbra_sub_f32(builder, w, m);
    umbra_store_32(builder, (umbra_ptr32){.ix=2}, r);
    struct test_backends B = make(builder);

    float const a = 1.0f + ldexpf(1.0f, -22);
    float const expected = fmaf(-a, a, 1.0f);    // single-rounded 1 - a*a

    // Sequence point between a*a and the subtraction prevents -ffp-contract=on
    // from fusing the two-op C-side computation into an fma.
    float const aa = a * a;
    !equiv(expected, 1.0f - aa) here;

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        float ax[] = {a}, aw[] = {1.0f}, z[1] = {0};
        if (run(&B, bi, 1, 1,
                 (struct umbra_buf[]){
                     {.ptr=ax, .count=1},
                     {.ptr=aw, .count=1},
                     {.ptr=z,  .count=1},
                 })) {
            equiv(z[0], expected) here;
        }
    }
    cleanup(&B);
}

TEST(test_fma_f32_single_rounding) {
    struct umbra_builder *builder = umbra_builder();
    umbra_val32 x = umbra_load_32(builder, (umbra_ptr32){0}),
              y = umbra_load_32(builder, (umbra_ptr32){.ix=1}),
              w = umbra_load_32(builder, (umbra_ptr32){.ix=2}),
              m = umbra_mul_f32(builder, x, y), r = umbra_add_f32(builder, m, w);
    umbra_store_32(builder, (umbra_ptr32){.ix=3}, r);
    struct test_backends B = make(builder);

    float const a = 1.0f + ldexpf(1.0f, -22);
    float const aa_round = a * a;
    float const expected = fmaf(a, a, -aa_round);

    // Sanity: picking this test up means fused and two-op disagree here.
    !equiv(expected, 0.0f) here;

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        float ax[] = {a}, ay[] = {a}, aw[] = {-aa_round}, z[1] = {0};
        if (run(&B, bi, 1, 1,
                 (struct umbra_buf[]){
                     {.ptr=ax, .count=1},
                     {.ptr=ay, .count=1},
                     {.ptr=aw, .count=1},
                     {.ptr=z,  .count=1},
                 })) {
            equiv(z[0], expected) here;
        }
    }
    cleanup(&B);
}

TEST(test_min_max_sqrt_f32) {
    {
        struct test_backends B;
        BINOP(umbra_min_f32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            float x[] = {5, 1, 3};
            float y[] = {2, 4, 3}, z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=3},
                         {.ptr=y, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                equiv(z[0], 2) here;
                equiv(z[1], 1) here;
                equiv(z[2], 3) here;
            }
        }
        cleanup(&B);
    }
    {
        struct test_backends B;
        BINOP(umbra_max_f32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            float x[] = {5, 1, 3};
            float y[] = {2, 4, 3}, z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=3},
                         {.ptr=y, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                equiv(z[0], 5) here;
                equiv(z[1], 4) here;
                equiv(z[2], 3) here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32            x = umbra_load_32(builder, (umbra_ptr32){0}),
                              r = umbra_sqrt_f32(builder, x);
        umbra_store_32(builder, (umbra_ptr32){.ix=1}, r);
        struct test_backends B = make(builder);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            float a[] = {4, 9, 16}, z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=a, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                equiv(z[0], 2) here;
                equiv(z[1], 3) here;
                equiv(z[2], 4) here;
            }
        }
        cleanup(&B);
    }
}

TEST(test_abs_f32) {
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32            x = umbra_load_32(builder, (umbra_ptr32){0}),
                              r = umbra_abs_f32(builder, x);
        umbra_store_32(builder, (umbra_ptr32){.ix=1}, r);
        struct test_backends B = make(builder);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            float a[] = {-1.5f, 2.5f, -0.0f};
            float z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=a, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                equiv(z[0], 1.5f) here;
                equiv(z[1], 2.5f) here;
                equiv(z[2], 0.0f) here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32            x = umbra_load_32(builder, (umbra_ptr32){0}),
                              r = umbra_sub_f32(builder, umbra_imm_f32(builder, 0), x);
        umbra_store_32(builder, (umbra_ptr32){.ix=1}, r);
        struct test_backends B = make(builder);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            float a[] = {-1.5f, 2.5f, 0.0f};
            float z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=a, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                equiv(z[0], 1.5f) here;
                equiv(z[1], -2.5f) here;
                equiv(z[2], 0.0f) here;
            }
        }
        cleanup(&B);
    }
}

TEST(test_round_floor_ceil) {
    float src[] = {1.3f, 1.5f, -1.5f, -2.7f};

#define RFC(op, e0, e1, e2, e3, as_int)                                     \
    do {                                                                    \
        struct umbra_builder *b_ = umbra_builder();                         \
        umbra_val32            x_ = umbra_load_32(b_, (umbra_ptr32){0}); \
        umbra_val32            r_ = op(b_, x_);                              \
        umbra_store_32(b_, (umbra_ptr32){.ix=1}, r_);                       \
        struct test_backends B_ = make(b_);                                          \
        for (int bi_ = 0; bi_ < NUM_BACKENDS; bi_++) {                      \
            float s_[4];                                                    \
            __builtin_memcpy(s_, src, 16);                                  \
            int d_[4] = {0};                                                \
            if (run(&B_, bi_, 4, 1,                                         \
                    (struct umbra_buf[]){                                   \
                        {.ptr=s_, .count=4},                                  \
                        {.ptr=d_, .count=4},                                  \
                    })) {                                                   \
                if (as_int) {                                               \
                    d_[0] == (int)(e0) here;                                \
                    d_[1] == (int)(e1) here;                                \
                    d_[2] == (int)(e2) here;                                \
                    d_[3] == (int)(e3) here;                                \
                } else {                                                    \
                    float g0, g1, g2, g3;                                   \
                    __builtin_memcpy(&g0, d_ + 0, 4);                       \
                    __builtin_memcpy(&g1, d_ + 1, 4);                       \
                    __builtin_memcpy(&g2, d_ + 2, 4);                       \
                    __builtin_memcpy(&g3, d_ + 3, 4);                       \
                    equiv(g0, (float)(e0)) here;                            \
                    equiv(g1, (float)(e1)) here;                            \
                    equiv(g2, (float)(e2)) here;                            \
                    equiv(g3, (float)(e3)) here;                            \
                }                                                           \
            }                                                               \
        }                                                                   \
        cleanup(&B_);                                                       \
    } while (0)

    RFC(umbra_round_f32, 1, 2, -2, -3, 0);
    RFC(umbra_floor_f32, 1, 1, -2, -3, 0);
    RFC(umbra_ceil_f32, 2, 2, -1, -2, 0);
    RFC(umbra_round_i32, 1, 2, -2, -3, 1);
    RFC(umbra_floor_i32, 1, 1, -2, -3, 1);
    RFC(umbra_ceil_i32, 2, 2, -1, -2, 1);
#undef RFC
}

TEST(test_large_n) {
    struct test_backends B;
    BINOP(umbra_add_f32, B);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        float x[100], y[100], z[100];
        for (int i = 0; i < 100; i++) {
            x[i] = (float)i;
            y[i] = (float)(100 - i);
        }
        if (run(&B, bi, 100, 1,
                 (struct umbra_buf[]){
                     {.ptr=x, .count=100},
                     {.ptr=y, .count=100},
                     {.ptr=z, .count=100},
                 })) {
            for (int i = 0; i < 100; i++) { equiv(z[i], 100) here; }
        }
    }
    cleanup(&B);
}

TEST(test_convert) {
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32            x = umbra_load_32(builder, (umbra_ptr32){0});
        umbra_val32            r = umbra_f32_from_i32(builder, x);
        umbra_store_32(builder, (umbra_ptr32){.ix=1}, r);
        struct test_backends B = make(builder);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int   a[] = {1, 255, -3};
            float z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=a, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                equiv(z[0], 1) here;
                equiv(z[1], 255) here;
                equiv(z[2], -3) here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32            x = umbra_load_32(builder, (umbra_ptr32){0});
        umbra_val32            r = umbra_i32_from_f32(builder, x);
        umbra_store_32(builder, (umbra_ptr32){.ix=1}, r);
        struct test_backends B = make(builder);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            float a[] = {1.9f, 255.0f, -3.7f};
            int   z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=a, .count=3},
                         {.ptr=z, .count=3},
                     })) {
                z[0] == 1 here;
                z[1] == 255 here;
                z[2] == -3 here;
            }
        }
        cleanup(&B);
    }
}

TEST(test_dedup) {
    struct umbra_builder *builder = umbra_builder();
    umbra_val32 v1 = umbra_imm_i32(builder, 42), v2 = umbra_imm_i32(builder, 42);
    val_eq(v1, v2) here;
    umbra_val32 c = umbra_imm_i32(builder, 99);
    !val_eq(v1, c) here;
    umbra_val32 x = umbra_load_32(builder, (umbra_ptr32){0}),
              s1 = umbra_add_i32(builder, x, v1), s2 = umbra_add_i32(builder, x, v1);
    val_eq(s1, s2) here;
    umbra_builder_free(builder);
}

TEST(test_constprop) {
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32 x = umbra_imm_i32(builder, 3),
                  y = umbra_imm_i32(builder, 5), s = umbra_add_i32(builder, x, y);
        umbra_store_32(builder, (umbra_ptr32){0}, s);
        struct test_backends B = make(builder);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=z, .count=3},
                     })) {
                z[0] == 8 here;
                z[1] == 8 here;
                z[2] == 8 here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32 a = umbra_imm_f32(builder, 2.0f), y = umbra_imm_f32(builder, 3.0f),
                  s = umbra_mul_f32(builder, a, y);
        umbra_store_32(builder, (umbra_ptr32){0}, s);
        struct test_backends B = make(builder);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            float z[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=z, .count=3},
                     })) {
                equiv(z[0], 6) here;
                equiv(z[1], 6) here;
                equiv(z[2], 6) here;
            }
        }
        cleanup(&B);
    }
}

TEST(test_strength_reduction) {
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32            x = umbra_load_32(builder, (umbra_ptr32){0}),
                  z = umbra_imm_i32(builder, 0), s = umbra_add_i32(builder, x, z);
        val_eq(s, x) here;
        umbra_builder_free(builder);
    }
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32            x = umbra_load_32(builder, (umbra_ptr32){0}),
                  one = umbra_imm_i32(builder, 1), s = umbra_mul_i32(builder, x, one);
        val_eq(s, x) here;
        umbra_builder_free(builder);
    }
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32            x = umbra_load_32(builder, (umbra_ptr32){0}),
                              eight = umbra_imm_i32(builder, 8),
                              s = umbra_mul_i32(builder, x, eight);
        umbra_store_32(builder, (umbra_ptr32){.ix=1}, s);
        struct test_backends B = make(builder);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int32_t a[] = {1, 2, 3, 4, 5}, c[5] = {0};
            if (run(&B, bi, 5, 1,
                     (struct umbra_buf[]){
                         {.ptr=a, .count=5},
                         {.ptr=c, .count=5},
                     })) {
                for (int i = 0; i < 5; i++) { c[i] == a[i] * 8 here; }
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32            x = umbra_load_32(builder, (umbra_ptr32){0}),
                              s = umbra_sub_i32(builder, x, x);
        umbra_store_32(builder, (umbra_ptr32){.ix=1}, s);
        struct test_backends B = make(builder);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int32_t a[] = {1, 2, 3}, c[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=a, .count=3},
                         {.ptr=c, .count=3},
                     })) {
                for (int i = 0; i < 3; i++) { c[i] == 0 here; }
            }
        }
        cleanup(&B);
    }
}

TEST(test_zero_imm) {
    struct umbra_builder *builder = umbra_builder();
    umbra_val32            zero = umbra_imm_i32(builder, 0);
    val_eq(zero, (umbra_val32){0}) here;
    umbra_val32            x = umbra_load_32(builder, (umbra_ptr32){0}),
              r = umbra_eq_i32(builder, x, zero);
    umbra_store_32(builder, (umbra_ptr32){.ix=1}, r);
    struct test_backends B = make(builder);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int a[] = {0, 1, 0}, z[3] = {0};
        if (run(&B, bi, 3, 1,
                 (struct umbra_buf[]){
                     {.ptr=a, .count=3},
                     {.ptr=z, .count=3},
                 })) {
            z[0] == -1 here;
            z[1] == 0 here;
            z[2] == -1 here;
        }
    }
    cleanup(&B);
}

TEST(test_min_f32_self_fold) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 const     x = umbra_load_32(b, (umbra_ptr32){0});
    val_eq(umbra_min_f32(b, x, x), x) here;
    umbra_builder_free(b);
}

TEST(test_max_f32_self_fold) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 const     x = umbra_load_32(b, (umbra_ptr32){0});
    val_eq(umbra_max_f32(b, x, x), x) here;
    umbra_builder_free(b);
}

TEST(test_lt_f32_self_fold) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 const     x = umbra_load_32(b, (umbra_ptr32){0});
    val_eq(umbra_lt_f32(b, x, x), umbra_imm_i32(b, 0)) here;
    umbra_builder_free(b);
}

TEST(test_late_imm_identity) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32            x = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32            z1 = umbra_imm_i32(b, 1);
    umbra_val32            zm = umbra_imm_i32(b, -1);
    val_lt(x, z1) here;
    val_lt(x, zm) here;

    val_eq(umbra_mul_i32(b, x, z1), x) here;
    val_eq(umbra_and_32(b, x, zm), x) here;
    val_eq(umbra_or_32(b, x, zm), zm) here;
    val_eq(umbra_sel_32(b, zm, x, z1), x) here;
    val_eq(umbra_eq_i32(b, x, x), umbra_imm_i32(b, -1)) here;
    val_eq(umbra_div_f32(b, x, umbra_imm_f32(b, 1.0f)), x) here;
    val_eq(umbra_sub_f32(b, x, umbra_imm_f32(b, 0.0f)), x) here;
    umbra_builder_free(b);
}

TEST(test_abs_peepholes) {
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val32            x = umbra_load_32(b, (umbra_ptr32){0});
        umbra_val32            direct = umbra_abs_f32(b, x);
        umbra_val32            mask = umbra_imm_i32(b, 0x7fffffff);

        val_eq(umbra_and_32(b, x, mask), direct) here;
        val_eq(umbra_and_32(b, mask, x), direct) here;
        umbra_builder_free(b);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val32            mask = umbra_imm_i32(b, 0x7fffffff);
        umbra_val32            x = umbra_load_32(b, (umbra_ptr32){0});
        val_lt(mask, x) here;
        umbra_val32 direct = umbra_abs_f32(b, x);
        val_eq(umbra_and_32(b, x, mask), direct) here;
        umbra_builder_free(b);
    }
}

TEST(test_load_8x4) {
    struct umbra_builder *builder = umbra_builder();
    umbra_val32            px_ = umbra_load_32(builder, (umbra_ptr32){0}),
                          m_ = umbra_imm_i32(builder, 0xFF);
    umbra_val32 r = umbra_and_32(builder, px_, m_),
              g = umbra_and_32(builder,
                                umbra_shr_u32(builder, px_, umbra_imm_i32(builder, 8)),
                                m_),
              b = umbra_and_32(builder,
                                umbra_shr_u32(builder, px_, umbra_imm_i32(builder, 16)),
                                m_),
              a = umbra_shr_u32(builder, px_, umbra_imm_i32(builder, 24));
    umbra_store_32(builder, (umbra_ptr32){.ix=1}, r);
    umbra_store_32(builder, (umbra_ptr32){.ix=2}, g);
    umbra_store_32(builder, (umbra_ptr32){.ix=3}, b);
    umbra_store_32(builder, (umbra_ptr32){.ix=4}, a);
    struct test_backends B = make(builder);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t src[] = {
            0xAABBCCDD,
            0x11223344,
            0xFF00FF00,
        };
        int32_t rr[3] = {0}, gg[3] = {0};
        int32_t b_[3] = {0}, aa[3] = {0};
        if (run(&B, bi, 3, 1,
                 (struct umbra_buf[]){
                     {.ptr=src, .count=3},
                     {.ptr=rr, .count=3},
                     {.ptr=gg, .count=3},
                     {.ptr=b_, .count=3},
                     {.ptr=aa, .count=3},
                 })) {
            rr[0] == 0xDD here;
            gg[0] == 0xCC here;
            b_[0] == 0xBB here;
            aa[0] == 0xAA here;
            rr[1] == 0x44 here;
            gg[1] == 0x33 here;
            b_[1] == 0x22 here;
            aa[1] == 0x11 here;
            rr[2] == 0x00 here;
            gg[2] == 0xFF here;
            b_[2] == 0x00 here;
            aa[2] == 0xFF here;
        }
    }
    cleanup(&B);
}

TEST(test_store_8x4) {
    struct umbra_builder *builder = umbra_builder();
    umbra_val32            r = umbra_load_32(builder, (umbra_ptr32){0}),
                          g = umbra_load_32(builder, (umbra_ptr32){.ix=1}),
                          b = umbra_load_32(builder, (umbra_ptr32){.ix=2}),
                          a = umbra_load_32(builder, (umbra_ptr32){.ix=3});
    umbra_val32            m_ = umbra_imm_i32(builder, 0xFF);
    umbra_val32            px_ = umbra_and_32(builder, r, m_);
    px_ = umbra_or_32(builder, px_,
                       umbra_shl_i32(builder, umbra_and_32(builder, g, m_),
                                     umbra_imm_i32(builder, 8)));
    px_ = umbra_or_32(builder, px_,
                       umbra_shl_i32(builder, umbra_and_32(builder, b, m_),
                                     umbra_imm_i32(builder, 16)));
    px_ = umbra_or_32(builder, px_,
                       umbra_shl_i32(builder, a, umbra_imm_i32(builder, 24)));
    umbra_store_32(builder, (umbra_ptr32){.ix=4}, px_);
    struct test_backends B = make(builder);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t  rr[] = {0xDD, 0x44, 0x00};
        int32_t  gg[] = {0xCC, 0x33, 0xFF};
        int32_t  b_[] = {0xBB, 0x22, 0x00};
        int32_t  aa[] = {0xAA, 0x11, 0xFF};
        uint32_t dst[3] = {0};
        if (run(&B, bi, 3, 1,
                 (struct umbra_buf[]){
                     {.ptr=rr, .count=3},
                     {.ptr=gg, .count=3},
                     {.ptr=b_, .count=3},
                     {.ptr=aa, .count=3},
                     {.ptr=dst, .count=3},
                 })) {
            dst[0] == 0xAABBCCDDu here;
            dst[1] == 0x11223344u here;
            dst[2] == 0xFF00FF00u here;
        }
    }
    cleanup(&B);
}

TEST(test_srcover) {
    struct umbra_builder *builder = build_srcover();
    struct test_backends   B = make(builder);
    enum { N = 16 };
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t src_px[N];
        __fp16 dst_r[N], dst_g[N], dst_b[N], dst_a[N];
        for (int i = 0; i < N; i++) {
            src_px[i] = 0x80402010u;
            dst_r[i] = dst_g[i] = dst_b[i] = dst_a[i] = (__fp16)0.5f;
        }
        if (run(&B, bi, N, 1,
                 (struct umbra_buf[]){
                     {.ptr=src_px, .count=count(src_px)},
                     {.ptr=dst_r, .count=count(dst_r)},
                     {.ptr=dst_g, .count=count(dst_g)},
                     {.ptr=dst_b, .count=count(dst_b)},
                     {.ptr=dst_a, .count=count(dst_a)},
                 })) {
            for (int i = 0; i < N; i++) {
                equiv((float)dst_r[i], 0x1.3f4p-2f) here;
                equiv((float)dst_g[i], 0x1.7f8p-2f) here;
                equiv((float)dst_b[i], 0.5f)         here;
                equiv((float)dst_a[i], 0x1.808p-1f)  here;
            }
        }
    }
    cleanup(&B);
}

TEST(test_hash_quality) {
    struct umbra_builder *builder = umbra_builder();
    enum { N = 1000 };
    int ids[N];
    for (int i = 0; i < N; i++) { ids[i] = umbra_imm_i32(builder, i).id; }
    for (int i = 0; i < N; i++) { (umbra_imm_i32(builder, i).id == ids[i]) here; }
    for (int i = 1; i < N; i++) { ids[i] != ids[i - 1] here; }
    umbra_val32 x = umbra_load_32(builder, (umbra_ptr32){0});
    for (int i = 0; i < N; i++) {
        umbra_val32 c = umbra_imm_i32(builder, i);
        umbra_val32 sum = umbra_add_i32(builder, x, c);
        umbra_val32 sum2 = umbra_add_i32(builder, x, c);
        val_eq(sum, sum2) here;
    }
    umbra_builder_free(builder);
}

TEST(test_narrow_16) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32            v = umbra_load_32(b, (umbra_ptr32){0});
    umbra_store_16(b, (umbra_ptr16){.ix=1}, umbra_i16_from_i32(b, v));
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int      src[] = {0x00010002, 0x00030004, 0x00050006};
        uint16_t dst[3] = {0};
        if (run(&B, bi, 3, 1,
                 (struct umbra_buf[]){
                     {.ptr=src, .count=3},
                     {.ptr=dst, .count=3},
                 })) {
            dst[0] == 0x0002 here;
            dst[1] == 0x0004 here;
            dst[2] == 0x0006 here;
        }
    }
    cleanup(&B);
}

TEST(test_mixed_ptr_sizes) {
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32            a = umbra_load_32(builder, (umbra_ptr32){0});
        umbra_val32 r = umbra_add_i32(builder, a, umbra_imm_i32(builder, 1));
        umbra_store_32(builder, (umbra_ptr32){.ix=1}, r);
        struct test_backends B = make(builder);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            uint32_t x[] = {10, 20, 30};
            uint32_t y[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=3},
                         {.ptr=y, .count=3},
                     })) {
                y[0] == 11 here;
                y[1] == 21 here;
                y[2] == 31 here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32 a = umbra_f32_from_f16(builder,
                                      umbra_load_16(builder, (umbra_ptr16){0}));
        umbra_val32 r = umbra_add_f32(builder, a, umbra_imm_f32(builder, 1.0f));
        umbra_store_16(builder, (umbra_ptr16){.ix=1}, umbra_f16_from_f32(builder, r));
        struct test_backends B = make(builder);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            __fp16 x[] = {1, 2, 3}, y[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=3},
                         {.ptr=y, .count=3},
                     })) {
                equiv((float)y[0], 2) here;
                equiv((float)y[1], 3) here;
                equiv((float)y[2], 4) here;
            }
        }
        cleanup(&B);
    }
}

TEST(test_n9) {
    {
        struct test_backends B;
        BINOP(umbra_add_f32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            float x[9], y[9], z[9] = {0};
            for (int i = 0; i < 9; i++) {
                x[i] = (float)(i + 1);
                y[i] = (float)(10 * (i + 1));
            }
            if (run(&B, bi, 9, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=9},
                         {.ptr=y, .count=9},
                         {.ptr=z, .count=9},
                     })) {
                for (int i = 0; i < 9; i++) { equiv(z[i], x[i] + y[i]) here; }
            }
        }
        cleanup(&B);
    }
    {
        struct test_backends B;
        BINOP(umbra_mul_i32, B);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int x[9], y[9], z[9] = {0};
            for (int i = 0; i < 9; i++) {
                x[i] = i + 1;
                y[i] = i + 2;
            }
            if (run(&B, bi, 9, 1,
                     (struct umbra_buf[]){
                         {.ptr=x, .count=9},
                         {.ptr=y, .count=9},
                         {.ptr=z, .count=9},
                     })) {
                for (int i = 0; i < 9; i++) { z[i] == x[i] * y[i] here; }
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32            px_ = umbra_load_32(builder, (umbra_ptr32){0}),
                              m_ = umbra_imm_i32(builder, 0xFF);
        umbra_val32            ch[4] = {
            umbra_and_32(builder, px_, m_),
            umbra_and_32(builder,
                          umbra_shr_u32(builder, px_, umbra_imm_i32(builder, 8)), m_),
            umbra_and_32(builder,
                          umbra_shr_u32(builder, px_, umbra_imm_i32(builder, 16)), m_),
            umbra_shr_u32(builder, px_, umbra_imm_i32(builder, 24)),
        };
        umbra_val32 spx = umbra_and_32(builder, ch[0], m_);
        spx = umbra_or_32(builder, spx,
                           umbra_shl_i32(builder, umbra_and_32(builder, ch[1], m_),
                                         umbra_imm_i32(builder, 8)));
        spx = umbra_or_32(builder, spx,
                           umbra_shl_i32(builder, umbra_and_32(builder, ch[2], m_),
                                         umbra_imm_i32(builder, 16)));
        spx = umbra_or_32(builder, spx,
                           umbra_shl_i32(builder, ch[3], umbra_imm_i32(builder, 24)));
        umbra_store_32(builder, (umbra_ptr32){.ix=1}, spx);
        struct test_backends B = make(builder);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            uint32_t src[9], dst[9] = {0};
            for (int i = 0; i < 9; i++) { src[i] = 0xAABBCC00u + (uint32_t)i; }
            if (run(&B, bi, 9, 1,
                     (struct umbra_buf[]){
                         {.ptr=src, .count=9},
                         {.ptr=dst, .count=9},
                     })) {
                for (int i = 0; i < 9; i++) { dst[i] == src[i] here; }
            }
        }
        cleanup(&B);
    }
}

TEST(test_preamble_pair_alias) {
    struct umbra_builder *builder = umbra_builder();

    enum { N_PRE = 24 };
    umbra_val32 pre[N_PRE];
    for (int i = 0; i < N_PRE; i++) {
        float fv_ = (float)(i + 1);
        pre[i] = umbra_imm_i32(builder,
                               ((union {
                                   float f;
                                   int   i;
                               }){.f = fv_})
                                   .i);
    }

    umbra_val32 x = umbra_load_32(builder, (umbra_ptr32){0});

    umbra_val32 sum = umbra_mul_f32(builder, x, pre[0]);
    for (int i = 1; i < N_PRE; i++) {
        sum = umbra_add_f32(builder, sum, umbra_mul_f32(builder, x, pre[i]));
    }

    umbra_store_32(builder, (umbra_ptr32){.ix=1}, sum);
    struct test_backends B = make(builder);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        float in[16], out[16] = {0};
        for (int i = 0; i < 16; i++) { in[i] = (float)(i + 1); }
        if (run(&B, bi, 16, 1,
                 (struct umbra_buf[]){
                     {.ptr=in, .count=16},
                     {.ptr=out, .count=16},
                 })) {
            float ref[16];
            B.p[0]->queue(B.p[0], 0, 0, 16, 1,
                                (struct umbra_buf[]){
                                    {.ptr=in, .count=16},
                                    {.ptr=ref, .count=16},
                                });
            for (int i = 0; i < 16; i++) { equiv(out[i], ref[i]) here; }
        }
    }
    cleanup(&B);
}

TEST(test_gather_clamp) {
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32            idx = umbra_load_32(builder, (umbra_ptr32){0}),
                              val = umbra_gather_32(builder, (umbra_ptr32){.ix=1}, idx);
        umbra_store_32(builder, (umbra_ptr32){.ix=2}, val);
        struct test_backends B = make(builder);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int32_t indices[4] = {-5, 0, 2, 100};
            int32_t src[3] = {10, 20, 30};
            int32_t dst[4] = {0};
            if (run(&B, bi, 4, 1,
                     (struct umbra_buf[]){
                         {.ptr=indices, .count=count(indices)},
                         {.ptr=src, .count=count(src)},
                         {.ptr=dst, .count=count(dst)},
                     })) {
                dst[0] == 0 here;
                dst[1] == 10 here;
                dst[2] == 30 here;
                dst[3] == 0 here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32            idx = umbra_load_32(builder, (umbra_ptr32){0});
        umbra_val32 val = umbra_i32_from_s16(builder,
                                        umbra_gather_16(builder, (umbra_ptr16){.ix=1}, idx));
        umbra_store_32(builder, (umbra_ptr32){.ix=2}, val);
        struct test_backends B = make(builder);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int32_t indices[4] = {-1, 1, 3, 999};
            int16_t src[3] = {100, 200, 300};
            int32_t dst[4] = {0};
            if (run(&B, bi, 4, 1,
                     (struct umbra_buf[]){
                         {.ptr=indices, .count=count(indices)},
                         {.ptr=src, .count=count(src)},
                         {.ptr=dst, .count=count(dst)},
                     })) {
                dst[0] == 0 here;
                dst[1] == 200 here;
                dst[2] == 0 here;
                dst[3] == 0 here;
            }
        }
        cleanup(&B);
    }
}

TEST(test_gather_clamp_zero_sz) {
    // gather_uniform with negative index → clamped to 0.
    struct umbra_builder *b = umbra_builder();
    umbra_val32            ix = umbra_uniform_32(b, (umbra_ptr32){0}, 0);
    umbra_val32            v = umbra_gather_32(b, (umbra_ptr32){.ix=1}, ix);
    umbra_store_32(b, (umbra_ptr32){.ix=2}, v);
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t neg_idx[] = {-10};
        int32_t src[3] = {100, 200, 300};
        int32_t dst[1] = {0};
        if (run(&B, bi, 1, 1, (struct umbra_buf[]){
            {.ptr=neg_idx, .count=1},
            {.ptr=src, .count=count(src)},
            {.ptr=dst, .count=1},
        })) {
            dst[0] == 0 here;
        }
    }

    // gather_uniform with over-range index → OOB returns 0.
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t big_idx[] = {999};
        int32_t src[3] = {100, 200, 300};
        int32_t dst[1] = {0};
        if (run(&B, bi, 1, 1, (struct umbra_buf[]){
            {.ptr=big_idx, .count=1},
            {.ptr=src, .count=count(src)},
            {.ptr=dst, .count=1},
        })) {
            dst[0] == 0 here;
        }
    }
    cleanup(&B);
}

TEST(test_offset_load_store) {
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32            ix = umbra_x(builder);
        umbra_val32            off = umbra_uniform_32(builder, (umbra_ptr32){.ix=1}, 0);
        umbra_val32            ixo = umbra_add_i32(builder, ix, off);
        umbra_val32 val = umbra_i32_from_s16(builder,
                                        umbra_gather_16(builder, (umbra_ptr16){0}, ixo));
        umbra_store_32(builder, (umbra_ptr32){.ix=2}, val);
        struct test_backends B = make(builder);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int16_t src[16] = {
                10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
            };
            int32_t uni[1] = {4};
            int32_t dst[8] = {0};
            if (run(&B, bi, 8, 1,
                     (struct umbra_buf[]){
                         {.ptr=src, .count=count(src)},
                         {.ptr=uni, .count=count(uni)},
                         {.ptr=dst, .count=count(dst)},
                     })) {
                for (int k = 0; k < 8; k++) { dst[k] == 14 + k here; }
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32            ix = umbra_x(builder);
        umbra_val32            off = umbra_uniform_32(builder, (umbra_ptr32){.ix=1}, 0);
        umbra_val32            ixo = umbra_add_i32(builder, ix, off);
        umbra_val32            val = umbra_gather_32(builder, (umbra_ptr32){0}, ixo);
        umbra_store_32(builder, (umbra_ptr32){.ix=2}, val);
        struct test_backends B = make(builder);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int32_t src[16] = {
                100, 101, 102, 103, 104, 105, 106, 107,
                108, 109, 110, 111, 112, 113, 114, 115,
            };
            int32_t uni[1] = {3};
            int32_t dst[8] = {0};
            if (run(&B, bi, 8, 1,
                     (struct umbra_buf[]){
                         {.ptr=src, .count=count(src)},
                         {.ptr=uni, .count=count(uni)},
                         {.ptr=dst, .count=count(dst)},
                     })) {
                for (int k = 0; k < 8; k++) { dst[k] == 103 + k here; }
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32            ix = umbra_x(builder);
        umbra_val32            off = umbra_uniform_32(builder, (umbra_ptr32){.ix=1}, 0);
        umbra_val32            ixo = umbra_add_i32(builder, ix, off);
        umbra_val32 val = umbra_f32_from_f16(builder,
                                        umbra_gather_16(builder, (umbra_ptr16){0}, ixo));
        umbra_store_16(builder, (umbra_ptr16){.ix=2}, umbra_f16_from_f32(builder, val));
        struct test_backends B = make(builder);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            uint16_t src[16];
            for (int k = 0; k < 16; k++) {
                __fp16 h = (__fp16)(k + 10);
                __builtin_memcpy(&src[k], &h, 2);
            }
            int32_t  uni[1] = {5};
            uint16_t dst[8] = {0};
            if (run(&B, bi, 8, 1,
                     (struct umbra_buf[]){
                         {.ptr=src, .count=count(src)},
                         {.ptr=uni, .count=count(uni)},
                         {.ptr=dst, .count=count(dst)},
                     })) {
                for (int k = 0; k < 8; k++) {
                    __fp16 h;
                    __builtin_memcpy(&h, &dst[k], 2);
                    equiv((float)h, (float)(15 + k)) here;
                }
            }
        }
        cleanup(&B);
    }
}

TEST(test_shift_imm) {
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32            x = umbra_load_32(builder, (umbra_ptr32){0});
        umbra_val32 r = umbra_shl_i32(builder, x, umbra_imm_i32(builder, 8));
        umbra_store_32(builder, (umbra_ptr32){.ix=1}, r);
        struct test_backends B = make(builder);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            uint32_t src[] = {1, 2, 3, 0xff};
            uint32_t dst[4] = {0};
            if (run(&B, bi, 4, 1,
                     (struct umbra_buf[]){
                         {.ptr=src, .count=4},
                         {.ptr=dst, .count=4},
                     })) {
                dst[0] == 0x100u here;
                dst[1] == 0x200u here;
                dst[2] == 0x300u here;
                dst[3] == 0xff00u here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32            x = umbra_load_32(builder, (umbra_ptr32){0});
        umbra_val32 r = umbra_shr_u32(builder, x, umbra_imm_i32(builder, 8));
        umbra_store_32(builder, (umbra_ptr32){.ix=1}, r);
        struct test_backends B = make(builder);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            uint32_t src[] = {0x100, 0x200, 0xff00};
            uint32_t dst[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=src, .count=3},
                         {.ptr=dst, .count=3},
                     })) {
                dst[0] == 1 here;
                dst[1] == 2 here;
                dst[2] == 0xff here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val32            x = umbra_load_32(builder, (umbra_ptr32){0});
        umbra_val32 r = umbra_shr_s32(builder, x, umbra_imm_i32(builder, 4));
        umbra_store_32(builder, (umbra_ptr32){.ix=1}, r);
        struct test_backends B = make(builder);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            uint32_t src[] = {0x80, 0xfffffff0u};
            uint32_t dst[2] = {0};
            if (run(&B, bi, 2, 1,
                     (struct umbra_buf[]){
                         {.ptr=src, .count=2},
                         {.ptr=dst, .count=2},
                     })) {
                dst[0] == 8 here;
                dst[1] == 0xffffffffu here;
            }
        }
        cleanup(&B);
    }
}

TEST(test_gather_deref_large) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32            idx = umbra_load_32(b, (umbra_ptr32){0});
    umbra_ptr16           src = umbra_deref_ptr16(b, (umbra_ptr32){.ix=1}, 0);
    umbra_val32            val = umbra_i32_from_s16(b, umbra_gather_16(b, src, idx));
    umbra_store_32(b, (umbra_ptr32){.ix=2}, val);
    struct test_backends B = make(b);

    enum { N = 33000 };
    void *data = malloc((size_t)N * sizeof(int16_t));
    { int16_t v;
      v = 10; __builtin_memcpy((char*)data + 0     * 2, &v, 2);
      v = 20; __builtin_memcpy((char*)data + 100   * 2, &v, 2);
      v = 30; __builtin_memcpy((char*)data + 32800 * 2, &v, 2);
      v = 42; __builtin_memcpy((char*)data + (N-1) * 2, &v, 2);
    }

    int32_t indices[4] = {0, 100, 32800, N - 1};
    int32_t dst[4] = {0};

    struct umbra_buf deref = {.ptr=data, .count=N};

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (run(&B, bi, 4, 1,
                 (struct umbra_buf[]){
                     {.ptr=indices, .count=count(indices)},
                     {.ptr=&deref, .count=(int)(sizeof deref / 4)},
                     {.ptr=dst, .count=count(dst)},
                 })) {
            dst[0] == 10 here;
            dst[1] == 20 here;
            dst[2] == 30 here;
            dst[3] == 42 here;
        }
    }

    free(data);
    cleanup(&B);
}

TEST(test_imm_fused) {
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val32            x = umbra_load_32(b, (umbra_ptr32){0});
        umbra_val32            r = umbra_div_f32(b, x, umbra_imm_f32(b, 2.0f));
        umbra_store_32(b, (umbra_ptr32){.ix=1}, r);
        struct test_backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            float src[] = {10, 20, 30}, dst[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=src, .count=3},
                         {.ptr=dst, .count=3},
                     })) {
                equiv(dst[0], 5) here;
                equiv(dst[1], 10) here;
                equiv(dst[2], 15) here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val32            x = umbra_load_32(b, (umbra_ptr32){0});
        umbra_val32            r = umbra_sub_i32(b, x, umbra_imm_i32(b, 5));
        umbra_store_32(b, (umbra_ptr32){.ix=1}, r);
        struct test_backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int src[] = {10, 20, 30}, dst[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=src, .count=3},
                         {.ptr=dst, .count=3},
                     })) {
                dst[0] == 5 here;
                dst[1] == 15 here;
                dst[2] == 25 here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val32            x = umbra_load_32(b, (umbra_ptr32){0});
        umbra_val32            r = umbra_or_32(b, x, umbra_imm_i32(b, 0xf0));
        umbra_store_32(b, (umbra_ptr32){.ix=1}, r);
        struct test_backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int src[] = {0x0f, 0x00, 0xff}, dst[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=src, .count=3},
                         {.ptr=dst, .count=3},
                     })) {
                dst[0] == 0xff here;
                dst[1] == 0xf0 here;
                dst[2] == 0xff here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val32            x = umbra_load_32(b, (umbra_ptr32){0});
        umbra_val32            r = umbra_eq_f32(b, x, umbra_imm_f32(b, 2.0f));
        umbra_store_32(b, (umbra_ptr32){.ix=1}, r);
        struct test_backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            float src[] = {1, 2, 3};
            int   dst[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=src, .count=3},
                         {.ptr=dst, .count=3},
                     })) {
                dst[0] == 0 here;
                dst[1] == -1 here;
                dst[2] == 0 here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val32            x = umbra_load_32(b, (umbra_ptr32){0});
        umbra_val32            r = umbra_le_f32(b, x, umbra_imm_f32(b, 2.0f));
        umbra_store_32(b, (umbra_ptr32){.ix=1}, r);
        struct test_backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            float src[] = {1, 2, 3};
            int   dst[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=src, .count=3},
                         {.ptr=dst, .count=3},
                     })) {
                dst[0] == -1 here;
                dst[1] == -1 here;
                dst[2] == 0 here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val32            x = umbra_load_32(b, (umbra_ptr32){0});
        umbra_val32            r = umbra_lt_s32(b, x, umbra_imm_i32(b, 5));
        umbra_store_32(b, (umbra_ptr32){.ix=1}, r);
        struct test_backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int src[] = {3, 5, 7}, dst[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=src, .count=3},
                         {.ptr=dst, .count=3},
                     })) {
                dst[0] == -1 here;
                dst[1] == 0 here;
                dst[2] == 0 here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val32            x = umbra_load_32(b, (umbra_ptr32){0});
        umbra_val32            r = umbra_le_s32(b, x, umbra_imm_i32(b, 5));
        umbra_store_32(b, (umbra_ptr32){.ix=1}, r);
        struct test_backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int src[] = {3, 5, 7}, dst[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=src, .count=3},
                         {.ptr=dst, .count=3},
                     })) {
                dst[0] == -1 here;
                dst[1] == -1 here;
                dst[2] == 0 here;
            }
        }
        cleanup(&B);
    }
}

TEST(test_cmp_zero) {
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val32            x = umbra_load_32(b, (umbra_ptr32){0});
        umbra_val32            r = umbra_lt_s32(b, x, umbra_imm_i32(b, 0));
        umbra_store_32(b, (umbra_ptr32){.ix=1}, r);
        struct test_backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int src[] = {-1, 0, 1}, dst[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=src, .count=3},
                         {.ptr=dst, .count=3},
                     })) {
                dst[0] == -1 here;
                dst[1] == 0 here;
                dst[2] == 0 here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val32            x = umbra_load_32(b, (umbra_ptr32){0});
        umbra_val32            r = umbra_le_f32(b, x, umbra_imm_f32(b, 0.0f));
        umbra_store_32(b, (umbra_ptr32){.ix=1}, r);
        struct test_backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            float src[] = {-1, 0, 1};
            int   dst[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=src, .count=3},
                         {.ptr=dst, .count=3},
                     })) {
                dst[0] == -1 here;
                dst[1] == -1 here;
                dst[2] == 0 here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val32            x = umbra_load_32(b, (umbra_ptr32){0});
        umbra_val32            r = umbra_eq_f32(b, x, umbra_imm_f32(b, 0.0f));
        umbra_store_32(b, (umbra_ptr32){.ix=1}, r);
        struct test_backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            float src[] = {-1, 0, 1};
            int   dst[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=src, .count=3},
                         {.ptr=dst, .count=3},
                     })) {
                dst[0] == 0 here;
                dst[1] == -1 here;
                dst[2] == 0 here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val32            x = umbra_load_32(b, (umbra_ptr32){0});
        umbra_val32            r = umbra_le_s32(b, x, umbra_imm_i32(b, 0));
        umbra_store_32(b, (umbra_ptr32){.ix=1}, r);
        struct test_backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int src[] = {-1, 0, 1}, dst[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=src, .count=3},
                         {.ptr=dst, .count=3},
                     })) {
                dst[0] == -1 here;
                dst[1] == -1 here;
                dst[2] == 0 here;
            }
        }
        cleanup(&B);
    }
}

TEST(test_imm_broadcast) {
    int patterns[] = {
        (int)0xfffe0000u,
        (int)0x7fffffffu,
    };
    for (int pi = 0; pi < 2; pi++) {
        struct umbra_builder *b = umbra_builder();
        umbra_val32            v = umbra_imm_i32(b, patterns[pi]);
        umbra_store_32(b, (umbra_ptr32){0}, v);
        struct test_backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int dst[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=dst, .count=3},
                     })) {
                dst[0] == patterns[pi] here;
                dst[1] == patterns[pi] here;
                dst[2] == patterns[pi] here;
            }
        }
        cleanup(&B);
    }
}

// Test x86 pool_broadcast special cases: vpcmpeqd for all-ones,
// vpcmpeqd+vpsrld for all-ones>>n, vpcmpeqd+vpslld for all-ones<<n.
// Storing an immediate directly forces op_imm_32 through pool_broadcast.
TEST(test_pool_broadcast_allones) {
    // 0xffffffff: vpcmpeqd(d,d,d)
    // 0x7fffffff: vpcmpeqd + vpsrld(1)
    // 0xfffffffe: vpcmpeqd + vpslld(1)
    int patterns[] = {
        (int)0xffffffffu,
        (int)0x7fffffffu,
        (int)0xfffffffeu,
    };
    for (int pi = 0; pi < 3; pi++) {
        struct umbra_builder *b = umbra_builder();
        umbra_val32 v = umbra_imm_i32(b, patterns[pi]);
        umbra_store_32(b, (umbra_ptr32){0}, v);
        struct test_backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int dst[5] = {0};
            if (run(&B, bi, 5, 1,
                     (struct umbra_buf[]){
                         {.ptr=dst, .count=5},
                     })) {
                dst[0] == patterns[pi] here;
                dst[1] == patterns[pi] here;
                dst[2] == patterns[pi] here;
                dst[3] == patterns[pi] here;
                dst[4] == patterns[pi] here;
            }
        }
        cleanup(&B);
    }
}

TEST(test_codegen_regalloc) {
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val32            x = umbra_load_32(b, (umbra_ptr32){0});
        umbra_val32            y = umbra_load_32(b, (umbra_ptr32){.ix=1});
        umbra_val32            z = umbra_load_32(b, (umbra_ptr32){.ix=2});
        umbra_val32            r = umbra_sub_f32(b, z, umbra_mul_f32(b, x, y));
        umbra_val32            s = umbra_add_f32(b, r, z);
        umbra_val32            u = umbra_add_f32(b, s, y);
        umbra_store_32(b, (umbra_ptr32){.ix=3}, u);
        struct test_backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            float a[] = {2, 3}, c[] = {5, 6}, d[] = {100, 200};
            float dst[4] = {0};
            if (run(&B, bi, 2, 1,
                     (struct umbra_buf[]){
                         {.ptr=a, .count=2},
                         {.ptr=c, .count=2},
                         {.ptr=d, .count=2},
                         {.ptr=dst, .count=2},
                     })) {
                equiv(dst[0], 90 + 100 + 5) here;
                equiv(dst[1], 182 + 200 + 6) here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val32            x = umbra_load_32(b, (umbra_ptr32){0});
        umbra_val32            y = umbra_load_32(b, (umbra_ptr32){.ix=1});
        umbra_val32            z = umbra_load_32(b, (umbra_ptr32){.ix=2});
        umbra_val32            r = umbra_sub_f32(b, z, umbra_mul_f32(b, x, y));
        umbra_val32            s = umbra_add_f32(b, r, x);
        umbra_val32            u = umbra_add_f32(b, s, y);
        umbra_val32            w = umbra_add_f32(b, u, z);
        umbra_store_32(b, (umbra_ptr32){.ix=3}, w);
        struct test_backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            float a[] = {2, 3}, c[] = {5, 6}, d[] = {100, 200};
            float dst[4] = {0};
            if (run(&B, bi, 2, 1,
                     (struct umbra_buf[]){
                         {.ptr=a, .count=2},
                         {.ptr=c, .count=2},
                         {.ptr=d, .count=2},
                         {.ptr=dst, .count=2},
                     })) {
                equiv(dst[0], 90 + 2 + 5 + 100) here;
                equiv(dst[1], 182 + 3 + 6 + 200) here;
            }
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val32            x = umbra_load_32(b, (umbra_ptr32){0});
        umbra_val32            y = umbra_load_32(b, (umbra_ptr32){.ix=1});
        umbra_val32            z = umbra_load_32(b, (umbra_ptr32){.ix=2});
        umbra_val32            r = umbra_sel_32(b, x, y, z);
        umbra_val32            s = umbra_add_i32(b, r, x);
        umbra_val32            u = umbra_add_i32(b, s, y);
        umbra_store_32(b, (umbra_ptr32){.ix=3}, u);
        struct test_backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int m[] = {-1, 0}, t[] = {10, 20}, f[] = {30, 40};
            int dst[2] = {0};
            if (run(&B, bi, 2, 1,
                     (struct umbra_buf[]){
                         {.ptr=m, .count=2},
                         {.ptr=t, .count=2},
                         {.ptr=f, .count=2},
                         {.ptr=dst, .count=2},
                     })) {
                dst[0] == 10 + (-1) + 10 here;
                dst[1] == 40 + 0 + 20 here;
            }
        }
        cleanup(&B);
    }
}

TEST(test_fms) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32            vx = umbra_load_32(b, (umbra_ptr32){0}),
                          vy = umbra_load_32(b, (umbra_ptr32){.ix=1}),
                          vz = umbra_load_32(b, (umbra_ptr32){.ix=2});
    umbra_val32            r = umbra_sub_f32(b, vz, umbra_mul_f32(b, vx, vy));
    umbra_store_32(b, (umbra_ptr32){.ix=3}, r);
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        float a[] = {2, 3, 4}, c[] = {5, 6, 7}, d[] = {100, 200, 300}, dst[3] = {0};
        if (run(&B, bi, 3, 1,
                 (struct umbra_buf[]){
                     {.ptr=a, .count=3},
                     {.ptr=c, .count=3},
                     {.ptr=d, .count=3},
                     {.ptr=dst, .count=3},
                 })) {
            equiv(dst[0], 90) here;
            equiv(dst[1], 182) here;
            equiv(dst[2], 272) here;
        }
    }
    cleanup(&B);
}

TEST(test_movi_patterns) {
    int patterns[] = {
        0x0000ff00,  0x00ff0000,        (int)0xff000000u, ~0x000000ff, ~0x0000ff00,
        ~0x00ff0000, ~(int)0xff000000u, (int)0xfffe0000u, 0x12345678,
    };
    for (int pi = 0; pi < 9; pi++) {
        struct umbra_builder *b = umbra_builder();
        umbra_val32            x = umbra_load_32(b, (umbra_ptr32){0});
        umbra_val32            r = umbra_or_32(b, x, umbra_imm_i32(b, patterns[pi]));
        umbra_store_32(b, (umbra_ptr32){.ix=1}, r);
        struct test_backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int src[] = {0, 0, 0}, dst[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=src, .count=3},
                         {.ptr=dst, .count=3},
                     })) {
                dst[0] == patterns[pi] here;
                dst[1] == patterns[pi] here;
                dst[2] == patterns[pi] here;
            }
        }
        cleanup(&B);
    }
}

TEST(test_uni_16) {
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val32 idx = umbra_uniform_32(b, (umbra_ptr32){0}, 0);
        umbra_val32 v = umbra_i32_from_u16(b, umbra_gather_16(b, (umbra_ptr16){.ix=1}, idx));
        umbra_store_32(b, (umbra_ptr32){.ix=2}, v);
        struct test_backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int      idx_val[] = {1};
            uint16_t src[] = {100, 200, 300, 400};
            int      dst[3] = {0};
            if (run(&B, bi, 3, 1,
                     (struct umbra_buf[]){
                         {.ptr=idx_val, .count=1},
                         {.ptr=src, .count=4},
                         {.ptr=dst, .count=3},
                     })) {
                dst[0] == 200 here;
                dst[1] == 200 here;
                dst[2] == 200 here;
            }
        }
        cleanup(&B);
    }
}

TEST(test_dump) {
    FILE *f = fopen("/dev/null", "w");
    if (f) {
        struct umbra_builder *b = umbra_builder();
        umbra_val32            x = umbra_load_32(b, (umbra_ptr32){0});
        umbra_val32            r = umbra_add_f32(b, x, umbra_imm_f32(b, 1.0f));
        // shl_i32 by an immediate folds to op_shl_i32_imm in the builder, which
        // dump_insts has its own switch case for; covering it here keeps the
        // dump table from rotting.
        umbra_val32            s = umbra_shl_i32(b, x, umbra_imm_i32(b, 3));
        umbra_store_32(b, (umbra_ptr32){.ix=1}, r);
        umbra_store_32(b, (umbra_ptr32){.ix=2}, s);
        umbra_builder_dump(b, f);

        struct umbra_flat_ir *ir = umbra_flat_ir(b);
        umbra_builder_free(b);
        umbra_flat_ir_dump(ir, f);
        umbra_flat_ir_free(ir);
        fclose(f);
    }
}

TEST(test_xy) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32            x = umbra_x(b);
    umbra_val32            y = umbra_y(b);
    umbra_store_32(b, (umbra_ptr32){0}, x);
    umbra_store_32(b, (umbra_ptr32){.ix=1}, y);
    struct test_backends B = make(b);

    enum { W = 5, H = 3, N = W * H };
    int32_t xbuf[N], ybuf[N];
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(xbuf, 0, sizeof xbuf);
        __builtin_memset(ybuf, 0, sizeof ybuf);
        if (run(&B, bi, W, H,
                 (struct umbra_buf[]){
                     {.ptr=xbuf, .count=count(xbuf), .stride=W},
                     {.ptr=ybuf, .count=count(ybuf), .stride=W},
                 })) {
            for (int j = 0; j < N; j++) {
                xbuf[j] == j % W here;
                ybuf[j] == j / W here;
            }
        }
    }
    cleanup(&B);
}

TEST(test_load_next_32) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32            v = umbra_load_32(b, (umbra_ptr32){0});
    umbra_store_32(b, (umbra_ptr32){.ix=1}, v);
    struct test_backends B = make(b);

    int32_t src[16], dst[16];
    for (int i = 0; i < 16; i++) { src[i] = i * 7 + 3; }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (run(&B, bi, 16, 1, (struct umbra_buf[]){
            {.ptr=src, .count=count(src)},
            {.ptr=dst, .count=count(dst)},
        })) {
            for (int i = 0; i < 16; i++) { dst[i] == src[i] here; }
        }
    }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (run(&B, bi, 4, 4, (struct umbra_buf[]){
            {.ptr=src, .count=count(src), .stride=4},
            {.ptr=dst, .count=count(dst), .stride=4},
        })) {
            for (int i = 0; i < 16; i++) { dst[i] == src[i] here; }
        }
    }
    cleanup(&B);
}

TEST(test_load_next_16) {
    struct umbra_builder *b = umbra_builder();
    umbra_val16            v = umbra_load_16(b, (umbra_ptr16){0});
    umbra_store_16(b, (umbra_ptr16){.ix=1}, v);
    struct test_backends B = make(b);

    int16_t src[16], dst[16];
    for (int i = 0; i < 16; i++) { src[i] = (int16_t)(i * 7 + 3); }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (run(&B, bi, 16, 1, (struct umbra_buf[]){
            {.ptr=src, .count=count(src)},
            {.ptr=dst, .count=count(dst)},
        })) {
            for (int i = 0; i < 16; i++) { dst[i] == src[i] here; }
        }
    }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (run(&B, bi, 4, 4, (struct umbra_buf[]){
            {.ptr=src, .count=count(src), .stride=4},
            {.ptr=dst, .count=count(dst), .stride=4},
        })) {
            for (int i = 0; i < 16; i++) { dst[i] == src[i] here; }
        }
    }
    cleanup(&B);
}

TEST(test_load_store_16_all_bits) {
    struct umbra_builder *b = umbra_builder();
    umbra_val16            v = umbra_load_16(b, (umbra_ptr16){0});
    umbra_store_16(b, (umbra_ptr16){.ix=1}, v);
    struct test_backends B = make(b);

    enum { N = 65536 };
    uint16_t *src = malloc(N * sizeof *src),
             *dst = malloc(N * sizeof *dst);
    for (int i = 0; i < N; i++) { src[i] = (uint16_t)i; }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, N * sizeof *dst);
        if (run(&B, bi, N, 1, (struct umbra_buf[]){
            {.ptr=src, .count=N},
            {.ptr=dst, .count=N},
        })) {
            for (int i = 0; i < N; i++) { dst[i] == src[i] here; }
        }
    }
    free(src);
    free(dst);
    cleanup(&B);
}

TEST(test_load_store_8x4) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 r, g, bl, a;
    umbra_load_8x4(b, (umbra_ptr32){0}, &r, &g, &bl, &a);
    umbra_store_8x4(b, (umbra_ptr32){.ix=1}, r, g, bl, a);
    struct test_backends B = make(b);

    uint32_t src[8], dst[8];
    for (int i = 0; i < 8; i++) { src[i] = 0xFF804020u + (unsigned)i; }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (run(&B, bi, 8, 1, (struct umbra_buf[]){
            {.ptr=src, .count=count(src)},
            {.ptr=dst, .count=count(dst)},
        })) {
            for (int i = 0; i < 8; i++) { dst[i] == src[i] here; }
        }
    }
    cleanup(&B);
}

TEST(test_load_store_16x4_planar) {
    struct umbra_builder *b = umbra_builder();
    umbra_val16 r, g, bl, a;
    umbra_load_16x4_planar(b, (umbra_ptr16){0}, &r, &g, &bl, &a);
    umbra_store_16x4_planar(b, (umbra_ptr16){.ix=1}, r, g, bl, a);
    struct test_backends B = make(b);

    enum { W = 35, H = 4 };
    __fp16 src[W * H * 4], dst[W * H * 4];
    for (int i = 0; i < W * H; i++) {
        src[i + W*H*0] = (__fp16)(0.1f * (float)(i + 1));
        src[i + W*H*1] = (__fp16)(0.2f * (float)(i + 1));
        src[i + W*H*2] = (__fp16)(0.3f * (float)(i + 1));
        src[i + W*H*3] = (__fp16)(0.9f + 0.001f * (float)i);
    }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (run(&B, bi, W, H, (struct umbra_buf[]){
            {.ptr=src, .count=count(src), .stride=W},
            {.ptr=dst, .count=count(dst), .stride=W},
        })) {
            0 == __builtin_memcmp(dst, src, sizeof src) here;
        }
    }
    cleanup(&B);
}

TEST(test_load_store_16x4) {
    struct umbra_builder *b = umbra_builder();
    umbra_val16 r, g, bl, a;
    umbra_load_16x4(b, (umbra_ptr64){0}, &r, &g, &bl, &a);
    umbra_store_16x4(b, (umbra_ptr64){.ix=1}, r, g, bl, a);
    struct test_backends B = make(b);

    enum { W = 35 };
    __fp16 src[W * 4], dst[W * 4];
    for (int i = 0; i < W; i++) {
        src[i * 4 + 0] = (__fp16)(0.1f * (float)(i + 1));
        src[i * 4 + 1] = (__fp16)(0.2f * (float)(i + 1));
        src[i * 4 + 2] = (__fp16)(0.3f * (float)(i + 1));
        src[i * 4 + 3] = (__fp16)(0.9f + 0.001f * (float)i);
    }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (run(&B, bi, W, 1, (struct umbra_buf[]){
            {.ptr=src, .count=count(src) / 4},  // count is 16x4-pixel count, not fp16-element count
            {.ptr=dst, .count=count(dst) / 4},
        })) {
            0 == __builtin_memcmp(dst, src, sizeof src) here;
        }
    }
    cleanup(&B);
}

TEST(test_load_stride_neq_w) {
    // Regression: add(mul(y, rs_uniform), x) was optimized to a contiguous
    // load using the linear loop counter.  When rs != w, this is wrong.
    struct umbra_builder *b = umbra_builder();
    int const ri = 0;
    umbra_val32 x = umbra_x(b);
    umbra_val32 y = umbra_y(b);
    umbra_val32 rs = umbra_uniform_32(b, (umbra_ptr32){0}, ri);
    umbra_val32 ix = umbra_add_i32(b, umbra_mul_i32(b, y, rs), x);
    umbra_val32 v = umbra_gather_32(b, (umbra_ptr32){.ix=1}, ix);
    umbra_store_32(b, (umbra_ptr32){.ix=2}, v);
    struct test_backends B = make(b);

    // w=4, h=2, rs=8 (padded rows).
    // Row 0 at src[0..3], row 1 at src[8..11].
    int32_t src[16] = {0};
    src[0] = 10; src[1] = 11; src[2] = 12; src[3] = 13;
    src[8] = 20; src[9] = 21; src[10] = 22; src[11] = 23;

    int32_t expected[8] = {10, 11, 12, 13, 20, 21, 22, 23};
    int32_t dst[8];

    uint64_t uni_[2] = {0};
    char    *uni = (char *)uni_;
    int32_t  rs_val = 8;
    __builtin_memcpy(uni + ri * 4, &rs_val, 4);

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (run(&B, bi, 4, 2, (struct umbra_buf[]){
            {.ptr=uni, .count=(int)(ri + 1)},
            {.ptr=src, .count=count(src)},
            {.ptr=dst, .count=count(dst), .stride=4},
        })) {
            for (int i = 0; i < 8; i++) { dst[i] == expected[i] here; }
        }
    }
    cleanup(&B);
}

TEST(test_jit_xs_init) {
    // Regression: ARM64 JIT only initialized XS (spill stack pointer) when
    // ns > 0.  For tiny programs with no spills, XS was caller garbage.
    // Use enough preamble values to force eviction+fill at the back-edge.
    struct umbra_builder *b = umbra_builder();
    enum { N = 25 };
    umbra_val32 pre[N];
    for (int i = 0; i < N; i++) { pre[i] = umbra_imm_i32(b, i + 1); }
    umbra_val32 v = umbra_load_32(b, (umbra_ptr32){0});
    for (int i = 0; i < N; i++) { v = umbra_add_i32(b, v, pre[i]); }
    umbra_store_32(b, (umbra_ptr32){.ix=1}, v);
    struct test_backends B = make(b);
    int32_t sum_pre = N * (N + 1) / 2;
    int32_t src[4] = {100, 200, 300, 400}, dst[4] = {0};
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
#if defined(__aarch64__)
        __asm__ volatile("mov x15, #0xdead" ::: "x15");
#endif
        if (run(&B, bi, 4, 1, (struct umbra_buf[]){
            {.ptr=src, .count=count(src)},
            {.ptr=dst, .count=count(dst)},
        })) {
            for (int i = 0; i < 4; i++) { dst[i] == src[i] + sum_pre here; }
        }
    }
    cleanup(&B);
}

TEST(test_program_threadsafe) {
    struct umbra_builder *b = umbra_builder();
    umbra_store_32(b, (umbra_ptr32){0}, umbra_x(b));
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);

    struct umbra_backend *interp = umbra_backend_interp();
    { struct umbra_program *p = interp->compile(interp, ir);
      p->threadsafe == 1 here;
      umbra_program_free(p); }
    umbra_backend_free(interp);

    struct umbra_backend *jit = umbra_backend_jit();
    if (jit) {
        struct umbra_program *p = jit->compile(jit, ir);
        p->threadsafe == 1 here;
        umbra_program_free(p);
        umbra_backend_free(jit);
    }

    struct umbra_backend *metal = umbra_backend_metal();
    if (metal) {
        struct umbra_program *p = metal->compile(metal, ir);
        p->threadsafe == 0 here;
        umbra_program_free(p);
        umbra_backend_free(metal);
    }

    umbra_flat_ir_free(ir);
}

TEST(test_umbra_uniforms_interp) {
    // umbra_uniforms() captures only the pointer at IR-build time; the bytes
    // can be filled (and later mutated) any time before a queue() call.
    uint32_t u[4] = {0};

    struct umbra_builder *b = umbra_builder();
    umbra_ptr32 const reg = umbra_uniforms(b, u, 4);
    umbra_store_32(b, (umbra_ptr32){0}, umbra_uniform_32(b, reg, 2));
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);

    struct umbra_backend *be = umbra_backend_interp();
    struct umbra_program *p  = be->compile(be, ir);

    u[0] = 42; u[1] = 100; u[2] = 200; u[3] = 300;
    int32_t dst[1] = {0};
    p->queue(p, 0, 0, 1, 1, (struct umbra_buf[]){{.ptr=dst, .count=1}});
    dst[0] == 200 here;

    u[2] = 999;
    p->queue(p, 0, 0, 1, 1, (struct umbra_buf[]){{.ptr=dst, .count=1}});
    dst[0] == 999 here;

    umbra_program_free(p);
    umbra_flat_ir_free(ir);
    umbra_backend_free(be);
}

TEST(test_program_null_guards) {

    struct umbra_backend *be = umbra_backend_interp();
    be->flush(be);

    struct umbra_builder *b = umbra_builder();
    umbra_store_32(b, (umbra_ptr32){0}, umbra_load_32(b, (umbra_ptr32){0}));
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    struct umbra_program *p = be->compile(be, ir);
    umbra_flat_ir_free(ir);

    // dump on interpreter (no dump fn)
    if (p->dump) { p->dump(p, stdout); }

    // queue with w=0 and h=0
    int32_t buf[1] = {0};
    p->queue(p, 0, 0, 0, 1, (struct umbra_buf[]){{.ptr=buf, .count=1}});
    p->queue(p, 0, 0, 1, 0, (struct umbra_buf[]){{.ptr=buf, .count=1}});

    umbra_program_free(p);
    umbra_backend_free(be);
}

// Regression test: register variant chains must not cross the preamble/body boundary.
// The preamble runs only on the first tile; subsequent tiles start at the body.
// If a preamble output is kept in the register and the body reads from it,
// tile 2+ would read the previous tile's last value instead.
TEST(test_preamble_register_boundary) {
    struct umbra_builder *b = umbra_builder();
    // uniform is preamble (loop-invariant), add is body (uses x which varies).
    // The uniform feeds directly into the add at offset -1 if scheduled adjacently.
    umbra_val32 u = umbra_uniform_32(b, (umbra_ptr32){0}, 0);
    umbra_val32 x = umbra_add_i32(b, umbra_x(b), u);
    umbra_store_32(b, (umbra_ptr32){.ix=1}, x);
    struct test_backends B = make(b);

    // Width 32 > K=16, so there are at least 2 tiles.
    // The uniform value is 1000, so result should be col + 1000.
    int32_t uni = 1000;
    int32_t dst[32];
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (run(&B, bi, 32, 1,
                 (struct umbra_buf[]){{.ptr=&uni, .count=1}, {.ptr=dst, .count=count(dst)}})) {
            for (int col = 0; col < 32; col++) {
                dst[col] == col + 1000 here;
            }
        }
    }
    cleanup(&B);
}

TEST(test_shr_ops) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 a = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32 s = umbra_load_32(b, (umbra_ptr32){.ix=1});
    // Chain: shr_u32 → shr_s32 exercises _rm variant on second op.
    umbra_val32 c = umbra_shr_u32(b, a, s);
    umbra_val32 d = umbra_shr_s32(b, c, s);
    umbra_store_32(b, (umbra_ptr32){.ix=2}, d);
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t va[] = {0x12345678, (int32_t)0xFF000000u, 0x7FFFFFFF, 0};
        int32_t vs[] = {4, 8, 16, 0};
        int32_t dst[4] = {0};
        if (run(&B, bi, 4, 1, (struct umbra_buf[]){
            {.ptr=va, .count=count(va)}, {.ptr=vs, .count=count(vs)},
            {.ptr=dst, .count=count(dst)},
        })) {
            for (int i = 0; i < 4; i++) {
                int32_t expect = (int32_t)((uint32_t)va[i] >> vs[i]) >> vs[i];
                dst[i] == expect here;
            }
        }
    }
    cleanup(&B);
}

TEST(test_neg_round_i32) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 x = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32 f = umbra_f32_from_i32(b, x);
    // Chain: neg → round_i32 exercises register variants for both.
    umbra_val32 n = umbra_sub_f32(b, umbra_imm_f32(b, 0), f);
    umbra_val32 r = umbra_round_i32(b, n);
    umbra_store_32(b, (umbra_ptr32){.ix=1}, r);
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t src[] = {3, -7, 0, 100};
        int32_t dst[4] = {0};
        if (run(&B, bi, 4, 1, (struct umbra_buf[]){
            {.ptr=src, .count=count(src)}, {.ptr=dst, .count=count(dst)},
        })) {
            dst[0] == -3 here;
            dst[1] ==  7 here;
            dst[2] ==  0 here;
            dst[3] == -100 here;
        }
    }
    cleanup(&B);
}

TEST(test_cmp_unsigned_signed) {
    // Base ops (non-chain, result to store → m_*_mm).
    struct umbra_builder *b = umbra_builder();
    umbra_val32 a = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32 c = umbra_load_32(b, (umbra_ptr32){.ix=1});
    umbra_val32 m1 = umbra_le_s32(b, a, c);
    umbra_val32 m2 = umbra_lt_u32(b, a, c);
    umbra_val32 m3 = umbra_le_u32(b, a, c);
    umbra_store_32(b, (umbra_ptr32){.ix=2}, m1);
    umbra_store_32(b, (umbra_ptr32){.ix=3}, m2);
    umbra_store_32(b, (umbra_ptr32){.ix=4}, m3);
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sa[] = {-1, 0, 5, 10};
        int32_t sc[] = { 0, 0, 5,  5};
        int32_t d1[4]={0}, d2[4]={0}, d3[4]={0};
        if (run(&B, bi, 4, 1, (struct umbra_buf[]){
            {.ptr=sa, .count=count(sa)}, {.ptr=sc, .count=count(sc)},
            {.ptr=d1, .count=count(d1)}, {.ptr=d2, .count=count(d2)}, {.ptr=d3, .count=count(d3)},
        })) {
            d1[0] == -1 here; d1[1] == -1 here; d1[2] == -1 here; d1[3] == 0 here;
            d2[0] == 0 here; d2[1] == 0 here; d2[2] == 0 here; d2[3] == 0 here;
            d3[0] == 0 here; d3[1] == -1 here; d3[2] == -1 here; d3[3] == 0 here;
        }
    }
    cleanup(&B);

    // Chained: cmp → sel exercises r_*_mm (result to acc) and
    // the base ops with register variant upgrade.
    b = umbra_builder();
    a = umbra_load_32(b, (umbra_ptr32){0});
    c = umbra_load_32(b, (umbra_ptr32){.ix=1});
    umbra_val32 one = umbra_imm_i32(b, 1), zero = umbra_imm_i32(b, 0);
    // le_s32 → sel: le result → acc, sel reads it
    umbra_val32 r1 = umbra_sel_32(b, umbra_le_s32(b, a, c), one, zero);
    umbra_val32 r2 = umbra_sel_32(b, umbra_lt_u32(b, a, c), one, zero);
    umbra_val32 r3 = umbra_sel_32(b, umbra_le_u32(b, a, c), one, zero);
    umbra_store_32(b, (umbra_ptr32){.ix=2}, r1);
    umbra_store_32(b, (umbra_ptr32){.ix=3}, r2);
    umbra_store_32(b, (umbra_ptr32){.ix=4}, r3);
    B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sa2[] = {-1, 0, 5, 10};
        int32_t sc2[] = { 0, 0, 5,  5};
        int32_t d12[4]={0}, d22[4]={0}, d32[4]={0};
        if (run(&B, bi, 4, 1, (struct umbra_buf[]){
            {.ptr=sa2, .count=count(sa2)}, {.ptr=sc2, .count=count(sc2)},
            {.ptr=d12, .count=count(d12)}, {.ptr=d22, .count=count(d22)},
            {.ptr=d32, .count=count(d32)},
        })) {
            d12[0] == 1 here; d12[1] == 1 here; d12[2] == 1 here; d12[3] == 0 here;
            d22[0] == 0 here; d22[1] == 0 here; d22[2] == 0 here; d22[3] == 0 here;
            d32[0] == 0 here; d32[1] == 1 here; d32[2] == 1 here; d32[3] == 0 here;
        }
    }
    cleanup(&B);
}

TEST(test_max_f32_imm) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 x = umbra_f32_from_i32(b, umbra_load_32(b, (umbra_ptr32){0}));
    umbra_val32 c = umbra_max_f32(b, x, umbra_imm_f32(b, 5.f));
    umbra_store_32(b, (umbra_ptr32){.ix=1}, umbra_i32_from_f32(b, c));
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t src[] = {-5, 0, 3, 100};
        int32_t dst[4] = {0};
        if (run(&B, bi, 4, 1, (struct umbra_buf[]){
            {.ptr=src, .count=count(src)}, {.ptr=dst, .count=count(dst)},
        })) {
            dst[0] == 5 here; dst[1] == 5 here; dst[2] == 5 here; dst[3] == 100 here;
        }
    }
    cleanup(&B);
}

TEST(test_imm_cmp_i32) {
    // Base imm ops (result to store).
    struct umbra_builder *b = umbra_builder();
    umbra_val32 a = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32 m1 = umbra_eq_i32(b, a, umbra_imm_i32(b, 5));
    umbra_val32 m2 = umbra_lt_s32(b, a, umbra_imm_i32(b, 5));
    umbra_val32 m3 = umbra_le_s32(b, a, umbra_imm_i32(b, 5));
    umbra_store_32(b, (umbra_ptr32){.ix=1}, m1);
    umbra_store_32(b, (umbra_ptr32){.ix=2}, m2);
    umbra_store_32(b, (umbra_ptr32){.ix=3}, m3);
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t src[] = {3, 5, 7, -1};
        int32_t d1[4]={0}, d2[4]={0}, d3[4]={0};
        if (run(&B, bi, 4, 1, (struct umbra_buf[]){
            {.ptr=src, .count=count(src)},
            {.ptr=d1, .count=count(d1)}, {.ptr=d2, .count=count(d2)}, {.ptr=d3, .count=count(d3)},
        })) {
            d1[0]==0 here; d1[1]==-1 here; d1[2]==0 here; d1[3]==0 here;
            d2[0]==-1 here; d2[1]==0 here; d2[2]==0 here; d2[3]==-1 here;
            d3[0]==-1 here; d3[1]==-1 here; d3[2]==0 here; d3[3]==-1 here;
        }
    }
    cleanup(&B);
}

TEST(test_uniform_register) {
    // uniform → acc: uniform consumed only by next ALU op.
    struct umbra_builder *b = umbra_builder();
    umbra_val32 u = umbra_uniform_32(b, (umbra_ptr32){0}, 0);
    umbra_val32 x = umbra_load_32(b, (umbra_ptr32){.ix=1});
    umbra_val32 r = umbra_add_i32(b, u, x);
    umbra_store_32(b, (umbra_ptr32){.ix=2}, r);
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t uni = 1000;
        int32_t src[] = {1, 2, 3, 4};
        int32_t dst[4] = {0};
        if (run(&B, bi, 4, 1, (struct umbra_buf[]){
            {.ptr=&uni, .count=1},
            {.ptr=src, .count=count(src)},
            {.ptr=dst, .count=count(dst)},
        })) {
            for (int i = 0; i < 4; i++) { dst[i] == 1000 + src[i] here; }
        }
    }
    cleanup(&B);
}

TEST(test_minmax_register_variants) {
    // Exercise _mm, _mr, _rr variants for min/max.
    struct umbra_builder *b = umbra_builder();
    umbra_val32 a = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32 bf_ = umbra_load_32(b, (umbra_ptr32){.ix=1});
    umbra_val32 fa = umbra_f32_from_i32(b, a);
    umbra_val32 fb = umbra_f32_from_i32(b, bf_);
    // _mr: x from mem, y from acc (fb just produced)
    umbra_val32 mn = umbra_min_f32(b, fa, fb);
    // _rr: same value as both args (mn just produced → acc, used twice)
    umbra_val32 mx = umbra_max_f32(b, mn, mn);
    umbra_store_32(b, (umbra_ptr32){.ix=2}, umbra_i32_from_f32(b, mx));
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sa[] = {1, 10, 5, -3};
        int32_t sb[] = {5, 2, 5, 7};
        int32_t dst[4] = {0};
        if (run(&B, bi, 4, 1, (struct umbra_buf[]){
            {.ptr=sa, .count=count(sa)}, {.ptr=sb, .count=count(sb)},
            {.ptr=dst, .count=count(dst)},
        })) {
            // min then max(x,x) = min
            dst[0] == 1 here; dst[1] == 2 here; dst[2] == 5 here; dst[3] == -3 here;
        }
    }
    cleanup(&B);
}

TEST(test_cmp_register_variants) {
    // Exercise _rm variant for comparisons: chain a→b→cmp(b,c).
    struct umbra_builder *b = umbra_builder();
    umbra_val32 x = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32 y = umbra_load_32(b, (umbra_ptr32){.ix=1});
    // add puts result in acc, then lt_f32 gets x from acc → _rm
    umbra_val32 fx = umbra_f32_from_i32(b, x);
    umbra_val32 fy = umbra_f32_from_i32(b, y);
    umbra_val32 s = umbra_add_f32(b, fx, umbra_imm_f32(b, 0.5f));
    umbra_val32 m = umbra_lt_f32(b, s, fy);
    umbra_store_32(b, (umbra_ptr32){.ix=2}, m);
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sx[] = {1, 10, 5, 0};
        int32_t sy[] = {5, 5, 5, 5};
        int32_t dst[4] = {0};
        if (run(&B, bi, 4, 1, (struct umbra_buf[]){
            {.ptr=sx, .count=count(sx)}, {.ptr=sy, .count=count(sy)},
            {.ptr=dst, .count=count(dst)},
        })) {
            // 1.5<5 → -1, 10.5<5 → 0, 5.5<5 → 0, 0.5<5 → -1
            dst[0] == -1 here; dst[1] == 0 here; dst[2] == 0 here; dst[3] == -1 here;
        }
    }
    cleanup(&B);
}

TEST(test_regvar_m_patterns) {
    // Targets: m_cmp_rm, m_cmp_rr, r_minmax_mm, base max_f32_imm,
    // m_float_cmp_imm_r, r/m_int_cmp_imm_r.
    struct umbra_builder *b = umbra_builder();
    umbra_val32 a = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32 c = umbra_load_32(b, (umbra_ptr32){.ix=1});
    umbra_val32 fa = umbra_f32_from_i32(b, a);
    umbra_val32 fc = umbra_f32_from_i32(b, c);

    // m_cmp_rm: add→acc, then lt_f32(acc, mem) → store (out_r=false → m_)
    umbra_val32 sum = umbra_add_f32(b, fa, umbra_imm_f32(b, 0.5f));
    umbra_store_32(b, (umbra_ptr32){.ix=2}, umbra_lt_f32(b, sum, fc));

    // m_cmp_rr: lt_f32(x, x) where x is from acc, result to store
    umbra_val32 neg = umbra_sub_f32(b, umbra_imm_f32(b, 0), fa);
    umbra_store_32(b, (umbra_ptr32){.ix=3}, umbra_lt_f32(b, neg, neg));

    // r_min_mm: both from memory, result feeds next ALU
    umbra_val32 dummy = umbra_add_f32(b, fa, fc);
    umbra_store_32(b, (umbra_ptr32){.ix=4}, umbra_i32_from_f32(b, dummy));
    umbra_val32 mn = umbra_min_f32(b, fa, fc);
    umbra_store_32(b, (umbra_ptr32){.ix=5}, umbra_i32_from_f32(b, mn));

    // base max_f32_imm: result direct to store (no chain → base op)
    umbra_val32 mx = umbra_max_f32(b, fc, umbra_imm_f32(b, 5.f));
    umbra_store_32(b, (umbra_ptr32){.ix=6}, mx);

    // m_float_cmp_imm_r: chain→lt_f32_imm→store (x from acc, result to memory)
    umbra_val32 fn_ = umbra_sub_f32(b, umbra_imm_f32(b, 0), fc);
    umbra_store_32(b, (umbra_ptr32){.ix=7}, umbra_lt_f32(b, fn_, umbra_imm_f32(b, 0.f)));

    // m_int_cmp_imm_r: chain→cmp_imm→store (x from acc, result to memory)
    umbra_val32 ai = umbra_add_i32(b, a, umbra_imm_i32(b, 1));
    umbra_store_32(b, (umbra_ptr32){.ix=8}, umbra_eq_i32(b, ai, umbra_imm_i32(b, 6)));
    umbra_val32 ai2 = umbra_add_i32(b, a, umbra_imm_i32(b, 2));
    umbra_store_32(b, (umbra_ptr32){.ix=9}, umbra_lt_s32(b, ai2, umbra_imm_i32(b, 6)));
    umbra_val32 ai3 = umbra_add_i32(b, a, umbra_imm_i32(b, 3));
    umbra_store_32(b, (umbra_ptr32){.ix=10}, umbra_le_s32(b, ai3, umbra_imm_i32(b, 6)));
    // r_int_cmp_imm_r: chain→cmp_imm→and (result to acc, feeds ALU)
    umbra_val32 ai4 = umbra_add_i32(b, a, umbra_imm_i32(b, 100));
    umbra_val32 eq_mask = umbra_eq_i32(b, ai4, umbra_imm_i32(b, 105));
    umbra_store_32(b, (umbra_ptr32){.ix=11}, umbra_and_32(b, eq_mask, a));

    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sa[] = {5, 0, -1, 10};
        int32_t sc[] = {3, 3, 3, 3};
        int32_t d[12][4];
        __builtin_memset(d, 0, sizeof d);
        if (run(&B, bi, 4, 1, (struct umbra_buf[]){
            {.ptr=sa, .count=count(sa)}, {.ptr=sc, .count=count(sc)},
            {.ptr=d[2], .count=4}, {.ptr=d[3], .count=4},
            {.ptr=d[4], .count=4}, {.ptr=d[5], .count=4},
            {.ptr=d[6], .count=4}, {.ptr=d[7], .count=4},
            {.ptr=d[8], .count=4}, {.ptr=d[9], .count=4},
            {.ptr=d[10], .count=4}, {.ptr=d[11], .count=4},
        })) {
            // lt(5.5,3)=0, lt(0.5,3)=-1, lt(-0.5,3)=-1, lt(10.5,3)=0
            d[2][0] == 0 here; d[2][1] == -1 here; d[2][2] == -1 here; d[2][3] == 0 here;
            // lt(x,x) always 0
            for (int i = 0; i < 4; i++) { d[3][i] == 0 here; }
            // min(5.0,3.0)=3 min(0.0,3.0)=0 min(-1.0,3.0)=-1 min(10.0,3.0)=3
            d[5][0] == 3 here; d[5][1] == 0 here; d[5][2] == -1 here; d[5][3] == 3 here;
            // max(3.0,5.0)=5.0 for all (fc=3, imm=5), stored as float bits
            union { float f; int32_t i; } five = {.f=5.f};
            for (int i = 0; i < 4; i++) { d[6][i] == five.i here; }
            // eq(5+1,6)=-1, eq(0+1,6)=0, eq(-1+1,6)=0, eq(10+1,6)=0
            d[8][0] == -1 here; d[8][1] == 0 here; d[8][2] == 0 here; d[8][3] == 0 here;
        }
    }
    cleanup(&B);
}

// Exercise m_*_rr for every binary op: chain→op(acc,acc)→store.
TEST(test_binary_m_rr) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 a = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32 fa = umbra_f32_from_i32(b, a);
    int p = 1;

    // Each needs a unique chain source so dedup doesn't merge them.
#define RR_F32(op, k) { \
        umbra_val32 src = umbra_add_f32(b, fa, umbra_imm_f32(b, (float)(k))); \
        umbra_store_32(b, (umbra_ptr32){.ix=(p++)}, op(b, src, src)); \
    }
#define RR_I32(op, k) { \
        umbra_val32 src = umbra_add_i32(b, a, umbra_imm_i32(b, (k))); \
        umbra_store_32(b, (umbra_ptr32){.ix=(p++)}, op(b, src, src)); \
    }
    RR_F32(umbra_sub_f32, 1)
    RR_F32(umbra_mul_f32, 2)
    RR_F32(umbra_div_f32, 3)
    RR_F32(umbra_min_f32, 4)
    RR_F32(umbra_max_f32, 5)
    RR_I32(umbra_add_i32, 100)
    RR_I32(umbra_sub_i32, 200)
    RR_I32(umbra_mul_i32, 300)
    RR_I32(umbra_shl_i32, 400)
    RR_I32(umbra_shr_u32, 500)
    RR_I32(umbra_shr_s32, 600)
    RR_I32(umbra_and_32, 700)
    RR_I32(umbra_or_32,  800)
    RR_I32(umbra_xor_32, 900)
#undef RR_F32
#undef RR_I32
    int const nbufs = p;
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t src[] = {2, 2, 2, 2};
        int32_t d[15][4];
        __builtin_memset(d, 0, sizeof d);
        struct umbra_buf bufs[15];
        bufs[0] = (struct umbra_buf){.ptr=src, .count=count(src)};
        for (int i = 1; i < nbufs; i++) {
            bufs[i] = (struct umbra_buf){.ptr=d[i], .count=4};
        }
        if (run(&B, bi, 4, 1, bufs)) {
            // sub(3,3)=0, mul(4,4)=16, div(5,5)=1
            union { float f; int32_t i; } u;
            u.f = 0.f;  d[1][0] == u.i here;
            u.f = 16.f; d[2][0] == u.i here;
            u.f = 1.f;  d[3][0] == u.i here;
            // add(102,102)=204, sub(202,202)=0, mul(302,302)=91204
            d[6][0] == 204 here;
            d[7][0] == 0 here;
            d[8][0] == 302*302 here;
            // xor(x,x) = 0 for any x
            d[14][0] == 0 here;
        }
    }
    cleanup(&B);
}

// Exercise m_min/max_rm: chain→min/max(acc,mem)→store.
TEST(test_minmax_m_rm) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 a = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32 c = umbra_load_32(b, (umbra_ptr32){.ix=1});
    umbra_val32 fa = umbra_f32_from_i32(b, a);
    umbra_val32 fc = umbra_f32_from_i32(b, c);
    // neg→acc, then min(acc, fc)→store = m_min_rm
    umbra_val32 na = umbra_sub_f32(b, umbra_imm_f32(b, 0), fa);
    umbra_store_32(b, (umbra_ptr32){.ix=2}, umbra_min_f32(b, na, fc));
    // neg→acc, then max(acc, fc)→store = m_max_rm
    umbra_val32 nc = umbra_sub_f32(b, umbra_imm_f32(b, 0), fc);
    umbra_store_32(b, (umbra_ptr32){.ix=3}, umbra_max_f32(b, nc, fa));
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sa[] = {5,5,5,5}, sc[] = {3,3,3,3};
        int32_t d2[4]={0}, d3[4]={0};
        if (run(&B, bi, 4, 1, (struct umbra_buf[]){
            {.ptr=sa, .count=4}, {.ptr=sc, .count=4},
            {.ptr=d2, .count=4}, {.ptr=d3, .count=4},
        })) {
            // min(-5, 3) = -5
            union { float f; int32_t i; } u;
            u.f = -5.f; d2[0] == u.i here;
            // max(-3, 5) = 5
            u.f = 5.f;  d3[0] == u.i here;
        }
    }
    cleanup(&B);
}

// Exercise r_cmp_rm and r_cmp_rr: chain→cmp→ALU→store.
TEST(test_cmp_r_rm_rr) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 a = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32 c = umbra_load_32(b, (umbra_ptr32){.ix=1});
    umbra_val32 fa = umbra_f32_from_i32(b, a);
    umbra_val32 fc = umbra_f32_from_i32(b, c);
    umbra_val32 one = umbra_imm_i32(b, 1), zero = umbra_imm_i32(b, 0);
    int p = 2;
    // r_cmp_rm: chain→cmp(acc, mem)→sel→store
#define CMP_RM(cmp, chain_op, chain_k) { \
        umbra_val32 src = chain_op(b, fa, umbra_imm_f32(b, (float)(chain_k))); \
        umbra_store_32(b, (umbra_ptr32){.ix=(p++)}, umbra_sel_32(b, cmp(b, src, fc), one, zero)); \
    }
    CMP_RM(umbra_lt_f32, umbra_add_f32, 0.5f)
    CMP_RM(umbra_le_f32, umbra_add_f32, 1.5f)
    CMP_RM(umbra_eq_f32, umbra_add_f32, 2.5f)
#undef CMP_RM
    // r_cmp_rr: chain→cmp(acc, acc)→sel→store
#define CMP_RR(cmp, chain_op, chain_k) { \
        umbra_val32 src = chain_op(b, fa, umbra_imm_f32(b, (float)(chain_k))); \
        umbra_store_32(b, (umbra_ptr32){.ix=(p++)}, umbra_sel_32(b, cmp(b, src, src), one, zero)); \
    }
    CMP_RR(umbra_lt_f32, umbra_add_f32, 3.5f)
    CMP_RR(umbra_eq_f32, umbra_add_f32, 4.5f)
    CMP_RR(umbra_eq_i32, umbra_add_i32, 100)
    CMP_RR(umbra_lt_s32, umbra_add_i32, 200)
    CMP_RR(umbra_le_s32, umbra_add_i32, 300)
    CMP_RR(umbra_lt_u32, umbra_add_i32, 400)
    CMP_RR(umbra_le_u32, umbra_add_i32, 500)
#undef CMP_RR
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sa[] = {2,2,2,2}, sc[] = {3,3,3,3};
        int32_t d[12][4];
        __builtin_memset(d, 0, sizeof d);
        struct umbra_buf bufs[12];
        bufs[0] = (struct umbra_buf){.ptr=sa, .count=4};
        bufs[1] = (struct umbra_buf){.ptr=sc, .count=4};
        for (int i = 2; i < 12; i++) { bufs[i] = (struct umbra_buf){.ptr=d[i], .count=4}; }
        if (run(&B, bi, 4, 1, bufs)) {
            // lt(2.5, 3)=1, le(3.5, 3)=0, eq(4.5, 3)=0
            d[2][0] == 1 here; d[3][0] == 0 here; d[4][0] == 0 here;
            // lt(x,x)=0, eq(x,x)=1, le(x,x)=1
            d[5][0] == 0 here; d[6][0] == 1 here;
            d[7][0] == 1 here; d[8][0] == 0 here;
            d[9][0] == 1 here; d[10][0] == 0 here; d[11][0] == 1 here;
        }
    }
    cleanup(&B);
}

// Exercise missing unary per-op variants.
// Need r_m (mem→acc) for ops that only have r_r and m_r covered.
TEST(test_unary_r_m) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 a = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32 fa = umbra_f32_from_i32(b, a);
    // r_sqrt_m: sqrt(mem) → acc → store via chain
    umbra_store_32(b, (umbra_ptr32){.ix=1},
                   umbra_i32_from_f32(b, umbra_sqrt_f32(b, fa)));
    // r_round_f32_m, r_floor_f32_m, r_ceil_f32_m:
    umbra_store_32(b, (umbra_ptr32){.ix=2},
                   umbra_i32_from_f32(b, umbra_round_f32(b, fa)));
    umbra_store_32(b, (umbra_ptr32){.ix=3},
                   umbra_i32_from_f32(b, umbra_floor_f32(b, fa)));
    umbra_store_32(b, (umbra_ptr32){.ix=4},
                   umbra_i32_from_f32(b, umbra_ceil_f32(b, fa)));
    // r_neg_m, r_round_i32_m, r_ceil_i32_m:
    umbra_store_32(b, (umbra_ptr32){.ix=5},
                   umbra_i32_from_f32(b, umbra_sub_f32(b, umbra_imm_f32(b, 0), fa)));
    umbra_store_32(b, (umbra_ptr32){.ix=6},
                   umbra_add_i32(b, umbra_round_i32(b, fa), umbra_imm_i32(b, 0)));
    umbra_store_32(b, (umbra_ptr32){.ix=7},
                   umbra_add_i32(b, umbra_ceil_i32(b, fa), umbra_imm_i32(b, 0)));
    // r_f32_from_i32_m: f32_from_i32(mem) → acc → chain
    umbra_store_32(b, (umbra_ptr32){.ix=8},
                   umbra_i32_from_f32(b, umbra_f32_from_i32(b, a)));
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t src[] = {4,4,4,4};
        int32_t d[9][4];
        __builtin_memset(d, 0, sizeof d);
        struct umbra_buf bufs[9];
        bufs[0] = (struct umbra_buf){.ptr=src, .count=4};
        for (int i = 1; i < 9; i++) { bufs[i] = (struct umbra_buf){.ptr=d[i], .count=4}; }
        if (run(&B, bi, 4, 1, bufs)) {
            d[1][0] == 2 here;  // sqrt(4)=2
            d[2][0] == 4 here;  // round(4.0)=4
            d[5][0] == -4 here; // i32_from_f32(neg(4.0))=-4
            d[8][0] == 4 here;  // i32_from_f32(f32_from_i32(4))=4
        }
    }
    cleanup(&B);
}

// Exercise base ops that are always upgraded: xor_32_imm, min_f32_imm.
// Need: op(mem, imm)→store with no chain on either side.
TEST(test_base_imm_ops) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 a = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32 c = umbra_load_32(b, (umbra_ptr32){.ix=1});
    // xor_32_imm base: neither chained
    umbra_store_32(b, (umbra_ptr32){.ix=2},
                   umbra_xor_32(b, a, umbra_imm_i32(b, 0xFF)));
    // min_f32_imm base:
    umbra_val32 fc = umbra_f32_from_i32(b, c);
    umbra_store_32(b, (umbra_ptr32){.ix=3},
                   umbra_min_f32(b, fc, umbra_imm_f32(b, 2.f)));
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sa[] = {0x12, 0x12, 0x12, 0x12};
        int32_t sc[] = {5, 1, 5, 1};
        int32_t d2[4]={0}, d3[4]={0};
        if (run(&B, bi, 4, 1, (struct umbra_buf[]){
            {.ptr=sa, .count=4}, {.ptr=sc, .count=4},
            {.ptr=d2, .count=4}, {.ptr=d3, .count=4},
        })) {
            d2[0] == (0x12 ^ 0xFF) here;
            union { float f; int32_t i; } u;
            u.f = 2.f; d3[0] == u.i here;  // min(5, 2) = 2
            u.f = 1.f; d3[1] == u.i here;  // min(1, 2) = 1
        }
    }
    cleanup(&B);
}

// Exercise r_sel_32_rm, r_fms_f32_mmr.
TEST(test_sel_fms_variants) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 a = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32 c = umbra_load_32(b, (umbra_ptr32){.ix=1});
    umbra_val32 fa = umbra_f32_from_i32(b, a);
    umbra_val32 fc = umbra_f32_from_i32(b, c);

    // r_sel_32_rm: mask from acc → sel → next ALU → store
    umbra_val32 mask = umbra_lt_f32(b, fa, fc);
    umbra_val32 sel_r = umbra_sel_32(b, mask, a, c);
    umbra_store_32(b, (umbra_ptr32){.ix=2}, umbra_add_i32(b, sel_r, umbra_imm_i32(b, 0)));

    // r_fms_f32_mmr: fms(x, y, acc) → acc → store
    // fms = z - x*y, with z from acc
    umbra_val32 fms_z = umbra_add_f32(b, fc, umbra_imm_f32(b, 10.f));
    umbra_val32 fms_r = umbra_sub_f32(b, fms_z, umbra_mul_f32(b, fa, fc));
    umbra_store_32(b, (umbra_ptr32){.ix=3}, umbra_i32_from_f32(b, fms_r));

    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sa[] = {2,2,2,2}, sc[] = {5,5,5,5};
        int32_t d[4][4];
        __builtin_memset(d, 0, sizeof d);
        struct umbra_buf bufs[4];
        bufs[0] = (struct umbra_buf){.ptr=sa, .count=4};
        bufs[1] = (struct umbra_buf){.ptr=sc, .count=4};
        for (int i = 2; i < 4; i++) { bufs[i] = (struct umbra_buf){.ptr=d[i], .count=4}; }
        if (run(&B, bi, 4, 1, bufs)) {
            // sel: 2<5 → true → pick a=2
            d[2][0] == 2 here;
        }
    }
    cleanup(&B);
}

// Exercise missing IMM register variants.
TEST(test_imm_regvar) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 a = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32 fa = umbra_f32_from_i32(b, a);
    int p = 1;

    // r_*_imm_r for each: chain→imm_op(acc)→chain→store
    // Need the imm op's result to feed the next ALU (out_r) AND input from acc (x_r).
#define IMM_CHAIN_F(op, k) { \
        umbra_val32 src = umbra_add_f32(b, fa, umbra_imm_f32(b, (float)(k))); \
        umbra_val32 r = op(b, src, umbra_imm_f32(b, 2.f)); \
        umbra_store_32(b, (umbra_ptr32){.ix=(p++)}, umbra_i32_from_f32(b, r)); \
    }
#define IMM_CHAIN_I(op, k) { \
        umbra_val32 src = umbra_add_i32(b, a, umbra_imm_i32(b, (k))); \
        umbra_val32 r = op(b, src, umbra_imm_i32(b, 3)); \
        umbra_store_32(b, (umbra_ptr32){.ix=(p++)}, umbra_add_i32(b, r, umbra_imm_i32(b, 1))); \
    }
    IMM_CHAIN_F(umbra_mul_f32, 1)
    IMM_CHAIN_F(umbra_div_f32, 2)
    IMM_CHAIN_F(umbra_min_f32, 3)
    IMM_CHAIN_I(umbra_shr_u32, 1000)
    IMM_CHAIN_I(umbra_and_32, 2000)
    IMM_CHAIN_I(umbra_xor_32, 3000)
    IMM_CHAIN_I(umbra_mul_i32, 4000)
    IMM_CHAIN_I(umbra_sub_i32, 5000)
    // Float CMP imm: chain→cmp_imm(acc)→chain
    {
        umbra_val32 src = umbra_add_f32(b, fa, umbra_imm_f32(b, 10.f));
        umbra_val32 m = umbra_eq_f32(b, src, umbra_imm_f32(b, 12.f));
        umbra_store_32(b, (umbra_ptr32){.ix=(p++)}, umbra_and_32(b, m, umbra_imm_i32(b, 1)));
    }
    {
        umbra_val32 src = umbra_add_f32(b, fa, umbra_imm_f32(b, 20.f));
        umbra_val32 m = umbra_lt_f32(b, src, umbra_imm_f32(b, 25.f));
        umbra_store_32(b, (umbra_ptr32){.ix=(p++)}, umbra_and_32(b, m, umbra_imm_i32(b, 1)));
    }
    {
        umbra_val32 src = umbra_add_f32(b, fa, umbra_imm_f32(b, 30.f));
        umbra_val32 m = umbra_le_f32(b, src, umbra_imm_f32(b, 32.f));
        umbra_store_32(b, (umbra_ptr32){.ix=(p++)}, umbra_and_32(b, m, umbra_imm_i32(b, 1)));
    }
    // Int CMP imm r_m variant: chain→cmp_imm(mem)→chain
    {
        umbra_val32 m = umbra_eq_i32(b, a, umbra_imm_i32(b, 2));
        umbra_store_32(b, (umbra_ptr32){.ix=(p++)}, umbra_and_32(b, m, umbra_imm_i32(b, 1)));
    }
#undef IMM_CHAIN_F
#undef IMM_CHAIN_I
    int const nbufs = p;
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t src[] = {2, 2, 2, 2};
        int32_t d[13][4];
        __builtin_memset(d, 0, sizeof d);
        struct umbra_buf bufs[13];
        bufs[0] = (struct umbra_buf){.ptr=src, .count=4};
        for (int i = 1; i < nbufs; i++) { bufs[i] = (struct umbra_buf){.ptr=d[i], .count=4}; }
        if (run(&B, bi, 4, 1, bufs)) {
            // mul(3*2)=6, eq(12,12)=1, lt(22,25)=1, le(32,32)=1, eq_i32(2,2)=1
            d[1][0] == 6 here;
            d[9][0] == 1 here;
            d[10][0] == 1 here;
            d[11][0] == 1 here;
            d[12][0] == 1 here;
        }
    }
    cleanup(&B);
}

// Exercise r_*_mm for binary ops: both from memory, result→acc→chain.
// The IR optimizer schedules loads near consumers. To get _mm we need both
// operands pinned early (via a use/store) then a barrier store between them
// and the target op so prev_r is false.
TEST(test_binary_r_mm) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 a = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32 c = umbra_load_32(b, (umbra_ptr32){.ix=1});
    umbra_val32 fa = umbra_f32_from_i32(b, a);
    umbra_val32 fc = umbra_f32_from_i32(b, c);
    // Pin fa and fc: use both in an add, store result, then barrier store.
    umbra_store_32(b, (umbra_ptr32){.ix=2}, umbra_i32_from_f32(b, umbra_add_f32(b, fa, fc)));
    // After the store, prev_r=false. op(fa,fc) gets both from memory → r_*_mm.
    // Use i32_from_f32 as the chain consumer (out_r=true for op).
    umbra_store_32(b, (umbra_ptr32){.ix=3}, umbra_i32_from_f32(b, umbra_sub_f32(b, fa, fc)));
    umbra_store_32(b, (umbra_ptr32){.ix=4}, umbra_i32_from_f32(b, umbra_mul_f32(b, fa, fc)));
    umbra_store_32(b, (umbra_ptr32){.ix=5}, umbra_i32_from_f32(b, umbra_div_f32(b, fa, fc)));
    // Integer: pin a and c, then barrier, then ops.
    umbra_store_32(b, (umbra_ptr32){.ix=6}, umbra_add_i32(b, a, c));
    umbra_store_32(b, (umbra_ptr32){.ix=7},
                   umbra_add_i32(b, umbra_sub_i32(b, a, c), umbra_imm_i32(b, 1)));
    umbra_store_32(b, (umbra_ptr32){.ix=8},
                   umbra_add_i32(b, umbra_mul_i32(b, a, c), umbra_imm_i32(b, 1)));
    umbra_store_32(b, (umbra_ptr32){.ix=9},
                   umbra_add_i32(b, umbra_or_32(b, a, c), umbra_imm_i32(b, 1)));
    umbra_store_32(b, (umbra_ptr32){.ix=10},
                   umbra_add_i32(b, umbra_xor_32(b, a, c), umbra_imm_i32(b, 1)));
    umbra_store_32(b, (umbra_ptr32){.ix=11},
                   umbra_add_i32(b, umbra_and_32(b, a, c), umbra_imm_i32(b, 1)));
    umbra_store_32(b, (umbra_ptr32){.ix=12},
                   umbra_add_i32(b, umbra_shl_i32(b, a, c), umbra_imm_i32(b, 1)));
    umbra_store_32(b, (umbra_ptr32){.ix=13},
                   umbra_add_i32(b, umbra_shr_u32(b, a, c), umbra_imm_i32(b, 1)));
    umbra_store_32(b, (umbra_ptr32){.ix=14},
                   umbra_add_i32(b, umbra_shr_s32(b, a, c), umbra_imm_i32(b, 1)));
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sa[] = {6,6,6,6}, sc[] = {2,2,2,2};
        int32_t d[15][4];
        __builtin_memset(d, 0, sizeof d);
        struct umbra_buf bufs[15];
        bufs[0] = (struct umbra_buf){.ptr=sa, .count=4};
        bufs[1] = (struct umbra_buf){.ptr=sc, .count=4};
        for (int i = 2; i < 15; i++) { bufs[i] = (struct umbra_buf){.ptr=d[i], .count=4}; }
        if (run(&B, bi, 4, 1, bufs)) {
            d[3][0]  == 4       here;  // sub(6,2)=4
            d[4][0]  == 12      here;  // mul(6,2)=12
            d[5][0]  == 3       here;  // div(6,2)=3
            d[7][0]  == 5       here;  // sub(6,2)+1=5
            d[10][0] == (6^2)+1 here;
        }
    }
    cleanup(&B);
}

// Exercise missing per-op UNARY and IMM variants more thoroughly.
// m_unary_r: chain→unary(acc)→store (no further chain).
TEST(test_unary_m_r) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 a = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32 fa = umbra_f32_from_i32(b, a);
    int p = 1;
    // Each: chain→unary→store
#define MR(unary, chain_val) { \
        umbra_store_32(b, (umbra_ptr32){.ix=(p++)}, unary(b, chain_val)); \
    }
    umbra_val32 fa2 = umbra_add_f32(b, fa, umbra_imm_f32(b, 1.f));
    MR(umbra_sqrt_f32, fa2)
    umbra_val32 fa3 = umbra_add_f32(b, fa, umbra_imm_f32(b, 2.f));
    MR(umbra_round_f32, fa3)
    umbra_val32 fa4 = umbra_add_f32(b, fa, umbra_imm_f32(b, 3.f));
    MR(umbra_floor_f32, fa4)
    umbra_val32 fa5 = umbra_add_f32(b, fa, umbra_imm_f32(b, 4.f));
    MR(umbra_ceil_f32, fa5)
    umbra_val32 fa7 = umbra_add_f32(b, fa, umbra_imm_f32(b, 6.f));
    MR(umbra_round_i32, fa7)
    umbra_val32 fa8 = umbra_add_f32(b, fa, umbra_imm_f32(b, 7.f));
    MR(umbra_ceil_i32, fa8)
    umbra_val32 ia = umbra_add_i32(b, a, umbra_imm_i32(b, 100));
    MR(umbra_f32_from_i32, ia)
#undef MR
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t src[] = {4,4,4,4};
        int32_t d[9][4];
        __builtin_memset(d, 0, sizeof d);
        struct umbra_buf bufs[9];
        bufs[0] = (struct umbra_buf){.ptr=src, .count=4};
        for (int i = 1; i < p; i++) { bufs[i] = (struct umbra_buf){.ptr=d[i], .count=4}; }
        if (run(&B, bi, 4, 1, bufs)) {
            d[5][0] == 10 here;                  // round(4+6) = 10
        }
    }
    cleanup(&B);
}

// Exercise base IMM ops: op(mem, imm)→store with no chain.
// Covers shr_s32_imm, shr_u32_imm, or_32_imm, mul_f32_imm, etc.
TEST(test_base_imm_more) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 a = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32 c = umbra_load_32(b, (umbra_ptr32){.ix=1});
    umbra_val32 fc = umbra_f32_from_i32(b, c);
    int p = 2;
    // Each: op(mem, imm)→store, no chain on either side.
    // Use multiple loads so dedup doesn't merge chains.
    umbra_store_32(b, (umbra_ptr32){.ix=(p++)}, umbra_shr_s32(b, a, umbra_imm_i32(b, 1)));
    umbra_store_32(b, (umbra_ptr32){.ix=(p++)}, umbra_shr_u32(b, a, umbra_imm_i32(b, 2)));
    umbra_store_32(b, (umbra_ptr32){.ix=(p++)}, umbra_or_32(b, a, umbra_imm_i32(b, 0xF0)));
    umbra_store_32(b, (umbra_ptr32){.ix=(p++)}, umbra_mul_f32(b, fc, umbra_imm_f32(b, 3.f)));
    umbra_store_32(b, (umbra_ptr32){.ix=(p++)}, umbra_mul_i32(b, a, umbra_imm_i32(b, 7)));
    umbra_store_32(b, (umbra_ptr32){.ix=(p++)}, umbra_sub_i32(b, a, umbra_imm_i32(b, 1)));
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sa[] = {16,16,16,16}, sc[] = {5,5,5,5};
        int32_t d[8][4];
        __builtin_memset(d, 0, sizeof d);
        struct umbra_buf bufs[8];
        bufs[0] = (struct umbra_buf){.ptr=sa, .count=4};
        bufs[1] = (struct umbra_buf){.ptr=sc, .count=4};
        for (int i = 2; i < p; i++) { bufs[i] = (struct umbra_buf){.ptr=d[i], .count=4}; }
        if (run(&B, bi, 4, 1, bufs)) {
            d[2][0] == 8 here;             // 16 >> 1
            d[3][0] == 4 here;             // 16 >>> 2
            d[4][0] == (16 | 0xF0) here;   // 0x10 | 0xF0 = 0xF0
            d[7][0] == 15 here;            // 16 - 1
        }
    }
    cleanup(&B);
}

// r_sel_32_rm: sel with mask from acc, result→acc.
// Need: chain→sel(acc_mask, y, z)→ALU→store
TEST(test_sel_r_rm) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 a = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32 c = umbra_load_32(b, (umbra_ptr32){.ix=1});
    umbra_val32 fa = umbra_f32_from_i32(b, a);
    umbra_val32 fc = umbra_f32_from_i32(b, c);
    // lt→acc, sel(acc, a, c)→acc, add→store
    umbra_val32 mask = umbra_lt_f32(b, fa, fc);
    umbra_val32 s = umbra_sel_32(b, mask, a, c);
    umbra_val32 out = umbra_add_i32(b, s, umbra_imm_i32(b, 1));
    umbra_store_32(b, (umbra_ptr32){.ix=2}, out);
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sa[] = {2,8,2,8}, sc[] = {5,5,5,5};
        int32_t d[4] = {0};
        if (run(&B, bi, 4, 1, (struct umbra_buf[]){
            {.ptr=sa, .count=4}, {.ptr=sc, .count=4}, {.ptr=d, .count=4},
        })) {
            // 2<5→sel a=2, 8<5=false→sel c=5; +1
            d[0] == 3 here; d[1] == 6 here;
        }
    }
    cleanup(&B);
}

// Exercise RA channel-register return: ra_step_unary consuming a non-zero
// channel of load_color where that channel dies at the unary op.
// Must use ops that go through ra_step_unary (abs, neg, imm shifts, etc.)
// not ra_step_alu (which is used by i32_from_f32, add, etc.)
TEST(test_ra_chan_unary) {
    struct umbra_builder *b = umbra_builder();
    umbra_val16 r, g, bl, a;
    umbra_load_16x4(b, (umbra_ptr64){0}, &r, &g, &bl, &a);
    umbra_val32 gf = umbra_f32_from_f16(b, g);
    umbra_val32 bf = umbra_f32_from_f16(b, bl);
    umbra_val32 af = umbra_f32_from_f16(b, a);
    umbra_store_32(b, (umbra_ptr32){.ix=1}, umbra_i32_from_f32(b, umbra_abs_f32(b, gf)));
    umbra_store_32(b, (umbra_ptr32){.ix=2},
                   umbra_i32_from_f32(b, umbra_sub_f32(b, umbra_imm_f32(b, 0), bf)));
    umbra_store_32(b, (umbra_ptr32){.ix=3}, umbra_i32_from_f32(b, umbra_abs_f32(b, af)));
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __fp16 src[] = {
            (__fp16)0.1f, (__fp16)0.25f, (__fp16)0.3f, (__fp16)0.9f,
            (__fp16)0.1f, (__fp16)0.25f, (__fp16)0.3f, (__fp16)0.9f,
            (__fp16)0.1f, (__fp16)0.25f, (__fp16)0.3f, (__fp16)0.9f,
            (__fp16)0.1f, (__fp16)0.25f, (__fp16)0.3f, (__fp16)0.9f,
        };
        int32_t dg[4]={0}, db[4]={0}, da[4]={0};
        if (run(&B, bi, 4, 1, (struct umbra_buf[]){
            {.ptr=src, .count=count(src) / 4},  // 16x4-pixel count, not fp16 element count
            {.ptr=dg, .count=4}, {.ptr=db, .count=4}, {.ptr=da, .count=4},
        })) {
            dg[0] == 0 here;
        }
    }
    cleanup(&B);
}

// Exercise mul-by-power-of-2 peephole where imm has lower val ID than operand.
TEST(test_mul_pow2_peephole) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 four = umbra_imm_i32(b, 4);  // low ID
    umbra_val32 a = umbra_load_32(b, (umbra_ptr32){0});  // higher ID
    umbra_val32 r = umbra_mul_i32(b, a, four);  // sort puts four as x → pow2 x-path
    umbra_store_32(b, (umbra_ptr32){.ix=1}, r);
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t src[] = {3,7,10,0};
        int32_t dst[4] = {0};
        if (run(&B, bi, 4, 1, (struct umbra_buf[]){
            {.ptr=src, .count=count(src)}, {.ptr=dst, .count=count(dst)},
        })) {
            dst[0] == 12 here; dst[1] == 28 here; dst[2] == 40 here; dst[3] == 0 here;
        }
    }
    cleanup(&B);
}

// Exercise umbra_const_eval: op(imm, imm) constant folding.
// All results are compile-time constants stored to a single output buffer.
TEST(test_const_eval) {
    // Split into two programs to stay under Metal's 31-buffer limit.
    // Part 1: float ops
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val32 fa = umbra_imm_f32(b, 10.f);
        umbra_val32 fc = umbra_imm_f32(b, 3.f);
        umbra_store_32(b, (umbra_ptr32){0}, umbra_add_f32(b, fa, fc));
        umbra_store_32(b, (umbra_ptr32){.ix=1}, umbra_sub_f32(b, fa, fc));
        umbra_store_32(b, (umbra_ptr32){.ix=2}, umbra_mul_f32(b, fa, fc));
        umbra_store_32(b, (umbra_ptr32){.ix=3}, umbra_div_f32(b, fa, fc));
        umbra_store_32(b, (umbra_ptr32){.ix=4}, umbra_min_f32(b, fa, fc));
        umbra_store_32(b, (umbra_ptr32){.ix=5}, umbra_max_f32(b, fa, fc));
        umbra_store_32(b, (umbra_ptr32){.ix=6}, umbra_sqrt_f32(b, fc));
        umbra_store_32(b, (umbra_ptr32){.ix=7}, umbra_abs_f32(b, umbra_imm_f32(b, -5.f)));
        umbra_store_32(b, (umbra_ptr32){.ix=8}, umbra_sub_f32(b, umbra_imm_f32(b, 0), fc));
        umbra_store_32(b, (umbra_ptr32){.ix=9}, umbra_round_f32(b, umbra_imm_f32(b, 3.7f)));
        umbra_store_32(b, (umbra_ptr32){.ix=10}, umbra_floor_f32(b, umbra_imm_f32(b, 3.7f)));
        umbra_store_32(b, (umbra_ptr32){.ix=11}, umbra_ceil_f32(b, umbra_imm_f32(b, 3.2f)));
        umbra_store_32(b, (umbra_ptr32){.ix=12}, umbra_round_i32(b, umbra_imm_f32(b, 3.7f)));
        umbra_store_32(b, (umbra_ptr32){.ix=13}, umbra_floor_i32(b, umbra_imm_f32(b, 3.7f)));
        umbra_store_32(b, (umbra_ptr32){.ix=14}, umbra_ceil_i32(b, umbra_imm_f32(b, 3.2f)));
        umbra_store_32(b, (umbra_ptr32){.ix=15}, umbra_f32_from_i32(b, umbra_imm_i32(b, 10)));
        umbra_store_32(b, (umbra_ptr32){.ix=16}, umbra_i32_from_f32(b, fa));
        umbra_store_32(b, (umbra_ptr32){.ix=17}, umbra_eq_f32(b, fa, fc));
        umbra_store_32(b, (umbra_ptr32){.ix=18}, umbra_lt_f32(b, fa, fc));
        umbra_store_32(b, (umbra_ptr32){.ix=19}, umbra_le_f32(b, fa, fc));
        struct test_backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int32_t d[20][4];
            __builtin_memset(d, 0, sizeof d);
            struct umbra_buf bufs[20];
            for (int i = 0; i < 20; i++) { bufs[i] = (struct umbra_buf){.ptr=d[i], .count=4}; }
            if (run(&B, bi, 4, 1, bufs)) {
                union { float f; int32_t i; } u;
                u.f = 13.f; d[0][0] == u.i here;
                u.f = 7.f;  d[1][0] == u.i here;
                d[12][0] == 4 here;
                d[13][0] == 3 here;
                d[14][0] == 4 here;
                d[16][0] == 10 here;
            }
        }
        cleanup(&B);
    }
    // Part 2: integer ops
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val32 a = umbra_imm_i32(b, 10);
        umbra_val32 c = umbra_imm_i32(b, 3);
        umbra_store_32(b, (umbra_ptr32){0}, umbra_sub_i32(b, a, c));
        umbra_store_32(b, (umbra_ptr32){.ix=1}, umbra_mul_i32(b, a, c));
        umbra_store_32(b, (umbra_ptr32){.ix=2}, umbra_shl_i32(b, a, c));
        umbra_store_32(b, (umbra_ptr32){.ix=3}, umbra_shr_u32(b, a, c));
        umbra_store_32(b, (umbra_ptr32){.ix=4}, umbra_shr_s32(b, a, c));
        umbra_store_32(b, (umbra_ptr32){.ix=5}, umbra_and_32(b, a, c));
        umbra_store_32(b, (umbra_ptr32){.ix=6}, umbra_or_32(b, a, c));
        umbra_store_32(b, (umbra_ptr32){.ix=7}, umbra_xor_32(b, a, c));
        umbra_store_32(b, (umbra_ptr32){.ix=8}, umbra_sel_32(b, umbra_imm_i32(b, -1), a, c));
        umbra_store_32(b, (umbra_ptr32){.ix=9}, umbra_eq_i32(b, a, c));
        umbra_store_32(b, (umbra_ptr32){.ix=10}, umbra_lt_s32(b, a, c));
        umbra_store_32(b, (umbra_ptr32){.ix=11}, umbra_le_s32(b, a, c));
        umbra_store_32(b, (umbra_ptr32){.ix=12}, umbra_lt_u32(b, a, c));
        umbra_store_32(b, (umbra_ptr32){.ix=13}, umbra_le_u32(b, a, c));
        // sel_32 with c that's neither 0 nor -1 falls through the short-circuits
        // in umbra_sel_32 and reaches the const_eval path: result is bit-wise
        // (c & t) | (~c & f).  c=5, t=0xff, f=0xf0 → 5 | 0xf0 = 0xf5.
        umbra_store_32(b, (umbra_ptr32){.ix=14},
            umbra_sel_32(b, umbra_imm_i32(b, 5),
                            umbra_imm_i32(b, 0xff),
                            umbra_imm_i32(b, 0xf0)));
        struct test_backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int32_t d[15][4];
            __builtin_memset(d, 0, sizeof d);
            struct umbra_buf bufs[15];
            for (int i = 0; i < 15; i++) { bufs[i] = (struct umbra_buf){.ptr=d[i], .count=4}; }
            if (run(&B, bi, 4, 1, bufs)) {
                d[0][0]  == 7    here;   // 10-3
                d[1][0]  == 30   here;   // 10*3
                d[7][0]  == 9    here;   // 10^3
                d[8][0]  == 10   here;   // sel(-1, 10, 3) = 10
                d[9][0]  == 0    here;   // eq(10,3)=0
                d[14][0] == 0xf5 here;   // sel(5, 0xff, 0xf0) folds via const_eval
            }
        }
        cleanup(&B);
    }
}

TEST(test_acc_coverage) {
    // Exercise interpreter acc chain patterns: chain_producer → op → store.
    // Use non-trivial constants to avoid identity optimizations (x+0=x).
    {
        // Unary m_r: unique sub starts chain → unary(r_r) → i32_from_f32(m_r) → store.
        struct umbra_builder *b = umbra_builder();
        umbra_val32 x = umbra_load_32(b, (umbra_ptr32){0});
        umbra_val32 fx = umbra_f32_from_i32(b, x);
        // Each sub has a different imm to avoid dedup.
        umbra_val32 p0 = umbra_sub_f32(b, fx, umbra_imm_f32(b, 1));
        umbra_store_32(b, (umbra_ptr32){.ix=1},
                       umbra_i32_from_f32(b, umbra_sub_f32(b, umbra_imm_f32(b, 0), p0)));
        umbra_val32 p1 = umbra_sub_f32(b, fx, umbra_imm_f32(b, 2));
        umbra_store_32(b, (umbra_ptr32){.ix=2}, umbra_i32_from_f32(b, umbra_round_f32(b, p1)));
        umbra_val32 p2 = umbra_sub_f32(b, fx, umbra_imm_f32(b, 3));
        umbra_store_32(b, (umbra_ptr32){.ix=3}, umbra_i32_from_f32(b, umbra_floor_f32(b, p2)));
        umbra_val32 p3 = umbra_sub_f32(b, fx, umbra_imm_f32(b, 4));
        umbra_store_32(b, (umbra_ptr32){.ix=4}, umbra_i32_from_f32(b, umbra_ceil_f32(b, p3)));
        struct test_backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int32_t sa[] = {10};
            int32_t d[5][1]; __builtin_memset(d, 0, sizeof d);
            if (run(&B, bi, 1, 1, (struct umbra_buf[]){
                {.ptr=sa, .count=1},
                {.ptr=d[1], .count=1}, {.ptr=d[2], .count=1},
                {.ptr=d[3], .count=1}, {.ptr=d[4], .count=1},
            })) {
                d[1][0] == -9 here;  // neg(10-1)=-9
                d[2][0] == 8 here;   // round(10-2)=8
                d[3][0] == 7 here;   // floor(10-3)=7
                d[4][0] == 6 here;   // ceil(10-4)=6
            }
        }
        cleanup(&B);
    }
    {
        // Comparison m_rm: sub→acc, then le_f32(acc, mem)→store.
        struct umbra_builder *b = umbra_builder();
        umbra_val32 x = umbra_load_32(b, (umbra_ptr32){0});
        umbra_val32 y = umbra_load_32(b, (umbra_ptr32){.ix=1});
        umbra_val32 fx = umbra_f32_from_i32(b, x);
        umbra_val32 fy = umbra_f32_from_i32(b, y);
        umbra_val32 s = umbra_sub_f32(b, fx, umbra_imm_f32(b, 0.5f));
        umbra_store_32(b, (umbra_ptr32){.ix=2}, umbra_le_f32(b, s, fy));
        struct test_backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int32_t sa[] = {2, 20}, sb[] = {5, 5};
            int32_t d[2] = {0};
            if (run(&B, bi, 2, 1, (struct umbra_buf[]){
                {.ptr=sa, .count=2}, {.ptr=sb, .count=2}, {.ptr=d, .count=2},
            })) {
                d[0] == -1 here;  // 2-0.5=1.5<=5 → true
                d[1] == 0 here;   // 20-0.5=19.5<=5 → false
            }
        }
        cleanup(&B);
    }
    {
        // Imm acc variants: sub(a,b)(r_mm)→imm_op(m_r)→store.
        // Each in its own builder to avoid dedup across chains.
#define ACC_IMM_TEST_I(op, k, expected) { \
        struct umbra_builder *b = umbra_builder(); \
        umbra_val32 a = umbra_load_32(b, (umbra_ptr32){0}), \
                    c = umbra_load_32(b, (umbra_ptr32){.ix=1}); \
        umbra_store_32(b, (umbra_ptr32){.ix=2}, \
                       op(b, umbra_sub_i32(b, a, c), umbra_imm_i32(b, k))); \
        struct test_backends B = make(b); \
        for (int bi = 0; bi < NUM_BACKENDS; bi++) { \
            int32_t sa[]={10}, sc[]={3}, d[]={0}; \
            struct umbra_buf bs[] = {{.ptr=sa,.count=1},{.ptr=sc,.count=1},{.ptr=d,.count=1}}; \
            if (run(&B, bi, 1, 1, bs)) { \
                d[0] == expected here; \
            } \
        } cleanup(&B); }
#define ACC_IMM_TEST_F(op, k, expected) { \
        struct umbra_builder *b = umbra_builder(); \
        umbra_val32 a = umbra_load_32(b, (umbra_ptr32){0}), \
                    c = umbra_load_32(b, (umbra_ptr32){.ix=1}); \
        umbra_val32 fa = umbra_f32_from_i32(b, a), fc = umbra_f32_from_i32(b, c); \
        umbra_store_32(b, (umbra_ptr32){.ix=2}, umbra_i32_from_f32(b, \
            op(b, umbra_sub_f32(b, fa, fc), umbra_imm_f32(b, k)))); \
        struct test_backends B = make(b); \
        for (int bi = 0; bi < NUM_BACKENDS; bi++) { \
            int32_t sa[]={10}, sc[]={3}, d[]={0}; \
            struct umbra_buf bs[] = {{.ptr=sa,.count=1},{.ptr=sc,.count=1},{.ptr=d,.count=1}}; \
            if (run(&B, bi, 1, 1, bs)) { \
                d[0] == expected here; \
            } \
        } cleanup(&B); }
#define ACC_IMM_TEST_CMP_I(op, k, expected) { \
        struct umbra_builder *b = umbra_builder(); \
        umbra_val32 a = umbra_load_32(b, (umbra_ptr32){0}), \
                    c = umbra_load_32(b, (umbra_ptr32){.ix=1}); \
        umbra_store_32(b, (umbra_ptr32){.ix=2}, \
                       op(b, umbra_sub_i32(b, a, c), umbra_imm_i32(b, k))); \
        struct test_backends B = make(b); \
        for (int bi = 0; bi < NUM_BACKENDS; bi++) { \
            int32_t sa[]={10}, sc[]={3}, d[]={0}; \
            struct umbra_buf bs[] = {{.ptr=sa,.count=1},{.ptr=sc,.count=1},{.ptr=d,.count=1}}; \
            if (run(&B, bi, 1, 1, bs)) { \
                d[0] == expected here; \
            } \
        } cleanup(&B); }
#define ACC_IMM_TEST_CMP_F(op, k, expected) { \
        struct umbra_builder *b = umbra_builder(); \
        umbra_val32 a = umbra_load_32(b, (umbra_ptr32){0}), \
                    c = umbra_load_32(b, (umbra_ptr32){.ix=1}); \
        umbra_val32 fa = umbra_f32_from_i32(b, a), fc = umbra_f32_from_i32(b, c); \
        umbra_store_32(b, (umbra_ptr32){.ix=2}, \
            op(b, umbra_sub_f32(b, fa, fc), umbra_imm_f32(b, k))); \
        struct test_backends B = make(b); \
        for (int bi = 0; bi < NUM_BACKENDS; bi++) { \
            int32_t sa[]={10}, sc[]={3}, d[]={0}; \
            struct umbra_buf bs[] = {{.ptr=sa,.count=1},{.ptr=sc,.count=1},{.ptr=d,.count=1}}; \
            if (run(&B, bi, 1, 1, bs)) { \
                d[0] == expected here; \
            } \
        } cleanup(&B); }
        // sub(10,3)=7
        ACC_IMM_TEST_I(umbra_shr_s32, 1, 3)          // 7>>1
        ACC_IMM_TEST_I(umbra_and_32, 0xFF, 7)         // 7&0xFF
        ACC_IMM_TEST_I(umbra_or_32, 0x1000, 4103)     // 7|0x1000
        ACC_IMM_TEST_I(umbra_xor_32, 0x0F, 8)         // 7^0x0F
        ACC_IMM_TEST_F(umbra_add_f32, 1.0f, 8)        // 7+1
        ACC_IMM_TEST_F(umbra_sub_f32, 1.0f, 6)        // 7-1
        ACC_IMM_TEST_CMP_F(umbra_lt_f32, 10.0f, -1)   // 7<10
        ACC_IMM_TEST_CMP_F(umbra_le_f32, 7.0f, -1)    // 7<=7
        ACC_IMM_TEST_CMP_I(umbra_eq_i32, 7, -1)       // 7==7
        ACC_IMM_TEST_CMP_I(umbra_lt_s32, 10, -1)      // 7<10
        ACC_IMM_TEST_CMP_I(umbra_le_s32, 7, -1)       // 7<=7
#undef ACC_IMM_TEST_I
#undef ACC_IMM_TEST_F
#undef ACC_IMM_TEST_CMP_I
#undef ACC_IMM_TEST_CMP_F
    }
}

TEST(test_acc_coverage_extra) {
    // Exercise remaining uncovered interpreter acc variants:
    // - CMP3 m_rm (comparison chain-end)
    // - UN2 for round_f32, floor_f32, ceil_f32
#define ACC_UNARY_F(unary, k, expected) { \
        struct umbra_builder *b = umbra_builder(); \
        umbra_val32 a = umbra_load_32(b, (umbra_ptr32){0}), \
                    c = umbra_load_32(b, (umbra_ptr32){.ix=1}); \
        umbra_val32 s = umbra_sub_f32(b, umbra_f32_from_i32(b, a), umbra_f32_from_i32(b, c)); \
        umbra_store_32(b, (umbra_ptr32){.ix=2}, umbra_i32_from_f32(b, unary(b, s))); \
        struct test_backends B = make(b); \
        for (int bi = 0; bi < NUM_BACKENDS; bi++) { \
            int32_t sa[]={10}, sc[]={k}, d[]={0}; \
            struct umbra_buf bs[] = {{.ptr=sa,.count=1},{.ptr=sc,.count=1},{.ptr=d,.count=1}}; \
            if (run(&B, bi, 1, 1, bs)) { \
                d[0] == expected here; \
            } \
        } cleanup(&B); }
    // round(10.0-3.0)=round(7.0)=7, floor(10.0-4.0)=floor(6.0)=6, ceil(10.0-5.0)=ceil(5.0)=5
    ACC_UNARY_F(umbra_round_f32, 3, 7)
    ACC_UNARY_F(umbra_floor_f32, 4, 6)
    ACC_UNARY_F(umbra_ceil_f32,  5, 5)
#undef ACC_UNARY_F
    // CMP m_rm: sub→acc, then eq_f32(acc, mem)→store.
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val32 a = umbra_load_32(b, (umbra_ptr32){0}),
                    c = umbra_load_32(b, (umbra_ptr32){.ix=1});
        umbra_val32 fa = umbra_f32_from_i32(b, a), fc = umbra_f32_from_i32(b, c);
        umbra_val32 s = umbra_sub_f32(b, fa, umbra_imm_f32(b, 1));
        umbra_store_32(b, (umbra_ptr32){.ix=2}, umbra_eq_f32(b, s, fc));
        struct test_backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int32_t sa[]={4}, sc[]={3}, d[]={0};
            struct umbra_buf bs[] = {{.ptr=sa,.count=1},{.ptr=sc,.count=1},{.ptr=d,.count=1}};
            if (run(&B, bi, 1, 1, bs)) {
                d[0] == -1 here;  // 4-1=3.0 == 3.0 → true
            }
        }
        cleanup(&B);
    }
}

TEST(test_acc_acc_register_variants) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 x = umbra_load_32(b, (umbra_ptr32){0});

    umbra_val32 scaled = umbra_mul_f32(b, x, umbra_imm_f32(b, 1.0f));
    umbra_val32 ceiled = umbra_ceil_i32(b, scaled);
    umbra_val32 shifted = umbra_shl_i32(b, ceiled, umbra_imm_i32(b, 1));
    umbra_store_32(b, (umbra_ptr32){.ix=1}, shifted);

    umbra_val32 y = umbra_add_f32(b, x, umbra_imm_f32(b, 0.0f));
    umbra_val32 eq = umbra_eq_f32(b, y, umbra_imm_f32(b, 0.0f));
    umbra_val32 masked = umbra_and_32(b, eq, umbra_imm_i32(b, 1));
    umbra_store_32(b, (umbra_ptr32){.ix=2}, masked);

    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        float src[] = {1.5f, 0.0f, -2.3f, 3.0f};
        int32_t dst1[4] = {0}, dst2[4] = {0};
        if (run(&B, bi, 4, 1, (struct umbra_buf[]){
            {.ptr=src, .count=count(src)},
            {.ptr=dst1, .count=count(dst1)},
            {.ptr=dst2, .count=count(dst2)},
        })) {
            dst1[0] == 4 here;   // ceil(1.5)*2 = 2*2 = 4
            dst1[1] == 0 here;   // ceil(0)*2 = 0
            dst1[2] == -4 here;  // ceil(-2.3)*2 = -2*2 = -4
            dst1[3] == 6 here;   // ceil(3)*2 = 3*2 = 6
            dst2[0] == 0 here;   // 1.5 == 0? no
            dst2[1] == 1 here;   // 0.0 == 0? yes -> -1 & 1 = 1
            dst2[2] == 0 here;   // -2.3 == 0? no
            dst2[3] == 0 here;   // 3.0 == 0? no
        }
    }
    cleanup(&B);
}

TEST(test_strided_2d_dispatch) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 x   = umbra_x(b);
    umbra_val32 y   = umbra_y(b);
    umbra_val32 k   = umbra_imm_i32(b, 1000);
    umbra_val32 val = umbra_add_i32(b, x, umbra_mul_i32(b, y, k));
    umbra_store_32(b, (umbra_ptr32){0}, val);
    struct test_backends B = make(b);

    enum { S = 32, TH = 16, L = 5, T = 3, R = 21, BT = 11 };
    int32_t buf[S * TH];

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(buf, 0xff, sizeof buf);
        if (!B.p[bi]) { continue; }
        B.p[bi]->queue(B.p[bi], L, T, R, BT,
                            (struct umbra_buf[]){{.ptr=buf, .count=count(buf), .stride=S}});
        B.be[bi]->flush(B.be[bi]);
        for (int row = T; row < BT; row++) {
            for (int col = L; col < R; col++) {
                buf[row * S + col] == col + row * 1000 here;
            }
        }
        for (int row = 0; row < TH; row++) {
            for (int col = 0; col < S; col++) {
                if (row >= T && row < BT && col >= L && col < R) { continue; }
                buf[row * S + col] == (int32_t)0xffffffff here;
            }
        }
    }
    cleanup(&B);
}

TEST(test_strided_load_arbitrary_tile) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 v   = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32 one = umbra_imm_i32(b, 1);
    umbra_store_32(b, (umbra_ptr32){.ix=1}, umbra_add_i32(b, v, one));
    struct test_backends B = make(b);

    enum { S = 20, TH = 10, L = 3, T = 2, R = 15, BT = 7 };
    int32_t src[S * TH], dst[S * TH];
    for (int i = 0; i < S * TH; i++) { src[i] = i; }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!B.p[bi]) { continue; }
        B.p[bi]->queue(B.p[bi], L, T, R, BT,
                            (struct umbra_buf[]){{.ptr=src, .count=count(src), .stride=S},
                                          {.ptr=dst, .count=count(dst), .stride=S}});
        B.be[bi]->flush(B.be[bi]);
        for (int row = T; row < BT; row++) {
            for (int col = L; col < R; col++) {
                int idx = row * S + col;
                dst[idx] == src[idx] + 1 here;
            }
        }
        for (int row = 0; row < TH; row++) {
            for (int col = 0; col < S; col++) {
                if (row >= T && row < BT && col >= L && col < R) { continue; }
                dst[row * S + col] == 0 here;
            }
        }
    }
    cleanup(&B);
}

TEST(test_two_buffers_different_row_bytes) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32            v   = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32            one = umbra_imm_i32(b, 1);
    umbra_store_32(b, (umbra_ptr32){.ix=1}, umbra_add_i32(b, v, one));
    struct test_backends B = make(b);

    enum { SW = 32, DW = 20, H = 5, L = 3, T = 1, R = 15, BT = 4 };
    int32_t src[SW * H], dst[DW * H];
    for (int i = 0; i < SW * H; i++) { src[i] = i * 10; }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!B.p[bi]) { continue; }
        B.p[bi]->queue(B.p[bi], L, T, R, BT,
                            (struct umbra_buf[]){{.ptr=src, .count=count(src), .stride=SW},
                                          {.ptr=dst, .count=count(dst), .stride=DW}});
        B.be[bi]->flush(B.be[bi]);
        for (int row = T; row < BT; row++) {
            for (int col = L; col < R; col++) {
                int si = row * SW + col;
                int di = row * DW + col;
                dst[di] == src[si] + 1 here;
            }
        }
    }
    cleanup(&B);
}

TEST(test_deref_row_bytes_l_gt_0) {
    struct umbra_builder *b = umbra_builder();
    umbra_ptr32           src = umbra_deref_ptr32(b, (umbra_ptr32){.ix=1}, 0);
    umbra_val32            v   = umbra_load_32(b, src);
    umbra_val32            one = umbra_imm_i32(b, 1);
    umbra_store_32(b, (umbra_ptr32){.ix=2}, umbra_add_i32(b, v, one));
    struct test_backends B = make(b);

    enum { S = 20, TH = 6, L = 3, T = 1, R = 15, BT = 5 };
    int32_t src_px[S * TH];
    for (int i = 0; i < S * TH; i++) { src_px[i] = i; }
    int32_t dst_px[S * TH];

    struct umbra_buf deref = {.ptr=src_px, .count=S * TH, .stride=S};

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst_px, 0, sizeof dst_px);
        if (!B.p[bi]) { continue; }
        B.p[bi]->queue(B.p[bi], L, T, R, BT,
                            (struct umbra_buf[]){{0},
                                          {.ptr=&deref, .count=(int)(sizeof deref / 4)},
                                          {.ptr=dst_px, .count=count(dst_px), .stride=S}});
        B.be[bi]->flush(B.be[bi]);
        for (int row = T; row < BT; row++) {
            for (int col = L; col < R; col++) {
                int idx = row * S + col;
                dst_px[idx] == src_px[idx] + 1 here;
            }
        }
        for (int row = 0; row < TH; row++) {
            for (int col = 0; col < S; col++) {
                if (row >= T && row < BT && col >= L && col < R) { continue; }
                dst_px[row * S + col] == 0 here;
            }
        }
    }
    cleanup(&B);
}

TEST(test_deref_16bit_row_bytes_l_gt_0) {
    struct umbra_builder *b = umbra_builder();
    umbra_ptr16           src = umbra_deref_ptr16(b, (umbra_ptr32){.ix=1}, 0);
    umbra_val32            v   = umbra_f32_from_f16(b, umbra_load_16(b, src));
    umbra_val32            one = umbra_imm_f32(b, 1.0f);
    umbra_store_32(b, (umbra_ptr32){.ix=2}, umbra_add_f32(b, v, one));
    struct test_backends B = make(b);

    enum { S = 16, TH = 4, L = 2, T = 1, R = 10, BT = 3 };
    uint16_t src_px[S * TH];
    for (int i = 0; i < S * TH; i++) {
        float f = (float)i;
        __fp16 h = (__fp16)f;
        __builtin_memcpy(&src_px[i], &h, 2);
    }
    float dst_px[S * TH];

    struct umbra_buf deref = {.ptr=src_px, .count=S * TH, .stride=S};

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst_px, 0, sizeof dst_px);
        if (!B.p[bi]) { continue; }
        B.p[bi]->queue(B.p[bi], L, T, R, BT,
                            (struct umbra_buf[]){{0},
                                          {.ptr=&deref, .count=(int)(sizeof deref / 4)},
                                          {.ptr=dst_px, .count=count(dst_px), .stride=S}});
        B.be[bi]->flush(B.be[bi]);
        for (int row = T; row < BT; row++) {
            for (int col = L; col < R; col++) {
                int idx = row * S + col;
                equiv(dst_px[idx], (float)(row * S + col) + 1.0f) here;
            }
        }
    }
    cleanup(&B);
}

// Three derefs in one IR exercise the rb_gpr=0 branch in the JIT pointer
// resolver: only the first two derefs get a row_bytes register, so the third
// deref's per-pixel pointer is just a copy of the deref'd base. The flat
// source buffer makes this safe — every (x,y) reads bufC[x] regardless of y.
TEST(test_deref_third_uses_else_branch) {
    enum { W = 8, H = 3 };
    struct umbra_builder         *b = umbra_builder();
    struct {
        struct umbra_buf a, b, c;
    } uni = {0};
    int const off1 = (int)(__builtin_offsetof(__typeof__(uni), a) / 4),
              off2 = (int)(__builtin_offsetof(__typeof__(uni), b) / 4),
              off3 = (int)(__builtin_offsetof(__typeof__(uni), c) / 4);
    umbra_ptr32 d1  = umbra_deref_ptr32(b, (umbra_ptr32){0}, off1);
    umbra_ptr32 d2  = umbra_deref_ptr32(b, (umbra_ptr32){0}, off2);
    umbra_ptr32 d3  = umbra_deref_ptr32(b, (umbra_ptr32){0}, off3);
    umbra_val32 v1  = umbra_load_32(b, d1);
    umbra_val32 v2  = umbra_load_32(b, d2);
    umbra_val32 v3  = umbra_load_32(b, d3);
    umbra_val32 sum = umbra_add_i32(b, umbra_add_i32(b, v1, v2), v3);
    umbra_store_32(b, (umbra_ptr32){.ix=1}, sum);
    struct test_backends B = make(b);

    int32_t bufA[W * H], bufB[W * H], bufC[W];
    for (int i = 0; i < W * H; i++) { bufA[i] = i;       bufB[i] = i * 10; }
    for (int i = 0; i < W;     i++) { bufC[i] = 1000 + i; }
    uni.a = (struct umbra_buf){.ptr=bufA, .count=count(bufA), .stride=W};
    uni.b = (struct umbra_buf){.ptr=bufB, .count=count(bufB), .stride=W};
    // bufC is flat (row_bytes==0): broadcast the same row to every y.
    uni.c = (struct umbra_buf){.ptr=bufC, .count=count(bufC)};

    int32_t dst[W * H];
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (run(&B, bi, W, H, (struct umbra_buf[]){
                {.ptr=&uni, .count=(int)(sizeof uni / 4)},
                {.ptr=dst, .count=count(dst), .stride=W}})) {
            for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                    int idx = y * W + x;
                    dst[idx] == bufA[idx] + bufB[idx] + bufC[x] here;
                }
            }
        }
    }
    cleanup(&B);
}

TEST(test_i32_from_f32_acc_acc) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 x = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32 f = umbra_f32_from_i32(b, x);
    umbra_val32 a = umbra_add_f32(b, f, umbra_imm_f32(b, 0.5f));
    umbra_val32 t = umbra_i32_from_f32(b, a);
    umbra_val32 r = umbra_add_i32(b, t, umbra_imm_i32(b, 100));
    umbra_store_32(b, (umbra_ptr32){.ix=1}, r);
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t src[4] = {1, 2, 3, 4};
        int32_t dst[4] = {0};
        if (run(&B, bi, 4, 1, (struct umbra_buf[]){
                {.ptr=src, .count=count(src)},
                {.ptr=dst, .count=count(dst)}})) {
            // (1+0.5)→1 +100=101, (2+0.5)→2 +100=102, etc.
            dst[0] == 101 here; dst[1] == 102 here;
            dst[2] == 103 here; dst[3] == 104 here;
        }
    }
    cleanup(&B);
}

TEST(test_tail_to_vector_row_transition) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 v   = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32 one = umbra_imm_i32(b, 1);
    umbra_store_32(b, (umbra_ptr32){.ix=1}, umbra_add_i32(b, v, one));
    struct test_backends B = make(b);

    enum { S = 20, TH = 3, L = 3, T = 0, R = 15, BT = 3 };
    int32_t src[S * TH], dst[S * TH];
    for (int i = 0; i < S * TH; i++) { src[i] = i; }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!B.p[bi]) { continue; }
        B.p[bi]->queue(B.p[bi], L, T, R, BT,
                            (struct umbra_buf[]){{.ptr=src, .count=count(src), .stride=S},
                                          {.ptr=dst, .count=count(dst), .stride=S}});
        B.be[bi]->flush(B.be[bi]);
        for (int row = T; row < BT; row++) {
            for (int col = L; col < R; col++) {
                int idx = row * S + col;
                dst[idx] == src[idx] + 1 here;
            }
        }
    }
    cleanup(&B);
}

TEST(test_gather_partial_oob) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 ix = umbra_x(b);
    umbra_val32 v  = umbra_gather_32(b, (umbra_ptr32){0}, ix);
    umbra_store_32(b, (umbra_ptr32){.ix=1}, v);
    struct test_backends B = make(b);

    int32_t src[6] = {10, 20, 30, 40, 50, 60};
    int32_t dst[16];

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0xff, sizeof dst);
        if (!B.p[bi]) { continue; }
        B.p[bi]->queue(B.p[bi], 0, 0, 16, 1,
                            (struct umbra_buf[]){{.ptr=src, .count=count(src)},
                                          {.ptr=dst, .count=count(dst)}});
        B.be[bi]->flush(B.be[bi]);
        for (int col = 0; col < 6; col++) { dst[col] == src[col] here; }
        for (int col = 6; col < 16; col++) { dst[col] == 0 here; }
    }
    cleanup(&B);
}

// Build a program large enough to overflow the JIT's initial code buffer estimate,
// exercising the put() grow path on arm64 (mmap resize) and x86 (realloc).
TEST(test_jit_code_buffer_overflow) {
    struct umbra_builder *b = umbra_builder();
    umbra_ptr32 p = {0};
    for (int i = 0; i < 500; i++) {
        umbra_val32 r, g, bl, a;
        umbra_load_8x4(b, p, &r, &g, &bl, &a);
        umbra_store_8x4(b, p, r, g, bl, a);
    }
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);

    struct umbra_backend *be = umbra_backend_jit();
    if (be) {
        struct umbra_program *prog = be->compile(be, ir);
        umbra_program_free(prog);
        umbra_backend_free(be);
    }

    be = umbra_backend_interp();
    { struct umbra_program *prog = be->compile(be, ir);
      umbra_program_free(prog); }
    umbra_backend_free(be);

    umbra_flat_ir_free(ir);
}

// Cover const-fold true-branches in op_eval comparisons and min.
TEST(test_const_fold_coverage) {
    struct umbra_builder *b = umbra_builder();
    umbra_val32 one = umbra_imm_f32(b, 1.f),
                two = umbra_imm_f32(b, 2.f);
    umbra_val32 i1  = umbra_imm_i32(b, 1),
                i2  = umbra_imm_i32(b, 2);

    int p = 0;
    // min(1,2) → 1  (x < y true)
    umbra_store_32(b, (umbra_ptr32){.ix=p++}, umbra_min_f32(b, one, two));
    // eq_f32(1,1) → -1
    umbra_store_32(b, (umbra_ptr32){.ix=p++}, umbra_eq_f32(b, one, one));
    // lt_f32(1,2) → -1
    umbra_store_32(b, (umbra_ptr32){.ix=p++}, umbra_lt_f32(b, one, two));
    // le_f32(1,1) → -1
    umbra_store_32(b, (umbra_ptr32){.ix=p++}, umbra_le_f32(b, one, one));
    // eq_i32(1,1) → -1
    umbra_store_32(b, (umbra_ptr32){.ix=p++}, umbra_eq_i32(b, i1, i1));
    // lt_s32(1,2) → -1
    umbra_store_32(b, (umbra_ptr32){.ix=p++}, umbra_lt_s32(b, i1, i2));
    // le_s32(1,1) → -1
    umbra_store_32(b, (umbra_ptr32){.ix=p++}, umbra_le_s32(b, i1, i1));
    // lt_u32(1,2) → -1
    umbra_store_32(b, (umbra_ptr32){.ix=p++}, umbra_lt_u32(b, i1, i2));
    // le_u32(1,1) → -1
    umbra_store_32(b, (umbra_ptr32){.ix=p++}, umbra_le_u32(b, i1, i1));

    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t out[9][8];
        __builtin_memset(out, 0, sizeof out);
        struct umbra_buf bufs[9];
        for (int i = 0; i < 9; i++) {
            bufs[i] = (struct umbra_buf){.ptr = out[i], .count=count(out[i])};
        }
        if (run(&B, bi, 8, 1, bufs)) {
            union { float f; int32_t i; } v;
            v.i = out[0][0]; v.f == 1.f here;  // min
            for (int i = 1; i < 9; i++) {
                out[i][0] == -1 here;           // all comparisons true → -1
            }
        }
    }
    cleanup(&B);
}

TEST(test_stats_safe) {
    struct umbra_backend *be[] = {
        umbra_backend_interp(),
        umbra_backend_jit(),
        umbra_backend_metal(),
        umbra_backend_vulkan(),
        umbra_backend_wgpu(),
    };
    // stats must be safe to call on a fresh backend with no prior work.
    for (int i = 0; i < NUM_BACKENDS; i++) {
        if (be[i]) {
            struct umbra_backend_stats st = be[i]->stats(be[i]);
            st.gpu_sec == 0.0 here;
            st.upload_bytes == 0 here;
            st.uniform_ring_rotations == 0 here;
            st.dispatches == 0 here;
        }
    }

    // Also safe after compile+flush with no queued work.
    struct umbra_builder *b = umbra_builder();
    umbra_store_32(b, (umbra_ptr32){0}, umbra_x(b));
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);

    for (int i = 0; i < NUM_BACKENDS; i++) {
        if (be[i]) {
            struct umbra_program *p = be[i]->compile(be[i], ir);
            be[i]->flush(be[i]);
            struct umbra_backend_stats st = be[i]->stats(be[i]);
            st.dispatches == 0 here;
            umbra_program_free(p);
        }
    }

    umbra_flat_ir_free(ir);
    for (int i = 0; i < NUM_BACKENDS; i++) {
        umbra_backend_free(be[i]);
    }
}

TEST(test_loop_accumulate) {
    struct umbra_builder *b = umbra_builder();
    struct umbra_var32 acc = umbra_var32(b);
    umbra_val32 n = umbra_uniform_32(b, (umbra_ptr32){0}, 0);
    umbra_loop(b, n); {
        umbra_store_var32(b, acc, umbra_add_i32(b, umbra_load_var32(b, acc), umbra_imm_i32(b, 1)));
    } umbra_end_loop(b);
    umbra_store_32(b, (umbra_ptr32){.ix = 1}, umbra_load_var32(b, acc));
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t uni[1] = {10};
        int32_t out[4] = {0};
        if (run(&B, bi, 4, 1, (struct umbra_buf[]){
                {.ptr = uni, .count=count(uni)},
                {.ptr = out, .count=count(out), .stride=count(out)},
            })) {
            out[0] == 10 here;
            out[1] == 10 here;
            out[2] == 10 here;
            out[3] == 10 here;
        }
    }
    cleanup(&B);
}

TEST(test_loop_zero_trip) {
    struct umbra_builder *b = umbra_builder();
    struct umbra_var32 acc = umbra_var32(b);
    umbra_store_var32(b, acc, umbra_imm_i32(b, 42));
    umbra_val32 n = umbra_uniform_32(b, (umbra_ptr32){0}, 0);
    umbra_loop(b, n); {
        umbra_store_var32(b, acc, umbra_imm_i32(b, 99));
    } umbra_end_loop(b);
    umbra_store_32(b, (umbra_ptr32){.ix = 1}, umbra_load_var32(b, acc));
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t uni[1] = {0};
        int32_t out[2] = {0};
        if (run(&B, bi, 2, 1, (struct umbra_buf[]){
                {.ptr = uni, .count=count(uni)},
                {.ptr = out, .count=count(out), .stride=count(out)},
            })) {
            out[0] == 42 here;
            out[1] == 42 here;
        }
    }
    cleanup(&B);
}

TEST(test_loop_gather_sum) {
    struct umbra_builder *b = umbra_builder();
    struct umbra_var32 acc = umbra_var32(b);
    umbra_val32 n = umbra_uniform_32(b, (umbra_ptr32){0}, 0);
    umbra_val32 i = umbra_loop(b, n); {
        umbra_store_var32(b, acc, umbra_add_f32(b, umbra_load_var32(b, acc),
                                              umbra_gather_32(b, (umbra_ptr32){.ix = 1}, i)));
    } umbra_end_loop(b);
    umbra_store_32(b, (umbra_ptr32){.ix = 2}, umbra_load_var32(b, acc));
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t uni[1] = {4};
        float data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
        float out[2] = {0};
        if (run(&B, bi, 2, 1, (struct umbra_buf[]){
                {.ptr = uni,  .count=count(uni)},
                {.ptr = data, .count=count(data)},
                {.ptr = out,  .count=count(out), .stride=count(out)},
            })) {
            out[0] == 10.0f here;
            out[1] == 10.0f here;
        }
    }
    cleanup(&B);
}

TEST(test_loop_induction_value) {
    struct umbra_builder *b = umbra_builder();
    struct umbra_var32 acc = umbra_var32(b);
    umbra_val32 n = umbra_uniform_32(b, (umbra_ptr32){0}, 0);
    umbra_val32 i = umbra_loop(b, n); {
        umbra_store_var32(b, acc, umbra_add_i32(b, umbra_load_var32(b, acc), i));
    } umbra_end_loop(b);
    umbra_store_32(b, (umbra_ptr32){.ix = 1}, umbra_load_var32(b, acc));
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t uni[1] = {5};
        int32_t out[2] = {0};
        if (run(&B, bi, 2, 1, (struct umbra_buf[]){
                {.ptr = uni, .count=count(uni)},
                {.ptr = out, .count=count(out), .stride=count(out)},
            })) {
            out[0] == 10 here;
            out[1] == 10 here;
        }
    }
    cleanup(&B);
}

TEST(test_loop_multi_var) {
    struct umbra_builder *b = umbra_builder();
    struct umbra_var32 sum = umbra_var32(b);
    struct umbra_var32 count = umbra_var32(b);
    umbra_val32 n = umbra_uniform_32(b, (umbra_ptr32){0}, 0);
    umbra_loop(b, n); {
        umbra_val32 s = umbra_load_var32(b, sum);
        umbra_val32 c = umbra_load_var32(b, count);
        umbra_store_var32(b, sum, umbra_add_f32(b, s, umbra_gather_32(b, (umbra_ptr32){.ix = 1}, c)));
        umbra_store_var32(b, count, umbra_add_i32(b, c, umbra_imm_i32(b, 1)));
    } umbra_end_loop(b);
    umbra_store_32(b, (umbra_ptr32){.ix = 2}, umbra_load_var32(b, sum));
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t uni[1] = {3};
        float data[4] = {10.0f, 20.0f, 30.0f, 0.0f};
        float out[2] = {0};
        if (run(&B, bi, 2, 1, (struct umbra_buf[]){
                {.ptr = uni,  .count=count(uni)},
                {.ptr = data, .count=count(data)},
                {.ptr = out,  .count=count(out), .stride=count(out)},
            })) {
            out[0] == 60.0f here;
            out[1] == 60.0f here;
        }
    }
    cleanup(&B);
}

TEST(test_loop_init_then_accumulate) {
    struct umbra_builder *b = umbra_builder();
    struct umbra_var32 acc = umbra_var32(b);
    umbra_store_var32(b, acc, umbra_imm_i32(b, 100));
    umbra_val32 n = umbra_uniform_32(b, (umbra_ptr32){0}, 0);
    umbra_loop(b, n); {
        umbra_store_var32(b, acc, umbra_add_i32(b, umbra_load_var32(b, acc), umbra_imm_i32(b, 1)));
    } umbra_end_loop(b);
    umbra_store_32(b, (umbra_ptr32){.ix = 1}, umbra_load_var32(b, acc));
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t uni[1] = {5};
        int32_t out[2] = {0};
        if (run(&B, bi, 2, 1, (struct umbra_buf[]){
                {.ptr = uni, .count=count(uni)},
                {.ptr = out, .count=count(out), .stride=count(out)},
            })) {
            out[0] == 105 here;
            out[1] == 105 here;
        }
    }
    cleanup(&B);
}

TEST(test_loop_pre_and_post) {
    struct umbra_builder *b = umbra_builder();

    umbra_val32 px = umbra_load_32(b, (umbra_ptr32){0});
    umbra_val32 base = umbra_mul_f32(b, px, umbra_imm_f32(b, 0.5f));

    struct umbra_var32 acc = umbra_var32(b);
    umbra_store_var32(b, acc, base);

    umbra_val32 n = umbra_uniform_32(b, (umbra_ptr32){.ix = 1}, 0);
    umbra_val32 i = umbra_loop(b, n); {
        umbra_val32 cur = umbra_load_var32(b, acc);
        umbra_val32 elem = umbra_gather_32(b, (umbra_ptr32){.ix = 2}, i);
        umbra_store_var32(b, acc, umbra_add_f32(b, cur, elem));
    } umbra_end_loop(b);

    umbra_val32 result = umbra_load_var32(b, acc);
    umbra_val32 doubled = umbra_mul_f32(b, result, umbra_imm_f32(b, 2.0f));
    umbra_store_32(b, (umbra_ptr32){.ix = 3}, doubled);

    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        float px_data[2] = {10.0f, 20.0f};
        int32_t uni[1] = {3};
        float data[3] = {1.0f, 2.0f, 3.0f};
        float out[2] = {0};
        if (run(&B, bi, 2, 1, (struct umbra_buf[]){
                {.ptr = px_data, .count=count(px_data), .stride=count(px_data)},
                {.ptr = uni,     .count=count(uni)},
                {.ptr = data,    .count=count(data)},
                {.ptr = out,     .count=count(out), .stride=count(out)},
            })) {
            out[0] == 22.0f here;
            out[1] == 32.0f here;
        }
    }
    cleanup(&B);
}

TEST(test_loop_sel_gather) {
    struct umbra_builder *b = umbra_builder();

    umbra_val32 n = umbra_uniform_32(b, (umbra_ptr32){0}, 0);
    umbra_val32 t = umbra_load_32(b, (umbra_ptr32){.ix = 1});

    struct umbra_var32 vr = umbra_var32(b);

    umbra_val32 i = umbra_loop(b, n); {
        umbra_val32 i1 = umbra_add_i32(b, i, umbra_imm_i32(b, 1));

        umbra_val32 lo = umbra_gather_32(b, (umbra_ptr32){.ix = 2}, i);
        umbra_val32 hi = umbra_gather_32(b, (umbra_ptr32){.ix = 2}, i1);
        umbra_val32 in_seg = umbra_and_32(b, umbra_le_f32(b, lo, t),
                                             umbra_le_f32(b, t, hi));

        umbra_val32 c0 = umbra_gather_32(b, (umbra_ptr32){.ix = 3}, i);
        umbra_val32 c1 = umbra_gather_32(b, (umbra_ptr32){.ix = 3}, i1);

        umbra_val32 frac = umbra_div_f32(b, umbra_sub_f32(b, t, lo),
                                            umbra_sub_f32(b, hi, lo));
        umbra_val32 lerped = umbra_add_f32(b, c0,
                                 umbra_mul_f32(b, umbra_sub_f32(b, c1, c0), frac));

        umbra_store_var32(b, vr, umbra_sel_32(b, in_seg, lerped, umbra_load_var32(b, vr)));
    } umbra_end_loop(b);

    umbra_store_32(b, (umbra_ptr32){.ix = 4}, umbra_load_var32(b, vr));

    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t uni[1] = {2};

        float t_data[4];
        float v0 = 0.0f, v1 = 0.25f, v2 = 0.75f, v3 = 1.0f;
        __builtin_memcpy(t_data + 0, &v0, 4);
        __builtin_memcpy(t_data + 1, &v1, 4);
        __builtin_memcpy(t_data + 2, &v2, 4);
        __builtin_memcpy(t_data + 3, &v3, 4);

        float pos[3] = {0.0f, 0.5f, 1.0f};
        float colors[3] = {1.0f, 0.0f, 0.0f};
        float out[4] = {-1, -1, -1, -1};
        if (run(&B, bi, 4, 1, (struct umbra_buf[]){
                {.ptr = uni,    .count=count(uni)},
                {.ptr = t_data, .count=count(t_data), .stride=count(t_data)},
                {.ptr = pos,    .count=count(pos)},
                {.ptr = colors, .count=count(colors)},
                {.ptr = out,    .count=count(out), .stride=count(out)},
            })) {
            equiv(out[0], 1.0f) here;
            equiv(out[1], 0.5f) here;
            equiv(out[2], 0.0f) here;
            equiv(out[3], 0.0f) here;
        }
    }
    cleanup(&B);
}

TEST(test_loop_high_register_pressure) {
    struct umbra_builder *b = umbra_builder();

    umbra_val32 n = umbra_uniform_32(b, (umbra_ptr32){0}, 0);
    umbra_val32 px = umbra_load_32(b, (umbra_ptr32){.ix = 1});

    umbra_val32 a = umbra_mul_f32(b, px, umbra_imm_f32(b, 1.0f));
    umbra_val32 c = umbra_mul_f32(b, px, umbra_imm_f32(b, 3.0f));
    umbra_val32 d = umbra_mul_f32(b, px, umbra_imm_f32(b, 4.0f));
    umbra_val32 e = umbra_mul_f32(b, px, umbra_imm_f32(b, 5.0f));
    umbra_val32 f = umbra_mul_f32(b, px, umbra_imm_f32(b, 6.0f));
    umbra_val32 g = umbra_mul_f32(b, px, umbra_imm_f32(b, 7.0f));
    umbra_val32 h = umbra_mul_f32(b, px, umbra_imm_f32(b, 8.0f));
    umbra_val32 j = umbra_mul_f32(b, px, umbra_imm_f32(b, 9.0f));
    umbra_val32 k = umbra_mul_f32(b, px, umbra_imm_f32(b, 10.0f));
    umbra_val32 l = umbra_mul_f32(b, px, umbra_imm_f32(b, 11.0f));
    umbra_val32 m = umbra_mul_f32(b, px, umbra_imm_f32(b, 12.0f));
    umbra_val32 o = umbra_mul_f32(b, px, umbra_imm_f32(b, 13.0f));
    umbra_val32 p = umbra_mul_f32(b, px, umbra_imm_f32(b, 14.0f));
    umbra_val32 q = umbra_mul_f32(b, px, umbra_imm_f32(b, 15.0f));

    struct umbra_var32 acc = umbra_var32(b);
    umbra_loop(b, n); {
        umbra_val32 cur = umbra_load_var32(b, acc);
        umbra_val32 sum = umbra_add_f32(b, a, c);
        sum = umbra_add_f32(b, sum, d);
        sum = umbra_add_f32(b, sum, e);
        sum = umbra_add_f32(b, sum, f);
        sum = umbra_add_f32(b, sum, g);
        sum = umbra_add_f32(b, sum, h);
        sum = umbra_add_f32(b, sum, j);
        sum = umbra_add_f32(b, sum, k);
        sum = umbra_add_f32(b, sum, l);
        sum = umbra_add_f32(b, sum, m);
        sum = umbra_add_f32(b, sum, o);
        sum = umbra_add_f32(b, sum, p);
        sum = umbra_add_f32(b, sum, q);
        umbra_store_var32(b, acc, umbra_add_f32(b, cur, sum));
    } umbra_end_loop(b);

    umbra_store_32(b, (umbra_ptr32){.ix = 2}, umbra_load_var32(b, acc));

    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t uni[1] = {3};
        float px_data[2] = {1.0f, 2.0f};
        float out[2] = {0};
        if (run(&B, bi, 2, 1, (struct umbra_buf[]){
                {.ptr = uni,     .count=count(uni)},
                {.ptr = px_data, .count=count(px_data), .stride=count(px_data)},
                {.ptr = out,     .count=count(out),     .stride=count(out)},
            })) {
            float const s = 1+3+4+5+6+7+8+9+10+11+12+13+14+15;
            equiv(out[0], s * 3.0f) here;
            equiv(out[1], s * 6.0f) here;
        }
    }
    cleanup(&B);
}

TEST(test_loop_var_war_hazard) {
    // Regression: store_var must not be scheduled before load_var of the same
    // variable.  The loop index is loaded twice (once for the gather address,
    // once for the increment) and the store of the incremented value must wait
    // for both loads.
    struct umbra_builder *b = umbra_builder();
    struct umbra_var32 idx = umbra_var32(b);
    struct umbra_var32 acc = umbra_var32(b);
    umbra_val32 n = umbra_uniform_32(b, (umbra_ptr32){0}, 0);
    umbra_loop(b, n); {
        umbra_val32 i  = umbra_load_var32(b, idx);
        umbra_val32 a  = umbra_load_var32(b, acc);
        umbra_val32 v  = umbra_gather_32(b, (umbra_ptr32){.ix = 1}, i);
        umbra_store_var32(b, acc, umbra_add_f32(b, a, v));
        umbra_store_var32(b, idx, umbra_add_i32(b, i, umbra_imm_i32(b, 1)));
    } umbra_end_loop(b);
    umbra_store_32(b, (umbra_ptr32){.ix = 2}, umbra_load_var32(b, acc));

    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t uni[1] = {4};
        float data[4] = {1.0f, 2.0f, 4.0f, 8.0f};
        float out[1] = {0};
        if (run(&B, bi, 1, 1, (struct umbra_buf[]){
                {.ptr = uni,  .count=count(uni)},
                {.ptr = data, .count=count(data)},
                {.ptr = out,  .count=count(out), .stride=count(out)},
            })) {
            out[0] == 15.0f here;
        }
    }
    cleanup(&B);
}

TEST(test_deref_ptr_r11_invalidation) {
    struct umbra_builder *b = umbra_builder();
    umbra_ptr16 deref = umbra_deref_ptr16(b, (umbra_ptr32){0}, 0);
    umbra_val32 cov   = umbra_f32_from_i32(b, umbra_i32_from_s16(b, umbra_load_16(b, deref)));
    umbra_val32 r, g, bl, a;
    umbra_load_8x4(b, (umbra_ptr32){.ix = 1}, &r, &g, &bl, &a);
    umbra_val32 c255 = umbra_imm_f32(b, 255.0f);
    umbra_store_8x4(b, (umbra_ptr32){.ix = 1},
                    umbra_round_i32(b, umbra_min_f32(b,
                        umbra_add_f32(b, umbra_f32_from_i32(b, r), cov), c255)),
                    umbra_round_i32(b, umbra_min_f32(b,
                        umbra_add_f32(b, umbra_f32_from_i32(b, g), cov), c255)),
                    umbra_round_i32(b, umbra_min_f32(b,
                        umbra_add_f32(b, umbra_f32_from_i32(b, bl), cov), c255)),
                    umbra_round_i32(b, umbra_min_f32(b,
                        umbra_add_f32(b, umbra_f32_from_i32(b, a), cov), c255)));

    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int16_t cov_data[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        uint32_t dst_data[8] = {0};
        dst_data[0] = 0x01020304u;
        struct umbra_buf cov_buf = {.ptr = cov_data, .count = 8, .stride = 8};
        if (run(&B, bi, 8, 1,
                 (struct umbra_buf[]){
                     {.ptr = &cov_buf, .count = 4},
                     {.ptr = dst_data, .count = 8, .stride = 8},
                 })) {
            dst_data[0] == 0x01020304u here;
        }
    }
    cleanup(&B);
}

TEST(test_if_basic) {
    struct umbra_builder *b = umbra_builder();

    struct umbra_var32 v = umbra_var32(b);
    umbra_val32 n = umbra_imm_i32(b, 3);
    umbra_val32 i = umbra_loop(b, n); {
        umbra_val32 cond = umbra_eq_i32(b, i, umbra_imm_i32(b, 1));
        umbra_if(b, cond); {
            umbra_store_var32(b, v, i);
        } umbra_end_if(b);
    } umbra_end_loop(b);

    umbra_store_32(b, (umbra_ptr32){0}, umbra_load_var32(b, v));

    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[8] = {0};
        if (run(&B, bi, 8, 1, (struct umbra_buf[]){
                {.ptr = dst, .count = 8, .stride = 8},
            })) {
            dst[0] == 1 here;
        }
    }
    cleanup(&B);
}

TEST(test_if_varying) {
    struct umbra_builder *b = umbra_builder();

    umbra_val32 x    = umbra_x(b);
    umbra_val32 cond = umbra_lt_s32(b, x, umbra_imm_i32(b, 4));
    umbra_val32 val  = umbra_imm_i32(b, 99);

    struct umbra_var32 v = umbra_var32(b);
    umbra_if(b, cond); {
        umbra_store_var32(b, v, val);
    } umbra_end_if(b);
    umbra_val32 result = umbra_load_var32(b, v);
    umbra_store_32(b, (umbra_ptr32){0}, result);

    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[8] = {0};
        if (run(&B, bi, 8, 1, (struct umbra_buf[]){
                {.ptr = dst, .count = 8, .stride = 8},
            })) {
            dst[0] == 99 here;
            dst[3] == 99 here;
            dst[4] == 0  here;
            dst[7] == 0  here;
        }
    }
    cleanup(&B);
}

TEST(test_if_nested) {
    struct umbra_builder *b = umbra_builder();

    umbra_val32 x = umbra_x(b);
    struct umbra_var32 v = umbra_var32(b);

    umbra_if(b, umbra_lt_s32(b, x, umbra_imm_i32(b, 6))); {
        umbra_if(b, umbra_lt_s32(b, x, umbra_imm_i32(b, 3))); {
            umbra_store_var32(b, v, umbra_imm_i32(b, 2));
        } umbra_end_if(b);
    } umbra_end_if(b);

    umbra_store_32(b, (umbra_ptr32){0}, umbra_load_var32(b, v));

    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t dst[8] = {0};
        if (run(&B, bi, 8, 1, (struct umbra_buf[]){
                {.ptr = dst, .count = 8, .stride = 8},
            })) {
            dst[0] == 2 here;
            dst[2] == 2 here;
            dst[3] == 0 here;
            dst[5] == 0 here;
            dst[6] == 0 here;
        }
    }
    cleanup(&B);
}

TEST(test_many_constants) {
    float const constants[] = {
        1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 1.6f, 1.7f, 1.8f, 1.9f,
        2.0f, 2.1f, 2.2f, 2.3f, 2.4f, 2.5f, 2.6f, 2.7f, 2.8f, 2.9f,
    };
    struct umbra_builder *b = umbra_builder();
    umbra_val32 x = umbra_f32_from_i32(b, umbra_x(b));
    for (int i = 0; i < 20; i++) {
        x = umbra_add_f32(b, x, umbra_imm_f32(b, constants[i]));
    }
    umbra_store_32(b, (umbra_ptr32){0}, x);
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        float dst[8] = {0};
        if (run(&B, bi, 8, 1, (struct umbra_buf[]){{.ptr=dst, .count=8, .stride=8}})) {
            equiv(dst[0], 39.0f) here;
            equiv(dst[1], 40.0f) here;
        }
    }
    cleanup(&B);
}

TEST(test_join_add_f32_imm) {
    struct umbra_builder *b = umbra_builder();
    umbra_store_32(b, (umbra_ptr32){0},
        umbra_add_f32(b, umbra_load_32(b, (umbra_ptr32){0}), umbra_imm_f32(b, 1.0f)));
    struct test_backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        float v[8] = {0};
        if (run(&B, bi, 8, 1, (struct umbra_buf[]){{.ptr=v, .count=8, .stride=8}})) {
            equiv(v[0], 1.0f) here;
        }
    }
    cleanup(&B);
}
