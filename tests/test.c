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

static backends make(struct umbra_builder *builder) {
    struct umbra_basic_block *bb = umbra_basic_block(builder);
    umbra_builder_free(builder);
    backends B = test_backends_make(bb);
    umbra_basic_block_free(bb);
    return B;
}
static _Bool run(backends *B, int b, int w, int h, umbra_buf buf[]) {
    return test_backends_run(B, b, w, h, buf);
}
static void cleanup(backends *B) { test_backends_free(B); }

#define BINOP_F32(op, B)                                                             \
    do {                                                                             \
        struct umbra_builder *b_ = umbra_builder();                                  \
        umbra_val x_ = umbra_load_32(b_, (umbra_ptr){0, 0}),                      \
                  y_ = umbra_load_32(b_, (umbra_ptr){1, 0}), r_ = op(b_, x_, y_); \
        umbra_store_32(b_, (umbra_ptr){2, 0}, r_);                                \
        B = make(b_);                                                                \
    } while (0)

#define BINOP_I32(op, B)                                                              \
    do {                                                                              \
        struct umbra_builder *b_ = umbra_builder();                                   \
        umbra_val x_ = umbra_load_32(b_, (umbra_ptr){0, 0}),                      \
                  y_ = umbra_load_32(b_, (umbra_ptr){1, 0}), r_ = op(b_, x_, y_);  \
        umbra_store_32(b_, (umbra_ptr){2, 0}, r_);                                 \
        B = make(b_);                                                                 \
    } while (0)

#define BINOP_CMP_F32(op, B)                                                \
    do {                                                                    \
        struct umbra_builder *b_ = umbra_builder();                         \
        umbra_val             x_ = umbra_load_32(b_, (umbra_ptr){0, 0}), \
                              y_ = umbra_load_32(b_, (umbra_ptr){1, 0}); \
        umbra_val             r_ = op(b_, x_, y_);                          \
        umbra_store_32(b_, (umbra_ptr){2, 0}, r_);                       \
        B = make(b_);                                                       \
    } while (0)

static void test_f32_ops(void) {
    {
        backends B;
        BINOP_F32(umbra_mul_f32, B);
        for (int bi = 0; bi < 3; bi++) {
            float x[] = {1, 2, 3, 4, 5};
            float y[] = {6, 7, 8, 9, 0}, z[5] = {0};
            if (!run(&B, bi, 5, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=5 * 4},
                         {.ptr=y, .sz=5 * 4},
                         {.ptr=z, .sz=5 * 4},
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
        BINOP_F32(umbra_add_f32, B);
        for (int bi = 0; bi < 3; bi++) {
            float x[] = {1, 2, 3};
            float y[] = {10, 20, 30}, z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=3 * 4},
                         {.ptr=y, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
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
        BINOP_F32(umbra_sub_f32, B);
        for (int bi = 0; bi < 3; bi++) {
            float x[] = {10, 20, 30};
            float y[] = {1, 2, 3}, z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=3 * 4},
                         {.ptr=y, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
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
        BINOP_F32(umbra_div_f32, B);
        for (int bi = 0; bi < 3; bi++) {
            float x[] = {10, 20, 30};
            float y[] = {2, 4, 5}, z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=3 * 4},
                         {.ptr=y, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
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

static void test_i32_ops(void) {
    {
        backends B;
        BINOP_I32(umbra_add_i32, B);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {1, 2, 3};
            int y[] = {10, 20, 30}, z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=3 * 4},
                         {.ptr=y, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
                     })) {
                continue;
            }
            z[0] == 11 here;
            z[1] == 22 here;
            z[2] == 33 here;
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_I32(umbra_sub_i32, B);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {10, 20, 30};
            int y[] = {1, 2, 3}, z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=3 * 4},
                         {.ptr=y, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
                     })) {
                continue;
            }
            z[0] == 9 here;
            z[1] == 18 here;
            z[2] == 27 here;
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_I32(umbra_mul_i32, B);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {2, 3, 4};
            int y[] = {5, 6, 7}, z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=3 * 4},
                         {.ptr=y, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
                     })) {
                continue;
            }
            z[0] == 10 here;
            z[1] == 18 here;
            z[2] == 28 here;
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_I32(umbra_shl_i32, B);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {1, 3, 7};
            int y[] = {1, 2, 3}, z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=3 * 4},
                         {.ptr=y, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
                     })) {
                continue;
            }
            z[0] == 2 here;
            z[1] == 12 here;
            z[2] == 56 here;
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_I32(umbra_shr_u32, B);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {-1, 8, 64};
            int y[] = {1, 1, 3}, z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=3 * 4},
                         {.ptr=y, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
                     })) {
                continue;
            }
            z[0] == (int)(0xffffffffu >> 1) here;
            z[1] == 4 here;
            z[2] == 8 here;
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_I32(umbra_shr_s32, B);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {-8, 8, 64};
            int y[] = {1, 1, 3}, z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=3 * 4},
                         {.ptr=y, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
                     })) {
                continue;
            }
            z[0] == -4 here;
            z[1] == 4 here;
            z[2] == 8 here;
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_I32(umbra_and_i32, B);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {0xff, 0x0f};
            int y[] = {0x0f, 0xff}, z[2] = {0};
            if (!run(&B, bi, 2, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=2 * 4},
                         {.ptr=y, .sz=2 * 4},
                         {.ptr=z, .sz=2 * 4},
                     })) {
                continue;
            }
            z[0] == 0x0f here;
            z[1] == 0x0f here;
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_I32(umbra_or_i32, B);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {0xf0, 0x0f};
            int y[] = {0x0f, 0xf0}, z[2] = {0};
            if (!run(&B, bi, 2, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=2 * 4},
                         {.ptr=y, .sz=2 * 4},
                         {.ptr=z, .sz=2 * 4},
                     })) {
                continue;
            }
            z[0] == 0xff here;
            z[1] == 0xff here;
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_I32(umbra_xor_i32, B);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {0xff, 0xff};
            int y[] = {0x0f, 0xff}, z[2] = {0};
            if (!run(&B, bi, 2, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=2 * 4},
                         {.ptr=y, .sz=2 * 4},
                         {.ptr=z, .sz=2 * 4},
                     })) {
                continue;
            }
            z[0] == 0xf0 here;
            z[1] == 0x00 here;
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
        backends B = make(builder);
        for (int bi = 0; bi < 3; bi++) {
            int cond[] = {-1, 0, -1};
            int va[] = {10, 20, 30};
            int vb[] = {40, 50, 60};
            int z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=cond, .sz=3 * 4},
                         {.ptr=va, .sz=3 * 4},
                         {.ptr=vb, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
                     })) {
                continue;
            }
            z[0] == 10 here;
            z[1] == 50 here;
            z[2] == 30 here;
        }
        cleanup(&B);
    }
}

static void test_cmp_i32(void) {
    {
        backends B;
        BINOP_I32(umbra_eq_i32, B);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {1, 2, 3};
            int y[] = {1, 9, 3}, z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=3 * 4},
                         {.ptr=y, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
                     })) {
                continue;
            }
            z[0] == -1 here;
            z[1] == 0 here;
            z[2] == -1 here;
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_I32(umbra_lt_s32, B);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {1, 5, 3};
            int y[] = {2, 5, 1}, z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=3 * 4},
                         {.ptr=y, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
                     })) {
                continue;
            }
            z[0] == -1 here;
            z[1] == 0 here;
            z[2] == 0 here;
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_I32(umbra_le_s32, B);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {1, 5, 3};
            int y[] = {2, 5, 1}, z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=3 * 4},
                         {.ptr=y, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
                     })) {
                continue;
            }
            z[0] == -1 here;
            z[1] == -1 here;
            z[2] == 0 here;
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_I32(umbra_lt_u32, B);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {1, -1};
            int y[] = {2, 1}, z[2] = {0};
            if (!run(&B, bi, 2, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=2 * 4},
                         {.ptr=y, .sz=2 * 4},
                         {.ptr=z, .sz=2 * 4},
                     })) {
                continue;
            }
            z[0] == -1 here;
            z[1] == 0 here;
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_I32(umbra_le_u32, B);
        for (int bi = 0; bi < 3; bi++) {
            int x[] = {1, 2, -1};
            int y[] = {2, 2, 1}, z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=3 * 4},
                         {.ptr=y, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
                     })) {
                continue;
            }
            z[0] == -1 here;
            z[1] == -1 here;
            z[2] == 0 here;
        }
        cleanup(&B);
    }
}

static void test_cmp_f32(void) {
    {
        backends B;
        BINOP_CMP_F32(umbra_eq_f32, B);
        for (int bi = 0; bi < 3; bi++) {
            float x[] = {1, 2, 3};
            float y[] = {1, 9, 3};
            int   z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=3 * 4},
                         {.ptr=y, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
                     })) {
                continue;
            }
            z[0] == -1 here;
            z[1] == 0 here;
            z[2] == -1 here;
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_CMP_F32(umbra_lt_f32, B);
        for (int bi = 0; bi < 3; bi++) {
            float x[] = {1, 5, 3};
            float y[] = {2, 5, 1};
            int   z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=3 * 4},
                         {.ptr=y, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
                     })) {
                continue;
            }
            z[0] == -1 here;
            z[1] == 0 here;
            z[2] == 0 here;
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_CMP_F32(umbra_le_f32, B);
        for (int bi = 0; bi < 3; bi++) {
            float x[] = {1, 5, 3};
            float y[] = {2, 5, 1};
            int   z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=3 * 4},
                         {.ptr=y, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
                     })) {
                continue;
            }
            z[0] == -1 here;
            z[1] == -1 here;
            z[2] == 0 here;
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_CMP_F32(umbra_gt_f32, B);
        for (int bi = 0; bi < 3; bi++) {
            float x[] = {3, 5, 1};
            float y[] = {2, 5, 3};
            int   z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=3 * 4},
                         {.ptr=y, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
                     })) {
                continue;
            }
            z[0] == -1 here;
            z[1] == 0 here;
            z[2] == 0 here;
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_CMP_F32(umbra_ge_f32, B);
        for (int bi = 0; bi < 3; bi++) {
            float x[] = {3, 5, 1};
            float y[] = {2, 5, 3};
            int   z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=3 * 4},
                         {.ptr=y, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
                     })) {
                continue;
            }
            z[0] == -1 here;
            z[1] == -1 here;
            z[2] == 0 here;
        }
        cleanup(&B);
    }
}

static void test_imm(void) {
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val v = umbra_imm_i32(builder, 42);
        umbra_store_32(builder, (umbra_ptr){0, 0}, v);
        backends B = make(builder);
        for (int bi = 0; bi < 3; bi++) {
            int z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=z, .sz=3 * 4},
                     })) {
                continue;
            }
            z[0] == 42 here;
            z[1] == 42 here;
            z[2] == 42 here;
        }
        cleanup(&B);
    }
}

static void test_fma_f32(void) {
    struct umbra_builder *builder = umbra_builder();
    umbra_val x = umbra_load_32(builder, (umbra_ptr){0, 0}),
              y = umbra_load_32(builder, (umbra_ptr){1, 0}),
              w = umbra_load_32(builder, (umbra_ptr){2, 0}),
              m = umbra_mul_f32(builder, x, y), r = umbra_add_f32(builder, m, w);
    umbra_store_32(builder, (umbra_ptr){3, 0}, r);
    backends B = make(builder);
    for (int bi = 0; bi < 3; bi++) {
        float a[] = {2, 3}, c[] = {4, 5};
        float d[] = {10, 20}, z[2] = {0};
        if (!run(&B, bi, 2, 1,
                 (umbra_buf[]){
                     {.ptr=a, .sz=2 * 4},
                     {.ptr=c, .sz=2 * 4},
                     {.ptr=d, .sz=2 * 4},
                     {.ptr=z, .sz=2 * 4},
                 })) {
            continue;
        }
        equiv(z[0], 18) here;
        equiv(z[1], 35) here;
    }
    cleanup(&B);
}

