#include "test.h"
#include <stdint.h>
#include <stdlib.h>

static struct umbra_builder *build_srcover(void) {
    struct umbra_builder *b = umbra_builder();

    umbra_val px = umbra_load_32(b, (umbra_ptr){0, 0}), mask = umbra_imm_i32(b, 0xFF);
    umbra_val rgba[4] = {
        umbra_and_i32(b, px, mask),
        umbra_and_i32(b, umbra_shr_u32(b, px, umbra_imm_i32(b, 8)), mask),
        umbra_and_i32(b, umbra_shr_u32(b, px, umbra_imm_i32(b, 16)), mask),
        umbra_shr_u32(b, px, umbra_imm_i32(b, 24)),
    };

    umbra_val inv255 = umbra_imm_f32(b, 1.0f / 255.0f);
    umbra_val sr = umbra_mul_f32(b, umbra_f32_from_i32(b, rgba[0]), inv255),
              sg = umbra_mul_f32(b, umbra_f32_from_i32(b, rgba[1]), inv255),
              sb = umbra_mul_f32(b, umbra_f32_from_i32(b, rgba[2]), inv255),
              sa = umbra_mul_f32(b, umbra_f32_from_i32(b, rgba[3]), inv255),
              dr = umbra_f32_from_f16(b, umbra_load_16(b, (umbra_ptr){1, 0})),
              dg = umbra_f32_from_f16(b, umbra_load_16(b, (umbra_ptr){2, 0})),
              db = umbra_f32_from_f16(b, umbra_load_16(b, (umbra_ptr){3, 0})),
              da = umbra_f32_from_f16(b, umbra_load_16(b, (umbra_ptr){4, 0})),
              one = umbra_imm_f32(b, 1.0f), inv_a = umbra_sub_f32(b, one, sa),
              rout = umbra_add_f32(b, sr, umbra_mul_f32(b, dr, inv_a)),
              gout = umbra_add_f32(b, sg, umbra_mul_f32(b, dg, inv_a)),
              bout = umbra_add_f32(b, sb, umbra_mul_f32(b, db, inv_a)),
              aout = umbra_add_f32(b, sa, umbra_mul_f32(b, da, inv_a));
    umbra_store_16(b, (umbra_ptr){1, 0}, umbra_f16_from_f32(b, rout));
    umbra_store_16(b, (umbra_ptr){2, 0}, umbra_f16_from_f32(b, gout));
    umbra_store_16(b, (umbra_ptr){3, 0}, umbra_f16_from_f32(b, bout));
    umbra_store_16(b, (umbra_ptr){4, 0}, umbra_f16_from_f32(b, aout));
    return b;
}

typedef test_backends backends;

static backends make_full(struct umbra_builder *builder, _Bool opt) {
    (void)opt;
    struct umbra_basic_block *bb = umbra_basic_block(builder);
    umbra_builder_free(builder);
    backends B = test_backends_make(bb);
    umbra_basic_block_free(bb);
    return B;
}
static backends make(struct umbra_builder *builder, _Bool opt) {
    return make_full(builder, opt);
}
static _Bool run(backends *B, int b, int w, int h, umbra_buf buf[]) {
    return test_backends_run(B, b, w, h, buf);
}
static void cleanup(backends *B) { test_backends_free(B); }

#define BINOP_F32(op, B, opt)                                                        \
    do {                                                                             \
        struct umbra_builder *b_ = umbra_builder();                                  \
        umbra_val x_ = umbra_load_32(b_, (umbra_ptr){0, 0}),                      \
                  y_ = umbra_load_32(b_, (umbra_ptr){1, 0}), r_ = op(b_, x_, y_); \
        umbra_store_32(b_, (umbra_ptr){2, 0}, r_);                                \
        B = make(b_, opt);                                                           \
    } while (0)

#define BINOP_I32(op, B, opt)                                                         \
    do {                                                                              \
        struct umbra_builder *b_ = umbra_builder();                                   \
        umbra_val x_ = umbra_load_32(b_, (umbra_ptr){0, 0}),                      \
                  y_ = umbra_load_32(b_, (umbra_ptr){1, 0}), r_ = op(b_, x_, y_);  \
        umbra_store_32(b_, (umbra_ptr){2, 0}, r_);                                 \
        B = make(b_, opt);                                                            \
    } while (0)

#define BINOP_CMP_F32(op, B, opt)                                           \
    do {                                                                    \
        struct umbra_builder *b_ = umbra_builder();                         \
        umbra_val             x_ = umbra_load_32(b_, (umbra_ptr){0, 0}), \
                              y_ = umbra_load_32(b_, (umbra_ptr){1, 0}); \
        umbra_val             r_ = op(b_, x_, y_);                          \
        umbra_store_32(b_, (umbra_ptr){2, 0}, r_);                       \
        B = make(b_, opt);                                                  \
    } while (0)