static void test_min_max_sqrt_f32(void) {
    {
        backends B;
        BINOP_F32(umbra_min_f32, B);
        for (int bi = 0; bi < 3; bi++) {
            float x[] = {5, 1, 3};
            float y[] = {2, 4, 3}, z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=3 * 4},
                         {.ptr=y, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
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
        BINOP_F32(umbra_max_f32, B);
        for (int bi = 0; bi < 3; bi++) {
            float x[] = {5, 1, 3};
            float y[] = {2, 4, 3}, z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=3 * 4},
                         {.ptr=y, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
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
        backends B = make(builder);
        for (int bi = 0; bi < 3; bi++) {
            float a[] = {4, 9, 16}, z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=a, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
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

static void test_abs_f32(void) {
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val             x = umbra_load_32(builder, (umbra_ptr){0, 0}),
                              r = umbra_abs_f32(builder, x);
        umbra_store_32(builder, (umbra_ptr){1, 0}, r);
        backends B = make(builder);
        for (int bi = 0; bi < 3; bi++) {
            float a[] = {-1.5f, 2.5f, -0.0f};
            float z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=a, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
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
                              r = umbra_neg_f32(builder, x);
        umbra_store_32(builder, (umbra_ptr){1, 0}, r);
        backends B = make(builder);
        for (int bi = 0; bi < 3; bi++) {
            float a[] = {-1.5f, 2.5f, 0.0f};
            float z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=a, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
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

static void test_round_floor_ceil(void) {
    float src[] = {1.3f, 1.5f, -1.5f, -2.7f};

#define RFC(op, e0, e1, e2, e3, as_int)                                     \
    do {                                                                    \
        struct umbra_builder *b_ = umbra_builder();                         \
        umbra_val             x_ = umbra_load_32(b_, (umbra_ptr){0, 0}); \
        umbra_val             r_ = op(b_, x_);                              \
        umbra_store_32(b_, (umbra_ptr){1, 0}, r_);                       \
        backends B_ = make(b_);                                          \
        for (int bi_ = 0; bi_ < 3; bi_++) {                                 \
            float s_[4];                                                    \
            __builtin_memcpy(s_, src, 16);                                  \
            int d_[4] = {0};                                                \
            if (!run(&B_, bi_, 4, 1,                                        \
                     (umbra_buf[]){                                         \
                         {.ptr=s_, .sz=16},                                          \
                         {.ptr=d_, .sz=16},                                          \
                     })) {                                                  \
                continue;                                                   \
            }                                                               \
            if (as_int) {                                                   \
                d_[0] == (int)(e0) here;                                  \
                d_[1] == (int)(e1) here;                                  \
                d_[2] == (int)(e2) here;                                  \
                d_[3] == (int)(e3) here;                                  \
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
    backends B;
    BINOP_F32(umbra_add_f32, B);
    for (int bi = 0; bi < 3; bi++) {
        float x[100], y[100], z[100];
        for (int i = 0; i < 100; i++) {
            x[i] = (float)i;
            y[i] = (float)(100 - i);
        }
        if (!run(&B, bi, 100, 1,
                 (umbra_buf[]){
                     {.ptr=x, .sz=100 * 4},
                     {.ptr=y, .sz=100 * 4},
                     {.ptr=z, .sz=100 * 4},
                 })) {
            continue;
        }
        for (int i = 0; i < 100; i++) { equiv(z[i], 100) here; }
    }
    cleanup(&B);
}

static void test_convert(void) {
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val             x = umbra_load_32(builder, (umbra_ptr){0, 0});
        umbra_val             r = umbra_f32_from_i32(builder, x);
        umbra_store_32(builder, (umbra_ptr){1, 0}, r);
        backends B = make(builder);
        for (int bi = 0; bi < 3; bi++) {
            int   a[] = {1, 255, -3};
            float z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=a, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
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
        backends B = make(builder);
        for (int bi = 0; bi < 3; bi++) {
            float a[] = {1.9f, 255.0f, -3.7f};
            int   z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=a, .sz=3 * 4},
                         {.ptr=z, .sz=3 * 4},
                     })) {
                continue;
            }
            z[0] == 1 here;
            z[1] == 255 here;
            z[2] == -3 here;
        }
        cleanup(&B);
    }
}

static void test_dedup(void) {
    struct umbra_builder *builder = umbra_builder();
    umbra_val v1 = umbra_imm_i32(builder, 42), v2 = umbra_imm_i32(builder, 42);
    (v1).bits == (v2).bits here;
    umbra_val c = umbra_imm_i32(builder, 99);
    (v1).bits != (c).bits here;
    umbra_val x = umbra_load_32(builder, (umbra_ptr){0, 0}),
              s1 = umbra_add_i32(builder, x, v1), s2 = umbra_add_i32(builder, x, v1);
    (s1).bits == (s2).bits here;
    umbra_builder_free(builder);
}

static void test_constprop(void) {
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val x = umbra_imm_i32(builder, 3),
                  y = umbra_imm_i32(builder, 5), s = umbra_add_i32(builder, x, y);
        umbra_store_32(builder, (umbra_ptr){0, 0}, s);
        backends B = make(builder);
        for (int bi = 0; bi < 3; bi++) {
            int z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=z, .sz=3 * 4},
                     })) {
                continue;
            }
            z[0] == 8 here;
            z[1] == 8 here;
            z[2] == 8 here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val a = umbra_imm_f32(builder, 2.0f), y = umbra_imm_f32(builder, 3.0f),
                  s = umbra_mul_f32(builder, a, y);
        umbra_store_32(builder, (umbra_ptr){0, 0}, s);
        backends B = make(builder);
        for (int bi = 0; bi < 3; bi++) {
            float z[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=z, .sz=3 * 4},
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

static void test_strength_reduction(void) {
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val             x = umbra_load_32(builder, (umbra_ptr){0, 0}),
                  z = umbra_imm_i32(builder, 0), s = umbra_add_i32(builder, x, z);
        (s).bits == (x).bits here;
        umbra_builder_free(builder);
    }
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val             x = umbra_load_32(builder, (umbra_ptr){0, 0}),
                  one = umbra_imm_i32(builder, 1), s = umbra_mul_i32(builder, x, one);
        (s).bits == (x).bits here;
        umbra_builder_free(builder);
    }
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val             x = umbra_load_32(builder, (umbra_ptr){0, 0}),
                              eight = umbra_imm_i32(builder, 8),
                              s = umbra_mul_i32(builder, x, eight);
        umbra_store_32(builder, (umbra_ptr){1, 0}, s);
        backends B = make(builder);
        for (int bi = 0; bi < 3; bi++) {
            int32_t a[] = {1, 2, 3, 4, 5}, c[5] = {0};
            if (!run(&B, bi, 5, 1,
                     (umbra_buf[]){
                         {.ptr=a, .sz=5 * 4},
                         {.ptr=c, .sz=5 * 4},
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
        backends B = make(builder);
        for (int bi = 0; bi < 3; bi++) {
            int32_t a[] = {1, 2, 3}, c[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=a, .sz=3 * 4},
                         {.ptr=c, .sz=3 * 4},
                     })) {
                continue;
            }
            for (int i = 0; i < 3; i++) { (c[i] == 0) here; }
        }
        cleanup(&B);
    }
}

static void test_zero_imm(void) {
    struct umbra_builder *builder = umbra_builder();
    umbra_val             zero = umbra_imm_i32(builder, 0);
    (zero).bits == 0 here;
    umbra_val             x = umbra_load_32(builder, (umbra_ptr){0, 0}),
              r = umbra_eq_i32(builder, x, zero);
    umbra_store_32(builder, (umbra_ptr){1, 0}, r);
    backends B = make(builder);
    for (int bi = 0; bi < 3; bi++) {
        int a[] = {0, 1, 0}, z[3] = {0};
        if (!run(&B, bi, 3, 1,
                 (umbra_buf[]){
                     {.ptr=a, .sz=3 * 4},
                     {.ptr=z, .sz=3 * 4},
                 })) {
            continue;
        }
        z[0] == -1 here;
        z[1] == 0 here;
        z[2] == -1 here;
    }
    cleanup(&B);
}

static void test_late_imm_identity(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_val             z1 = umbra_imm_i32(b, 1);
    umbra_val             zm = umbra_imm_i32(b, -1);
    (z1).bits > (x).bits here;
    (zm).bits > (x).bits here;

    umbra_mul_i32(b, x, z1).bits == (x).bits here;
    umbra_and_i32(b, x, zm).bits == (x).bits here;
    umbra_or_i32(b, x, zm).bits == (zm).bits here;
    umbra_sel_i32(b, zm, x, z1).bits == (x).bits here;
    umbra_eq_i32(b, x, x).bits == umbra_imm_i32(b, -1).bits here;
    umbra_div_f32(b, x, umbra_imm_f32(b, 1.0f)).bits == (x).bits here;
    umbra_sub_f32(b, x, umbra_imm_f32(b, 0.0f)).bits == (x).bits here;
    umbra_builder_free(b);
}

static void test_abs_peepholes(void) {
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             direct = umbra_abs_f32(b, x);
        umbra_val             neg_x = umbra_neg_f32(b, x);
        umbra_val             mask = umbra_imm_i32(b, 0x7fffffff);

        umbra_max_f32(b, x, neg_x).bits == (direct).bits here;
        umbra_max_f32(b, neg_x, x).bits == (direct).bits here;
        umbra_and_i32(b, x, mask).bits == (direct).bits here;
        umbra_and_i32(b, mask, x).bits == (direct).bits here;
        umbra_builder_free(b);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             mask = umbra_imm_i32(b, 0x7fffffff);
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        (mask).bits < (x).bits here;
        umbra_val direct = umbra_abs_f32(b, x);
        umbra_and_i32(b, x, mask).bits == (direct).bits here;
        umbra_builder_free(b);
    }
}

static void test_load_8x4(void) {
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
    backends B = make(builder);
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
                     {.ptr=src, .sz=3 * 4},
                     {.ptr=rr, .sz=3 * 4},
                     {.ptr=gg, .sz=3 * 4},
                     {.ptr=b_, .sz=3 * 4},
                     {.ptr=aa, .sz=3 * 4},
                 })) {
            continue;
        }
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
    cleanup(&B);
}

static void test_store_8x4(void) {
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
    backends B = make(builder);
    for (int bi = 0; bi < 3; bi++) {
        int32_t  rr[] = {0xDD, 0x44, 0x00};
        int32_t  gg[] = {0xCC, 0x33, 0xFF};
        int32_t  b_[] = {0xBB, 0x22, 0x00};
        int32_t  aa[] = {0xAA, 0x11, 0xFF};
        uint32_t dst[3] = {0};
        if (!run(&B, bi, 3, 1,
                 (umbra_buf[]){
                     {.ptr=rr, .sz=3 * 4},
                     {.ptr=gg, .sz=3 * 4},
                     {.ptr=b_, .sz=3 * 4},
                     {.ptr=aa, .sz=3 * 4},
                     {.ptr=dst, .sz=3 * 4},
                 })) {
            continue;
        }
        dst[0] == 0xAABBCCDDu here;
        dst[1] == 0x11223344u here;
        dst[2] == 0xFF00FF00u here;
    }
    cleanup(&B);
}

static void test_srcover(void) {
    struct umbra_builder *builder = build_srcover();
    backends              B = make(builder);
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
                     {.ptr=src_px, .sz=3 * 4},
                     {.ptr=dst_r, .sz=3 * 2},
                     {.ptr=dst_g, .sz=3 * 2},
                     {.ptr=dst_b, .sz=3 * 2},
                     {.ptr=dst_a, .sz=3 * 2},
                 })) {
            continue;
        }
        (float)dst_r[0] > 0.28f - tol && (float)dst_r[0] < 0.34f + tol here;
    }
    cleanup(&B);
}

static void test_hash_quality(void) {
    struct umbra_builder *builder = umbra_builder();
    enum { N = 1000 };
    int ids[N];
    for (int i = 0; i < N; i++) { ids[i] = umbra_imm_i32(builder, i).bits; }
    for (int i = 0; i < N; i++) { (umbra_imm_i32(builder, i).bits == ids[i]) here; }
    for (int i = 1; i < N; i++) { (ids[i] != ids[i - 1]) here; }
    umbra_val x = umbra_load_32(builder, (umbra_ptr){0, 0});
    for (int i = 0; i < N; i++) {
        umbra_val c = umbra_imm_i32(builder, i);
        umbra_val sum = umbra_add_i32(builder, x, c);
        umbra_val sum2 = umbra_add_i32(builder, x, c);
        (sum).bits == (sum2).bits here;
    }
    umbra_builder_free(builder);
}

static void test_narrow_16(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val             v = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_store_16(b, (umbra_ptr){1, 0}, umbra_i16_from_i32(b, v));
    backends B = make(b);
    for (int bi = 0; bi < 3; bi++) {
        int      src[] = {0x00010002, 0x00030004, 0x00050006};
        uint16_t dst[3] = {0};
        if (!run(&B, bi, 3, 1,
                 (umbra_buf[]){
                     {.ptr=src, .sz=3 * 4},
                     {.ptr=dst, .sz=3 * 2},
                 })) {
            continue;
        }
        dst[0] == 0x0002 here;
        dst[1] == 0x0004 here;
        dst[2] == 0x0006 here;
    }
    cleanup(&B);
}