static void test_f32_ops(void) {
    for (int opt = 0; opt < 2; opt++) {
        {
            backends B;
            BINOP_F32(umbra_mul_f32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                float x[] = {1, 2, 3, 4, 5};
                float y[] = {6, 7, 8, 9, 0}, z[5] = {0};
                if (!run(&B, bi, 5, 1,
                         (umbra_buf[]){
                             {x, 5 * 4, 0, 0},
                             {y, 5 * 4, 0, 0},
                             {z, 5 * 4, 0, 0},
                         })) {
                    continue;
                }
                equiv(z[0], 6) here;
                equiv(z[1], 14) here;
                equiv(z[2], 24) here;
                equiv(z[3], 36) here;
                equiv(z[4], 0) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_F32(umbra_add_f32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                float x[] = {1, 2, 3};
                float y[] = {10, 20, 30}, z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                equiv(z[0], 11) here;
                equiv(z[1], 22) here;
                equiv(z[2], 33) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_F32(umbra_sub_f32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                float x[] = {10, 20, 30};
                float y[] = {1, 2, 3}, z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                equiv(z[0], 9) here;
                equiv(z[1], 18) here;
                equiv(z[2], 27) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_F32(umbra_div_f32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                float x[] = {10, 20, 30};
                float y[] = {2, 4, 5}, z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                equiv(z[0], 5) here;
                equiv(z[1], 5) here;
                equiv(z[2], 6) here;
            }
            cleanup(&B);
        }
    }
}

static void test_i32_ops(void) {
    for (int opt = 0; opt < 2; opt++) {
        {
            backends B;
            BINOP_I32(umbra_add_i32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                int x[] = {1, 2, 3};
                int y[] = {10, 20, 30}, z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == 11) here;
                (z[1] == 22) here;
                (z[2] == 33) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_I32(umbra_sub_i32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                int x[] = {10, 20, 30};
                int y[] = {1, 2, 3}, z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == 9) here;
                (z[1] == 18) here;
                (z[2] == 27) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_I32(umbra_mul_i32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                int x[] = {2, 3, 4};
                int y[] = {5, 6, 7}, z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == 10) here;
                (z[1] == 18) here;
                (z[2] == 28) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_I32(umbra_shl_i32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                int x[] = {1, 3, 7};
                int y[] = {1, 2, 3}, z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == 2) here;
                (z[1] == 12) here;
                (z[2] == 56) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_I32(umbra_shr_u32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                int x[] = {-1, 8, 64};
                int y[] = {1, 1, 3}, z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == (int)(0xffffffffu >> 1)) here;
                (z[1] == 4) here;
                (z[2] == 8) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_I32(umbra_shr_s32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                int x[] = {-8, 8, 64};
                int y[] = {1, 1, 3}, z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == -4) here;
                (z[1] == 4) here;
                (z[2] == 8) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_I32(umbra_and_i32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                int x[] = {0xff, 0x0f};
                int y[] = {0x0f, 0xff}, z[2] = {0};
                if (!run(&B, bi, 2, 1,
                         (umbra_buf[]){
                             {x, 2 * 4, 0, 0},
                             {y, 2 * 4, 0, 0},
                             {z, 2 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == 0x0f) here;
                (z[1] == 0x0f) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_I32(umbra_or_i32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                int x[] = {0xf0, 0x0f};
                int y[] = {0x0f, 0xf0}, z[2] = {0};
                if (!run(&B, bi, 2, 1,
                         (umbra_buf[]){
                             {x, 2 * 4, 0, 0},
                             {y, 2 * 4, 0, 0},
                             {z, 2 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == 0xff) here;
                (z[1] == 0xff) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_I32(umbra_xor_i32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                int x[] = {0xff, 0xff};
                int y[] = {0x0f, 0xff}, z[2] = {0};
                if (!run(&B, bi, 2, 1,
                         (umbra_buf[]){
                             {x, 2 * 4, 0, 0},
                             {y, 2 * 4, 0, 0},
                             {z, 2 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == 0xf0) here;
                (z[1] == 0x00) here;
            }
            cleanup(&B);
        }
        {
            struct umbra_builder *builder = umbra_builder();
            umbra_val             c = umbra_load_32(builder, (umbra_ptr){0, 0}),
                                  t = umbra_load_32(builder, (umbra_ptr){1, 0}),
                                  f = umbra_load_32(builder, (umbra_ptr){2, 0}),
                                  r = umbra_sel_i32(builder, c, t, f);
            umbra_store_32(builder, (umbra_ptr){3, 0}, r);
            backends B = make(builder, opt);
            for (int bi = 0; bi < 3; bi++) {
                int cond[] = {-1, 0, -1};
                int va[] = {10, 20, 30};
                int vb[] = {40, 50, 60};
                int z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {cond, 3 * 4, 0, 0},
                             {va, 3 * 4, 0, 0},
                             {vb, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == 10) here;
                (z[1] == 50) here;
                (z[2] == 30) here;
            }
            cleanup(&B);
        }
    }
}

static void test_cmp_i32(void) {
    for (int opt = 0; opt < 2; opt++) {
        {
            backends B;
            BINOP_I32(umbra_eq_i32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                int x[] = {1, 2, 3};
                int y[] = {1, 9, 3}, z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == -1) here;
                (z[1] == 0) here;
                (z[2] == -1) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_I32(umbra_ne_i32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                int x[] = {1, 2};
                int y[] = {1, 9}, z[2] = {0};
                if (!run(&B, bi, 2, 1,
                         (umbra_buf[]){
                             {x, 2 * 4, 0, 0},
                             {y, 2 * 4, 0, 0},
                             {z, 2 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == 0) here;
                (z[1] == -1) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_I32(umbra_lt_s32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                int x[] = {1, 5, 3};
                int y[] = {2, 5, 1}, z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == -1) here;
                (z[1] == 0) here;
                (z[2] == 0) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_I32(umbra_le_s32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                int x[] = {1, 5, 3};
                int y[] = {2, 5, 1}, z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == -1) here;
                (z[1] == -1) here;
                (z[2] == 0) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_I32(umbra_gt_s32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                int x[] = {3, 5, 1};
                int y[] = {2, 5, 3}, z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == -1) here;
                (z[1] == 0) here;
                (z[2] == 0) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_I32(umbra_ge_s32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                int x[] = {3, 5, 1};
                int y[] = {2, 5, 3}, z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == -1) here;
                (z[1] == -1) here;
                (z[2] == 0) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_I32(umbra_lt_u32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                int x[] = {1, -1};
                int y[] = {2, 1}, z[2] = {0};
                if (!run(&B, bi, 2, 1,
                         (umbra_buf[]){
                             {x, 2 * 4, 0, 0},
                             {y, 2 * 4, 0, 0},
                             {z, 2 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == -1) here;
                (z[1] == 0) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_I32(umbra_le_u32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                int x[] = {1, 2, -1};
                int y[] = {2, 2, 1}, z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == -1) here;
                (z[1] == -1) here;
                (z[2] == 0) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_I32(umbra_gt_u32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                int x[] = {2, -1, 1};
                int y[] = {1, 1, 2}, z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == -1) here;
                (z[1] == -1) here;
                (z[2] == 0) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_I32(umbra_ge_u32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                int x[] = {2, 2, 1};
                int y[] = {1, 2, -1}, z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == -1) here;
                (z[1] == -1) here;
                (z[2] == 0) here;
            }
            cleanup(&B);
        }
    }
}

static void test_cmp_f32(void) {
    for (int opt = 0; opt < 2; opt++) {
        {
            backends B;
            BINOP_CMP_F32(umbra_eq_f32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                float x[] = {1, 2, 3};
                float y[] = {1, 9, 3};
                int   z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == -1) here;
                (z[1] == 0) here;
                (z[2] == -1) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_CMP_F32(umbra_ne_f32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                float x[] = {1, 2};
                float y[] = {1, 9};
                int   z[2] = {0};
                if (!run(&B, bi, 2, 1,
                         (umbra_buf[]){
                             {x, 2 * 4, 0, 0},
                             {y, 2 * 4, 0, 0},
                             {z, 2 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == 0) here;
                (z[1] == -1) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_CMP_F32(umbra_lt_f32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                float x[] = {1, 5, 3};
                float y[] = {2, 5, 1};
                int   z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == -1) here;
                (z[1] == 0) here;
                (z[2] == 0) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_CMP_F32(umbra_le_f32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                float x[] = {1, 5, 3};
                float y[] = {2, 5, 1};
                int   z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == -1) here;
                (z[1] == -1) here;
                (z[2] == 0) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_CMP_F32(umbra_gt_f32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                float x[] = {3, 5, 1};
                float y[] = {2, 5, 3};
                int   z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == -1) here;
                (z[1] == 0) here;
                (z[2] == 0) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_CMP_F32(umbra_ge_f32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                float x[] = {3, 5, 1};
                float y[] = {2, 5, 3};
                int   z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == -1) here;
                (z[1] == -1) here;
                (z[2] == 0) here;
            }
            cleanup(&B);
        }
    }
}

static void test_imm(void) {
    for (int opt = 0; opt < 2; opt++) {
        {
            struct umbra_builder *builder = umbra_builder();
            umbra_val v = umbra_imm_i32(builder, 42);
            umbra_store_32(builder, (umbra_ptr){0, 0}, v);
            backends B = make(builder, opt);
            for (int bi = 0; bi < 3; bi++) {
                int z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == 42) here;
                (z[1] == 42) here;
                (z[2] == 42) here;
            }
            cleanup(&B);
        }
    }
}

static void test_fma_f32(void) {
    for (int opt = 0; opt < 2; opt++) {
        struct umbra_builder *builder = umbra_builder();
        umbra_val x = umbra_load_32(builder, (umbra_ptr){0, 0}),
                  y = umbra_load_32(builder, (umbra_ptr){1, 0}),
                  w = umbra_load_32(builder, (umbra_ptr){2, 0}),
                  m = umbra_mul_f32(builder, x, y), r = umbra_add_f32(builder, m, w);
        umbra_store_32(builder, (umbra_ptr){3, 0}, r);
        backends B = make(builder, opt);
        for (int bi = 0; bi < 3; bi++) {
            float a[] = {2, 3}, c[] = {4, 5};
            float d[] = {10, 20}, z[2] = {0};
            if (!run(&B, bi, 2, 1,
                     (umbra_buf[]){
                         {a, 2 * 4, 0, 0},
                         {c, 2 * 4, 0, 0},
                         {d, 2 * 4, 0, 0},
                         {z, 2 * 4, 0, 0},
                     })) {
                continue;
            }
            equiv(z[0], 18) here;
            equiv(z[1], 35) here;
        }
        cleanup(&B);
    }
}

static void test_min_max_sqrt_f32(void) {
    for (int opt = 0; opt < 2; opt++) {
        {
            backends B;
            BINOP_F32(umbra_min_f32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                float x[] = {5, 1, 3};
                float y[] = {2, 4, 3}, z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                equiv(z[0], 2) here;
                equiv(z[1], 1) here;
                equiv(z[2], 3) here;
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_F32(umbra_max_f32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                float x[] = {5, 1, 3};
                float y[] = {2, 4, 3}, z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                equiv(z[0], 5) here;
                equiv(z[1], 4) here;
                equiv(z[2], 3) here;
            }
            cleanup(&B);
        }
        {
            struct umbra_builder *builder = umbra_builder();
            umbra_val             x = umbra_load_32(builder, (umbra_ptr){0, 0}),
                                  r = umbra_sqrt_f32(builder, x);
            umbra_store_32(builder, (umbra_ptr){1, 0}, r);
            backends B = make(builder, opt);
            for (int bi = 0; bi < 3; bi++) {
                float a[] = {4, 9, 16}, z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {a, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                equiv(z[0], 2) here;
                equiv(z[1], 3) here;
                equiv(z[2], 4) here;
            }
            cleanup(&B);
        }
    }
}

static void test_abs_sign_f32(void) {
    for (int opt = 0; opt < 2; opt++) {
        {
            struct umbra_builder *builder = umbra_builder();
            umbra_val             x = umbra_load_32(builder, (umbra_ptr){0, 0}),
                                  r = umbra_abs_f32(builder, x);
            umbra_store_32(builder, (umbra_ptr){1, 0}, r);
            backends B = make(builder, opt);
            for (int bi = 0; bi < 3; bi++) {
                float a[] = {-1.5f, 2.5f, -0.0f};
                float z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {a, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                equiv(z[0], 1.5f) here;
                equiv(z[1], 2.5f) here;
                equiv(z[2], 0.0f) here;
            }
            cleanup(&B);
        }
        {
            struct umbra_builder *builder = umbra_builder();
            umbra_val             x = umbra_load_32(builder, (umbra_ptr){0, 0}),
                                  r = umbra_sign_f32(builder, x);
            umbra_store_32(builder, (umbra_ptr){1, 0}, r);
            backends B = make(builder, opt);
            for (int bi = 0; bi < 3; bi++) {
                float a[] = {-3.0f, 7.0f, 0.0f};
                float z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {a, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                equiv(z[0], -1.0f) here;
                equiv(z[1], 1.0f) here;
                equiv(z[2], 0.0f) here;
            }
            cleanup(&B);
        }
        {
            struct umbra_builder *builder = umbra_builder();
            umbra_val             x = umbra_load_32(builder, (umbra_ptr){0, 0}),
                                  r = umbra_neg_f32(builder, x);
            umbra_store_32(builder, (umbra_ptr){1, 0}, r);
            backends B = make(builder, opt);
            for (int bi = 0; bi < 3; bi++) {
                float a[] = {-1.5f, 2.5f, 0.0f};
                float z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {a, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                equiv(z[0], 1.5f) here;
                equiv(z[1], -2.5f) here;
                equiv(z[2], 0.0f) here;
            }
            cleanup(&B);
        }
    }
}

static void test_round_floor_ceil(void) {
    float src[] = {1.3f, 1.5f, -1.5f, -2.7f};

#define RFC(op, e0, e1, e2, e3, as_int)                                     \
    do {                                                                    \
        struct umbra_builder *b_ = umbra_builder();                         \
        umbra_val             x_ = umbra_load_32(b_, (umbra_ptr){0, 0}); \
        umbra_val             r_ = op(b_, x_);                              \
        umbra_store_32(b_, (umbra_ptr){1, 0}, r_);                       \
        backends B_ = make(b_, 0);                                          \
        for (int bi_ = 0; bi_ < 3; bi_++) {                                 \
            float s_[4];                                                    \
            __builtin_memcpy(s_, src, 16);                                  \
            int d_[4] = {0};                                                \
            if (!run(&B_, bi_, 4, 1,                                        \
                     (umbra_buf[]){                                         \
                         {s_, 16, 0, 0},                                          \
                         {d_, 16, 0, 0},                                          \
                     })) {                                                  \
                continue;                                                   \
            }                                                               \
            if (as_int) {                                                   \
                (d_[0] == (int)(e0)) here;                                  \
                (d_[1] == (int)(e1)) here;                                  \
                (d_[2] == (int)(e2)) here;                                  \
                (d_[3] == (int)(e3)) here;                                  \
            } else {                                                        \
                float g0, g1, g2, g3;                                       \
                __builtin_memcpy(&g0, d_ + 0, 4);                           \
                __builtin_memcpy(&g1, d_ + 1, 4);                           \
                __builtin_memcpy(&g2, d_ + 2, 4);                           \
                __builtin_memcpy(&g3, d_ + 3, 4);                           \
                equiv(g0, (float)(e0)) here;                                \
                equiv(g1, (float)(e1)) here;                                \
                equiv(g2, (float)(e2)) here;                                \
                equiv(g3, (float)(e3)) here;                                \
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

static void test_large_n(void) {
    for (int opt = 0; opt < 2; opt++) {
        backends B;
        BINOP_F32(umbra_add_f32, B, opt);
        for (int bi = 0; bi < 3; bi++) {
            float x[100], y[100], z[100];
            for (int i = 0; i < 100; i++) {
                x[i] = (float)i;
                y[i] = (float)(100 - i);
            }
            if (!run(&B, bi, 100, 1,
                     (umbra_buf[]){
                         {x, 100 * 4, 0, 0},
                         {y, 100 * 4, 0, 0},
                         {z, 100 * 4, 0, 0},
                     })) {
                continue;
            }
            for (int i = 0; i < 100; i++) { equiv(z[i], 100) here; }
        }
        cleanup(&B);
    }
}

static void test_convert(void) {
    for (int opt = 0; opt < 2; opt++) {
        {
            struct umbra_builder *builder = umbra_builder();
            umbra_val             x = umbra_load_32(builder, (umbra_ptr){0, 0});
            umbra_val             r = umbra_f32_from_i32(builder, x);
            umbra_store_32(builder, (umbra_ptr){1, 0}, r);
            backends B = make(builder, opt);
            for (int bi = 0; bi < 3; bi++) {
                int   a[] = {1, 255, -3};
                float z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {a, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                equiv(z[0], 1) here;
                equiv(z[1], 255) here;
                equiv(z[2], -3) here;
            }
            cleanup(&B);
        }
        {
            struct umbra_builder *builder = umbra_builder();
            umbra_val             x = umbra_load_32(builder, (umbra_ptr){0, 0});
            umbra_val             r = umbra_i32_from_f32(builder, x);
            umbra_store_32(builder, (umbra_ptr){1, 0}, r);
            backends B = make(builder, opt);
            for (int bi = 0; bi < 3; bi++) {
                float a[] = {1.9f, 255.0f, -3.7f};
                int   z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {a, 3 * 4, 0, 0},
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == 1) here;
                (z[1] == 255) here;
                (z[2] == -3) here;
            }
            cleanup(&B);
        }
    }
}

static void test_dedup(void) {
    struct umbra_builder *builder = umbra_builder();
    umbra_val v1 = umbra_imm_i32(builder, 42), v2 = umbra_imm_i32(builder, 42);
    (v1.id == v2.id) here;
    umbra_val c = umbra_imm_i32(builder, 99);
    (v1.id != c.id) here;
    umbra_val x = umbra_load_32(builder, (umbra_ptr){0, 0}),
              s1 = umbra_add_i32(builder, x, v1), s2 = umbra_add_i32(builder, x, v1);
    (s1.id == s2.id) here;
    umbra_builder_free(builder);
}

static void test_constprop(void) {
    for (int opt = 0; opt < 2; opt++) {
        {
            struct umbra_builder *builder = umbra_builder();
            umbra_val x = umbra_imm_i32(builder, 3),
                      y = umbra_imm_i32(builder, 5), s = umbra_add_i32(builder, x, y);
            umbra_store_32(builder, (umbra_ptr){0, 0}, s);
            backends B = make(builder, opt);
            for (int bi = 0; bi < 3; bi++) {
                int z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (z[0] == 8) here;
                (z[1] == 8) here;
                (z[2] == 8) here;
            }
            cleanup(&B);
        }
        {
            struct umbra_builder *builder = umbra_builder();
            umbra_val a = umbra_imm_f32(builder, 2.0f), y = umbra_imm_f32(builder, 3.0f),
                      s = umbra_mul_f32(builder, a, y);
            umbra_store_32(builder, (umbra_ptr){0, 0}, s);
            backends B = make(builder, opt);
            for (int bi = 0; bi < 3; bi++) {
                float z[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {z, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                equiv(z[0], 6) here;
                equiv(z[1], 6) here;
                equiv(z[2], 6) here;
            }
            cleanup(&B);
        }
    }
}

static void test_strength_reduction(void) {
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val             x = umbra_load_32(builder, (umbra_ptr){0, 0}),
                  z = umbra_imm_i32(builder, 0), s = umbra_add_i32(builder, x, z);
        (s.id == x.id) here;
        umbra_builder_free(builder);
    }
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val             x = umbra_load_32(builder, (umbra_ptr){0, 0}),
                  one = umbra_imm_i32(builder, 1), s = umbra_mul_i32(builder, x, one);
        (s.id == x.id) here;
        umbra_builder_free(builder);
    }
    for (int opt = 0; opt < 2; opt++) {
        {
            struct umbra_builder *builder = umbra_builder();
            umbra_val             x = umbra_load_32(builder, (umbra_ptr){0, 0}),
                                  eight = umbra_imm_i32(builder, 8),
                                  s = umbra_mul_i32(builder, x, eight);
            umbra_store_32(builder, (umbra_ptr){1, 0}, s);
            backends B = make(builder, opt);
            for (int bi = 0; bi < 3; bi++) {
                int32_t a[] = {1, 2, 3, 4, 5}, c[5] = {0};
                if (!run(&B, bi, 5, 1,
                         (umbra_buf[]){
                             {a, 5 * 4, 0, 0},
                             {c, 5 * 4, 0, 0},
                         })) {
                    continue;
                }
                for (int i = 0; i < 5; i++) { (c[i] == a[i] * 8) here; }
            }
            cleanup(&B);
        }
        {
            struct umbra_builder *builder = umbra_builder();
            umbra_val             x = umbra_load_32(builder, (umbra_ptr){0, 0}),
                                  s = umbra_sub_i32(builder, x, x);
            umbra_store_32(builder, (umbra_ptr){1, 0}, s);
            backends B = make(builder, opt);
            for (int bi = 0; bi < 3; bi++) {
                int32_t a[] = {1, 2, 3}, c[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {a, 3 * 4, 0, 0},
                             {c, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                for (int i = 0; i < 3; i++) { (c[i] == 0) here; }
            }
            cleanup(&B);
        }
    }
}

static void test_zero_imm(void) {
    for (int opt = 0; opt < 2; opt++) {
        struct umbra_builder *builder = umbra_builder();
        umbra_val             zero = umbra_imm_i32(builder, 0);
        (zero.id == 0) here;
        umbra_val             x = umbra_load_32(builder, (umbra_ptr){0, 0}),
                  r = umbra_eq_i32(builder, x, zero);
        umbra_store_32(builder, (umbra_ptr){1, 0}, r);
        backends B = make(builder, opt);
        for (int bi = 0; bi < 3; bi++) {
            int a[] = {0, 1, 0}, z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {a, 3 * 4, 0, 0},
                         {z, 3 * 4, 0, 0},
                     })) {
                continue;
            }
            (z[0] == -1) here;
            (z[1] == 0) here;
            (z[2] == -1) here;
        }
        cleanup(&B);
    }
}

static void test_late_imm_identity(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_val             z1 = umbra_imm_i32(b, 1);
    umbra_val             zm = umbra_imm_i32(b, -1);
    (z1.id > x.id) here;
    (zm.id > x.id) here;

    (umbra_mul_i32(b, x, z1).id == x.id) here;
    (umbra_and_i32(b, x, zm).id == x.id) here;
    (umbra_or_i32(b, x, zm).id == zm.id) here;
    (umbra_sel_i32(b, zm, x, z1).id == x.id) here;
    (umbra_eq_i32(b, x, x).id == umbra_imm_i32(b, -1).id) here;
    (umbra_div_f32(b, x, umbra_imm_f32(b, 1.0f)).id == x.id) here;
    (umbra_sub_f32(b, x, umbra_imm_f32(b, 0.0f)).id == x.id) here;
    umbra_builder_free(b);
}

static void test_abs_peepholes(void) {
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             direct = umbra_abs_f32(b, x);
        umbra_val             neg_x = umbra_neg_f32(b, x);
        umbra_val             mask = umbra_imm_i32(b, 0x7fffffff);

        (umbra_max_f32(b, x, neg_x).id == direct.id) here;
        (umbra_max_f32(b, neg_x, x).id == direct.id) here;
        (umbra_and_i32(b, x, mask).id == direct.id) here;
        (umbra_and_i32(b, mask, x).id == direct.id) here;
        umbra_builder_free(b);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             mask = umbra_imm_i32(b, 0x7fffffff);
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        (mask.id < x.id) here;
        umbra_val direct = umbra_abs_f32(b, x);
        (umbra_and_i32(b, x, mask).id == direct.id) here;
        umbra_builder_free(b);
    }
}

static void test_load_8x4(void) {
    for (int opt = 0; opt < 2; opt++) {
        struct umbra_builder *builder = umbra_builder();
        umbra_val             px_ = umbra_load_32(builder, (umbra_ptr){0, 0}),
                              m_ = umbra_imm_i32(builder, 0xFF);
        umbra_val r = umbra_and_i32(builder, px_, m_),
                  g = umbra_and_i32(builder,
                                    umbra_shr_u32(builder, px_, umbra_imm_i32(builder, 8)),
                                    m_),
                  b = umbra_and_i32(builder,
                                    umbra_shr_u32(builder, px_, umbra_imm_i32(builder, 16)),
                                    m_),
                  a = umbra_shr_u32(builder, px_, umbra_imm_i32(builder, 24));
        umbra_store_32(builder, (umbra_ptr){1, 0}, r);
        umbra_store_32(builder, (umbra_ptr){2, 0}, g);
        umbra_store_32(builder, (umbra_ptr){3, 0}, b);
        umbra_store_32(builder, (umbra_ptr){4, 0}, a);
        backends B = make(builder, opt);
        for (int bi = 0; bi < 3; bi++) {
            uint32_t src[] = {
                0xAABBCCDD,
                0x11223344,
                0xFF00FF00,
            };
            int32_t rr[3] = {0}, gg[3] = {0};
            int32_t b_[3] = {0}, aa[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {src, 3 * 4, 0, 0},
                         {rr, 3 * 4, 0, 0},
                         {gg, 3 * 4, 0, 0},
                         {b_, 3 * 4, 0, 0},
                         {aa, 3 * 4, 0, 0},
                     })) {
                continue;
            }
            (rr[0] == 0xDD) here;
            (gg[0] == 0xCC) here;
            (b_[0] == 0xBB) here;
            (aa[0] == 0xAA) here;
            (rr[1] == 0x44) here;
            (gg[1] == 0x33) here;
            (b_[1] == 0x22) here;
            (aa[1] == 0x11) here;
            (rr[2] == 0x00) here;
            (gg[2] == 0xFF) here;
            (b_[2] == 0x00) here;
            (aa[2] == 0xFF) here;
        }
        cleanup(&B);
    }
}

static void test_store_8x4(void) {
    for (int opt = 0; opt < 2; opt++) {
        struct umbra_builder *builder = umbra_builder();
        umbra_val             r = umbra_load_32(builder, (umbra_ptr){0, 0}),
                              g = umbra_load_32(builder, (umbra_ptr){1, 0}),
                              b = umbra_load_32(builder, (umbra_ptr){2, 0}),
                              a = umbra_load_32(builder, (umbra_ptr){3, 0});
        umbra_val             m_ = umbra_imm_i32(builder, 0xFF);
        umbra_val             px_ = umbra_and_i32(builder, r, m_);
        px_ = umbra_or_i32(builder, px_,
                           umbra_shl_i32(builder, umbra_and_i32(builder, g, m_),
                                         umbra_imm_i32(builder, 8)));
        px_ = umbra_or_i32(builder, px_,
                           umbra_shl_i32(builder, umbra_and_i32(builder, b, m_),
                                         umbra_imm_i32(builder, 16)));
        px_ = umbra_or_i32(builder, px_,
                           umbra_shl_i32(builder, a, umbra_imm_i32(builder, 24)));
        umbra_store_32(builder, (umbra_ptr){4, 0}, px_);
        backends B = make(builder, opt);
        for (int bi = 0; bi < 3; bi++) {
            int32_t  rr[] = {0xDD, 0x44, 0x00};
            int32_t  gg[] = {0xCC, 0x33, 0xFF};
            int32_t  b_[] = {0xBB, 0x22, 0x00};
            int32_t  aa[] = {0xAA, 0x11, 0xFF};
            uint32_t dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {rr, 3 * 4, 0, 0},
                         {gg, 3 * 4, 0, 0},
                         {b_, 3 * 4, 0, 0},
                         {aa, 3 * 4, 0, 0},
                         {dst, 3 * 4, 0, 0},
                     })) {
                continue;
            }
            (dst[0] == 0xAABBCCDDu) here;
            (dst[1] == 0x11223344u) here;
            (dst[2] == 0xFF00FF00u) here;
        }
        cleanup(&B);
    }
}

static void test_srcover(void) {
    for (int opt = 0; opt < 2; opt++) {
        struct umbra_builder *builder = build_srcover();
        backends              B = make_full(builder, opt);
        float const           tol = 0;
        for (int bi = 0; bi < 3; bi++) {
            uint32_t src_px[] = {
                0x80402010u,
                0x80402010u,
                0x80402010u,
            };
            __fp16 dst_r[] = {0.5, 0.5, 0.5}, dst_g[] = {0.5, 0.5, 0.5},
                   dst_b[] = {0.5, 0.5, 0.5}, dst_a[] = {0.5, 0.5, 0.5};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {src_px, 3 * 4, 0, 0},
                         {dst_r, 3 * 2, 0, 0},
                         {dst_g, 3 * 2, 0, 0},
                         {dst_b, 3 * 2, 0, 0},
                         {dst_a, 3 * 2, 0, 0},
                     })) {
                continue;
            }
            ((float)dst_r[0] > 0.28f - tol && (float)dst_r[0] < 0.34f + tol) here;
        }
        cleanup(&B);
    }
}

static void test_hash_quality(void) {
    struct umbra_builder *builder = umbra_builder();
    enum { N = 1000 };
    int ids[N];
    for (int i = 0; i < N; i++) { ids[i] = umbra_imm_i32(builder, i).id; }
    for (int i = 0; i < N; i++) { (umbra_imm_i32(builder, i).id == ids[i]) here; }
    for (int i = 1; i < N; i++) { (ids[i] != ids[i - 1]) here; }
    umbra_val x = umbra_load_32(builder, (umbra_ptr){0, 0});
    for (int i = 0; i < N; i++) {
        umbra_val c = umbra_imm_i32(builder, i);
        umbra_val sum = umbra_add_i32(builder, x, c);
        umbra_val sum2 = umbra_add_i32(builder, x, c);
        (sum.id == sum2.id) here;
    }
    umbra_builder_free(builder);
}

static void test_narrow_16(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val             v = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_store_16(b, (umbra_ptr){1, 0}, umbra_i16_from_i32(b, v));
    backends B = make(b, 0);
    for (int bi = 0; bi < 3; bi++) {
        int      src[] = {0x00010002, 0x00030004, 0x00050006};
        uint16_t dst[3] = {0};
        if (!run(&B, bi, 3, 1,
                 (umbra_buf[]){
                     {src, 3 * 4, 0, 0},
                     {dst, 3 * 2, 0, 0},
                 })) {
            continue;
        }
        (dst[0] == 0x0002) here;
        (dst[1] == 0x0004) here;
        (dst[2] == 0x0006) here;
    }
    cleanup(&B);
}

static void test_mixed_ptr_sizes(void) {
    for (int opt = 0; opt < 2; opt++) {
        {
            struct umbra_builder *builder = umbra_builder();
            umbra_val             a = umbra_load_32(builder, (umbra_ptr){0, 0});
            umbra_val r = umbra_add_i32(builder, a, umbra_imm_i32(builder, 1));
            umbra_store_32(builder, (umbra_ptr){1, 0}, r);
            backends B = make(builder, opt);
            for (int bi = 0; bi < 3; bi++) {
                uint32_t x[] = {10, 20, 30};
                uint32_t y[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 4, 0, 0},
                             {y, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (y[0] == 11) here;
                (y[1] == 21) here;
                (y[2] == 31) here;
            }
            cleanup(&B);
        }
        {
            struct umbra_builder *builder = umbra_builder();
            umbra_val a = umbra_f32_from_f16(builder,
                                          umbra_load_16(builder, (umbra_ptr){0, 0}));
            umbra_val r = umbra_add_f32(builder, a, umbra_imm_f32(builder, 1.0f));
            umbra_store_16(builder, (umbra_ptr){1, 0}, umbra_f16_from_f32(builder, r));
            backends B = make(builder, opt);
            for (int bi = 0; bi < 3; bi++) {
                __fp16 x[] = {1, 2, 3}, y[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {x, 3 * 2, 0, 0},
                             {y, 3 * 2, 0, 0},
                         })) {
                    continue;
                }
                equiv((float)y[0], 2) here;
                equiv((float)y[1], 3) here;
                equiv((float)y[2], 4) here;
            }
            cleanup(&B);
        }
    }
}

static void test_n9(void) {
    for (int opt = 0; opt < 2; opt++) {
        {
            backends B;
            BINOP_F32(umbra_add_f32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                float x[9], y[9], z[9] = {0};
                for (int i = 0; i < 9; i++) {
                    x[i] = (float)(i + 1);
                    y[i] = (float)(10 * (i + 1));
                }
                if (!run(&B, bi, 9, 1,
                         (umbra_buf[]){
                             {x, 9 * 4, 0, 0},
                             {y, 9 * 4, 0, 0},
                             {z, 9 * 4, 0, 0},
                         })) {
                    continue;
                }
                for (int i = 0; i < 9; i++) { equiv(z[i], x[i] + y[i]) here; }
            }
            cleanup(&B);
        }
        {
            backends B;
            BINOP_I32(umbra_mul_i32, B, opt);
            for (int bi = 0; bi < 3; bi++) {
                int x[9], y[9], z[9] = {0};
                for (int i = 0; i < 9; i++) {
                    x[i] = i + 1;
                    y[i] = i + 2;
                }
                if (!run(&B, bi, 9, 1,
                         (umbra_buf[]){
                             {x, 9 * 4, 0, 0},
                             {y, 9 * 4, 0, 0},
                             {z, 9 * 4, 0, 0},
                         })) {
                    continue;
                }
                for (int i = 0; i < 9; i++) { (z[i] == x[i] * y[i]) here; }
            }
            cleanup(&B);
        }
        {
            struct umbra_builder *builder = umbra_builder();
            umbra_val             px_ = umbra_load_32(builder, (umbra_ptr){0, 0}),
                                  m_ = umbra_imm_i32(builder, 0xFF);
            umbra_val             ch[4] = {
                umbra_and_i32(builder, px_, m_),
                umbra_and_i32(builder,
                              umbra_shr_u32(builder, px_, umbra_imm_i32(builder, 8)), m_),
                umbra_and_i32(builder,
                              umbra_shr_u32(builder, px_, umbra_imm_i32(builder, 16)), m_),
                umbra_shr_u32(builder, px_, umbra_imm_i32(builder, 24)),
            };
            umbra_val spx = umbra_and_i32(builder, ch[0], m_);
            spx = umbra_or_i32(builder, spx,
                               umbra_shl_i32(builder, umbra_and_i32(builder, ch[1], m_),
                                             umbra_imm_i32(builder, 8)));
            spx = umbra_or_i32(builder, spx,
                               umbra_shl_i32(builder, umbra_and_i32(builder, ch[2], m_),
                                             umbra_imm_i32(builder, 16)));
            spx = umbra_or_i32(builder, spx,
                               umbra_shl_i32(builder, ch[3], umbra_imm_i32(builder, 24)));
            umbra_store_32(builder, (umbra_ptr){1, 0}, spx);
            backends B = make(builder, opt);
            for (int bi = 0; bi < 3; bi++) {
                uint32_t src[9], dst[9] = {0};
                for (int i = 0; i < 9; i++) { src[i] = 0xAABBCC00u + (uint32_t)i; }
                if (!run(&B, bi, 9, 1,
                         (umbra_buf[]){
                             {src, 9 * 4, 0, 0},
                             {dst, 9 * 4, 0, 0},
                         })) {
                    continue;
                }
                for (int i = 0; i < 9; i++) { (dst[i] == src[i]) here; }
            }
            cleanup(&B);
        }
    }
}

static void test_preamble_pair_alias(void) {
    for (int opt = 0; opt < 2; opt++) {
        struct umbra_builder *builder = umbra_builder();

        enum { N_PRE = 24 };
        umbra_val pre[N_PRE];
        for (int i = 0; i < N_PRE; i++) {
            float fv_ = (float)(i + 1);
            pre[i] = umbra_imm_i32(builder,
                                   ((union {
                                       float f;
                                       int   i;
                                   }){.f = fv_})
                                       .i);
        }

        umbra_val x = umbra_load_32(builder, (umbra_ptr){0, 0});

        umbra_val sum = umbra_mul_f32(builder, x, pre[0]);
        for (int i = 1; i < N_PRE; i++) {
            sum = umbra_add_f32(builder, sum, umbra_mul_f32(builder, x, pre[i]));
        }

        umbra_store_32(builder, (umbra_ptr){1, 0}, sum);
        backends B = make(builder, opt);

        for (int bi = 0; bi < 3; bi++) {
            float in[16], out[16] = {0};
            for (int i = 0; i < 16; i++) { in[i] = (float)(i + 1); }
            if (!run(&B, bi, 16, 1,
                     (umbra_buf[]){
                         {in, 16 * 4, 0, 0},
                         {out, 16 * 4, 0, 0},
                     })) {
                continue;
            }
            float ref[16];
            umbra_program_queue(B.p[0], 0, 0, 16, 1,
                                (umbra_buf[]){
                                    {in, 16 * 4, 0, 0},
                                    {ref, 16 * 4, 0, 0},
                                });
            for (int i = 0; i < 16; i++) { equiv(out[i], ref[i]) here; }
        }
        cleanup(&B);
    }
}

static void test_gather_clamp(void) {
    for (int opt = 0; opt < 2; opt++) {
        {
            struct umbra_builder *builder = umbra_builder();
            umbra_val             idx = umbra_load_32(builder, (umbra_ptr){0, 0}),
                                  val = umbra_gather_32(builder, (umbra_ptr){1, 0}, idx);
            umbra_store_32(builder, (umbra_ptr){2, 0}, val);
            backends B = make_full(builder, opt);
            for (int bi = 0; bi < 3; bi++) {
                int32_t indices[4] = {-5, 0, 2, 100};
                int32_t src[3] = {10, 20, 30};
                int32_t dst[4] = {0};
                if (!run(&B, bi, 4, 1,
                         (umbra_buf[]){
                             {indices, sizeof indices, 0, 0},
                             {src, sizeof src, 0, 0},
                             {dst, sizeof dst, 0, 0},
                         })) {
                    continue;
                }
                (dst[0] == 0) here;
                (dst[1] == 10) here;
                (dst[2] == 30) here;
                (dst[3] == 0) here;
            }
            cleanup(&B);
        }
        {
            struct umbra_builder *builder = umbra_builder();
            umbra_val             idx = umbra_load_32(builder, (umbra_ptr){0, 0});
            umbra_val val = umbra_i32_from_s16(builder,
                                            umbra_gather_16(builder, (umbra_ptr){1, 0}, idx));
            umbra_store_32(builder, (umbra_ptr){2, 0}, val);
            backends B = make_full(builder, opt);
            for (int bi = 0; bi < 3; bi++) {
                int32_t indices[4] = {-1, 1, 3, 999};
                int16_t src[3] = {100, 200, 300};
                int32_t dst[4] = {0};
                if (!run(&B, bi, 4, 1,
                         (umbra_buf[]){
                             {indices, sizeof indices, 0, 0},
                             {src, sizeof src, 0, 0},
                             {dst, sizeof dst, 0, 0},
                         })) {
                    continue;
                }
                (dst[0] == 0) here;
                (dst[1] == 200) here;
                (dst[2] == 0) here;
                (dst[3] == 0) here;
            }
            cleanup(&B);
        }
    }
}

static void test_gather_clamp_zero_sz(void) {
    // gather_uniform with negative index → clamped to 0.
    struct umbra_builder *b = umbra_builder();
    umbra_val             ix = umbra_uniform_32(b, (umbra_ptr){0, 0}, 0);
    umbra_val             v = umbra_gather_32(b, (umbra_ptr){1, 0}, ix);
    umbra_store_32(b, (umbra_ptr){2, 0}, v);
    backends B = make(b, 0);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t neg_idx[] = {-10};
        int32_t src[3] = {100, 200, 300};
        int32_t dst[1] = {0};
        if (!run(&B, bi, 1, 1, (umbra_buf[]){
            {neg_idx, 4, 0, 0},
            {src, sizeof src, 0, 0},
            {dst, 4, 0, 0},
        })) { continue; }
        (dst[0] == 0) here;
    }

    // gather_uniform with over-range index → OOB returns 0.
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t big_idx[] = {999};
        int32_t src[3] = {100, 200, 300};
        int32_t dst[1] = {0};
        if (!run(&B, bi, 1, 1, (umbra_buf[]){
            {big_idx, 4, 0, 0},
            {src, sizeof src, 0, 0},
            {dst, 4, 0, 0},
        })) { continue; }
        (dst[0] == 0) here;
    }
    cleanup(&B);
}

static void test_offset_load_store(void) {
    for (int opt = 0; opt < 2; opt++) {
        {
            struct umbra_builder *builder = umbra_builder();
            umbra_val             ix = umbra_x(builder);
            umbra_val             off = umbra_uniform_32(builder, (umbra_ptr){1, 0}, 0);
            umbra_val             ixo = umbra_add_i32(builder, ix, off);
            umbra_val val = umbra_i32_from_s16(builder,
                                            umbra_gather_16(builder, (umbra_ptr){0, 0}, ixo));
            umbra_store_32(builder, (umbra_ptr){2, 0}, val);
            backends B = make_full(builder, opt);
            for (int bi = 0; bi < 3; bi++) {
                int16_t src[16] = {
                    10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                };
                int32_t uni[1] = {4};
                int32_t dst[8] = {0};
                if (!run(&B, bi, 8, 1,
                         (umbra_buf[]){
                             {src, sizeof src, 0, 0},
                             {uni, sizeof uni, 1, 0},
                             {dst, sizeof dst, 0, 0},
                         })) {
                    continue;
                }
                for (int k = 0; k < 8; k++) { (dst[k] == 14 + k) here; }
            }
            cleanup(&B);
        }
        {
            struct umbra_builder *builder = umbra_builder();
            umbra_val             ix = umbra_x(builder);
            umbra_val             off = umbra_uniform_32(builder, (umbra_ptr){1, 0}, 0);
            umbra_val             ixo = umbra_add_i32(builder, ix, off);
            umbra_val             val = umbra_gather_32(builder, (umbra_ptr){0, 0}, ixo);
            umbra_store_32(builder, (umbra_ptr){2, 0}, val);
            backends B = make_full(builder, opt);
            for (int bi = 0; bi < 3; bi++) {
                int32_t src[16] = {
                    100, 101, 102, 103, 104, 105, 106, 107,
                    108, 109, 110, 111, 112, 113, 114, 115,
                };
                int32_t uni[1] = {3};
                int32_t dst[8] = {0};
                if (!run(&B, bi, 8, 1,
                         (umbra_buf[]){
                             {src, sizeof src, 0, 0},
                             {uni, sizeof uni, 1, 0},
                             {dst, sizeof dst, 0, 0},
                         })) {
                    continue;
                }
                for (int k = 0; k < 8; k++) { (dst[k] == 103 + k) here; }
            }
            cleanup(&B);
        }
        {
            struct umbra_builder *builder = umbra_builder();
            umbra_val             ix = umbra_x(builder);
            umbra_val             off = umbra_uniform_32(builder, (umbra_ptr){1, 0}, 0);
            umbra_val             ixo = umbra_add_i32(builder, ix, off);
            umbra_val val = umbra_f32_from_f16(builder,
                                            umbra_gather_16(builder, (umbra_ptr){0, 0}, ixo));
            umbra_store_16(builder, (umbra_ptr){2, 0}, umbra_f16_from_f32(builder, val));
            backends B = make_full(builder, opt);
            for (int bi = 0; bi < 3; bi++) {
                uint16_t src[16];
                for (int k = 0; k < 16; k++) {
                    __fp16 h = (__fp16)(k + 10);
                    __builtin_memcpy(&src[k], &h, 2);
                }
                int32_t  uni[1] = {5};
                uint16_t dst[8] = {0};
                if (!run(&B, bi, 8, 1,
                         (umbra_buf[]){
                             {src, sizeof src, 0, 0},
                             {uni, sizeof uni, 1, 0},
                             {dst, sizeof dst, 0, 0},
                         })) {
                    continue;
                }
                for (int k = 0; k < 8; k++) {
                    __fp16 h;
                    __builtin_memcpy(&h, &dst[k], 2);
                    equiv((float)h, (float)(15 + k)) here;
                }
            }
            cleanup(&B);
        }
    }
}

static void test_shift_imm(void) {
    for (int opt = 0; opt < 2; opt++) {
        {
            struct umbra_builder *builder = umbra_builder();
            umbra_val             x = umbra_load_32(builder, (umbra_ptr){0, 0});
            umbra_val r = umbra_shl_i32(builder, x, umbra_imm_i32(builder, 8));
            umbra_store_32(builder, (umbra_ptr){1, 0}, r);
            backends B = make(builder, opt);
            for (int bi = 0; bi < 3; bi++) {
                uint32_t src[] = {1, 2, 3, 0xff};
                uint32_t dst[4] = {0};
                if (!run(&B, bi, 4, 1,
                         (umbra_buf[]){
                             {src, 4 * 4, 0, 0},
                             {dst, 4 * 4, 0, 0},
                         })) {
                    continue;
                }
                (dst[0] == 0x100u) here;
                (dst[1] == 0x200u) here;
                (dst[2] == 0x300u) here;
                (dst[3] == 0xff00u) here;
            }
            cleanup(&B);
        }
        {
            struct umbra_builder *builder = umbra_builder();
            umbra_val             x = umbra_load_32(builder, (umbra_ptr){0, 0});
            umbra_val r = umbra_shr_u32(builder, x, umbra_imm_i32(builder, 8));
            umbra_store_32(builder, (umbra_ptr){1, 0}, r);
            backends B = make(builder, opt);
            for (int bi = 0; bi < 3; bi++) {
                uint32_t src[] = {0x100, 0x200, 0xff00};
                uint32_t dst[3] = {0};
                if (!run(&B, bi, 3, 1,
                         (umbra_buf[]){
                             {src, 3 * 4, 0, 0},
                             {dst, 3 * 4, 0, 0},
                         })) {
                    continue;
                }
                (dst[0] == 1) here;
                (dst[1] == 2) here;
                (dst[2] == 0xff) here;
            }
            cleanup(&B);
        }
        {
            struct umbra_builder *builder = umbra_builder();
            umbra_val             x = umbra_load_32(builder, (umbra_ptr){0, 0});
            umbra_val r = umbra_shr_s32(builder, x, umbra_imm_i32(builder, 4));
            umbra_store_32(builder, (umbra_ptr){1, 0}, r);
            backends B = make(builder, opt);
            for (int bi = 0; bi < 3; bi++) {
                uint32_t src[] = {0x80, 0xfffffff0u};
                uint32_t dst[2] = {0};
                if (!run(&B, bi, 2, 1,
                         (umbra_buf[]){
                             {src, 2 * 4, 0, 0},
                             {dst, 2 * 4, 0, 0},
                         })) {
                    continue;
                }
                (dst[0] == 8) here;
                (dst[1] == 0xffffffffu) here;
            }
            cleanup(&B);
        }
    }
}

static void test_pack_channels(void) {
    for (int opt = 0; opt < 2; opt++) {
        struct umbra_builder *builder = umbra_builder();
        umbra_val             r = umbra_load_32(builder, (umbra_ptr){0, 0});
        umbra_val             g = umbra_load_32(builder, (umbra_ptr){1, 0});
        umbra_val             b = umbra_load_32(builder, (umbra_ptr){2, 0});
        umbra_val             a = umbra_load_32(builder, (umbra_ptr){3, 0});
        umbra_val             mask = umbra_imm_i32(builder, 0xff);
        umbra_val             px = umbra_and_i32(builder, r, mask);
        px = umbra_pack(builder, px, umbra_and_i32(builder, g, mask), 8);
        px = umbra_pack(builder, px, umbra_and_i32(builder, b, mask), 16);
        px = umbra_pack(builder, px, a, 24);
        umbra_store_32(builder, (umbra_ptr){4, 0}, px);
        backends B = make(builder, opt);
        for (int bi = 0; bi < 3; bi++) {
            uint32_t rr[] = {0xAA, 0x11, 0xFF};
            uint32_t gg[] = {0xBB, 0x22, 0x00};
            uint32_t bb_[] = {0xCC, 0x33, 0xFF};
            uint32_t aa[] = {0xDD, 0x44, 0x00};
            uint32_t dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {rr, 3 * 4, 0, 0},
                         {gg, 3 * 4, 0, 0},
                         {bb_, 3 * 4, 0, 0},
                         {aa, 3 * 4, 0, 0},
                         {dst, 3 * 4, 0, 0},
                     })) {
                continue;
            }
            (dst[0] == 0xDDCCBBAAu) here;
            (dst[1] == 0x44332211u) here;
            (dst[2] == 0x00FF00FFu) here;
        }
        cleanup(&B);
    }
}

static void test_gather_deref_large(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val             idx = umbra_load_32(b, (umbra_ptr){0, 0});
    int                   off = umbra_reserve_ptr(b);
    umbra_ptr             src = umbra_deref_ptr(b, (umbra_ptr){1, 0}, off);
    umbra_val             val = umbra_i32_from_s16(b, umbra_gather_16(b, src, idx));
    umbra_store_32(b, (umbra_ptr){2, 0}, val);
    backends B = make_full(b, 0);

    enum { N = 33000 };
    int16_t *data = calloc(N, sizeof(int16_t));
    data[0] = 10;
    data[100] = 20;
    data[32800] = 30;
    data[N - 1] = 42;

    int32_t indices[4] = {0, 100, 32800, N - 1};
    int32_t dst[4] = {0};

    uint64_t uni_[3] = {0};
    char    *uni = (char *)uni_;
    {
        void   *p = data;
        size_t sz = (size_t)N * 2;
        __builtin_memcpy(uni + off, &p, 8);
        __builtin_memcpy(uni + off + 8, &sz, 8);
    }

    for (int bi = 0; bi < 3; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!run(&B, bi, 4, 1,
                 (umbra_buf[]){
                     {indices, sizeof indices, 0, 0},
                     {uni, sizeof uni_, 1, 0},
                     {dst, sizeof dst, 0, 0},
                 })) {
            continue;
        }
        (dst[0] == 10) here;
        (dst[1] == 20) here;
        (dst[2] == 30) here;
        (dst[3] == 42) here;
    }

    free(data);
    cleanup(&B);
}

static void test_imm_fused(void) {
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             r = umbra_div_f32(b, x, umbra_imm_f32(b, 2.0f));
        umbra_store_32(b, (umbra_ptr){1, 0}, r);
        backends B = make(b, 0);
        for (int bi = 0; bi < 3; bi++) {
            float src[] = {10, 20, 30}, dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {src, 3 * 4, 0, 0},
                         {dst, 3 * 4, 0, 0},
                     })) {
                continue;
            }
            equiv(dst[0], 5) here;
            equiv(dst[1], 10) here;
            equiv(dst[2], 15) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             r = umbra_sub_i32(b, x, umbra_imm_i32(b, 5));
        umbra_store_32(b, (umbra_ptr){1, 0}, r);
        backends B = make(b, 0);
        for (int bi = 0; bi < 3; bi++) {
            int src[] = {10, 20, 30}, dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {src, 3 * 4, 0, 0},
                         {dst, 3 * 4, 0, 0},
                     })) {
                continue;
            }
            (dst[0] == 5) here;
            (dst[1] == 15) here;
            (dst[2] == 25) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             r = umbra_or_i32(b, x, umbra_imm_i32(b, 0xf0));
        umbra_store_32(b, (umbra_ptr){1, 0}, r);
        backends B = make(b, 0);
        for (int bi = 0; bi < 3; bi++) {
            int src[] = {0x0f, 0x00, 0xff}, dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {src, 3 * 4, 0, 0},
                         {dst, 3 * 4, 0, 0},
                     })) {
                continue;
            }
            (dst[0] == 0xff) here;
            (dst[1] == 0xf0) here;
            (dst[2] == 0xff) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             r = umbra_eq_f32(b, x, umbra_imm_f32(b, 2.0f));
        umbra_store_32(b, (umbra_ptr){1, 0}, r);
        backends B = make(b, 0);
        for (int bi = 0; bi < 3; bi++) {
            float src[] = {1, 2, 3};
            int   dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {src, 3 * 4, 0, 0},
                         {dst, 3 * 4, 0, 0},
                     })) {
                continue;
            }
            (dst[0] == 0) here;
            (dst[1] == -1) here;
            (dst[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             r = umbra_le_f32(b, x, umbra_imm_f32(b, 2.0f));
        umbra_store_32(b, (umbra_ptr){1, 0}, r);
        backends B = make(b, 0);
        for (int bi = 0; bi < 3; bi++) {
            float src[] = {1, 2, 3};
            int   dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {src, 3 * 4, 0, 0},
                         {dst, 3 * 4, 0, 0},
                     })) {
                continue;
            }
            (dst[0] == -1) here;
            (dst[1] == -1) here;
            (dst[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             r = umbra_lt_s32(b, x, umbra_imm_i32(b, 5));
        umbra_store_32(b, (umbra_ptr){1, 0}, r);
        backends B = make(b, 0);
        for (int bi = 0; bi < 3; bi++) {
            int src[] = {3, 5, 7}, dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {src, 3 * 4, 0, 0},
                         {dst, 3 * 4, 0, 0},
                     })) {
                continue;
            }
            (dst[0] == -1) here;
            (dst[1] == 0) here;
            (dst[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             r = umbra_le_s32(b, x, umbra_imm_i32(b, 5));
        umbra_store_32(b, (umbra_ptr){1, 0}, r);
        backends B = make(b, 0);
        for (int bi = 0; bi < 3; bi++) {
            int src[] = {3, 5, 7}, dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {src, 3 * 4, 0, 0},
                         {dst, 3 * 4, 0, 0},
                     })) {
                continue;
            }
            (dst[0] == -1) here;
            (dst[1] == -1) here;
            (dst[2] == 0) here;
        }
        cleanup(&B);
    }
}

static void test_cmp_zero(void) {
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             r = umbra_lt_s32(b, x, umbra_imm_i32(b, 0));
        umbra_store_32(b, (umbra_ptr){1, 0}, r);
        backends B = make(b, 0);
        for (int bi = 0; bi < 3; bi++) {
            int src[] = {-1, 0, 1}, dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {src, 3 * 4, 0, 0},
                         {dst, 3 * 4, 0, 0},
                     })) {
                continue;
            }
            (dst[0] == -1) here;
            (dst[1] == 0) here;
            (dst[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             r = umbra_le_f32(b, x, umbra_imm_f32(b, 0.0f));
        umbra_store_32(b, (umbra_ptr){1, 0}, r);
        backends B = make(b, 0);
        for (int bi = 0; bi < 3; bi++) {
            float src[] = {-1, 0, 1};
            int   dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {src, 3 * 4, 0, 0},
                         {dst, 3 * 4, 0, 0},
                     })) {
                continue;
            }
            (dst[0] == -1) here;
            (dst[1] == -1) here;
            (dst[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             r = umbra_eq_f32(b, x, umbra_imm_f32(b, 0.0f));
        umbra_store_32(b, (umbra_ptr){1, 0}, r);
        backends B = make(b, 0);
        for (int bi = 0; bi < 3; bi++) {
            float src[] = {-1, 0, 1};
            int   dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {src, 3 * 4, 0, 0},
                         {dst, 3 * 4, 0, 0},
                     })) {
                continue;
            }
            (dst[0] == 0) here;
            (dst[1] == -1) here;
            (dst[2] == 0) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             r = umbra_le_s32(b, x, umbra_imm_i32(b, 0));
        umbra_store_32(b, (umbra_ptr){1, 0}, r);
        backends B = make(b, 0);
        for (int bi = 0; bi < 3; bi++) {
            int src[] = {-1, 0, 1}, dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {src, 3 * 4, 0, 0},
                         {dst, 3 * 4, 0, 0},
                     })) {
                continue;
            }
            (dst[0] == -1) here;
            (dst[1] == -1) here;
            (dst[2] == 0) here;
        }
        cleanup(&B);
    }
}