static void test_mixed_ptr_sizes(void) {
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val             a = umbra_load_32(builder, (umbra_ptr){0, 0});
        umbra_val r = umbra_add_i32(builder, a, umbra_imm_i32(builder, 1));
        umbra_store_32(builder, (umbra_ptr){1, 0}, r);
        backends B = make(builder);
        for (int bi = 0; bi < 3; bi++) {
            uint32_t x[] = {10, 20, 30};
            uint32_t y[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=3 * 4},
                         {.ptr=y, .sz=3 * 4},
                     })) {
                continue;
            }
            y[0] == 11 here;
            y[1] == 21 here;
            y[2] == 31 here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val a = umbra_f32_from_f16(builder,
                                      umbra_load_16(builder, (umbra_ptr){0, 0}));
        umbra_val r = umbra_add_f32(builder, a, umbra_imm_f32(builder, 1.0f));
        umbra_store_16(builder, (umbra_ptr){1, 0}, umbra_f16_from_f32(builder, r));
        backends B = make(builder);
        for (int bi = 0; bi < 3; bi++) {
            __fp16 x[] = {1, 2, 3}, y[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=3 * 2},
                         {.ptr=y, .sz=3 * 2},
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

static void test_n9(void) {
    {
        backends B;
        BINOP_F32(umbra_add_f32, B);
        for (int bi = 0; bi < 3; bi++) {
            float x[9], y[9], z[9] = {0};
            for (int i = 0; i < 9; i++) {
                x[i] = (float)(i + 1);
                y[i] = (float)(10 * (i + 1));
            }
            if (!run(&B, bi, 9, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=9 * 4},
                         {.ptr=y, .sz=9 * 4},
                         {.ptr=z, .sz=9 * 4},
                     })) {
                continue;
            }
            for (int i = 0; i < 9; i++) { equiv(z[i], x[i] + y[i]) here; }
        }
        cleanup(&B);
    }
    {
        backends B;
        BINOP_I32(umbra_mul_i32, B);
        for (int bi = 0; bi < 3; bi++) {
            int x[9], y[9], z[9] = {0};
            for (int i = 0; i < 9; i++) {
                x[i] = i + 1;
                y[i] = i + 2;
            }
            if (!run(&B, bi, 9, 1,
                     (umbra_buf[]){
                         {.ptr=x, .sz=9 * 4},
                         {.ptr=y, .sz=9 * 4},
                         {.ptr=z, .sz=9 * 4},
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
        backends B = make(builder);
        for (int bi = 0; bi < 3; bi++) {
            uint32_t src[9], dst[9] = {0};
            for (int i = 0; i < 9; i++) { src[i] = 0xAABBCC00u + (uint32_t)i; }
            if (!run(&B, bi, 9, 1,
                     (umbra_buf[]){
                         {.ptr=src, .sz=9 * 4},
                         {.ptr=dst, .sz=9 * 4},
                     })) {
                continue;
            }
            for (int i = 0; i < 9; i++) { (dst[i] == src[i]) here; }
        }
        cleanup(&B);
    }
}

static void test_preamble_pair_alias(void) {
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
    backends B = make(builder);

    for (int bi = 0; bi < 3; bi++) {
        float in[16], out[16] = {0};
        for (int i = 0; i < 16; i++) { in[i] = (float)(i + 1); }
        if (!run(&B, bi, 16, 1,
                 (umbra_buf[]){
                     {.ptr=in, .sz=16 * 4},
                     {.ptr=out, .sz=16 * 4},
                 })) {
            continue;
        }
        float ref[16];
        umbra_program_queue(B.p[0], 0, 0, 16, 1,
                            (umbra_buf[]){
                                {.ptr=in, .sz=16 * 4},
                                {.ptr=ref, .sz=16 * 4},
                            });
        for (int i = 0; i < 16; i++) { equiv(out[i], ref[i]) here; }
    }
    cleanup(&B);
}

static void test_gather_clamp(void) {
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val             idx = umbra_load_32(builder, (umbra_ptr){0, 0}),
                              val = umbra_gather_32(builder, (umbra_ptr){1, 0}, idx);
        umbra_store_32(builder, (umbra_ptr){2, 0}, val);
        backends B = make(builder);
        for (int bi = 0; bi < 3; bi++) {
            int32_t indices[4] = {-5, 0, 2, 100};
            int32_t src[3] = {10, 20, 30};
            int32_t dst[4] = {0};
            if (!run(&B, bi, 4, 1,
                     (umbra_buf[]){
                         {.ptr=indices, .sz=sizeof indices},
                         {.ptr=src, .sz=sizeof src},
                         {.ptr=dst, .sz=sizeof dst},
                     })) {
                continue;
            }
            dst[0] == 0 here;
            dst[1] == 10 here;
            dst[2] == 30 here;
            dst[3] == 0 here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val             idx = umbra_load_32(builder, (umbra_ptr){0, 0});
        umbra_val val = umbra_i32_from_s16(builder,
                                        umbra_gather_16(builder, (umbra_ptr){1, 0}, idx));
        umbra_store_32(builder, (umbra_ptr){2, 0}, val);
        backends B = make(builder);
        for (int bi = 0; bi < 3; bi++) {
            int32_t indices[4] = {-1, 1, 3, 999};
            int16_t src[3] = {100, 200, 300};
            int32_t dst[4] = {0};
            if (!run(&B, bi, 4, 1,
                     (umbra_buf[]){
                         {.ptr=indices, .sz=sizeof indices},
                         {.ptr=src, .sz=sizeof src},
                         {.ptr=dst, .sz=sizeof dst},
                     })) {
                continue;
            }
            dst[0] == 0 here;
            dst[1] == 200 here;
            dst[2] == 0 here;
            dst[3] == 0 here;
        }
        cleanup(&B);
    }
}

static void test_gather_clamp_zero_sz(void) {
    // gather_uniform with negative index → clamped to 0.
    struct umbra_builder *b = umbra_builder();
    umbra_val             ix = umbra_uniform_32(b, (umbra_ptr){0, 0}, 0);
    umbra_val             v = umbra_gather_32(b, (umbra_ptr){1, 0}, ix);
    umbra_store_32(b, (umbra_ptr){2, 0}, v);
    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t neg_idx[] = {-10};
        int32_t src[3] = {100, 200, 300};
        int32_t dst[1] = {0};
        if (!run(&B, bi, 1, 1, (umbra_buf[]){
            {.ptr=neg_idx, .sz=4},
            {.ptr=src, .sz=sizeof src},
            {.ptr=dst, .sz=4},
        })) { continue; }
        dst[0] == 0 here;
    }

    // gather_uniform with over-range index → OOB returns 0.
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t big_idx[] = {999};
        int32_t src[3] = {100, 200, 300};
        int32_t dst[1] = {0};
        if (!run(&B, bi, 1, 1, (umbra_buf[]){
            {.ptr=big_idx, .sz=4},
            {.ptr=src, .sz=sizeof src},
            {.ptr=dst, .sz=4},
        })) { continue; }
        dst[0] == 0 here;
    }
    cleanup(&B);
}

static void test_offset_load_store(void) {
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val             ix = umbra_x(builder);
        umbra_val             off = umbra_uniform_32(builder, (umbra_ptr){1, 0}, 0);
        umbra_val             ixo = umbra_add_i32(builder, ix, off);
        umbra_val val = umbra_i32_from_s16(builder,
                                        umbra_gather_16(builder, (umbra_ptr){0, 0}, ixo));
        umbra_store_32(builder, (umbra_ptr){2, 0}, val);
        backends B = make(builder);
        for (int bi = 0; bi < 3; bi++) {
            int16_t src[16] = {
                10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
            };
            int32_t uni[1] = {4};
            int32_t dst[8] = {0};
            if (!run(&B, bi, 8, 1,
                     (umbra_buf[]){
                         {.ptr=src, .sz=sizeof src},
                         {.ptr=uni, .sz=sizeof uni, .read_only=1},
                         {.ptr=dst, .sz=sizeof dst},
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
        backends B = make(builder);
        for (int bi = 0; bi < 3; bi++) {
            int32_t src[16] = {
                100, 101, 102, 103, 104, 105, 106, 107,
                108, 109, 110, 111, 112, 113, 114, 115,
            };
            int32_t uni[1] = {3};
            int32_t dst[8] = {0};
            if (!run(&B, bi, 8, 1,
                     (umbra_buf[]){
                         {.ptr=src, .sz=sizeof src},
                         {.ptr=uni, .sz=sizeof uni, .read_only=1},
                         {.ptr=dst, .sz=sizeof dst},
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
        backends B = make(builder);
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
                         {.ptr=src, .sz=sizeof src},
                         {.ptr=uni, .sz=sizeof uni, .read_only=1},
                         {.ptr=dst, .sz=sizeof dst},
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

static void test_shift_imm(void) {
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val             x = umbra_load_32(builder, (umbra_ptr){0, 0});
        umbra_val r = umbra_shl_i32(builder, x, umbra_imm_i32(builder, 8));
        umbra_store_32(builder, (umbra_ptr){1, 0}, r);
        backends B = make(builder);
        for (int bi = 0; bi < 3; bi++) {
            uint32_t src[] = {1, 2, 3, 0xff};
            uint32_t dst[4] = {0};
            if (!run(&B, bi, 4, 1,
                     (umbra_buf[]){
                         {.ptr=src, .sz=4 * 4},
                         {.ptr=dst, .sz=4 * 4},
                     })) {
                continue;
            }
            dst[0] == 0x100u here;
            dst[1] == 0x200u here;
            dst[2] == 0x300u here;
            dst[3] == 0xff00u here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val             x = umbra_load_32(builder, (umbra_ptr){0, 0});
        umbra_val r = umbra_shr_u32(builder, x, umbra_imm_i32(builder, 8));
        umbra_store_32(builder, (umbra_ptr){1, 0}, r);
        backends B = make(builder);
        for (int bi = 0; bi < 3; bi++) {
            uint32_t src[] = {0x100, 0x200, 0xff00};
            uint32_t dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=src, .sz=3 * 4},
                         {.ptr=dst, .sz=3 * 4},
                     })) {
                continue;
            }
            dst[0] == 1 here;
            dst[1] == 2 here;
            dst[2] == 0xff here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *builder = umbra_builder();
        umbra_val             x = umbra_load_32(builder, (umbra_ptr){0, 0});
        umbra_val r = umbra_shr_s32(builder, x, umbra_imm_i32(builder, 4));
        umbra_store_32(builder, (umbra_ptr){1, 0}, r);
        backends B = make(builder);
        for (int bi = 0; bi < 3; bi++) {
            uint32_t src[] = {0x80, 0xfffffff0u};
            uint32_t dst[2] = {0};
            if (!run(&B, bi, 2, 1,
                     (umbra_buf[]){
                         {.ptr=src, .sz=2 * 4},
                         {.ptr=dst, .sz=2 * 4},
                     })) {
                continue;
            }
            dst[0] == 8 here;
            dst[1] == 0xffffffffu here;
        }
        cleanup(&B);
    }
}

static void test_pack_channels(void) {
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
    backends B = make(builder);
    for (int bi = 0; bi < 3; bi++) {
        uint32_t rr[] = {0xAA, 0x11, 0xFF};
        uint32_t gg[] = {0xBB, 0x22, 0x00};
        uint32_t bb_[] = {0xCC, 0x33, 0xFF};
        uint32_t aa[] = {0xDD, 0x44, 0x00};
        uint32_t dst[3] = {0};
        if (!run(&B, bi, 3, 1,
                 (umbra_buf[]){
                     {.ptr=rr, .sz=3 * 4},
                     {.ptr=gg, .sz=3 * 4},
                     {.ptr=bb_, .sz=3 * 4},
                     {.ptr=aa, .sz=3 * 4},
                     {.ptr=dst, .sz=3 * 4},
                 })) {
            continue;
        }
        dst[0] == 0xDDCCBBAAu here;
        dst[1] == 0x44332211u here;
        dst[2] == 0x00FF00FFu here;
    }
    cleanup(&B);
}

static void test_gather_deref_large(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val             idx = umbra_load_32(b, (umbra_ptr){0, 0});
    int                   off = umbra_reserve_ptr(b);
    umbra_ptr             src = umbra_deref_ptr(b, (umbra_ptr){1, 0}, off);
    umbra_val             val = umbra_i32_from_s16(b, umbra_gather_16(b, src, idx));
    umbra_store_32(b, (umbra_ptr){2, 0}, val);
    backends B = make(b);

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
                     {.ptr=indices, .sz=sizeof indices},
                     {.ptr=uni, .sz=sizeof uni_, .read_only=1},
                     {.ptr=dst, .sz=sizeof dst},
                 })) {
            continue;
        }
        dst[0] == 10 here;
        dst[1] == 20 here;
        dst[2] == 30 here;
        dst[3] == 42 here;
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
        backends B = make(b);
        for (int bi = 0; bi < 3; bi++) {
            float src[] = {10, 20, 30}, dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=src, .sz=3 * 4},
                         {.ptr=dst, .sz=3 * 4},
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
        backends B = make(b);
        for (int bi = 0; bi < 3; bi++) {
            int src[] = {10, 20, 30}, dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=src, .sz=3 * 4},
                         {.ptr=dst, .sz=3 * 4},
                     })) {
                continue;
            }
            dst[0] == 5 here;
            dst[1] == 15 here;
            dst[2] == 25 here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             r = umbra_or_i32(b, x, umbra_imm_i32(b, 0xf0));
        umbra_store_32(b, (umbra_ptr){1, 0}, r);
        backends B = make(b);
        for (int bi = 0; bi < 3; bi++) {
            int src[] = {0x0f, 0x00, 0xff}, dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=src, .sz=3 * 4},
                         {.ptr=dst, .sz=3 * 4},
                     })) {
                continue;
            }
            dst[0] == 0xff here;
            dst[1] == 0xf0 here;
            dst[2] == 0xff here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             r = umbra_eq_f32(b, x, umbra_imm_f32(b, 2.0f));
        umbra_store_32(b, (umbra_ptr){1, 0}, r);
        backends B = make(b);
        for (int bi = 0; bi < 3; bi++) {
            float src[] = {1, 2, 3};
            int   dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=src, .sz=3 * 4},
                         {.ptr=dst, .sz=3 * 4},
                     })) {
                continue;
            }
            dst[0] == 0 here;
            dst[1] == -1 here;
            dst[2] == 0 here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             r = umbra_le_f32(b, x, umbra_imm_f32(b, 2.0f));
        umbra_store_32(b, (umbra_ptr){1, 0}, r);
        backends B = make(b);
        for (int bi = 0; bi < 3; bi++) {
            float src[] = {1, 2, 3};
            int   dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=src, .sz=3 * 4},
                         {.ptr=dst, .sz=3 * 4},
                     })) {
                continue;
            }
            dst[0] == -1 here;
            dst[1] == -1 here;
            dst[2] == 0 here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             r = umbra_lt_s32(b, x, umbra_imm_i32(b, 5));
        umbra_store_32(b, (umbra_ptr){1, 0}, r);
        backends B = make(b);
        for (int bi = 0; bi < 3; bi++) {
            int src[] = {3, 5, 7}, dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=src, .sz=3 * 4},
                         {.ptr=dst, .sz=3 * 4},
                     })) {
                continue;
            }
            dst[0] == -1 here;
            dst[1] == 0 here;
            dst[2] == 0 here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             r = umbra_le_s32(b, x, umbra_imm_i32(b, 5));
        umbra_store_32(b, (umbra_ptr){1, 0}, r);
        backends B = make(b);
        for (int bi = 0; bi < 3; bi++) {
            int src[] = {3, 5, 7}, dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=src, .sz=3 * 4},
                         {.ptr=dst, .sz=3 * 4},
                     })) {
                continue;
            }
            dst[0] == -1 here;
            dst[1] == -1 here;
            dst[2] == 0 here;
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
        backends B = make(b);
        for (int bi = 0; bi < 3; bi++) {
            int src[] = {-1, 0, 1}, dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=src, .sz=3 * 4},
                         {.ptr=dst, .sz=3 * 4},
                     })) {
                continue;
            }
            dst[0] == -1 here;
            dst[1] == 0 here;
            dst[2] == 0 here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             r = umbra_le_f32(b, x, umbra_imm_f32(b, 0.0f));
        umbra_store_32(b, (umbra_ptr){1, 0}, r);
        backends B = make(b);
        for (int bi = 0; bi < 3; bi++) {
            float src[] = {-1, 0, 1};
            int   dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=src, .sz=3 * 4},
                         {.ptr=dst, .sz=3 * 4},
                     })) {
                continue;
            }
            dst[0] == -1 here;
            dst[1] == -1 here;
            dst[2] == 0 here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             r = umbra_eq_f32(b, x, umbra_imm_f32(b, 0.0f));
        umbra_store_32(b, (umbra_ptr){1, 0}, r);
        backends B = make(b);
        for (int bi = 0; bi < 3; bi++) {
            float src[] = {-1, 0, 1};
            int   dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=src, .sz=3 * 4},
                         {.ptr=dst, .sz=3 * 4},
                     })) {
                continue;
            }
            dst[0] == 0 here;
            dst[1] == -1 here;
            dst[2] == 0 here;
        }
        cleanup(&B);
    }
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             x = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             r = umbra_le_s32(b, x, umbra_imm_i32(b, 0));
        umbra_store_32(b, (umbra_ptr){1, 0}, r);
        backends B = make(b);
        for (int bi = 0; bi < 3; bi++) {
            int src[] = {-1, 0, 1}, dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=src, .sz=3 * 4},
                         {.ptr=dst, .sz=3 * 4},
                     })) {
                continue;
            }
            dst[0] == -1 here;
            dst[1] == -1 here;
            dst[2] == 0 here;
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
        backends B = make(b);
        for (int bi = 0; bi < 3; bi++) {
            int dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=dst, .sz=3 * 4},
                     })) {
                continue;
            }
            dst[0] == patterns[pi] here;
            dst[1] == patterns[pi] here;
            dst[2] == patterns[pi] here;
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
        backends B = make(b);
        for (int bi = 0; bi < 3; bi++) {
            float a[] = {2, 3}, c[] = {5, 6}, d[] = {100, 200};
            float dst[4] = {0};
            if (!run(&B, bi, 2, 1,
                     (umbra_buf[]){
                         {.ptr=a, .sz=2 * 4},
                         {.ptr=c, .sz=2 * 4},
                         {.ptr=d, .sz=2 * 4},
                         {.ptr=dst, .sz=2 * 4},
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
        backends B = make(b);
        for (int bi = 0; bi < 3; bi++) {
            float a[] = {2, 3}, c[] = {5, 6}, d[] = {100, 200};
            float dst[4] = {0};
            if (!run(&B, bi, 2, 1,
                     (umbra_buf[]){
                         {.ptr=a, .sz=2 * 4},
                         {.ptr=c, .sz=2 * 4},
                         {.ptr=d, .sz=2 * 4},
                         {.ptr=dst, .sz=2 * 4},
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
        backends B = make(b);
        for (int bi = 0; bi < 3; bi++) {
            int m[] = {-1, 0}, t[] = {10, 20}, f[] = {30, 40};
            int dst[2] = {0};
            if (!run(&B, bi, 2, 1,
                     (umbra_buf[]){
                         {.ptr=m, .sz=2 * 4},
                         {.ptr=t, .sz=2 * 4},
                         {.ptr=f, .sz=2 * 4},
                         {.ptr=dst, .sz=2 * 4},
                     })) {
                continue;
            }
            dst[0] == 10 + (-1) + 10 here;
            dst[1] == 40 + 0 + 20 here;
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
        backends B = make(b);
        for (int bi = 0; bi < 3; bi++) {
            int ba[] = {0x0F, 0xFF};
            int vl[] = {0x0A, 0x0B};
            int dst[2] = {0}, dst2[2] = {0};
            if (!run(&B, bi, 2, 1,
                     (umbra_buf[]){
                         {.ptr=ba, .sz=2 * 4},
                         {.ptr=vl, .sz=2 * 4},
                         {.ptr=dst, .sz=2 * 4},
                         {.ptr=dst2, .sz=2 * 4},
                     })) {
                continue;
            }
            dst[0] == (0x0F | (0x0A << 8)) here;
            dst[1] == (0xFF | (0x0B << 8)) here;
            dst2[0] == 0x10 here;
            dst2[1] == 0x100 here;
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
    backends B = make(b);
    for (int bi = 0; bi < 3; bi++) {
        float a[] = {2, 3, 4}, c[] = {5, 6, 7}, d[] = {100, 200, 300}, dst[3] = {0};
        if (!run(&B, bi, 3, 1,
                 (umbra_buf[]){
                     {.ptr=a, .sz=3 * 4},
                     {.ptr=c, .sz=3 * 4},
                     {.ptr=d, .sz=3 * 4},
                     {.ptr=dst, .sz=3 * 4},
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
        backends B = make(b);
        for (int bi = 0; bi < 3; bi++) {
            int src[] = {0, 0, 0}, dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=src, .sz=3 * 4},
                         {.ptr=dst, .sz=3 * 4},
                     })) {
                continue;
            }
            dst[0] == patterns[pi] here;
            dst[1] == patterns[pi] here;
            dst[2] == patterns[pi] here;
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
    backends B = make(b);
    for (int bi = 0; bi < 3; bi++) {
        uint32_t src[] = {0x00010002, 0x00030004, 0x00050006};
        int      dst[3] = {0};
        if (!run(&B, bi, 3, 1,
                 (umbra_buf[]){
                     {.ptr=src, .sz=sizeof src},
                     {.ptr=dst, .sz=3 * 4},
                 })) {
            continue;
        }
        dst[0] == (int)(0x00010002 + 0x0002) here;
        dst[1] == (int)(0x00030004 + 0x0001) here;
        dst[2] == (int)(0x00050006 + 0x0004) here;
    }
    cleanup(&B);
}

static void test_uni_16(void) {
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val idx = umbra_uniform_32(b, (umbra_ptr){0, 0}, 0);
        umbra_val v = umbra_i32_from_u16(b, umbra_gather_16(b, (umbra_ptr){1, 0}, idx));
        umbra_store_32(b, (umbra_ptr){2, 0}, v);
        backends B = make(b);
        for (int bi = 0; bi < 3; bi++) {
            int      idx_val[] = {1};
            uint16_t src[] = {100, 200, 300, 400};
            int      dst[3] = {0};
            if (!run(&B, bi, 3, 1,
                     (umbra_buf[]){
                         {.ptr=idx_val, .sz=4},
                         {.ptr=src, .sz=8},
                         {.ptr=dst, .sz=3 * 4},
                     })) {
                continue;
            }
            dst[0] == 200 here;
            dst[1] == 200 here;
            dst[2] == 200 here;
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
    backends B = make(b);

    enum { W = 5, H = 3, N = W * H };
    int32_t xbuf[N], ybuf[N];
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(xbuf, 0, sizeof xbuf);
        __builtin_memset(ybuf, 0, sizeof ybuf);
        if (!run(&B, bi, W, H,
                 (umbra_buf[]){
                     {.ptr=xbuf, .sz=sizeof xbuf, .row_bytes=W * 4},
                     {.ptr=ybuf, .sz=sizeof ybuf, .row_bytes=W * 4},
                 })) {
            continue;
        }
        for (int j = 0; j < N; j++) {
            xbuf[j] == j % W here;
            ybuf[j] == j / W here;
        }
    }
    cleanup(&B);
}

static void test_load_next_32(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val             v = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_store_32(b, (umbra_ptr){1, 0}, v);
    backends B = make(b);

    int32_t src[16], dst[16];
    for (int i = 0; i < 16; i++) { src[i] = i * 7 + 3; }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!run(&B, bi, 16, 1, (umbra_buf[]){
            {.ptr=src, .sz=sizeof src},
            {.ptr=dst, .sz=sizeof dst},
        })) { continue; }
        for (int i = 0; i < 16; i++) { (dst[i] == src[i]) here; }
    }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!run(&B, bi, 4, 4, (umbra_buf[]){
            {.ptr=src, .sz=sizeof src, .row_bytes=4*4},
            {.ptr=dst, .sz=sizeof dst, .row_bytes=4*4},
        })) { continue; }
        for (int i = 0; i < 16; i++) { (dst[i] == src[i]) here; }
    }
    cleanup(&B);
}

static void test_load_next_16(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val             v = umbra_load_16(b, (umbra_ptr){0, 0});
    umbra_store_16(b, (umbra_ptr){1, 0}, v);
    backends B = make(b);

    int16_t src[16], dst[16];
    for (int i = 0; i < 16; i++) { src[i] = (int16_t)(i * 7 + 3); }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!run(&B, bi, 16, 1, (umbra_buf[]){
            {.ptr=src, .sz=sizeof src},
            {.ptr=dst, .sz=sizeof dst},
        })) { continue; }
        for (int i = 0; i < 16; i++) { (dst[i] == src[i]) here; }
    }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!run(&B, bi, 4, 4, (umbra_buf[]){
            {.ptr=src, .sz=sizeof src, .row_bytes=4*2},
            {.ptr=dst, .sz=sizeof dst, .row_bytes=4*2},
        })) { continue; }
        for (int i = 0; i < 16; i++) { (dst[i] == src[i]) here; }
    }
    cleanup(&B);
}

static void test_load_store_color_8888(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_color c = umbra_load_color(b, (umbra_ptr){0, 0});
    umbra_store_color(b, (umbra_ptr){1, 0}, c);
    backends B = make(b);

    uint32_t src[8], dst[8];
    for (int i = 0; i < 8; i++) { src[i] = 0xFF804020u + (unsigned)i; }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!run(&B, bi, 8, 1, (umbra_buf[]){
            {.ptr=src, .sz=sizeof src, .fmt=umbra_fmt_8888},
            {.ptr=dst, .sz=sizeof dst, .fmt=umbra_fmt_8888},
        })) { continue; }
        for (int i = 0; i < 8; i++) { (dst[i] == src[i]) here; }
    }
    cleanup(&B);
}

static void test_load_store_color_f16_planar(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_color c = umbra_load_color(b, (umbra_ptr){0, 0});
    umbra_store_color(b, (umbra_ptr){1, 0}, c);
    backends B = make(b);

    enum { W = 8, H = 4 };
    __fp16 src[W * H * 4], dst[W * H * 4];
    for (int i = 0; i < W * H; i++) {
        src[i + W*H*0] = (__fp16)(0.1f * (float)(i + 1));
        src[i + W*H*1] = (__fp16)(0.2f * (float)(i + 1));
        src[i + W*H*2] = (__fp16)(0.3f * (float)(i + 1));
        src[i + W*H*3] = (__fp16)(0.9f + 0.001f * (float)i);
    }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!run(&B, bi, W, H, (umbra_buf[]){
            {.ptr=src, .sz=sizeof src, .row_bytes=W*2,
             .fmt=umbra_fmt_fp16_planar},
            {.ptr=dst, .sz=sizeof dst, .row_bytes=W*2,
             .fmt=umbra_fmt_fp16_planar},
        })) { continue; }
        (0 == __builtin_memcmp(dst, src, sizeof src)) here;
    }
    cleanup(&B);
}

static void test_load_store_color_565(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_color c = umbra_load_color(b, (umbra_ptr){0, 0});
    umbra_store_color(b, (umbra_ptr){1, 0}, c);
    backends B = make(b);

    uint16_t src[7], dst[7];
    for (int i = 0; i < 7; i++) { src[i] = (uint16_t)(0x1234u + (unsigned)i * 0x1111u); }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!run(&B, bi, 7, 1, (umbra_buf[]){
            {.ptr=src, .sz=sizeof src, .fmt=umbra_fmt_565},
            {.ptr=dst, .sz=sizeof dst, .fmt=umbra_fmt_565},
        })) { continue; }
        for (int i = 0; i < 7; i++) { (dst[i] == src[i]) here; }
    }
    cleanup(&B);
}

static void test_load_store_color_1010102(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_color c = umbra_load_color(b, (umbra_ptr){0, 0});
    umbra_store_color(b, (umbra_ptr){1, 0}, c);
    backends B = make(b);

    uint32_t src[7], dst[7];
    for (int i = 0; i < 7; i++) {
        src[i] = ((unsigned)i * 73u) | ((unsigned)i * 37u << 10)
               | ((unsigned)i * 19u << 20) | (2u << 30);
    }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!run(&B, bi, 7, 1, (umbra_buf[]){
            {.ptr=src, .sz=sizeof src, .fmt=umbra_fmt_1010102},
            {.ptr=dst, .sz=sizeof dst, .fmt=umbra_fmt_1010102},
        })) { continue; }
        for (int i = 0; i < 7; i++) { (dst[i] == src[i]) here; }
    }
    cleanup(&B);
}

static void test_load_store_color_fp16(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_color c = umbra_load_color(b, (umbra_ptr){0, 0});
    umbra_store_color(b, (umbra_ptr){1, 0}, c);
    backends B = make(b);

    __fp16 src[7 * 4], dst[7 * 4];
    for (int i = 0; i < 7; i++) {
        src[i * 4 + 0] = (__fp16)(0.1f * (float)(i + 1));
        src[i * 4 + 1] = (__fp16)(0.2f * (float)(i + 1));
        src[i * 4 + 2] = (__fp16)(0.3f * (float)(i + 1));
        src[i * 4 + 3] = (__fp16)(0.9f + 0.001f * (float)i);
    }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!run(&B, bi, 7, 1, (umbra_buf[]){
            {.ptr=src, .sz=sizeof src, .fmt=umbra_fmt_fp16},
            {.ptr=dst, .sz=sizeof dst, .fmt=umbra_fmt_fp16},
        })) { continue; }
        (0 == __builtin_memcmp(dst, src, sizeof src)) here;
    }
    cleanup(&B);
}

static void test_load_store_color_srgb(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_color c = umbra_load_color(b, (umbra_ptr){0, 0});
    umbra_store_color(b, (umbra_ptr){1, 0}, c);
    backends B = make(b);

    uint32_t src[7], dst[7];
    for (int i = 0; i < 7; i++) {
        unsigned v = (unsigned)i * 37u;
        src[i] = v | (v << 8) | (v << 16) | (0xFFu << 24);
    }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!run(&B, bi, 7, 1, (umbra_buf[]){
            {.ptr=src, .sz=sizeof src, .fmt=umbra_fmt_srgb},
            {.ptr=dst, .sz=sizeof dst, .fmt=umbra_fmt_srgb},
        })) { continue; }
        for (int i = 0; i < 7; i++) { (dst[i] == src[i]) here; }
    }
    cleanup(&B);
}

static void test_srgb_roundtrip_256(void) {
    // sRGB byte → load_color (sRGB→linear) → store_color (linear→sRGB) → compare.
    struct umbra_builder *b = umbra_builder();
    umbra_color c = umbra_load_color(b, (umbra_ptr){0, 0});
    umbra_store_color(b, (umbra_ptr){1, 0}, c);
    backends B = make(b);

    // Build a 256-pixel row: pixel i has R=G=B=A=i.
    uint32_t src[256], dst[256];
    for (int i = 0; i < 256; i++) {
        uint32_t v = (uint32_t)i;
        src[i] = v | (v << 8) | (v << 16) | (v << 24);
    }

    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!run(&B, bi, 256, 1, (umbra_buf[]){
            {.ptr=src, .sz=sizeof src, .fmt=umbra_fmt_srgb},
            {.ptr=dst, .sz=sizeof dst, .fmt=umbra_fmt_srgb},
        })) { continue; }
        // All 256 values: check max delta.
        int worst = 0;
        for (int i = 0; i < 256; i++) {
            for (int ch = 0; ch < 4; ch++) {
                int a = (int)((src[i] >> (ch*8)) & 0xFF);
                int d_ = (int)((dst[i] >> (ch*8)) & 0xFF);
                int delta = a - d_;
                if (delta < 0) { delta = -delta; }
                if (delta > worst) { worst = delta; }
            }
        }
        (dst[0x00] == src[0x00]) here;
        (dst[0x7F] == src[0x7F]) here;
        (dst[0xFF] == src[0xFF]) here;
        worst == 0 here;
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
    backends B = make(b);

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
            {.ptr=uni, .sz=(size_t)(ri * 4 + 4), .read_only=1},
            {.ptr=src, .sz=sizeof src},
            {.ptr=dst, .sz=sizeof dst, .row_bytes=4*4},
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
    backends B = make(b);
    int32_t sum_pre = N * (N + 1) / 2;
    int32_t src[4] = {100, 200, 300, 400}, dst[4] = {0};
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
#if defined(__aarch64__)
        __asm__ volatile("mov x15, #0xdead" ::: "x15");
#endif
        if (!run(&B, bi, 4, 1, (umbra_buf[]){
            {.ptr=src, .sz=sizeof src},
            {.ptr=dst, .sz=sizeof dst},
        })) { continue; }
        for (int i = 0; i < 4; i++) { (dst[i] == src[i] + sum_pre) here; }
    }
    cleanup(&B);
}

static void test_backend_threadsafe(void) {
    struct umbra_backend *interp = umbra_backend_interp();
    interp->threadsafe == 1 here;
    umbra_backend_free(interp);

    struct umbra_backend *jit = umbra_backend_jit();
    if (jit) {
        jit->threadsafe == 1 here;
        umbra_backend_free(jit);
    }

    struct umbra_backend *metal = umbra_backend_metal();
    if (metal) {
        metal->threadsafe == 0 here;
        umbra_backend_free(metal);
    }

}

static void test_program_null_guards(void) {
    umbra_backend_flush(NULL);
    umbra_backend_free(NULL);

    // flush/dump on backends that don't have those fns
    struct umbra_backend *be = umbra_backend_interp();
    umbra_backend_flush(be);

    struct umbra_builder *b = umbra_builder();
    umbra_store_32(b, (umbra_ptr){0, 0}, umbra_load_32(b, (umbra_ptr){0, 0}));
    struct umbra_basic_block *bb = umbra_basic_block(b);
    umbra_builder_free(b);
    struct umbra_program *p = be->compile(be, bb);
    umbra_basic_block_free(bb);

    // dump on interpreter (no dump fn)
    if (p->dump) { p->dump(p->ctx, stdout); }

    // queue with w=0 and h=0
    int32_t buf[1] = {0};
    umbra_program_queue(p, 0, 0, 0, 1, (umbra_buf[]){{.ptr=buf, .sz=4}});
    umbra_program_queue(p, 0, 0, 1, 0, (umbra_buf[]){{.ptr=buf, .sz=4}});

    p->free_fn(p->ctx); free(p);
    umbra_backend_free(be);
}

// Regression test: register variant chains must not cross the preamble/body boundary.
// The preamble runs only on the first tile; subsequent tiles start at the body.
// If a preamble output is kept in the register and the body reads from it,
// tile 2+ would read the previous tile's last value instead.
static void test_preamble_register_boundary(void) {
    struct umbra_builder *b = umbra_builder();
    // uniform is preamble (loop-invariant), add is body (uses x which varies).
    // The uniform feeds directly into the add at offset -1 if scheduled adjacently.
    umbra_val u = umbra_uniform_32(b, (umbra_ptr){0, 0}, 0);
    umbra_val x = umbra_add_i32(b, umbra_x(b), u);
    umbra_store_32(b, (umbra_ptr){1, 0}, x);
    backends B = make(b);

    // Width 32 > K=16, so there are at least 2 tiles.
    // The uniform value is 1000, so result should be col + 1000.
    int32_t uni = 1000;
    int32_t dst[32];
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        __builtin_memset(dst, 0, sizeof dst);
        if (!run(&B, bi, 32, 1,
                 (umbra_buf[]){{.ptr=&uni, .sz=4, .read_only=1}, {.ptr=dst, .sz=sizeof dst}})) {
            continue;
        }
        for (int col = 0; col < 32; col++) {
            dst[col] == col + 1000 here;
        }
    }
    cleanup(&B);
}