static void test_imm_broadcast(void) {
    int patterns[] = {
        (int)0xfffe0000u,
        (int)0x7fffffffu,
    };
    for (int pi = 0; pi < 2; pi++) {
        struct umbra_builder *b = umbra_builder();
        umbra_val             v = umbra_imm_i32(b, patterns[pi]);
        umbra_store_32(b, (umbra_ptr){0, 0}, v);
        backends B = make(b, 0);
        for (int bi = 0; bi < 3; bi++) {
            int dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {dst, 3 * 4, 0, 0},
                     })) {
                continue;
            }
            (dst[0] == patterns[pi]) here;
            (dst[1] == patterns[pi]) here;
            (dst[2] == patterns[pi]) here;
        }
        cleanup(&B);
    }
}

static void test_codegen_regalloc(void) {
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             y = umbra_load_32(b, (umbra_ptr){1, 0});
        umbra_val             z = umbra_load_32(b, (umbra_ptr){2, 0});
        umbra_val             r = umbra_sub_f32(b, z, umbra_mul_f32(b, x, y));
        umbra_val             s = umbra_add_f32(b, r, z);
        umbra_val             u = umbra_add_f32(b, s, y);
        umbra_store_32(b, (umbra_ptr){3, 0}, u);
        backends B = make(b, 0);
        for (int bi = 0; bi < 3; bi++) {
            float a[] = {2, 3}, c[] = {5, 6}, d[] = {100, 200};
            float dst[4] = {0};
            if (!run(&B, bi, 2, 1,
                     (umbra_buf[]){
                         {a, 2 * 4, 0, 0},
                         {c, 2 * 4, 0, 0},
                         {d, 2 * 4, 0, 0},
                         {dst, 2 * 4, 0, 0},
                     })) {
                continue;
            }
            equiv(dst[0], 90 + 100 + 5) here;
            equiv(dst[1], 182 + 200 + 6) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             y = umbra_load_32(b, (umbra_ptr){1, 0});
        umbra_val             z = umbra_load_32(b, (umbra_ptr){2, 0});
        umbra_val             r = umbra_sub_f32(b, z, umbra_mul_f32(b, x, y));
        umbra_val             s = umbra_add_f32(b, r, x);
        umbra_val             u = umbra_add_f32(b, s, y);
        umbra_val             w = umbra_add_f32(b, u, z);
        umbra_store_32(b, (umbra_ptr){3, 0}, w);
        backends B = make(b, 0);
        for (int bi = 0; bi < 3; bi++) {
            float a[] = {2, 3}, c[] = {5, 6}, d[] = {100, 200};
            float dst[4] = {0};
            if (!run(&B, bi, 2, 1,
                     (umbra_buf[]){
                         {a, 2 * 4, 0, 0},
                         {c, 2 * 4, 0, 0},
                         {d, 2 * 4, 0, 0},
                         {dst, 2 * 4, 0, 0},
                     })) {
                continue;
            }
            equiv(dst[0], 90 + 2 + 5 + 100) here;
            equiv(dst[1], 182 + 3 + 6 + 200) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             y = umbra_load_32(b, (umbra_ptr){1, 0});
        umbra_val             z = umbra_load_32(b, (umbra_ptr){2, 0});
        umbra_val             r = umbra_sel_i32(b, x, y, z);
        umbra_val             s = umbra_add_i32(b, r, x);
        umbra_val             u = umbra_add_i32(b, s, y);
        umbra_store_32(b, (umbra_ptr){3, 0}, u);
        backends B = make(b, 0);
        for (int bi = 0; bi < 3; bi++) {
            int m[] = {-1, 0}, t[] = {10, 20}, f[] = {30, 40};
            int dst[2] = {0};
            if (!run(&B, bi, 2, 1,
                     (umbra_buf[]){
                         {m, 2 * 4, 0, 0},
                         {t, 2 * 4, 0, 0},
                         {f, 2 * 4, 0, 0},
                         {dst, 2 * 4, 0, 0},
                     })) {
                continue;
            }
            (dst[0] == 10 + (-1) + 10) here;
            (dst[1] == 40 + 0 + 20) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             base = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             v = umbra_load_32(b, (umbra_ptr){1, 0});
        umbra_val             r = umbra_pack(b, base, v, 8);
        umbra_val             s = umbra_add_i32(b, base, umbra_imm_i32(b, 1));
        umbra_store_32(b, (umbra_ptr){2, 0}, r);
        umbra_store_32(b, (umbra_ptr){3, 0}, s);
        backends B = make(b, 0);
        for (int bi = 0; bi < 3; bi++) {
            int ba[] = {0x0F, 0xFF};
            int vl[] = {0x0A, 0x0B};
            int dst[2] = {0}, dst2[2] = {0};
            if (!run(&B, bi, 2, 1,
                     (umbra_buf[]){
                         {ba, 2 * 4, 0, 0},
                         {vl, 2 * 4, 0, 0},
                         {dst, 2 * 4, 0, 0},
                         {dst2, 2 * 4, 0, 0},
                     })) {
                continue;
            }
            (dst[0] == (0x0F | (0x0A << 8))) here;
            (dst[1] == (0xFF | (0x0B << 8))) here;
            (dst2[0] == 0x10) here;
            (dst2[1] == 0x100) here;
        }
        cleanup(&B);
    }
}

static void test_fms(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val             vx = umbra_load_32(b, (umbra_ptr){0, 0}),
                          vy = umbra_load_32(b, (umbra_ptr){1, 0}),
                          vz = umbra_load_32(b, (umbra_ptr){2, 0});
    umbra_val             r = umbra_sub_f32(b, vz, umbra_mul_f32(b, vx, vy));
    umbra_store_32(b, (umbra_ptr){3, 0}, r);
    backends B = make(b, 0);
    for (int bi = 0; bi < 3; bi++) {
        float a[] = {2, 3, 4}, c[] = {5, 6, 7}, d[] = {100, 200, 300}, dst[3] = {0};
        if (!run(&B, bi, 3, 1,
                 (umbra_buf[]){
                     {a, 3 * 4, 0, 0},
                     {c, 3 * 4, 0, 0},
                     {d, 3 * 4, 0, 0},
                     {dst, 3 * 4, 0, 0},
                 })) {
            continue;
        }
        equiv(dst[0], 90) here;
        equiv(dst[1], 182) here;
        equiv(dst[2], 272) here;
    }
    cleanup(&B);
}

static void test_movi_patterns(void) {
    int patterns[] = {
        0x0000ff00,  0x00ff0000,        (int)0xff000000u, ~0x000000ff, ~0x0000ff00,
        ~0x00ff0000, ~(int)0xff000000u, (int)0xfffe0000u, 0x12345678,
    };
    for (int pi = 0; pi < 9; pi++) {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             r = umbra_or_i32(b, x, umbra_imm_i32(b, patterns[pi]));
        umbra_store_32(b, (umbra_ptr){1, 0}, r);
        backends B = make(b, 0);
        for (int bi = 0; bi < 3; bi++) {
            int src[] = {0, 0, 0}, dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {src, 3 * 4, 0, 0},
                         {dst, 3 * 4, 0, 0},
                     })) {
                continue;
            }
            (dst[0] == patterns[pi]) here;
            (dst[1] == patterns[pi]) here;
            (dst[2] == patterns[pi]) here;
        }
        cleanup(&B);
    }
}