static void test_shr_ops(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val a = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_val s = umbra_load_32(b, (umbra_ptr){1, 0});
    // Chain: shr_u32 → shr_s32 exercises _rm variant on second op.
    umbra_val c = umbra_shr_u32(b, a, s);
    umbra_val d = umbra_shr_s32(b, c, s);
    umbra_store_32(b, (umbra_ptr){2, 0}, d);
    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t va[] = {0x12345678, (int32_t)0xFF000000u, 0x7FFFFFFF, 0};
        int32_t vs[] = {4, 8, 16, 0};
        int32_t dst[4] = {0};
        if (!run(&B, bi, 4, 1, (umbra_buf[]){
            {.ptr=va, .sz=sizeof va}, {.ptr=vs, .sz=sizeof vs},
            {.ptr=dst, .sz=sizeof dst},
        })) { continue; }
        for (int i = 0; i < 4; i++) {
            int32_t expect = (int32_t)((uint32_t)va[i] >> vs[i]) >> vs[i];
            (dst[i] == expect) here;
        }
    }
    cleanup(&B);
}

static void test_neg_round_i32(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val x = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_val f = umbra_f32_from_i32(b, x);
    // Chain: neg → round_i32 exercises register variants for both.
    umbra_val n = umbra_neg_f32(b, f);
    umbra_val r = umbra_round_i32(b, n);
    umbra_store_32(b, (umbra_ptr){1, 0}, r);
    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t src[] = {3, -7, 0, 100};
        int32_t dst[4] = {0};
        if (!run(&B, bi, 4, 1, (umbra_buf[]){
            {.ptr=src, .sz=sizeof src}, {.ptr=dst, .sz=sizeof dst},
        })) { continue; }
        (dst[0] == -3) here;
        (dst[1] ==  7) here;
        (dst[2] ==  0) here;
        (dst[3] == -100) here;
    }
    cleanup(&B);
}

static void test_cmp_unsigned_signed(void) {
    // Base ops (non-chain, result to store → m_*_mm).
    struct umbra_builder *b = umbra_builder();
    umbra_val a = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_val c = umbra_load_32(b, (umbra_ptr){1, 0});
    umbra_val m1 = umbra_le_s32(b, a, c);
    umbra_val m2 = umbra_lt_u32(b, a, c);
    umbra_val m3 = umbra_le_u32(b, a, c);
    umbra_store_32(b, (umbra_ptr){2, 0}, m1);
    umbra_store_32(b, (umbra_ptr){3, 0}, m2);
    umbra_store_32(b, (umbra_ptr){4, 0}, m3);
    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sa[] = {-1, 0, 5, 10};
        int32_t sc[] = { 0, 0, 5,  5};
        int32_t d1[4]={0}, d2[4]={0}, d3[4]={0};
        if (!run(&B, bi, 4, 1, (umbra_buf[]){
            {.ptr=sa, .sz=sizeof sa}, {.ptr=sc, .sz=sizeof sc},
            {.ptr=d1, .sz=sizeof d1}, {.ptr=d2, .sz=sizeof d2}, {.ptr=d3, .sz=sizeof d3},
        })) { continue; }
        (d1[0] == -1) here; (d1[1] == -1) here; (d1[2] == -1) here; (d1[3] == 0) here;
        (d2[0] == 0) here; (d2[1] == 0) here; (d2[2] == 0) here; (d2[3] == 0) here;
        (d3[0] == 0) here; (d3[1] == -1) here; (d3[2] == -1) here; (d3[3] == 0) here;
    }
    cleanup(&B);

    // Chained: cmp → sel exercises r_*_mm (result to acc) and
    // the base ops with register variant upgrade.
    b = umbra_builder();
    a = umbra_load_32(b, (umbra_ptr){0, 0});
    c = umbra_load_32(b, (umbra_ptr){1, 0});
    umbra_val one = umbra_imm_i32(b, 1), zero = umbra_imm_i32(b, 0);
    // le_s32 → sel: le result → acc, sel reads it
    umbra_val r1 = umbra_sel_i32(b, umbra_le_s32(b, a, c), one, zero);
    umbra_val r2 = umbra_sel_i32(b, umbra_lt_u32(b, a, c), one, zero);
    umbra_val r3 = umbra_sel_i32(b, umbra_le_u32(b, a, c), one, zero);
    umbra_store_32(b, (umbra_ptr){2, 0}, r1);
    umbra_store_32(b, (umbra_ptr){3, 0}, r2);
    umbra_store_32(b, (umbra_ptr){4, 0}, r3);
    B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sa2[] = {-1, 0, 5, 10};
        int32_t sc2[] = { 0, 0, 5,  5};
        int32_t d12[4]={0}, d22[4]={0}, d32[4]={0};
        if (!run(&B, bi, 4, 1, (umbra_buf[]){
            {.ptr=sa2, .sz=sizeof sa2}, {.ptr=sc2, .sz=sizeof sc2},
            {.ptr=d12, .sz=sizeof d12}, {.ptr=d22, .sz=sizeof d22}, {.ptr=d32, .sz=sizeof d32},
        })) { continue; }
        (d12[0] == 1) here; (d12[1] == 1) here; (d12[2] == 1) here; (d12[3] == 0) here;
        (d22[0] == 0) here; (d22[1] == 0) here; (d22[2] == 0) here; (d22[3] == 0) here;
        (d32[0] == 0) here; (d32[1] == 1) here; (d32[2] == 1) here; (d32[3] == 0) here;
    }
    cleanup(&B);
}

static void test_max_f32_imm(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val x = umbra_f32_from_i32(b, umbra_load_32(b, (umbra_ptr){0, 0}));
    umbra_val c = umbra_max_f32(b, x, umbra_imm_f32(b, 5.f));
    umbra_store_32(b, (umbra_ptr){1, 0}, umbra_i32_from_f32(b, c));
    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t src[] = {-5, 0, 3, 100};
        int32_t dst[4] = {0};
        if (!run(&B, bi, 4, 1, (umbra_buf[]){
            {.ptr=src, .sz=sizeof src}, {.ptr=dst, .sz=sizeof dst},
        })) { continue; }
        (dst[0] == 5) here; (dst[1] == 5) here; (dst[2] == 5) here; (dst[3] == 100) here;
    }
    cleanup(&B);
}

static void test_imm_cmp_i32(void) {
    // Base imm ops (result to store).
    struct umbra_builder *b = umbra_builder();
    umbra_val a = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_val m1 = umbra_eq_i32(b, a, umbra_imm_i32(b, 5));
    umbra_val m2 = umbra_lt_s32(b, a, umbra_imm_i32(b, 5));
    umbra_val m3 = umbra_le_s32(b, a, umbra_imm_i32(b, 5));
    umbra_store_32(b, (umbra_ptr){1, 0}, m1);
    umbra_store_32(b, (umbra_ptr){2, 0}, m2);
    umbra_store_32(b, (umbra_ptr){3, 0}, m3);
    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t src[] = {3, 5, 7, -1};
        int32_t d1[4]={0}, d2[4]={0}, d3[4]={0};
        if (!run(&B, bi, 4, 1, (umbra_buf[]){
            {.ptr=src, .sz=sizeof src},
            {.ptr=d1, .sz=sizeof d1}, {.ptr=d2, .sz=sizeof d2}, {.ptr=d3, .sz=sizeof d3},
        })) { continue; }
        (d1[0]==0) here; (d1[1]==-1) here; (d1[2]==0) here; (d1[3]==0) here;
        (d2[0]==-1) here; (d2[1]==0) here; (d2[2]==0) here; (d2[3]==-1) here;
        (d3[0]==-1) here; (d3[1]==-1) here; (d3[2]==0) here; (d3[3]==-1) here;
    }
    cleanup(&B);
}

static void test_uniform_register(void) {
    // uniform → acc: uniform consumed only by next ALU op.
    struct umbra_builder *b = umbra_builder();
    umbra_val u = umbra_uniform_32(b, (umbra_ptr){0, 0}, 0);
    umbra_val x = umbra_load_32(b, (umbra_ptr){1, 0});
    umbra_val r = umbra_add_i32(b, u, x);
    umbra_store_32(b, (umbra_ptr){2, 0}, r);
    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t uni = 1000;
        int32_t src[] = {1, 2, 3, 4};
        int32_t dst[4] = {0};
        if (!run(&B, bi, 4, 1, (umbra_buf[]){
            {.ptr=&uni, .sz=4, .read_only=1},
            {.ptr=src, .sz=sizeof src},
            {.ptr=dst, .sz=sizeof dst},
        })) { continue; }
        for (int i = 0; i < 4; i++) { (dst[i] == 1000 + src[i]) here; }
    }
    cleanup(&B);
}

static void test_minmax_register_variants(void) {
    // Exercise _mm, _mr, _rr variants for min/max.
    struct umbra_builder *b = umbra_builder();
    umbra_val a = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_val bf_ = umbra_load_32(b, (umbra_ptr){1, 0});
    umbra_val fa = umbra_f32_from_i32(b, a);
    umbra_val fb = umbra_f32_from_i32(b, bf_);
    // _mr: x from mem, y from acc (fb just produced)
    umbra_val mn = umbra_min_f32(b, fa, fb);
    // _rr: same value as both args (mn just produced → acc, used twice)
    umbra_val mx = umbra_max_f32(b, mn, mn);
    umbra_store_32(b, (umbra_ptr){2, 0}, umbra_i32_from_f32(b, mx));
    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sa[] = {1, 10, 5, -3};
        int32_t sb[] = {5, 2, 5, 7};
        int32_t dst[4] = {0};
        if (!run(&B, bi, 4, 1, (umbra_buf[]){
            {.ptr=sa, .sz=sizeof sa}, {.ptr=sb, .sz=sizeof sb},
            {.ptr=dst, .sz=sizeof dst},
        })) { continue; }
        // min then max(x,x) = min
        (dst[0] == 1) here; (dst[1] == 2) here; (dst[2] == 5) here; (dst[3] == -3) here;
    }
    cleanup(&B);
}

static void test_cmp_register_variants(void) {
    // Exercise _rm variant for comparisons: chain a→b→cmp(b,c).
    struct umbra_builder *b = umbra_builder();
    umbra_val x = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_val y = umbra_load_32(b, (umbra_ptr){1, 0});
    // add puts result in acc, then lt_f32 gets x from acc → _rm
    umbra_val fx = umbra_f32_from_i32(b, x);
    umbra_val fy = umbra_f32_from_i32(b, y);
    umbra_val s = umbra_add_f32(b, fx, umbra_imm_f32(b, 0.5f));
    umbra_val m = umbra_lt_f32(b, s, fy);
    umbra_store_32(b, (umbra_ptr){2, 0}, m);
    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sx[] = {1, 10, 5, 0};
        int32_t sy[] = {5, 5, 5, 5};
        int32_t dst[4] = {0};
        if (!run(&B, bi, 4, 1, (umbra_buf[]){
            {.ptr=sx, .sz=sizeof sx}, {.ptr=sy, .sz=sizeof sy},
            {.ptr=dst, .sz=sizeof dst},
        })) { continue; }
        // 1.5<5 → -1, 10.5<5 → 0, 5.5<5 → 0, 0.5<5 → -1
        (dst[0] == -1) here; (dst[1] == 0) here; (dst[2] == 0) here; (dst[3] == -1) here;
    }
    cleanup(&B);
}

static void test_pack_register_variants(void) {
    // Exercise _rm, _mr, _rr variants for pack.
    struct umbra_builder *b = umbra_builder();
    umbra_val a = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_val bf_ = umbra_load_32(b, (umbra_ptr){1, 0});
    // _rm: a→and_imm→acc, pack(acc, bf_) = pack with x from acc
    umbra_val lo = umbra_and_i32(b, a, umbra_imm_i32(b, 0xFF));
    umbra_val p1 = umbra_pack(b, lo, bf_, 8);
    umbra_store_32(b, (umbra_ptr){2, 0}, p1);
    // _mr: bf_ from mem, and_imm→acc as y
    umbra_val hi = umbra_and_i32(b, bf_, umbra_imm_i32(b, 0xFF));
    umbra_val p2 = umbra_pack(b, a, hi, 8);
    umbra_store_32(b, (umbra_ptr){3, 0}, p2);
    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sa[] = {0x12, 0x34, 0xAB, 0x00};
        int32_t sb[] = {0x56, 0x78, 0xCD, 0xFF};
        int32_t d1[4]={0}, d2[4]={0};
        if (!run(&B, bi, 4, 1, (umbra_buf[]){
            {.ptr=sa, .sz=sizeof sa}, {.ptr=sb, .sz=sizeof sb},
            {.ptr=d1, .sz=sizeof d1}, {.ptr=d2, .sz=sizeof d2},
        })) { continue; }
        for (int i = 0; i < 4; i++) {
            ((d1[i] == ((sa[i] & 0xFF) | (sb[i] << 8)))) here;
            ((d2[i] == (sa[i] | ((sb[i] & 0xFF) << 8)))) here;
        }
    }
    cleanup(&B);
}