static void test_mixed_ptr(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val             v32 = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_val             v16 = umbra_i32_from_u16(b, umbra_load_16(b, (umbra_ptr){0, 0}));
    umbra_val             r = umbra_add_i32(b, v32, v16);
    umbra_store_32(b, (umbra_ptr){1, 0}, r);
    backends B = make(b, 0);
    for (int bi = 0; bi < 3; bi++) {
        uint32_t src[] = {0x00010002, 0x00030004, 0x00050006};
        int      dst[3] = {0};
        if (!run(&B, bi, 3, 1,
                 (umbra_buf[]){
                     {src, sizeof src, 0, 0},
                     {dst, 3 * 4, 0, 0},
                 })) {
            continue;
        }
        (dst[0] == (int)(0x00010002 + 0x0002)) here;
        (dst[1] == (int)(0x00030004 + 0x0001)) here;
        (dst[2] == (int)(0x00050006 + 0x0004)) here;
    }
    cleanup(&B);
}

static void test_uni_16(void) {
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             v = umbra_uniform_16(b, (umbra_ptr){0, 0}, 2);
        umbra_store_32(b, (umbra_ptr){1, 0}, v);
        backends B = make(b, 0);
        for (int bi = 0; bi < 3; bi++) {
            uint16_t src[] = {100, 200, 300, 400};
            int      dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {src, 8, 0, 0},
                         {dst, 3 * 4, 0, 0},
                     })) {
                continue;
            }
            (dst[0] == 300) here;
            (dst[1] == 300) here;
            (dst[2] == 300) here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val idx = umbra_uniform_32(b, (umbra_ptr){0, 0}, 0);
        umbra_val v = umbra_i32_from_u16(b, umbra_gather_16(b, (umbra_ptr){1, 0}, idx));
        umbra_store_32(b, (umbra_ptr){2, 0}, v);
        backends B = make(b, 0);
        for (int bi = 0; bi < 3; bi++) {
            int      idx_val[] = {1};
            uint16_t src[] = {100, 200, 300, 400};
            int      dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {idx_val, 4, 0, 0},
                         {src, 8, 0, 0},
                         {dst, 3 * 4, 0, 0},
                     })) {
                continue;
            }
            (dst[0] == 200) here;
            (dst[1] == 200) here;
            (dst[2] == 200) here;
        }
        cleanup(&B);
    }
}