static void test_regvar_m_patterns(void) {
    // Targets: m_cmp_rm, m_cmp_rr, r_minmax_mm, base max_f32_imm,
    // m_float_cmp_imm_r, r/m_int_cmp_imm_r, r_pack_rm, m_pack_rr.
    struct umbra_builder *b = umbra_builder();
    umbra_val a = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_val c = umbra_load_32(b, (umbra_ptr){1, 0});
    umbra_val fa = umbra_f32_from_i32(b, a);
    umbra_val fc = umbra_f32_from_i32(b, c);

    // m_cmp_rm: add→acc, then lt_f32(acc, mem) → store (out_r=false → m_)
    umbra_val sum = umbra_add_f32(b, fa, umbra_imm_f32(b, 0.5f));
    umbra_store_32(b, (umbra_ptr){2, 0}, umbra_lt_f32(b, sum, fc));

    // m_cmp_rr: lt_f32(x, x) where x is from acc, result to store
    umbra_val neg = umbra_neg_f32(b, fa);
    umbra_store_32(b, (umbra_ptr){3, 0}, umbra_lt_f32(b, neg, neg));

    // r_min_mm: both from memory, result feeds next ALU
    umbra_val dummy = umbra_add_f32(b, fa, fc);
    umbra_store_32(b, (umbra_ptr){4, 0}, umbra_i32_from_f32(b, dummy));
    umbra_val mn = umbra_min_f32(b, fa, fc);
    umbra_store_32(b, (umbra_ptr){5, 0}, umbra_i32_from_f32(b, mn));

    // base max_f32_imm: result direct to store (no chain → base op)
    umbra_val mx = umbra_max_f32(b, fc, umbra_imm_f32(b, 5.f));
    umbra_store_32(b, (umbra_ptr){6, 0}, mx);

    // m_float_cmp_imm_r: chain→lt_f32_imm→store (x from acc, result to memory)
    umbra_val fn_ = umbra_neg_f32(b, fc);
    umbra_store_32(b, (umbra_ptr){7, 0}, umbra_lt_f32(b, fn_, umbra_imm_f32(b, 0.f)));

    // m_int_cmp_imm_r: chain→cmp_imm→store (x from acc, result to memory)
    umbra_val ai = umbra_add_i32(b, a, umbra_imm_i32(b, 1));
    umbra_store_32(b, (umbra_ptr){8, 0}, umbra_eq_i32(b, ai, umbra_imm_i32(b, 6)));
    umbra_val ai2 = umbra_add_i32(b, a, umbra_imm_i32(b, 2));
    umbra_store_32(b, (umbra_ptr){9, 0}, umbra_lt_s32(b, ai2, umbra_imm_i32(b, 6)));
    umbra_val ai3 = umbra_add_i32(b, a, umbra_imm_i32(b, 3));
    umbra_store_32(b, (umbra_ptr){10, 0}, umbra_le_s32(b, ai3, umbra_imm_i32(b, 6)));
    // r_int_cmp_imm_r: chain→cmp_imm→and (result to acc, feeds ALU)
    umbra_val ai4 = umbra_add_i32(b, a, umbra_imm_i32(b, 100));
    umbra_val eq_mask = umbra_eq_i32(b, ai4, umbra_imm_i32(b, 105));
    umbra_store_32(b, (umbra_ptr){13, 0}, umbra_and_i32(b, eq_mask, a));

    // r_pack_rm: and→acc, pack(acc, mem)→acc, add→store (pack result feeds ALU)
    umbra_val lo2 = umbra_and_i32(b, a, umbra_imm_i32(b, 0xFF));
    umbra_val pk = umbra_pack(b, lo2, c, 8);
    umbra_store_32(b, (umbra_ptr){11, 0}, umbra_add_i32(b, pk, umbra_imm_i32(b, 1)));

    // m_pack_rr: pack(x, x) where x from acc, result to store
    umbra_val lo3 = umbra_and_i32(b, a, umbra_imm_i32(b, 0xF));
    umbra_store_32(b, (umbra_ptr){12, 0}, umbra_pack(b, lo3, lo3, 4));

    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sa[] = {5, 0, -1, 10};
        int32_t sc[] = {3, 3, 3, 3};
        int32_t d[14][4];
        __builtin_memset(d, 0, sizeof d);
        if (!run(&B, bi, 4, 1, (umbra_buf[]){
            {.ptr=sa, .sz=sizeof sa}, {.ptr=sc, .sz=sizeof sc},
            {.ptr=d[2], .sz=16}, {.ptr=d[3], .sz=16},
            {.ptr=d[4], .sz=16}, {.ptr=d[5], .sz=16},
            {.ptr=d[6], .sz=16}, {.ptr=d[7], .sz=16},
            {.ptr=d[8], .sz=16}, {.ptr=d[9], .sz=16},
            {.ptr=d[10], .sz=16}, {.ptr=d[11], .sz=16},
            {.ptr=d[12], .sz=16}, {.ptr=d[13], .sz=16},
        })) { continue; }
        // lt(5.5,3)=0, lt(0.5,3)=-1, lt(-0.5,3)=-1, lt(10.5,3)=0
        (d[2][0] == 0) here; (d[2][1] == -1) here; (d[2][2] == -1) here; (d[2][3] == 0) here;
        // lt(x,x) always 0
        for (int i = 0; i < 4; i++) { (d[3][i] == 0) here; }
        // min(5.0,3.0)=3 min(0.0,3.0)=0 min(-1.0,3.0)=-1 min(10.0,3.0)=3
        (d[5][0] == 3) here; (d[5][1] == 0) here; (d[5][2] == -1) here; (d[5][3] == 3) here;
        // max(3.0,5.0)=5.0 for all (fc=3, imm=5), stored as float bits
        union { float f; int32_t i; } five = {.f=5.f};
        for (int i = 0; i < 4; i++) { (d[6][i] == five.i) here; }
        // eq(5+1,6)=-1, eq(0+1,6)=0, eq(-1+1,6)=0, eq(10+1,6)=0
        (d[8][0] == -1) here; (d[8][1] == 0) here; (d[8][2] == 0) here; (d[8][3] == 0) here;
    }
    cleanup(&B);
}

// Exercise m_*_rr for every binary op: chain→op(acc,acc)→store.
static void test_binary_m_rr(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val a = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_val fa = umbra_f32_from_i32(b, a);
    int p = 1;

    // Each needs a unique chain source so dedup doesn't merge them.
#define RR_F32(op, k) { \
        umbra_val src = umbra_add_f32(b, fa, umbra_imm_f32(b, (float)(k))); \
        umbra_store_32(b, (umbra_ptr){p++, 0}, op(b, src, src)); \
    }
#define RR_I32(op, k) { \
        umbra_val src = umbra_add_i32(b, a, umbra_imm_i32(b, (k))); \
        umbra_store_32(b, (umbra_ptr){p++, 0}, op(b, src, src)); \
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
    RR_I32(umbra_and_i32, 700)
    RR_I32(umbra_or_i32,  800)
    RR_I32(umbra_xor_i32, 900)
#undef RR_F32
#undef RR_I32
    int const nbufs = p;
    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t src[] = {2, 2, 2, 2};
        int32_t d[15][4];
        __builtin_memset(d, 0, sizeof d);
        umbra_buf bufs[15];
        bufs[0] = (umbra_buf){.ptr=src, .sz=sizeof src};
        for (int i = 1; i < nbufs; i++) {
            bufs[i] = (umbra_buf){.ptr=d[i], .sz=16};
        }
        if (!run(&B, bi, 4, 1, bufs)) { continue; }
        // sub(3,3)=0, mul(4,4)=16, div(5,5)=1
        union { float f; int32_t i; } u;
        u.f = 0.f;  (d[1][0] == u.i) here;
        u.f = 16.f; (d[2][0] == u.i) here;
        u.f = 1.f;  (d[3][0] == u.i) here;
        // add(102,102)=204, sub(202,202)=0, mul(302,302)=91204
        (d[6][0] == 204) here;
        (d[7][0] == 0)   here;
        (d[8][0] == 302*302) here;
        // xor(x,x) = 0 for any x
        (d[14][0] == 0) here;
    }
    cleanup(&B);
}

// Exercise m_min/max_rm: chain→min/max(acc,mem)→store.
static void test_minmax_m_rm(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val a = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_val c = umbra_load_32(b, (umbra_ptr){1, 0});
    umbra_val fa = umbra_f32_from_i32(b, a);
    umbra_val fc = umbra_f32_from_i32(b, c);
    // neg→acc, then min(acc, fc)→store = m_min_rm
    umbra_val na = umbra_neg_f32(b, fa);
    umbra_store_32(b, (umbra_ptr){2, 0}, umbra_min_f32(b, na, fc));
    // neg→acc, then max(acc, fc)→store = m_max_rm
    umbra_val nc = umbra_neg_f32(b, fc);
    umbra_store_32(b, (umbra_ptr){3, 0}, umbra_max_f32(b, nc, fa));
    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sa[] = {5,5,5,5}, sc[] = {3,3,3,3};
        int32_t d2[4]={0}, d3[4]={0};
        if (!run(&B, bi, 4, 1, (umbra_buf[]){
            {.ptr=sa, .sz=16}, {.ptr=sc, .sz=16},
            {.ptr=d2, .sz=16}, {.ptr=d3, .sz=16},
        })) { continue; }
        // min(-5, 3) = -5
        union { float f; int32_t i; } u;
        u.f = -5.f; (d2[0] == u.i) here;
        // max(-3, 5) = 5
        u.f = 5.f;  (d3[0] == u.i) here;
    }
    cleanup(&B);
}

// Exercise r_cmp_rm and r_cmp_rr: chain→cmp→ALU→store.
static void test_cmp_r_rm_rr(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val a = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_val c = umbra_load_32(b, (umbra_ptr){1, 0});
    umbra_val fa = umbra_f32_from_i32(b, a);
    umbra_val fc = umbra_f32_from_i32(b, c);
    umbra_val one = umbra_imm_i32(b, 1), zero = umbra_imm_i32(b, 0);
    int p = 2;
    // r_cmp_rm: chain→cmp(acc, mem)→sel→store
#define CMP_RM(cmp, chain_op, chain_k) { \
        umbra_val src = chain_op(b, fa, umbra_imm_f32(b, (float)(chain_k))); \
        umbra_store_32(b, (umbra_ptr){p++, 0}, umbra_sel_i32(b, cmp(b, src, fc), one, zero)); \
    }
    CMP_RM(umbra_lt_f32, umbra_add_f32, 0.5f)
    CMP_RM(umbra_le_f32, umbra_add_f32, 1.5f)
    CMP_RM(umbra_eq_f32, umbra_add_f32, 2.5f)
#undef CMP_RM
    // r_cmp_rr: chain→cmp(acc, acc)→sel→store
#define CMP_RR(cmp, chain_op, chain_k) { \
        umbra_val src = chain_op(b, fa, umbra_imm_f32(b, (float)(chain_k))); \
        umbra_store_32(b, (umbra_ptr){p++, 0}, umbra_sel_i32(b, cmp(b, src, src), one, zero)); \
    }
    CMP_RR(umbra_lt_f32, umbra_add_f32, 3.5f)
    CMP_RR(umbra_eq_f32, umbra_add_f32, 4.5f)
    CMP_RR(umbra_eq_i32, umbra_add_i32, 100)
    CMP_RR(umbra_lt_s32, umbra_add_i32, 200)
    CMP_RR(umbra_le_s32, umbra_add_i32, 300)
    CMP_RR(umbra_lt_u32, umbra_add_i32, 400)
    CMP_RR(umbra_le_u32, umbra_add_i32, 500)
#undef CMP_RR
    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sa[] = {2,2,2,2}, sc[] = {3,3,3,3};
        int32_t d[12][4];
        __builtin_memset(d, 0, sizeof d);
        umbra_buf bufs[12];
        bufs[0] = (umbra_buf){.ptr=sa, .sz=16};
        bufs[1] = (umbra_buf){.ptr=sc, .sz=16};
        for (int i = 2; i < 12; i++) { bufs[i] = (umbra_buf){.ptr=d[i], .sz=16}; }
        if (!run(&B, bi, 4, 1, bufs)) { continue; }
        // lt(2.5, 3)=1, le(3.5, 3)=0, eq(4.5, 3)=0
        (d[2][0] == 1) here; (d[3][0] == 0) here; (d[4][0] == 0) here;
        // lt(x,x)=0, eq(x,x)=1, le(x,x)=1
        (d[5][0] == 0) here; (d[6][0] == 1) here;
        (d[7][0] == 1) here; (d[8][0] == 0) here;
        (d[9][0] == 1) here; (d[10][0] == 0) here; (d[11][0] == 1) here;
    }
    cleanup(&B);
}

// Exercise missing unary per-op variants.
// Need r_m (mem→acc) for ops that only have r_r and m_r covered.
static void test_unary_r_m(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val a = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_val fa = umbra_f32_from_i32(b, a);
    // r_sqrt_m: sqrt(mem) → acc → store via chain
    umbra_store_32(b, (umbra_ptr){1, 0},
                   umbra_i32_from_f32(b, umbra_sqrt_f32(b, fa)));
    // r_round_f32_m, r_floor_f32_m, r_ceil_f32_m:
    umbra_store_32(b, (umbra_ptr){2, 0},
                   umbra_i32_from_f32(b, umbra_round_f32(b, fa)));
    umbra_store_32(b, (umbra_ptr){3, 0},
                   umbra_i32_from_f32(b, umbra_floor_f32(b, fa)));
    umbra_store_32(b, (umbra_ptr){4, 0},
                   umbra_i32_from_f32(b, umbra_ceil_f32(b, fa)));
    // r_neg_m, r_round_i32_m, r_ceil_i32_m:
    umbra_store_32(b, (umbra_ptr){5, 0},
                   umbra_i32_from_f32(b, umbra_neg_f32(b, fa)));
    umbra_store_32(b, (umbra_ptr){6, 0},
                   umbra_add_i32(b, umbra_round_i32(b, fa), umbra_imm_i32(b, 0)));
    umbra_store_32(b, (umbra_ptr){7, 0},
                   umbra_add_i32(b, umbra_ceil_i32(b, fa), umbra_imm_i32(b, 0)));
    // r_f32_from_i32_m: f32_from_i32(mem) → acc → chain
    umbra_store_32(b, (umbra_ptr){8, 0},
                   umbra_i32_from_f32(b, umbra_f32_from_i32(b, a)));
    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t src[] = {4,4,4,4};
        int32_t d[9][4];
        __builtin_memset(d, 0, sizeof d);
        umbra_buf bufs[9];
        bufs[0] = (umbra_buf){.ptr=src, .sz=16};
        for (int i = 1; i < 9; i++) { bufs[i] = (umbra_buf){.ptr=d[i], .sz=16}; }
        if (!run(&B, bi, 4, 1, bufs)) { continue; }
        (d[1][0] == 2) here;  // sqrt(4)=2
        (d[2][0] == 4) here;  // round(4.0)=4
        (d[5][0] == -4) here; // i32_from_f32(neg(4.0))=-4
        (d[8][0] == 4) here;  // i32_from_f32(f32_from_i32(4))=4
    }
    cleanup(&B);
}

// Exercise base ops that are always upgraded: xor_32_imm, min_f32_imm.
// Need: op(mem, imm)→store with no chain on either side.
static void test_base_imm_ops(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val a = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_val c = umbra_load_32(b, (umbra_ptr){1, 0});
    // xor_32_imm base: neither chained
    umbra_store_32(b, (umbra_ptr){2, 0},
                   umbra_xor_i32(b, a, umbra_imm_i32(b, 0xFF)));
    // min_f32_imm base:
    umbra_val fc = umbra_f32_from_i32(b, c);
    umbra_store_32(b, (umbra_ptr){3, 0},
                   umbra_min_f32(b, fc, umbra_imm_f32(b, 2.f)));
    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sa[] = {0x12, 0x12, 0x12, 0x12};
        int32_t sc[] = {5, 1, 5, 1};
        int32_t d2[4]={0}, d3[4]={0};
        if (!run(&B, bi, 4, 1, (umbra_buf[]){
            {.ptr=sa, .sz=16}, {.ptr=sc, .sz=16},
            {.ptr=d2, .sz=16}, {.ptr=d3, .sz=16},
        })) { continue; }
        (d2[0] == (0x12 ^ 0xFF)) here;
        union { float f; int32_t i; } u;
        u.f = 2.f; (d3[0] == u.i) here;  // min(5, 2) = 2
        u.f = 1.f; (d3[1] == u.i) here;  // min(1, 2) = 1
    }
    cleanup(&B);
}