static void test_dump(void) {
    FILE *f = fopen("/dev/null", "w");
    if (!f) { return; }

    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             r = umbra_add_f32(b, x, umbra_imm_f32(b, 1.0f));
        umbra_store_32(b, (umbra_ptr){1, 0}, r);
        umbra_dump_builder(b, f);

        struct umbra_basic_block *bb = umbra_basic_block(b);
        umbra_builder_free(b);
        umbra_dump_basic_block(bb, f);
        umbra_basic_block_free(bb);
    }
    fclose(f);
}

static void test_xy(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val             x = umbra_x(b);
    umbra_val             y = umbra_y(b);
    umbra_store_32(b, (umbra_ptr){0, 0}, x);
    umbra_store_32(b, (umbra_ptr){1, 0}, y);
    backends B = make(b, 0);

    enum { W = 5, H = 3, N = W * H };
    int32_t xbuf[N], ybuf[N];
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(xbuf, 0, sizeof xbuf);
        __builtin_memset(ybuf, 0, sizeof ybuf);
        if (!run(&B, bi, W, H,
                 (umbra_buf[]){
                     {xbuf, sizeof xbuf, 0, W * 4},
                     {ybuf, sizeof ybuf, 0, W * 4},
                 })) {
            continue;
        }
        for (int j = 0; j < N; j++) {
            (xbuf[j] == j % W) here;
            (ybuf[j] == j / W) here;
        }
    }
    cleanup(&B);
}