// Exercise r_sel_32_rm, r_fms_f32_mmr, r_pack_mm/mr/rr.
static void test_sel_fms_pack_variants(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val a = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_val c = umbra_load_32(b, (umbra_ptr){1, 0});
    umbra_val fa = umbra_f32_from_i32(b, a);
    umbra_val fc = umbra_f32_from_i32(b, c);

    // r_sel_32_rm: mask from acc → sel → next ALU → store
    umbra_val mask = umbra_lt_f32(b, fa, fc);
    umbra_val sel_r = umbra_sel_i32(b, mask, a, c);
    umbra_store_32(b, (umbra_ptr){2, 0}, umbra_add_i32(b, sel_r, umbra_imm_i32(b, 0)));

    // r_fms_f32_mmr: fms(x, y, acc) → acc → store
    // fms = z - x*y, with z from acc
    umbra_val fms_z = umbra_add_f32(b, fc, umbra_imm_f32(b, 10.f));
    umbra_val fms_r = umbra_sub_f32(b, fms_z, umbra_mul_f32(b, fa, fc));
    umbra_store_32(b, (umbra_ptr){3, 0}, umbra_i32_from_f32(b, fms_r));

    // r_pack_mm: both from memory, result→acc→chain
    umbra_val lo_a = umbra_and_i32(b, a, umbra_imm_i32(b, 0xFF));
    umbra_val dummy = umbra_add_i32(b, lo_a, c);
    umbra_store_32(b, (umbra_ptr){4, 0}, dummy);
    // Now lo_a and c are both "old" (in memory). pack(lo_a, c, 8)→acc→add→store
    umbra_val pk_mm = umbra_pack(b, lo_a, c, 8);
    umbra_store_32(b, (umbra_ptr){5, 0}, umbra_add_i32(b, pk_mm, umbra_imm_i32(b, 1)));

    // r_pack_mr: x from mem, y from acc, result→acc→chain
    umbra_val hi = umbra_and_i32(b, c, umbra_imm_i32(b, 0xFF));
    umbra_val pk_mr = umbra_pack(b, a, hi, 8);
    umbra_store_32(b, (umbra_ptr){6, 0}, umbra_add_i32(b, pk_mr, umbra_imm_i32(b, 1)));

    // r_pack_rr: same val as both args, from acc, result→acc→chain
    umbra_val lo_c = umbra_and_i32(b, c, umbra_imm_i32(b, 0xF));
    umbra_val pk_rr = umbra_pack(b, lo_c, lo_c, 4);
    umbra_store_32(b, (umbra_ptr){7, 0}, umbra_add_i32(b, pk_rr, umbra_imm_i32(b, 1)));

    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sa[] = {2,2,2,2}, sc[] = {5,5,5,5};
        int32_t d[8][4];
        __builtin_memset(d, 0, sizeof d);
        umbra_buf bufs[8];
        bufs[0] = (umbra_buf){.ptr=sa, .sz=16};
        bufs[1] = (umbra_buf){.ptr=sc, .sz=16};
        for (int i = 2; i < 8; i++) { bufs[i] = (umbra_buf){.ptr=d[i], .sz=16}; }
        if (!run(&B, bi, 4, 1, bufs)) { continue; }
        // sel: 2<5 → true → pick a=2
        (d[2][0] == 2) here;
        // pack_mm: (2 & 0xFF) | (5 << 8) + 1 = 2 | 1280 + 1 = 1283
        (d[5][0] == (2 | (5 << 8)) + 1) here;
    }
    cleanup(&B);
}

// Exercise missing IMM register variants.
static void test_imm_regvar(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val a = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_val fa = umbra_f32_from_i32(b, a);
    int p = 1;

    // r_*_imm_r for each: chain→imm_op(acc)→chain→store
    // Need the imm op's result to feed the next ALU (out_r) AND input from acc (x_r).
#define IMM_CHAIN_F(op, k) { \
        umbra_val src = umbra_add_f32(b, fa, umbra_imm_f32(b, (float)(k))); \
        umbra_val r = op(b, src, umbra_imm_f32(b, 2.f)); \
        umbra_store_32(b, (umbra_ptr){p++, 0}, umbra_i32_from_f32(b, r)); \
    }
#define IMM_CHAIN_I(op, k) { \
        umbra_val src = umbra_add_i32(b, a, umbra_imm_i32(b, (k))); \
        umbra_val r = op(b, src, umbra_imm_i32(b, 3)); \
        umbra_store_32(b, (umbra_ptr){p++, 0}, umbra_add_i32(b, r, umbra_imm_i32(b, 1))); \
    }
    IMM_CHAIN_F(umbra_mul_f32, 1)
    IMM_CHAIN_F(umbra_div_f32, 2)
    IMM_CHAIN_F(umbra_min_f32, 3)
    IMM_CHAIN_I(umbra_shr_u32, 1000)
    IMM_CHAIN_I(umbra_and_i32, 2000)
    IMM_CHAIN_I(umbra_xor_i32, 3000)
    IMM_CHAIN_I(umbra_mul_i32, 4000)
    IMM_CHAIN_I(umbra_sub_i32, 5000)
    // Float CMP imm: chain→cmp_imm(acc)→chain
    {
        umbra_val src = umbra_add_f32(b, fa, umbra_imm_f32(b, 10.f));
        umbra_val m = umbra_eq_f32(b, src, umbra_imm_f32(b, 12.f));
        umbra_store_32(b, (umbra_ptr){p++, 0}, umbra_and_i32(b, m, umbra_imm_i32(b, 1)));
    }
    {
        umbra_val src = umbra_add_f32(b, fa, umbra_imm_f32(b, 20.f));
        umbra_val m = umbra_lt_f32(b, src, umbra_imm_f32(b, 25.f));
        umbra_store_32(b, (umbra_ptr){p++, 0}, umbra_and_i32(b, m, umbra_imm_i32(b, 1)));
    }
    {
        umbra_val src = umbra_add_f32(b, fa, umbra_imm_f32(b, 30.f));
        umbra_val m = umbra_le_f32(b, src, umbra_imm_f32(b, 32.f));
        umbra_store_32(b, (umbra_ptr){p++, 0}, umbra_and_i32(b, m, umbra_imm_i32(b, 1)));
    }
    // Int CMP imm r_m variant: chain→cmp_imm(mem)→chain
    {
        umbra_val m = umbra_eq_i32(b, a, umbra_imm_i32(b, 2));
        umbra_store_32(b, (umbra_ptr){p++, 0}, umbra_and_i32(b, m, umbra_imm_i32(b, 1)));
    }
#undef IMM_CHAIN_F
#undef IMM_CHAIN_I
    int const nbufs = p;
    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t src[] = {2, 2, 2, 2};
        int32_t d[13][4];
        __builtin_memset(d, 0, sizeof d);
        umbra_buf bufs[13];
        bufs[0] = (umbra_buf){.ptr=src, .sz=16};
        for (int i = 1; i < nbufs; i++) { bufs[i] = (umbra_buf){.ptr=d[i], .sz=16}; }
        if (!run(&B, bi, 4, 1, bufs)) { continue; }
        // mul(3*2)=6, eq(12,12)=1, lt(22,25)=1, le(32,32)=1, eq_i32(2,2)=1
        (d[1][0] == 6) here;
        (d[9][0] == 1) here;
        (d[10][0] == 1) here;
        (d[11][0] == 1) here;
        (d[12][0] == 1) here;
    }
    cleanup(&B);
}

// Exercise r_*_mm for binary ops: both from memory, result→acc→chain.
// The BB optimizer schedules loads near consumers. To get _mm we need both
// operands pinned early (via a use/store) then a barrier store between them
// and the target op so prev_r is false.
static void test_binary_r_mm(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val a = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_val c = umbra_load_32(b, (umbra_ptr){1, 0});
    umbra_val fa = umbra_f32_from_i32(b, a);
    umbra_val fc = umbra_f32_from_i32(b, c);
    // Pin fa and fc: use both in an add, store result, then barrier store.
    umbra_store_32(b, (umbra_ptr){2, 0}, umbra_i32_from_f32(b, umbra_add_f32(b, fa, fc)));
    // After the store, prev_r=false. op(fa,fc) gets both from memory → r_*_mm.
    // Use i32_from_f32 as the chain consumer (out_r=true for op).
    umbra_store_32(b, (umbra_ptr){3, 0}, umbra_i32_from_f32(b, umbra_sub_f32(b, fa, fc)));
    umbra_store_32(b, (umbra_ptr){4, 0}, umbra_i32_from_f32(b, umbra_mul_f32(b, fa, fc)));
    umbra_store_32(b, (umbra_ptr){5, 0}, umbra_i32_from_f32(b, umbra_div_f32(b, fa, fc)));
    // Integer: pin a and c, then barrier, then ops.
    umbra_store_32(b, (umbra_ptr){6, 0}, umbra_add_i32(b, a, c));
    umbra_store_32(b, (umbra_ptr){7, 0}, umbra_add_i32(b, umbra_sub_i32(b, a, c), umbra_imm_i32(b, 1)));
    umbra_store_32(b, (umbra_ptr){8, 0}, umbra_add_i32(b, umbra_mul_i32(b, a, c), umbra_imm_i32(b, 1)));
    umbra_store_32(b, (umbra_ptr){9, 0}, umbra_add_i32(b, umbra_or_i32(b, a, c), umbra_imm_i32(b, 1)));
    umbra_store_32(b, (umbra_ptr){10, 0}, umbra_add_i32(b, umbra_xor_i32(b, a, c), umbra_imm_i32(b, 1)));
    umbra_store_32(b, (umbra_ptr){11, 0}, umbra_add_i32(b, umbra_and_i32(b, a, c), umbra_imm_i32(b, 1)));
    umbra_store_32(b, (umbra_ptr){12, 0}, umbra_add_i32(b, umbra_shl_i32(b, a, c), umbra_imm_i32(b, 1)));
    umbra_store_32(b, (umbra_ptr){13, 0}, umbra_add_i32(b, umbra_shr_u32(b, a, c), umbra_imm_i32(b, 1)));
    umbra_store_32(b, (umbra_ptr){14, 0}, umbra_add_i32(b, umbra_shr_s32(b, a, c), umbra_imm_i32(b, 1)));
    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sa[] = {6,6,6,6}, sc[] = {2,2,2,2};
        int32_t d[15][4];
        __builtin_memset(d, 0, sizeof d);
        umbra_buf bufs[15];
        bufs[0] = (umbra_buf){.ptr=sa, .sz=16};
        bufs[1] = (umbra_buf){.ptr=sc, .sz=16};
        for (int i = 2; i < 15; i++) { bufs[i] = (umbra_buf){.ptr=d[i], .sz=16}; }
        if (!run(&B, bi, 4, 1, bufs)) { continue; }
        (d[3][0] == 4)  here;  // sub(6,2)=4
        (d[4][0] == 12) here;  // mul(6,2)=12
        (d[5][0] == 3)  here;  // div(6,2)=3
        (d[7][0] == 5)  here;  // sub(6,2)+1=5
        (d[10][0] == (6^2)+1) here;
    }
    cleanup(&B);
}

// Exercise missing per-op UNARY and IMM variants more thoroughly.
// m_unary_r: chain→unary(acc)→store (no further chain).
static void test_unary_m_r(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val a = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_val fa = umbra_f32_from_i32(b, a);
    int p = 1;
    // Each: chain→unary→store
#define MR(unary, chain_val) { \
        umbra_store_32(b, (umbra_ptr){p++, 0}, unary(b, chain_val)); \
    }
    umbra_val fa2 = umbra_add_f32(b, fa, umbra_imm_f32(b, 1.f));
    MR(umbra_sqrt_f32, fa2)
    umbra_val fa3 = umbra_add_f32(b, fa, umbra_imm_f32(b, 2.f));
    MR(umbra_round_f32, fa3)
    umbra_val fa4 = umbra_add_f32(b, fa, umbra_imm_f32(b, 3.f));
    MR(umbra_floor_f32, fa4)
    umbra_val fa5 = umbra_add_f32(b, fa, umbra_imm_f32(b, 4.f));
    MR(umbra_ceil_f32, fa5)
    umbra_val fa6 = umbra_add_f32(b, fa, umbra_imm_f32(b, 5.f));
    MR(umbra_neg_f32, fa6)
    umbra_val fa7 = umbra_add_f32(b, fa, umbra_imm_f32(b, 6.f));
    MR(umbra_round_i32, fa7)
    umbra_val fa8 = umbra_add_f32(b, fa, umbra_imm_f32(b, 7.f));
    MR(umbra_ceil_i32, fa8)
    umbra_val ia = umbra_add_i32(b, a, umbra_imm_i32(b, 100));
    MR(umbra_f32_from_i32, ia)
#undef MR
    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t src[] = {4,4,4,4};
        int32_t d[9][4];
        __builtin_memset(d, 0, sizeof d);
        umbra_buf bufs[9];
        bufs[0] = (umbra_buf){.ptr=src, .sz=16};
        for (int i = 1; i < p; i++) { bufs[i] = (umbra_buf){.ptr=d[i], .sz=16}; }
        if (!run(&B, bi, 4, 1, bufs)) { continue; }
        // sqrt(5)≈2.236
        union { float f; int32_t i; } u;
        u.f = -9.f; (d[5][0] == u.i) here;   // neg(4+5) = -9
        (d[6][0] == 10) here;                  // round(4+6) = 10
    }
    cleanup(&B);
}

// Exercise base IMM ops: op(mem, imm)→store with no chain.
// Covers shr_s32_imm, shr_u32_imm, or_32_imm, mul_f32_imm, etc.
static void test_base_imm_more(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val a = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_val c = umbra_load_32(b, (umbra_ptr){1, 0});
    umbra_val fc = umbra_f32_from_i32(b, c);
    int p = 2;
    // Each: op(mem, imm)→store, no chain on either side.
    // Use multiple loads so dedup doesn't merge chains.
    umbra_store_32(b, (umbra_ptr){p++, 0}, umbra_shr_s32(b, a, umbra_imm_i32(b, 1)));
    umbra_store_32(b, (umbra_ptr){p++, 0}, umbra_shr_u32(b, a, umbra_imm_i32(b, 2)));
    umbra_store_32(b, (umbra_ptr){p++, 0}, umbra_or_i32(b, a, umbra_imm_i32(b, 0xF0)));
    umbra_store_32(b, (umbra_ptr){p++, 0}, umbra_mul_f32(b, fc, umbra_imm_f32(b, 3.f)));
    umbra_store_32(b, (umbra_ptr){p++, 0}, umbra_mul_i32(b, a, umbra_imm_i32(b, 7)));
    umbra_store_32(b, (umbra_ptr){p++, 0}, umbra_sub_i32(b, a, umbra_imm_i32(b, 1)));
    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sa[] = {16,16,16,16}, sc[] = {5,5,5,5};
        int32_t d[8][4];
        __builtin_memset(d, 0, sizeof d);
        umbra_buf bufs[8];
        bufs[0] = (umbra_buf){.ptr=sa, .sz=16};
        bufs[1] = (umbra_buf){.ptr=sc, .sz=16};
        for (int i = 2; i < p; i++) { bufs[i] = (umbra_buf){.ptr=d[i], .sz=16}; }
        if (!run(&B, bi, 4, 1, bufs)) { continue; }
        (d[2][0] == 8) here;   // 16 >> 1
        (d[3][0] == 4) here;   // 16 >>> 2
        (d[4][0] == (16 | 0xF0)) here;
        (d[7][0] == 15) here;  // 16 - 1
    }
    cleanup(&B);
}

// r_sel_32_rm: sel with mask from acc, result→acc.
// Need: chain→sel(acc_mask, y, z)→ALU→store
static void test_sel_r_rm(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val a = umbra_load_32(b, (umbra_ptr){0, 0});
    umbra_val c = umbra_load_32(b, (umbra_ptr){1, 0});
    umbra_val fa = umbra_f32_from_i32(b, a);
    umbra_val fc = umbra_f32_from_i32(b, c);
    // lt→acc, sel(acc, a, c)→acc, add→store
    umbra_val mask = umbra_lt_f32(b, fa, fc);
    umbra_val s = umbra_sel_i32(b, mask, a, c);
    umbra_val out = umbra_add_i32(b, s, umbra_imm_i32(b, 1));
    umbra_store_32(b, (umbra_ptr){2, 0}, out);
    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t sa[] = {2,8,2,8}, sc[] = {5,5,5,5};
        int32_t d[4] = {0};
        if (!run(&B, bi, 4, 1, (umbra_buf[]){
            {.ptr=sa, .sz=16}, {.ptr=sc, .sz=16}, {.ptr=d, .sz=16},
        })) { continue; }
        // 2<5→sel a=2, 8<5=false→sel c=5; +1
        (d[0] == 3) here; (d[1] == 6) here;
    }
    cleanup(&B);
}

// Exercise RA channel-register return: ra_step_unary consuming a non-zero
// channel of load_color where that channel dies at the unary op.
// Must use ops that go through ra_step_unary (abs, neg, imm shifts, etc.)
// not ra_step_alu (which is used by i32_from_f32, add, etc.)
static void test_ra_chan_unary(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_color c = umbra_load_color(b, (umbra_ptr){0, 0});
    // abs_f32 and neg_f32 use ra_step_unary.
    // Use c.g (chan 1) only via abs → dies at abs → triggers chan return.
    umbra_store_32(b, (umbra_ptr){1, 0}, umbra_i32_from_f32(b, umbra_abs_f32(b, c.g)));
    // Use c.b (chan 2) only via neg.
    umbra_store_32(b, (umbra_ptr){2, 0}, umbra_i32_from_f32(b, umbra_neg_f32(b, c.b)));
    // Use c.a (chan 3) only via abs.
    umbra_store_32(b, (umbra_ptr){3, 0}, umbra_i32_from_f32(b, umbra_abs_f32(b, c.a)));
    // c.r (chan 0) unused.
    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        uint32_t src[] = {0x80402010u, 0x80402010u, 0x80402010u, 0x80402010u};
        int32_t dg[4]={0}, db[4]={0}, da[4]={0};
        if (!run(&B, bi, 4, 1, (umbra_buf[]){
            {.ptr=src, .sz=sizeof src, .fmt=umbra_fmt_8888},
            {.ptr=dg, .sz=16}, {.ptr=db, .sz=16}, {.ptr=da, .sz=16},
        })) { continue; }
        // g = 0x40/255 ≈ 0.251, abs(0.251) ≈ 0.251, i32 truncates to 0
        (dg[0] == 0) here;
    }
    cleanup(&B);
}

// Exercise mul-by-power-of-2 peephole where imm has lower val ID than operand.
static void test_mul_pow2_peephole(void) {
    struct umbra_builder *b = umbra_builder();
    umbra_val four = umbra_imm_i32(b, 4);  // low ID
    umbra_val a = umbra_load_32(b, (umbra_ptr){0, 0});  // higher ID
    umbra_val r = umbra_mul_i32(b, a, four);  // sort puts four as x → pow2 x-path
    umbra_store_32(b, (umbra_ptr){1, 0}, r);
    backends B = make(b);
    for (int bi = 0; bi < NUM_BACKENDS; bi++) {
        int32_t src[] = {3,7,10,0};
        int32_t dst[4] = {0};
        if (!run(&B, bi, 4, 1, (umbra_buf[]){
            {.ptr=src, .sz=sizeof src}, {.ptr=dst, .sz=sizeof dst},
        })) { continue; }
        (dst[0] == 12) here; (dst[1] == 28) here; (dst[2] == 40) here; (dst[3] == 0) here;
    }
    cleanup(&B);
}

// Exercise umbra_const_eval: op(imm, imm) constant folding.
// All results are compile-time constants stored to a single output buffer.
static void test_const_eval(void) {
    // Split into two programs to stay under Metal's 31-buffer limit.
    // Part 1: float ops
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val fa = umbra_imm_f32(b, 10.f);
        umbra_val fc = umbra_imm_f32(b, 3.f);
        umbra_store_32(b, (umbra_ptr){0, 0}, umbra_add_f32(b, fa, fc));
        umbra_store_32(b, (umbra_ptr){1, 0}, umbra_sub_f32(b, fa, fc));
        umbra_store_32(b, (umbra_ptr){2, 0}, umbra_mul_f32(b, fa, fc));
        umbra_store_32(b, (umbra_ptr){3, 0}, umbra_div_f32(b, fa, fc));
        umbra_store_32(b, (umbra_ptr){4, 0}, umbra_min_f32(b, fa, fc));
        umbra_store_32(b, (umbra_ptr){5, 0}, umbra_max_f32(b, fa, fc));
        umbra_store_32(b, (umbra_ptr){6, 0}, umbra_sqrt_f32(b, fc));
        umbra_store_32(b, (umbra_ptr){7, 0}, umbra_abs_f32(b, umbra_imm_f32(b, -5.f)));
        umbra_store_32(b, (umbra_ptr){8, 0}, umbra_neg_f32(b, fc));
        umbra_store_32(b, (umbra_ptr){9, 0}, umbra_round_f32(b, umbra_imm_f32(b, 3.7f)));
        umbra_store_32(b, (umbra_ptr){10, 0}, umbra_floor_f32(b, umbra_imm_f32(b, 3.7f)));
        umbra_store_32(b, (umbra_ptr){11, 0}, umbra_ceil_f32(b, umbra_imm_f32(b, 3.2f)));
        umbra_store_32(b, (umbra_ptr){12, 0}, umbra_round_i32(b, umbra_imm_f32(b, 3.7f)));
        umbra_store_32(b, (umbra_ptr){13, 0}, umbra_floor_i32(b, umbra_imm_f32(b, 3.7f)));
        umbra_store_32(b, (umbra_ptr){14, 0}, umbra_ceil_i32(b, umbra_imm_f32(b, 3.2f)));
        umbra_store_32(b, (umbra_ptr){15, 0}, umbra_f32_from_i32(b, umbra_imm_i32(b, 10)));
        umbra_store_32(b, (umbra_ptr){16, 0}, umbra_i32_from_f32(b, fa));
        umbra_store_32(b, (umbra_ptr){17, 0}, umbra_eq_f32(b, fa, fc));
        umbra_store_32(b, (umbra_ptr){18, 0}, umbra_lt_f32(b, fa, fc));
        umbra_store_32(b, (umbra_ptr){19, 0}, umbra_le_f32(b, fa, fc));
        backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int32_t d[20][4];
            __builtin_memset(d, 0, sizeof d);
            umbra_buf bufs[20];
            for (int i = 0; i < 20; i++) { bufs[i] = (umbra_buf){.ptr=d[i], .sz=16}; }
            if (!run(&B, bi, 4, 1, bufs)) { continue; }
            union { float f; int32_t i; } u;
            u.f = 13.f; (d[0][0] == u.i) here;
            u.f = 7.f;  (d[1][0] == u.i) here;
            (d[12][0] == 4) here;
            (d[13][0] == 3) here;
            (d[14][0] == 4) here;
            (d[16][0] == 10) here;
        }
        cleanup(&B);
    }
    // Part 2: integer ops
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val a = umbra_imm_i32(b, 10);
        umbra_val c = umbra_imm_i32(b, 3);
        umbra_store_32(b, (umbra_ptr){0, 0}, umbra_sub_i32(b, a, c));
        umbra_store_32(b, (umbra_ptr){1, 0}, umbra_mul_i32(b, a, c));
        umbra_store_32(b, (umbra_ptr){2, 0}, umbra_shl_i32(b, a, c));
        umbra_store_32(b, (umbra_ptr){3, 0}, umbra_shr_u32(b, a, c));
        umbra_store_32(b, (umbra_ptr){4, 0}, umbra_shr_s32(b, a, c));
        umbra_store_32(b, (umbra_ptr){5, 0}, umbra_and_i32(b, a, c));
        umbra_store_32(b, (umbra_ptr){6, 0}, umbra_or_i32(b, a, c));
        umbra_store_32(b, (umbra_ptr){7, 0}, umbra_xor_i32(b, a, c));
        umbra_store_32(b, (umbra_ptr){8, 0}, umbra_sel_i32(b, umbra_imm_i32(b, -1), a, c));
        umbra_store_32(b, (umbra_ptr){9, 0}, umbra_eq_i32(b, a, c));
        umbra_store_32(b, (umbra_ptr){10, 0}, umbra_lt_s32(b, a, c));
        umbra_store_32(b, (umbra_ptr){11, 0}, umbra_le_s32(b, a, c));
        umbra_store_32(b, (umbra_ptr){12, 0}, umbra_lt_u32(b, a, c));
        umbra_store_32(b, (umbra_ptr){13, 0}, umbra_le_u32(b, a, c));
        backends B = make(b);
        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            int32_t d[14][4];
            __builtin_memset(d, 0, sizeof d);
            umbra_buf bufs[14];
            for (int i = 0; i < 14; i++) { bufs[i] = (umbra_buf){.ptr=d[i], .sz=16}; }
            if (!run(&B, bi, 4, 1, bufs)) { continue; }
            (d[0][0] == 7)  here;   // 10-3
            (d[1][0] == 30) here;   // 10*3
            (d[7][0] == 9)  here;   // 10^3
            (d[8][0] == 10) here;   // sel(-1, 10, 3) = 10
            (d[9][0] == 0)  here;   // eq(10,3)=0
        }
        cleanup(&B);
    }
}

int main(void) {
    test_ra_chan_unary();
    test_mul_pow2_peephole();
    test_const_eval();
    test_binary_m_rr();
    test_binary_r_mm();
    test_minmax_m_rm();
    test_cmp_r_rm_rr();
    test_unary_r_m();
    test_unary_m_r();
    test_base_imm_ops();
    test_base_imm_more();
    test_sel_fms_pack_variants();
    test_sel_r_rm();
    test_imm_regvar();
    test_unary_r_m();
    test_base_imm_ops();
    test_sel_fms_pack_variants();
    test_imm_regvar();
    test_shr_ops();
    test_neg_round_i32();
    test_cmp_unsigned_signed();
    test_max_f32_imm();
    test_imm_cmp_i32();
    test_uniform_register();
    test_minmax_register_variants();
    test_cmp_register_variants();
    test_pack_register_variants();
    test_regvar_m_patterns();
    test_preamble_register_boundary();
    test_backend_threadsafe();
    test_program_null_guards();
    test_f32_ops();
    test_i32_ops();
    test_cmp_i32();
    test_cmp_f32();
    test_imm();
    test_fma_f32();
    test_min_max_sqrt_f32();
    test_abs_f32();
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
    test_load_store_color_8888();
    test_load_store_color_565();
    test_load_store_color_1010102();
    test_load_store_color_fp16();
    test_load_store_color_srgb();
    test_load_store_color_f16_planar();
    test_srgb_roundtrip_256();
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
        backends B = make(b);

        enum { S = 32, TH = 16, L = 5, T = 3, R = 21, BT = 11 };
        int32_t buf[S * TH];

        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            __builtin_memset(buf, 0xff, sizeof buf);
            if (!B.p[bi]) { continue; }
            umbra_program_queue(B.p[bi], L, T, R, BT,
                                (umbra_buf[]){{.ptr=buf, .sz=sizeof buf, .row_bytes=S * 4}});
            umbra_backend_flush(B.be[bi]);
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

    // Regression test: load from strided buffer with arbitrary tile.
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val v   = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val one = umbra_imm_i32(b, 1);
        umbra_store_32(b, (umbra_ptr){1, 0}, umbra_add_i32(b, v, one));
        backends B = make(b);

        enum { S = 20, TH = 10, L = 3, T = 2, R = 15, BT = 7 };
        int32_t src[S * TH], dst[S * TH];
        for (int i = 0; i < S * TH; i++) { src[i] = i; }

        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            __builtin_memset(dst, 0, sizeof dst);
            if (!B.p[bi]) { continue; }
            umbra_program_queue(B.p[bi], L, T, R, BT,
                                (umbra_buf[]){{.ptr=src, .sz=sizeof src, .row_bytes=S * 4, .read_only=1},
                                              {.ptr=dst, .sz=sizeof dst, .row_bytes=S * 4}});
            umbra_backend_flush(B.be[bi]);
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

    // Regression test: two direct buffers with different row_bytes (strides).
    // Metal bug: uses a single shared stride from one store buffer.
    {
        struct umbra_builder *b = umbra_builder();
        umbra_val             v   = umbra_load_32(b, (umbra_ptr){0, 0});
        umbra_val             one = umbra_imm_i32(b, 1);
        umbra_store_32(b, (umbra_ptr){1, 0}, umbra_add_i32(b, v, one));
        backends B = make(b);

        enum { SW = 32, DW = 20, H = 5, L = 3, T = 1, R = 15, BT = 4 };
        int32_t src[SW * H], dst[DW * H];
        for (int i = 0; i < SW * H; i++) { src[i] = i * 10; }

        for (int bi = 0; bi < NUM_BACKENDS; bi++) {
            __builtin_memset(dst, 0, sizeof dst);
            if (!B.p[bi]) { continue; }
            umbra_program_queue(B.p[bi], L, T, R, BT,
                                (umbra_buf[]){{.ptr=src, .sz=sizeof src, .row_bytes=SW * 4, .read_only=1},
                                              {.ptr=dst, .sz=sizeof dst, .row_bytes=DW * 4}});
            umbra_backend_flush(B.be[bi]);
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

    // Regression test: l>0 with deref'd buffer that has row_bytes>0.
    {
        struct umbra_builder *b = umbra_builder();
        int                   off = umbra_reserve_ptr(b);
        umbra_ptr             src = umbra_deref_ptr(b, (umbra_ptr){1, 0}, off);
        umbra_val             v   = umbra_load_32(b, src);
        umbra_val             one = umbra_imm_i32(b, 1);
        umbra_store_32(b, (umbra_ptr){2, 0}, umbra_add_i32(b, v, one));
        backends B = make(b);

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
                                (umbra_buf[]){{0},
                                              {.ptr=uni, .sz=sizeof uni_, .read_only=1},
                                              {.ptr=dst_px, .sz=sizeof dst_px, .row_bytes=S * 4}});
            umbra_backend_flush(B.be[bi]);
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

    // Regression test: l>0 with deref'd 16-bit buffer, row_bytes>0.
    {
        struct umbra_builder *b = umbra_builder();
        int                   off = umbra_reserve_ptr(b);
        umbra_ptr             src = umbra_deref_ptr(b, (umbra_ptr){1, 0}, off);
        umbra_val             v   = umbra_f32_from_f16(b, umbra_load_16(b, src));
        umbra_val             one = umbra_imm_f32(b, 1.0f);
        umbra_store_32(b, (umbra_ptr){2, 0}, umbra_add_f32(b, v, one));
        backends B = make(b);

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
                                (umbra_buf[]){{0},
                                              {.ptr=uni, .sz=sizeof uni_, .read_only=1},
                                              {.ptr=dst_px, .sz=sizeof dst_px, .row_bytes=S * 4}});
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