static void test_load_next_32(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val             v = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_store_32(b, (umbra_ptr){1, 0}, v);
    backends B = make(b, 0);

    int32_t src[16], dst[16];
    for (int i = 0; i < 16; i++) { src[i] = i * 7 + 3; }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!run(&B, bi, 16, 1, (umbra_buf[]){
            {src, sizeof src, 0, 0},
            {dst, sizeof dst, 0, 0},
        })) { continue; }
        for (int i = 0; i < 16; i++) { (dst[i] == src[i]) here; }
    }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!run(&B, bi, 4, 4, (umbra_buf[]){
            {src, sizeof src, 0, 4*4},
            {dst, sizeof dst, 0, 4*4},
        })) { continue; }
        for (int i = 0; i < 16; i++) { (dst[i] == src[i]) here; }
    }
    cleanup(&B);
}

static void test_load_next_16(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val             v = umbra_load_16(b, (umbra_ptr){0, 0});
    umbra_store_16(b, (umbra_ptr){1, 0}, v);
    backends B = make(b, 0);

    int16_t src[16], dst[16];
    for (int i = 0; i < 16; i++) { src[i] = (int16_t)(i * 7 + 3); }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!run(&B, bi, 16, 1, (umbra_buf[]){
            {src, sizeof src, 0, 0},
            {dst, sizeof dst, 0, 0},
        })) { continue; }
        for (int i = 0; i < 16; i++) { (dst[i] == src[i]) here; }
    }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!run(&B, bi, 4, 4, (umbra_buf[]){
            {src, sizeof src, 0, 4*2},
            {dst, sizeof dst, 0, 4*2},
        })) { continue; }
        for (int i = 0; i < 16; i++) { (dst[i] == src[i]) here; }
    }
    cleanup(&B);
}

static void test_load_store_next_64(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val             lo, hi;
    umbra_load_64(b, (umbra_ptr){0, 0}, &lo, &hi);
    umbra_store_64(b, (umbra_ptr){1, 0}, lo, hi);
    backends B = make(b, 0);

    int32_t src[32], dst[32];
    for (int i = 0; i < 32; i++) { src[i] = i * 7 + 3; }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!run(&B, bi, 16, 1, (umbra_buf[]){
            {src, sizeof src, 0, 0},
            {dst, sizeof dst, 0, 0},
        })) { continue; }
        for (int i = 0; i < 32; i++) { (dst[i] == src[i]) here; }
    }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!run(&B, bi, 4, 4, (umbra_buf[]){
            {src, sizeof src, 0, 4*8},
            {dst, sizeof dst, 0, 4*8},
        })) { continue; }
        for (int i = 0; i < 32; i++) { (dst[i] == src[i]) here; }
    }
    cleanup(&B);
}

static void test_load_stride_neq_w(void) {
    // Regression: add(mul(y, rs_uniform), x) was optimized to a contiguous
    // load using the linear loop counter.  When rs != w, this is wrong.
    struct umbra_builder *b = umbra_builder();
    int       ri = umbra_reserve(b, 1);
    umbra_val x = umbra_x(b);
    umbra_val y = umbra_y(b);
    umbra_val rs = umbra_uniform_32(b, (umbra_ptr){0, 0}, ri);
    umbra_val ix = umbra_add_i32(b, umbra_mul_i32(b, y, rs), x);
    umbra_val v = umbra_gather_32(b, (umbra_ptr){1, 0}, ix);
    umbra_store_32(b, (umbra_ptr){2, 0}, v);
    backends B = make(b, 0);

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
        if (!run(&B, bi, 4, 2, (umbra_buf[]){
            {uni, (size_t)(ri * 4 + 4), 1, 0},
            {src, sizeof src, 0, 0},
            {dst, sizeof dst, 0, 4*4},
        })) { continue; }
        for (int i = 0; i < 8; i++) { (dst[i] == expected[i]) here; }
    }
    cleanup(&B);
}

static void test_jit_xs_init(void) {
    // Regression: ARM64 JIT only initialized XS (spill stack pointer) when
    // ns > 0.  For tiny programs with no spills, XS was caller garbage.
    // Use enough preamble values to force eviction+fill at the back-edge.
    struct umbra_builder *b = umbra_builder();
    enum { N = 25 };
    umbra_val pre[N];
    for (int i = 0; i < N; i++) { pre[i] = umbra_imm_i32(b, i + 1); }
    umbra_val v = umbra_load_32(b, (umbra_ptr){0, 0});
    for (int i = 0; i < N; i++) { v = umbra_add_i32(b, v, pre[i]); }
    umbra_store_32(b, (umbra_ptr){1, 0}, v);
    backends B = make(b, 0);
    int32_t sum_pre = N * (N + 1) / 2;
    int32_t src[4] = {100, 200, 300, 400}, dst[4] = {0};
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
#if defined(__aarch64__)
        __asm__ volatile("mov x15, #0xdead" ::: "x15");
#endif
        if (!run(&B, bi, 4, 1, (umbra_buf[]){
            {src, sizeof src, 0, 0},
            {dst, sizeof dst, 0, 0},
        })) { continue; }
        for (int i = 0; i < 4; i++) { (dst[i] == src[i] + sum_pre) here; }
    }
    cleanup(&B);
}

static void test_backend_threadsafe(void) {
    struct umbra_backend *interp = umbra_backend_interp();
    (umbra_backend_threadsafe(interp) == 1) here;
    umbra_backend_free(interp);

    struct umbra_backend *jit = umbra_backend_jit();
    (umbra_backend_threadsafe(jit) == 1) here;
    umbra_backend_free(jit);

    struct umbra_backend *metal = umbra_backend_metal();
    if (metal) {
        (umbra_backend_threadsafe(metal) == 0) here;
        umbra_backend_free(metal);
    }

    (umbra_backend_threadsafe(NULL) == 0) here;
}

static void test_program_null_guards(void) {
    umbra_backend_flush(NULL);
    umbra_backend_free(NULL);
    umbra_program_free(NULL);
    umbra_program_dump(NULL, NULL);

    // flush/dump on backends that don't have those fns
    struct umbra_backend *be = umbra_backend_interp();
    umbra_backend_flush(be);

    struct umbra_builder *b = umbra_builder();
    umbra_store_32(b, (umbra_ptr){0, 0}, umbra_load_32(b, (umbra_ptr){0, 0}));
    struct umbra_basic_block *bb = umbra_basic_block(b);
    umbra_builder_free(b);
    struct umbra_program *p = umbra_program(be, bb);
    umbra_basic_block_free(bb);

    // dump on interpreter (no dump fn)
    umbra_program_dump(p, stdout);

    // queue with w=0 and h=0
    int32_t buf[1] = {0};
    umbra_program_queue(p, 0, 0, 0, 1, (umbra_buf[]){{buf, 4, 0, 0}});
    umbra_program_queue(p, 0, 0, 1, 0, (umbra_buf[]){{buf, 4, 0, 0}});

    umbra_program_free(p);
    umbra_backend_free(be);
}

int main(void) {
    test_backend_threadsafe();
    test_program_null_guards();
    test_f32_ops();
    test_i32_ops();
    test_cmp_i32();
    test_cmp_f32();
    test_imm();
    test_fma_f32();
    test_min_max_sqrt_f32();
    test_abs_sign_f32();
    test_round_floor_ceil();
    test_large_n();
    test_convert();
    test_dedup();
    test_constprop();
    test_strength_reduction();
    test_zero_imm();
    test_late_imm_identity();
    test_abs_peepholes();
    test_load_8x4();
    test_store_8x4();
    test_srcover();
    test_hash_quality();
    test_narrow_16();
    test_mixed_ptr_sizes();
    test_n9();
    test_preamble_pair_alias();
    test_gather_clamp();
    test_gather_clamp_zero_sz();
    test_offset_load_store();
    test_shift_imm();
    test_pack_channels();
    test_gather_deref_large();
    test_imm_fused();
    test_cmp_zero();
    test_imm_broadcast();
    test_codegen_regalloc();
    test_fms();
    test_movi_patterns();
    test_mixed_ptr();
    test_uni_16();
    test_dump();
    test_xy();
    test_load_next_32();
    test_load_next_16();
    test_load_store_next_64();
    test_load_stride_neq_w();
    test_jit_xs_init();

    // Regression test: arbitrary (l,t,r,b) tile dispatch on a strided buffer.
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val x   = umbra_x(b);
        umbra_val y   = umbra_y(b);
        umbra_val k   = umbra_imm_i32(b, 1000);
        umbra_val val = umbra_add_i32(b, x, umbra_mul_i32(b, y, k));
        umbra_store_32(b, (umbra_ptr){0, 0}, val);
        backends B = make(b, 0);

        enum { S = 32, TH = 16, L = 5, T = 3, R = 21, BT = 11 };
        int32_t buf[S * TH];

        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            __builtin_memset(buf, 0xff, sizeof buf);
            if (!B.p[bi]) { continue; }
            umbra_program_queue(B.p[bi], L, T, R, BT,
                                (umbra_buf[]){{buf, sizeof buf, 0, S * 4}});
            umbra_backend_flush(B.be[bi]);
            for (int row = T; row < BT; row++) {
                for (int col = L; col < R; col++) {
                    (buf[row * S + col] == col + row * 1000) here;
                }
            }
            for (int row = 0; row < TH; row++) {
                for (int col = 0; col < S; col++) {
                    if (row >= T && row < BT && col >= L && col < R) { continue; }
                    (buf[row * S + col] == (int32_t)0xffffffff) here;
                }
            }
        }
        cleanup(&B);
    }

    // Regression test: load from strided buffer with arbitrary tile.
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val v   = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val one = umbra_imm_i32(b, 1);
        umbra_store_32(b, (umbra_ptr){1, 0}, umbra_add_i32(b, v, one));
        backends B = make(b, 0);

        enum { S = 20, TH = 10, L = 3, T = 2, R = 15, BT = 7 };
        int32_t src[S * TH], dst[S * TH];
        for (int i = 0; i < S * TH; i++) { src[i] = i; }

        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            __builtin_memset(dst, 0, sizeof dst);
            if (!B.p[bi]) { continue; }
            umbra_program_queue(B.p[bi], L, T, R, BT,
                                (umbra_buf[]){{src, sizeof src, 1, S * 4},
                                              {dst, sizeof dst, 0, S * 4}});
            umbra_backend_flush(B.be[bi]);
            for (int row = T; row < BT; row++) {
                for (int col = L; col < R; col++) {
                    int idx = row * S + col;
                    (dst[idx] == src[idx] + 1) here;
                }
            }
            for (int row = 0; row < TH; row++) {
                for (int col = 0; col < S; col++) {
                    if (row >= T && row < BT && col >= L && col < R) { continue; }
                    (dst[row * S + col] == 0) here;
                }
            }
        }
        cleanup(&B);
    }

    // Regression test: two direct buffers with different row_bytes (strides).
    // Metal bug: uses a single shared stride from one store buffer.
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             v   = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             one = umbra_imm_i32(b, 1);
        umbra_store_32(b, (umbra_ptr){1, 0}, umbra_add_i32(b, v, one));
        backends B = make(b, 0);

        enum { SW = 32, DW = 20, H = 5, L = 3, T = 1, R = 15, BT = 4 };
        int32_t src[SW * H], dst[DW * H];
        for (int i = 0; i < SW * H; i++) { src[i] = i * 10; }

        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            __builtin_memset(dst, 0, sizeof dst);
            if (!B.p[bi]) { continue; }
            umbra_program_queue(B.p[bi], L, T, R, BT,
                                (umbra_buf[]){{src, sizeof src, 1, SW * 4},
                                              {dst, sizeof dst, 0, DW * 4}});
            umbra_backend_flush(B.be[bi]);
            for (int row = T; row < BT; row++) {
                for (int col = L; col < R; col++) {
                    int si = row * SW + col;
                    int di = row * DW + col;
                    (dst[di] == src[si] + 1) here;
                }
            }
        }
        cleanup(&B);
    }

    // Regression test: l>0 with deref'd buffer that has row_bytes>0.
    {
        struct umbra_builder *b = umbra_builder();
        int                   off = umbra_reserve_ptr(b);
        umbra_ptr             src = umbra_deref_ptr(b, (umbra_ptr){1, 0}, off);
        umbra_val             v   = umbra_load_32(b, src);
        umbra_val             one = umbra_imm_i32(b, 1);
        umbra_store_32(b, (umbra_ptr){2, 0}, umbra_add_i32(b, v, one));
        backends B = make(b, 0);

        enum { S = 20, TH = 6, L = 3, T = 1, R = 15, BT = 5 };
        int32_t src_px[S * TH];
        for (int i = 0; i < S * TH; i++) { src_px[i] = i; }
        int32_t dst_px[S * TH];

        uint64_t uni_[4] = {0};
        char    *uni = (char *)uni_;
        {
            void   *p = src_px;
            size_t  sz = sizeof src_px;
            size_t  rb = S * 4;
            __builtin_memcpy(uni + off,      &p,  8);
            __builtin_memcpy(uni + off + 8,  &sz, 8);
            __builtin_memcpy(uni + off + 16, &rb, 8);
        }

        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            __builtin_memset(dst_px, 0, sizeof dst_px);
            if (!B.p[bi]) { continue; }
            umbra_program_queue(B.p[bi], L, T, R, BT,
                                (umbra_buf[]){{0, 0, 0, 0},
                                              {uni, sizeof uni_, 1, 0},
                                              {dst_px, sizeof dst_px, 0, S * 4}});
            umbra_backend_flush(B.be[bi]);
            for (int row = T; row < BT; row++) {
                for (int col = L; col < R; col++) {
                    int idx = row * S + col;
                    (dst_px[idx] == src_px[idx] + 1) here;
                }
            }
            for (int row = 0; row < TH; row++) {
                for (int col = 0; col < S; col++) {
                    if (row >= T && row < BT && col >= L && col < R) { continue; }
                    (dst_px[row * S + col] == 0) here;
                }
            }
        }
        cleanup(&B);
    }

    // Regression test: l>0 with deref'd 16-bit buffer, row_bytes>0.
    {
        struct umbra_builder *b = umbra_builder();
        int                   off = umbra_reserve_ptr(b);
        umbra_ptr             src = umbra_deref_ptr(b, (umbra_ptr){1, 0}, off);
        umbra_val             v   = umbra_f32_from_f16(b, umbra_load_16(b, src));
        umbra_val             one = umbra_imm_f32(b, 1.0f);
        umbra_store_32(b, (umbra_ptr){2, 0}, umbra_add_f32(b, v, one));
        backends B = make(b, 0);

        enum { S = 16, TH = 4, L = 2, T = 1, R = 10, BT = 3 };
        uint16_t src_px[S * TH];
        for (int i = 0; i < S * TH; i++) {
            float f = (float)i;
            __fp16 h = (__fp16)f;
            __builtin_memcpy(&src_px[i], &h, 2);
        }
        float dst_px[S * TH];

        uint64_t uni_[4] = {0};
        char    *uni = (char *)uni_;
        {
            void   *p = src_px;
            size_t  sz = sizeof src_px;
            size_t  rb = S * 2;
            __builtin_memcpy(uni + off,      &p,  8);
            __builtin_memcpy(uni + off + 8,  &sz, 8);
            __builtin_memcpy(uni + off + 16, &rb, 8);
        }

        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            __builtin_memset(dst_px, 0, sizeof dst_px);
            if (!B.p[bi]) { continue; }
            umbra_program_queue(B.p[bi], L, T, R, BT,
                                (umbra_buf[]){{0, 0, 0, 0},
                                              {uni, sizeof uni_, 1, 0},
                                              {dst_px, sizeof dst_px, 0, S * 4}});
            umbra_backend_flush(B.be[bi]);
            for (int row = T; row < BT; row++) {
                for (int col = L; col < R; col++) {
                    int idx = row * S + col;
                    equiv(dst_px[idx], (float)(row * S + col) + 1.0f) here;
                }
            }
        }
        cleanup(&B);
    }

    return 0;
}


